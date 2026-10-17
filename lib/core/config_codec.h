#pragma once
#include <cstdint>
#include <cstddef>

struct Config {
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  char tz[48] = "CET-1CEST,M3.5.0,M10.5.0/3";   // POSIX TZ, por defecto España peninsular
  char ntp[48] = "pool.ntp.org";                 // servidor NTP
  uint16_t pollMinutes = 5;
  uint8_t brightness = 80;                       // 0..100
  uint8_t screenMode = 0;                        // 0=rotación 1=tamagotchi 2=reloj
  uint16_t rotateSeconds = 15;
  uint16_t fadeMs = 300;                         // duración total del fade entre pantallas (0 = instantáneo)
  char webPass[33] = "";                         // "" = sin password (hasta guardar token)
  char model[48] = "claude-haiku-4-5";           // modelo de la peticion minima del poll
};

struct Tokens {
  char access[2048] = "";
  char refresh[512] = "";
  uint32_t expiresAt = 0;
};

// pure (native-testable): JSON <-> struct
bool configToJson(const Config&, char* out, size_t n);
bool configFromJson(const char* json, Config& out);
bool tokensToJson(const Tokens&, char* out, size_t n);
bool tokensFromJson(const char* json, Tokens& out);
