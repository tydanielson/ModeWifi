#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include "van_state.h"
#include "web_interface.h"

extern WebServer server;

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleStatus() {
  String json = "{";
  json += "\"voltage\":" + String(vanState.voltage, 1) + ",";
  json += "\"temp\":" + String(vanState.glycolTemp, 1) + ",";
  
  json += "\"pdm1\":[";
  const char* pdm1Names[] = {"SOLAR_BACKUP", "CARGO_LIGHTS", "READING_LIGHT", "CABIN_LIGHTS",
                             "AWNING_LIGHTS", "RECIRC_PUMP", "AWNING_ENABLE", "PDM_1_8", 
                             "PDM_1_9", "EXHAUST_FAN", "FURNACE_POWER", "WATER_PUMP"};
  for (int i = 0; i < 6; i++) {
    if (i > 0) json += ",";
    json += "{\"name\":\"" + String(pdm1Names[i]) + "\",\"value\":" + String(vanState.pdm1_ch1_6[i]) + "}";
  }
  for (int i = 0; i < 6; i++) {
    json += ",{\"name\":\"" + String(pdm1Names[i+6]) + "\",\"value\":" + String(vanState.pdm1_ch7_12[i]) + "}";
  }
  json += "],";
  
  json += "\"pdm2\":[";
  const char* pdm2Names[] = {"PDM_2_1", "GALLEY_FAN", "REFRIGERATOR", "12V_USB",
                             "AWNING_M_PLUS", "AWNING_M_MINUS", "TANK_MON_PWR", "POWER_SW",
                             "HVAC_POWER", "12V_SPEAKER", "SINK_PUMP", "AUX_POWER"};
  for (int i = 0; i < 6; i++) {
    if (i > 0) json += ",";
    json += "{\"name\":\"" + String(pdm2Names[i]) + "\",\"value\":" + String(vanState.pdm2_ch1_6[i]) + "}";
  }
  for (int i = 0; i < 6; i++) {
    json += ",{\"name\":\"" + String(pdm2Names[i+6]) + "\",\"value\":" + String(vanState.pdm2_ch7_12[i]) + "}";
  }
  json += "]";
  
  json += "}";
  
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.begin();
}

#endif
