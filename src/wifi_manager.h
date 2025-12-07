#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "config.h"

// WiFi connection status LED
#define WIFI_LED_PIN 2  // Built-in LED on most ESP32 boards

enum class WifiManagerMode {
  STATION,   // Connected to Starlink
  ACCESS_POINT         // Access Point fallback
};

class WiFiManager {
private:
  WifiManagerMode currentMode;
  unsigned long lastReconnectAttempt = 0;
  const unsigned long RECONNECT_INTERVAL = 30000;  // Try reconnecting every 30 seconds
  
public:
  WiFiManager() : currentMode(WifiManagerMode::STATION) {
    pinMode(WIFI_LED_PIN, OUTPUT);
  }
  
  void begin() {
    Serial.println("\n🌐 WiFi Manager Starting...");
    
    // Try to connect to Starlink first
    if (connectToStation()) {
      currentMode = WifiManagerMode::STATION;
      Serial.println("✅ Connected to Starlink");
    } else {
      // Fallback to AP mode
      startAccessPoint();
      currentMode = WifiManagerMode::ACCESS_POINT;
      Serial.println("⚠️ Starlink unavailable, started Access Point");
    }
  }
  
  bool connectToStation() {
    Serial.printf("📡 Connecting to WiFi: %s\n", WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(false);  // Disable power saving for stable connection
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("✅ WiFi Connected!\n");
      Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("   Signal: %d dBm\n", WiFi.RSSI());
      digitalWrite(WIFI_LED_PIN, HIGH);
      return true;
    }
    
    Serial.println("❌ WiFi connection failed");
    return false;
  }
  
  void startAccessPoint() {
    Serial.printf("🔌 Starting Access Point: %s\n", AP_SSID);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    Serial.printf("✅ AP Started\n");
    Serial.printf("   SSID: %s\n", AP_SSID);
    Serial.printf("   Password: %s\n", AP_PASSWORD);
    Serial.printf("   IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    // Blink LED to indicate AP mode
    for (int i = 0; i < 3; i++) {
      digitalWrite(WIFI_LED_PIN, HIGH);
      delay(200);
      digitalWrite(WIFI_LED_PIN, LOW);
      delay(200);
    }
  }
  
  void loop() {
    // If in station mode and disconnected, try to reconnect
    if (currentMode == WifiManagerMode::STATION && WiFi.status() != WL_CONNECTED) {
      digitalWrite(WIFI_LED_PIN, LOW);
      
      if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
        Serial.println("⚠️ WiFi disconnected, attempting reconnect...");
        lastReconnectAttempt = millis();
        
        if (connectToStation()) {
          Serial.println("✅ Reconnected to WiFi");
        } else {
          Serial.println("❌ Reconnect failed, will try again in 30s");
        }
      }
    }
    
    // Keep LED on if connected
    if (currentMode == WifiManagerMode::STATION && WiFi.status() == WL_CONNECTED) {
      digitalWrite(WIFI_LED_PIN, HIGH);
    }
  }
  
  bool isConnected() {
    return (currentMode == WifiManagerMode::STATION && WiFi.status() == WL_CONNECTED) ||
           (currentMode == WifiManagerMode::ACCESS_POINT);
  }
  
  bool isStationMode() {
    return currentMode == WifiManagerMode::STATION && WiFi.status() == WL_CONNECTED;
  }
  
  WifiManagerMode getMode() {
    return currentMode;
  }
  
  int getSignalStrength() {
    if (currentMode == WifiManagerMode::STATION && WiFi.status() == WL_CONNECTED) {
      return WiFi.RSSI();
    }
    return 0;
  }
  
  String getIPAddress() {
    if (currentMode == WifiManagerMode::STATION) {
      return WiFi.localIP().toString();
    } else {
      return WiFi.softAPIP().toString();
    }
  }
};

#endif // WIFI_MANAGER_H
