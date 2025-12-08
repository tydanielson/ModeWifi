# Van Control System - ESP32 Native CAN

A modern ESP32-based control system for Storyteller Overland Sprinter vans with Rixen HVAC and PDM power distribution. This project provides WiFi-enabled monitoring and control of van systems via a mobile-optimized web interface with AWS IoT Core integration for remote access.

> **Fork Notice:** This is a heavily modified fork of [changer65535/ModeWifi](https://github.com/changer65535/ModeWifi). See [Credits](#-credits) for details.

> **AI Development Notice:** Significant portions of this project's code, infrastructure, and documentation were developed with assistance from Claude (Anthropic). This includes AWS IoT integration, Terraform infrastructure, Lambda functions, and various firmware improvements.

## 📖 Table of Contents

- [Quick Start](#-quick-start) - Get up and running in 30 minutes
- [Personalization](#-personalize-your-van) - Give your van a name
- [Features](#-features) - What this system can do
- [Hardware Requirements](#-hardware-requirements) - Parts list and wiring
- [Setup Guide](#-complete-setup-guide) - Step-by-step instructions
- [Architecture](#-architecture) - How it works
- [Documentation](#-documentation) - Detailed guides
- [Troubleshooting](#-troubleshooting) - Common issues
- [Credits](#-credits) - Original project and changes

## 🚀 Quick Start

**Time required:** ~30 minutes for basic setup, +15 minutes for AWS IoT

### Prerequisites
- Storyteller Overland van with Rixen HVAC and PDM modules
- Basic soldering skills (or willingness to learn)
- Computer with USB port
- AWS account (optional, for remote monitoring)

### Quick Setup Path

1. **Order hardware** (~$35-85, arrives in 2-3 days)
   - ESP32 DevKit, SN65HVD230 CAN transceiver, wires
   - See [Parts List](#parts-list) below

2. **Build electronics** (15 minutes)
   - Connect 4 wires between ESP32 and CAN transceiver
   - See [Wiring Connections](#wiring-connections) below

3. **Flash firmware** (5 minutes)
   - Install PlatformIO, clone repo, configure WiFi
   - Upload code to ESP32

4. **Test on bench** (5 minutes)
   - Power ESP32, connect to WiFi, verify web interface

5. **Deploy AWS infrastructure** (15 minutes - optional)
   - Run Terraform to create IoT resources
   - Configure certificates in firmware

6. **Install in van** (5 minutes)
   - Connect to van's CAN bus under Groove Lounge seat
   - Power from 12V with buck converter or USB battery

**Total time:** 30-45 minutes + parts shipping

### What You'll Get

- **Local monitoring** via WiFi hotspot (works without internet)
- **Remote monitoring** via AWS dashboard (optional, works over Starlink/cellular)
- **Real-time data**: Temperature, voltage, PDM states, heating system
- **Mobile-friendly** interface accessible from phone/tablet

## 🏷️ Personalize Your Van

Give your van a name! Edit `src/config.h`:
```cpp
const char* VAN_NAME = "Storyteller";  // or "Odyssey", "Adventure", etc.
```

This name will appear in:
- Dashboard header and browser title
- WiFi hotspot name (`{VAN_NAME}-Direct`)
- System messages

## ⚠️ Safety Notice

**Don't start messing with this stuff unless you know what you're doing. You could brick your van, and Storyteller will not help you. You could also void any warranty you may have.**

**You have been warned!**

This is currently a **monitoring system**. Control features from the original project need to be carefully ported and tested before use. Always test thoroughly in a safe environment before deploying in your van.

## 🔧 Why This Fork?

This project started with the original [ModeWifi by Christopher Hanger](https://github.com/changer65535/ModeWifi), which used an MCP2515 CAN controller + TJA1050 transceiver. I wanted to simplify the hardware and take advantage of the ESP32's built-in CAN capabilities:

**Original Setup (MCP2515 + TJA1050):**
- Requires external SPI CAN controller (MCP2515)
- TJA1050 transceiver needs 5V operation
- 4-wire SPI connection plus CAN wires
- Additional power supply considerations

**This Fork (ESP32 Native CAN + SN65HVD230):**
- ✅ Uses ESP32's built-in CAN controller (TWAI)
- ✅ SN65HVD230 runs directly on 3.3V
- ✅ Simple 2-wire connection (GPIO 21/22)
- ✅ Fewer components, simpler wiring
- ✅ Tested successfully: 50+ IDs, 20K+ messages, zero crashes

This approach also allowed for a complete code rewrite with modern PlatformIO tooling and a redesigned web interface optimized for performance and continued development.

## 🎯 Features

- **Real-time CAN bus monitoring** - Reads 50+ message types at 500kbps
- **AWS IoT Core integration** - Remote monitoring via MQTT over Starlink/cellular
- **Cloud dashboard** - Secure HTTPS dashboard with live telemetry updates
- **DynamoDB storage** - 30-day telemetry retention with efficient GSI queries
- **Responsive web interface** - Mobile-friendly dark theme UI
- **Dual-core operation** - WiFi/web on Core 0, CAN processing on Core 1
- **PDM1/PDM2 support** - Monitor and control lights, pumps, fans, and power systems
- **Rixen HVAC integration** - Temperature, voltage, and heating system monitoring
- **Infrastructure as code** - Terraform for reproducible AWS deployments
- **Active development** - Ongoing improvements and feature additions

## 🚐 Hardware Requirements

### Main Components
- **ESP32 DevKit** (tested with esp32doit-devkit-v1)
- **SN65HVD230 CAN Transceiver** (3.3V compatible)
- **Storyteller Overland Van** with:
  - Rixen HVAC system
  - PDM1/PDM2 power distribution modules
  - 500kbps CAN bus (Yellow=CAN-H, Green=CAN-L)

### Parts List
- **ESP32 DevKit** - [ESP32 Development Board](https://www.amazon.com/dp/B0718T232Z) - ~$15 (2-pack)
- **SN65HVD230 CAN Transceiver** - [HiLetgo 3.3V CAN Module](https://www.amazon.com/dp/B0B5DTN62K) - ~$11 (2-pack)
- **Power Converter** - [DC-DC Buck Converter 12V to 5V](https://www.amazon.com/dp/B0F1M24KG3) - ~$10 (5-pack)
- **Breadboard & Jumper Wires** - [Dupont Wire Kit with Breadboards](https://www.amazon.com/dp/B08Y59P6D1) - ~$7
- **DT Connector Kit** (optional) - [Professional DT Connector Kit](https://www.amazon.com/dp/B0DB86YJ8D) - ~$20
- **Professional Harness** (optional) - [Pre-made Wiring Harness](https://www.amazon.com/dp/B0F1JB2941) - ~$30

**Total cost:** ~$35 for basics, ~$85 with professional connectors

**Note:** The original project used MCP2515 + TJA1050, but this had voltage compatibility issues. The SN65HVD230 is specifically chosen for 3.3V operation with ESP32's native CAN controller.

### Wiring Connections
```
ESP32 GPIO 21 → SN65HVD230 CTX
ESP32 GPIO 22 → SN65HVD230 CRX
ESP32 3.3V    → SN65HVD230 VCC
ESP32 GND     → SN65HVD230 GND

SN65HVD230 CANH → Van CAN-H (yellow wire)
SN65HVD230 CANL → Van CAN-L (green wire)
```

**Why SN65HVD230 instead of TJA1050?**
- ✅ TJA1050 is 5V-only and requires external power supply
- ✅ SN65HVD230 runs on 3.3V directly from ESP32
- ✅ Simpler wiring (2 GPIO pins vs 4 with MCP2515)
- ✅ Uses ESP32's built-in TWAI/CAN controller (more reliable)

## 📚 Complete Setup Guide

### Step 1: Hardware Assembly (15 minutes)

**What you need:**
- ESP32 DevKit board
- SN65HVD230 CAN transceiver module
- 4 female-to-female jumper wires
- USB cable for programming

**Connections:**
```
ESP32 → SN65HVD230
──────────────────
GPIO 21 → CTX (CAN TX)
GPIO 22 → CRX (CAN RX)
3.3V    → VCC
GND     → GND
```

**Tips:**
- Double-check connections before powering on
- Use colored wires for easy identification
- Consider taking a photo of your wiring for reference

### Step 2: Software Setup (10 minutes)

**Install PlatformIO:**
```bash
# Option 1: VS Code Extension (recommended)
# Install "PlatformIO IDE" from VS Code extensions

# Option 2: Command line
pip install platformio
```

**Clone and configure:**
```bash
git clone https://github.com/tydanielson/ModeWifi.git
cd ModeWifi

# Copy example config
cp src/config.h.example src/config.h

# Edit src/config.h with your settings:
# - VAN_NAME (optional personalization)
# - WIFI_SSID (your van's WiFi network)
# - WIFI_PASSWORD (your WiFi password)
```

**Build and upload:**
```bash
# Connect ESP32 via USB
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

**What to expect:**
- "CAN bus initialized" message
- "WiFi connected" or "AP started" message
- CAN messages appearing in serial monitor

### Step 3: Bench Test (5 minutes)

Before installing in your van, test the hardware:

1. **Power on ESP32** (via USB)
2. **Check serial monitor** for startup messages
3. **Connect to WiFi**:
   - If van WiFi is on: Should auto-connect
   - If not: Look for `{VAN_NAME}-Direct` hotspot
4. **Verify web interface** works (local only, no CAN data yet)

### Step 4: AWS IoT Setup (15 minutes - Optional)

For remote monitoring over Starlink/cellular:

**Prerequisites:**
- AWS account
- AWS CLI configured (`aws configure`)
- Terraform installed (`brew install terraform`)

**Deploy infrastructure:**
```bash
cd terraform

# Initialize Terraform
terraform init

# Review what will be created
terraform plan

# Deploy (creates IoT Thing, DynamoDB, API Gateway, Lambda, CloudFront)
terraform apply

# Copy the outputs - you'll need these
terraform output
```

**Update firmware with certificates:**
```bash
# Terraform generates certificates in ../certificates/
# Copy the certificate details into src/config.h:

# 1. IoT Endpoint
terraform output iot_endpoint
# Add to config.h: AWS_IOT_ENDPOINT = "your-endpoint.iot.us-east-1.amazonaws.com"

# 2. Device Certificate
cat ../certificates/certificate.pem.crt
# Paste into config.h: DEVICE_CERTIFICATE = "..."

# 3. Private Key
cat ../certificates/private.pem.key
# Paste into config.h: PRIVATE_KEY = "..."

# Re-upload firmware
cd ..
pio run --target upload
```

**Access remote dashboard:**
```bash
# Get your CloudFront URL
cd terraform
terraform output cloudfront_domain

# Open in browser - works from anywhere!
```

### Step 5: Van Installation (5 minutes)

**Locate CAN bus:**
- Under Groove Lounge seat
- Between "CAN Bus 1" label and Rixen heater
- Look for DT connectors with yellow (CAN-H) and green (CAN-L) wires

**Connect to CAN:**
```
SN65HVD230 → Van CAN Bus
────────────────────────
CANH → Yellow wire (CAN-H)
CANL → Green wire (CAN-L)
```

**Connection methods:**
1. **Splice**: Cut wires, solder ESP32 wires in, heat shrink (permanent)
2. **DT splitter**: Build Y-adapter with DT connector kit (cleanest)
3. **Crimp taps**: Use 3M Scotchlok connectors (quick, reversible)

**Power options:**
```
Option 1: USB power bank (testing/temporary)
Option 2: 12V → 5V buck converter → ESP32 (permanent)
Option 3: Tap into existing 5V USB port
```

### Step 6: Verify Operation (5 minutes)

**Check serial monitor:**
```bash
pio device monitor --baud 115200
```

**Look for:**
- ✅ "CAN bus initialized" 
- ✅ "Connected to van WiFi"
- ✅ "AWS IoT connected" (if configured)
- ✅ CAN messages appearing every few seconds
- ✅ "📤 Publishing telemetry" every 30 seconds

**Test dashboard:**
1. **Local**: Connect to van WiFi, browse to API endpoint
2. **Remote**: Open CloudFront URL from anywhere
3. **Verify**: Temperature, voltage, PDM states updating

**Success indicators:**
- Dashboard shows current data (not stale timestamps)
- Temperature matches actual cabin temp
- Battery voltage ~13V when van running
- PDM channels reflect actual switch states

---

## 📱 Quick Start (Legacy - Local Only)
```bash
# VS Code extension (recommended)
# Search for "PlatformIO IDE" in VS Code extensions

# Or via CLI:
pip install platformio
```

### 2. Clone and Build
```bash
git clone https://github.com/YOUR_USERNAME/VanControl-ESP32.git
cd VanControl-ESP32
```

### 3. Configure AWS IoT Core (Required for Remote Monitoring)

This project uses AWS IoT Core for remote monitoring over Starlink/cellular. Initial setup takes ~15 minutes.

#### Prerequisites
- AWS account with credentials configured
- Terraform installed (`brew install terraform`)
- AWS CLI configured (`aws configure` or AWS SSO)

#### Deploy Infrastructure
```bash
cd terraform
terraform init
terraform plan
terraform apply
```

This creates:
- **AWS IoT Thing** - Device identity for your van
- **DynamoDB table** - Stores telemetry data with 30-day TTL
- **API Gateway + Lambda** - REST API to query telemetry
- **CloudFront + S3** - Dashboard hosting with HTTPS
- **Device certificates** - Secure MQTT authentication

#### Configure ESP32
```bash
# 1. Copy certificate info from Terraform output
cat ../certificates/certificate-info.json

# 2. Get IoT endpoint
terraform output iot_endpoint

# 3. Update src/config.h with certificates and endpoint
cp src/config.h.example src/config.h
# Edit src/config.h with your values
```

See [docs/ESP32_SETUP.md](docs/ESP32_SETUP.md) for detailed configuration steps.

### 4. Build and Upload Firmware
```bash
platformio run --target upload
platformio device monitor --baud 115200
```

### 5. Access Dashboard

**Local (WiFi Direct):**
1. Connect to van WiFi or `{VAN_NAME}-Direct` hotspot
2. Browse to your API Gateway URL (from Terraform output)

**Remote (AWS CloudFront):**
- Dashboard URL provided in Terraform output
- Updates every 5 seconds with live telemetry
- Works from anywhere with internet

### 6. Connect to Van
1. Power ESP32 via USB (or van 12V with buck converter)
2. Wait for "VanControl" WiFi network to appear
3. Connect with password: `vanlife2025`
4. Open browser to: `http://192.168.4.1`

## 🎨 Web Interface

The mobile-optimized interface shows:
- **System Status** - Battery voltage and glycol temperature
- **PDM1 Controls** - Lights, pumps, and awning systems
- **PDM2 Controls** - Fans, refrigerator, and power outlets
- **Auto-refresh** - Updates every 2 seconds

Active items are highlighted in green with their current percentage values.

**Test the UI locally:** Open `test_web_interface.html` in your browser to preview the interface with mock data.

## 🏗️ Architecture

### Code Structure
```
src/
├── main.cpp           # Main loop, CAN message handling, dual-core setup
├── can_messages.h     # CAN message ID definitions
├── van_state.h        # Shared van state data structure
├── can_decoder.h      # PDM and Rixen message decoders
├── wifi_manager.h     # WiFi connection with AP fallback
├── aws_iot.h          # AWS IoT Core MQTT client
└── config.h           # WiFi, AWS, and certificate configuration

terraform/
├── main.tf            # AWS IoT Thing, DynamoDB, certificates
├── webapp.tf          # API Gateway, Lambda, CloudFront, S3
├── variables.tf       # Configurable parameters
└── outputs.tf         # Endpoints and deployment info

lambda/
├── get-telemetry/     # Lambda function to query latest telemetry
└── send-command/      # Lambda function for future control features

dashboard/
└── index.html         # CloudFront-hosted dashboard with live updates
```

### Dual-Core Operation
- **Core 0 (Protocol CPU)** - WiFi Access Point + Web Server
- **Core 1 (Application CPU)** - CAN bus processing + message decoding

Benefits: No blocking between web requests and CAN processing, smooth UI updates even with high CAN traffic (20K+ messages in testing).

## 📊 CAN Messages Supported

### PDM1 (Power Distribution Module 1)
**Channels 1-6:** Solar backup, cargo lights, reading lights, cabin lights, awning lights, recirc pump  
**Channels 7-12:** Awning enable, exhaust fan, furnace power, water pump

### PDM2 (Power Distribution Module 2)
**Channels 1-6:** Galley fan, refrigerator, 12V USB, awning motors, tank monitor power  
**Channels 7-12:** Power switch, HVAC power, 12V speaker, sink pump, aux power

### Rixen HVAC System
- **0x726** - Glycol temperature and voltage
- **0x724** - Heat source selection
- **0x725** - Fan speed and fuel usage  
- **0x788** - Control commands

See [VAN_TESTING_CHECKLIST.md](VAN_TESTING_CHECKLIST.md) for complete message ID list.

## 🔧 Configuration

### WiFi Settings
Edit `src/main.cpp`:
```cpp
const char* ap_ssid = "VanControl";
const char* ap_password = "vanlife2025";  // Change this!
```

### GPIO Pins
Default pins (GPIO5 avoided due to boot conflicts):
```cpp
#define CAN_TX_PIN 21  // CTX on SN65HVD230
#define CAN_RX_PIN 22  // CRX on SN65HVD230
```

### CAN Bus Speed
Van uses 500kbps (default in code). If your van differs:
```cpp
CAN.begin(500E3);  // Change to 250E3 for 250kbps, etc.
```

## 🐛 Troubleshooting

### "Failed to start CAN bus!"
- ✅ Check wiring connections
- ✅ Verify 3.3V power to transceiver
- ✅ Ensure CAN-H/CAN-L not reversed
- ✅ Test with multimeter: idle voltage ~2.6V on both lines

### WiFi network not visible
- ✅ Check serial monitor for IP address
- ✅ Verify ESP32 has adequate power (USB or 5V buck converter)
- ✅ Restart ESP32 and wait 10-15 seconds for network to appear

### Web interface not loading
- ✅ Verify connected to "VanControl" WiFi
- ✅ Browse to exactly: `http://192.168.4.1`
- ✅ Try different browser or incognito mode
- ✅ Clear browser cache

## ⚙️ Van Installation

### Where to Connect
**Location:** Under the Groove Lounge seat, between CAN Bus 1 and the Rixen heater

The CAN bus uses **DT connectors** with yellow (CAN-H) and green (CAN-L) wires. You can:
1. **Splice into existing wires** using solder and heat shrink
2. **Build a splitter cable** using [DT connector kit](https://www.amazon.com/dp/B0BVH43P9L) and [crimper](https://www.amazon.com/dp/B0718T232Z)
3. **Y-adapter** (if available) - cleanest option

### Power Options
- **USB power bank** - Simple for testing
- **12V to 5V buck converter** - Permanent installation
- **Direct to ESP32 VIN** - If using 7-12V power source

## 🔮 Roadmap

### Phase 1 - Local Monitoring ✅ (Complete)
- [x] CAN bus reading and decoding
- [x] WiFi access point
- [x] Mobile web interface
- [x] Real-time van state display

### Phase 2 - AWS IoT Integration ✅ (Complete)
- [x] AWS IoT Core MQTT publishing
- [x] DynamoDB telemetry storage (30-day retention)
- [x] REST API via API Gateway + Lambda
- [x] CloudFront dashboard with HTTPS
- [x] Remote monitoring over Starlink/cellular
- [x] Terraform infrastructure-as-code

### Phase 3 - Control Features (Planned)
- [ ] Send CAN commands to control lights/pumps
- [ ] Awning automation
- [ ] Climate control presets
- [ ] Tank level alerts
- [ ] Mobile app notifications

Note: The original repo had control features working - this needs to be ported to the new architecture.

## 📚 Documentation

- **[ESP32 Setup Guide](docs/ESP32_SETUP.md)** - Complete setup with AWS IoT configuration
- **[Security Guide](SECURITY.md)** - Certificate management and AWS security best practices
- **[Testing Checklist](docs/VAN_TESTING_CHECKLIST.md)** - Van testing procedures
- **[Terraform Variables](terraform/variables.tf)** - Infrastructure configuration options

## 📚 Credits

This project is based on **[changer65535/ModeWifi](https://github.com/changer65535/ModeWifi)** by Christopher Hanger, originally designed for MCP2515-based CAN interfaces.

**Why this fork exists:**

The original used an **MCP2515 CAN controller + TJA1050 transceiver**, which had hardware issues:
- ❌ TJA1050 is 5V-only (caused van CAN bus crashes at 5V, under-performed at 3.3V)
- ❌ ESP32's 5V rail can't supply the 60-100mA needed by TJA1050
- ❌ Complex 4-wire SPI connection plus CAN wires

**Solution:** Use ESP32's **built-in CAN controller (TWAI)** with an **SN65HVD230 3.3V transceiver**:
- ✅ Native 3.3V operation (no voltage issues)
- ✅ Simple 2-wire connection (GPIO 21/22), avoid GPIO5 for voltage startup issues
- ✅ No external SPI controller needed
- ✅ Tested successfully: 50+ IDs, 20K+ messages, zero crashes

**Major changes in this fork:**
- ✨ Switched from MCP2515 (SPI) to ESP32 native CAN (TWAI)
- ✨ Changed from TJA1050 (5V) to SN65HVD230 (3.3V) transceiver
- ✨ Added AWS IoT Core integration with MQTT over Starlink/cellular
- ✨ Terraform infrastructure-as-code for AWS resources
- ✨ CloudFront + S3 dashboard with live telemetry updates
- ✨ DynamoDB storage with 30-day TTL and GSI for efficient queries
- ✨ Migrated from Arduino IDE to PlatformIO
- ✨ Modular code architecture with separate headers
- ✨ Dual-core FreeRTOS task implementation

**Huge thanks to Christopher Hanger** for reverse-engineering the PDM and Rixen CAN protocols! All the CAN message decoding logic comes from that original work.

## 📄 License

GNU General Public License v3.0 - See [LICENSE](LICENSE) file

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This is a derivative work based on the original [ModeWifi by Christopher Hanger (changer65535)](https://github.com/changer65535/ModeWifi), licensed under GPL v3.

## 🤝 Contributing

Issues and pull requests welcome! This is an active project being used in a production van build.

**Current status:** Production-ready! Tested with 13+ hours continuous operation (50+ CAN IDs, 1560+ MQTT publishes, zero crashes). Deployed Dec 8, 2025.

## ⚠️ Disclaimer

This project interfaces with your van's CAN bus. Incorrect wiring or commands could potentially damage vehicle systems. **Use at your own risk.** 

- Always test on a bench first before installing in your van
- Storyteller Overland is not affiliated with this project
- Modifying your van's systems may void warranties
- The authors are not responsible for any damage

**You have been warned!**

---

**Van life? Tech life. Both.** 🚐💻

*Built with ☕ and determination during a 2.5-week sprint before departure*
