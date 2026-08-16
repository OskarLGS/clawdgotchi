#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "store.h"
#include "quota.h"
#include "htmlutil.h"
#include "display.h"
#include "oauth_login.h"

// Definidas en main.cpp; el orquestador (task 14) es quien conoce el estado
// real de arranque y de autenticacion con Claude.
extern bool safeMode;
extern bool authBad;
extern bool pollPedido;
// Buffer de Tokens compartido con runPoll() en main.cpp, ver su declaracion.
extern Tokens g_tokens;
// Login OAuth (Task 16): /oauth/finish (aqui) solo guarda el codigo pegado y
// levanta el flag; el canje TLS lo hace loop() en main.cpp (nunca TLS dentro
// de un handler HTTP, ver oauth_login.h).
extern bool oauthPending;
extern String g_pendingCode;

static const char SETTINGS_HTML[] PROGMEM = R"(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clawdgotchi</title>
<style>
:root{--bg:#1b1b19;--card:#26251f;--fg:#ededed;--muted:#9d988c;--coral:#d97757;--bd:#39392f}
*{box-sizing:border-box}
body{font-family:-apple-system,system-ui,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg);margin:0;padding:16px;line-height:1.5}
.wrap{max-width:30em;margin:0 auto}
h1{font-size:1.45em;margin:.1em 0 .7em;display:flex;align-items:center;gap:.35em}
.card{background:var(--card);border:1px solid var(--bd);border-radius:14px;padding:16px;margin-bottom:14px}
.card h2{font-size:.72em;text-transform:uppercase;letter-spacing:.09em;color:var(--muted);margin:0 0 12px;font-weight:600}
label{display:block;font-size:.9em;color:var(--muted);margin:14px 0 5px}
input,select{width:100%;padding:10px 12px;background:#151513;border:1px solid var(--bd);border-radius:8px;color:var(--fg);font-size:1em;font-family:inherit}
input[type=range]{padding:0;accent-color:var(--coral)}
.row{display:flex;gap:10px}.row>div{flex:1}
button,.btn{display:block;width:100%;margin-top:16px;padding:12px;background:var(--coral);color:#1b1b19;border:0;border-radius:9px;font-size:1em;font-weight:600;cursor:pointer;text-align:center;text-decoration:none}
.btn.ghost{background:transparent;color:var(--coral);border:1px solid var(--coral)}
.bar{height:20px;background:#151513;border-radius:6px;overflow:hidden;margin:4px 0 14px}
.bar>span{display:block;height:100%;border-radius:6px;transition:width .3s}
.q{display:flex;justify-content:space-between;font-size:.92em;margin-bottom:3px}
.muted{color:var(--muted);font-size:.85em;margin-top:6px}
.warn{background:#3a2420;border:1px solid var(--coral);border-radius:9px;padding:10px 12px;color:#f0b8a6;font-size:.9em;margin-bottom:12px}
a{color:var(--coral)}
details{margin-top:14px}details summary{cursor:pointer;color:var(--muted);font-size:.9em}
.foot{text-align:center;font-size:.88em;margin-top:2px}
</style>
<body><div class=wrap>
<h1>&#129408; Clawdgotchi</h1>
%SAFEMODE%
<div class=card>
<h2>Uso de Claude</h2>
<div class=q><span>5 horas</span><span>%P5%%</span></div>
<div class=bar><span style="width:%P5%%;background:%C5%"></span></div>
<div class=q><span>Semana</span><span>%P7%%</span></div>
<div class=bar><span style="width:%P7%%;background:%C7%"></span></div>
<div class=muted>Heap libre: %HEAP% B</div>
</div>
<form method=POST action=/cfg><div class=card>
<h2>Ajustes</h2>
<label>Pantalla</label>
<select name=mode><option value=0 %M0%>Rotaci&oacute;n autom&aacute;tica</option><option value=1 %M1%>Solo tamagotchi</option><option value=2 %M2%>Solo reloj</option></select>
<div class=row><div><label>Rotar cada (s)</label><input name=rot value=%ROT%></div><div><label>Consultar (min)</label><input name=poll value=%POLL%></div></div>
<label>Brillo: <span id=bv>%BRT%</span>%</label><input type=range name=brt min=5 max=100 value=%BRT% oninput="bv.textContent=this.value">
<label>Zona horaria (POSIX)</label><input name=tz value="%TZ%">
<label>Contrase&ntilde;a web &mdash; usuario: admin</label><input name=webpass value="%WP%">
<div class=muted>Reinicia el cubo para aplicar la contrase&ntilde;a tambi&eacute;n a /update.</div>
<button>Guardar ajustes</button>
</div></form>
<div class=card>
<h2>Sesi&oacute;n de Claude</h2>
%OAUTHMSG%
<a class="btn ghost" href=/oauth/start target=_blank %OAUTHDIS%>1 &middot; Abrir login de Claude</a>
<form method=POST action=/oauth/finish>
<label>2 &middot; Pega el c&oacute;digo que te da Claude</label>
<input name=code %OAUTHDIS%>
<button %OAUTHDIS%>Iniciar sesi&oacute;n</button>
</form>
<details><summary>Alternativa: pegar token manual</summary>
%TOKMSG%
<form method=POST action=/token>
<label>Access token (de <code>claude setup-token</code>)</label><input name=access>
<label>Refresh token (opcional)</label><input name=refresh>
<button %TOKDIS%>Guardar token</button>
</form></details>
</div>
<div class=foot><a href=/log>Registro</a> &middot; <a href=/update>Actualizar firmware</a></div>
</div></body>)";

inline bool webAuth(ESP8266WebServer& s, const Config& c) {
  if (!c.webPass[0]) return true;
  if (s.authenticate("admin", c.webPass)) return true;
  s.requestAuthentication();
  return false;
}

// Color de la barra de cuota por nivel (mismos tramos que el humor de la
// mascota): verde <50, ambar <75, naranja <90, rojo >=90.
inline const char* barColor(uint8_t pct) {
  if (pct >= 90) return "#e2574d";
  if (pct >= 75) return "#d97757";
  if (pct >= 50) return "#d9a441";
  return "#5fb872";
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
    p.replace("%C5%", barColor(quotaNow.pct5h)); p.replace("%C7%", barColor(quotaNow.pctWeek));
    p.replace("%ROT%", String(cfg.rotateSeconds)); p.replace("%BRT%", String(cfg.brightness));
    p.replace("%POLL%", String(cfg.pollMinutes));
    p.replace("%SAFEMODE%", safeMode ?
      "<div class=warn><b>Modo seguro</b> tras 3 crashes. Se reintentar&aacute; el arranque normal en unos segundos, o sube un firmware corregido en /update.</div>" : "");
    if (cfg.webPass[0]) {
      p.replace("%TOKMSG%", authBad ?
        "<div class=warn>Token inv&aacute;lido o caducado sin refresh. Inicia sesi&oacute;n de nuevo o pega uno nuevo.</div>" : "");
      p.replace("%TOKDIS%", "");
      p.replace("%OAUTHMSG%", oauthPending ?
        "<div class=warn>Procesando login&hellip; recarga esta p&aacute;gina en unos segundos.</div>" : "");
      p.replace("%OAUTHDIS%", "");
    } else {
      p.replace("%TOKMSG%", "<div class=warn>Configura y guarda una contrase&ntilde;a arriba primero.</div>");
      p.replace("%TOKDIS%", "disabled");
      p.replace("%OAUTHMSG%", "<div class=warn>Configura y guarda una contrase&ntilde;a arriba primero.</div>");
      p.replace("%OAUTHDIS%", "disabled");
    }
    // Texto libre de usuario al final y escapado: evita tanto XSS (< > " &)
    // como que un valor con "%XX%" dentro reescriba otro placeholder ya
    // sustituido por los .replace() de arriba (htmlEscape tambien escapa %).
    p.replace("%TZ%", htmlEscape(cfg.tz));
    p.replace("%WP%", htmlEscape(cfg.webPass));
    s.sendHeader("Cache-Control", "no-store");  // muestra siempre los valores recien guardados
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
  s.on("/oauth/start", [&] {
    if (!webAuth(s, cfg)) return;
    if (!cfg.webPass[0]) { s.send(403, "text/plain", "configura primero una contrase\xc3\xb1a"); return; }
    String url = oauthBuildUrl();
    url.replace("&", "&amp;");  // valido dentro de un atributo href
    String p = "<!doctype html><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>Clawdgotchi</title>"
      "<body style=\"font-family:sans-serif;background:#222;color:#eee;max-width:34em;margin:2em auto\">"
      "<h2>Iniciar sesi\xc3\xb3n con Claude</h2>"
      "<p>Toca el enlace, inicia sesi\xc3\xb3n en Claude y autoriza el acceso. "
      "Al final Claude te muestra un c\xc3\xb3digo: c\xc3\xb3pialo entero y vuelve a la p\xc3\xa1gina "
      "anterior para pegarlo.</p>"
      "<p><a href=\"" + url + "\" target=_blank style=\"font-size:1.3em\">Abrir login de Claude &rarr;</a></p>"
      "<p><a href=/ style=color:#8cf>&larr; volver</a></p>";
    s.send(200, "text/html", p);
  });
  s.on("/oauth/finish", HTTP_POST, [&] {
    if (!webAuth(s, cfg)) return;
    if (!cfg.webPass[0]) { s.send(403, "text/plain", "configura primero una contrase\xc3\xb1a"); return; }
    g_pendingCode = s.arg("code");
    oauthPending = true;
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
