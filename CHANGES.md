# Project Changes Log

## November 25, 2025

### Safety and Security Enhancements

All changes documented here address critical safety issues identified in the security review before deployment to the van.

---

### 1. WiFi Network Connection Enhancement

**Problem:**
The ESP32 was hardcoded to always start in Access Point (AP) mode, creating its own "Commmode" network. This required users to manually switch WiFi networks to connect to the device.

**Solution:**
Modified the `setup()` function to attempt connecting to existing WiFi networks before falling back to AP mode.

**Changes Made:**

**File: `src/main.cpp` - Modified `setup()` function (around line 1675)**
- Added logic to check if any WiFi credentials exist (`connectionsIndex > 0`)
- Calls `findExistingAP()` to scan and connect to known networks
- Only creates Access Point if no known networks are found
- Added debug messages for better visibility of connection process

**How to Use:**

1. **Add WiFi credentials to `data/config.txt`:**
   ```
   wifi YourNetworkName,YourPassword
   wifi AnotherNetwork,AnotherPassword
   ```

2. **Upload the filesystem data:**
   - Use PlatformIO's "Upload Filesystem Image" command
   - Or manually upload via the web interface (if already running)

3. **Flash the updated firmware:**
   ```bash
   platformio run -t upload
   ```

**Behavior:**
- **With WiFi credentials:** Device scans for known networks → Connects if found → Access via router IP or `http://cm.local`
- **Without WiFi credentials:** Device creates "Commmode" AP → Access via `192.168.8.100`
- **Connection fails:** Device falls back to AP mode automatically

---

### 2. Buffer Overflow Protection (CRITICAL FIX)

**Problem:**
The serial command handler could write past the end of the buffer if more than 32 characters were sent, causing memory corruption, system crashes, or unpredictable behavior.

**Risk:** HIGH - Could crash ESP32 during operation, potentially leaving van systems in unknown state.

**Solution:**
Modified `handleSerial()` to check buffer bounds BEFORE writing data.

**File: `src/main.cpp` - `handleSerial()` function (line 1373)**

**Before (Vulnerable):**
```cpp
if (ch != 0x0a) {
    currentCommand[cIndex] = ch;  // Write first
    cIndex++;                      
}
if (cIndex >= sizeof(currentCommand)) {  // Check too late!
    Serial.println("Serial Error");
    return;
}
```

**After (Safe):**
```cpp
if (ch != 0x0a) {
    if (cIndex < sizeof(currentCommand) - 1) {  // Check FIRST
        currentCommand[cIndex] = ch;             // Then write
        cIndex++;
    } else {
        Serial.println("Serial Error: Command too long - ignoring");
        cIndex = 0;  // Reset for next command
        return;
    }
}
```

**Impact:**
- System can no longer crash from overly long serial commands
- Buffer corruption prevented
- Error message logged for debugging

---

### 3. File Operation Validation (CRITICAL FIX)

**Problem:**
Web interface allowed deletion or modification of ANY file on the filesystem without validation. Attackers could delete critical config files, corrupt the filesystem, or cause denial of service.

**Risk:** HIGH - Could corrupt config files causing system malfunction on next boot.

**Solution:**
Added whitelist validation for file operations and size limits.

**File: `src/main.cpp` - `handleAJAX()` function (lines 703-760)**

**Security Measures Added:**

1. **File Write Protection:**
   - Only allows writing to: `config.txt`, `filters.txt`, `ssid.txt`, `rixens.txt`, `examples.html`
   - Maximum file size: 10KB (prevents filesystem DOS)
   - Returns 403 Forbidden for unauthorized files
   - Returns 413 Payload Too Large for oversized files

2. **File Delete Protection:**
   - Only allows deletion of: `examples.html`, `test.html`, `upload.html`
   - Critical config files CANNOT be deleted
   - Returns 403 Forbidden for unauthorized deletions

**Code Added:**
```cpp
// Validate filename whitelist
if (szFname != "config.txt" && szFname != "filters.txt" && ...) {
    server.send(403, "text/plain", "{\"error\":\"File not allowed\"}");
    return;
}

// Validate file size
if (szContents.length() > 10240) {  // 10KB limit
    server.send(413, "text/plain", "{\"error\":\"File too large\"}");
    return;
}
```

---

### 4. Watchdog Timer Protection

**Problem:**
If the code hangs (infinite loop, waiting for network, etc.), the ESP32 would become unresponsive indefinitely with no automatic recovery.

**Risk:** MEDIUM - System could hang requiring manual power cycle in potentially dangerous situations.

**Solution:**
Implemented hardware watchdog timer with 30-second timeout.

