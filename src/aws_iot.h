#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <CAN.h>
#include "config.h"
#include "van_state.h"
#include "can_messages.h"

// Message tracking (defined in main.cpp)
extern MessageTracker trackedMessages[];
extern int trackedCount;
extern int totalMsgCount;

// Forward declaration of global PDM command function (defined in web_server.h)
bool sendPDMCommand(int pdm, int channel, bool state);

// MQTT configuration
#define MQTT_PORT 8883
#define PUBLISH_INTERVAL 60000   // Publish every 60 seconds
#define RECONNECT_INTERVAL 5000  // Try reconnecting every 5 seconds

class AWSIoT {
private:
  WiFiClientSecure wifiClient;
  MQTTClient mqttClient;
  
  unsigned long lastPublishTime = 0;
  unsigned long lastReconnectAttempt = 0;
  bool connected = false;
  
  // MQTT topics
  String telemetryTopic;
  String statusTopic;
  String alertsTopic;
  String commandsTopic;
  
public:
  AWSIoT() : mqttClient(2048) {  // 2048 byte buffer for large nested JSON payloads
    // Build MQTT topics
    telemetryTopic = String("van/") + THING_NAME + "/telemetry";
    statusTopic = String("van/") + THING_NAME + "/status";
    alertsTopic = String("van/") + THING_NAME + "/alerts";
    commandsTopic = String("van/") + THING_NAME + "/commands";
  }
  
  void begin() {
    Serial.println("\n☁️  AWS IoT Client Starting (MQTT Library)...");
    
    // Configure WiFiClientSecure to use AWS IoT certificates
    wifiClient.setCACert(AWS_ROOT_CA);
    wifiClient.setCertificate(DEVICE_CERTIFICATE);
    wifiClient.setPrivateKey(PRIVATE_KEY);
    
    // Set up MQTT client
    mqttClient.begin(AWS_IOT_ENDPOINT, MQTT_PORT, wifiClient);
    
    // Set options for AWS IoT compatibility
    mqttClient.setOptions(60, true, 10000);  // keep-alive 60s, clean session, 10s timeout
    
    // Set message callback
    mqttClient.onMessage([this](String &topic, String &payload) {
      this->messageCallback(topic, payload);
    });
    
    Serial.printf("   Endpoint: %s:%d\n", AWS_IOT_ENDPOINT, MQTT_PORT);
    Serial.printf("   Thing: %s\n", THING_NAME);
    Serial.println("   Using 256dpi/MQTT Library (AWS IoT compatible)");
    Serial.println("   Keep-Alive: 60 seconds");
    Serial.println("   Buffer: 2048 bytes (for large nested JSON)");
    
    // Initial connection attempt
    connect();
  }
  
  bool connect() {
    if (mqttClient.connected()) {
      return true;
    }
    
    Serial.print("🔐 Connecting to AWS IoT Core...");
    
    // Connect with client ID = thing name
    if (mqttClient.connect(THING_NAME)) {
      Serial.println(" ✅ Connected!");
      connected = true;
      
      // Subscribe to commands topic
      mqttClient.subscribe(commandsTopic);
      Serial.printf("📥 Subscribed to: %s\n", commandsTopic.c_str());
      
      // Publish status message on connect
      publishStatus("connected");
      
      return true;
    } else {
      connected = false;
      int error = mqttClient.lastError();
      int returnCode = mqttClient.returnCode();
      Serial.printf(" ❌ Failed (error=%d, rc=%d)\n", error, returnCode);
      
      // Error codes from MQTT library
      switch (error) {
        case LWMQTT_SUCCESS: Serial.println("   No error"); break;
        case LWMQTT_BUFFER_TOO_SHORT: Serial.println("   Buffer too short"); break;
        case LWMQTT_VARNUM_OVERFLOW: Serial.println("   Variable number overflow"); break;
        case LWMQTT_NETWORK_FAILED_CONNECT: Serial.println("   Network failed to connect"); break;
        case LWMQTT_NETWORK_TIMEOUT: Serial.println("   Network timeout"); break;
        case LWMQTT_NETWORK_FAILED_READ: Serial.println("   Network failed to read"); break;
        case LWMQTT_NETWORK_FAILED_WRITE: Serial.println("   Network failed to write"); break;
        case LWMQTT_REMAINING_LENGTH_OVERFLOW: Serial.println("   Remaining length overflow"); break;
        case LWMQTT_REMAINING_LENGTH_MISMATCH: Serial.println("   Remaining length mismatch"); break;
        case LWMQTT_MISSING_OR_WRONG_PACKET: Serial.println("   Missing or wrong packet"); break;
        case LWMQTT_CONNECTION_DENIED: Serial.println("   Connection denied"); break;
        case LWMQTT_FAILED_SUBSCRIPTION: Serial.println("   Failed subscription"); break;
        case LWMQTT_SUBACK_ARRAY_OVERFLOW: Serial.println("   SUBACK array overflow"); break;
        case LWMQTT_PONG_TIMEOUT: Serial.println("   Ping timeout"); break;
        default: Serial.printf("   Unknown error: %d\n", error); break;
      }
      
      return false;
    }
  }
  
