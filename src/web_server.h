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

// Button simulation: spoof a digital input message to toggle a PDM channel
// This is what physically toggles lights -- the Firefly processes the simulated button press
bool pressDigitalButton(uint32_t canId, uint8_t* lastData, uint8_t dlc, int dataIndex, int byteNum) {
  if (dlc == 0) {
    Serial.println("❌ No digital input data for button simulation");
    return false;
  }
  
  uint8_t andMask = 0xFF;
  if (byteNum == 3) andMask = 0b00111111;
  else if (byteNum == 2) andMask = 0b11001111;
  else if (byteNum == 1) andMask = 0b11110011;
  else if (byteNum == 0) andMask = 0b11111100;
  uint8_t orMask = (0b10 << (byteNum * 2));
  
  uint8_t pressData[8];
  memcpy(pressData, lastData, 8);
  pressData[dataIndex] = (pressData[dataIndex] & andMask) | orMask;
  
  if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  
  if (!CAN.beginExtendedPacket(canId)) { xSemaphoreGive(canMutex); return false; }
  CAN.write(pressData, dlc);
  if (!CAN.endPacket()) { xSemaphoreGive(canMutex); return false; }
  xSemaphoreGive(canMutex);
  
  delay(100);
  
  if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  uint8_t releaseData[8];
  memcpy(releaseData, lastData, 8);
  releaseData[dataIndex] = releaseData[dataIndex] & andMask;
  if (!CAN.beginExtendedPacket(canId)) { xSemaphoreGive(canMutex); return false; }
  CAN.write(releaseData, dlc);
  if (!CAN.endPacket()) { xSemaphoreGive(canMutex); return false; }
  xSemaphoreGive(canMutex);
  
  Serial.println("✅ Button press simulated");
  return true;
}

bool pressCargo() { return pressDigitalButton(PDM1_MESSAGE, vanState.lastPDM1inputs1to6.data, vanState.lastPDM1inputs1to6.dlc, 6, 1); }
bool pressReading() { return pressDigitalButton(PDM1_MESSAGE, vanState.lastPDM1inputs1to6.data, vanState.lastPDM1inputs1to6.dlc, 6, 2); }
bool pressCabin() { return pressDigitalButton(PDM1_MESSAGE, vanState.lastPDM1inputs1to6.data, vanState.lastPDM1inputs1to6.dlc, 6, 0); }
bool pressAwning() { return pressDigitalButton(PDM1_MESSAGE, vanState.lastPDM1inputs1to6.data, vanState.lastPDM1inputs1to6.dlc, 7, 3); }

// Toggle a light via button simulation (channels 2-5 on PDM1)
bool toggleLightButton(int channel) {
  // Reset amps and command for this channel before toggle
  // This ensures stale non-zero data doesn't persist if the light turns off
  vanState.pdm1[channel].feedbackAmps = 0;
  vanState.pdm1[channel].command = 0;
  
  bool success = false;
  switch (channel) {
    case 2: success = pressCargo(); break;
    case 3: success = pressReading(); break;
    case 4: success = pressCabin(); break;
    case 5: success = pressAwning(); break;
    default: return false;
  }
  
  // Force next telemetry publish immediately (reset the throttle timer)
  // This way the dashboard gets fresh data within seconds, not 60s
  extern unsigned long lastPublishOverride;
  lastPublishOverride = millis();
  
  return success;
}

