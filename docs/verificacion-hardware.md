# Verificación hardware pendiente (tras recuperar el cubo por UART)

Todo el firmware está construido, testeado (19 tests nativos) y revisado,
pero desde el incidente del brickeo (ver `docs/decisiones.md`) las tareas
7-15 se verificaron solo en compilación. Al recuperar el cubo:

## Paso 0 — recuperar y flashear

1. UART: `esptool.py write_flash 0x0 backup/rescue-firmware.bin` (guía en
   `docs/decisiones.md`). Con eso el cubo vuelve a la red.
2. Flashear la versión final: `pio run -e ultra && tools/flash-ota.sh`
   (compilar con `PLATFORMIO_BUILD_FLAGS` de WiFi o dejar que el portal pida la red).
3. Iniciar sesión: web UI → poner password → «Abrir login de Claude» y pegar
   el código (alternativa: pegar un `claude setup-token`).

## Checklist (en orden)

- [ ] Arranque: pantalla enciende, texto/colores correctos (si colores
      invertidos: añadir `-D TFT_INVERSION_ON` a build_flags y re-flashear OTA).
- [ ] Tamagotchi con datos reales en <1 min tras guardar el token
      (`/status` muestra los %).
- [ ] Mascota transparente sobre fondo negro (sin cuadrado azul).
- [ ] Barras: el "%" se ve entero (incl. "100%"); countdowns sensatos.
- [ ] Reloj: hora correcta; cambiar TZ en la web UI y ver el salto al momento.
- [ ] Rotación tama/reloj según config, sin restos entre pantallas.
- [ ] Brillo desde el slider surte efecto.
- [ ] Quitar la WiFi → mascota OFFLINE; recuperar → vuelve sola (backoff).
- [ ] Token inválido a propósito → "!" rojo + aviso web; re-pegar token
      bueno lo limpia al instante sin reiniciar.
- [ ] Web UI con password: pide auth en /, /log, /status; /token sin
      password da 403.
- [ ] `/update` acepta un firmware (¡la puerta de salida!).
- [ ] Desenchufar 3 veces seguidas NO entra en modo seguro (no son crashes).
- [ ] Portal cautivo: con config vaciada (borrar /config.json desde un
      firmware de prueba o reflashear con FS limpio) aparece el AP
      `Clawdgotchi-Setup` y el formulario lista redes (o admite SSID manual).
- [ ] Refresh de token: si se pegó también un refresh token, comprobar en
      /log que renueva al caducar (o forzar con expiresAt corto).
- [ ] Animación: freeze de ~5 s durante el poll es esperado (el TLS bloquea
      el `loop()`).

## Tras la auditoría (rama `fix/auditoria`)

Compilado solo en host contra las cabeceras del core; falta el visto bueno
en el cubo:

- [ ] `pio run -e ultra` → SUCCESS y RAM/flash similares a antes.
- [ ] **Refresh real**: tras un login OAuth, esperar a que caduque (~8 h) o
      forzar `exp` corto; `/log` debe decir `token renovado` y la mascota
      seguir con datos.
- [ ] Login OAuth completo con el `state` nuevo; pegar el código de una
      sesión anterior → `/log` dice `oauth: state no coincide`.
- [ ] Cambiar la contraseña en `/cfg` y, **sin reiniciar**, `/update` pide la
      nueva. Guardar con el campo vacío → la contraseña no cambia.
- [ ] Contraseña de 33 caracteres → 400 con mensaje legible (con «ñ»).
- [ ] La página `/` carga entera (barras, ajustes, sesión) y el heap libre
      es igual o mejor que antes.
- [ ] `tools/flash-ota.sh` con `CLAWD_WEB_AUTH` sigue funcionando.
- [ ] Modelo inválido a propósito (p.ej. `claude-no-existe`) → aviso en la
      web UI con el código HTTP; restaurarlo → desaparece en el siguiente poll.
- [ ] Apagar el router y reiniciar el cubo (entra al portal); encender el
      router → en ≤5 min el cubo vuelve solo a la red.
