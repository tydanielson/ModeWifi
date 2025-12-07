# System Architecture - Van Control via AWS IoT

## 🎯 High-Level Flow

```
┌─────────────┐      WiFi        ┌──────────────┐      MQTT/TLS     ┌─────────────┐
│             │ ◄────────────────►│              │ ◄────────────────►│             │
│  Van CAN    │                   │    ESP32     │                   │  AWS IoT    │
│    Bus      │   CAN Messages    │   Device     │   JSON Payloads   │    Core     │
│             │ ──────────────────►│              │ ──────────────────►│             │
└─────────────┘                   └──────────────┘                   └──────┬──────┘
                                         ▲                                   │
                                         │                                   │
                                         │                                   ▼
                                         │                            ┌─────────────┐
                                         │         Commands           │  DynamoDB   │
                                         │         (Future)           │  Telemetry  │
                                         └────────────────────────────┤   Table     │
                                                                      └──────┬──────┘
                                                                             │
                                                                             │ Query
                                                                             ▼
                                                                      ┌─────────────┐
                                                                      │   Web App   │
                                                                      │  Dashboard  │
                                                                      └─────────────┘
```

## 📡 Phase 1: ESP32 → AWS IoT (Telemetry Publishing)

### Step 1: ESP32 Connects to Starlink WiFi
```cpp
// src/wifi_manager.h
void connectWiFi() {
  WiFi.begin(STARLINK_SSID, STARLINK_PASSWORD);
  // Wait for connection...
  // IP: 192.168.1.xxx (assigned by Starlink router)
}
```

### Step 2: ESP32 Establishes Secure MQTT Connection
```cpp
// src/aws_iot.h
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Load certificates
espClient.setCACert(AMAZON_ROOT_CA1);
espClient.setCertificate(DEVICE_CERTIFICATE);
espClient.setPrivateKey(DEVICE_PRIVATE_KEY);

// Connect to AWS IoT endpoint
client.setServer(AWS_IOT_ENDPOINT, 8883);
client.connect(THING_NAME);  // "storyteller-van-01"
```

**Security**: TLS 1.2 encryption + X.509 certificate authentication

### Step 3: ESP32 Publishes Telemetry Every 30 Seconds
```cpp
// Main loop on Core 0
void publishTelemetry() {
  StaticJsonDocument<512> doc;
  
  // Build JSON payload from VanState
  doc["thing_name"] = THING_NAME;
  doc["timestamp"] = millis();
  doc["message_type"] = "telemetry";
  doc["battery_voltage"] = vanState.batteryVoltage;
  doc["glycol_temp"] = vanState.glycolTemp;
  
  JsonArray pdm1 = doc.createNestedArray("pdm1");
  for (int i = 0; i < 8; i++) {
    pdm1.add(vanState.pdm1[i]);
  }
  
  JsonArray pdm2 = doc.createNestedArray("pdm2");
  for (int i = 0; i < 8; i++) {
    pdm2.add(vanState.pdm2[i]);
  }
  
  doc["hvac_mode"] = vanState.hvacMode;
  doc["hvac_temp"] = vanState.hvacSetTemp;
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
  
  // Publish to AWS IoT
  client.publish("van/storyteller-van-01/telemetry", jsonBuffer);
}
```

**MQTT Topic**: `van/storyteller-van-01/telemetry`
**Frequency**: Every 30 seconds
**Payload Size**: ~400 bytes

### Step 4: AWS IoT Core Receives & Routes Message

**IoT Rule**: `storyteller_van_01_store_telemetry`
```sql
SELECT 
  topic(2) as thing_name,
  timestamp() as timestamp,
  'telemetry' as message_type,
  battery_voltage,
  glycol_temp,
  pdm1,
  pdm2,
  hvac_mode,
  hvac_temp,
  timestamp() + 2592000000 as ttl
FROM 'van/+/telemetry'
```

**Action**: Write to DynamoDB table

### Step 5: DynamoDB Stores Data

**Table**: `storyteller-van-01-telemetry`

