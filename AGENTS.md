# Clawdgotchi

Firmware ESP8266 (PlatformIO / Arduino) para el GeekMagic SmallTV Ultra:
tamagotchi de la mascota de Claude con las cuotas de Claude Code + reloj, con
login OAuth en el propio dispositivo.

Normas comunes y conocimiento: `~/cerebro/AGENTS.md` (no repetir aquí lo que ya está allí).

## Estado
- **Familia:** firmware
- **Publicado:** — (no va a tiendas). v1 mergeada en `main`, 9 tests nativos
- **En curso:** nada abierto desde el 16-08-2026; operativo en el segundo cubo (`192.168.1.116`) con el firmware al día y OAuth en el dispositivo funcionando
- **Siguiente:** recuperar por UART el primer cubo (`192.168.1.114`), soft-brickeado por un experimento con `CONT_STACKSIZE` (ver `docs/decisiones.md`)

## Flujo de trabajo

editar → `pio test -e native` (9/9) → `pio run -e ultra` con las build flags de
WiFi → `tools/flash-ota.sh` → verificar en el cubo real (el visto bueno visual es
de Oskar, ver `~/cerebro/AGENTS.md`). Tras un OTA el cubo tarda ~25-30 s en
volver: espera con reintentos, no asumas que respondió.

Al terminar un cambio:
- Si flasheas, actualiza `backup/rescue-firmware.bin` con la última build buena.
- Si añades un campo a `Config`, actualiza `config_codec.{h,cpp}` (JSON), el
  formulario y el handler de `webui.h`, y comprueba que los tests nativos siguen
  pasando.

## Comandos

```sh
# PlatformIO suele estar en ~/Library/Python/3.9/bin (macOS). Añádelo al PATH.
export PATH="$HOME/Library/Python/3.9/bin:$PATH"

# Compilar firmware. Las credenciales WiFi van SIEMPRE por build flags, NUNCA en el repo:
PLATFORMIO_BUILD_FLAGS="-DCLAWD_WIFI_SSID='\"MiRed\"' -DCLAWD_WIFI_PASS='\"MiClave\"'" \
  pio run -e ultra

# Tests nativos (host) de la lógica pura — deben quedar 9/9:
pio test -e native

# Regenerar sprites (Pillow). Emite assets/*.png y src/sprites.h (generado):
python3 tools/make_sprites.py

# Flashear por OTA (el cubo se reinicia solo; LittleFS se conserva):
CLAWD_WEB_AUTH=admin:<clave> tools/flash-ota.sh <ip-o-host>
```

⚠️ **`pio test -e native` borra `.pio/build/ultra/`.** Si vas a flashear, compila
`ultra` DESPUÉS de correr los tests, no antes.

## Verificación mínima antes de commitear

1. `pio test -e native` → 9/9.
2. `pio run -e ultra` (con las build flags de WiFi) → SUCCESS.
3. `strings .pio/build/ultra/firmware.bin | grep -c "MiRed"` → ≥1 (las
   credenciales se incrustan) **y** `git diff` sin ninguna credencial.

## Mapa de módulos

- **`lib/core/`** — C++ puro, sin Arduino, compila en host y en el ESP. Es lo
  único con tests (`test/test_native_*`).
  - `quota.{h,cpp}` — `parseRateHeaders()`, `moodFor()`, `formatCountdown()`.
  - `config_codec.{h,cpp}` — `Config`/`Tokens` ↔ JSON (ArduinoJson).
- **`src/`** — código de dispositivo.
  - `main.cpp` — `setup()`/`loop()`: máquina de estados de arranque, planificador
    de poll con backoff, rotación de pantallas + fade, canje OAuth, modo seguro.
  - `claude_client.{h,cpp}` — `pollQuota()`, `refreshTokens()`, `tlsHeapOk()`.
  - `oauth_login.h` — PKCE (verifier/challenge SHA256+base64url) + `oauthExchange()`.
  - `webui.h` — HTML (PROGMEM con placeholders `%XX%`) + rutas `/`, `/cfg`,
    `/token`, `/oauth/start`, `/oauth/finish`, `/status`, `/log`, `/update`.
  - `wifi_portal.h` — portal cautivo de primer arranque.
  - `display.{h,cpp}`, `screen_tama.cpp`, `screen_clock.cpp` — pantalla ST7789.
  - `sprites.h` (**generado**, no editar), `sprites.cpp`, `mascot.h`.
  - `store.h` (LittleFS), `certs.h` (CA pineadas), `safemode.h`, `netlog.h`,
    `ota.h`, `htmlutil.h`, `board.h`.
