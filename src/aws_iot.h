#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <ArduinoJson.h>
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
  AWSIoT() : mqttClient(1024) {  // 1024 byte buffer
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
    Serial.println("   Buffer: 1024 bytes");
    
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
    
    StaticJsonDocument<512> doc;
    
    // Add metadata
    doc["thing_name"] = THING_NAME;
    doc["timestamp"] = millis();
    doc["message_type"] = "telemetry";
    
    // Add van state
    doc["battery_voltage"] = vanState.voltage;
    doc["glycol_temp"] = vanState.glycolTemp;
    
    // PDM1 channels (two 6-channel groups)
    JsonArray pdm1_1_6 = doc.createNestedArray("pdm1_ch1_6");
    for (int i = 0; i < 6; i++) {
      pdm1_1_6.add(vanState.pdm1_ch1_6[i]);
    }
    JsonArray pdm1_7_12 = doc.createNestedArray("pdm1_ch7_12");
    for (int i = 0; i < 6; i++) {
      pdm1_7_12.add(vanState.pdm1_ch7_12[i]);
    }
    
    // PDM2 channels (two 6-channel groups)
    JsonArray pdm2_1_6 = doc.createNestedArray("pdm2_ch1_6");
    for (int i = 0; i < 6; i++) {
      pdm2_1_6.add(vanState.pdm2_ch1_6[i]);
    }
    JsonArray pdm2_7_12 = doc.createNestedArray("pdm2_ch7_12");
    for (int i = 0; i < 6; i++) {
      pdm2_7_12.add(vanState.pdm2_ch7_12[i]);
    }
    
    // Serialize to JSON string
    char jsonBuffer[512];
    size_t len = serializeJson(doc, jsonBuffer);
    
    // Publish to AWS IoT
    Serial.printf("📤 Publishing telemetry (%d bytes)...", len);
    if (mqttClient.publish(telemetryTopic.c_str(), jsonBuffer, len, false, 0)) {
      Serial.println(" ✅");
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
    
    // Parse JSON command (for Phase 3)
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.println("❌ Failed to parse command JSON");
      return;
    }
    
    const char* command = doc["command"];
    Serial.printf("   Command: %s\n", command);
    
    // TODO Phase 3: Handle commands
    // if (strcmp(command, "awning") == 0) { ... }
    // if (strcmp(command, "hvac") == 0) { ... }
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
