#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <CAN.h>
#include "config.h"
#include "van_state.h"

// MQTT configuration
#define MQTT_PORT 8883
#define PUBLISH_INTERVAL 30000   // Publish every 30 seconds
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
    
    StaticJsonDocument<3072> doc;  // Increased size for 24 PDM channels with nested objects + overhead
    
    // Add metadata
    doc["thing_name"] = THING_NAME;
    doc["timestamp"] = millis();
    doc["message_type"] = "telemetry";
    
    // Add van state
    doc["battery_voltage"] = vanState.voltage;
    doc["glycol_temp"] = vanState.glycolTemp;
    doc["cabin_temp"] = vanState.cabinTemp;
    doc["fuel_level"] = vanState.fuelLevel;
    doc["fan_speed"] = vanState.fanSpeed;
    doc["heat_source"] = vanState.heatSource;
    
    // PDM1 channels - command states and feedback amps
    JsonObject pdm1 = doc.createNestedObject("pdm1");
    for (int i = 1; i <= 12; i++) {
      JsonObject ch = pdm1.createNestedObject(String(i));
      ch["name"] = vanState.pdm1[i].name;
      ch["state"] = vanState.pdm1[i].command;
      ch["amps"] = serialized(String(vanState.pdm1[i].feedbackAmps, 2));
    }
    
    // PDM2 channels - command states and feedback amps
    JsonObject pdm2 = doc.createNestedObject("pdm2");
    for (int i = 1; i <= 12; i++) {
      JsonObject ch = pdm2.createNestedObject(String(i));
      ch["name"] = vanState.pdm2[i].name;
      ch["state"] = vanState.pdm2[i].command;
      ch["amps"] = serialized(String(vanState.pdm2[i].feedbackAmps, 2));
    }
    
    // Serialize to JSON string
    char jsonBuffer[2048];  // Increased for large nested JSON payloads
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
      
      Serial.printf("   Setting PDM%d Channel %d to %s\n", pdm, channel, state ? "ON" : "OFF");
      
      // Send CAN bus command
      bool success = sendPDMCommand(pdm, channel, state);
      
      if (success) {
        publishCommandResponse(true, "Command sent to PDM", command);
      } else {
        publishCommandResponse(false, "Failed to send CAN command", command);
      }
    } else {
      Serial.printf("⚠️  Unknown command: %s\n", command);
      publishCommandResponse(false, "Unknown command", command);
    }
  }
  
  bool sendPDMCommand(int pdm, int channel, bool state) {
    // PDM channels use CAN bus commands
    // PDM1: 0x14EF1E11, PDM2: 0x14EF1F11
    uint32_t canId = (pdm == 1) ? 0x14EF1E11 : 0x14EF1F11;
    
    // Build 8-byte PDM command message
    // Byte 0: Channel number (1-12)
    // Byte 1: State (0x00 = OFF, 0x01 = ON)
    // Bytes 2-7: Reserved (0x00)
    uint8_t data[8] = {0};
    data[0] = (uint8_t)channel;
    data[1] = state ? 0x01 : 0x00;
    
    Serial.printf("📤 Sending CAN: ID=0x%08X Data=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
                  canId, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    
    // Send extended CAN frame
    if (!CAN.beginExtendedPacket(canId)) {
      Serial.println("❌ Failed to start CAN packet");
      return false;
    }
    
    CAN.write(data, 8);
    
    if (!CAN.endPacket()) {
      Serial.println("❌ Failed to send CAN packet");
      return false;
    }
    
    Serial.println("✅ CAN command sent");
    return true;
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
