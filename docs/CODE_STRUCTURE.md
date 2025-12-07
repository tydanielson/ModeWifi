# Van Control System - Code Structure

## File Organization

### Main Files
- **src/main.cpp** - Main program loop, CAN message handling, setup
- **platformio.ini** - PlatformIO configuration

### Header Files (Modular Components)
- **src/can_messages.h** - CAN message ID definitions
- **src/van_state.h** - Van state data structure
- **src/can_decoder.h** - PDM and Rixen message decoders
- **src/web_server.h** - Web server handlers and API
- **src/web_interface.h** - HTML/CSS/JavaScript web UI

### Backup Files
- **src/main.cpp.old** - Previous monolithic version (568 lines)
- **src/cm.cpp.old** - Original Arduino MCP2515 code
- **src/cm.h.old** - Original headers with all definitions

### Test Files
- **test_web_interface.html** - Standalone HTML for testing UI locally

## Architecture

```
┌─────────────────┐
│   main.cpp      │  ← Entry point, message loop
└────────┬────────┘
         │
    ┌────┴─────────────────────────────┐
    │                                   │
┌───▼──────────┐              ┌────────▼──────────┐
│ CAN Decoder  │              │   Web Server      │
│              │              │                   │
│ - PDM1/PDM2  │              │ - handleRoot()    │
│ - Rixen HVAC │              │ - handleStatus()  │
│ - Filtering  │              │ - JSON API        │
└───┬──────────┘              └────────┬──────────┘
    │                                   │
    └────────────┬──────────────────────┘
                 │
         ┌───────▼────────┐
         │   Van State    │  ← Shared state
         │                │
         │ - PDM outputs  │
         │ - Temperature  │
         │ - Voltage      │
         └────────────────┘
```

## Benefits of New Structure

1. **Separation of Concerns**
   - CAN decoding logic isolated from web code
   - HTML/CSS/JS in separate file
   - Message definitions centralized

2. **Easier Maintenance**
   - Update web UI without touching CAN code
   - Modify decoders without breaking server
   - Clear dependencies between modules

3. **Better Testing**
   - Test HTML locally (test_web_interface.html)
   - Mock VanState for web testing
   - Unit test decoders independently

4. **Scalability**
   - Easy to add new message types
   - Simple to add new web endpoints
   - Clear place for AWS IoT integration

## Code Size Comparison

| Version | Lines | Structure |
|---------|-------|-----------|
| Old (main.cpp.old) | 568 | Monolithic |
| New (main.cpp) | 210 | Modular |
| Web UI (web_interface.h) | 186 | Separate |
| Decoders (can_decoder.h) | 130 | Separate |

**Total savings**: 42 lines while improving organization

## Next Steps

1. **Test compilation**: ✅ Done (compiles successfully)
2. **Upload to ESP32**: Test WiFi AP and web interface
3. **Van testing**: Verify CAN messages populate web UI
4. **Add controls**: Send CAN commands from web interface
5. **AWS IoT**: Add MQTT client for remote access
