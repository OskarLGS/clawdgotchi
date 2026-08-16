# Clawdgotchi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware desde cero para el GeekMagic SmallTV Ultra que muestra un tamagotchi de Claude con las cuotas de uso (5 h / semanal) y un reloj, de forma 100 % autónoma.

**Architecture:** ESP8266 (ESP-12F, 4 MB) + ST7789 240×240. Lógica pura (parseo de cuotas, humor de la mascota, countdowns) en `lib/core/` con tests nativos en host; hardware (display, TLS, web UI, FS) en `src/`. Toda imagen flasheada incluye `/update` OTA desde el primer día, así el cubo nunca queda sin camino de vuelta.

**Tech Stack:** PlatformIO (`espressif8266`, board `esp12e`), Arduino core, TFT_eSPI, ArduinoJson 7, BearSSL (WiFiClientSecure), LittleFS, Unity (tests nativos).

**Spec:** `docs/superpowers/specs/2026-08-16-clawdgotchi-design.md`

## Global Constraints

- Board: `esp12e`, ldscript `eagle.flash.4m1m.ld` (1 MB LittleFS), `-D PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY`.
- Pines del panel (verificados de smalltv-mod/ESPHome/Tasmota): `SCLK=14, MOSI=13, DC=0, RST=2, CS=15, BL=5` — backlight **activo-bajo**, orden de color **RGB** (no BGR).
- Nunca un framebuffer completo (115 KB no caben): render por sprites parciales de TFT_eSPI.
- API de cuota: `POST https://api.anthropic.com/v1/messages` con headers `anthropic-version: 2023-06-01`, `anthropic-beta: oauth-2025-04-20`, `Authorization: Bearer <token>`, body `{"model":"claude-haiku-4-5-20251001","max_tokens":1,"messages":[{"role":"user","content":"hi"}]}`. Respuesta: cabeceras `anthropic-ratelimit-unified-{5h,7d}-utilization` (float 0..1), `-{5h,7d}-reset` (epoch), `-5h-status`.
- Refresh de token: `POST https://platform.claude.com/v1/oauth/token` form-encoded `grant_type=refresh_token&refresh_token=…`.
- Textos de la web UI en español. Puerto HTTP 80. mDNS: `clawdgotchi.local`.
- Cada tarea termina en commit. Mensajes de commit en español, cortos.
- El dispositivo del usuario: 192.168.1.114, stock v9.0.50. Las tareas 4 y 5 requieren al usuario presente (flasheo físico y unirse a un AP).

---

### Task 1: Scaffold del proyecto PlatformIO

**Files:**
- Create: `platformio.ini`, `.gitignore`, `src/main.cpp`, `src/board.h`
- Test: (build = test en esta tarea)

**Interfaces:**
- Produces: entornos `ultra` (firmware) y `native` (tests host); `src/board.h` con los defines de pines que usan las tareas 10–12.

- [ ] **Step 1: Comprobar/instalar PlatformIO**

Run: `command -v pio || pip3 install --user platformio`
Expected: `pio --version` imprime una versión 6.x.

- [ ] **Step 2: Escribir `platformio.ini`**

```ini
[platformio]
default_envs = ultra

[env:ultra]
platform = espressif8266
board = esp12e
framework = arduino
board_build.filesystem = littlefs
board_build.ldscript = eagle.flash.4m1m.ld
monitor_speed = 115200
build_flags =
    -D PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY
    -D ARDUINOJSON_USE_DOUBLE=0
    -D USER_SETUP_LOADED
    -D ST7789_DRIVER
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=240
    -D TFT_SCLK=14
    -D TFT_MOSI=13
    -D TFT_DC=0
    -D TFT_RST=2
    -D TFT_CS=15
    -D SPI_FREQUENCY=40000000
lib_deps =
    bblanchon/ArduinoJson @ ^7.2.0
    bodmer/TFT_eSPI @ ^2.5.43

[env:native]
platform = native
test_framework = unity
build_flags = -std=c++17
lib_deps = bblanchon/ArduinoJson @ ^7.2.0
```

- [ ] **Step 3: Escribir `src/board.h`**

```cpp
// board.h — GeekMagic SmallTV Ultra (ESP-12F + ST7789 240x240)
#pragma once
#define PIN_TFT_BL 5           // backlight, PWM, ACTIVO-BAJO
#define BL_ON_DUTY(pct) (1023 - ((pct) * 1023 / 100))  // 100% brillo -> duty 0
```

(El resto de pines del TFT van por build_flags para TFT_eSPI.)

- [ ] **Step 4: `src/main.cpp` mínimo**

```cpp
#include <Arduino.h>
void setup() {}
void loop() { delay(100); }
```

- [ ] **Step 5: Verificar que compila**

Run: `pio run -e ultra`
Expected: `SUCCESS`, tamaño de firmware impreso.

- [ ] **Step 6: `.gitignore` y commit**

```
.pio/
.vscode/
```

```bash
git add -A && git commit -m "scaffold PlatformIO: envs ultra y native"
```

---

### Task 2: Módulo `core` — cuotas, humor y countdowns (TDD nativo)

**Files:**
- Create: `lib/core/quota.h`, `lib/core/quota.cpp`
- Test: `test/test_native/test_quota.cpp`

**Interfaces:**
- Produces (lo usan las tareas 7, 12, 13):

```cpp
struct QuotaSnapshot {
  uint8_t pct5h; uint8_t pctWeek;          // 0..100 (clampado)
  uint32_t reset5h; uint32_t resetWeek;    // epoch seconds
  bool exhausted;                          // status "rejected" o pct>=100
  bool valid;                              // parseo correcto
};
enum class Mood { HAPPY, UNEASY, STRESSED, PANIC, SLEEPING, OFFLINE };
bool parseRateHeaders(const char* util5h, const char* reset5h,
                      const char* util7d, const char* reset7d,
                      const char* status5h, QuotaSnapshot& out);
Mood moodFor(const QuotaSnapshot& q, bool online);
void formatCountdown(uint32_t nowEpoch, uint32_t resetEpoch, char* buf, size_t n); // "3h 12m", "2d 5h", "0m"
```

- [ ] **Step 1: Escribir los tests que fallan** (`test/test_native/test_quota.cpp`)

