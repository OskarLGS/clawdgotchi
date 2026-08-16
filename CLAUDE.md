# CLAUDE.md

Guía para Claude Code en este repo. La referencia operativa completa (comandos,
mapa de módulos, convenciones y lecciones de hardware) está en
[`AGENTS.md`](AGENTS.md) — **léelo antes de tocar el firmware**.

## Contexto en una línea

Firmware ESP8266 para el GeekMagic SmallTV Ultra: tamagotchi de la mascota de
Claude con las cuotas de Claude Code + reloj, con login OAuth en el propio
dispositivo. PlatformIO / Arduino.

## Lo esencial que no se ve en el código

- **Flujo de trabajo:** editar → `pio test -e native` (9/9) → `pio run -e ultra`
  con las build flags de WiFi → `tools/flash-ota.sh` → verificar en el cubo real.
  El cambio visual/físico lo juzga el usuario mirando la pantalla; no lo des por
  bueno sin su confirmación.
- **NO metas credenciales WiFi ni tokens en el repo.** Solo por build flags.
- **NO toques la configuración del cubo (`/cfg`) por curl para "demos".** Pisa los
  ajustes del usuario (brillo, NTP…) y parece que "no se guardan". Si necesitas
  cambiar el modo de pantalla, pídeselo al usuario o lee y reescribe TODOS los
  campos actuales.
- **TLS solo desde `loop()`**, nunca en handlers HTTP. Buffer TLS = 8192.
- Tras un OTA, el cubo tarda ~25-30 s en volver; espera con reintentos, no
  asumas que respondió.

## Al terminar un cambio

- Un commit por cambio, mensaje en español, corto y descriptivo.
- Actualiza `backup/rescue-firmware.bin` con la última build buena si flasheas.
- Si añades un campo a `Config`, actualiza `config_codec.{h,cpp}` (JSON), el
  formulario y el handler de `webui.h`, y comprueba que los tests nativos siguen
  pasando.

## Estado del hardware

- **Cubo operativo:** el segundo (`192.168.1.116`), con el firmware al día.
- **Primer cubo (`192.168.1.114`):** soft-brickeado por un experimento con
  `CONT_STACKSIZE`; solo se recupera por UART (ver `docs/decisiones.md`).