Example record:
```json
{
  "thing_name": "storyteller-van-01",
  "timestamp": 1733529600000,
  "message_type": "telemetry",
  "battery_voltage": 13.2,
  "glycol_temp": 72,
  "pdm1": [true, false, true, false, false, false, false, false],
  "pdm2": [false, true, false, false, false, false, false, false],
  "hvac_mode": "heat",
  "hvac_temp": 68,
  "ttl": 1736208000000
}
```

**Retention**: 30 days (auto-deleted by TTL)

## 🌐 Phase 2: Web Dashboard (Reading Data)

### Architecture

```
┌─────────────────┐
│   Web Browser   │
│   (User)        │
└────────┬────────┘
         │ HTTPS
         ▼
┌─────────────────┐
│  AWS Amplify    │  ← Static hosting (React/Vue/HTML)
│  or S3 + CF     │
└────────┬────────┘
         │ API calls
         ▼
┌─────────────────┐
│  API Gateway    │  ← REST API with IAM auth
│  + Lambda       │
└────────┬────────┘
         │ Query
         ▼
┌─────────────────┐
│   DynamoDB      │  ← Read telemetry data
│   Table         │
└─────────────────┘
```

### Web App Stack Options

#### Option A: Simple Static Site + Lambda
```
Frontend: HTML/JS (like current web_interface.h)
API: API Gateway + Lambda (Python/Node.js)
Auth: AWS Cognito (username/password)
Hosting: S3 + CloudFront ($0.50/month)
```

#### Option B: React Dashboard
```
Frontend: React + Material-UI
API: API Gateway + Lambda
Auth: AWS Cognito
Hosting: AWS Amplify ($5/month)
Features: Charts, real-time updates, mobile-friendly
```

### Sample Lambda Function (API Backend)

```python
# lambda/get_telemetry.py
import boto3
from decimal import Decimal

dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('storyteller-van-01-telemetry')

def handler(event, context):
    thing_name = event['queryStringParameters']['thing_name']
    hours = int(event['queryStringParameters'].get('hours', 24))
    
    # Query last N hours of data
    now = int(time.time() * 1000)
    start_time = now - (hours * 3600 * 1000)
    
    response = table.query(
        KeyConditionExpression='thing_name = :tn AND timestamp > :ts',
        ExpressionAttributeValues={
            ':tn': thing_name,
            ':ts': start_time
        },
        ScanIndexForward=False,  # Newest first
        Limit=100
    )
    
    return {
        'statusCode': 200,
        'body': json.dumps(response['Items'], cls=DecimalEncoder),
        'headers': {
            'Access-Control-Allow-Origin': '*',
            'Content-Type': 'application/json'
        }
    }
```

### Sample Frontend Code

```javascript
// dashboard.js
async function loadTelemetry() {
  const response = await fetch(
    'https://api.yourdomain.com/telemetry?thing_name=storyteller-van-01&hours=24'
  );
  const data = await response.json();
  
  // Update UI
  document.getElementById('battery').textContent = 
    data[0].battery_voltage + ' V';
  document.getElementById('temp').textContent = 
    data[0].glycol_temp + ' °F';
  
  // Draw chart with historical data
  drawChart(data);
}

// Refresh every 30 seconds
setInterval(loadTelemetry, 30000);
```

## 📤 Phase 3: Web Dashboard → ESP32 (Commands)

### Step 1: User Triggers Command in Web App

```javascript
// dashboard.js
async function turnOnAwning() {
  await fetch('https://api.yourdomain.com/command', {
    method: 'POST',
    body: JSON.stringify({
      thing_name: 'storyteller-van-01',
      command: 'awning',
      action: 'extend'
    })
  });
}
```

### Step 2: Lambda Publishes to IoT Topic

```python
# lambda/send_command.py
import boto3

iot_client = boto3.client('iot-data')

def handler(event, context):
    body = json.loads(event['body'])
    thing_name = body['thing_name']
    command = body['command']
    action = body['action']
    
    # Publish to command topic
    iot_client.publish(
        topic=f'van/{thing_name}/commands',
        qos=1,
        payload=json.dumps({
            'command': command,
            'action': action,
            'timestamp': int(time.time() * 1000)
        })
    )
    
    return {'statusCode': 200, 'body': 'Command sent'}
```

**MQTT Topic**: `van/storyteller-van-01/commands`

### Step 3: ESP32 Receives Command

