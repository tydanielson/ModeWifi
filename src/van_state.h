#ifndef VAN_STATE_H
#define VAN_STATE_H

#include <Arduino.h>

// Van state tracking structure
struct VanState {
  uint8_t pdm1_ch1_6[6] = {0};
  uint8_t pdm1_ch7_12[6] = {0};
  uint8_t pdm2_ch1_6[6] = {0};
  uint8_t pdm2_ch7_12[6] = {0};
  float glycolTemp = 0;
  float voltage = 0;
  unsigned long lastUpdate = 0;
};

extern VanState vanState;

#endif