// Send a direct PDM command using the 0xFC/0xFD format (for dimming experiments)
// NOTE: Does NOT physically toggle lights -- Firefly overrides these
bool sendDirectPDMCommand(int pdm, int channel, uint8_t pwmValue) {
  if (channel < 1 || channel > 12) return false;
  
  uint32_t canId = (pdm == 1) ? PDM1_COMMAND : PDM2_COMMAND;
  PDMChannel* channels = (pdm == 1) ? vanState.pdm1 : vanState.pdm2;
  
  // Determine which sub-message: 0xFC for channels 1-6, 0xFD for channels 7-12
  bool isHigh = (channel > 6);
  uint8_t b0 = isHigh ? 0xFD : 0xFC;
  int baseChannel = isHigh ? 7 : 1;
  
  // Build data: current state for all 6 channels, with target channel changed
  uint8_t data[8];
  data[0] = b0;
  for (int i = 0; i < 6; i++) {
    data[i + 1] = channels[baseChannel + i].command;
  }
  data[7] = 0xFF;  // Rolling counter
  
  // Set the target channel
  int byteIdx = channel - baseChannel + 1;
  data[byteIdx] = pwmValue;
  
  Serial.printf("💡 Direct PDM%d Ch%d = %d (0x%02X): [%02X %02X %02X %02X %02X %02X %02X %02X]\n",
    pdm, channel, pwmValue, b0,
    data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
  
  // Send on CAN bus
  if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("❌ Failed to acquire CAN mutex");
    return false;
  }
  
  if (!CAN.beginExtendedPacket(canId)) {
    xSemaphoreGive(canMutex);
    return false;
  }
  CAN.write(data, 8);
  bool ok = CAN.endPacket();
  xSemaphoreGive(canMutex);
  
  if (ok) {
    // Update local state immediately so next telemetry reflects the change
    channels[channel].command = pwmValue;
    Serial.printf("✅ PDM%d Ch%d set to %d\n", pdm, channel, pwmValue);
  } else {
    Serial.println("❌ Failed to send PDM command");
  }
  
  return ok;
}

// Global PDM command function (used by both web server and AWS IoT)
// For light channels (PDM1 2-5): uses button simulation (physically toggles)
// For other channels: uses direct PDM command
bool sendPDMCommand(int pdm, int channel, int brightness) {
  Serial.printf("🎮 Control: PDM%d Ch%d brightness=%d\n", pdm, channel, brightness);
  
  if (pdm < 1 || pdm > 2 || channel < 1 || channel > 12) return false;
  
  // Light channels on PDM1 use button simulation (only way to physically toggle)
  if (pdm == 1 && channel >= 2 && channel <= 5) {
    Serial.println("→ Using button simulation for light toggle");
    return toggleLightButton(channel);
  }
  
  // Other channels use direct PDM command
  uint8_t pwmValue;
  if (brightness < 0) {
    PDMChannel* channels = (pdm == 1) ? vanState.pdm1 : vanState.pdm2;
    pwmValue = (channels[channel].command > 0) ? 0 : 255;
  } else {
    pwmValue = (uint8_t)((brightness * 255) / 100);
  }
  return sendDirectPDMCommand(pdm, channel, pwmValue);
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
  int brightness = doc["brightness"] | -1;  // -1 = toggle, 0-100 = set level
  
  Serial.printf("🎛️  Web control: PDM%d Ch%d brightness=%d\n", pdm, channel, brightness);
  
  // Validate parameters
  if (pdm < 1 || pdm > 2 || channel < 1 || channel > 12) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid PDM or channel\"}");
    return;
  }
  
  // Send direct PDM command
  bool success = sendPDMCommand(pdm, channel, brightness);
  
  if (success) {
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Command sent\"}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to send CAN command\"}");
  }
}

