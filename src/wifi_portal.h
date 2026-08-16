#pragma once
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "store.h"
#include "netlog.h"

#ifndef CLAWD_WIFI_SSID
#define CLAWD_WIFI_SSID ""
#define CLAWD_WIFI_PASS ""
#endif

static const char PORTAL_HTML[] PROGMEM = R"(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clawdgotchi</title>
<body style="font-family:sans-serif;background:#222;color:#eee;padding:2em">
<h2>Clawdgotchi &mdash; WiFi</h2>
<form method=POST action=/save>
Red: <select name=ssid>%OPTS%</select><br><br>
O escribe la red: <input name=ssid_manual><br><br>
Clave: <input type=password name=pass><br><br>
<input type=submit value=Guardar></form>)";

// escapa & < > " para no inyectar HTML/JS via SSIDs escaneados (nombres de
// red los pone quien los crea, no son de fiar)
inline String htmlEscape(const String& in) {
  String s = in;
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

// ponytail: las credenciales compiladas (CLAWD_WIFI_SSID/PASS) se conservan
// como fallback de un solo uso cuando la config esta vacia (LittleFS recien
// formateado). Sin esto, un flasheo desatendido dejaria el cubo esperando en
// modo AP para siempre en vez de conectar solo. Una vez usadas se migran a
// config.json y no se vuelven a tocar.
inline bool wifiTryConnect(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  return WiFi.status() == WL_CONNECTED;
}

inline bool wifiConnectOrPortal(Config& cfg) {
  if (!cfg.wifiSsid[0] && CLAWD_WIFI_SSID[0]) {
    snprintf(cfg.wifiSsid, sizeof cfg.wifiSsid, "%s", CLAWD_WIFI_SSID);
    snprintf(cfg.wifiPass, sizeof cfg.wifiPass, "%s", CLAWD_WIFI_PASS);
    saveConfig(cfg);
    LOGN("wifi: migradas credenciales compiladas");
  }
  if (cfg.wifiSsid[0]) {
    if (wifiTryConnect(cfg.wifiSsid, cfg.wifiPass)) {
      LOGN("wifi ok %s", WiFi.localIP().toString().c_str());
      return true;
    }
    LOGN("wifi fallo, abriendo portal");
  }
  // scan en STA antes de pasar a AP: en algunos cores de ESP8266 el scan
  // devuelve 0 redes si ya se cambio a WIFI_AP puro, orden mas fiable
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  String opts;
  for (int i = 0; i < n; i++)
    opts += "<option>" + htmlEscape(WiFi.SSID(i)) + "</option>";
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Clawdgotchi-Setup");
  DNSServer dns;
  dns.start(53, "*", WiFi.softAPIP());          // portal cautivo
  ESP8266WebServer portal(80);
  portal.onNotFound([&] {
    String page = FPSTR(PORTAL_HTML);
    page.replace("%OPTS%", opts);
    portal.send(200, "text/html", page);
  });
  portal.on("/save", HTTP_POST, [&] {
    // ssid_manual (texto libre) tiene prioridad sobre el select: el scan
    // puede no listar la red del usuario (oculta, o 0 resultados)
    String ssid = portal.arg("ssid_manual").length() ? portal.arg("ssid_manual") : portal.arg("ssid");
    snprintf(cfg.wifiSsid, sizeof cfg.wifiSsid, "%s", ssid.c_str());
    snprintf(cfg.wifiPass, sizeof cfg.wifiPass, "%s", portal.arg("pass").c_str());
    saveConfig(cfg);
    portal.send(200, "text/html", "Guardado. Reiniciando...");
    delay(1000);
    ESP.restart();
  });
  portal.begin();
  while (true) { dns.processNextRequest(); portal.handleClient(); delay(2); }  // sale por restart
  return false;  // inalcanzable, silencia -Wreturn-type
}
