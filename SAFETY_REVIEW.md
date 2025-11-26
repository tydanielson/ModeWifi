# Safety Review - Van CAN Bus Controller

## Executive Summary

I've completed a comprehensive code review of your van's CAN bus controller system. The code interfaces with critical van systems including HVAC, lighting, water pumps, and power distribution. **Overall, the code is reasonably safe, but there are several issues that should be addressed before connecting to your van.**

---

## 🔴 CRITICAL ISSUES (Fix Before Deployment)

### 1. **No Authentication on Web Interface**
**Severity:** CRITICAL  
**Risk:** Anyone on the WiFi network can control all van systems

**Location:** `src/main.cpp` - `handleAJAX()` function (line 688)

**Problem:**
- Web interface has NO password protection
- Any device on WiFi can send commands via HTTP POST
- Commands like `allOff`, `reset`, `acSetTemp` are completely unprotected

**Impact:**
- Malicious user could turn off HVAC in extreme temperatures
- Could drain water tanks by activating pumps
- Could cycle critical systems causing damage
- Could upload malicious config files

**Recommendation:**
```cpp
// Add authentication check to handleAJAX()
const char* API_KEY = "your-secret-key-here"; // Change this!

void handleAJAX() {
    String authKey = server.arg("apiKey");
    if (authKey != API_KEY) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    // ... rest of code
}
```

**Alternative:** Use HTTP Basic Authentication or token-based auth.

---

### 2. **Buffer Overflow in Serial Command Handler**
**Severity:** CRITICAL  
**Risk:** System crash or undefined behavior

**Location:** `src/main.cpp` - `handleSerial()` (line 1373)

**Problem:**
```cpp
static char currentCommand[32];
static int cIndex = 0;

if (ch != 0x0a) {
    currentCommand[cIndex] = ch;
    cIndex++;
}
if (cIndex >= sizeof(currentCommand)) {
    Serial.println("Serial Error");
    return;  // ⚠️ TOO LATE! Already wrote past buffer
}
```

**Impact:**
- If someone sends >32 characters via serial, buffer overflow occurs
- Could crash ESP32 or cause unpredictable behavior
- CAN bus could be left in unknown state

**Fix:**
```cpp
if (ch != 0x0a) {
    if (cIndex < sizeof(currentCommand) - 1) {  // Check BEFORE writing
        currentCommand[cIndex] = ch;
        cIndex++;
    } else {
        Serial.println("Command too long - ignored");
        cIndex = 0;  // Reset
        return;
    }
}
```

---

### 3. **Unvalidated File Operations**
**Severity:** HIGH  
**Risk:** File system corruption, denial of service

**Location:** `src/main.cpp` - `handleAJAX()` (lines 703-717, 721-727)

**Problem:**
- No validation on filenames from web requests
- Could delete critical system files
- Could fill filesystem with garbage data
- No size limits on uploaded files

**Examples:**
```cpp
// Anyone can delete ANY file
if (szCommand == "deleteFile") {
    String szFname = server.arg("fname");
    SPIFFS.remove("/" + szFname);  // ⚠️ No validation!
}

// Anyone can overwrite ANY file with ANY content
if (szCommand == "saveFile") {
    String szFname = server.arg("fname");
    String szContents = server.arg("contents");
    File file = SPIFFS.open("/" + szFname, FILE_WRITE);
    file.print(szContents);  // ⚠️ No size limit!
}
```

**Fix:**
```cpp
// Whitelist allowed files
bool isAllowedFile(String fname) {
    return (fname == "config.txt" || 
            fname == "filters.txt" || 
            fname == "ssid.txt" || 
            fname == "rixens.txt");
}

// Add size limits
const size_t MAX_FILE_SIZE = 10240; // 10KB
if (szContents.length() > MAX_FILE_SIZE) {
    server.send(413, "text/plain", "File too large");
    return;
}
```

---

## 🟡 HIGH PRIORITY WARNINGS

### 4. **No CAN Bus Message Validation**
**Severity:** HIGH  
**Risk:** Processing malformed messages could cause system malfunction

**Location:** `src/cm.cpp` - Multiple message handlers

**Problem:**
- No validation that CAN messages have expected data length
- Direct array access without bounds checking
- Example: `handlePDMMessage()` assumes data[0-7] exist

