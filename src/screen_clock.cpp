// screen_clock.cpp
#include "screen_clock.h"
#include "display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// HH:MM centrada en la pantalla (240x240), en FreeSansBold24pt (GFX sans-serif,
// grande). FreeSansBold24pt7b lo declara TFT_eSPI al estar LOAD_GFXFF activo.
static const int TIME_Y = 120;        // centro vertical

// Anti-retencion de imagen ("burn-in"): cada minuto la hora se dibuja desplazada
// unos pixeles siguiendo una orbita lenta, para no fijar siempre los mismos
// pixeles. Vuelta completa cada 8 minutos.
static const int8_t ORBIT_X[8] = {0, 4, 8, 4, 0, -4, -8, -4};
static const int8_t ORBIT_Y[8] = {-8, -4, 0, 4, 8, 4, 0, -4};

static bool s_force = true;

void clockInvalidate() { s_force = true; }

struct PaintedState {
  bool valid = false;
  char timeStr[8] = "";     // "HH:MM" o "--:--"
};
static PaintedState s_last;

void drawClock(const tm& local) {
  // Guard NTP: si time(nullptr) < 1600000000 (Jan 2020), muestra "--:--".
  time_t now = time(nullptr);
  bool ntpOk = now >= 1600000000;

  char timeStr[8];
  if (ntpOk) snprintf(timeStr, sizeof timeStr, "%02d:%02d", local.tm_hour, local.tm_min);
  else       snprintf(timeStr, sizeof timeStr, "--:--");

  // Redibuja solo al cambiar el minuto (y con ello avanza la orbita antiburn-in).
  bool changed = s_force || !s_last.valid || strcmp(s_last.timeStr, timeStr) != 0;
  if (changed) {
    int i = ntpOk ? (local.tm_hour * 60 + local.tm_min) % 8 : 0;
    // Limpia una franja amplia (cubre toda la orbita) y dibuja desplazado.
    tft.fillRect(0, TIME_Y - 60, 240, 120, TFT_BLACK);
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.setTextSize(2);           // x2: "HH:MM" ocupa casi todo el ancho
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(CC_DATUM);   // centro-centro
    tft.drawString(timeStr, 120 + ORBIT_X[i], TIME_Y + ORBIT_Y[i]);
    tft.setTextSize(1);           // restaura escala y fuente por defecto para el resto
    tft.setTextFont(2);
    tft.setTextDatum(TL_DATUM);
    s_last.valid = true;
    snprintf(s_last.timeStr, sizeof s_last.timeStr, "%s", timeStr);
    s_force = false;
  }
}
