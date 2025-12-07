# Fork Changes Summary

This document summarizes the major changes from the original ModeWifi project.

## Overview

This fork represents a complete rewrite optimized for:
- **ESP32 native CAN** (TWAI controller) instead of MCP2515
- **PlatformIO** instead of Arduino IDE
- **Modular architecture** instead of monolithic code
- **Modern development** practices and tooling

## Hardware Changes

### Original Setup
- **Microcontroller:** ESP32 via Arduino IDE
- **CAN Interface:** MCP2515 (external SPI chip)
- **Transceiver:** TJA1050 (5V, requires external power)
- **Wiring:** 4 pins (SPI: MOSI, MISO, SCK, CS)

### New Setup  
- **Microcontroller:** ESP32 via PlatformIO
- **CAN Interface:** ESP32 built-in TWAI/CAN controller
- **Transceiver:** SN65HVD230 (3.3V, powered from ESP32)
- **Wiring:** 2 pins (GPIO 21/22 for TX/RX)

**Why?** Simpler, more reliable, no external SPI communication overhead.

## Software Architecture Changes

### File Structure

**Before (monolithic):**
```
BlinkyVan.ino         (187 lines)
Commode.ino          (1751 lines)
cm.cpp               (1745 lines)
cm.h                 (350+ lines)
```

**After (modular):**
```
src/
  main.cpp           (220 lines) - Entry point, dual-core setup
  can_messages.h     (17 lines)  - Message ID definitions
  van_state.h        (18 lines)  - Data structure
  can_decoder.h      (130 lines) - PDM/Rixen decoders
  web_server.h       (53 lines)  - HTTP handlers
  web_interface.h    (186 lines) - Embedded HTML/CSS/JS
```

### Key Improvements

1. **Dual-Core Operation**
   ```cpp
   // Core 0: WiFi + Web Server
   xTaskCreatePinnedToCore(webServerTaskFunction, "WebServer", 10000, NULL, 1, &webServerTask, 0);
   
   // Core 1: CAN processing (main loop)
   void loop() { /* CAN message handling */ }
   ```

2. **Modular Headers**
   - Each component in separate file
   - Clear dependencies
   - Easy to test and modify

3. **Better State Management**
   ```cpp
   struct VanState {
     uint8_t pdm1_ch1_6[6];
     uint8_t pdm1_ch7_12[6];
     uint8_t pdm2_ch1_6[6];
     uint8_t pdm2_ch7_12[6];
     float glycolTemp;
     float voltage;
     unsigned long lastUpdate;
   } vanState;
   ```

4. **Mobile-Optimized UI**
   - Responsive grid layout
   - Touch-friendly (48px min tap targets)
   - Native fonts for better performance
   - Dark theme optimized for night driving

## Code Size Comparison

| Metric | Original | This Fork | Change |
|--------|----------|-----------|--------|
| Total Lines | ~4000+ | ~600 | -85% |
| Main Logic | 1751 | 220 | -87% |
| Web UI | Mixed in C++ | 186 (separate) | Cleaner |
| Dependencies | Implicit | Explicit (platformio.ini) | Better |

## Testing Improvements

1. **Local UI Testing**
   - `test_web_interface.html` - test interface without ESP32
   - Mock data for rapid development

2. **Better Debugging**
   - Structured serial output
   - Baseline tracking (only show changes)
   - Message ID discovery
   - 30-second summaries

3. **Documentation**
   - `README.md` - Complete project overview
   - `SETUP.md` - Quick start guide
   - `CODE_STRUCTURE.md` - Architecture explanation
   - `VAN_TESTING_CHECKLIST.md` - Testing procedures

## Performance Improvements

### CAN Processing
- **Original:** Blocking loop processing
- **New:** Dedicated Core 1, non-blocking
- **Result:** Can handle 20K+ messages without lag

### Web Server
- **Original:** Shared with CAN processing
- **New:** Dedicated Core 0 task
- **Result:** UI always responsive, no blocking

### Memory Usage
- **RAM:** 14.2% (46,384 bytes)
- **Flash:** 60.0% (786,257 bytes)
- Room for future features

## Migration Path

If someone wants to port back to MCP2515:

1. Replace CAN library:
   ```cpp
   // Change from:
   #include <CAN.h>
   CAN.setPins(21, 22);
   CAN.begin(500E3);
   
   // To:
   #include <mcp2515.h>
   MCP2515 mcp2515(CS_PIN);
   mcp2515.setBitrate(CAN_500KBPS);
   ```

2. Keep everything else the same
   - Web server code unchanged
   - Decoders unchanged
   - UI unchanged

## Future Enhancements

This fork's architecture makes these easier:

- [ ] AWS IoT MQTT integration (just add another Core 0 task)
- [ ] OTA updates (PlatformIO built-in)
- [ ] Multiple web pages (just add handlers)
- [ ] API authentication (modify web_server.h)
- [ ] Data logging (add to van_state.h)

## Original Source

The original ModeWifi code is preserved at:
- https://github.com/changer65535/ModeWifi

This fork uses a completely different architecture (ESP32 native CAN vs MCP2515 SPI).

## License

This fork maintains the original **GNU General Public License v3.0** and includes proper attribution to the original author Christopher Hanger (changer65535).

As required by GPL v3, this derivative work is also licensed under GPL v3. All source code is publicly available on GitHub.

## Credits

All CAN protocol reverse engineering credit goes to **changer65535** (original ModeWifi author).

This fork just:
- Changed the hardware interface
- Reorganized the code
- Modernized the tooling
- Optimized for mobile

The hard work of figuring out what the PDM and Rixen messages mean was done by the original author! 🙏

---

**Timeline:** Dec 6, 2025 - Complete rewrite during 2.5-week pre-departure sprint
