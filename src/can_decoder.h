#ifndef CAN_DECODER_H
#define CAN_DECODER_H

#include <Arduino.h>
#include "van_state.h"
#include "can_messages.h"

// Helper to calculate feedback amps from PDM message byte
// Based on original ModeWifi code: byte value * 0.125 = Amps
float feedbackAmps(uint8_t* data, int channelNumber) {
  int nByteOffset;
  if ((channelNumber == 1) || (channelNumber == 7)) nByteOffset = 2;
  else if ((channelNumber == 2) || (channelNumber == 8)) nByteOffset = 3;
  else if ((channelNumber == 3) || (channelNumber == 9)) nByteOffset = 4;
  else if ((channelNumber == 4) || (channelNumber == 10)) nByteOffset = 5;
  else if ((channelNumber == 5) || (channelNumber == 11)) nByteOffset = 6;
  else if ((channelNumber == 6) || (channelNumber == 12)) nByteOffset = 7;
  else return 0;
  
  return (float)data[nByteOffset] * 0.125;
}

// Baseline tracking for noise reduction
uint8_t pdm1Ch16Baseline[8] = {0};
uint8_t pdm1Ch712Baseline[8] = {0};
uint8_t pdm2Ch16Baseline[8] = {0};
uint8_t pdm2Ch712Baseline[8] = {0};
bool baselinesSet = false;

