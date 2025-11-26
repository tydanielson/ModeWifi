
# ModeWifi
WiFi Controller for Storyteller Campervan RV-C CAN Bus

## ⚠️ Safety Notice
***Don't start messing with this stuff unless you know what you're doing. You could brick your van, and Storyteller will not help you. You could also void any warranty you may have.***

**You have been warned!**

## What's New (November 2025)
This project has been migrated to **PlatformIO** with significant safety improvements:

- ✅ **30-second watchdog timer** - automatic recovery from hangs
- ✅ **Buffer overflow protection** - prevents crashes from long commands
- ✅ **File validation** - protects critical configuration files
- ✅ **CAN message validation** - validates message lengths before processing
- ✅ **Non-fatal CAN initialization** - system continues even without CAN hardware
- ✅ **Better dependency management** - exact library versions specified
- ✅ **Professional build system** - consistent builds across machines

See [SAFETY_REVIEW.md](SAFETY_REVIEW.md) and [CHANGES.md](CHANGES.md) for details.

## Features

### What it CAN do:
1. Control A/C (temperature, fan speed, mode)
2. Control Vent Fan (speed, direction)
3. Push Toggle Buttons (lights, pumps, awning)
4. Read Tank Status (fresh, gray, black water levels)
5. Monitor Power (amp draw on circuits)
6. Control Awning (extend/retract)

### What it CANNOT do/control:
1. Rixens (glycol heating system)
2. Dimming lights
3. Battery management
4. Inverter control

*Note: This is due to how ModeCom interfaces with its PDM's/Rixens. It may be possible to completely replace ModeCom with this device, at least on CAN Bus 1.*

## Hardware Requirements

