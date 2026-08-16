#!/bin/sh
# Sube el firmware compilado al cubo por OTA.
# Uso: tools/flash-ota.sh [host]           (por defecto clawdgotchi.local)
# Si la web UI tiene password: CLAWD_WEB_AUTH=admin:clave tools/flash-ota.sh
set -e
HOST=${1:-clawdgotchi.local}
BIN=.pio/build/ultra/firmware.bin
[ -f "$BIN" ] || { echo "no existe $BIN — compila primero (pio run -e ultra)"; exit 1; }
curl -sf --max-time 120 ${CLAWD_WEB_AUTH:+--user "$CLAWD_WEB_AUTH"} \
  -F "update=@$BIN" "http://$HOST/update" >/dev/null
echo "OK: $BIN subido a $HOST (el cubo se reinicia solo)"
