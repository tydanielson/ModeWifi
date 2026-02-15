#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <CAN.h>
#include <ArduinoJson.h>
#include "van_state.h"
#include "can_messages.h"
#include "web_interface.h"

extern WebServer server;
extern SemaphoreHandle_t canMutex;

// Message tracking (defined in main.cpp, struct in van_state.h)
extern MessageTracker trackedMessages[];
extern int trackedCount;
extern int totalMsgCount;

// Simulate a digital button press by modifying the last received digital input message
// This is the same approach the original ModeWifi author used
bool pressDigitalButton(uint32_t canId, uint8_t* lastData, uint8_t dlc, int dataIndex, int byteNum) {
  if (dlc == 0) {
    Serial.println("❌ No previous digital input message to simulate button press");
    return false;
  }
  
  // Create bit masks for the button press pattern
  // Each input uses 2 bits: 00=released, 01=short-to-ground, 10=short-to-power, 11=invalid
  // We use 10 (0b10) to simulate a button press
  uint8_t andMask = 0xFF;
  if (byteNum == 3) andMask = 0b00111111;
  else if (byteNum == 2) andMask = 0b11001111;
  else if (byteNum == 1) andMask = 0b11110011;
  else if (byteNum == 0) andMask = 0b11111100;
  
  uint8_t orMask = (0b10 << (byteNum * 2));
  
  // Build modified message with button press
  uint8_t pressData[8];
  memcpy(pressData, lastData, 8);
  pressData[dataIndex] = (pressData[dataIndex] & andMask) | orMask;
  
  Serial.printf("🔘 Simulating button press: ID=0x%08X byte[%d] mask=0x%02X\n", canId, dataIndex, orMask);
  
  // Acquire CAN mutex for thread-safe access (Core 1 is reading CAN)
  if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("❌ Failed to acquire CAN mutex");
    return false;
  }
  
  // Send button press
  if (!CAN.beginExtendedPacket(canId)) {
    Serial.println("❌ Failed to start CAN packet");
    xSemaphoreGive(canMutex);
    return false;
  }
  CAN.write(pressData, dlc);
  if (!CAN.endPacket()) {
    Serial.println("❌ Failed to send button press");
    xSemaphoreGive(canMutex);
    return false;
  }
  
  xSemaphoreGive(canMutex);  // Release during delay
  delay(100);  // Hold button press for 100ms
  
  // Re-acquire for release message
  if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("❌ Failed to acquire CAN mutex for release");
    return false;
  }
  
  // Send button release (clear the bits)
  uint8_t releaseData[8];
  memcpy(releaseData, lastData, 8);
  releaseData[dataIndex] = releaseData[dataIndex] & andMask;
  
  if (!CAN.beginExtendedPacket(canId)) {
    Serial.println("❌ Failed to start CAN packet for release");
    xSemaphoreGive(canMutex);
    return false;
  }
  CAN.write(releaseData, dlc);
  if (!CAN.endPacket()) {
    Serial.println("❌ Failed to send button release");
    xSemaphoreGive(canMutex);
    return false;
  }
  
  xSemaphoreGive(canMutex);
  Serial.println("✅ Button press/release simulated");
  return true;
}

// Press specific light buttons (from original ModeWifi code)
bool pressCargo() {
  return pressDigitalButton(
    PDM1_MESSAGE,  // 0x14EF111E
    vanState.lastPDM1inputs1to6.data,
    vanState.lastPDM1inputs1to6.dlc,
    6,  // byte index
    1   // bit position (bits 2-3)
  );
}

bool pressReading() {
  return pressDigitalButton(
    PDM1_MESSAGE,  // 0x14EF111E
    vanState.lastPDM1inputs1to6.data,
    vanState.lastPDM1inputs1to6.dlc,
    6,  // byte index - NEED TO VERIFY THIS
    2   // bit position - NEED TO VERIFY THIS
  );
}

bool pressCabin() {
  return pressDigitalButton(
    PDM1_MESSAGE,  // 0x14EF111E
    vanState.lastPDM1inputs1to6.data,
    vanState.lastPDM1inputs1to6.dlc,
    6,  // byte index
    0   // bit position (bits 0-1)
  );
}

bool pressAwning() {
  Serial.printf("🎯 pressAwning() called - lastPDM1inputs1to6.dlc=%d\n", vanState.lastPDM1inputs1to6.dlc);
  return pressDigitalButton(
    PDM1_MESSAGE,  // 0x14EF111E
    vanState.lastPDM1inputs1to6.data,
    vanState.lastPDM1inputs1to6.dlc,
    7,  // byte index
    3   // bit position (bits 6-7)
  );
}