```cpp
// src/aws_iot.h
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  
  String command = doc["command"];
  String action = doc["action"];
  
  if (command == "awning" && action == "extend") {
    // Send CAN message to activate awning
    sendCANCommand(AWNING_EXTEND_MSG_ID, AWNING_EXTEND_DATA);
  }
  
  if (command == "hvac") {
    int setTemp = doc["temperature"];
    // Send CAN message to Rixen controller
    sendHVACCommand(setTemp);
  }
}

void setup() {
  client.setCallback(mqttCallback);
  client.subscribe("van/storyteller-van-01/commands");
}
```

### Step 4: ESP32 Sends CAN Message

```cpp
// src/can_control.h (new file for Phase 3)
void sendCANCommand(uint32_t msgId, uint8_t data[], uint8_t len) {
  CAN.beginPacket(msgId);
  CAN.write(data, len);
  CAN.endPacket();
  
  Serial.printf("CAN TX: 0x%03X [%d bytes]\n", msgId, len);
}

void sendHVACCommand(int temperature) {
  uint8_t data[8] = {0};
  data[0] = 0x01;  // HVAC command
  data[1] = temperature;
  
  sendCANCommand(RIXEN_HVAC_CMD_ID, data, 8);
}
```

## 🔐 Data Contract (JSON Schema)

### Telemetry Message (ESP32 → AWS)

```json
{
  "thing_name": "storyteller-van-01",
  "timestamp": 1733529600000,
  "message_type": "telemetry",
  "battery_voltage": 13.2,
  "glycol_temp": 72,
  "pdm1": [true, false, true, false, false, false, false, false],
  "pdm2": [false, true, false, false, false, false, false, false],
  "hvac_mode": "heat",
  "hvac_temp": 68,
  "hvac_set_temp": 70,
  "water_level": 75,
  "gray_level": 30,
  "black_level": 15
}
```

### Status Message (ESP32 → AWS)

```json
{
  "thing_name": "storyteller-van-01",
  "timestamp": 1733529600000,
  "message_type": "status",
  "wifi_rssi": -45,
  "uptime_seconds": 86400,
  "free_heap": 120000,
  "can_messages_received": 50000,
  "mqtt_reconnects": 2,
  "firmware_version": "1.0.0"
}
```

### Alert Message (ESP32 → AWS)

```json
{
  "thing_name": "storyteller-van-01",
  "timestamp": 1733529600000,
  "message_type": "alert",
  "severity": "warning",
  "alert_type": "low_battery",
  "message": "Battery voltage below 12.0V",
  "value": 11.8
}
```

### Command Message (AWS → ESP32)

```json
{
  "command": "awning",
  "action": "extend",
  "timestamp": 1733529600000
}

{
  "command": "hvac",
  "mode": "heat",
  "temperature": 68,
  "timestamp": 1733529600000
}

{
  "command": "pdm",
  "channel": 3,
  "state": true,
  "timestamp": 1733529600000
}
```

### Command Response (ESP32 → AWS)

```json
{
  "thing_name": "storyteller-van-01",
  "timestamp": 1733529600000,
  "message_type": "command_response",
  "command_id": "abc123",
  "status": "success",
  "message": "Awning extended"
}
```

## 🔄 Complete Flow Example

### Scenario: User Checks Battery, Then Turns On Heater

1. **User opens web dashboard**
   - Browser loads from CloudFront CDN
   - JavaScript makes API call to API Gateway
   - Lambda queries DynamoDB for last 100 telemetry records
   - Dashboard shows battery at 13.2V, temp at 45°F

2. **User clicks "Set Heat to 68°F"**
   - JavaScript sends POST to API Gateway
   - Lambda publishes to `van/storyteller-van-01/commands`:
     ```json
     {"command": "hvac", "mode": "heat", "temperature": 68}
     ```

3. **ESP32 receives command** (within 1 second)
   - MQTT callback triggered
   - Parses JSON command
   - Sends CAN message to Rixen HVAC controller:
     ```
     CAN TX: 0x18FDB3F1 [8] 01 44 00 00 00 00 00 00
     ```

4. **Van HVAC turns on**
   - Rixen controller receives CAN message
   - Activates heat mode, sets to 68°F
   - Sends acknowledgment on CAN bus

