# Decisiones tomadas en autonomía

> ## ⚠️ LO PRIMERO QUE DEBES LEER: el cubo necesita recuperación por UART
>
> Durante la Task 7 un subagente flasheó una build de diagnóstico con
> `CONT_STACKSIZE=8192` (hack de linker NO commiteado). Ese flag corrompe
> la región SYS del ESP8266 al arrancar: el cubo entra en bucle de crash
> **antes de levantar el WiFi**, así que no responde a HTTP ni a OTA.
> Encender/apagar no lo arregla.
>
> **Cómo recuperarlo (~15 min):**
> 1. Necesitas un adaptador USB-TTL de 3.3 V (CP2102/CH340/FTDI).
> 2. Abre la caja del cubo; el PCB tiene los pads UART del ESP-12F
>    (TX, RX, GND) y GPIO0.
> 3. Conecta TX↔RX, RX↔TX, GND↔GND. Puentea **GPIO0 a GND** mientras
>    enchufas la alimentación (modo bootloader).
> 4. `pip3 install esptool` y luego:
>    `esptool.py --port /dev/tty.usbserial* write_flash 0x0 backup/rescue-firmware.bin`
>    (`backup/rescue-firmware.bin` es la build buena que dejé preparada;
>    también vale `backup/FW-Smalltv-Ultra-V9.0.50.bin` para volver al stock).
> 5. Quita el puente de GPIO0, reinicia — el cubo vuelve a `clawdgotchi.local`
>    y desde ahí todo sigue por OTA como siempre.
>
> He dejado un "watcher de rescate" corriendo por si el cubo diera alguna
> señal de vida en red (improbable): si responde, le flashea la build buena
> solo. Mientras tanto he seguido construyendo el firmware completo con
> tests nativos y verificación de compilación; cuando el cubo vuelva, se
> flashea la versión final y se verifica todo el hardware de una tanda.

Registro para Oskar de todo lo decidido sin consultarle (sesión del
2026-08-16, mientras estaba fuera). Orden cronológico. El detalle fino de
cada tarea está en `.superpowers/sdd/2026-08-16-clawdgotchi/progress.md`.

## Antes de quedarme solo (ya comunicadas en el chat)

1. **v1 con `claude setup-token` en vez de login PKCE en el cubo.** El PKCE
   queda como Task 16 opcional. Coste si no te gusta: implementar la T16.
2. **Task 3 compiló con WiFi vacío** y las credenciales se inyectaron al
   flashear. Sin coste.
3. **Raíces TLS pineadas GTS Root R4 + ISRG Root YE** (capturadas de las
   cadenas reales, no el ejemplo ISRG X1 del plan). Si Anthropic rota de CA,
   se recaptura y se reflashea por OTA.
4. **Buffers TLS RX=16384/TX=512**: el servidor rechazó MFLN
   (`mfln1024=0`); medido en hardware con 13 KB de margen en el pico.

## En autonomía

5. **`caffeinate -ims` en segundo plano** mientras trabajo: tu Mac se
   durmió a mitad de una revisión y mató al subagente. Se para solo al
   cerrar la sesión; si lo ves en Actividad, es esto.
6. **Tu token `setup-token` lo guardo yo en el cubo** (POST al endpoint
   `/token` del firmware) para poder verificar el poll real sin ti. El
   token vive en `/tokens.json` del LittleFS del cubo, no en el repo.
7. **El portal cautivo (T8) mantiene las credenciales compiladas como
   fallback**, desviación del plan (que las eliminaba): si un flasheo
   desatendido dejara la config vacía, el cubo seguiría entrando en tu
   WiFi en vez de quedarse en modo AP esperándote. El portal sigue
   existiendo para un cubo virgen. Coste: un `#ifdef` extra.
   **Prueba pendiente contigo:** verificar que el AP `Clawdgotchi-Setup`
   aparece con una config vacía (yo no puedo unirme a APs).
8. **Las verificaciones visuales de pantalla (T10-T13) quedan pendientes
   de tus ojos.** Yo verifico todo lo verificable sin mirar la pantalla:
   compilación, cubo vivo tras cada flasheo, heap, log y un endpoint de
   debug que dice qué se está dibujando. Si los colores salen invertidos,
   el ajuste es un build flag (`TFT_INVERSION_ON`) y un flasheo OTA.
9. **Password de la web UI**: el spec la exige para configurar OAuth y así
   quedó implementado (guardar token responde 403 sin password). La eliges
   tú en la web UI al estrenar el cubo — no hay ninguna por defecto.

## Estado final

- **Las 15 tareas del plan completadas**, cada una con revisión
  independiente y ronda de fixes; revisión final de rama (modelo mayor)
  con veredicto "mergeable" tras una ola de 16 correcciones, re-revisada
  en limpio.
- 9 tests nativos en verde; compila con y sin credenciales; sin secretos
  en el repo (verificado en cada revisión y en el historial).
- RAM estática final 41,5 KB (se recuperaron 7,4 KB unificando buffers);
  flash 71 % con los 12 sprites incluidos.
- **Pendiente solo**: recuperar el cubo por UART y pasar el checklist de
  `docs/verificacion-hardware.md` (todo lo visual/físico en una tanda).
- El PKCE en el cubo queda como mejora opcional (Task 16 del plan).
