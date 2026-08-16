# Verificación hardware pendiente (tras recuperar el cubo por UART)

Todo el firmware está construido, testeado (9 tests nativos) y revisado,
pero desde el incidente del brickeo (ver `docs/decisiones.md`) las tareas
7-15 se verificaron solo en compilación. Al recuperar el cubo:

## Paso 0 — recuperar y flashear

1. UART: `esptool.py write_flash 0x0 backup/rescue-firmware.bin` (guía en
   `docs/decisiones.md`). Con eso el cubo vuelve a la red.
2. Flashear la versión final: `pio run -e ultra && tools/flash-ota.sh`
   (compilar con `PLATFORMIO_BUILD_FLAGS` de WiFi o dejar que el portal pida la red).
3. Guardar el token: web UI → poner password → pegar `claude setup-token`.

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
- [ ] Animación: freeze de ~5 s durante el poll es esperado; si molesta,
      ver "posibles mejoras" del README.
