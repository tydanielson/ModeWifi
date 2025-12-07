/*
 * ModeWifi - ESP32 CAN Bus Interface for Storyteller Overland Sprinter
 * 
 * Based on original work by Christopher Hanger (changer65535)
 * https://github.com/changer65535/ModeWifi
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <CAN.h>
#include <WiFi.h>
#include <WebServer.h>

// Project headers
#include "can_messages.h"
#include "van_state.h"
#include "can_decoder.h"
#include "web_server.h"

// WiFi Access Point settings
const char* ap_ssid = "VanControl";
const char* ap_password = "vanlife2025";

// Global instances
WebServer server(80);
VanState vanState;

// Pin definitions for ESP32 native CAN
#define CAN_TX_PIN 21  // Connect to CTX on SN65HVD230
#define CAN_RX_PIN 22  // Connect to CRX on SN65HVD230

// Message tracking
struct MessageTracker {
  uint32_t id;
  uint32_t count;
  uint8_t lastData[8];
  uint8_t dlc;
};
MessageTracker trackedMessages[50];
int trackedCount = 0;

// Task handles for dual-core operation
TaskHandle_t webServerTask;
TaskHandle_t canBusTask;

// Web server task - runs on Core 0
void webServerTaskFunction(void * parameter) {
  for(;;) {
    server.handleClient();
    vTaskDelay(1); // Yield to other tasks
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== VAN CONTROL SYSTEM ===");
  Serial.println("ESP32 + SN65HVD230 CAN Transceiver");
  
  // Start WiFi Access Point
  Serial.println("\n📶 Starting WiFi Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("   SSID: ");
  Serial.println(ap_ssid);
  Serial.print("   Password: ");
  Serial.println(ap_password);
  Serial.print("   IP Address: ");
  Serial.println(IP);
  Serial.println("   Connect and go to: http://192.168.4.1");
  
  // Setup web server
  setupWebServer();
  Serial.println("✓ Web server started!");
  
  // Set CAN pins and start
  CAN.setPins(CAN_RX_PIN, CAN_TX_PIN);
  Serial.println("\n🚗 Starting CAN bus at 500 kbps...");
  
  if (!CAN.begin(500E3)) {
    Serial.println("❌ Failed to start CAN bus!");
    Serial.println("Check wiring:");
    Serial.println("  ESP32 GPIO 21 → SN65HVD230 CTX");
    Serial.println("  ESP32 GPIO 22 → SN65HVD230 CRX");
    Serial.println("  ESP32 3.3V    → SN65HVD230 VCC");
    Serial.println("  ESP32 GND     → SN65HVD230 GND");
    Serial.println("  SN65HVD230 CANH → Van CAN-H (yellow)");
    Serial.println("  SN65HVD230 CANL → Van CAN-L (green)");
    while(1) delay(1000);
  }
  
  Serial.println("✓ CAN bus started successfully!");
  Serial.println("Listening for messages...\n");
  
  // Create web server task on Core 0 (handles WiFi/networking)
  xTaskCreatePinnedToCore(
    webServerTaskFunction,   // Task function
    "WebServer",             // Name
    10000,                   // Stack size (bytes)
    NULL,                    // Parameter
    1,                       // Priority
    &webServerTask,          // Task handle
    0                        // Core 0 (WiFi/networking)
  );
  
  Serial.println("✓ Web server running on Core 0");
  Serial.println("✓ CAN bus running on Core 1");
}

void loop() {
  // CAN bus processing runs on Core 1 (Arduino default)
  // Web server runs on Core 0 (in separate task)
  
  static unsigned long lastPrint = 0;
  static unsigned long baselineTimer = 0;
  static int msgCount = 0;
  
  // Check for incoming CAN messages
  int packetSize = CAN.parsePacket();
  
  if (packetSize) {
    msgCount++;
    uint32_t msgId = CAN.packetId();
    uint8_t data[8] = {0};
    int dataLen = 0;
    
    // Read the data
    while (CAN.available() && dataLen < 8) {
      data[dataLen++] = CAN.read();
    }
    
    // Find or add this message ID to tracking
    int idx = -1;
    for (int i = 0; i < trackedCount; i++) {
      if (trackedMessages[i].id == msgId) {
        idx = i;
        break;
      }
    }
    
    if (idx == -1 && trackedCount < 50) {
      // New message ID discovered
      idx = trackedCount++;
      trackedMessages[idx].id = msgId;
      trackedMessages[idx].count = 0;
      trackedMessages[idx].dlc = dataLen;
      
      if (!baselinesSet) {
        Serial.print("📌 NEW ID: 0x");
        Serial.print(msgId, HEX);
        if (CAN.packetExtended()) Serial.print(" (EXT)");
        Serial.println();
      }
    }
    
    if (idx >= 0) {
      trackedMessages[idx].count++;
      
      // Check if data changed (ignore last byte for PDM commands)
      bool isPDMCommand = (msgId == PDM1_COMMAND || msgId == PDM2_COMMAND);
      int compareLen = (isPDMCommand && dataLen > 1) ? dataLen - 1 : dataLen;
      
      bool changed = false;
      for (int i = 0; i < compareLen; i++) {
        if (trackedMessages[idx].lastData[i] != data[i]) {
          changed = true;
          break;
        }
      }
      
      // Update stored data
      memcpy(trackedMessages[idx].lastData, data, dataLen);
      
      // Decode important messages
      if (changed || !baselinesSet) {
        if (msgId == PDM1_COMMAND || msgId == PDM2_COMMAND) {
          decodePDMCommand(msgId, data, dataLen);
        } else if (msgId == RIXENS_GLYCOL) {
          decodeRixens(msgId, data, dataLen);
        }
      }
    }
    
    // Set baselines after 10 seconds
    if (!baselinesSet && millis() > 10000) {
      uint8_t pdm1_16[7] = {0}, pdm1_712[7] = {0};
      uint8_t pdm2_16[7] = {0}, pdm2_712[7] = {0};
      
      // Find PDM command messages and store their current values
      for (int i = 0; i < trackedCount; i++) {
        if (trackedMessages[i].id == PDM1_COMMAND) {
          if (trackedMessages[i].lastData[0] == 0x04) {
            memcpy(pdm1_16, trackedMessages[i].lastData, 7);
          } else if (trackedMessages[i].lastData[0] == 0x05) {
            memcpy(pdm1_712, trackedMessages[i].lastData, 7);
          }
        } else if (trackedMessages[i].id == PDM2_COMMAND) {
          if (trackedMessages[i].lastData[0] == 0x04) {
            memcpy(pdm2_16, trackedMessages[i].lastData, 7);
          } else if (trackedMessages[i].lastData[0] == 0x05) {
            memcpy(pdm2_712, trackedMessages[i].lastData, 7);
          }
        }
      }
      
      setBaselines(pdm1_16, pdm1_712, pdm2_16, pdm2_712);
    }
  }
  
  // Print summary every 30 seconds
  if (millis() - lastPrint >= 30000) {
    lastPrint = millis();
    Serial.println("\n========== 30-SEC SUMMARY ==========");
    Serial.print("Messages received: ");
    Serial.println(msgCount);
    Serial.print("Unique IDs tracked: ");
    Serial.println(trackedCount);
    Serial.print("Van state last updated: ");
    if (vanState.lastUpdate > 0) {
      Serial.print((millis() - vanState.lastUpdate) / 1000);
      Serial.println(" seconds ago");
    } else {
      Serial.println("never");
    }
    Serial.println("====================================\n");
  }
}