5. **ESP32 confirms command** (next telemetry cycle, 30s later)
   - Reads HVAC status from CAN bus
   - Publishes telemetry with `hvac_mode: "heat"`, `hvac_set_temp: 68`
   - IoT Rule writes to DynamoDB

6. **Dashboard auto-refreshes** (30s polling)
   - Queries DynamoDB
   - Shows HVAC now in heat mode at 68°F
   - User sees confirmation

## 📊 Message Flow Rates

| Direction | Topic | Frequency | Size | Monthly Messages |
|-----------|-------|-----------|------|------------------|
| ESP32→AWS | telemetry | 30s | 400B | 86,400 |
| ESP32→AWS | status | 5min | 200B | 8,640 |
| ESP32→AWS | alerts | Event-driven | 150B | ~100 |
| AWS→ESP32 | commands | User-initiated | 100B | ~500 |
| ESP32→AWS | cmd_response | Per command | 150B | ~500 |

**Total**: ~96,000 messages/month = **$0.10/month** at $1/million

## 🏗️ Implementation Phases

### ✅ Phase 1: Telemetry (Current Sprint)
- [x] Terraform infrastructure
- [x] DynamoDB single-table design
- [ ] ESP32 MQTT client code
- [ ] WiFi manager (Starlink + AP fallback)
- [ ] Test with AWS IoT Test client

### 📋 Phase 2: Web Dashboard (Next)
- [ ] API Gateway + Lambda
- [ ] Simple HTML/JS dashboard
- [ ] Query last 24 hours of data
- [ ] Deploy to S3 + CloudFront
- [ ] Add AWS Cognito auth

### 🚀 Phase 3: Commands (Future)
- [ ] Command Lambda function
- [ ] Dashboard control buttons
- [ ] ESP32 command receiver
- [ ] CAN message sender
- [ ] Command acknowledgment flow

### 🎨 Phase 4: Polish (Before Trip)
- [ ] Real-time updates (WebSocket or polling)
- [ ] Historical charts
- [ ] Mobile-friendly UI
- [ ] Push notifications (SNS)
- [ ] Geofencing alerts

## 🔌 MQTT Topics Summary

| Topic | Direction | QoS | Purpose |
|-------|-----------|-----|---------|
| `van/{thing}/telemetry` | ESP32→AWS | 0 | Regular sensor data |
| `van/{thing}/status` | ESP32→AWS | 0 | Device health |
| `van/{thing}/alerts` | ESP32→AWS | 1 | Important events |
| `van/{thing}/commands` | AWS→ESP32 | 1 | Control commands |
| `van/{thing}/command_response` | ESP32→AWS | 1 | Ack/results |

**QoS 0**: At most once (fire and forget)
**QoS 1**: At least once (guaranteed delivery)

## 💰 Monthly Cost Breakdown

| Service | Usage | Cost |
|---------|-------|------|
| AWS IoT Core | 96K messages | $0.10 |
| DynamoDB | 96K writes, 10MB storage | $0.12 |
| API Gateway | 2,880 requests | $0.01 |
| Lambda | 2,880 invocations | Free tier |
| CloudFront | 1GB transfer | Free tier |
| S3 | Static hosting | $0.01 |
| **TOTAL** | | **$0.24/month** |

With full dashboard: **~$5-6/month** (Amplify hosting + Cognito)

## 🎯 Success Criteria

- ✅ ESP32 publishes telemetry every 30 seconds
- ✅ Data visible in DynamoDB within 1 second
- ✅ Web dashboard shows live data
- ✅ Dashboard can send commands to van
- ✅ ESP32 executes commands on CAN bus
- ✅ Everything works over Starlink in van
- ✅ Failover to AP mode if Starlink down
- ✅ System runs reliably for weeks

## Next Steps

1. **Deploy Terraform** (today)
   ```bash
   cd terraform && terraform apply
   ```

2. **Build ESP32 MQTT client** (this weekend)
   - Add PubSubClient library
   - Create aws_iot.h with MQTT code
   - Test with AWS IoT Test client

3. **Build simple dashboard** (next week)
   - Lambda + API Gateway
   - Basic HTML/JS frontend
   - Deploy to S3

Ready to start coding the ESP32 MQTT client?
