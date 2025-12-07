#ifndef CAN_DECODER_H
#define CAN_DECODER_H

#include <Arduino.h>
#include "van_state.h"
#include "can_messages.h"

// PDM1 outputs (what these control)
const char* pdm1Names[] = {
  "", "SOLAR_BACKUP", "CARGO_LIGHTS", "READING_LIGHT", "CABIN_LIGHTS",
  "AWNING_LIGHTS", "RECIRC_PUMP", "AWNING_ENABLE", "PDM_1_8", "PDM_1_9",
  "EXHAUST_FAN", "FURNACE_POWER", "WATER_PUMP"
};

// PDM2 outputs
const char* pdm2Names[] = {
  "", "PDM_2_1", "GALLEY_FAN", "REFRIGERATOR", "12V_USB",
  "AWNING_M_PLUS", "AWNING_M_MINUS", "TANK_MON_PWR", "POWER_SW",
  "HVAC_POWER", "12V_SPEAKER", "SINK_PUMP", "AUX_POWER"
};

// Baseline tracking for noise reduction
uint8_t pdm1Ch16Baseline[7] = {0};
uint8_t pdm1Ch712Baseline[7] = {0};
uint8_t pdm2Ch16Baseline[7] = {0};
uint8_t pdm2Ch712Baseline[7] = {0};
bool baselinesSet = false;

void decodePDMCommand(uint32_t id, uint8_t* data, int len) {
  int pdm = (id == PDM1_COMMAND) ? 1 : 2;
  
  if (len < 2) return;
  
  uint8_t b0 = data[0];
  
  // Channels 1-6
  if (b0 == 0x04) {
    for (int i = 1; i <= 6 && i < len; i++) {
      if (pdm == 1) {
        vanState.pdm1_ch1_6[i-1] = data[i];
      } else {
        vanState.pdm2_ch1_6[i-1] = data[i];
      }
    }
    vanState.lastUpdate = millis();
    
    // Debug output
    if (!baselinesSet) {
      Serial.print("  PDM");
      Serial.print(pdm);
      Serial.print(" Channels 1-6: ");
      for (int i = 1; i <= 6 && i < len; i++) {
        if (i <= 12) {
          Serial.print(pdm == 1 ? pdm1Names[i] : pdm2Names[i]);
          Serial.print("=");
          Serial.print(data[i]);
          Serial.print(" ");
        }
      }
      Serial.println();
    }
  }
  
  // Channels 7-12
  else if (b0 == 0x05) {
    for (int i = 1; i <= 6 && i < len; i++) {
      if (pdm == 1) {
        vanState.pdm1_ch7_12[i-1] = data[i];
      } else {
        vanState.pdm2_ch7_12[i-1] = data[i];
      }
    }
    vanState.lastUpdate = millis();
    
    // Debug output
    if (!baselinesSet) {
      Serial.print("  PDM");
      Serial.print(pdm);
      Serial.print(" Channels 7-12: ");
      for (int i = 1; i <= 6 && i < len; i++) {
        int ch = i + 6;
        if (ch <= 12) {
          Serial.print(pdm == 1 ? pdm1Names[ch] : pdm2Names[ch]);
          Serial.print("=");
          Serial.print(data[i]);
          Serial.print(" ");
        }
      }
      Serial.println();
    }
  }
}

void decodeRixens(uint32_t id, uint8_t* data, int len) {
  if (id == RIXENS_GLYCOL && len >= 5) {
    // Bytes 2-3: Glycol temp (LSB, MSB)
    int16_t tempRaw = (data[3] << 8) | data[2];
    float tempC = tempRaw / 10.0;
    
    // Byte 4: Voltage (multiply by 0.1)
    float volts = data[4] * 0.1;
    
    vanState.glycolTemp = tempC;
    vanState.voltage = volts;
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  GLYCOL Temp: ");
      Serial.print(tempC);
      Serial.print("°C, Voltage: ");
      Serial.print(volts);
      Serial.println("V");
    }
  }
}

void setBaselines(uint8_t* pdm1_16, uint8_t* pdm1_712, uint8_t* pdm2_16, uint8_t* pdm2_712) {
  memcpy(pdm1Ch16Baseline, pdm1_16, 7);
  memcpy(pdm1Ch712Baseline, pdm1_712, 7);
  memcpy(pdm2Ch16Baseline, pdm2_16, 7);
  memcpy(pdm2Ch712Baseline, pdm2_712, 7);
  baselinesSet = true;
  
  Serial.println("\n✓ Baselines set! Now only showing changes...\n");
}

bool hasDataChanged(uint32_t id, uint8_t* data, int len, uint8_t* baseline) {
  if (!baselinesSet) return true;
  
  // Ignore last byte (counter/noise)
  int compareLen = (len > 1) ? len - 1 : len;
  
  for (int i = 0; i < compareLen; i++) {
    if (data[i] != baseline[i]) return true;
  }
  return false;
}

#endif
