# clawdgotchi

**Firmware propio y autónomo para el GeekMagic SmallTV Ultra** que convierte el cubo en un tamagotchi de la mascota de Claude. Muestra las cuotas semanales de Claude Code (5h/semanal) + modo reloj NTP.

> **Descargo:** Este proyecto no es afiliado a GeekMagic ni a Anthropic.

## Mascota con cuota

| Estado | Sprite |
|--------|--------|
| Feliz (cuota > 60%) | ![happy](assets/sprite_happy_0.png) |
| Despierto (cuota 30-60%) | ![uneasy](assets/sprite_uneasy_0.png) |
| Dormido (cuota < 30%) | ![sleeping](assets/sprite_sleeping_0.png) |

La mascota tiene **6 humores** según la cuota restante de Claude Code:
- **Feliz** (>60%): animación activa
- **Despierto** (30-60%): alerta, movimiento moderado  
- **Dormido** (<30%): animación lenta, bajo consumo
- **Incómodo** (~10%): advertencia visual
- **Estresado** (~5%): urgencia
- **Pánico** (0%): reset semanal — barras de countdown

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

## Instalación

Para la **primera instalación**, necesitas un cable UART y esptool (ver [docs/instalacion.md](docs/instalacion.md)).

A partir de la segunda instalación, todo se hace por OTA.

### Configuración rápida

1. Conéctate al AP `SmallTV-Clawdgotchi` que expone el cubo
2. Abre `http://192.168.4.1` en el navegador
3. Configura red WiFi (SSID + contraseña)
4. Reinicia y accede a `http://clawdgotchi.local` (o por IP)
5. Portal de configuración:
   - Password (opcional, para la web UI)
   - Token de Claude Code: usa `claude setup-token` en la CLI de Claude Code y pega el token

## ⚠️ ADVERTENCIA: Estado actual

**El cubo requiere recuperación por UART** tras un flasheo de diagnóstico durante desarrollo. Lee [docs/decisiones.md](docs/decisiones.md) para detalles de recuperación (~15 minutos con adaptador USB-TTL).

Una vez recuperado, la verificación completa de hardware es la siguiente tarea.

## Desarrollo

### Compilación

```sh
# Build para el cubo (ESP8266 + pantalla 160×128)
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
- **Portal WiFi** (vía web UI en `192.168.4.1`)
- **Build flags** (variable de entorno `PLATFORMIO_BUILD_FLAGS`):
  ```sh
  export PLATFORMIO_BUILD_FLAGS="-D WIFI_SSID=MySSID -D WIFI_PASS=MyPassword"
  pio run -e ultra
  ```

### Pruebas

```sh
pio test -e native
```

Corre 9 tests unitarios (sin hardware):
- Parsing de cuotas desde headers HTTP
- Cálculo de countdown (reset semanal)
- Selección de sprite según cuota
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