**Example Issue:**
```cpp
void CM::handleTankLevel(can_frame m) {
    if (m.data[0] == 0x00) {  // ⚠️ No check that can_dlc >= 3
        nFreshTankLevel = m.data[1];
        nFreshTankDenom = m.data[2];  // Could read garbage
    }
}
```

**Fix:**
```cpp
void CM::handleTankLevel(can_frame m) {
    if (m.can_dlc < 3) {
        Serial.println("Invalid tank level message");
        return;
    }
    if (m.data[0] == 0x00) {
        nFreshTankLevel = m.data[1];
        nFreshTankDenom = m.data[2];
    }
}
```

---

### 5. **Unsafe ESP.restart() Command**
**Severity:** HIGH  
**Risk:** System reset during critical operation

**Location:** `src/main.cpp` - line 1287

**Problem:**
- `reset` command immediately restarts ESP32
- No checks if pumps are running, HVAC is active, etc.
- Could leave systems in unsafe state
- Anyone with web/serial access can trigger

**Fix:**
```cpp
if (szCommand.startsWith("reset")) {
    // Safe shutdown sequence
    Serial.println("Initiating safe shutdown...");
    cm.allOff();  // Turn off all loads
    delay(2000);  // Give CAN bus time to process
    server.send(200, "text/plain", "Restarting...");
    delay(100);
    ESP.restart();
}
```

---

### 6. **Infinite Loop on SPIFFS Failure**
**Severity:** MEDIUM-HIGH  
**Risk:** System becomes unresponsive if filesystem fails

**Location:** `src/main.cpp` - `setupSPIFFS()` (line 1407)

**Problem:**
```cpp
if (!SPIFFS.begin(true)) {
    displayMessage("SPIFFS FAIL");
    while (true) {  // ⚠️ Infinite loop!
    }
}
```

**Impact:**
- If SPIFFS is corrupted, system hangs forever
- No way to recover without reflashing
- Watchdog timer not enabled

**Fix:**
```cpp
int retries = 3;
while (!SPIFFS.begin(true) && retries > 0) {
    Serial.println("SPIFFS mount failed, retrying...");
    delay(1000);
    retries--;
}
if (retries == 0) {
    Serial.println("SPIFFS failed - formatting...");
    SPIFFS.format();
    SPIFFS.begin(true);
}
```

---

## 🟢 MEDIUM PRIORITY ISSUES

### 7. **No Watchdog Timer**
**Severity:** MEDIUM  
**Risk:** System could hang indefinitely

**Problem:** ESP32 has no watchdog timer enabled. If code hangs (e.g., waiting for CAN message), system becomes unresponsive.

**Fix:**
```cpp
// In setup()
esp_task_wdt_init(30, true);  // 30 second timeout
esp_task_wdt_add(NULL);

// In loop()
esp_task_wdt_reset();  // Reset watchdog
```

---

### 8. **CAN Bus Error Handling Missing**
**Severity:** MEDIUM  
**Risk:** Could send invalid messages to van's CAN bus

**Location:** `src/cm.cpp` - All `sendMessage()` calls

**Problem:**
- No error checking after `mcp2515.sendMessage()`
- Don't know if message was actually sent
- Could fail silently

**Fix:**
```cpp
MCP2515::ERROR result = mPtr->sendMessage(&m);
if (result != MCP2515::ERROR_OK) {
    Serial.print("CAN send failed: ");
    Serial.println(result);
}
```

---

### 9. **Hardcoded Credentials in Code**
**Severity:** MEDIUM  
**Risk:** Exposed secrets if code is shared

**Location:** `src/main.cpp` (lines 26-27, 428-429)

**Problem:**
```cpp
const char* hostURL = "http://130.211.161.177/cv/cvAjax.php";
const char* szSecret = "dfgeartdsfcvbfgg53564fgfgh";  // ⚠️ Plaintext secret
const char* ssidWAP = "Commmode";
const char* passwordWAP = "123456789";  // ⚠️ Weak password
```

**Fix:**
- Move to config file
- Use environment variables or secure storage
- Implement proper encryption for secrets

---

