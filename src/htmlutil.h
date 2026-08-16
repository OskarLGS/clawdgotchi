#pragma once
#include <Arduino.h>

// Escapa texto de usuario para insertarlo seguro en HTML: & < > " y tambien %
// (este ultimo para que un valor de usuario no pueda formar un placeholder
// %XX% y filtrar otro campo en un String::replace() encadenado tipo
// FPSTR(...).replace("%A%",...).replace("%B%",...)).
inline String htmlEscape(const String& in) {
  String s = in;
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("%", "&#37;");
  return s;
}