**Files Modified:**
- `src/main.cpp` - Added `#include <esp_task_wdt.h>` (line 14)
- `src/main.cpp` - `setup()` function (line 1698) - Initialize watchdog
- `src/main.cpp` - `loop()` function (line 1785) - Reset watchdog each cycle

**How It Works:**
```cpp
// In setup():
esp_task_wdt_init(30, true);   // 30 second timeout
esp_task_wdt_add(NULL);        // Monitor main task

// In loop():
esp_task_wdt_reset();  // "I'm still alive!"
```

**Behavior:**
- If `loop()` stops executing for 30+ seconds, ESP32 automatically reboots
- Protects against: infinite loops, network hangs, CAN bus deadlocks
- Serial output: "Serial OK - Watchdog enabled (30s)" on boot

---

### 5. CAN Message Validation

**Problem:**
CAN message handlers accessed data array elements without checking if the message actually contained enough bytes, potentially reading garbage data or causing crashes.

**Risk:** MEDIUM - Could process invalid data from CAN bus, leading to incorrect system state or crashes.

**Solution:**
Added data length validation to all critical CAN message handlers before accessing array elements.

**Files Modified:** `src/cm.cpp`

**Handlers Updated:**
1. **`handleTankLevel()`** - Validates `can_dlc >= 3` before reading tank data
2. **`handleRixensGlycolVoltage()`** - Validates `can_dlc >= 8` before reading temperature/voltage
3. **`handleThermostatStatus()`** - Validates `can_dlc >= 7` before reading AC data
4. **`handleThermostatAmbientStatus()`** - Validates `can_dlc >= 3` before reading temperature
5. **`handleRoofFanStatus()`** - Validates `can_dlc >= 8` before reading fan data
6. **`handlePDMMessage()`** - Validates `can_dlc >= 1` before checking message type

**Example Protection:**
```cpp
void CM::handleTankLevel(can_frame m) {
    // NEW: Validate before accessing data
    if (m.can_dlc < 3) {
        if (bVerbose) Serial.println("Invalid tank level message (too short)");
        return;
    }
    
    // Now safe to access m.data[0], m.data[1], m.data[2]
    nFreshTankLevel = m.data[1];
    nFreshTankDenom = m.data[2];
}
```

**Impact:**
- Protects against malformed CAN messages
- Logs validation errors when in verbose mode
- Prevents reading uninitialized memory
- Makes system more robust against CAN bus noise/errors

---

## Testing Checklist

Before deploying to van:

- [ ] Verify watchdog triggers after 30s test hang
- [ ] Test buffer overflow with >32 character serial command
- [ ] Attempt to delete `config.txt` via web (should fail)
- [ ] Attempt to upload 20KB file (should fail)
- [ ] Connect to existing WiFi network successfully
- [ ] Test CAN message validation with verbose logging enabled
- [ ] Verify system boots and displays "Watchdog enabled"
- [ ] Test all critical functions: lights, HVAC, pumps
- [ ] Monitor CAN bus traffic for unexpected messages

---

## Build Configuration Changes

**Previous Build Issues Fixed:**
- Fixed include path: Changed `"CM.h"` to `"cm.h"` (case-sensitive)
- Updated library dependencies in `platformio.ini`:
  - Changed from `mcp_can` to `autowp-mcp2515` (provides `mcp2515.h`)
  - Added `ArduinoJson @ ^6.21.0` library
- Generated `compile_commands.json` for IntelliSense support

---

## Security Posture Summary

| Issue | Before | After | Risk Reduction |
|-------|--------|-------|----------------|
| Web Authentication | ❌ None | ⚠️ Still needed | N/A - Skipped for now |
| Buffer Overflow | ❌ Vulnerable | ✅ Protected | HIGH → NONE |
| File Operations | ❌ Unvalidated | ✅ Whitelisted | HIGH → LOW |
| System Hangs | ❌ Manual recovery | ✅ Auto-reboot | MEDIUM → LOW |
| CAN Validation | ❌ None | ✅ Length checked | MEDIUM → LOW |
| Watchdog Timer | ❌ Disabled | ✅ 30s timeout | MEDIUM → LOW |

---

## Recommended Next Steps

1. **Consider adding web authentication** - Current system has no password protection
2. **Enable CAN error checking** - Check `sendMessage()` return codes
3. **Implement safe shutdown sequence** - Before `ESP.restart()`, turn off all loads
4. **Test in read-only mode first** - Use `mcp2515.setListenOnlyMode()` initially
5. **Add physical kill switch** - Emergency disable for CAN transmit

---

**Review Date:** November 25, 2025  
**Code Version:** Current main branch  
**Safety Review:** See `SAFETY_REVIEW.md` for complete details