**Parts:**
- [ESP32 DevKit](https://www.amazon.com/dp/B0DB86YJ8D)
- [MCP2515 CAN Controller](https://www.amazon.com/dp/B0B2NWK1QX)
- [DT Connectors for CAN Bus](https://www.amazon.com/dp/B0BVH43P9L)
- [DT Crimper Tool](https://www.amazon.com/dp/B0718T232Z)

**Wiring:**
- MCP2515 CS → ESP32 GPIO 5
- MCP2515 SCK → ESP32 GPIO 18
- MCP2515 MISO → ESP32 GPIO 19
- MCP2515 MOSI → ESP32 GPIO 23
- MCP2515 VCC → ESP32 3.3V
- MCP2515 GND → ESP32 GND
- MCP2515 CAN_H → CAN Bus High
- MCP2515 CAN_L → CAN Bus Low

## Building & Flashing

### Prerequisites
Install [PlatformIO](https://platformio.org/):
```bash
# Using Homebrew (macOS)
brew install platformio

# Or install VS Code with PlatformIO extension
```

### Build and Upload
```bash
# Clone the repository
git clone https://github.com/changer65535/ModeWifi.git
cd ModeWifi

# Build the project
platformio run

# Upload firmware (replace with your serial port)
platformio run -t upload --upload-port /dev/cu.usbserial-0001

# Upload web files to SPIFFS
platformio run -t uploadfs --upload-port /dev/cu.usbserial-0001
```

### Monitor Serial Output
```bash
platformio device monitor
```

## Installation

**Step 1:** Build/Buy a splitter cable using DT plugs and crimper

**Step 2:** Build the board (ESP32 + MCP2515)

**Step 3:** Attach to CAN bus splitter cable
- Connection location: Between CAN Bus 1 and the Rixens heater, under the Groove Lounge
- It has DT-plugs already - just unplug, attach your splitter cable and you're done!

**Step 4:** Upload firmware and web files (see Building & Flashing above)

**Step 5:** Connect to WiFi
- Default AP: "Commmode" (no password)
- Access web interface: http://192.168.8.100
- Or configure to connect to existing WiFi network via web interface

## Network Access

The system creates a WiFi access point "Commmode" at 192.168.8.100.

*Note: mDNS (cm.local) is currently disabled due to ESP32 TCP/IP stack compatibility issues.*

## Serial Commands Reference

### WiFi Commands
- `password` - Set WiFi password
- `addFile` - Add file to SPIFFS
- `printFilters` - Display active CAN filters

### Button Commands
- `pressCargo` - Toggle cargo lights
- `pressCabin` - Toggle cabin lights
- `pressAwning` - Toggle awning lights
- `pressCirc` - Toggle circulation pump
- `pressPump` - Toggle water pump
- `pressDrain` - Toggle drain pump
- `pressAux` - Toggle auxiliary output
- `cabinOn` - Turn cabin lights on
- `lightsOn` - Turn all lights on
- `lightsOff` - Turn all lights off
- `allOff` - Turn everything off (double tap feature)
- `allOffXAux` - Turn all off except auxiliary
- `allOn` - Turn all on
- `printPDM` - Print PDM status
- `verbose` - Toggle verbose mode
- `blink` - Blink LED

### A/C Commands
- `acOff` - Turn A/C off
- `acOn` - Turn A/C on
- `acModeHeat` - Set to heat mode
- `acFanOnly` - Set to fan only mode
- `acFanLow` - Set fan to low speed
- `acFanHigh` - Set fan to high speed
- `acAlwaysOn` - Set fan to always on
- `acAuto` - Set fan to auto mode
- `acSetSpeed <0-255>` - Set fan speed
- `acSetOperatingMode <mode>` - Set operating mode
- `acSetFanMode <mode>` - Set fan mode
- `acSetTemp <temp>` - Set temperature (°C)

### Power Commands
- `printAmps1` - Print amperage on circuit 1
- `printAmps2` - Print amperage on circuit 2

### Awning Commands
- `awningEnable` - Enable awning control
- `awningOut` - Extend awning
- `awningIn` - Retract awning

### Vent Commands
- `openVent` - Open roof vent
- `closeVent` - Close roof vent
- `setVentSpeed <speed>` - Set vent fan speed
- `setVentDir <dir>` - Set vent direction (intake/exhaust)

### Filter Commands
- `rf` - Reset filters
- `filterOut <id>` - Add filter out
- `filterIn <id>` - Add filter in
- `filterMode <mode>` - Set filter mode
- `clearFilters` - Clear all filters
- `changeMask <mask>` - Change filter mask
- `filterOutB0` - Filter out bank 0
- `filterInB0` - Filter in bank 0
- `showChangeOnly` - Show only changed values
- `parseRaw` - Parse raw CAN messages

### System Commands
- `reset` - Reset system
- `quickSSID <name>` - Quick SSID configuration
- `wifi <ssid> <password>` - Configure WiFi
- `webOn` - Enable web server
- `webOff` - Disable web server
- `start` - Start CAN monitoring
- `stop` - Stop CAN monitoring

## Project Structure

```
ModeWifi/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp           # Main application code (formerly BlinkyVan.ino)
│   ├── cm.h               # CAN message definitions
│   └── cm.cpp             # CAN message handlers
├── include/               # Header files
├── lib/                   # Custom libraries
├── data/                  # Web interface files (uploaded to SPIFFS)
│   ├── index.html
│   ├── jquery.min.js
│   ├── utils.js
│   └── ...
├── CHANGES.md            # Changelog of improvements
├── SAFETY_REVIEW.md      # Security audit results
└── README.md             # This file
```

## Legacy Files

The following files are preserved for reference but are superseded by the `src/` directory:
- `BlinkyVan.ino` - Original Arduino sketch (now `src/main.cpp`)
- `Commode.ino` - Older version
- `CM.h` - Old header (now `src/cm.h`)
- `cm.cpp` - Old implementation (now `src/cm.cpp`)

## Contributing

When contributing, please:
1. Use PlatformIO for building and testing
2. Follow the existing code style
3. Test thoroughly before creating a pull request
4. Update CHANGES.md with your modifications
5. Consider safety implications - this controls critical van systems

## License

See [LICENSE](LICENSE) file.

## Credits

Photos and original project concept from the Storyteller community.

![ESP32 + MCP2515 Setup](https://github.com/user-attachments/assets/bc3380b2-bb1e-4e78-ac02-17fa1cec6312)
![CAN Bus Connection](https://github.com/user-attachments/assets/c38c6dc8-7039-4bed-9ab6-0a4ef97f4efc)
![Installed in Van](https://github.com/user-attachments/assets/866c1593-9f9a-4545-80b3-02fb4a69ca2c)
