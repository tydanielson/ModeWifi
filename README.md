# Van Control System - ESP32 Native CAN

A modern ESP32-based control system for Storyteller Overland Sprinter vans with Rixen HVAC and PDM power distribution. This project provides WiFi-enabled monitoring and control of van systems via a mobile-optimized web interface.

> **Fork Notice:** This is a heavily modified fork of [changer65535/ModeWifi](https://github.com/changer65535/ModeWifi). See [Credits](#-credits) for details.

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
- **Responsive web interface** - Mobile-friendly dark theme UI accessible via WiFi
- **Dual-core operation** - WiFi/web on Core 0, CAN processing on Core 1
- **PDM1/PDM2 support** - Monitor and control lights, pumps, fans, and power systems
- **Rixen HVAC integration** - Temperature, voltage, and heating system monitoring
- **Zero configuration** - Connect to "VanControl" WiFi network and go
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
- **ESP32 DevKit** - [AOKIN ESP32 Dev Board](https://www.amazon.com/dp/B0B5DTN62K) - ~$15 (2-pack)
- **SN65HVD230 CAN Transceiver** - [HiLetgo 3.3V CAN Module](https://www.amazon.com/dp/B0F1JB2941) - ~$11 (2-pack)
- **Jumper Wires** - [Dupont Wire Kit](https://www.amazon.com/dp/B08Y59P6D1) - ~$7
- **DT Connector Kit** (optional) - [For clean van connections](https://www.amazon.com/dp/B0BVH43P9L) - ~$20
- **DT Crimper Tool** (optional) - [For DT connectors](https://www.amazon.com/dp/B0718T232Z) - ~$30

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

## 📱 Quick Start

### 1. Install PlatformIO
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
platformio run --target upload
platformio device monitor --baud 115200
```

### 3. Connect to Van
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
├── web_server.h       # HTTP server and JSON API handlers
└── web_interface.h    # HTML/CSS/JavaScript embedded UI

Backups:
├── src/main.cpp.old   # Previous monolithic version
├── src/cm.cpp.old     # Original Arduino MCP2515 code
└── src/cm.h.old       # Original headers
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

### Phase 1 - Local Monitoring ✅ (Current)
- [x] CAN bus reading and decoding
- [x] WiFi access point
- [x] Mobile web interface
- [x] Real-time van state display

### Phase 2 - Control Features (In Progress)
- [ ] Send CAN commands to control lights/pumps
- [ ] Awning automation
- [ ] Climate control presets
- [ ] Tank level alerts

Note: The original repo had control features working - this needs to be ported to the new architecture.

### Phase 3 - Remote Access (Planned)
- [ ] AWS IoT Core integration via MQTT
- [ ] Remote monitoring over Starlink
- [ ] Cloud dashboard for van status
- [ ] Mobile app notifications

## 📚 Documentation

- **[Setup Guide](docs/SETUP.md)** - Quick start guide (15 minutes)
- **[Code Structure](docs/CODE_STRUCTURE.md)** - Architecture overview
- **[Testing Checklist](docs/VAN_TESTING_CHECKLIST.md)** - Van testing procedures
- **[Fork Changes](docs/FORK_CHANGES.md)** - Detailed comparison with original
- **[GPL Compliance](docs/GPL_COMPLIANCE.md)** - License compliance details

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
- ✨ Migrated from Arduino IDE to PlatformIO
- ✨ Modular code architecture with separate headers
- ✨ Dual-core FreeRTOS task implementation
- ✨ Mobile-first responsive web UI

**Huge thanks to Christopher Hanger** for reverse-engineering the PDM and Rixen CAN protocols! All the CAN message decoding logic comes from that original work.

## 📄 License

GNU General Public License v3.0 - See [LICENSE](LICENSE) file

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This is a derivative work based on the original [ModeWifi by Christopher Hanger (changer65535)](https://github.com/changer65535/ModeWifi), licensed under GPL v3.

## 🤝 Contributing

Issues and pull requests welcome! This is an active project being used in a production van build.

**Current status:** Working prototype, tested with real van CAN bus (Dec 6, 2025).

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