```cpp
#include <unity.h>
#include "quota.h"

void test_parse_ok() {
  QuotaSnapshot q;
  TEST_ASSERT_TRUE(parseRateHeaders("0.42", "1755350000", "0.07", "1755600000", "allowed", q));
  TEST_ASSERT_EQUAL(42, q.pct5h);
  TEST_ASSERT_EQUAL(7, q.pctWeek);
  TEST_ASSERT_EQUAL_UINT32(1755350000u, q.reset5h);
  TEST_ASSERT_FALSE(q.exhausted);
  TEST_ASSERT_TRUE(q.valid);
}
void test_parse_garbage_fails() {
  QuotaSnapshot q;
  TEST_ASSERT_FALSE(parseRateHeaders("banana", "0", "0.1", "0", "allowed", q));
  TEST_ASSERT_FALSE(q.valid);
}
void test_parse_clamps_and_flags_exhausted() {
  QuotaSnapshot q;
  parseRateHeaders("1.30", "1", "0.5", "1", "allowed", q);
  TEST_ASSERT_EQUAL(100, q.pct5h);
  TEST_ASSERT_TRUE(q.exhausted);
  parseRateHeaders("0.5", "1", "0.5", "1", "rejected", q);
  TEST_ASSERT_TRUE(q.exhausted);
}
void test_moods() {
  QuotaSnapshot q{};
  q.valid = true;
  q.pct5h = 10; q.pctWeek = 20;
  TEST_ASSERT_EQUAL((int)Mood::HAPPY, (int)moodFor(q, true));
  q.pct5h = 50;  TEST_ASSERT_EQUAL((int)Mood::UNEASY,   (int)moodFor(q, true));
  q.pct5h = 75;  TEST_ASSERT_EQUAL((int)Mood::STRESSED, (int)moodFor(q, true));
  q.pct5h = 90;  TEST_ASSERT_EQUAL((int)Mood::PANIC,    (int)moodFor(q, true));
  q.pct5h = 30; q.pctWeek = 91; // manda la peor cuota
  TEST_ASSERT_EQUAL((int)Mood::PANIC, (int)moodFor(q, true));
  q.exhausted = true;
  TEST_ASSERT_EQUAL((int)Mood::SLEEPING, (int)moodFor(q, true));
  TEST_ASSERT_EQUAL((int)Mood::OFFLINE, (int)moodFor(q, false));
  QuotaSnapshot inv{}; // sin datos válidos -> OFFLINE aunque haya red
  TEST_ASSERT_EQUAL((int)Mood::OFFLINE, (int)moodFor(inv, true));
}
void test_countdown_format() {
  char b[16];
  formatCountdown(1000, 1000 + 3*3600 + 12*60, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("3h 12m", b);
  formatCountdown(1000, 1000 + 2*86400 + 5*3600, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("2d 5h", b);
  formatCountdown(2000, 1000, b, sizeof b);   // reset en el pasado
  TEST_ASSERT_EQUAL_STRING("0m", b);
  formatCountdown(1000, 1000 + 59, b, sizeof b);
  TEST_ASSERT_EQUAL_STRING("0m", b);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_ok);
  RUN_TEST(test_parse_garbage_fails);
  RUN_TEST(test_parse_clamps_and_flags_exhausted);
  RUN_TEST(test_moods);
  RUN_TEST(test_countdown_format);
  return UNITY_END();
}
```

- [ ] **Step 2: Verificar que fallan**

Run: `pio test -e native`
Expected: FAIL (quota.h no existe).

- [ ] **Step 3: Implementar `lib/core/quota.h` + `quota.cpp`**

```cpp
// quota.h
#pragma once
#include <stdint.h>
#include <stddef.h>
struct QuotaSnapshot {
  uint8_t pct5h = 0, pctWeek = 0;
  uint32_t reset5h = 0, resetWeek = 0;
  bool exhausted = false, valid = false;
};
enum class Mood { HAPPY, UNEASY, STRESSED, PANIC, SLEEPING, OFFLINE };
bool parseRateHeaders(const char* util5h, const char* reset5h,
                      const char* util7d, const char* reset7d,
                      const char* status5h, QuotaSnapshot& out);
Mood moodFor(const QuotaSnapshot& q, bool online);
void formatCountdown(uint32_t nowEpoch, uint32_t resetEpoch, char* buf, size_t n);
```

```cpp
// quota.cpp
#include "quota.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static bool parsePct(const char* s, uint8_t& outPct) {
  if (!s || !*s) return false;
  char* end = nullptr;
  double v = strtod(s, &end);
  if (end == s || v < 0) return false;
  double pct = round(v * 100.0);
  outPct = (uint8_t)(pct > 100 ? 100 : pct);
  return true;
}
static uint32_t parseEpoch(const char* s) {
  if (!s) return 0;
  return (uint32_t)strtoul(s, nullptr, 10);
}
bool parseRateHeaders(const char* u5, const char* r5, const char* u7,
                      const char* r7, const char* st, QuotaSnapshot& out) {
  out = QuotaSnapshot{};
  uint8_t p5, p7;
  if (!parsePct(u5, p5) || !parsePct(u7, p7)) return false;
  out.pct5h = p5; out.pctWeek = p7;
  out.reset5h = parseEpoch(r5); out.resetWeek = parseEpoch(r7);
  double raw5 = strtod(u5, nullptr), raw7 = strtod(u7, nullptr);
  out.exhausted = raw5 >= 1.0 || raw7 >= 1.0 ||
                  (st && strcmp(st, "rejected") == 0);
  out.valid = true;
  return true;
}
Mood moodFor(const QuotaSnapshot& q, bool online) {
  if (!online || !q.valid) return Mood::OFFLINE;
  if (q.exhausted) return Mood::SLEEPING;
  uint8_t worst = q.pct5h > q.pctWeek ? q.pct5h : q.pctWeek;
  if (worst >= 90) return Mood::PANIC;
  if (worst >= 75) return Mood::STRESSED;
  if (worst >= 50) return Mood::UNEASY;
  return Mood::HAPPY;
}
void formatCountdown(uint32_t now, uint32_t reset, char* buf, size_t n) {
  uint32_t s = reset > now ? reset - now : 0;
  uint32_t d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
  if (d > 0)      snprintf(buf, n, "%ud %uh", d, h);
  else if (h > 0) snprintf(buf, n, "%uh %um", h, m);
  else            snprintf(buf, n, "%um", m);
}
```

- [ ] **Step 4: Verificar que pasan**

Run: `pio test -e native`
Expected: 5 tests PASS. También `pio run -e ultra` sigue compilando.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "core: parseo de cuotas, humor y countdowns con tests nativos"
```

---

### Task 3: Firmware base "siempre recuperable" (WiFi + OTA + log web)

**Files:**
- Create: `src/netlog.h`, `src/ota.h`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `LOGN(fmt, ...)` (macro de log a ring buffer visible en `/log`); servidor `ESP8266WebServer server(80)` global declarado en `main.cpp` (`extern ESP8266WebServer server;`) al que las tareas 5, 8 y 9 añaden rutas; `/update` OTA permanente.

En esta fase el WiFi va con credenciales de compilación (`-D CLAWD_WIFI_SSID/PASS`); la tarea 8 lo sustituye por el portal cautivo.

- [ ] **Step 1: `src/netlog.h` — ring buffer de log**

```cpp
#pragma once
#include <Arduino.h>
// Log en RAM visible por web: en el Ultra no hay chip USB-serial.
class NetLog {
  static const size_t CAP = 3000;
  String buf;
public:
  void line(const String& s) {
    buf += s; buf += '\n';
    if (buf.length() > CAP) buf.remove(0, buf.length() - CAP);
  }
  const String& text() const { return buf; }
};
extern NetLog netlog;
#define LOGN(...) do { char _b[128]; snprintf(_b, sizeof _b, __VA_ARGS__); \
                       netlog.line(String(millis()/1000) + "s " + _b); } while (0)
