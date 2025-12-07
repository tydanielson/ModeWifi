# ESP32 AWS IoT Setup Guide

## Quick Start

### 1. Install Dependencies
```bash
# PlatformIO will auto-download these:
# - sandeepmistry/CAN @ ^0.3.1
# - bblanchon/ArduinoJson @ ^6.21.0
# - knolleary/PubSubClient @ ^2.8
```

### 2. Configure Your Device

Copy the config template:
```bash
cp src/config.h.example src/config.h
```

Edit `src/config.h` with:

**AWS IoT Settings** (from terraform output):
```cpp
const char* AWS_IOT_ENDPOINT = "YOUR_ENDPOINT.iot.us-east-1.amazonaws.com";
const char* THING_NAME = "storyteller-van-01";
```

**WiFi Settings**:
```cpp
const char* WIFI_SSID = "YOUR_STARLINK_SSID";
const char* WIFI_PASSWORD = "YOUR_STARLINK_PASSWORD";
```

**Certificates** (from `certificates/` folder):
```cpp
// Device Certificate (cat certificates/certificate.pem.crt)
const char* DEVICE_CERTIFICATE = R"EOF(
-----BEGIN CERTIFICATE-----
... paste your certificate here ...
-----END CERTIFICATE-----
)EOF";

// Private Key (cat certificates/private.pem.key)
const char* PRIVATE_KEY = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
... paste your private key here ...
-----END RSA PRIVATE KEY-----
)EOF";
```

### 3. Build and Upload
```bash
pio run --target upload
pio device monitor --baud 115200
```

## Architecture

### Dual-Core Operation

```
Core 0 (WiFi/Network):              Core 1 (CAN Bus):
├── WiFi Manager                    ├── CAN Message Reader
├── AWS IoT (MQTT)                  ├── PDM1/PDM2 Decoder
├── Web Server                      ├── Rixen HVAC Decoder
└── Publish every 30s               └── Van State Updates
```

### Data Flow

```
CAN Bus → ESP32 Core 1 → VanState → ESP32 Core 0 → AWS IoT → DynamoDB
                                  ↓
                                Web UI (local)
```

### File Structure

```
src/
├── main.cpp              # Main program loop, dual-core setup
├── config.h              # WiFi + AWS credentials (GITIGNORED)
├── config.h.example      # Template for config
├── wifi_manager.h        # WiFi connection (Starlink + AP fallback)
├── aws_iot.h             # MQTT client for AWS IoT
├── can_messages.h        # CAN message ID definitions
├── van_state.h           # Van state data structure
├── can_decoder.h         # PDM/Rixen message decoders
├── web_server.h          # HTTP handlers
└── web_interface.h       # HTML/CSS/JS for local UI
```

## WiFi Modes

### Station Mode (Preferred)
- Connects to Starlink WiFi
- Enables AWS IoT publishing
- Local web UI still available
- LED: Solid ON

### Access Point Mode (Fallback)
- No Starlink available
- Creates "VanControl" hotspot
- Local web UI only (no AWS IoT)
- LED: Blinks 3 times

## MQTT Topics

### Publishing (ESP32 → AWS)
- `van/storyteller-van-01/telemetry` - Every 30 seconds
- `van/storyteller-van-01/status` - On connect/disconnect
- `van/storyteller-van-01/alerts` - Critical events

### Subscribing (AWS → ESP32)
- `van/storyteller-van-01/commands` - Control commands (Phase 3)

## Testing

### 1. Serial Monitor
```bash
pio device monitor --baud 115200
```

Look for:
```
✅ WiFi Connected!
✅ Connected to AWS IoT Core!
📤 Publishing telemetry (450 bytes)... ✅
```

### 2. Check AWS IoT Test Client
```bash
# Open in browser
open "https://us-east-1.console.aws.amazon.com/iot/home?region=us-east-1#/test"

# Subscribe to:
van/storyteller-van-01/#
```

### 3. Check DynamoDB
```bash
aws dynamodb scan \
  --table-name storyteller-van-01-telemetry \
  --limit 1 \
  --profile skadi | jq
```

## Troubleshooting

### WiFi Won't Connect
```
❌ WiFi connection failed
```
- Check WIFI_SSID and WIFI_PASSWORD in config.h
- Make sure Starlink is powered on
- ESP32 will fallback to AP mode automatically

### MQTT Connection Failed
```
❌ Failed (rc=-2)
Error: Connect failed
```
- Check AWS_IOT_ENDPOINT in config.h
- Verify certificates are correct (no extra spaces/newlines)
- Run terraform output to confirm endpoint
- Check certificate is attached: `aws iot list-thing-principals --thing-name storyteller-van-01 --profile skadi`

### Certificate Errors
```
Error: Bad credentials (rc=4)
```
- Certificate/private key mismatch
- Copy exact contents from certificates/ folder
- Don't add or remove any characters
- Make sure R"EOF( and )EOF" are on their own lines

### Out of Memory
```
Guru Meditation Error: Core 0 panic'ed (LoadProhibited)
```
- Increase stack size in xTaskCreatePinnedToCore (try 20000)
- Reduce MQTT_BUFFER_SIZE in aws_iot.h
- Check for memory leaks in custom code

## Security Notes

⚠️ **NEVER commit `src/config.h` to git!**

This file contains:
- Your WiFi password
- Your AWS private key
- AWS IoT endpoint

The file is automatically gitignored. If you accidentally commit it:
1. Remove from git: `git rm --cached src/config.h`
2. Revoke certificates: `aws iot update-certificate --certificate-id <ID> --new-status REVOKED`
3. Generate new certificates: `cd terraform && terraform taint null_resource.generate_certificates && terraform apply`
4. Update config.h with new certificates

## Performance

### Memory Usage
- ~180KB free heap with TLS connection
- ~40KB used by MQTT buffers
- ~20KB used by web server

### Network Usage
- MQTT: ~450 bytes per message
- Frequency: Every 30 seconds
- Monthly: ~1.1 MB (WiFi), ~86K MQTT messages
- Cost: ~$0.10/month AWS IoT

### Power Consumption
- WiFi connected: ~150 mA
- WiFi + MQTT: ~180 mA
- AP mode: ~120 mA

## Next Steps

1. **Test with Local WiFi First**
   - Upload code
   - Monitor serial output
   - Verify MQTT publishing
   - Check DynamoDB

2. **Test in Van with Starlink**
   - Install ESP32
   - Power up Starlink
   - Verify connection
   - Monitor for 1 hour

3. **Deploy for Trip**
   - Mount ESP32
   - Route power
   - Secure wiring
   - Test failover (disconnect Starlink)

## Support

See documentation:
- `docs/ARCHITECTURE.md` - System design
- `docs/AWS_IOT_SETUP.md` - AWS configuration
- `terraform/SECURITY_FEATURES.md` - Security details
