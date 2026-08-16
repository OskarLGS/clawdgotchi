#pragma once
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
// /update acepta un firmware.bin nuevo: la puerta de salida permanente.
inline ESP8266HTTPUpdateServer otaUpdater;
inline void otaBegin(ESP8266WebServer& server) { otaUpdater.setup(&server, "/update"); }
