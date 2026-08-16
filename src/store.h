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

inline bool loadConfig(Config& c)  { char b[768];  return loadFile("/config.json", b, sizeof b) && configFromJson(b, c); }
inline bool saveConfig(const Config& c) { char b[768];  return configToJson(c, b, sizeof b) && saveFile("/config.json", b); }
inline bool loadTokens(Tokens& t)  { char b[3072]; return loadFile("/tokens.json", b, sizeof b) && tokensFromJson(b, t); }
inline bool saveTokens(const Tokens& t) { char b[3072]; return tokensToJson(t, b, sizeof b) && saveFile("/tokens.json", b); }