- **`tools/`** — `make_sprites.py`, `flash-ota.sh`, `seed-token.sh` (**eliminado**;
  el OAuth en dispositivo lo hace redundante).

## Convenciones y reglas duras

- Credenciales WiFi solo por build flags (`CLAWD_WIFI_SSID`/`CLAWD_WIFI_PASS`);
  la norma general de secretos está en `~/cerebro/AGENTS.md`.
- **No toques la configuración del cubo (`/cfg`) por curl para "demos".** Pisa los
  ajustes del usuario (brillo, NTP…) y parece que "no se guardan". Si hace falta
  cambiar el modo de pantalla, pídeselo a Oskar o lee y reescribe TODOS los
  campos actuales.
- **TLS solo desde `loop()`**, jamás dentro de un handler HTTP: la pila "cont"
  del WebServer es de ~4 KB y el handshake la revienta. El patrón es: el handler
  guarda datos + levanta un flag (`pollPedido`, `oauthPending`), y `loop()`
  ejecuta el TLS.
- Nada de arrays grandes en la pila de handlers: `Tokens` y buffers son `static`
  o globales (`g_tokens`, `s_iobuf`).
- La web UI escapa todo texto de usuario con `htmlEscape()` (evita XSS y que un
  valor con `%XX%` reescriba otro placeholder en los `String::replace`
  encadenados).
- `sprites.h` es **generado**: edita `tools/make_sprites.py` y regenera, no el
  header.
- Textos de la interfaz y comentarios en español.
- **Fuentes de pantalla:** `LOAD_GFXFF` habilita las GFX Free Fonts de TFT_eSPI
  (Sans/Serif/Mono, 9-24 pt, regular/bold/oblique). En uso: `FreeMono9pt7b`
  (etiquetas de las barras), `FreeSansBold24pt7b` escalada ×2 (reloj). El linker
  solo enlaza las que referencias. Tras dibujar con `setFreeFont`, restaura con
  `setTextFont(2)` (y `setTextSize(1)` si escalaste) para no afectar al resto.

## Lecciones de hardware (no repetir estos errores)

- **Buffer TLS = 8192, no 16384.** El handshake consume heap proporcional al
  buffer RX. Con 16384 (~22 KB de pico) NO cabe fiable en este chip con
  pantalla+sprites (~19-20 KB contiguos) y el poll crasheaba a veces al arrancar.
  Con 8192 (~11-12 KB) cabe. `platform.claude.com` **no** soporta MFLN, así que
  8192 es el mínimo seguro. Guard `tlsHeapOk()` (umbral 13000) evita intentar el
  handshake sin heap.
- **El contador de crashes vive en RTC y sobrevive al OTA** (reset "suave"). Si
  se atasca en ≥3, cada reflasheo arranca en modo seguro; antes hacía falta
  desenchufar. Ya hay auto-recuperación (30 s en modo seguro → limpia el contador
  y reinicia). Un `CONT_STACKSIZE` grande brickea de verdad (corrompe la región
  SYS antes del WiFi): **prohibido**.
- **La respuesta OAuth se lee con `http.getString()` antes de parsear**, no con
  `deserializeJson(stream)`: con el buffer TLS pequeño el stream se trocea y el
  parseo falla.
- **LittleFS (config + tokens) persiste** entre reinicios y OTA. Si al probar
  parece que "no se guarda", casi seguro es que algo (¡o alguien!) reescribió
  `/config.json` después — `/cfg` sobrescribe TODOS los campos, así que un POST
  parcial pone defaults.

## Docs relacionadas

- `docs/instalacion.md` — flasheo y recuperación (loader, UART).
- `docs/decisiones.md` — decisiones de diseño y el incidente del primer cubo.
- `docs/verificacion-hardware.md` — checklist de verificación física.
- `docs/superpowers/` — spec y plan originales del proyecto.
