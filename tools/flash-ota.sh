#!/bin/sh
# Sube el firmware compilado al cubo por OTA.
# Uso: tools/flash-ota.sh [host]           (por defecto clawdgotchi.local)
# Si la web UI tiene password: CLAWD_WEB_AUTH=admin:clave tools/flash-ota.sh
set -e
HOST=${1:-clawdgotchi.local}
BIN=.pio/build/ultra/firmware.bin
[ -f "$BIN" ] || { echo "no existe $BIN — compila primero (pio run -e ultra)"; exit 1; }
# Las credenciales van a curl por stdin (-K -), no como argumento: asi no
# aparecen en `ps` ni en el historial de procesos.
if [ -n "$CLAWD_WEB_AUTH" ]; then
  # Escapa \ y " para el formato de config de curl.
  AUTH_ESC=$(printf '%s' "$CLAWD_WEB_AUTH" | sed 's/[\\"]/\\&/g')
  printf 'user = "%s"\n' "$AUTH_ESC" |
    curl -sf --max-time 120 -K - -F "update=@$BIN" "http://$HOST/update" >/dev/null
else
  curl -sf --max-time 120 -F "update=@$BIN" "http://$HOST/update" >/dev/null
fi
echo "OK: $BIN subido a $HOST (el cubo se reinicia solo)"