// Global PDM command function (used by both web server and AWS IoT)
// Now uses button press simulation instead of direct commands
bool sendPDMCommand(int pdm, int channel, bool state) {
  Serial.printf("🎮 Control request: PDM%d Ch%d→%s\n", pdm, channel, state ? "ON" : "OFF");
  
  // Only support PDM1 lights for now (channels 2,3,4,5)
  if (pdm != 1) {
    Serial.printf("❌ PDM%d not supported yet (only PDM1)\n", pdm);
    return false;
  }
  
  // Map channels to button press functions
  // Based on original ModeWifi and our van's wiring
  bool success = false;
  
  switch (channel) {
    case 2:  // Cargo lights
      Serial.println("→ Pressing cargo light button");
      success = pressCargo();
      break;
      
    case 3:  // Reading lights
      Serial.println("→ Pressing reading light button");
      success = pressReading();
      break;
      
    case 4:  // Cabin lights
      Serial.println("→ Pressing cabin light button");
      success = pressCabin();
      break;
      
    case 5:  // Awning lights
      Serial.println("→ Pressing awning light button");
      success = pressAwning();
      break;
      
    default:
      Serial.printf("❌ Channel %d not mapped to a button\n", channel);
      return false;
  }
  
  if (success) {
    Serial.printf("✅ Button press for PDM%d Ch%d completed\n", pdm, channel);
  }
  
  return success;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleStatus() {
  String json = "{";
  json += "\"voltage\":" + String(vanState.voltage, 1) + ",";
  json += "\"temp\":" + String(vanState.glycolTemp, 1) + ",";
  json += "\"cabin_temp\":" + String(vanState.cabinTemp, 1) + ",";
  json += "\"fuel\":" + String(vanState.fuelLevel) + ",";
  json += "\"fanSpeed\":" + String(vanState.fanSpeed) + ",";
  
  json += "\"pdm1\":[";
  for (int i = 1; i <= 12; i++) {
    if (i > 1) json += ",";
    json += "{";
    json += "\"name\":\"" + String(vanState.pdm1[i].name) + "\",";
    json += "\"state\":" + String(vanState.pdm1[i].command) + ",";
    json += "\"amps\":" + String(vanState.pdm1[i].feedbackAmps, 2);
    json += "}";
  }
  json += "],";
  
  json += "\"pdm2\":[";
  for (int i = 1; i <= 12; i++) {
    if (i > 1) json += ",";
    json += "{";
    json += "\"name\":\"" + String(vanState.pdm2[i].name) + "\",";
    json += "\"state\":" + String(vanState.pdm2[i].command) + ",";
    json += "\"amps\":" + String(vanState.pdm2[i].feedbackAmps, 2);
    json += "}";
  }
  json += "]";
  
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleControl() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"success\":false,\"message\":\"Method not allowed\"}");
    return;
  }
  
  // Parse POST body as JSON using ArduinoJson
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, body);
  
  if (err) {
    Serial.printf("❌ JSON parse error: %s\n", err.c_str());
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  int pdm = doc["pdm"] | 0;
  int channel = doc["channel"] | 0;
  bool state = doc["state"] | false;
  
  Serial.printf("🎛️  Web control request: PDM%d Channel %d → %s\n", pdm, channel, state ? "ON" : "OFF");
  
  // Validate parameters
  if (pdm < 1 || pdm > 2 || channel < 1 || channel > 12) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid PDM or channel\"}");
    return;
  }
  
  // Send CAN command
  bool success = sendPDMCommand(pdm, channel, state);
  
  if (success) {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to send CAN command\"}");
  }
}

// AC thermostat control - sends command on THERMOSTAT_COMMAND_1
// Operating modes: 0=Off, 1=Cool, 2=Heat, 3=Auto, 4=Fan Only
// Fan modes: 0=Auto, 1=Always On
bool sendACCommand(uint8_t operatingMode, uint8_t fanMode, uint8_t fanSpeed, float setpointCoolC) {
  // Encode cool setpoint: (tempC + 273.0) / 0.03125
  int encoded = (int)((setpointCoolC + 273.0) / 0.03125);
  
  uint8_t data[8] = {
    1,                                       // data[0] = always 1
    (uint8_t)((fanMode << 4) | operatingMode), // data[1] = fan mode + op mode
    fanSpeed,                                // data[2] = fan speed 0-255
    0xF9, 0x24,                              // data[3-4] = heat setpoint (default)
    (uint8_t)(encoded & 0xFF),               // data[5] = cool setpoint low byte
    (uint8_t)((encoded >> 8) & 0xFF),        // data[6] = cool setpoint high byte
    0                                        // data[7]
  };
  
  Serial.printf("🌡️ AC Command: mode=%d fan=%d speed=%d setpoint=%.1fC\n",
    operatingMode, fanMode, fanSpeed, setpointCoolC);
  
  if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("❌ Failed to acquire CAN mutex for AC command");
    return false;
  }
  
  if (!CAN.beginExtendedPacket(THERMOSTAT_COMMAND_1)) {
    xSemaphoreGive(canMutex);
    return false;
  }
  CAN.write(data, 8);
  bool ok = CAN.endPacket();
  xSemaphoreGive(canMutex);
  
  if (ok) Serial.println("✅ AC command sent");
  else Serial.println("❌ AC command failed");
  return ok;
}

