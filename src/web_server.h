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

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.begin();
}

#endif
