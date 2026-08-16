#!/bin/sh
# Siembra el token de Claude en el cubo SIN copiar y pegar: lee las
# credenciales que Claude Code ya guarda en tu máquina y las empuja al
# firmware. Incluye el refresh token, así que el cubo se renueva solo
# para siempre y no vuelves a necesitar el ordenador.
#
# Uso: tools/seed-token.sh <ip-o-host-del-cubo> <password-web>
#   ej: tools/seed-token.sh 192.168.1.116 miclave
#
# El usuario de la web UI es siempre "admin".
set -e
HOST=${1:?falta la IP/host del cubo}
PASS=${2:?falta la contraseña de la web UI}
CREDS="$HOME/.claude/.credentials.json"
[ -f "$CREDS" ] || { echo "no encuentro $CREDS — ¿has hecho login en Claude Code?"; exit 1; }

# El fichero guarda expiresAt en milisegundos; el firmware lo quiere en
# segundos epoch. python3 extrae los tres campos del bloque claudeAiOauth.
eval "$(python3 - "$CREDS" <<'PY'
import json, sys, shlex
o = (json.load(open(sys.argv[1])) or {}).get("claudeAiOauth") or {}
a = o.get("accessToken", ""); r = o.get("refreshToken", "")
e = int(o.get("expiresAt", 0) or 0) // 1000
if not a:
    sys.stderr.write("no hay accessToken en las credenciales\n"); sys.exit(1)
print(f"ACCESS={shlex.quote(a)}\nREFRESH={shlex.quote(r)}\nEXP={e}")
PY
)"

curl -sf --max-time 20 -u "admin:$PASS" \
  --data-urlencode "access=$ACCESS" \
  --data-urlencode "refresh=$REFRESH" \
  --data-urlencode "exp=$EXP" \
  "http://$HOST/token" >/dev/null

echo "OK: token sembrado en $HOST (con refresh token → el cubo se renovará solo)."
[ -n "$REFRESH" ] || echo "AVISO: no había refresh token; el cubo funcionará hasta que el access caduque y luego habrá que re-sembrar."
