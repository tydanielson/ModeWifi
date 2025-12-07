# Van CAN Bus Testing Checklist
**Quick troubleshooting guide for cold weather testing**

## Pre-Test Setup (Inside, Before Going to Van)
- [ ] ESP32 powered via USB
- [ ] Serial monitor running (`platformio device monitor`)
- [ ] Verify "✓ CAN bus started successfully!" appears
- [ ] Laptop ready with long USB cable to reach van door

## Initial Connection Test (5 minutes max)

### Step 1: Basic Connection
1. Connect SN65HVD230 to van CAN wires:
   - Yellow wire → CANH
   - Green wire → CANL
2. Watch serial monitor - should still show "CAN bus started successfully"
3. If ESP32 reboots or crashes → **STOP, disconnect immediately**

### Step 2: Trigger Van Activity (Try each for 10 seconds)
- [ ] Turn ignition to ACC (don't start engine)
- [ ] Toggle headlights on/off
- [ ] Toggle interior lights
- [ ] Adjust climate control temperature
- [ ] Open/close driver door

**Expected:** Messages should appear immediately with any activity

---

## If NO Messages Appear

### Quick Fix #1: Swap CAN-H/CAN-L (30 seconds)
- [ ] Disconnect and swap yellow/green wires
- [ ] Try triggering lights again
- **Reason:** Sometimes CAN-H/CAN-L are reversed

### Quick Fix #2: Wrong Bitrate (2 minutes)
Edit code to try different speeds:
```cpp
// Change this line in main.cpp:
CAN.begin(250E3);  // Try 250kbps instead of 500kbps
// OR
CAN.begin(1000E3); // Try 1000kbps (1Mbps)
```
Upload and retest

### Quick Fix #3: Swap TX/RX Pins (30 seconds)
Sometimes pin definitions are backwards:
```cpp
// In main.cpp, swap these:
#define CAN_TX_PIN 22  // Change to 22
#define CAN_RX_PIN 21  // Change to 21
```
Upload and retest

---

## If Messages DO Appear 🎉

### Verify Message Quality
- [ ] Messages have consistent IDs (e.g., 0x180, 0x280, 0x380)
- [ ] Data changes when you toggle switches
- [ ] No continuous errors or resets

### Quick Functionality Test
1. Toggle headlights - note which message ID changes
2. Adjust climate - note which message ID changes
3. Open door - note which message ID changes
4. Copy 5-10 sample messages for later analysis

### Sample Message Format
```
ID: 0x180  DLC: 8  Data: 01 23 45 67 89 AB CD EF
```

---

## If Van Systems Glitch or Crash

### IMMEDIATE ACTIONS:
1. **Disconnect yellow/green wires from SN65HVD230 NOW**
2. Turn ignition off and back on to reset van systems
3. Check Rixen display - should come back online
4. **DO NOT reconnect until we troubleshoot**

### Possible Causes:
- Wrong bitrate flooding the bus
- Termination resistor issue (unlikely with SN65HVD230)
- Faulty transceiver board

---

## Success Criteria
- ✅ Messages appear when van systems are active
- ✅ Van systems continue to function normally
- ✅ No ESP32 crashes or reboots
- ✅ Message IDs are in reasonable range (0x100-0x7FF typical)

## Next Steps After Success
1. Save serial output with sample messages
2. Disconnect and go back inside
3. We'll analyze message IDs and add decoding logic
4. Re-add WiFi/web interface features

---

## Emergency Contact Info
If van systems crash:
1. Disconnect CAN wires immediately
2. Cycle van ignition
3. Wait for Rixen to reboot (30-60 seconds)
4. Report what happened - we'll investigate before retrying

---

## Common Issues Reference

| Symptom | Most Likely Cause | Fix |
|---------|------------------|-----|
| No messages at all | Wrong bitrate or swapped H/L | Try 250kbps, swap wires |
| Garbage messages | Wrong bitrate | Try different speeds |
| ESP32 crashes | Boot pin issue (shouldn't happen with GPIO21/22) | Check wiring |
| Van systems glitch | Bus interference | Disconnect immediately |
| "CAN bus failed" error | Wiring issue or bad transceiver | Check all 6 connections |

---

## Tools Needed
- ✅ Laptop with USB cable
- ✅ ESP32 with SN65HVD230 on breadboard
- ✅ Access to van CAN yellow/green wires
- ✅ Screwdriver (if needed to access wires)
- ✅ Warm gloves (can type with!)

**Estimated Time:** 5-10 minutes if working, 15 minutes with troubleshooting