```

- [ ] **Step 2: `src/ota.h`**

```cpp
#pragma once
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
// /update acepta un firmware.bin nuevo: la puerta de salida permanente.
inline ESP8266HTTPUpdateServer otaUpdater;
inline void otaBegin(ESP8266WebServer& server) { otaUpdater.setup(&server, "/update"); }
```

- [ ] **Step 3: `src/main.cpp` — WiFi + servidor + mDNS**

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "netlog.h"
#include "ota.h"

#ifndef CLAWD_WIFI_SSID
#define CLAWD_WIFI_SSID ""
#define CLAWD_WIFI_PASS ""
#endif

NetLog netlog;
ESP8266WebServer server(80);

void setup() {
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
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  server.handleClient();
  MDNS.update();
  delay(2);
}
```

- [ ] **Step 4: Compilar con las credenciales del usuario**

Run: `pio run -e ultra` con `PLATFORMIO_BUILD_FLAGS="-D CLAWD_WIFI_SSID='\"<SSID>\"' -D CLAWD_WIFI_PASS='\"<PASS>\"'"` (pedir SSID/pass al usuario en este punto).
Expected: SUCCESS. Anotar el tamaño del binario (debe quedar muy por debajo de ~1 MB).

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "firmware base: wifi, mDNS, log web y OTA /update"
```

---

### Task 4: Instalación en el cubo ⚠ REQUIERE AL USUARIO

Sin tests automatizables: es el flasheo físico. Checklist manual.

**Files:**
- Create: `docs/instalacion.md` (los pasos de abajo, como referencia permanente)

- [ ] **Step 1: Red de seguridad — descargar el stock v9.0.50**

Run: bajar `firmware.bin` de `https://github.com/GeekMagicClock/smalltv-ultra/tree/main/Ultra-V9.0.50` a `backup/stock-9.0.50.bin` (fuera de git: añadir `backup/` a `.gitignore`). Verificar tamaño > 0.
Recuperación futura: subir ese bin por el `/update` de cualquier firmware nuestro.

- [ ] **Step 2: Descargar el loader de smalltv-mod**

De `https://github.com/giovi321/smalltv-mod/releases` (asset `smalltv-mod-loader.bin`, licencia WTFPL). Guardar en `backup/`.

- [ ] **Step 3: Flashear el loader vía el updater stock**

Navegador → `http://192.168.1.114/update` → subir `smalltv-mod-loader.bin`. El cubo se reinicia y abre el AP abierto `SmallTV-Loader`.

- [ ] **Step 4: Flashear clawdgotchi vía el loader** (usuario: unirse al AP)

Unirse a `SmallTV-Loader` → `http://192.168.4.1/update` → subir `.pio/build/ultra/firmware.bin`.

- [ ] **Step 5: Verificar**

De vuelta en la WiFi de casa: `curl http://clawdgotchi.local/` responde con heap y uptime; `/log` muestra `wifi: ok`. Si no aparece, buscar la IP nueva en el router.

- [ ] **Step 6: Commit de `docs/instalacion.md`**

```bash
git add -A && git commit -m "docs: instalación y recuperación"
```

---

### Task 5: Spike TLS/OAuth en hardware ⚠ REQUIERE AL USUARIO (checkpoint decisivo)

Valida la única incógnita real: TLS contra `api.anthropic.com` en 40 KB de heap. Si esto no pasa, el proyecto se replantea aquí (no después).

**Files:**
- Create: `src/certs.h`, `src/spike.h`
- Modify: `src/main.cpp` (añadir `spikeRoutes(server)`)

**Interfaces:**
- Produces: `src/certs.h` con las CA raíz pineadas (lo reutiliza la tarea 7); resultado medido del spike (MFLN sí/no, tamaño de buffers, heap mínimo) que parametriza la tarea 7.

- [ ] **Step 1: Capturar las CA raíz reales**

```bash
for h in api.anthropic.com platform.claude.com; do
  openssl s_client -connect $h:443 -showcerts </dev/null 2>/dev/null \
    | awk '/BEGIN CERT/{n++} n>=2' | tail -40
done
```

Identificar el emisor raíz de cada cadena (esperado: ISRG Root X1 o Google Trust Services). Descargar el PEM oficial de cada raíz y pegarlos en `src/certs.h`:

```cpp
#pragma once
// Raíces pineadas para api.anthropic.com y platform.claude.com.
// ponytail: si Anthropic rota de CA, esto rompe el poll; el fix es re-ejecutar
// la captura de arriba y re-flashear por OTA.
static const char CA_ROOTS[] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
...pegar aquí la(s) raíz(ces) reales...
-----END CERTIFICATE-----
)CERT";
```

- [ ] **Step 2: `src/spike.h` — endpoint de prueba**

```cpp
#pragma once
#include <ESP8266WebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include "certs.h"
#include "netlog.h"

// GET /spike?token=<token de `claude setup-token`>
// Prueba: NTP -> MFLN probe -> TLS -> POST minimo -> cabeceras de cuota.
inline void spikeRoutes(ESP8266WebServer& server) {
  server.on("/spike", [&server] {
    String token = server.arg("token");
    if (token.isEmpty()) { server.send(400, "text/plain", "falta ?token="); return; }
    configTime(0, 0, "pool.ntp.org");
    time_t now = 0;
    for (int i = 0; i < 40 && (now = time(nullptr)) < 1600000000; i++) delay(250);
    LOGN("ntp: %ld", (long)now);

    bool mfln = BearSSL::WiFiClientSecure::probeMaxFragmentLength("api.anthropic.com", 443, 1024);
    LOGN("mfln1024=%d maxblock=%u", mfln, ESP.getMaxFreeBlockSize());

    BearSSL::WiFiClientSecure client;
    BearSSL::X509List roots(CA_ROOTS);
    client.setTrustAnchors(&roots);
    if (mfln) client.setBufferSizes(1024, 512); else client.setBufferSizes(16384, 512);

    HTTPClient http;
    http.begin(client, "https://api.anthropic.com/v1/messages");
    http.addHeader("anthropic-version", "2023-06-01");
    http.addHeader("anthropic-beta", "oauth-2025-04-20");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + token);
    const char* keep[] = {"anthropic-ratelimit-unified-5h-utilization",
                          "anthropic-ratelimit-unified-5h-reset",
                          "anthropic-ratelimit-unified-7d-utilization",
                          "anthropic-ratelimit-unified-7d-reset",
                          "anthropic-ratelimit-unified-5h-status"};
    http.collectHeaders(keep, 5);
    uint32_t before = ESP.getMaxFreeBlockSize();
    int code = http.POST("{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,"
                         "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}");
    String out = "code=" + String(code) + " maxblock antes=" + String(before) +
                 " despues=" + String(ESP.getMaxFreeBlockSize()) + "\n";
    for (int i = 0; i < http.headers(); i++)
      out += http.headerName(i) + ": " + http.header(i) + "\n";
    http.end();
    LOGN("spike code=%d", code);
    server.send(200, "text/plain", out);
  });
}
```

