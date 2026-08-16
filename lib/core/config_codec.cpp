#include "config_codec.h"
#include <ArduinoJson.h>

static void cp(char* dst, size_t n, const char* src) {
  snprintf(dst, n, "%s", src ? src : "");
}

bool configFromJson(const char* json, Config& out) {
  JsonDocument d;
  if (deserializeJson(d, json)) return false;
  out = Config{};
  if (d["ssid"].is<const char*>()) cp(out.wifiSsid, sizeof out.wifiSsid, d["ssid"]);
  if (d["pass"].is<const char*>()) cp(out.wifiPass, sizeof out.wifiPass, d["pass"]);
  if (d["tz"].is<const char*>())   cp(out.tz, sizeof out.tz, d["tz"]);
  out.pollMinutes  = d["poll"] | out.pollMinutes;
  out.brightness   = d["brt"]  | out.brightness;
  out.screenMode   = d["mode"] | out.screenMode;
  out.rotateSeconds= d["rot"]  | out.rotateSeconds;
  if (d["webpass"].is<const char*>()) cp(out.webPass, sizeof out.webPass, d["webpass"]);
  return true;
}

bool configToJson(const Config& c, char* out, size_t n) {
  JsonDocument d;
  d["ssid"] = c.wifiSsid;
  d["pass"] = c.wifiPass;
  d["tz"] = c.tz;
  d["poll"] = c.pollMinutes;
  d["brt"] = c.brightness;
  d["mode"] = c.screenMode;
  d["rot"] = c.rotateSeconds;
  d["webpass"] = c.webPass;
  return serializeJson(d, out, n) > 0;
}

bool tokensFromJson(const char* json, Tokens& out) {
  JsonDocument d;
  if (deserializeJson(d, json)) return false;
  out = Tokens{};
  if (d["access"].is<const char*>()) cp(out.access, sizeof out.access, d["access"]);
  if (d["refresh"].is<const char*>()) cp(out.refresh, sizeof out.refresh, d["refresh"]);
  out.expiresAt = d["exp"] | out.expiresAt;
  return true;
}

bool tokensToJson(const Tokens& t, char* out, size_t n) {
  JsonDocument d;
  d["access"] = t.access;
  d["refresh"] = t.refresh;
  d["exp"] = t.expiresAt;
  return serializeJson(d, out, n) > 0;
}
