#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "store.h"
#include "quota.h"
#include "htmlutil.h"
#include "display.h"
#include "oauth_login.h"
#include "oauth_codec.h"
#include "ota.h"

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
// Resultado del ultimo poll (main.cpp), para avisar si la API lo rechaza.
extern PollStatus lastPollStatus;
extern int lastPollCode;

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
%APIERR%
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
<label>Transici&oacute;n / fade: <span id=fv>%FADE%</span> ms</label><input type=range name=fade min=0 max=1000 step=50 value=%FADE% oninput="fv.textContent=this.value">
<label>Brillo: <span id=bv>%BRT%</span>%</label><input type=range name=brt min=5 max=100 value=%BRT% oninput="bv.textContent=this.value">
<label>Zona horaria (POSIX)</label><input name=tz value="%TZ%">
<label>Servidor NTP</label><input name=ntp value="%NTP%">
<label>Modelo para consultar la cuota</label><input name=model value="%MODEL%">
<label>Contrase&ntilde;a web &mdash; usuario: admin</label><input type=password name=webpass maxlength=32 autocomplete=new-password placeholder="%WPHINT%">
<div class=muted>1-32 caracteres. Vac&iacute;o = no cambiarla. Se aplica al momento, tambi&eacute;n a /update.</div>
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

// Anti-CSRF para los POST: el navegador reenvia solo la Basic Auth cacheada,
// asi que una web ajena podria mandar un formulario a /cfg o /token. Si la
// peticion trae Origin (o Referer) debe ser este mismo host; sin ninguno de
// los dos (curl, tools/flash-ota.sh) se acepta. Requiere collectHeaders()
// (lo hace webuiRoutes). Responde 403 y devuelve false si no pasa.
inline bool sameOrigin(ESP8266WebServer& s) {
  String self = String("http://") + s.hostHeader();
  String o = s.header("Origin");
  bool ok;
  if (o.length()) ok = (o == self);
  else {
    String r = s.header("Referer");
    ok = !r.length() || r == self || r.startsWith(self + "/");
  }
  if (!ok) s.send(403, "text/plain", "origen no permitido");
  return ok;
}

