#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "netlog.h"
#include "ota.h"
#include "spike.h"
#include "store.h"

#ifndef CLAWD_WIFI_SSID
#define CLAWD_WIFI_SSID ""
#define CLAWD_WIFI_PASS ""
#endif

NetLog netlog;
ESP8266WebServer server(80);

void setup() {
  LittleFS.begin();
  WiFi.mode(WIFI_STA);
  WiFi.begin(CLAWD_WIFI_SSID, CLAWD_WIFI_PASS);
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  LOGN("wifi: %s ip=%s", WiFi.status() == WL_CONNECTED ? "ok" : "FAIL",
       WiFi.localIP().toString().c_str());
  MDNS.begin("clawdgotchi");
  server.on("/", [] {
    server.send(200, "text/plain",
      "clawdgotchi\nheap=" + String(ESP.getFreeHeap()) +
      " maxblock=" + String(ESP.getMaxFreeBlockSize()) +
      "\nuptime=" + String(millis() / 1000) + "s\n");
  });
  server.on("/log", [] { server.send(200, "text/plain", netlog.text()); });
  otaBegin(server);
  spikeRoutes(server);
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  server.handleClient();
  MDNS.update();
  delay(2);
}