// Decode PDM command messages (0x04 = channels 1-6, 0x05 = channels 7-12)
void decodePDMCommand(uint32_t id, uint8_t* data, int len) {
  int pdm = (id == PDM1_COMMAND) ? 1 : 2;
  
  if (len < 2) return;
  
  uint8_t b0 = data[0];
  
  // Channels 1-6 command states
  if (b0 == 0x04) {
    for (int i = 1; i <= 6 && i < len; i++) {
      if (pdm == 1) {
        vanState.pdm1[i].command = data[i];
      } else {
        vanState.pdm2[i].command = data[i];
      }
    }
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  PDM");
      Serial.print(pdm);
      Serial.print(" CMD 1-6: ");
      for (int i = 1; i <= 6 && i < len; i++) {
        Serial.print(pdm == 1 ? vanState.pdm1[i].name : vanState.pdm2[i].name);
        Serial.print("=");
        Serial.print(data[i]);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
  
  // Channels 7-12 command states
  else if (b0 == 0x05) {
    for (int i = 1; i <= 6 && i < len; i++) {
      int ch = i + 6;
      if (pdm == 1) {
        vanState.pdm1[ch].command = data[i];
      } else {
        vanState.pdm2[ch].command = data[i];
      }
    }
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  PDM");
      Serial.print(pdm);
      Serial.print(" CMD 7-12: ");
      for (int i = 1; i <= 6 && i < len; i++) {
        int ch = i + 6;
        Serial.print(pdm == 1 ? vanState.pdm1[ch].name : vanState.pdm2[ch].name);
        Serial.print("=");
        Serial.print(data[i]);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
}

// Decode PDM status/feedback messages (0xF9/0xC9/0x39 = channels 1-6, 0x0A/0xCA/0xFA = channels 7-12)
void decodePDMStatus(uint32_t id, uint8_t* data, int len) {
  int pdm = (id == PDM1_MESSAGE) ? 1 : 2;
  
  if (len < 8) return;
  
  uint8_t b0 = data[0];
  
  // Channels 1-6 feedback (current draw in amps)
  if (b0 == 0xF9 || b0 == 0xC9 || b0 == 0x39) {
    for (int i = 1; i <= 6; i++) {
      float amps = feedbackAmps(data, i);
      if (pdm == 1) {
        vanState.pdm1[i].feedbackAmps = amps;
      } else {
        vanState.pdm2[i].feedbackAmps = amps;
      }
    }
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  PDM");
      Serial.print(pdm);
      Serial.print(" FEEDBACK 1-6: ");
      for (int i = 1; i <= 6; i++) {
        float amps = pdm == 1 ? vanState.pdm1[i].feedbackAmps : vanState.pdm2[i].feedbackAmps;
        if (amps > 0.1) {  // Only show channels with current draw
          Serial.print(pdm == 1 ? vanState.pdm1[i].name : vanState.pdm2[i].name);
          Serial.print("=");
          Serial.print(amps, 2);
          Serial.print("A ");
        }
      }
      Serial.println();
    }
  }
  
  // Channels 7-12 feedback
  else if (b0 == 0x0A || b0 == 0xCA || b0 == 0xFA) {
    for (int i = 7; i <= 12; i++) {
      float amps = feedbackAmps(data, i);
      if (pdm == 1) {
        vanState.pdm1[i].feedbackAmps = amps;
      } else {
        vanState.pdm2[i].feedbackAmps = amps;
      }
    }
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  PDM");
      Serial.print(pdm);
      Serial.print(" FEEDBACK 7-12: ");
      for (int i = 7; i <= 12; i++) {
        float amps = pdm == 1 ? vanState.pdm1[i].feedbackAmps : vanState.pdm2[i].feedbackAmps;
        if (amps > 0.1) {
          Serial.print(pdm == 1 ? vanState.pdm1[i].name : vanState.pdm2[i].name);
          Serial.print("=");
          Serial.print(amps, 2);
          Serial.print("A ");
        }
      }
      Serial.println();
    }
  }
}

// Decode tank level messages (fuel, water, etc.)
void decodeTankLevel(uint32_t id, uint8_t* data, int len) {
  // Tank level message: b[0]=tank type, b[1]=level, b[2]=resolution
  if (id == TANK_LEVEL && len >= 3) {
    uint8_t tankType = data[0];
    uint8_t level = data[1];
    uint8_t resolution = data[2];
    
    // 0x00 = fresh water, 0x02 = gray water, others = fuel
    if (tankType != 0x00 && tankType != 0x02) {
      // This is fuel - calculate percentage
      if (resolution > 0) {
        vanState.fuelLevel = (level * 100.0) / resolution;
      } else {
        vanState.fuelLevel = level;  // Use raw value if resolution is 0
      }
      vanState.lastUpdate = millis();
      
      if (!baselinesSet) {
        Serial.print("  -> FUEL: ");
        Serial.print(level);
        Serial.print("/");
        Serial.print(resolution);
        Serial.print(" = ");
        Serial.print(vanState.fuelLevel, 1);
        Serial.println("%");
      }
    }
  }
}

void decodeRixens(uint32_t id, uint8_t* data, int len) {
  // Debug: Print all Rixens messages during baseline setting
  if (!baselinesSet && (id == 0x724 || id == 0x725 || id == 0x726 || id == 0x728 || id == 0x78A || id == 0x78B || id == THERMOSTAT_AMBIENT_STATUS || id == TANK_LEVEL)) {
    Serial.print("  RAW 0x");
    Serial.print(id, HEX);
    Serial.print(": ");
    for (int i = 0; i < len; i++) {
      if (data[i] < 0x10) Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
  
  // Helper to decode bytes to Celsius (from original repo)
  auto bytes2DegreesC = [](uint8_t b1, uint8_t b2) -> float {
    return ((int)b1 + (int)b2 * 0x100) * 0.03125 - 273.0;
  };
  
  if (id == THERMOSTAT_AMBIENT_STATUS && len >= 3) {
    // Cabin/interior temperature from thermostat
    vanState.cabinTemp = bytes2DegreesC(data[1], data[2]);
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  -> Cabin Temp: ");
      Serial.print(vanState.cabinTemp);
      Serial.println("°C");
    }
  }
  else if (id == RIXENS_GLYCOL && len >= 8) {
    // Bytes 0-1: Glycol inlet temp (LSB, MSB)
    // Bytes 2-3: Glycol outlet temp (LSB, MSB) 
    float glycolOutlet = (data[3] * 256 + data[2]) / 100.0;
    float glycolInlet = (data[1] * 256 + data[0]) / 100.0;
    
    // Bytes 6-7: Voltage (LSB, MSB, divide by 10)
    float volts = (data[7] * 256 + data[6]) / 10.0;
    
    // Use outlet temp as primary glycol temp
    vanState.glycolTemp = glycolOutlet;
    vanState.voltage = volts;
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  -> GLYCOL Outlet: ");
      Serial.print(glycolOutlet);
      Serial.print("°C, Inlet: ");
      Serial.print(glycolInlet);
      Serial.print("°C, Voltage: ");
      Serial.print(volts);
      Serial.println("V");
    }
  }
  else if (id == RIXENS_RETURN4 && len >= 4) {
    // 0x725: Fan speed (bytes 4-5), fuel level (byte 3 - not reliable), glow (bytes 6-7)
    vanState.fanSpeed = len >= 6 ? (data[5] * 256 + data[4]) : 0;
    // Note: byte 3 fuel level not used - get from TANK_LEVEL message instead
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  -> Fan: ");
      Serial.print(vanState.fanSpeed);
      Serial.println();
    }
  }
  else if (id == RIXENS_RETURN3 && len >= 7) {
    // 0x724: Heat source selection
    // Bit 51 (byte 6, bit 3) = Furnace
    // Bit 52 (byte 6, bit 4) = Electric
    // Bit 53 (byte 6, bit 5) = Engine
    vanState.heatSource = data[6];
    vanState.lastUpdate = millis();
    
    if (!baselinesSet) {
      Serial.print("  -> Heat source: 0x");
      Serial.println(data[6], HEX);
    }
  }
}

void setBaselines(uint8_t* pdm1_16, uint8_t* pdm1_712, uint8_t* pdm2_16, uint8_t* pdm2_712) {
  memcpy(pdm1Ch16Baseline, pdm1_16, 8);
  memcpy(pdm1Ch712Baseline, pdm1_712, 8);
  memcpy(pdm2Ch16Baseline, pdm2_16, 8);
  memcpy(pdm2Ch712Baseline, pdm2_712, 8);
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
