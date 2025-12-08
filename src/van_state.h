#ifndef VAN_STATE_H
#define VAN_STATE_H

#include <Arduino.h>

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
  
  // Heater data
  float glycolTemp = 0;
  float voltage = 0;
  float fuelLevel = 0;        // Fuel level percentage
  uint16_t fanSpeed = 0;      // Heater fan speed
  uint8_t heatSource = 0;     // Heat source (0x724)
  float cabinTemp = 0;        // Interior cabin temperature (°C)
  
  unsigned long lastUpdate = 0;
};

extern VanState vanState;

#endif