### 10. **Potential Race Conditions**
**Severity:** MEDIUM  
**Risk:** PDM state could become inconsistent

**Location:** Multiple locations in `cm.cpp`

**Problem:**
- Global state variables modified from multiple contexts
- No mutex protection on shared data
- CAN bus handlers and loop() functions both modify same data

**Example:**
```cpp
// Multiple functions modify lastACCommand without protection
lastACCommand = m;  // Could be interrupted mid-write
```

---

## 🔵 LOW PRIORITY / CODE QUALITY ISSUES

### 11. **Syntax Error in Header File**
**Location:** `src/cm.h` line 100

```cpp
#define PDM2_DIN_MASTER_LIGHT_SW 6 f  // ⚠️ 'f' should not be here
```

**Fix:** Remove the trailing `f`

---

### 12. **Memory Leaks in String Operations**
**Location:** Various locations in `main.cpp`

**Problem:** Heavy use of String concatenation in loops can fragment heap memory.

**Fix:** Use fixed-size char arrays where possible.

---

### 13. **Magic Numbers Throughout Code**
**Example:** `delay(500)`, `delay(100)`, bit masks like `0b00111111`

**Fix:** Define constants with meaningful names.

---

## 📋 TESTING RECOMMENDATIONS

### Before Connecting to Van:

1. **Bench Test with CAN Bus Simulator**
   - Test all commands in isolated environment
   - Verify message formats match RV-C standard
   - Monitor for unintended messages

2. **Enable Verbose Logging**
   - Set `bVerbose = true` in config
   - Monitor all CAN traffic during initial connection
   - Log to SD card for analysis

3. **Implement Kill Switch**
   - Add physical button to disable CAN transmit
   - Emergency stop that cuts power to MCP2515

4. **Test in Read-Only Mode First**
   ```cpp
   mcp2515.setListenOnlyMode();  // Read but don't send
   ```

5. **Start with Non-Critical Systems**
   - Test with lights first (safe to toggle)
   - Avoid HVAC and water systems until proven stable
   - Never test awning motor without supervision

---

## 🛠️ IMMEDIATE ACTION ITEMS (Priority Order)

1. **Fix buffer overflow in handleSerial()** - Could crash system
2. **Add web authentication** - Critical security hole
3. **Add file operation validation** - Prevent filesystem corruption
4. **Implement watchdog timer** - Prevent system hangs
5. **Add CAN message validation** - Prevent processing bad data
6. **Enable verbose logging** - Essential for debugging
7. **Test in read-only mode first** - Verify message formats

---

## 🔒 SECURITY BEST PRACTICES

1. **Change Default Credentials**
   - WiFi password "123456789" is extremely weak
   - Change to strong random password

2. **Disable Unnecessary Features**
   - Remote web upload disabled by default? ✓
   - Consider disabling web interface when not needed

3. **Network Isolation**
   - Don't connect van WiFi to internet if possible
   - Use separate network for van control vs. general use

4. **Regular Updates**
   - Keep PlatformIO and libraries updated
   - Review security advisories for ESP32

---

## 📝 ADDITIONAL NOTES

### Positive Aspects:
- Good use of RV-C standard protocol
- Reasonable separation of concerns (CM class)
- Digital input state tracking prevents unnecessary CAN traffic
- Decent error messages for debugging

### Code Maturity:
- This appears to be actively developed code
- Some commented-out code suggests ongoing testing
- Good documentation of CAN message formats in comments

---

## ⚠️ DISCLAIMER

This review identifies potential issues but cannot guarantee safety. **Always:**
- Test thoroughly in isolated environment first
- Have manual overrides for all critical systems
- Monitor behavior during initial deployment
- Keep manufacturer's original controllers accessible
- Document all changes and keep backups

**When in doubt, consult with an RV electrician or automotive systems engineer.**

---

## 📞 Questions to Consider

1. Do you have schematics of your van's CAN bus topology?
2. Are there other devices on the bus that could conflict?
3. Do you have a CAN bus analyzer for debugging?
4. What's your rollback plan if something goes wrong?
5. Have you contacted the PDM/HVAC manufacturers about third-party control?

---

**Review Date:** November 25, 2025  
**Code Version:** Current main branch  
**Reviewer:** AI Code Analysis