En `main.cpp`, tras `otaBegin(server);` añadir `spikeRoutes(server);` con `#include "spike.h"`.

- [ ] **Step 3: Flashear por OTA y ejecutar**

Run: `pio run -e ultra` → subir `firmware.bin` a `http://clawdgotchi.local/update`.
Usuario: ejecutar `claude setup-token` en su Mac y visitar `http://clawdgotchi.local/spike?token=<token>`.
Expected: `code=200` y las 5 cabeceras con valores. Anotar `mfln` y los `maxblock`.

- [ ] **Step 4: Decisión del checkpoint**

- `code=200` → parámetros TLS validados; seguir.
- Falla por heap (`code=-1` y maxblock < ~17 KB sin MFLN) → probar liberar más heap (sin display aún debería sobrar). Si aun así no entra, PARAR y volver al usuario con las opciones medidas.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "spike TLS validado contra api.anthropic.com (resultado en docs)"
```

Documentar en `docs/instalacion.md` una línea con los números medidos.

---

### Task 6: Config y tokens en LittleFS

**Files:**
- Create: `lib/core/config_codec.h`, `lib/core/config_codec.cpp`, `src/store.h`
- Test: `test/test_native/test_config.cpp`

**Interfaces:**
- Produces (lo usan 7, 8, 9, 14):

```cpp
struct Config {
  char wifiSsid[33] = "", wifiPass[65] = "";
  char tz[48] = "CET-1CEST,M3.5.0,M10.5.0/3";   // POSIX TZ, por defecto España peninsular
  uint16_t pollMinutes = 5;
  uint8_t brightness = 80;                       // 0..100
  uint8_t screenMode = 0;                        // 0=rotación 1=tamagotchi 2=reloj
  uint16_t rotateSeconds = 15;
  char webPass[33] = "";                         // "" = sin password (hasta guardar token)
};
struct Tokens { char access[2048] = ""; char refresh[512] = ""; uint32_t expiresAt = 0; };
// pure (native-testable): JSON <-> struct
bool configToJson(const Config&, char* out, size_t n);
bool configFromJson(const char* json, Config& out);
bool tokensToJson(const Tokens&, char* out, size_t n);
bool tokensFromJson(const char* json, Tokens& out);
// device-only (store.h): loadConfig/saveConfig/loadTokens/saveTokens sobre LittleFS
```

- [ ] **Step 1: Test que falla** (`test/test_native/test_config.cpp`)

```cpp
#include <unity.h>
#include "config_codec.h"

void test_config_roundtrip() {
  Config c;
  strcpy(c.wifiSsid, "MiRed"); c.pollMinutes = 10; c.brightness = 55;
  char buf[768];
  TEST_ASSERT_TRUE(configToJson(c, buf, sizeof buf));
  Config d;
  TEST_ASSERT_TRUE(configFromJson(buf, d));
  TEST_ASSERT_EQUAL_STRING("MiRed", d.wifiSsid);
  TEST_ASSERT_EQUAL(10, d.pollMinutes);
  TEST_ASSERT_EQUAL(55, d.brightness);
}
void test_config_bad_json() {
  Config d;
  TEST_ASSERT_FALSE(configFromJson("{noesjson", d));
}
void test_config_defaults_on_missing_fields() {
  Config d;
  TEST_ASSERT_TRUE(configFromJson("{}", d));
  TEST_ASSERT_EQUAL(5, d.pollMinutes);   // defaults sobreviven
}
void test_tokens_roundtrip() {
  Tokens t; strcpy(t.access, "at"); strcpy(t.refresh, "rt"); t.expiresAt = 123;
  char buf[3072];
  TEST_ASSERT_TRUE(tokensToJson(t, buf, sizeof buf));
  Tokens u;
  TEST_ASSERT_TRUE(tokensFromJson(buf, u));
  TEST_ASSERT_EQUAL_STRING("rt", u.refresh);
  TEST_ASSERT_EQUAL_UINT32(123, u.expiresAt);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_config_roundtrip);
  RUN_TEST(test_config_bad_json);
  RUN_TEST(test_config_defaults_on_missing_fields);
  RUN_TEST(test_tokens_roundtrip);
  return UNITY_END();
}
```

Nota: Unity compila cada carpeta `test_*` como binario aparte; mover el `main` de la tarea 2 no hace falta — cada test tiene el suyo.

- [ ] **Step 2: Verificar FAIL** — `pio test -e native`

- [ ] **Step 3: Implementar `config_codec`** con ArduinoJson:

```cpp
// config_codec.cpp (extracto del patrón; aplicar a los 4 métodos)
#include "config_codec.h"
#include <ArduinoJson.h>
static void cp(char* dst, size_t n, const char* src) { snprintf(dst, n, "%s", src ? src : ""); }
bool configFromJson(const char* json, Config& out) {
  JsonDocument d;
  if (deserializeJson(d, json)) return false;
  out = Config{};
  if (d["ssid"].is<const char*>()) cp(out.wifiSsid, sizeof out.wifiSsid, d["ssid"]);
  if (d["pass"].is<const char*>()) cp(out.wifiPass, sizeof out.wifiPass, d["pass"]);
  if (d["tz"].is<const char*>())   cp(out.tz, sizeof out.tz, d["tz"]);
  out.pollMinutes  = d["poll"] | out.pollMinutes;
  out.brightness   = d["brt"]  | out.brightness;
  out.screenMode   = d["mode"] | out.screenMode;
  out.rotateSeconds= d["rot"]  | out.rotateSeconds;
  if (d["webpass"].is<const char*>()) cp(out.webPass, sizeof out.webPass, d["webpass"]);
  return true;
}
bool configToJson(const Config& c, char* out, size_t n) {
  JsonDocument d;
  d["ssid"] = c.wifiSsid; d["pass"] = c.wifiPass; d["tz"] = c.tz;
  d["poll"] = c.pollMinutes; d["brt"] = c.brightness; d["mode"] = c.screenMode;
  d["rot"] = c.rotateSeconds; d["webpass"] = c.webPass;
  return serializeJson(d, out, n) > 0;
}
// tokensToJson/tokensFromJson: mismo patrón con claves "access","refresh","exp".
```

`src/store.h` (solo dispositivo):

```cpp
#pragma once
#include <LittleFS.h>
#include "config_codec.h"
inline bool loadFile(const char* path, char* buf, size_t n) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t r = f.readBytes(buf, n - 1); buf[r] = 0; f.close();
  return r > 0;
}
inline bool saveFile(const char* path, const char* data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(data); f.close();
  return true;
}
inline bool loadConfig(Config& c)  { char b[768];  return loadFile("/config.json", b, sizeof b) && configFromJson(b, c); }
inline bool saveConfig(const Config& c) { char b[768];  return configToJson(c, b, sizeof b) && saveFile("/config.json", b); }
inline bool loadTokens(Tokens& t)  { char b[3072]; return loadFile("/tokens.json", b, sizeof b) && tokensFromJson(b, t); }
inline bool saveTokens(const Tokens& t) { char b[3072]; return tokensToJson(t, b, sizeof b) && saveFile("/tokens.json", b); }
```

En `main.cpp` `setup()`: `LittleFS.begin();` al principio.

- [ ] **Step 4: Verificar PASS** — `pio test -e native` (9 tests) y `pio run -e ultra`.

- [ ] **Step 5: Commit** — `git commit -m "config y tokens en LittleFS con codec testeado"`

---

### Task 7: Cliente Claude en el dispositivo (poll + refresh)

**Files:**
- Create: `src/claude_client.h`, `src/claude_client.cpp`
- Modify: `src/main.cpp` (ruta de debug `/poll`)

**Interfaces:**
- Consumes: `certs.h` (task 5), `QuotaSnapshot`/`parseRateHeaders` (task 2), `Tokens` (task 6).
- Produces (lo usa la task 14): 

```cpp
enum class PollStatus { OK, AUTH_FAILED, NET_ERROR };
PollStatus pollQuota(const char* accessToken, QuotaSnapshot& out);
bool refreshTokens(Tokens& t);  // true si renovó y hay que persistir
```

- [ ] **Step 1: Implementar `claude_client.cpp`**

Misma estructura que el spike (que queda como referencia), con los buffers que el spike validó:

```cpp
#include "claude_client.h"
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "certs.h"
#include "netlog.h"

