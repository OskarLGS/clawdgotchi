# Clawdgotchi — spec de diseño

**Fecha:** 2026-08-16
**Estado:** aprobado en conversación, pendiente de plan de implementación

## Qué es

Firmware propio, escrito desde cero, para el GeekMagic SmallTV Ultra
(ESP8266 ESP-12F, 4 MB flash, pantalla IPS 1.54" 240×240 ST7789 por SPI).
Sustituye por completo al firmware de fábrica (v9.0.x) y convierte el cubo
en un tamagotchi de la mascota de Claude que muestra las cuotas de uso de
Claude Code (ventana de 5 horas y semanal), más un modo reloj.

**Requisito rector: el dispositivo es autónomo.** No depende de ningún
demonio en el Mac ni de otro dispositivo. Obtiene las cuotas él solo
hablando con la API de Anthropic, con su propia sesión OAuth.

No afiliado a GeekMagic ni a Anthropic.

## Contexto que fija decisiones

- El repo oficial `GeekMagicClock/smalltv-ultra` solo publica binarios:
  no hay fuente que forkear. El firmware es nuestro desde cero.
- El proyecto WTFPL [smalltv-mod](https://github.com/giovi321/smalltv-mod)
  se usa **solo como documentación de referencia** (pines del panel,
  truco del loader para instalar en el Ultra). Su medidor de Claude depende
  de un demonio en el PC; ese diseño queda descartado por el requisito de
  autonomía.
- La cuota se lee como hace clawdmeter-daemon: una llamada mínima a la API
  (modelo más barato, `max_tokens: 1`) leyendo las cabeceras de rate limit
  unificadas (5 h + 7 días). Flujo OAuth de Claude Code: no es API pública
  documentada; riesgo asumido de que Anthropic lo cambie.
- El dispositivo del usuario está en 192.168.1.114 con stock v9.0.50.

## Arquitectura

Proyecto PlatformIO, core Arduino para ESP8266, repo `clawdgotchi`.

| Módulo | Responsabilidad |
|---|---|
| `display` | TFT_eSPI + ST7789. Render por sprites/franjas; nunca framebuffer completo (115 KB no caben en ~80 KB de RAM). |
| `mascot` | Máquina de estados del tamagotchi y sus animaciones. Entrada: % de cuota y estado de conexión. Salida: frames. |
| `claude` | OAuth (login inicial PKCE, refresh) y polling de cuotas por TLS (BearSSL). |
| `clockscreen` | Hora y fecha vía NTP, zona horaria configurable. |
| `webui` | Portal cautivo de primer arranque (WiFi) + página de ajustes con contraseña + OTA de actualizaciones propias. |
| `store` | Config y tokens en LittleFS. |

Loop principal: máquina de estados `BOOT → WIFI → AUTH → RUN`.
Animación a ~10–15 fps. Polling de cuota cada 5 min (configurable).

## Pantallas

Dos pantallas; rotación automática configurable o pantalla fija, elegido
desde la web UI.

**Tamagotchi.** Mascota pixel-art estilo Clawd, sprites RGB565 en flash.
El humor lo dicta la peor de las dos cuotas:

| Cuota (peor de 5h/semanal) | Estado |
|---|---|
| < 50 % | contenta |
| 50–75 % | inquieta |
| 75–90 % | agobiada |
| > 90 % | pánico |
| agotada | dormida, con cuenta atrás hasta el reset |
| sin datos / sin WiFi | animación "desconectada" |

Debajo: dos barras (5 h y semanal) con porcentaje y countdown de reset.

**Reloj.** Hora grande + fecha.

## Datos de Claude

- **Login una sola vez.** La web UI del cubo genera el enlace de
  autorización (PKCE, verifier generado en el dispositivo). El usuario se
  loguea en su navegador y pega el código de vuelta en la web UI. El cubo
  canjea el código, guarda access + refresh token en LittleFS y desde ahí
  se renueva solo. Sesión propia: no toca ni rompe la sesión del Mac.
- **Lectura de cuota.** Llamada mínima a la API leyendo cabeceras de rate
  limit. TLS con la CA raíz pineada (no `setInsecure`).

## Manejo de errores

- Sin WiFi → reintentos con backoff; mascota "desconectada".
- Falla el refresh del token → icono de aviso persistente ("necesito
  re-login") en pantalla; la config nunca se borra sola.
- TLS sin heap → liberar sprites durante el handshake y reintentar.
- Watchdog activo. Tres reinicios seguidos por crash → modo seguro (solo
  web UI), para que un bug no deje el cubo inservible.

## Seguridad

El cubo guarda tokens de la cuenta Claude del usuario y sirve HTTP plano
en la LAN: la web UI lleva contraseña (obligatoria al configurar OAuth) y
los tokens nunca se muestran ni se exportan por la web UI.

## Instalación y recuperación

1. **Backup primero:** descargar el binario stock v9.0.50 del repo oficial
   como red de seguridad.
2. **Instalación en dos pasos sin abrir la caja** (método documentado por
   smalltv-mod): el hueco OTA del stock es pequeño, así que se sube un
   loader por `http://<ip>/update`, el loader abre un AP y por
   `http://192.168.4.1/update` se sube el firmware completo.
3. **Plan B:** UART con esptool (`write_flash 0x0`), abriendo la caja.
4. Tras la primera instalación, las iteraciones van por OTA desde nuestra
   propia web UI.

## Orden de construcción (riesgo primero)

1. **Spike TLS/OAuth en el hardware real.** Sketch mínimo: refresh de
   token + llamada de cuota + lectura de cabeceras, midiendo heap libre.
   Es la única incógnita técnica real; si no cabe, se sabe el día 1.
2. Hello-world de pantalla (pines del Ultra según smalltv-mod).
3. Resto de módulos sobre la base validada.

## Testing

- Lógica pura (parseo de cabeceras de rate limit, máquina de estados del
  humor, countdowns) con tests nativos en host (`pio test -e native`).
- Display, WiFi, OAuth: verificación manual en hardware.

## Fuera de alcance

- Tiempo meteorológico, ticker, radar o cualquier otra pantalla.
- Soporte a otros modelos de GeekMagic (solo SmallTV Ultra).
- Notificaciones push, integración Home Assistant, MQTT.