// AC thermostat control - sends command on THERMOSTAT_COMMAND_1
// Operating modes: 0=Off, 1=Cool, 2=Heat, 3=Auto, 4=Fan Only
// Fan modes: 0=Auto, 1=Always On
bool sendACCommand(uint8_t operatingMode, uint8_t fanMode, uint8_t fanSpeed, float setpointC) {
  // Encode setpoint: (tempC + 273.0) / 0.03125
  int encoded = (int)((setpointC + 273.0) / 0.03125);
  uint8_t spLo = (uint8_t)(encoded & 0xFF);
  uint8_t spHi = (uint8_t)((encoded >> 8) & 0xFF);
  
  // Place setpoint in correct bytes based on mode:
  //   Heat (mode 2): bytes 3-4 = heat setpoint, bytes 5-6 = 0xFF (no change)
  //   Cool (mode 1): bytes 3-4 = 0xFF (no change), bytes 5-6 = cool setpoint
  //   Off  (mode 0): both setpoints don't matter
  uint8_t heatLo, heatHi, coolLo, coolHi;
  if (operatingMode == 2) {
    // Heat mode: user temp goes in heat setpoint
    heatLo = spLo; heatHi = spHi;
    coolLo = 0xFF; coolHi = 0xFF;
  } else {
    // Cool mode (or off/auto): user temp goes in cool setpoint
    heatLo = 0xFF; heatHi = 0xFF;
    coolLo = spLo; coolHi = spHi;
  }
  
  uint8_t data[8] = {
    1,                                       // data[0] = instance
    (uint8_t)((fanMode << 4) | operatingMode), // data[1] = fan mode + op mode
    fanSpeed,                                // data[2] = fan speed 0-255
    heatLo, heatHi,                          // data[3-4] = heat setpoint
    coolLo, coolHi,                          // data[5-6] = cool setpoint
    0                                        // data[7]
  };
  
  Serial.printf("🌡️ AC Command: mode=%d fan=%d speed=%d setpoint=%.1fC (heat=%02X%02X cool=%02X%02X)\n",
    operatingMode, fanMode, fanSpeed, setpointC, heatHi, heatLo, coolHi, coolLo);
  
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

// Vent fan control using ROOFFAN_CONTROL (0x19FEA603)
// speed: 0-127, direction: 0=out, 1=in, dome: 0=closed, 4=open
bool sendVentCommand(uint8_t speed, bool direction, uint8_t domePosition) {
  uint8_t data[8] = {
    2,                                        // data[0] = instance always 2
    0b10101,                                  // data[1] = system on, fan force on, speed manual
    speed,                                    // data[2] = fan speed 0-127
    (uint8_t)((domePosition << 2) | (direction & 1)), // data[3] = dome + direction
    0, 0, 0, 0                               // data[4-7] = temp/setpoint (unused)
  };
  
  // Set rain sensor on and dome command
  if (domePosition > 0) {
    data[3] |= 0x40;  // Rain sensor on
  }
  
  Serial.printf("🌀 Vent: speed=%d dir=%s dome=%d\n", speed, direction ? "in" : "out", domePosition);
  
  if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
  
  if (!CAN.beginExtendedPacket(ROOFFAN_CONTROL)) {
    xSemaphoreGive(canMutex);
    return false;
  }
  CAN.write(data, 8);
  bool ok = CAN.endPacket();
  xSemaphoreGive(canMutex);
  
  if (ok) Serial.println("✅ Vent command sent");
  return ok;
}

void handleVent() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"success\":false,\"message\":\"Method not allowed\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  uint8_t speed = doc["speed"] | 0;
  bool direction = doc["direction"] | false;  // false=out, true=in
  uint8_t dome = doc["dome"] | 0;  // 0=closed, 4=open
  
  bool success = sendVentCommand(speed, direction, dome);
  server.send(success ? 200 : 500, "application/json",
    success ? "{\"success\":true,\"message\":\"Vent command sent\"}" 
            : "{\"success\":false,\"message\":\"Failed\"}");
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
  
  // Per-b0 raw data for PDM1_MESSAGE sub-types
  const char* b0Names[] = {"F9","C9","0A","FC","FD","FB","FE","F0","F8"};
  uint8_t* b0Ptrs[] = {vanState.pdm1_lastF9, vanState.pdm1_lastC9, vanState.pdm1_last0A,
                        vanState.pdm1_lastFC, vanState.pdm1_lastFD, vanState.pdm1_lastFB,
                        vanState.pdm1_lastFE, vanState.pdm1_lastF0, vanState.pdm1_lastF8};
  json += "\"pdm1_raw\":{";
  for (int t = 0; t < 9; t++) {
    if (t > 0) json += ",";
    json += "\"" + String(b0Names[t]) + "\":\"";
    for (int i = 0; i < 8; i++) {
      if (i > 0) json += " ";
      if (b0Ptrs[t][i] < 0x10) json += "0";
      json += String(b0Ptrs[t][i], HEX);
    }
    json += "\"";
  }
  json += "},";
  
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
  server.on("/api/vent", handleVent);
  server.on("/api/debug", handleDebug);
  server.begin();
}

#endif
