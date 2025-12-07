# Quick Setup Guide

Get your ESP32 reading your van's CAN bus in 15 minutes.

## What You Need

### Hardware
- [ ] ESP32 DevKit board
- [ ] SN65HVD230 CAN transceiver board
- [ ] USB cable (for programming)
- [ ] 4-6 dupont wires (or solder)
- [ ] Access to van's CAN bus wires (yellow/green under Groove Lounge)

### Software
- [ ] VS Code installed
- [ ] PlatformIO extension installed
- [ ] This repository cloned

## Step 1: Wire It Up (5 min)

```
ESP32          →    SN65HVD230
---------------------------------
GPIO 21        →    CTX
GPIO 22        →    CRX  
3.3V           →    VCC
GND            →    GND

SN65HVD230     →    Van CAN Bus
---------------------------------
CANH           →    Yellow wire (CAN-H)
CANL           →    Green wire (CAN-L)
```

**Pro tip:** Don't connect to van yet - test the upload first!

## Step 2: Upload Code (5 min)

1. **Open project in VS Code**
   ```bash
   cd VanControl-ESP32
   code .
   ```

2. **Plug in ESP32 via USB**

3. **Upload firmware**
   - Click PlatformIO icon in sidebar
   - Click "Upload" under Project Tasks
   - Wait for "SUCCESS" message

4. **Open serial monitor**
   - Click "Monitor" under Project Tasks
   - You should see startup messages

## Step 3: Test WiFi (2 min)

1. **Look for WiFi network** on your phone
   - SSID: `VanControl`
   - Password: `vanlife2025`

2. **Connect and test**
   - Open browser to: `http://192.168.4.1`
   - You should see the web interface (no data yet)

## Step 4: Connect to Van (3 min)

**⚠️ Make sure van is OFF for first connection!**

1. **Locate CAN bus splice point**
   - Under Groove Lounge seat
   - Between main CAN bus and Rixen heater
   - Yellow (CAN-H) and Green (CAN-L) wires

2. **Connect your transceiver**
   - CANH → Yellow wire
   - CANL → Green wire
   - Use T-tap connectors or DT splitter

3. **Turn on van**

4. **Check serial monitor**
   - You should see "📌 NEW ID: 0x..." messages
   - After 10 seconds: "✓ Baseline set!"
   - PDM and Rixen messages should appear

5. **Refresh web interface**
   - You should now see real data!
   - Voltage, temperature, and PDM states

## Troubleshooting

### No CAN messages
```
Check:
- Van is powered on
- CAN-H/CAN-L not reversed (swap them and try again)
- Transceiver has power (3.3V from ESP32)
- Wiring connections are solid
```

### Serial monitor shows gibberish
```
Solution: Set baud rate to 115200
- Click gear icon in monitor
- Select 115200 baud
```

### WiFi doesn't appear
```
Check:
- ESP32 has adequate power (USB or 5V)
- Serial monitor shows "AP IP address: 192.168.4.1"
- Try rebooting ESP32
```

### Web page loads but no data
```
Check:
- Van is on
- Serial monitor shows CAN messages
- "vanState.lastUpdate" is recent in serial output
```

## Next Steps

Once you have data flowing:

1. **Secure the hardware** - 3D print an enclosure or use project box
2. **Power from van** - Use 12V→5V buck converter instead of USB
3. **Change WiFi password** - Edit `ap_password` in `src/main.cpp`
4. **Customize UI** - Edit `src/web_interface.h` and test with `test_web_interface.html`

## Success Criteria ✅

You're done when you can:
- [x] See "VanControl" WiFi network
- [x] Load web page at 192.168.4.1
- [x] See real voltage (12-14V)
- [x] See real temperature
- [x] See PDM states changing when you press van buttons

**Estimated time:** 15 minutes for first upload, 30 minutes including van installation.

---

Questions? Open an issue on GitHub!
