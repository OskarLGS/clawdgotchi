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
#include "safemode.h"

NetLog netlog;
ESP8266WebServer server(80);
QuotaSnapshot quotaNow;
Config cfg;
// Unico buffer de Tokens del firmware: runPoll() (aqui) y /token (webui.h)
// lo comparten via extern en vez de tener cada uno su propio `static Tokens`
// (~2.6KB cada uno); ambos lo recargan con loadTokens() antes de usarlo, asi
// que no hay estado cruzado entre llamadas que compartir el buffer rompa.
Tokens g_tokens;
// No static: webui.h necesita ponerlo a true desde /token y /cfg (extern).
bool pollPedido = false;
static uint32_t lastPollOk = 0;
// Modo seguro (3 crashes seguidos, ver safemode.h): solo web UI + /update,
// sin pantalla ni consultas a Claude. Leido tambien por webui.h (extern).
bool safeMode = false;
// true cuando pollQuota devuelve AUTH_FAILED y un intento de refresh no lo
// arregla. Se limpia con un poll OK o al guardar un token nuevo en /token
// (webui.h). Leido tambien por webui.h (extern) para avisar en la web UI.
bool authBad = false;

void setup() {
  // Primera linea, antes de tocar LittleFS: un FS corrupto que haga crashear
  // el propio LittleFS.begin()/loadConfig debe poder contarse como crash y
  // llevar a modo seguro, no solo los crashes de despues.
  bool safe = safeModeRequested();
  LittleFS.begin();
  loadConfig(cfg);
  if (safe) {
    safeMode = true;
    wifiConnectOrPortal(cfg);
    MDNS.begin("clawdgotchi");
    webuiRoutes(server, cfg, quotaNow);
    server.on("/log", [] {
      if (!webAuth(server, cfg)) return;
      server.send(200, "text/plain", netlog.text());
    });
    otaBegin(server, cfg);
    server.begin();
    MDNS.addService("http", "tcp", 80);
    LOGN("MODO SEGURO: 3 reinicios por fallo seguidos");
    return;  // sin display ni poll
  }
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
                " lastPollOk=" + lastPollOk + " authBad=" + authBad);
  });
  otaBegin(server, cfg);
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

// Planificador de poll: intervalo cfg.pollMinutes, con backoff exponencial
// 1,2,4,...,32 min mientras pollQuota devuelva NET_ERROR (se resetea a 1 en
// cuanto un poll sale OK). /poll fuerza un poll inmediato via pollPedido.
static void runPoll(uint32_t nowMs, bool ntpVisto) {
  static uint32_t nextPollMs = 0;
  static uint32_t backoffMin = 1;
  bool due = ntpVisto && (int32_t)(nowMs - nextPollMs) >= 0;
  if (!pollPedido && !due) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (!ntpVisto) {
    // Solo se llega aqui via /poll (pollPedido): un poll programado nunca
    // dispara sin NTP, ver "due" arriba. NADA de esperar bloqueando: loop()
    // ya comprueba time(nullptr) sin bloqueo en cada iteracion y nos pasara
    // ntpVisto=true en cuanto sincronice; aqui solo dejamos pollPedido
    // pendiente para ese momento y logueamos una vez (no en cada iteracion,
    // que con handleClient corriendo cada ~2ms inundaria el log).
    static bool logged = false;
    if (!logged) { LOGN("poll aplazado: sin NTP"); logged = true; }
    return;
  }
  pollPedido = false;

  Tokens& t = g_tokens;  // buffer compartido, ver declaracion arriba
  if (!loadTokens(t)) {
    LOGN("loadTokens fallo en loop");
    nextPollMs = nowMs + (uint32_t)cfg.pollMinutes * 60000UL;
    return;
  }

  if (t.expiresAt > 0 && t.refresh[0]) {
    int32_t remaining = (int32_t)(t.expiresAt - (uint32_t)time(nullptr));
    if (remaining < 300) {
      if (refreshTokens(t)) saveTokens(t);
    }
  }

  PollStatus st = pollQuota(t.access, quotaNow);
  if (st == PollStatus::AUTH_FAILED) {
    bool recovered = t.refresh[0] && refreshTokens(t) && saveTokens(t) &&
                      pollQuota(t.access, quotaNow) == PollStatus::OK;
    authBad = !recovered;
    if (recovered) st = PollStatus::OK;
  }
  LOGN("poll st=%d 5h=%u%% 7d=%u%%", (int)st, quotaNow.pct5h, quotaNow.pctWeek);

  if (st == PollStatus::OK) {
    lastPollOk = (uint32_t)time(nullptr);
    authBad = false;
    backoffMin = 1;
    nextPollMs = nowMs + (uint32_t)cfg.pollMinutes * 60000UL;
  } else if (st == PollStatus::NET_ERROR) {
    nextPollMs = nowMs + backoffMin * 60000UL;
    if (backoffMin < 32) backoffMin *= 2;
  } else {  // AUTH_FAILED persistente: no machacar, reintenta en el ciclo normal
    nextPollMs = nowMs + (uint32_t)cfg.pollMinutes * 60000UL;
  }
}

void loop() {
  server.handleClient();
  MDNS.update();
  if (safeMode) return;

  uint32_t nowMs = millis();

  // Una vez sincronizado NTP no hace falta volver a comprobarlo: evita el
  // time(nullptr) redundante en cada iteracion.
  static bool ntpVisto = false;
  if (!ntpVisto && time(nullptr) >= 1600000000) ntpVisto = true;

  runPoll(nowMs, ntpVisto);

  static bool healthMarked = false;
  if (!healthMarked && millis() > 60000) { markBootHealthy(); healthMarked = true; }

  static uint32_t lastFrameMs = 0;
  static uint8_t frame = 0;
  if (nowMs - lastFrameMs >= 500) {
    lastFrameMs = nowMs;
    frame ^= 1;

    // currentScreen: 0=tama 1=reloj. En screenMode 0 alterna segun
    // cfg.rotateSeconds; cfg se lee en vivo asi que /cfg surte efecto sin
    // reiniciar. Un unico flag detecta cualquier cambio (modo o rotacion) y
    // dispara el repintado completo solo al conmutar.
    int currentScreen;
    if (cfg.screenMode == 1) currentScreen = 0;
    else if (cfg.screenMode == 2) currentScreen = 1;
    else {
      uint32_t rotSec = cfg.rotateSeconds ? cfg.rotateSeconds : 15;
      currentScreen = (int)((nowMs / 1000 / rotSec) % 2);
    }
    static int lastScreen = -1;
    if (currentScreen != lastScreen) {
      lastScreen = currentScreen;
      tft.fillScreen(TFT_BLACK);
      clockInvalidate();
      tamaInvalidate();
    }

    time_t now = time(nullptr);
    if (currentScreen == 1) {
      struct tm lt;
      localtime_r(&now, &lt);
      drawClock(lt);
    } else {
      Mood m = moodFor(quotaNow, WiFi.status() == WL_CONNECTED && !authBad);
      drawTamagotchi(quotaNow, m, (uint32_t)now, frame);
    }

    static bool lastAuthBad = false;
    if (authBad) {
      tft.setTextDatum(TR_DATUM);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("!", 238, 2, 2);
      tft.setTextDatum(TL_DATUM);
    } else if (lastAuthBad) {
      tft.fillRect(220, 0, 20, 18, TFT_BLACK);  // borra el "!" al recuperarse
    }
    lastAuthBad = authBad;
  }
  delay(2);
}
