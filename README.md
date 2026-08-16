# clawdgotchi

**Firmware propio y autónomo para el GeekMagic SmallTV Ultra** que convierte el cubo en un tamagotchi de la mascota de Claude. Muestra las cuotas semanales de Claude Code (5h/semanal) + modo reloj NTP.

> **Descargo:** Este proyecto no es afiliado a GeekMagic ni a Anthropic.

## Mascota con cuota

El humor lo decide la **peor** (más usada) de las dos cuotas de Claude Code
que están en uso, la de 5h y la semanal — no la que queda libre:

| Estado | Sprite |
|--------|--------|
| Contenta, <50% usada | ![happy](assets/sprite_happy_0.png) |
| Inquieta, 50-74% usada | ![uneasy](assets/sprite_uneasy_0.png) |
| Dormida, cuota agotada | ![sleeping](assets/sprite_sleeping_0.png) |

La mascota tiene **6 humores**, calculados en `moodFor()` (`lib/core/quota.cpp`):
- **Contenta** (happy, <50% usada): animación normal
- **Inquieta** (uneasy, 50-74% usada)
- **Agobiada** (stressed, 75-89% usada)
- **Pánico** (panic, ≥90% usada)
- **Dormida** (sleeping, cuota agotada — `status=rejected` o 100% usada): countdown de cuándo despierta
- **Desconectada** (offline, sprite gris): sin datos válidos o sin WiFi

## Características

- **Reloj NTP** con zona horaria configurable
- **Captive Portal WiFi** para configuración de credenciales sin código
- **Web UI** con autenticación por contraseña
- **Actualización OTA** — sin intervención física tras la primera instalación
- **Modo seguro anti-brickeo** — si hay 3+ crashes consecutivos, el firmware entra en modo seguro y siempre expone `/update`
- **Poll directo a Anthropic API** con handshake TLS pineado (sin demonio en el PC)
  - Certs pineados: GTS Root R4 para `api.anthropic.com`, ISRG Root YE para `platform.claude.com`
  - Buffers: RX=16KB, TX=512B (MFLN rechazado por el servidor)
- **Memoria mínima**: heap máximo 35KB antes del handshake, margen suficiente
- Durante la consulta TLS (~5 s cada pocos minutos) la animación se congela brevemente

## Instalación

La **primera instalación NO requiere abrir la caja ni cable UART**: se hace
en dos pasos por `/update` (loader + firmware), ver
[docs/instalacion.md](docs/instalacion.md). El UART es el plan B, solo para
recuperación si el cubo se queda sin red.

A partir de la segunda instalación, todo se hace por OTA.

### Configuración rápida

1. Conéctate al AP `Clawdgotchi-Setup` que expone el cubo
2. Abre `http://192.168.4.1` en el navegador
3. Configura red WiFi (SSID + contraseña)
4. Reinicia y accede a `http://clawdgotchi.local` (o por IP)
5. Portal de configuración (usuario del navegador siempre **`admin`**):
   - Password de la web UI: configúrala y guárdala primero — es **obligatoria**
     para poder iniciar sesión con Claude después.
   - **Iniciar sesión con Claude (sin ordenador)** — la vía recomendada: pulsa
     «Abrir login de Claude» desde el móvil, inicia sesión, copia el código que
     te da Claude y pégalo en el cubo. El dispositivo hace el canje OAuth él
     solo y guarda access + refresh token, así que se renueva para siempre sin
     PC ni CLI.
   - Alternativa manual: pega un token de `claude setup-token` en el formulario
     de token.

Una vez recuperado, la verificación completa de hardware es la siguiente tarea.

## Desarrollo

### Compilación

```sh
# Build para el cubo (ESP8266 + pantalla 240×240)
pio run -e ultra

# Tests nativos (9 casos de prueba)
pio test -e native

# Generar sprites (desde spritesheet)
python3 tools/make_sprites.py

# Actualización OTA (requiere acceso a http://clawdgotchi.local/update)
tools/flash-ota.sh
# Con password en web UI:
CLAWD_WEB_AUTH=admin:contraseña tools/flash-ota.sh
```

### Credenciales WiFi

**Nunca hardcodees credenciales en el repositorio.**

Las credenciales se pasan solo por:
- **Portal cautivo WiFi** (vía web UI en `192.168.4.1`) — la vía normal, sin
  tocar el build.
- **Build flags** (`PLATFORMIO_BUILD_FLAGS`) — **opcional**: un fallback de
  un solo uso que se migra a `config.json` en el primer arranque y no vuelve
  a usarse; solo hace falta si quieres saltarte el portal cautivo la primera
  vez. Escapado exacto (los macros son `CLAWD_WIFI_SSID`/`CLAWD_WIFI_PASS`):
  ```sh
  PLATFORMIO_BUILD_FLAGS="-DCLAWD_WIFI_SSID='\"MiRed\"' -DCLAWD_WIFI_PASS='\"MiClave\"'" pio run -e ultra
  ```

### Pruebas

```sh
pio test -e native
```

Corre 9 tests unitarios (sin hardware):
- Parsing de cuotas desde headers HTTP
- Cálculo de countdown (reset semanal)
- Umbrales de humor (moodFor)
- Configuración (lectura/escritura)
- Y más.

## Licencia

MIT License (copyright 2026 Oskar).

Ver [LICENSE](LICENSE) para el texto completo.

### Agradecimientos

Este proyecto usa como referencia:
- [smalltv-mod](https://github.com/giovi321/smalltv-mod) v2.9.2 (loader OTA y hardware) — WTFPL
- [clawdmeter-daemon](https://github.com/oskar-gmerek/clawdmeter-daemon) (endpoints de cuota) — WTFPL

## Nota honesta

El flujo de autenticación OAuth y las cabeceras de rate limit (`anthropic-ratelimit-*`) que este firmware usa **no son API pública documentada** de Anthropic. Pueden cambiar sin previo aviso. Si construyes sobre esto, mantén ojo a los cambios en `api.anthropic.com`.
