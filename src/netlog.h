#pragma once
#include <Arduino.h>
// Log en RAM visible por web: en el Ultra no hay chip USB-serial.
class NetLog {
  static const size_t CAP = 1200;  // ring pequeño: menos heap y menos fragmentacion (el TLS necesita bloque contiguo)
  String buf;
public:
  void line(const String& s) {
    buf += s; buf += '\n';
    if (buf.length() > CAP) buf.remove(0, buf.length() - CAP);
  }
  const String& text() const { return buf; }
};
extern NetLog netlog;
#define LOGN(...) do { char _b[128]; snprintf(_b, sizeof _b, __VA_ARGS__); \
                       netlog.line(String(millis()/1000) + "s " + _b); } while (0)
