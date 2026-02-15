#ifndef VAN_STATE_H
#define VAN_STATE_H

#include <Arduino.h>

// CAN message tracking structure (shared between main.cpp and web_server.h)
struct MessageTracker {
  uint32_t id;
  uint32_t count;
  uint8_t lastData[8];
  uint8_t dlc;
};

// PDM channel structure with name and feedback current
struct PDMChannel {
  const char* name;
  uint8_t command;      // On/off command state (0-255)
  float feedbackAmps;   // Current draw in Amps
};

// Van state tracking structure
struct VanState {
  // PDM1 channels (1-12)
  PDMChannel pdm1[13] = {
    {"", 0, 0},                    // Index 0 unused
    {"SOLAR_BACKUP", 0, 0},        // 1
    {"CARGO_LIGHTS", 0, 0},        // 2
    {"READING_LIGHT", 0, 0},       // 3
    {"CABIN_LIGHTS", 0, 0},        // 4
    {"AWNING_LIGHTS", 0, 0},       // 5
    {"RECIRC_PUMP", 0, 0},         // 6
    {"AWNING_ENABLE", 0, 0},       // 7
    {"PDM_1_8", 0, 0},             // 8
    {"PDM_1_9", 0, 0},             // 9
    {"EXHAUST_FAN", 0, 0},         // 10
    {"FURNACE_POWER", 0, 0},       // 11
    {"WATER_PUMP", 0, 0}           // 12
  };
  
  // PDM2 channels (1-12)
  PDMChannel pdm2[13] = {
    {"", 0, 0},                    // Index 0 unused
    {"PDM_2_1", 0, 0},             // 1
    {"GALLEY_FAN", 0, 0},          // 2
    {"REFRIGERATOR", 0, 0},        // 3
    {"12V_USB", 0, 0},             // 4
    {"AWNING_M_PLUS", 0, 0},       // 5
    {"AWNING_M_MINUS", 0, 0},      // 6
    {"TANK_MON_PWR", 0, 0},        // 7
    {"POWER_SW", 0, 0},            // 8
    {"HVAC_POWER", 0, 0},          // 9
    {"12V_SPEAKER", 0, 0},         // 10
    {"SINK_PUMP", 0, 0},           // 11
    {"AUX_POWER", 0, 0}            // 12
  };
  
  // Heater data (from Rixen CAN messages - only when HVAC active)
  float glycolTemp = 0;
  float voltage = 0;
  float fuelLevel = -1;       // Diesel fuel level % (-1 = no data)
  uint16_t fanSpeed = 0;      // Heater fan speed
  uint8_t heatSource = 0;     // Heat source (0x724)
  float cabinTemp = 0;        // Interior cabin temperature (°C)
  
  // Tank levels (-1 = no data received yet)
  float freshWaterLevel = -1;
  float grayWaterLevel = -1;
  
  // PDM sub-message tracking -- per b0 type raw data for debugging
  uint32_t pdm1SubTypeCounts[16] = {0};  // Count by high nibble
  uint8_t pdm1_lastF9[8] = {0};   // Feedback ch1-6
  uint8_t pdm1_lastC9[8] = {0};   // Feedback ch1-6 (alt)
  uint8_t pdm1_last0A[8] = {0};   // Feedback ch7-12
  uint8_t pdm1_lastFC[8] = {0};   // Handshake/motor model
  uint8_t pdm1_lastFD[8] = {0};   // Diagnostics
  uint8_t pdm1_lastFB[8] = {0};   // Supply voltage
  uint8_t pdm1_lastFE[8] = {0};   // Heartbeat
  uint8_t pdm1_lastF0[8] = {0};   // Digital inputs 1-6
  uint8_t pdm1_lastF8[8] = {0};   // Digital inputs 7-12
  
  // AC state (from THERMOSTAT_STATUS_1)
  uint8_t acOperatingMode = 0;  // 0=off, 1=cool, 2=heat, 3=auto, 4=fan
  uint8_t acFanMode = 0;        // 0=auto, 1=always on
  uint8_t acFanSpeed = 0;
  float acSetpointCool = 20.0;  // celsius
  
  unsigned long lastUpdate = 0;
  
  // Last digital input messages from PDM (for button press simulation)
  // These come from messages with ID 0x14EF111E (PDM1) or 0x14EF111F (PDM2)
  // with data[0] = 0xF0 (inputs 1-6) or 0xF8 (inputs 7-12)
  // Note: IDs are raw 29-bit extended CAN IDs (no 0x80000000 flag)
  // Initialize with fake messages so button simulation works even without receiving real 0xF0
  struct {
    uint32_t id;
    uint8_t data[8];
    uint8_t dlc;
    unsigned long timestamp;
  } lastPDM1inputs1to6 = {0x14EF111E, {0xF0, 0, 0, 0, 0, 0, 0, 0}, 8, 0},
    lastPDM1inputs7to12 = {0x14EF111E, {0xF8, 0, 0, 0, 0, 0, 0, 0}, 8, 0},
    lastPDM2inputs1to6 = {0x14EF111F, {0xF0, 0, 0, 0, 0, 0, 0, 0}, 8, 0},
    lastPDM2inputs7to12 = {0x14EF111F, {0xF8, 0, 0, 0, 0, 0, 0, 0}, 8, 0};
};

extern VanState vanState;

#endif
