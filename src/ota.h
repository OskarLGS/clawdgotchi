#pragma once
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include "config_codec.h"
// /update acepta un firmware.bin nuevo: la puerta de salida permanente.
// Password aplicado con cfg.webPass al arrancar; /cfg lo actualiza en caliente
// con otaUpdater.updateCredentials() al cambiarlo (webui.h).
inline ESP8266HTTPUpdateServer otaUpdater;
inline void otaBegin(ESP8266WebServer& server, const Config& cfg) {
  if (cfg.webPass[0]) otaUpdater.setup(&server, "/update", "admin", cfg.webPass);
  else otaUpdater.setup(&server, "/update");
}
