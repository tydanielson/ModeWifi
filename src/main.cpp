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
#include <ArduinoOTA.h>

// Project headers
#include "can_messages.h"
#include "van_state.h"
#include "can_decoder.h"
#include "wifi_manager.h"
#include "web_server.h"  // Must be before aws_iot.h (button functions)
#include "aws_iot.h"

// Global instances
WiFiManager wifiManager;
AWSIoT awsIot;
WebServer server(80);
VanState vanState;

// Pin definitions for ESP32 native CAN
#define CAN_TX_PIN 21  // Connect to CTX on SN65HVD230
#define CAN_RX_PIN 22  // Connect to CRX on SN65HVD230

// Message tracking (MessageTracker struct defined in van_state.h)
MessageTracker trackedMessages[100];  // Increased from 50 for safety
int trackedCount = 0;
int totalMsgCount = 0;  // Exposed for debug endpoint

// CAN bus mutex for thread-safe access between cores
SemaphoreHandle_t canMutex;

// Task handles for dual-core operation
TaskHandle_t networkTask;  // WiFi + AWS IoT + Web Server on Core 0
TaskHandle_t canBusTask;    // CAN bus processing on Core 1

// Network task - runs on Core 0 (handles WiFi, MQTT, Web)
void networkTaskFunction(void * parameter) {
  unsigned long lastTelemetryCheck = 0;
  
  for(;;) {
    ArduinoOTA.handle();          // Handle OTA updates
    wifiManager.loop();           // Check WiFi connection
    awsIot.loop();                // Handle MQTT reconnection
    server.handleClient();         // Handle web requests
    
    // Only try to publish telemetry once per second (function has 30s internal throttle)
    if (millis() - lastTelemetryCheck >= 1000) {
      lastTelemetryCheck = millis();
      awsIot.publishTelemetry(vanState);
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay for responsiveness
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== VAN CONTROL SYSTEM v2.0 ===");
  Serial.println("ESP32 + SN65HVD230 CAN Transceiver");
  Serial.println("AWS IoT Core Integration");
  
  // Initialize WiFi (tries van WiFi, falls back to AP)
  wifiManager.begin();
  
  // Setup OTA updates
  ArduinoOTA.setHostname("van-esp32");
  ArduinoOTA.setPassword("vanupdate");  // Set a password for security
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("\n🔄 OTA Update Starting: " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n✅ OTA Update Complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("✓ OTA updates enabled (hostname: van-esp32, password: vanupdate)");
  
  // Initialize AWS IoT (only if connected to van WiFi)
  if (wifiManager.isStationMode()) {
    awsIot.begin();
  } else {
    Serial.println("⚠️ AWS IoT disabled in AP mode (need van WiFi connection)");
  }
  
  // Setup web server
  Serial.println("\n🌐 Starting web server...");
  setupWebServer();
  Serial.println("✓ Web server started on port 80");
  
  // Create CAN bus mutex for thread-safe access between cores
  canMutex = xSemaphoreCreateMutex();
  
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
  
  // Create network task on Core 0 (handles WiFi/MQTT/Web)
  xTaskCreatePinnedToCore(
    networkTaskFunction,         // Task function
    "Network",                   // Name
    16000,                       // Stack size (bytes) - increased for TLS
    NULL,                        // Parameter
    1,                           // Priority
    &networkTask,                // Task handle
    0                            // Core 0 (WiFi/networking)
  );
  
  Serial.println("✓ Network task running on Core 0 (WiFi + AWS IoT + Web)");
  Serial.println("✓ CAN bus running on Core 1 (main loop)");
  Serial.println("\n=== SYSTEM READY ===\n");
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
    totalMsgCount++;
    uint32_t msgId = CAN.packetId();
    uint8_t data[8] = {0};
    int dataLen = 0;
    
    // Read the data first (always)
    while (CAN.available() && dataLen < 8) {
      data[dataLen++] = CAN.read();
    }
    
    // Debug: Print non-PDM messages for first 15 seconds to diagnose missing Rixens/thermostat
    if (millis() < 15000 && msgId != PDM1_MESSAGE && msgId != PDM2_MESSAGE && msgId != PDM1_COMMAND && msgId != PDM2_COMMAND) {
      Serial.printf("🔍 CAN 0x%X%s [", msgId, CAN.packetExtended() ? " (EXT)" : "");
      for (int i = 0; i < dataLen; i++) {
        if (i > 0) Serial.print(" ");
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
      }
      Serial.println("]");
    }
    
    // Find or add this message ID to tracking
    int idx = -1;
    for (int i = 0; i < trackedCount; i++) {
      if (trackedMessages[i].id == msgId) {
        idx = i;
        break;
      }
    }
    
    if (idx == -1 && trackedCount < 100) {
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
      
      // Note: Digital input storage (0xF0/0xF8) is handled by decodePDMStatus() in can_decoder.h
      
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
      
      // Print PDM commands ONLY when they change (reduces spam)
      if ((msgId == PDM1_COMMAND || msgId == PDM2_COMMAND) && changed) {
        Serial.print("🔵 PDM CMD CHANGED: 0x");
        Serial.print(msgId, HEX);
        Serial.print(" [");
        for (int i = 0; i < dataLen; i++) {
          if (i > 0) Serial.print(" ");
          if (data[i] < 0x10) Serial.print("0");
          Serial.print(data[i], HEX);
        }
        Serial.println("]");
      }
      
      // Print PDM MESSAGE when they change (to see feedback)
      if ((msgId == PDM1_MESSAGE || msgId == PDM2_MESSAGE) && changed) {
        Serial.print("📊 PDM MSG CHANGED: 0x");
        Serial.print(msgId, HEX);
        Serial.print(" [");
        for (int i = 0; i < dataLen; i++) {
          if (i > 0) Serial.print(" ");
          if (data[i] < 0x10) Serial.print("0");
          Serial.print(data[i], HEX);
        }
        Serial.println("]");
      }
      
      // Decode important messages
      if (changed || !baselinesSet) {
        if (msgId == PDM1_COMMAND || msgId == PDM2_COMMAND) {
          decodePDMCommand(msgId, data, dataLen);
        } else if (msgId == PDM1_MESSAGE || msgId == PDM2_MESSAGE) {
          decodePDMStatus(msgId, data, dataLen);
        } else if (msgId == RIXENS_GLYCOL || msgId == RIXENS_RETURN3 || msgId == RIXENS_RETURN4 || 
                   msgId == RIXENS_RETURN1 || msgId == RIXENS_RETURN2 || msgId == RIXENS_RETURN6 ||
                   msgId == THERMOSTAT_AMBIENT_STATUS) {
          decodeRixens(msgId, data, dataLen);
        } else if (msgId == TANK_LEVEL) {
          decodeTankLevel(msgId, data, dataLen);
        } else if (msgId == THERMOSTAT_STATUS_1) {
          decodeThermostatStatus(msgId, data, dataLen);
        }
      }
    }
    
    // Set baselines after 10 seconds
    if (!baselinesSet && millis() > 10000) {
      uint8_t pdm1_16[8] = {0}, pdm1_712[8] = {0};
      uint8_t pdm2_16[8] = {0}, pdm2_712[8] = {0};
      
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
