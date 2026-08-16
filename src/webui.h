#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "store.h"
#include "quota.h"
#include "htmlutil.h"
#include "display.h"

// Definidas en main.cpp; el orquestador (task 14) es quien conoce el estado
// real de arranque y de autenticacion con Claude.
extern bool safeMode;
extern bool authBad;
extern bool pollPedido;
// Buffer de Tokens compartido con runPoll() en main.cpp, ver su declaracion.
extern Tokens g_tokens;

static const char SETTINGS_HTML[] PROGMEM = R"(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clawdgotchi</title>
<body style="font-family:sans-serif;background:#222;color:#eee;max-width:34em;margin:2em auto">
<h2>&#129408; Clawdgotchi</h2>
%SAFEMODE%
<p>5h: %P5%% &middot; semana: %P7%% &middot; heap %HEAP%</p>
<form method=POST action=/cfg>
<h3>Pantalla</h3>
Modo: <select name=mode>
<option value=0 %M0%>Rotaci&oacute;n</option>
<option value=1 %M1%>Tamagotchi</option>
<option value=2 %M2%>Reloj</option></select>
&nbsp; Rotar cada <input name=rot size=3 value=%ROT%> s<br><br>
Brillo: <input type=range name=brt min=5 max=100 value=%BRT%><br><br>
Zona horaria (POSIX): <input name=tz size=30 value="%TZ%"><br><br>
<h3>Claude</h3>
Consultar cada <input name=poll size=3 value=%POLL%> min<br><br>
<h3>Seguridad</h3>
Password web UI: <input name=webpass value="%WP%"><br>
<small>Tras cambiarla, reinicia el cubo para que se aplique tambi&eacute;n a /update.</small><br><br>
<input type=submit value=Guardar>
</form>
<form method=POST action=/token>
<h3>Token de Claude</h3>
%TOKMSG%
<p>En tu ordenador: <code>claude setup-token</code> y pega aqu&iacute; el resultado.
O pega un refresh token para renovaci&oacute;n autom&aacute;tica.</p>
Access: <input name=access size=40><br><br>
Refresh (opcional): <input name=refresh size=40><br><br>
<input type=submit value="Guardar token" %TOKDIS%>
</form>
<p><a href=/log style="color:#8cf">log</a> &middot; <a href=/update style="color:#8cf">actualizar firmware</a></p>)";

inline bool webAuth(ESP8266WebServer& s, const Config& c) {
  if (!c.webPass[0]) return true;
  if (s.authenticate("admin", c.webPass)) return true;
  s.requestAuthentication();
  return false;
}

// quotaNow: la última QuotaSnapshot que mantiene el loop principal (task 14).
inline void webuiRoutes(ESP8266WebServer& s, Config& cfg, QuotaSnapshot& quotaNow) {
  s.on("/", [&] {
    if (!webAuth(s, cfg)) return;
    String p = FPSTR(SETTINGS_HTML);
    p.replace("%P5%", String(quotaNow.pct5h));  p.replace("%P7%", String(quotaNow.pctWeek));
    p.replace("%HEAP%", String(ESP.getFreeHeap()));
    p.replace("%M0%", cfg.screenMode == 0 ? "selected" : "");
    p.replace("%M1%", cfg.screenMode == 1 ? "selected" : "");
    p.replace("%M2%", cfg.screenMode == 2 ? "selected" : "");
    p.replace("%ROT%", String(cfg.rotateSeconds)); p.replace("%BRT%", String(cfg.brightness));
    p.replace("%POLL%", String(cfg.pollMinutes));
    p.replace("%SAFEMODE%", safeMode ?
      "<p style=color:#f88><b>MODO SEGURO</b> tras 3 crashes. Desenchufa y vuelve a enchufar para reintentar el arranque normal, o sube un firmware corregido en /update.</p>" : "");
    if (cfg.webPass[0]) {
      p.replace("%TOKMSG%", authBad ?
        "<p style=color:#f88>Token invalido o caducado sin refresh posible. Pega uno nuevo abajo.</p>" : "");
      p.replace("%TOKDIS%", "");
    } else {
      p.replace("%TOKMSG%", "<p style=color:#f88>Configura primero una contrase&ntilde;a arriba y guarda para poder guardar el token.</p>");
      p.replace("%TOKDIS%", "disabled");
    }
    // Texto libre de usuario al final y escapado: evita tanto XSS (< > " &)
    // como que un valor con "%XX%" dentro reescriba otro placeholder ya
    // sustituido por los .replace() de arriba (htmlEscape tambien escapa %).
    p.replace("%TZ%", htmlEscape(cfg.tz));
    p.replace("%WP%", htmlEscape(cfg.webPass));
    s.send(200, "text/html", p);
  });
  s.on("/cfg", HTTP_POST, [&] {
    if (!webAuth(s, cfg)) return;
    cfg.screenMode = s.arg("mode").toInt();
    cfg.rotateSeconds = constrain(s.arg("rot").toInt(), 3, 3600);
    cfg.brightness = constrain(s.arg("brt").toInt(), 5, 100);
    snprintf(cfg.tz, sizeof cfg.tz, "%s", s.arg("tz").c_str());
    cfg.pollMinutes = constrain(s.arg("poll").toInt(), 1, 1440);
    snprintf(cfg.webPass, sizeof cfg.webPass, "%s", s.arg("webpass").c_str());
    saveConfig(cfg);
    setTZ(cfg.tz);  // aplicar TZ inmediatamente sin necesidad de reiniciar
    displayBrightness(cfg.brightness);
    // pollMinutes puede haber cambiado: nextPollMs (planificador de
    // main.cpp) es un static local a runPoll(), no expuesto aqui. En vez de
    // exponerlo con otro extern, reusamos pollPedido para forzar un poll ya
    // mismo: el poll siguiente vuelve a calcular nextPollMs con el intervalo
    // nuevo. Efecto secundario asumido: cualquier guardado en /cfg (aunque
    // solo cambie brillo o TZ) adelanta el proximo poll.
    pollPedido = true;
    s.sendHeader("Location", "/");
    s.send(303);
  });
  s.on("/token", HTTP_POST, [&] {
    if (!webAuth(s, cfg)) return;
    if (!cfg.webPass[0]) { s.send(403, "text/plain", "configura primero una contrase\xc3\xb1a"); return; }
    Tokens& t = g_tokens;  // buffer compartido, ver declaracion en main.cpp
    loadTokens(t);
    if (s.arg("access").length())  snprintf(t.access, sizeof t.access, "%s", s.arg("access").c_str());
    if (s.arg("refresh").length()) snprintf(t.refresh, sizeof t.refresh, "%s", s.arg("refresh").c_str());
    if (s.arg("exp").length())     t.expiresAt = strtoul(s.arg("exp").c_str(), nullptr, 10);
    saveTokens(t);
    authBad = false;  // token nuevo: da otra oportunidad antes de volver a marcarlo
    pollPedido = true;  // revalida el token nuevo ya mismo, no esperar al proximo ciclo
    s.sendHeader("Location", "/");
    s.send(303);
  });
}
