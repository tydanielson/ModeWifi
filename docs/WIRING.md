# ESP32 to CAN Bus Wiring Guide

## Components Required

- **ESP32 Development Board** (ESP32-DevKitC or similar)
- **SN65HVD230 CAN Transceiver Module**
- **Van CAN Bus Connection** (CANH and CANL wires)
- Jumper wires
- Optional: 120Ω termination resistor (if at end of bus)

---

## Wiring Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ESP32 DevKit                                │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────┐      │
│  │                                                          │      │
│  │  GPIO 21 (TX) ────────────────────────┐                 │      │
│  │                                       │                 │      │
│  │  GPIO 22 (RX) ──────────────────┐    │                 │      │
│  │                                  │    │                 │      │
│  │  3.3V ─────────────────────┐    │    │                 │      │
│  │                            │    │    │                 │      │
│  │  GND ──────────────────┐   │    │    │                 │      │
│  │                        │   │    │    │                 │      │
│  └────────────────────────┼───┼────┼────┼─────────────────┘      │
│                           │   │    │    │                        │
└───────────────────────────┼───┼────┼────┼────────────────────────┘
                            │   │    │    │
                            │   │    │    │
┌───────────────────────────┼───┼────┼────┼────────────────────────┐
│                           │   │    │    │   SN65HVD230           │
│  ┌────────────────────────┼───┼────┼────┼──────────────┐         │
│  │                        │   │    │    │              │         │
│  │                      ┌─┴─┐ │  ┌─┴─┐  │              │         │
│  │                  GND │   │ │  │   │ ┌┴┐             │         │
│  │                      └───┘ │  │   │ │ │ CTX (TX)    │         │
│  │                            │  │   │ └─┘             │         │
│  │                        ┌───┴┐ │ ┌─┴─┐               │         │
│  │                    VCC │    │ │ │   │ CRX (RX)      │         │
│  │                        └────┘ │ └───┘               │         │
│  │                               │                     │         │
│  │                           3.3V│                     │         │
│  │                               │                     │         │
│  │                        ┌──────┴──────┐              │         │
│  │                        │   CAN IC    │              │         │
│  │                        │  SN65HVD230 │              │         │
│  │                        │             │              │         │
│  │                        └──────┬──────┘              │         │
│  │                               │                     │         │
│  │                          ┌────┴────┐                │         │
│  │                     CANH │         │ CANL           │         │
│  └──────────────────────────┼─────────┼────────────────┘         │
│                             │         │                          │
└─────────────────────────────┼─────────┼──────────────────────────┘
                              │         │
                              │         │
                    ┌─────────┴─────────┴─────────┐
                    │     Van CAN Bus              │
                    │                              │
                    │  CANH (Yellow wire)          │
                    │  CANL (Green wire)           │
                    └──────────────────────────────┘
```

---

## Pin Connections

### ESP32 to SN65HVD230

| ESP32 Pin | SN65HVD230 Pin | Description |
|-----------|----------------|-------------|
| **3.3V**  | **VCC**        | Power supply (3.3V) |
| **GND**   | **GND**        | Ground |
| **GPIO 21** | **CTX**      | CAN TX (Transmit) |
| **GPIO 22** | **CRX**      | CAN RX (Receive) |

### SN65HVD230 to Van CAN Bus

| SN65HVD230 Pin | Van CAN Wire | Wire Color | Description |
|----------------|--------------|------------|-------------|
| **CANH**       | CAN-H        | Yellow     | CAN High signal |
| **CANL**       | CAN-L        | Green      | CAN Low signal |

> **Note**: Ground is provided through the ESP32's USB power connection (12V→5V buck converter→USB). No separate ground wire to the van CAN bus is needed.

---

## Detailed Wiring Instructions

### Step 1: Power Connections
1. Connect ESP32 **3.3V** pin to SN65HVD230 **VCC**
2. Connect ESP32 **GND** pin to SN65HVD230 **GND**

⚠️ **IMPORTANT**: The SN65HVD230 operates at 3.3V, NOT 5V!

### Step 2: CAN Signal Connections
1. Connect ESP32 **GPIO 21** to SN65HVD230 **CTX** (Transmit)
2. Connect ESP32 **GPIO 22** to SN65HVD230 **CRX** (Receive)

### Step 3: Van CAN Bus Connections
1. Locate the van's CAN bus connector (typically OBD-II port or junction box)
2. Identify the CAN wires:
   - **CANH**: Usually yellow or white/blue
   - **CANL**: Usually green or white/green
3. Connect SN65HVD230 **CANH** to van's **CAN-H** wire
4. Connect SN65HVD230 **CANL** to van's **CAN-L** wire

### Step 4: Power Supply
1. Connect 12V van power to a 5V buck converter (12V→5V)
2. Connect buck converter output to ESP32 via USB cable
3. Ground is automatically provided through the USB power path

⚠️ **IMPORTANT**: Only 2 wires connect to the van CAN bus (CANH and CANL). Power and ground come from the separate 12V→USB power supply.

---

## Physical Layout Example

```
         Van 12V Power
               │
               ▼
         ┌──────────┐
         │  12V→5V  │ Buck converter
         │ Converter│
         └────┬─────┘
              │ USB cable (power + ground)
              ▼
         ┌─────────────┐
         │   ESP32     │
         │  DevKit     │
         │             │
         │  ┌───────┐  │
         │  │ USB-C │  │ ← Power input
         └──┴───────┴──┘
             │││││
        Jumper wires (4)
             │││││
         ┌───┴┴┴┴┴───┐
         │ SN65HVD230│ ← Small blue PCB module
         │  CAN TRX  │
         └─────┬┬────┘
               ││
          CAN wires (2)
               ││
         ┌─────┴┴─────┐
         │  Van CAN   │
         │    Bus     │
         │ (CANH/CANL)│
         └────────────┘