  void loop() {
    // Process MQTT messages
    mqttClient.loop();
    
    // Update connection status
    bool currentlyConnected = mqttClient.connected();
    if (connected && !currentlyConnected) {
      int error = mqttClient.lastError();
      Serial.printf("⚠️ MQTT connection lost (error=%d)\n", error);
      connected = false;
    }
    
    // Try to reconnect if disconnected
    if (!currentlyConnected) {
      if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
        lastReconnectAttempt = millis();
        Serial.println("🔄 Attempting MQTT reconnect...");
        connect();
      }
    } else {
      connected = true;
    }
  }
  
  void publishTelemetry(const VanState& vanState) {
    if (!mqttClient.connected()) {
      return;  // Silently skip if not connected
    }
    
    // Check if it's time to publish
    if (millis() - lastPublishTime < PUBLISH_INTERVAL) {
      return;
    }
    
    Serial.printf("🧠 Free heap: %d bytes\n", ESP.getFreeHeap());
    
    StaticJsonDocument<4096> doc;  // Size for PDM channels + CAN diagnostics + ID list
    
    // Add metadata
    doc["thing_name"] = THING_NAME;
    doc["timestamp"] = millis();
    doc["message_type"] = "telemetry";
    
    // Add van state (round floats to avoid precision artifacts like 13.60000038)
    doc["battery_voltage"] = round(vanState.voltage * 10.0) / 10.0;
    doc["glycol_temp"] = round(vanState.glycolTemp * 100.0) / 100.0;
    doc["cabin_temp"] = round(vanState.cabinTemp * 100.0) / 100.0;
    doc["fuel_level"] = round(vanState.fuelLevel * 10.0) / 10.0;
    doc["fan_speed"] = vanState.fanSpeed;
    doc["heat_source"] = vanState.heatSource;
    
    // PDM1 channels - command states and feedback amps
    JsonObject pdm1 = doc.createNestedObject("pdm1");
    for (int i = 1; i <= 12; i++) {
      JsonObject ch = pdm1.createNestedObject(String(i));
      ch["name"] = vanState.pdm1[i].name;
      ch["state"] = vanState.pdm1[i].command;
      ch["amps"] = serialized(String(vanState.pdm1[i].feedbackAmps, 2));
      
      // Debug: Show awning lights (Ch5) value
      if (i == 5) {
        Serial.printf("   Ch5 (AWNING_LIGHTS) feedbackAmps = %.2fA\n", vanState.pdm1[i].feedbackAmps);
      }
    }
    
    // PDM2 channels - command states and feedback amps
    JsonObject pdm2 = doc.createNestedObject("pdm2");
    for (int i = 1; i <= 12; i++) {
      JsonObject ch = pdm2.createNestedObject(String(i));
      ch["name"] = vanState.pdm2[i].name;
      ch["state"] = vanState.pdm2[i].command;
      ch["amps"] = serialized(String(vanState.pdm2[i].feedbackAmps, 2));
    }
    
    // Tank levels and AC state
    doc["fresh_water"] = round(vanState.freshWaterLevel * 10.0) / 10.0;
    doc["gray_water"] = round(vanState.grayWaterLevel * 10.0) / 10.0;
    doc["ac_mode"] = vanState.acOperatingMode;
    doc["ac_fan_speed"] = vanState.acFanSpeed;
    doc["ac_setpoint"] = round(vanState.acSetpointCool * 10.0) / 10.0;
    
    // CAN bus diagnostics (so we can debug remotely via DynamoDB)
    JsonObject canDiag = doc.createNestedObject("can_diag");
    canDiag["total_msgs"] = totalMsgCount;
    canDiag["unique_ids"] = trackedCount;
    canDiag["free_heap"] = ESP.getFreeHeap();
    
    // Check if key CAN IDs have been seen
    bool seenRixensGlycol = false, seenRixensFan = false, seenThermostat = false;
    bool seenTank = false, seenPdm1Msg = false, seenPdm1Cmd = false;
    for (int i = 0; i < trackedCount; i++) {
      if (trackedMessages[i].id == RIXENS_GLYCOL) seenRixensGlycol = true;
      else if (trackedMessages[i].id == RIXENS_RETURN4) seenRixensFan = true;
      else if (trackedMessages[i].id == THERMOSTAT_AMBIENT_STATUS) seenThermostat = true;
      else if (trackedMessages[i].id == TANK_LEVEL) seenTank = true;
      else if (trackedMessages[i].id == PDM1_MESSAGE) seenPdm1Msg = true;
      else if (trackedMessages[i].id == PDM1_COMMAND) seenPdm1Cmd = true;
    }
    canDiag["seen_rixens_glycol"] = seenRixensGlycol;
    canDiag["seen_rixens_fan"] = seenRixensFan;
    canDiag["seen_thermostat"] = seenThermostat;
    canDiag["seen_tank"] = seenTank;
    canDiag["seen_pdm1_msg"] = seenPdm1Msg;
    canDiag["seen_pdm1_cmd"] = seenPdm1Cmd;
    
    // Full CAN ID list for remote diagnosis
    JsonArray canIds = doc.createNestedArray("can_ids");
    for (int i = 0; i < trackedCount && i < 25; i++) {
      JsonObject entry = canIds.createNestedObject();
      char hexId[12];
      sprintf(hexId, "0x%X", trackedMessages[i].id);
      entry["id"] = hexId;
      entry["n"] = trackedMessages[i].count;
      entry["b0"] = trackedMessages[i].lastData[0];
      entry["dlc"] = trackedMessages[i].dlc;
    }
    
    // Serialize to JSON string
    char jsonBuffer[3072];  // Increased for CAN ID list
    size_t len = serializeJson(doc, jsonBuffer);
    
    Serial.printf("🧠 Free heap after serialize: %d bytes\n", ESP.getFreeHeap());
    
    // Publish to AWS IoT
    Serial.printf("📤 Publishing telemetry (%d bytes)...", len);
    if (mqttClient.publish(telemetryTopic.c_str(), jsonBuffer, len, false, 0)) {
      Serial.println(" ✅");
      Serial.printf("🧠 Free heap after publish: %d bytes\n", ESP.getFreeHeap());
      lastPublishTime = millis();
    } else {
      Serial.println(" ❌ Failed!");
    }
  }
  
  void publishStatus(const char* status) {
    if (!mqttClient.connected()) {
      return;
    }
    
    StaticJsonDocument<256> doc;
    doc["thing_name"] = THING_NAME;
    doc["timestamp"] = millis();
    doc["message_type"] = "status";
    doc["status"] = status;
    doc["uptime_seconds"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    
    char jsonBuffer[256];
    size_t len = serializeJson(doc, jsonBuffer);
    
    mqttClient.publish(statusTopic.c_str(), jsonBuffer, len, false, 0);
    Serial.printf("📊 Status: %s\n", status);
  }
  
  void publishAlert(const char* alertType, const char* message, float value = 0) {
    if (!mqttClient.connected()) {
      return;
    }
    
    StaticJsonDocument<256> doc;
    doc["thing_name"] = THING_NAME;
    doc["timestamp"] = millis();
    doc["message_type"] = "alert";
    doc["alert_type"] = alertType;
    doc["message"] = message;
    doc["severity"] = "warning";
    if (value != 0) {
      doc["value"] = value;
    }
    
    char jsonBuffer[256];
    size_t len = serializeJson(doc, jsonBuffer);
    
    mqttClient.publish(alertsTopic.c_str(), jsonBuffer, len, false, 0);
    Serial.printf("🚨 Alert: %s - %s\n", alertType, message);
  }
  
  void messageCallback(String &topic, String &payload) {
    Serial.printf("📥 Message received on: %s\n", topic.c_str());
    Serial.printf("   Payload: %s\n", payload.c_str());
    
    // Parse JSON command
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.println("❌ Failed to parse command JSON");
      publishCommandResponse(false, "Failed to parse JSON", "");
      return;
    }
    
    const char* command = doc["command"];
    if (!command) {
      Serial.println("❌ No command field in JSON");
      publishCommandResponse(false, "No command field", "");
      return;
    }
    
    Serial.printf("   Command: %s\n", command);
    
    // Handle PDM channel control
    if (strcmp(command, "set_pdm_channel") == 0) {
      int pdm = doc["parameters"]["pdm"] | 1;
      int channel = doc["parameters"]["channel"] | 1;
      bool state = doc["parameters"]["state"] | false;
      
      if (pdm < 1 || pdm > 2 || channel < 1 || channel > 12) {
        Serial.println("❌ Invalid PDM or channel number");
        publishCommandResponse(false, "Invalid PDM or channel", command);
        return;
      }
      
      Serial.printf("   PDM%d Channel %d -> TOGGLE (button simulation, state param ignored)\n", pdm, channel);
      
      // Use button press simulation for PDM1 lights
      // NOTE: We ignore the 'state' parameter because these are momentary switches
      // Each button press toggles the light regardless of requested state
      bool success = false;
      if (pdm == 1) {
        if (channel == 2) success = pressCargo();       // Cargo lights
        else if (channel == 3) success = pressReading(); // Reading lights  
        else if (channel == 4) success = pressCabin();   // Cabin lights
        else if (channel == 5) success = pressAwning();  // Awning lights
        else {
          Serial.printf("⚠️  PDM1 Channel %d not supported yet\n", channel);
          publishCommandResponse(false, "Channel not supported", command);
          return;
        }
      } else {
        Serial.println("⚠️  PDM2 not supported yet");
        publishCommandResponse(false, "PDM2 not supported", command);
        return;
      }
      
      if (success) {
        publishCommandResponse(true, "Button press simulated", command);
      } else {
        publishCommandResponse(false, "Failed to simulate button press", command);
      }
    }
    // Handle AC/HVAC control
    else if (strcmp(command, "set_ac") == 0) {
      uint8_t mode = doc["parameters"]["mode"] | 0;
      float tempF = doc["parameters"]["temp_f"] | 72.0;
      uint8_t fanSpeed = doc["parameters"]["fan_speed"] | 64;
      uint8_t fanMode = doc["parameters"]["fan_mode"] | 0;
      
      // Convert F to C
      float tempC = (tempF - 32.0) * 5.0 / 9.0;
      
      Serial.printf("   AC: mode=%d temp=%.1fF(%.1fC) fan=%d speed=%d\n", mode, tempF, tempC, fanMode, fanSpeed);
      
      bool success = sendACCommand(mode, fanMode, fanSpeed, tempC);
      publishCommandResponse(success, success ? "AC command sent" : "AC command failed", command);
    }
    else if (strcmp(command, "ac_off") == 0) {
      bool success = sendACCommand(0, 0, 0, 20.0);
      publishCommandResponse(success, success ? "AC turned off" : "AC off failed", command);
    }
    // Remote restart
    else if (strcmp(command, "restart") == 0) {
      publishCommandResponse(true, "Restarting in 2 seconds", command);
      delay(2000);
      ESP.restart();
    }
    // CAN bus reset (without full reboot)
    else if (strcmp(command, "can_reset") == 0) {
      if (xSemaphoreTake(canMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        CAN.end();
        delay(100);
        CAN.begin(500E3);
        xSemaphoreGive(canMutex);
        publishCommandResponse(true, "CAN bus reset", command);
      } else {
        publishCommandResponse(false, "Failed to acquire CAN mutex", command);
      }
    }
    else {
      Serial.printf("⚠️  Unknown command: %s\n", command);
      publishCommandResponse(false, "Unknown command", command);
    }
  }
  
  void publishCommandResponse(bool success, const char* message, const char* command) {
    if (!mqttClient.connected()) {
      return;
    }
    
    StaticJsonDocument<512> doc;
    doc["thing_name"] = THING_NAME;
    doc["timestamp"] = millis();
    doc["message_type"] = "command_response";
    doc["success"] = success;
    doc["message"] = message;
    if (command && strlen(command) > 0) {
      doc["command"] = command;
    }
    
    char jsonBuffer[512];
    size_t len = serializeJson(doc, jsonBuffer);
    
    String responseTopic = String("van/") + THING_NAME + "/command_responses";
    mqttClient.publish(responseTopic.c_str(), jsonBuffer, len, false, 0);
    Serial.printf("📤 Command response published: %s\n", success ? "SUCCESS" : "FAILED");
  }
  
  bool isConnected() {
    return connected && mqttClient.connected();
  }
  
  String getStatus() {
    if (connected) {
      return "Connected";
    } else {
      return "Disconnected";
    }
  }
};

#endif // AWS_IOT_H
