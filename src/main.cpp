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
#include "screen_tama.h"
#include "screen_clock.h"

NetLog netlog;
ESP8266WebServer server(80);
QuotaSnapshot quotaNow;
Config cfg;
static bool pollPedido = false;
static uint32_t lastPollOk = 0;
// -1 = sin forzar (humor real); 0..5 = índice de Mood forzado por /demo.
// Ruta de debug temporal para probar los 6 humores en pantalla; T14 la retira.
static int demoMood = -1;
// Demo de pantalla reloj (T14 lo sustituye por cfg.screenMode).
static bool demoClock = false;

void setup() {
  LittleFS.begin();
  loadConfig(cfg);
  wifiConnectOrPortal(cfg);
  displayBegin(cfg.brightness);
  MDNS.begin("clawdgotchi");
  configTime(cfg.tz, "pool.ntp.org");  // POSIX TZ de config; la validacion de certs necesita hora
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
  // Debug temporal (task 12/13): fuerza humor o pantalla en demo.
  // GET /demo?mood=0..5 (HAPPY..OFFLINE): fuerza humor en pantalla tamagotchi.
  // GET /demo?clock=1: activa demo de pantalla reloj (localtime con TZ de config).
  // Sin argumentos: desactiva demos. Se retira en la task 14.
  server.on("/demo", [] {
    if (!webAuth(server, cfg)) return;
    // Limpia pantalla y invalida ambos modos en cualquier cambio
    tft.fillScreen(TFT_BLACK);
    clockInvalidate();
    tamaInvalidate();
    if (server.hasArg("clock")) {
      demoClock = server.arg("clock").toInt() != 0;
      demoMood = -1;  // desactiva mood si activamos clock
      server.send(200, "text/plain", String("demoClock=") + (demoClock ? "true" : "false"));
    } else if (server.hasArg("mood")) {
      int v = server.arg("mood").toInt();
      demoMood = (v >= 0 && v <= 5) ? v : -1;
      demoClock = false;  // desactiva clock si activamos mood
      server.send(200, "text/plain", String("demoMood=") + demoMood);
    } else {
      demoMood = -1;
      demoClock = false;
      server.send(200, "text/plain", "demos off");
    }
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
      if (now < 1600000000) {
        // NTP no sincronizo a tiempo: no pollear con reloj en epoca (rompe
        // formatCountdown en pantalla). Reintenta en el siguiente ciclo.
        pollPedido = true;
        LOGN("poll aplazado: sin NTP");
      } else {
        PollStatus st = pollQuota(t.access, quotaNow);
        if (st == PollStatus::OK) lastPollOk = (uint32_t)time(nullptr);
        LOGN("poll st=%d 5h=%u%% 7d=%u%%", (int)st, quotaNow.pct5h, quotaNow.pctWeek);
      }
    }
  }
  static uint32_t lastFrameMs = 0;
  static uint8_t frame = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastFrameMs >= 500) {
    lastFrameMs = nowMs;
    frame ^= 1;
    time_t now = time(nullptr);
    if (demoClock) {
      struct tm lt;
      localtime_r(&now, &lt);
      drawClock(lt);
    } else {
      Mood m = moodFor(quotaNow, WiFi.status() == WL_CONNECTED);
      if (demoMood >= 0) m = (Mood)demoMood;
      drawTamagotchi(quotaNow, m, (uint32_t)now, frame);
    }
  }
  delay(2);
}