```

---

## Configuration in Code

The pin definitions are set in `src/main.cpp`:

```cpp
#define CAN_TX_PIN 21  // Connect to CTX on SN65HVD230
#define CAN_RX_PIN 22  // Connect to CRX on SN65HVD230
```

If you need to use different GPIO pins, update these values and recompile.

---

## Testing the Connection

### 1. Visual Inspection
- Check all connections are secure
- Verify no crossed wires (TX→CTX, RX→CRX)
- Ensure 3.3V power (NOT 5V!)

### 2. Serial Monitor Test
```bash
pio device monitor --baud 115200
```

Look for:
```
✓ CAN bus started successfully!
Listening for messages...
```

### 3. Verify Messages
You should see CAN messages being received:
```
CAN 0x18FEF117: 8 bytes
CAN 0x18FEF217: 8 bytes
```

---

## Troubleshooting

### No CAN Messages Received

**Problem**: CAN bus starts but no messages appear

**Solutions**:
1. **Check wiring**:
   - Verify CANH and CANL are not swapped
   - Ensure good connection to van CAN bus
2. **Check van state**:
   - Van ignition must be ON or ACC
   - Some vans require engine running
3. **Check CAN speed**:
   - Code uses 500 kbps (standard for Sprinter vans)
   - If different, change: `CAN.begin(500E3)` to `CAN.begin(250E3)`

### CAN Bus Fails to Start

**Problem**: `❌ Failed to start CAN bus!`

**Solutions**:
1. **Check power**:
   - Verify 3.3V at SN65HVD230 VCC pin
   - Check ESP32 is powered properly
2. **Check wiring**:
   - GPIO 21 → CTX
   - GPIO 22 → CRX
   - GND connected
3. **Check SN65HVD230**:
   - Module may be defective - try another one
   - Some modules have S (slope) pin - leave unconnected for high speed

### ESP32 Resets/Crashes

**Problem**: ESP32 keeps resetting

**Solutions**:
1. **Power issue**:
   - USB power may be insufficient
   - Use external 5V power supply (1A minimum)
2. **Power/Ground**:
   - ESP32 powered via USB from 12V buck converter
   - Ground provided through USB power path
   - No separate ground wire to van CAN bus needed

---

## Advanced: Termination Resistor

If your ESP32 is at the **end** of the CAN bus (not recommended), you may need a 120Ω termination resistor between CANH and CANL.

```
CANH ────┬──── 120Ω ────┬──── CANL
         │              │
    SN65HVD230      SN65HVD230
```

Most vans already have termination resistors built-in. **Only add if experiencing bus errors.**

---

## Safety Notes

⚠️ **Important Safety Information**:

1. **Double-check polarity**: Wrong power polarity can damage components
2. **Use 3.3V only**: Do NOT connect 5V to SN65HVD230
3. **Test on bench first**: Verify everything works before installing in van
4. **Van CAN is passive**: This setup only reads CAN data, doesn't control anything
5. **Fuse protection**: Consider adding a fuse on power supply line
6. **Secure connections**: Use soldering or screw terminals in vehicle (not breadboard)

---

## Parts List with Links

| Component | Quantity | Approximate Cost | Notes |
|-----------|----------|-----------------|-------|
| ESP32 DevKit | 1 | $8-15 | ESP32-DevKitC recommended |
| SN65HVD230 Module | 1 | $2-5 | Blue PCB with 4 pins |
| Jumper Wires | 4 | $1-2 | Female-to-female or male-to-female |
| CAN Cable | 1m | $5-10 | 2-conductor shielded twisted pair |
| USB Cable | 1 | $3-5 | For programming and power |
| (Optional) 5V Buck Converter | 1 | $3-8 | For 12V van power → 5V USB |

**Total Cost**: ~$20-30

---

## Additional Resources

- **ESP32 Pinout**: https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
- **SN65HVD230 Datasheet**: https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf
- **CAN Bus Basics**: https://www.csselectronics.com/pages/can-bus-simple-intro-tutorial
- **Project Documentation**: See `docs/ARCHITECTURE.md`

---

## Getting Help

If you run into issues:

1. Check serial monitor output for error messages
2. Verify all connections with a multimeter
3. Take photos of your wiring
4. Review the troubleshooting section above

For code issues, see `docs/ESP32_SETUP.md`