// Ajustar según el resultado del spike (Task 5): si MFLN 1024 fue aceptado,
// {1024, 512}; si no, {16384, 512}.
static const int RX_BUF = 1024, TX_BUF = 512;

static void tlsClient(BearSSL::WiFiClientSecure& c, BearSSL::X509List& roots) {
  c.setTrustAnchors(&roots);
  c.setBufferSizes(RX_BUF, TX_BUF);
}

PollStatus pollQuota(const char* accessToken, QuotaSnapshot& out) {
  BearSSL::WiFiClientSecure client;
  BearSSL::X509List roots(CA_ROOTS);
  tlsClient(client, roots);
  HTTPClient http;
  http.begin(client, "https://api.anthropic.com/v1/messages");
  http.addHeader("anthropic-version", "2023-06-01");
  http.addHeader("anthropic-beta", "oauth-2025-04-20");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + accessToken);
  const char* keep[] = {"anthropic-ratelimit-unified-5h-utilization",
                        "anthropic-ratelimit-unified-5h-reset",
                        "anthropic-ratelimit-unified-7d-utilization",
                        "anthropic-ratelimit-unified-7d-reset",
                        "anthropic-ratelimit-unified-5h-status"};
  http.collectHeaders(keep, 5);
  int code = http.POST("{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,"
                       "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}");
  PollStatus st;
  if (code == 200 || code == 429) {   // 429 = cuota agotada: las cabeceras vienen igual
    bool ok = parseRateHeaders(http.header(keep[0]).c_str(), http.header(keep[1]).c_str(),
                               http.header(keep[2]).c_str(), http.header(keep[3]).c_str(),
                               http.header(keep[4]).c_str(), out);
    st = ok ? PollStatus::OK : PollStatus::NET_ERROR;
  } else if (code == 401 || code == 403) st = PollStatus::AUTH_FAILED;
  else st = PollStatus::NET_ERROR;
  LOGN("poll code=%d 5h=%u%% 7d=%u%%", code, out.pct5h, out.pctWeek);
  http.end();
  return st;
}

