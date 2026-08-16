# Instalación y recuperación

## Primera instalación (hecha el 2026-08-16)

Solo se hace una vez; después todo va por OTA.

1. **Backup del stock** (red de seguridad): `backup/FW-Smalltv-Ultra-V9.0.50.bin`
   (md5 `a5fe50093ada3c2637225bbf306a62c3`, verificado contra el
   [repo oficial](https://github.com/GeekMagicClock/smalltv-ultra)).
2. **Loader** (el slot OTA del stock es demasiado pequeño para nuestra imagen):
   subir `backup/smalltv-mod-loader.bin` (WTFPL, de
   [smalltv-mod](https://github.com/giovi321/smalltv-mod) v2.9.2) en
   `http://<ip-del-cubo>/update`. El cubo abre el AP `SmallTV-Loader`.
3. **Clawdgotchi**: unirse al AP y subir `.pio/build/ultra/firmware.bin` en
   `http://192.168.4.1/update`.

Resultado medido tras el primer arranque: heap libre 48,2 KB, bloque
contiguo máximo 47,9 KB — margen de sobra para el handshake TLS (~17-25 KB).

## Actualizaciones (sin intervención)

```sh
pio run -e ultra && tools/flash-ota.sh          # clawdgotchi.local por defecto
CLAWD_WEB_AUTH=admin:clave tools/flash-ota.sh   # si la web UI tiene password
```

## Spike TLS (Task 5)

`/spike?token=<token de \`claude setup-token\`>` prueba NTP + MFLN + TLS pineado
(`src/certs.h`: GTS Root R4 para api.anthropic.com, ISRG Root YE para
platform.claude.com) + POST mínimo a `api.anthropic.com/v1/messages`.
Resultado con token real (2026-08-16): **code=200 con las 5 cabeceras de
cuota** (5h=0.12, 7d=0.18). `mfln1024=0` (el servidor rechaza MFLN) →
buffers definitivos **RX=16384, TX=512**. Heap: maxblock 35656 antes del
handshake, 13024 en el punto más bajo — margen suficiente. Checkpoint
superado.

## Recuperación

- **Volver al stock**: subir `backup/FW-Smalltv-Ultra-V9.0.50.bin` por el
  `/update` de clawdgotchi (misma mecánica que cualquier OTA).
- **Cubo sin red** (bug de WiFi): el firmware entra en modo seguro tras 3
  crashes seguidos y siempre expone `/update`; si ni eso, UART con esptool
  (`write_flash 0x0 <bin>`) abriendo la caja.
