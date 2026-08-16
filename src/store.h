#pragma once
#include <LittleFS.h>
#include "config_codec.h"

inline bool loadFile(const char* path, char* buf, size_t n) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  size_t r = f.readBytes(buf, n - 1); buf[r] = 0; f.close();
  return r > 0;
}

inline bool saveFile(const char* path, const char* data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(data); f.close();
  return true;
}

// ponytail: buffer `static` en vez de la pila del handler. En ESP8266 los
// handlers de ESP8266WebServer corren en la pila "cont" (~4KB); Tokens
// (~2.6KB) + char[3072] localmente en cada llamada desbordaba y reiniciaba
// el cubo. Las 4 funciones comparten UN UNICO buffer (en vez de uno cada
// una) porque nunca se anidan: single-threaded (handleClient() procesa una
// request a la vez) y ninguna llama a otra de este fichero. El tamaño es el
// que exige Tokens (3072B; Config solo necesita 768B) — compartir ahorra
// ~4.6KB de .bss frente a un buffer estatico por funcion.
static char s_iobuf[3072];
inline bool loadConfig(Config& c)  { return loadFile("/config.json", s_iobuf, sizeof s_iobuf) && configFromJson(s_iobuf, c); }
inline bool saveConfig(const Config& c) { return configToJson(c, s_iobuf, sizeof s_iobuf) && saveFile("/config.json", s_iobuf); }
inline bool loadTokens(Tokens& t)  { return loadFile("/tokens.json", s_iobuf, sizeof s_iobuf) && tokensFromJson(s_iobuf, t); }
inline bool saveTokens(const Tokens& t) { return tokensToJson(t, s_iobuf, sizeof s_iobuf) && saveFile("/tokens.json", s_iobuf); }
