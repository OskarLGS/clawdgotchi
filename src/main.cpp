#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <time.h>
#include "netlog.h"
#include "ota.h"
#include "spike.h"
#include "store.h"
#include "claude_client.h"

#ifndef CLAWD_WIFI_SSID
#define CLAWD_WIFI_SSID ""
#define CLAWD_WIFI_PASS ""
#endif

NetLog netlog;
ESP8266WebServer server(80);
QuotaSnapshot quotaNow;

void setup() {
  LittleFS.begin();
  WiFi.mode(WIFI_STA);
  WiFi.begin(CLAWD_WIFI_SSID, CLAWD_WIFI_PASS);
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  LOGN("wifi: %s ip=%s", WiFi.status() == WL_CONNECTED ? "ok" : "FAIL",
       WiFi.localIP().toString().c_str());
  MDNS.begin("clawdgotchi");
  configTime(0, 0, "pool.ntp.org");  // la validacion de certs necesita hora
  server.on("/", [] {
    server.send(200, "text/plain",
      "clawdgotchi\nheap=" + String(ESP.getFreeHeap()) +
      " maxblock=" + String(ESP.getMaxFreeBlockSize()) +
      "\nuptime=" + String(millis() / 1000) + "s\n");
  });
  server.on("/log", [] { server.send(200, "text/plain", netlog.text()); });
  server.on("/token", HTTP_POST, [] {
    Tokens t;
    loadTokens(t);  // conserva lo que ya hubiera si solo llega uno de los dos args
    if (server.arg("access").length()) snprintf(t.access, sizeof t.access, "%s", server.arg("access").c_str());
    if (server.arg("refresh").length()) snprintf(t.refresh, sizeof t.refresh, "%s", server.arg("refresh").c_str());
    saveTokens(t);
    server.send(200, "text/plain", "guardado");
  });
  server.on("/poll", [] {
    Tokens t;
    if (!loadTokens(t)) { server.send(400, "text/plain", "sin token guardado"); return; }
    time_t now = 0;
    for (int i = 0; i < 40 && (now = time(nullptr)) < 1600000000; i++) delay(250);
    PollStatus st = pollQuota(t.access, quotaNow);
    server.send(200, "text/plain", String("st=") + (int)st + " 5h=" + quotaNow.pct5h +
                "% 7d=" + quotaNow.pctWeek + "% exhausted=" + quotaNow.exhausted);
  });
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