void handleHVAC() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"success\":false,\"message\":\"Method not allowed\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, body);
  
  if (err) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  // mode: 0=off, 1=cool, 2=heat, 4=fan
  // temp_f: temperature in Fahrenheit
  // fan_speed: 0-255 (0 = off with mode 0)
  uint8_t mode = doc["mode"] | 0;
  float tempF = doc["temp_f"] | 72.0;
  uint8_t fanSpeed = doc["fan_speed"] | 64;
  uint8_t fanMode = doc["fan_mode"] | 0;  // 0=auto, 1=always on
  
  // Convert F to C
  float tempC = (tempF - 32.0) * 5.0 / 9.0;
  
  bool success = sendACCommand(mode, fanMode, fanSpeed, tempC);
  
  if (success) {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"HVAC command sent\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to send HVAC command\"}");
  }
}

// Debug endpoint for remote CAN bus diagnostics
void handleDebug() {
  String json = "{";
  json += "\"uptime_ms\":" + String(millis()) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"total_messages\":" + String(totalMsgCount) + ",";
  json += "\"unique_ids\":" + String(trackedCount) + ",";
  json += "\"baselines_set\":" + String(baselinesSet ? "true" : "false") + ",";
  json += "\"van_last_update\":" + String(vanState.lastUpdate) + ",";
  json += "\"voltage\":" + String(vanState.voltage, 1) + ",";
  json += "\"glycol_temp\":" + String(vanState.glycolTemp, 1) + ",";
  json += "\"cabin_temp\":" + String(vanState.cabinTemp, 1) + ",";
  
  // PDM1 sub-message type distribution (high nibble counts)
  json += "\"pdm1_b0_counts\":{";
  const char* nibbleNames[] = {"0x0_","0x1_","0x2_","0x3_","0x4_","0x5_","0x6_","0x7_","0x8_","0x9_","0xA_","0xB_","0xC_","0xD_","0xE_","0xF_"};
  bool firstNibble = true;
  for (int i = 0; i < 16; i++) {
    if (vanState.pdm1SubTypeCounts[i] > 0) {
      if (!firstNibble) json += ",";
      json += "\"" + String(nibbleNames[i]) + "\":" + String(vanState.pdm1SubTypeCounts[i]);
      firstNibble = false;
    }
  }
  json += "},";
  
  // Last raw 0xF9/0xC9 feedback data for PDM1
  json += "\"pdm1_feedback_raw\":\"";
  for (int i = 0; i < 8; i++) {
    if (i > 0) json += " ";
    if (vanState.lastPdm1FeedbackData[i] < 0x10) json += "0";
    json += String(vanState.lastPdm1FeedbackData[i], HEX);
  }
  json += "\",";
  
  // PDM1 amps
  json += "\"pdm1_amps\":[";
  for (int i = 1; i <= 12; i++) {
    if (i > 1) json += ",";
    json += String(vanState.pdm1[i].feedbackAmps, 3);
  }
  json += "],";
  
  // Known CAN IDs to check for
  json += "\"known_ids\":{";
  json += "\"PDM1_CMD\":\"0x" + String(PDM1_COMMAND, HEX) + "\",";
  json += "\"PDM2_CMD\":\"0x" + String(PDM2_COMMAND, HEX) + "\",";
  json += "\"PDM1_MSG\":\"0x" + String(PDM1_MESSAGE, HEX) + "\",";
  json += "\"PDM2_MSG\":\"0x" + String(PDM2_MESSAGE, HEX) + "\",";
  json += "\"RIXENS_GLYCOL\":\"0x" + String(RIXENS_GLYCOL, HEX) + "\",";
  json += "\"RIXENS_FAN\":\"0x" + String(RIXENS_RETURN4, HEX) + "\",";
  json += "\"THERMOSTAT\":\"0x" + String(THERMOSTAT_AMBIENT_STATUS, HEX) + "\",";
  json += "\"TANK\":\"0x" + String(TANK_LEVEL, HEX) + "\"";
  json += "},";
  
  // All tracked CAN messages
  json += "\"tracked\":[";
  for (int i = 0; i < trackedCount; i++) {
    if (i > 0) json += ",";
    json += "{\"id\":\"0x" + String(trackedMessages[i].id, HEX) + "\",";
    json += "\"count\":" + String(trackedMessages[i].count) + ",";
    json += "\"dlc\":" + String(trackedMessages[i].dlc) + ",";
    json += "\"data\":\"";
    for (int j = 0; j < trackedMessages[i].dlc && j < 8; j++) {
      if (j > 0) json += " ";
      if (trackedMessages[i].lastData[j] < 0x10) json += "0";
      json += String(trackedMessages[i].lastData[j], HEX);
    }
    json += "\"}";
  }
  json += "]";
  
  json += "}";
  
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/control", handleControl);
  server.on("/api/hvac", handleHVAC);
  server.on("/api/debug", handleDebug);
  server.begin();
}

#endif