bool refreshTokens(Tokens& t) {
  if (!t.refresh[0]) return false;
  BearSSL::WiFiClientSecure client;
  BearSSL::X509List roots(CA_ROOTS);
  tlsClient(client, roots);
  HTTPClient http;
  http.begin(client, "https://platform.claude.com/v1/oauth/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(String("grant_type=refresh_token&refresh_token=") + t.refresh);
  if (code != 200) { LOGN("refresh code=%d", code); http.end(); return false; }
  JsonDocument d;
  DeserializationError e = deserializeJson(d, http.getStream());
  http.end();
  if (e || !d["access_token"].is<const char*>()) return false;
  snprintf(t.access, sizeof t.access, "%s", (const char*)d["access_token"]);
  if (d["refresh_token"].is<const char*>())
    snprintf(t.refresh, sizeof t.refresh, "%s", (const char*)d["refresh_token"]);
  t.expiresAt = (uint32_t)time(nullptr) + (d["expires_in"] | 28800);
  LOGN("token renovado");
  return true;
}
```

- [ ] **Step 2: Ruta de debug en `main.cpp`**

```cpp
server.on("/poll", [] {
  Tokens t;
  if (!loadTokens(t)) { server.send(400, "text/plain", "sin token guardado"); return; }
  QuotaSnapshot q;
  PollStatus st = pollQuota(t.access, q);
  server.send(200, "text/plain", String("st=") + (int)st + " 5h=" + q.pct5h +
              "% 7d=" + q.pctWeek + "% exhausted=" + q.exhausted);
});
```

Guardar el token del spike a mano una vez: ruta temporal `POST /token` que hace `saveTokens` con `server.arg("access")` y `server.arg("refresh")` (la task 9 la sustituye por el formulario).

- [ ] **Step 3: Verificar en hardware**

OTA flash → `curl -X POST 'http://clawdgotchi.local/token' --data-urlencode 'access=<token>'` → `curl http://clawdgotchi.local/poll`.
Expected: `st=0` con porcentajes reales.

- [ ] **Step 4: Commit** — `git commit -m "cliente claude: poll de cuota y refresh de token"`

---

### Task 8: Portal cautivo de primer arranque

**Files:**
- Create: `src/wifi_portal.h`
- Modify: `src/main.cpp` (usar Config para WiFi; quitar CLAWD_WIFI_* de build)

**Interfaces:**
- Consumes: `Config`, `loadConfig`/`saveConfig` (task 6).
- Produces: `bool wifiConnectOrPortal(Config& cfg)` — conecta como STA con `cfg`; si no hay SSID o falla 30 s, abre AP `Clawdgotchi-Setup` con portal cautivo, guarda lo elegido y reinicia. Bloqueante hasta tener red.

- [ ] **Step 1: Implementar `src/wifi_portal.h`**

```cpp
#pragma once
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "store.h"
#include "netlog.h"

static const char PORTAL_HTML[] PROGMEM = R"(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clawdgotchi</title>
<body style="font-family:sans-serif;background:#222;color:#eee;padding:2em">
<h2>Clawdgotchi &mdash; WiFi</h2>
<form method=POST action=/save>
Red: <select name=ssid>%OPTS%</select><br><br>
Clave: <input type=password name=pass><br><br>
<input type=submit value=Guardar></form>)";

inline bool wifiConnectOrPortal(Config& cfg) {
  if (cfg.wifiSsid[0]) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
    for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) delay(500);
    if (WiFi.status() == WL_CONNECTED) { LOGN("wifi ok %s", WiFi.localIP().toString().c_str()); return true; }
    LOGN("wifi fallo, abriendo portal");
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Clawdgotchi-Setup");
  DNSServer dns;
  dns.start(53, "*", WiFi.softAPIP());          // portal cautivo
  ESP8266WebServer portal(80);
  int n = WiFi.scanNetworks();
  String opts;
  for (int i = 0; i < n; i++)
    opts += "<option>" + WiFi.SSID(i) + "</option>";
  portal.onNotFound([&] {
    String page = FPSTR(PORTAL_HTML);
    page.replace("%OPTS%", opts);
    portal.send(200, "text/html", page);
  });
  portal.on("/save", HTTP_POST, [&] {
    snprintf(cfg.wifiSsid, sizeof cfg.wifiSsid, "%s", portal.arg("ssid").c_str());
    snprintf(cfg.wifiPass, sizeof cfg.wifiPass, "%s", portal.arg("pass").c_str());
    saveConfig(cfg);
    portal.send(200, "text/html", "Guardado. Reiniciando...");
    delay(1000);
    ESP.restart();
  });
  portal.begin();
  while (true) { dns.processNextRequest(); portal.handleClient(); delay(2); }  // sale por restart
}
```

- [ ] **Step 2: Integrar en `main.cpp`**

`setup()`: `LittleFS.begin(); Config cfg; loadConfig(cfg); wifiConnectOrPortal(cfg);` — y eliminar el bloque `CLAWD_WIFI_*`. `cfg` pasa a ser global (`Config cfg;`).

- [ ] **Step 3: Verificar en hardware**

Flashear OTA. Con `/config.json` aún sin SSID (LittleFS recién formateado), el cubo abre `Clawdgotchi-Setup` (usuario: unirse, elegir red, guardar). Tras reiniciar aparece en `clawdgotchi.local`.
Expected: `/log` muestra `wifi ok`.

- [ ] **Step 4: Commit** — `git commit -m "portal cautivo de configuración wifi"`

---

### Task 9: Web UI de ajustes (con password) + guardado de token

**Files:**
- Create: `src/webui.h`
- Modify: `src/main.cpp` (montar rutas; retirar `/token` temporal y `/spike`)

**Interfaces:**
- Consumes: `Config`, `Tokens`, store (6); `server` global (3).
- Produces: `void webuiRoutes(ESP8266WebServer&, Config&)` — página `/` con formularios; `bool webAuth(ESP8266WebServer&, const Config&)` que exige HTTP Basic si `webPass` no está vacío (lo reutiliza también `/update` y `/poll`).

- [ ] **Step 1: Implementar `src/webui.h`**

```cpp
#pragma once
#include <ESP8266WebServer.h>
#include "store.h"

static const char SETTINGS_HTML[] PROGMEM = R"(<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clawdgotchi</title>
<body style="font-family:sans-serif;background:#222;color:#eee;max-width:34em;margin:2em auto">
<h2>&#129408; Clawdgotchi</h2>
<p>5h: %P5%%% &middot; semana: %P7%%% &middot; heap %HEAP%</p>
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
Password web UI: <input name=webpass value="%WP%"><br><br>
<input type=submit value=Guardar>
</form>
<form method=POST action=/token>
<h3>Token de Claude</h3>
<p>En tu ordenador: <code>claude setup-token</code> y pega aqu&iacute; el resultado.
O pega un refresh token para renovaci&oacute;n autom&aacute;tica.</p>
Access: <input name=access size=40><br><br>
Refresh (opcional): <input name=refresh size=40><br><br>
<input type=submit value="Guardar token">
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
    p.replace("%TZ%", cfg.tz); p.replace("%POLL%", String(cfg.pollMinutes));
    p.replace("%WP%", cfg.webPass);
    s.send(200, "text/html", p);
  });
  s.on("/cfg", HTTP_POST, [&] {
    if (!webAuth(s, cfg)) return;
    cfg.screenMode = s.arg("mode").toInt();
    cfg.rotateSeconds = s.arg("rot").toInt();
    cfg.brightness = s.arg("brt").toInt();
    snprintf(cfg.tz, sizeof cfg.tz, "%s", s.arg("tz").c_str());
    cfg.pollMinutes = s.arg("poll").toInt();
    snprintf(cfg.webPass, sizeof cfg.webPass, "%s", s.arg("webpass").c_str());
    saveConfig(cfg);
    s.sendHeader("Location", "/");
    s.send(303);
  });
  s.on("/token", HTTP_POST, [&] {
    if (!webAuth(s, cfg)) return;
    Tokens t;
    loadTokens(t);
    if (s.arg("access").length())  snprintf(t.access, sizeof t.access, "%s", s.arg("access").c_str());
    if (s.arg("refresh").length()) snprintf(t.refresh, sizeof t.refresh, "%s", s.arg("refresh").c_str());
    saveTokens(t);
    s.sendHeader("Location", "/");
    s.send(303);
  });
}
```

- [ ] **Step 2: Integrar y proteger rutas existentes**

En `main.cpp`: quitar `spikeRoutes`, quitar el `/token` temporal y el `/` de la task 3; montar `webuiRoutes(server, cfg, quotaNow)`; envolver `/log` y `/poll` con `if (!webAuth(server, cfg)) return;`. El `/update` de `ESP8266HTTPUpdateServer` acepta credenciales: cambiar `otaBegin` a `otaUpdater.setup(&server, "/update", "admin", cfg.webPass)` cuando `webPass` no esté vacío.

- [ ] **Step 3: Verificar en hardware**

Flashear OTA. Visitar `http://clawdgotchi.local/`, guardar un password, comprobar que pide auth; guardar el token por el formulario; `/poll` devuelve cuota.
Expected: todo el flujo funciona desde el navegador, sin curl.

- [ ] **Step 4: Commit** — `git commit -m "web UI de ajustes con password y guardado de token"`

---

### Task 10: Display — init y hello world

**Files:**
- Create: `src/display.h`, `src/display.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces (lo usan 12 y 13): `TFT_eSPI tft;` global; `void displayBegin(uint8_t brightnessPct);` `void displayBrightness(uint8_t pct);` y un `TFT_eSprite strip(&tft)` reutilizable para render parcial.

- [ ] **Step 1: Implementar**

```cpp
// display.cpp
#include "display.h"
#include "board.h"
TFT_eSPI tft;
TFT_eSprite strip(&tft);
void displayBegin(uint8_t pct) {
  pinMode(PIN_TFT_BL, OUTPUT);
  analogWriteRange(1023);
  displayBrightness(pct);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
}
void displayBrightness(uint8_t pct) { analogWrite(PIN_TFT_BL, BL_ON_DUTY(pct)); }
```

En `setup()`: `displayBegin(cfg.brightness);` y para probar: `tft.setTextColor(TFT_ORANGE); tft.drawString("clawdgotchi", 60, 110, 4);`

- [ ] **Step 2: Verificar en hardware**

Flashear OTA. Expected: texto naranja centrado, brillo cambia al mover el slider de la web UI (conectar `displayBrightness(cfg.brightness)` en `/cfg`). Si los colores salen invertidos o desplazados: alternar `-D TFT_INVERSION_ON` y/o `TFT_RGB_ORDER` en build_flags — un ciclo de prueba OTA por ajuste.

- [ ] **Step 3: Commit** — `git commit -m "display ST7789 funcionando con brillo PWM"`

---

### Task 11: Pipeline de sprites + arte de la mascota

**Files:**
- Create: `tools/make_sprites.py`, `assets/` (PNGs generados), `src/sprites.h` (generado)

**Interfaces:**
- Produces (lo usa la 12): `src/sprites.h` con `const uint16_t SPRITE_<MOOD>_<F>[96*96] PROGMEM` (2 frames por humor, 6 humores, RGB565) y `const uint16_t* mascotFrame(Mood m, uint8_t frame);`

- [ ] **Step 1: `tools/make_sprites.py`**

Script Python (Pillow) que: (a) dibuja pixel-art procedural de Clawd —blob naranja `#D97757` estilo la mascota de Claude, 24×24 lógico escalado ×4 a 96×96— con variantes por humor (ojos, boca, gotas de sudor, "Zzz", antenita) y 2 frames (parpadeo/rebote de 2 px); (b) guarda los PNG en `assets/` para retocarlos a mano después; (c) emite `src/sprites.h` con los arrays RGB565 y la función `mascotFrame`. Transparencia: color clave `0x0000` no se dibuja (el fondo de los PNG es negro puro).

- [ ] **Step 2: Ejecutar y verificar**

Run: `python3 tools/make_sprites.py && pio run -e ultra`
Expected: 12 PNGs en `assets/`, `sprites.h` compila. Tamaño: 12×96×96×2 = 216 KB en flash — cabe de sobra en el hueco de sketch de 4 MB… **comprobar en la salida de `pio run` que el sketch sigue < 1 MB** (límite del slot OTA con 4m1m); si se pasara, bajar a 80×80 o a 1 frame por humor.

- [ ] **Step 3: Inspección visual de los PNG** (abrirlos; deben distinguirse los 6 humores a simple vista)

- [ ] **Step 4: Commit** — `git commit -m "pipeline de sprites y arte procedural de la mascota"`

---

### Task 12: Pantalla tamagotchi

**Files:**
- Create: `src/screen_tama.h`, `src/screen_tama.cpp`
- Test: la lógica de humor ya está testeada (task 2); esto es composición visual.

**Interfaces:**
- Consumes: `tft`/`strip` (10), `mascotFrame` (11), `Mood`/`QuotaSnapshot`/`formatCountdown` (2).
- Produces (lo usa la 14): `void drawTamagotchi(const QuotaSnapshot& q, Mood m, uint32_t nowEpoch, uint8_t frame);`

- [ ] **Step 1: Implementar**

Layout 240×240: mascota 96×96 centrada arriba (y=18, animada alternando `frame` cada ~500 ms con rebote vertical de 2 px); dos barras de 200×18 (x=20) en y=140 (5h) e y=185 (semanal), rellenas proporcionalmente con color por tramo (verde <50, ámbar <75, naranja <90, rojo ≥90), porcentaje a la derecha, etiqueta y countdown (`formatCountdown`) encima en font 2. En `SLEEPING`, sustituir el countdown de la barra agotada por "despierta en X". En `OFFLINE`, mascota gris + texto "sin conexión". Render: `strip` de 240×60 reutilizado en 4 franjas para no fragmentar heap; el sprite de la mascota se dibuja con `tft.pushImage(x, y, 96, 96, mascotFrame(m, frame), 0x0000)` (color clave = transparente).

- [ ] **Step 2: Conectar temporalmente en `loop()`** (cada 500 ms, con la `QuotaSnapshot` del `/poll` guardada en `quotaNow`) y flashear.

- [ ] **Step 3: Verificar en hardware**

Expected: mascota animada + barras con datos reales. Probar los 6 humores forzándolos con una ruta de debug `GET /demo?mood=0..5` (temporal, se retira en la task 14).

- [ ] **Step 4: Commit** — `git commit -m "pantalla tamagotchi con mascota animada y barras de cuota"`

---

### Task 13: Pantalla reloj + NTP con zona horaria

**Files:**
- Create: `src/screen_clock.h`, `src/screen_clock.cpp`
- Modify: `src/main.cpp` (NTP con TZ de config)

**Interfaces:**
- Consumes: `tft`/`strip` (10), `cfg.tz` (6).
- Produces (14): `void drawClock(const tm& local);` y en `setup()`: `configTime(cfg.tz, "pool.ntp.org");` (la variante ESP8266 `configTime(tz, server)` aplica el POSIX TZ directamente).

- [ ] **Step 1: Implementar** — hora HH:MM en font 7 (48 px) centrada, segundos y fecha "sáb 16 ago" en font 2 debajo. Redibujar solo cuando cambia el minuto (y los segundos en una franja pequeña).

- [ ] **Step 2: Verificar en hardware** — hora correcta para la TZ configurada (cambiarla en la web UI y comprobar el salto).

- [ ] **Step 3: Commit** — `git commit -m "pantalla reloj con NTP y zona horaria"`

---

### Task 14: Máquina de estados principal + planificador + modo seguro

**Files:**
- Modify: `src/main.cpp` (versión final)
- Create: `src/safemode.h`

**Interfaces:**
- Consumes: todo lo anterior.
- Produces: firmware completo. `main.cpp` queda como orquestador: BOOT → WIFI → AUTH → RUN.

- [ ] **Step 1: `src/safemode.h` — contador de crashes en RTC**

```cpp
#pragma once
#include <Arduino.h>
// 3 crashes seguidos -> modo seguro (solo web UI). Un arranque sano resetea el contador.
struct RtcGuard { uint32_t magic; uint32_t crashes; };
inline bool safeModeRequested() {
  RtcGuard g;
  ESP.rtcUserMemoryRead(0, (uint32_t*)&g, sizeof g);
  rst_info* ri = ESP.getResetInfoPtr();
  bool crash = ri && (ri->reason == REASON_EXCEPTION_RST || ri->reason == REASON_WDT_RST ||
                      ri->reason == REASON_SOFT_WDT_RST);
  if (g.magic != 0xC1A3D077) { g.magic = 0xC1A3D077; g.crashes = 0; }
  g.crashes = crash ? g.crashes + 1 : 0;
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&g, sizeof g);
  return g.crashes >= 3;
}
inline void markBootHealthy() {   // llamar tras 60 s de uptime estable
  RtcGuard g{0xC1A3D077, 0};
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&g, sizeof g);
}
```

- [ ] **Step 2: `main.cpp` final**

```cpp
// Estructura (código completo en el repo al implementar):
// setup():
//   LittleFS.begin(); loadConfig(cfg);
//   if (safeModeRequested()) { wifiConnectOrPortal(cfg); montar webui+ota; safeMode=true; return; }
//   wifiConnectOrPortal(cfg);
//   configTime(cfg.tz, "pool.ntp.org");
//   displayBegin(cfg.brightness);
//   montar webui + ota + /log (+auth);
// loop():
//   server.handleClient(); MDNS.update();
//   if (safeMode) return;
//   cada cfg.pollMinutes (y con backoff 1,2,4..32 min en NET_ERROR):
//     Tokens t; loadTokens(t);
//     si t.expiresAt y quedan <5 min y hay refresh -> refreshTokens(t) && saveTokens(t);
//     PollStatus st = pollQuota(t.access, quotaNow);
//     authBad = (st == AUTH_FAILED) && (!t.refresh[0] || !(refreshTokens(t) && saveTokens(t)
//               && pollQuota(t.access, quotaNow) == PollStatus::OK));
//   cada 500 ms: frame ^= 1;
//     Mood m = moodFor(quotaNow, WiFi.status() == WL_CONNECTED && !authBad);
//     pantalla según cfg.screenMode (0: alterna cada cfg.rotateSeconds; 1: tama; 2: reloj);
//     si authBad: icono "!" rojo fijo en la esquina (drawString) + aviso en la web UI;
//   a los 60 s de uptime: markBootHealthy() (una vez).
```

Retirar `/demo` y cualquier ruta temporal. El poll usa `strip.deleteSprite()` antes de la conexión TLS y lo recrea después **solo si** el spike midió heap justo (decidir con los números de la task 5).

- [ ] **Step 3: Verificar los tests nativos siguen en verde** — `pio test -e native`

- [ ] **Step 4: Verificación funcional en hardware (checklist)**

- Arranque limpio → tamagotchi con datos reales en <1 min.
- Rotación tama/reloj según config.
- Quitar la WiFi (apagar AP o alejarlo) → mascota OFFLINE, y recupera al volver.
- Token inválido a propósito → icono "!" y aviso; re-pegar token lo arregla sin reiniciar.
- `/update` sigue aceptando un firmware nuevo (¡la puerta de salida!).
- Reinicio por cable (desenchufar) ×3 NO entra en modo seguro (no son crashes).

- [ ] **Step 5: Commit** — `git commit -m "orquestador final: estados, planificador, backoff y modo seguro"`

---

### Task 15: README + cierre

**Files:**
- Create: `README.md`

- [ ] **Step 1: README** — qué es, foto/captura, instalación (enlazando `docs/instalacion.md`), recuperación a stock, configuración (web UI, `claude setup-token`), desarrollo (`pio run`, `pio test -e native`), licencia y agradecimientos (smalltv-mod y clawdmeter-daemon como referencia, WTFPL).

- [ ] **Step 2: Verificación final completa** — `pio test -e native` + checklist de la task 14 en verde.

- [ ] **Step 3: Commit** — `git commit -m "README y cierre de la v1"`

---

### Task 16 (OPCIONAL, post-v1): Login OAuth PKCE en el propio cubo

El spec pedía login PKCE en el dispositivo. La v1 usa `claude setup-token` (token de larga vida) + refresh token opcional: el cubo sigue siendo 100 % autónomo en operación y el código es una fracción. Esta tarea añade el login completo si el usuario lo quiere:

**Files:**
- Create: `src/oauth_login.h`
- Modify: `src/webui.h` (sección "Iniciar sesión con Claude")

Esbozo verificado contra implementaciones públicas (el detalle exacto se confirma al implementarla, es API no documentada): generar `code_verifier` aleatorio (43 chars base64url del RNG hardware) y `code_challenge` = base64url(SHA256(verifier)); mostrar enlace a `https://claude.ai/oauth/authorize?code=true&client_id=9d1c250a-e61b-44d9-88ed-5944d1962f5e&response_type=code&redirect_uri=https%3A%2F%2Fplatform.claude.com%2Foauth%2Fcode%2Fcallback&scope=org%3Acreate_api_key+user%3Aprofile+user%3Ainference&code_challenge=<challenge>&code_challenge_method=S256&state=<verifier>`; el usuario pega el `code#state` resultante en la web UI; el cubo hace `POST https://platform.claude.com/v1/oauth/token` (JSON: `grant_type=authorization_code, code, state, client_id, redirect_uri, code_verifier`) y guarda `access+refresh` con `saveTokens`. A partir de ahí el ciclo de refresh de la task 7 lo mantiene vivo para siempre.

---

## Self-review (hecho)

- **Cobertura del spec:** autonomía (T5/7/9), tamagotchi 6 humores (T2/11/12), barras+countdowns (T12), reloj NTP (T13), portal+web UI con password (T8/9), OTA propio (T3), modo seguro (T14), instalación/recuperación (T4), tests nativos de la lógica pura (T2/6). Desviación consciente: login PKCE movido a T16 opcional; la v1 autentica con `claude setup-token` — señalado al usuario en la entrega del plan.
- **Placeholders:** ninguno; el único "esbozo" (T16) está marcado como opcional y con sus endpoints concretos.
- **Consistencia de tipos:** `QuotaSnapshot/Mood/parseRateHeaders/formatCountdown` (T2) se usan idénticos en T5/7/12/14; `Config/Tokens` (T6) idénticos en T7/8/9/14; `webAuth` (T9) protege rutas de T3/7.
