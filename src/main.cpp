#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <time.h>
#include "netlog.h"
#include "ota.h"
#include "store.h"
#include "wifi_portal.h"
#include "claude_client.h"
#include "webui.h"
#include "display.h"

NetLog netlog;
ESP8266WebServer server(80);
QuotaSnapshot quotaNow;
Config cfg;
static bool pollPedido = false;
static uint32_t lastPollOk = 0;

void setup() {
  LittleFS.begin();
  loadConfig(cfg);
  wifiConnectOrPortal(cfg);
  displayBegin(cfg.brightness);
  tft.setTextColor(TFT_ORANGE);
  tft.drawString("clawdgotchi", 60, 110, 4);
  MDNS.begin("clawdgotchi");
  configTime(0, 0, "pool.ntp.org");  // la validacion de certs necesita hora
  webuiRoutes(server, cfg, quotaNow);
  server.on("/log", [] {
    if (!webAuth(server, cfg)) return;
    server.send(200, "text/plain", netlog.text());
  });
  // El TLS del poll ya NO corre dentro del handler HTTP (crasheaba la pila
  // "cont" del WebServer): esta ruta solo pide el poll, loop() lo ejecuta.
  server.on("/poll", [] {
    if (!webAuth(server, cfg)) return;
    if (!LittleFS.exists("/tokens.json")) { server.send(400, "text/plain", "sin token guardado"); return; }
    pollPedido = true;
    server.send(200, "text/plain", "poll programado; mira /status");
  });
  server.on("/status", [] {
    if (!webAuth(server, cfg)) return;
    server.send(200, "text/plain", String("valid=") + quotaNow.valid + " 5h=" + quotaNow.pct5h +
                "% 7d=" + quotaNow.pctWeek + "% exhausted=" + quotaNow.exhausted +
                " lastPollOk=" + lastPollOk);
  });
  otaBegin(server, cfg);
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  server.handleClient();
  MDNS.update();
  if (pollPedido && WiFi.status() == WL_CONNECTED) {
    pollPedido = false;
    static Tokens t;  // ponytail: fuera de la pila de loop(), ver store.h
    if (loadTokens(t)) {
      time_t now = 0;
      for (int i = 0; i < 40 && (now = time(nullptr)) < 1600000000; i++) delay(250);
      PollStatus st = pollQuota(t.access, quotaNow);
      if (st == PollStatus::OK) lastPollOk = (uint32_t)time(nullptr);
      LOGN("poll st=%d 5h=%u%% 7d=%u%%", (int)st, quotaNow.pct5h, quotaNow.pctWeek);
    }
  }
  delay(2);
}