// Modelo: id tipo "claude-haiku-4-5" ([a-z0-9.-], 1..47). Evita guardar
// basura que haria fallar cada poll.
inline bool validModelId(const String& m) {
  if (!m.length() || m.length() >= sizeof(Config::model)) return false;
  for (size_t i = 0; i < m.length(); i++) {
    char c = m[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.')) return false;
  }
  return true;
}

// Color de la barra de cuota por nivel (mismos tramos que el humor de la
// mascota): verde <50, ambar <75, naranja <90, rojo >=90.
inline const char* barColor(uint8_t pct) {
  if (pct >= 90) return "#e2574d";
  if (pct >= 75) return "#d97757";
  if (pct >= 50) return "#d9a441";
  return "#5fb872";
}

// Envia una plantilla PROGMEM sustituyendo cada %CLAVE% ([A-Z0-9]+) por
// value(clave), en UNA sola pasada y por trozos (chunked). Frente a copiar la
// plantilla a un String y encadenar .replace(): no reserva ~5KB de heap de
// golpe (fragmentaba el bloque contiguo que necesita el TLS) y un valor de
// usuario nunca se re-escanea como placeholder. Un '%' que no forma
// %CLAVE% (p.ej. "%P5%%" o "%</label>") se emite tal cual.
template <typename Server, typename F>
inline void sendTemplate(Server& s, PGM_P tpl, F value) {
  s.setContentLength(CONTENT_LENGTH_UNKNOWN);
  s.send(200, "text/html", "");
  char buf[256];
  size_t n = 0;
  auto flush = [&] { if (n) { s.sendContent(buf, n); n = 0; } };
  size_t len = strlen_P(tpl);
  for (size_t i = 0; i < len;) {
    char c = pgm_read_byte(tpl + i);
    if (c == '%') {
      char key[12];
      size_t k = 0, j = i + 1;
      while (j < len && k < sizeof key - 1) {
        char d = pgm_read_byte(tpl + j);
        if (!((d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9'))) break;
        key[k++] = d; j++;
      }
      key[k] = 0;
      if (k && j < len && pgm_read_byte(tpl + j) == '%') {
        flush();
        String v = value(key);
        if (v.length()) s.sendContent(v);  // nunca vacio: un trozo de 0 bytes cierra el chunked
        i = j + 1;
        continue;
      }
    }
    buf[n++] = c;
    if (n == sizeof buf) flush();
    i++;
  }
  flush();
  s.chunkedResponseFinalize();
}

// quotaNow: la última QuotaSnapshot que mantiene el loop principal (task 14).
inline void webuiRoutes(ESP8266WebServer& s, Config& cfg, QuotaSnapshot& quotaNow) {
  s.collectHeaders("Origin", "Referer");  // para sameOrigin()

  s.on("/", [&] {
    if (!webAuth(s, cfg)) return;
    static const char NEEDPASS[] = "<div class=warn>Configura y guarda una contrase&ntilde;a arriba primero.</div>";
    bool hasPass = cfg.webPass[0] != 0;
    s.sendHeader("Cache-Control", "no-store");  // muestra siempre los valores recien guardados
    // Todo texto de usuario pasa por htmlEscape (XSS).
    sendTemplate(s, SETTINGS_HTML, [&](const char* k) -> String {
      if (!strcmp(k, "P5")) return String(quotaNow.pct5h);
      if (!strcmp(k, "P7")) return String(quotaNow.pctWeek);
      if (!strcmp(k, "C5")) return barColor(quotaNow.pct5h);
      if (!strcmp(k, "C7")) return barColor(quotaNow.pctWeek);
      if (!strcmp(k, "HEAP")) return String(ESP.getFreeHeap());
      if (!strcmp(k, "M0")) return cfg.screenMode == 0 ? "selected" : "";
      if (!strcmp(k, "M1")) return cfg.screenMode == 1 ? "selected" : "";
      if (!strcmp(k, "M2")) return cfg.screenMode == 2 ? "selected" : "";
      if (!strcmp(k, "ROT")) return String(cfg.rotateSeconds);
      if (!strcmp(k, "BRT")) return String(cfg.brightness);
      if (!strcmp(k, "POLL")) return String(cfg.pollMinutes);
      if (!strcmp(k, "FADE")) return String(cfg.fadeMs);
      if (!strcmp(k, "TZ")) return htmlEscape(cfg.tz);
      if (!strcmp(k, "NTP")) return htmlEscape(cfg.ntp);
      if (!strcmp(k, "MODEL")) return htmlEscape(cfg.model);
      if (!strcmp(k, "WPHINT")) return hasPass ? "(sin cambios)" : "elige una";
      if (!strcmp(k, "SAFEMODE")) return safeMode ?
        "<div class=warn><b>Modo seguro</b> tras 3 crashes. Se reintentar&aacute; el arranque normal en unos segundos, o sube un firmware corregido en /update.</div>" : "";
      if (!strcmp(k, "APIERR")) return lastPollStatus == PollStatus::API_ERROR ?
        String("<div class=warn>La API de Claude rechaz&oacute; la consulta de cuota (HTTP ") + lastPollCode +
        "). Revisa el modelo en Ajustes y mira el <a href=/log>registro</a>.</div>" : String();
      if (!strcmp(k, "TOKMSG")) return !hasPass ? NEEDPASS : authBad ?
        "<div class=warn>Token inv&aacute;lido o caducado sin refresh. Inicia sesi&oacute;n de nuevo o pega uno nuevo.</div>" : "";
      if (!strcmp(k, "OAUTHMSG")) return !hasPass ? NEEDPASS : oauthPending ?
        "<div class=warn>Procesando login&hellip; recarga esta p&aacute;gina en unos segundos.</div>" : "";
      if (!strcmp(k, "TOKDIS") || !strcmp(k, "OAUTHDIS")) return hasPass ? "" : "disabled";
      return "";
    });
  });
  s.on("/cfg", HTTP_POST, [&] {
    if (!webAuth(s, cfg) || !sameOrigin(s)) return;
    // Valida antes de tocar cfg: un POST rechazado no deja la config a medias.
    String wp = s.arg("webpass"), model = s.arg("model");
    if (wp.length() && !validWebPass(wp.c_str())) {
      s.send(400, "text/plain; charset=utf-8", "contraseña no válida: 1-32 caracteres ASCII imprimibles"); return;
    }
    if (model.length() && !validModelId(model)) { s.send(400, "text/plain; charset=utf-8", "modelo no válido"); return; }
    cfg.screenMode = constrain(s.arg("mode").toInt(), 0, 2);
    cfg.rotateSeconds = constrain(s.arg("rot").toInt(), 3, 3600);
    cfg.brightness = constrain(s.arg("brt").toInt(), 5, 100);
    cfg.fadeMs = constrain(s.arg("fade").toInt(), 0, 2000);
    snprintf(cfg.tz, sizeof cfg.tz, "%s", s.arg("tz").c_str());
    if (s.arg("ntp").length()) snprintf(cfg.ntp, sizeof cfg.ntp, "%s", s.arg("ntp").c_str());
    if (model.length()) snprintf(cfg.model, sizeof cfg.model, "%s", model.c_str());
    cfg.pollMinutes = constrain(s.arg("poll").toInt(), 1, 1440);
    // Campo vacio = no cambiarla: asi la contraseña nunca se manda de vuelta al
    // navegador ni se puede quitar con un POST que olvide el campo.
    if (wp.length()) {
      snprintf(cfg.webPass, sizeof cfg.webPass, "%s", wp.c_str());
      otaUpdater.updateCredentials("admin", cfg.webPass);  // /update protegido ya, sin reiniciar
    }
    saveConfig(cfg);
    configTime(cfg.tz, cfg.ntp);  // aplica TZ y servidor NTP en vivo (re-sincroniza)
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
    if (!cfg.webPass[0]) { s.send(403, "text/plain; charset=utf-8", "configura primero una contraseña"); return; }
    String url = oauthBuildUrl();
    url.replace("&", "&amp;");  // valido dentro de un atributo href
    String p = "<!doctype html><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>Clawdgotchi</title>"
      "<body style=\"font-family:sans-serif;background:#222;color:#eee;max-width:34em;margin:2em auto\">"
      "<h2>Iniciar sesión con Claude</h2>"
      "<p>Toca el enlace, inicia sesión en Claude y autoriza el acceso. "
      "Al final Claude te muestra un código: cópialo entero y vuelve a la página "
      "anterior para pegarlo.</p>"
      "<p><a href=\"" + url + "\" target=_blank rel=noreferrer style=\"font-size:1.3em\">Abrir login de Claude &rarr;</a></p>"
      "<p><a href=/ style=color:#8cf>&larr; volver</a></p>";
    s.sendHeader("Cache-Control", "no-store");
    s.send(200, "text/html", p);
  });
  s.on("/oauth/finish", HTTP_POST, [&] {
    if (!webAuth(s, cfg) || !sameOrigin(s)) return;
    if (!cfg.webPass[0]) { s.send(403, "text/plain; charset=utf-8", "configura primero una contraseña"); return; }
    g_pendingCode = s.arg("code");
    oauthPending = true;
    s.sendHeader("Location", "/");
    s.send(303);
  });
  s.on("/token", HTTP_POST, [&] {
    if (!webAuth(s, cfg) || !sameOrigin(s)) return;
    if (!cfg.webPass[0]) { s.send(403, "text/plain; charset=utf-8", "configura primero una contraseña"); return; }
    Tokens& t = g_tokens;  // buffer compartido, ver declaracion en main.cpp
    loadTokens(t);
    if (s.arg("access").length()) {
      // Token nuevo: el refresh y la caducidad del anterior no le corresponden.
      // Si no se pega refresh, se queda sin el (y sin caducidad conocida).
      snprintf(t.access, sizeof t.access, "%s", s.arg("access").c_str());
      t.refresh[0] = 0;
      t.expiresAt = s.arg("exp").length() ? strtoul(s.arg("exp").c_str(), nullptr, 10) : 0;
    }
    if (s.arg("refresh").length()) snprintf(t.refresh, sizeof t.refresh, "%s", s.arg("refresh").c_str());
    saveTokens(t);
    authBad = false;  // token nuevo: da otra oportunidad antes de volver a marcarlo
    pollPedido = true;  // revalida el token nuevo ya mismo, no esperar al proximo ciclo
    s.sendHeader("Location", "/");
    s.send(303);
  });
}
