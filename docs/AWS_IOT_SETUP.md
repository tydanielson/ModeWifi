# AWS IoT Core Integration Plan

## Overview

Add remote monitoring capability using AWS IoT Core via MQTT over Starlink internet connection.

## Architecture

### Current (Phase 1)
```
Van CAN Bus → ESP32 → WiFi AP → Local Web Interface
```

### Target (Phase 3)
```
Van CAN Bus → ESP32 → WiFi Client (Starlink) → AWS IoT Core → Cloud Dashboard
                    ↓
                WiFi AP (fallback) → Local Web Interface
```

## Implementation Steps

### 1. AWS IoT Core Setup
- [ ] Create AWS account / Use existing
- [ ] Create IoT Thing for the van
- [ ] Generate certificates (device cert, private key, root CA)
- [ ] Create IoT Policy for publish/subscribe permissions
- [ ] Attach policy to certificate
- [ ] Note IoT endpoint URL

### 2. Add Required Libraries
Add to `platformio.ini`:
```ini
lib_deps = 
    sandeepmistry/CAN @ ^0.3.1
    bblanchon/ArduinoJson @ 6.21.5
    knolleary/PubSubClient @ ^2.8    ; MQTT client
```

### 3. WiFi Mode Changes
**Current:** Access Point only
**New:** Support both modes
- AP mode for local setup/fallback
- Station mode for Starlink connection
- Add web UI to configure WiFi credentials

### 4. Certificate Management
Options:
1. **Hardcode in code** (quick but not secure)
2. **Store in SPIFFS** (better - certificates as files)
3. **Provision via web UI** (best - paste certs via setup page)

### 5. MQTT Topics Design

**Publish (ESP32 → Cloud):**
- `van/{clientId}/telemetry` - Regular status updates (every 30s)
- `van/{clientId}/status` - Connection status
- `van/{clientId}/alerts` - Tank levels, errors

**Subscribe (Cloud → ESP32):**
- `van/{clientId}/commands` - Control commands (future Phase 2)

**Message Format:**
```json
{
  "timestamp": 1733529600,
  "battery_voltage": 12.8,
  "glycol_temp": 22.5,
  "pdm1": [0, 85, 0, 100, 0, 0, 0, 0, 0, 50, 0, 0, 0],
  "pdm2": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  "location": "optional-gps-coords"
}
```

### 6. Code Structure

New files to create:
- `src/aws_iot.h` - AWS IoT connection and MQTT logic
- `src/wifi_manager.h` - Handle AP/Station mode switching
- `src/config.h` - WiFi credentials and AWS settings

Modified files:
- `src/main.cpp` - Add IoT task on Core 0
- `src/web_interface.h` - Add WiFi config page

### 7. Dual-Core Task Assignment

**Core 0 (Protocol CPU):**
- WiFi management
- Web server (local AP mode)
- AWS IoT MQTT client (station mode)

**Core 1 (Application CPU):**
- CAN bus reading (no changes)
- Message decoding (no changes)

### 8. Testing Plan

1. **Local WiFi Test**
   - Connect ESP32 to home WiFi
   - Test MQTT connection to AWS IoT
   - Verify telemetry publishing

2. **Starlink Test**
   - Connect to van's Starlink WiFi
   - Test connection stability
   - Measure data usage (estimate monthly cost)

3. **Failover Test**
   - Disconnect from Starlink
   - Verify fallback to AP mode
   - Reconnect and verify recovery

### 9. Power Considerations

**Current draw estimates:**
- ESP32 idle: ~80mA @ 3.3V
- ESP32 WiFi active: ~160-260mA @ 3.3V
- With MQTT (every 30s): ~150mA average

**Daily power usage:**
- 150mA × 24h = 3.6Ah per day
- At 12V: ~0.5Ah per day from battery
- Negligible impact on van battery

### 10. Security Considerations

- ✅ AWS IoT uses TLS 1.2
- ✅ Certificate-based authentication
- ✅ Private keys never leave device
- ⚠️ No command validation initially (read-only)
- 🔒 Future: Add command authentication for control features

### 11. Cost Analysis

**AWS IoT Core Pricing (US-East-1):**
- Connectivity: $0.08 per million minutes
- Messaging: $1.00 per million messages
- Device Shadow: $1.25 per million updates

**Monthly estimate (1 device, 30s updates):**
- Messages: ~86,400/month → $0.09
- Connectivity: 43,200 minutes → $0.003
- **Total: ~$0.10/month** (practically free!)

### 12. Features for v1.0

**Must Have:**
- [x] Maintain local AP mode for setup
- [ ] Connect to Starlink WiFi
- [ ] Publish telemetry every 30 seconds
- [ ] Auto-reconnect on connection loss
- [ ] LED status indicator (connected/disconnected)

**Nice to Have:**
- [ ] Web UI for WiFi configuration
- [ ] Certificate upload via web UI
- [ ] Cloud dashboard (separate web app)
- [ ] Historical data storage (DynamoDB)
- [ ] Mobile push notifications

**Future (Phase 2):**
- [ ] Receive commands from cloud
- [ ] Send CAN commands to control van systems
- [ ] Geofencing alerts
- [ ] Maintenance reminders

## Implementation Order

1. ✅ Create branch `aws-iot-integration`
2. Add PubSubClient library
3. Create `src/aws_iot.h` with connection logic
4. Add WiFi station mode support
5. Hardcode test credentials for initial testing
6. Test with AWS IoT Test client
7. Add web UI for configuration
8. Add certificate management
9. Test with Starlink in van
10. Document setup process

## Next Steps

Start with basic AWS IoT connection and MQTT publish of van state. Keep local web interface working for fallback.

**Current branch:** `aws-iot-integration`
**Target timeline:** 1-2 weeks for basic remote monitoring
