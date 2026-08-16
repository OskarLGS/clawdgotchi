// screen_clock.cpp
#include "screen_clock.h"
#include "display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Layout (240x240 display).
static const int TIME_Y = 80;
static const int TIME_CLEAR_H = 60;  // franja para limpiar HH:MM (font 7 es ~48px)
static const int DATE_Y = 170;
static const int DATE_CLEAR_H = 18;   // alto de font 2

// Dias de la semana abreviados en español (sin acentos: "mie" en lugar de "mié").
static const char* const weekdays[7] = {"dom", "lun", "mar", "mie", "jue", "vie", "sab"};

// Meses abreviados en español (sin acentos).
static const char* const months[12] = {
  "ene", "feb", "mar", "abr", "may", "jun",
  "jul", "ago", "sep", "oct", "nov", "dic"
};

static bool s_force = true;

void clockInvalidate() { s_force = true; }

// Estado del ultimo pintado, para no repintar innecesariamente.
struct PaintedState {
  bool valid = false;
  int mday = -1, mon = -1;
  char timeStr[8] = "";     // "HH:MM" o "--:--"; la fecha se compara por mday/mon
};
static PaintedState s_last;

void drawClock(const tm& local) {
  // Guard NTP: si time(nullptr) < 1600000000 (Jan 2020), muestra "--:--".
  time_t now = time(nullptr);
  bool ntpOk = now >= 1600000000;

  // Construye las cadenas de hora y fecha (sin segundos).
  char timeStr[8], dateStr[20];
  if (ntpOk) {
    snprintf(timeStr, sizeof timeStr, "%02d:%02d", local.tm_hour, local.tm_min);
    snprintf(dateStr, sizeof dateStr, "%s %d %s",
             weekdays[local.tm_wday], local.tm_mday, months[local.tm_mon]);
  } else {
    snprintf(timeStr, sizeof timeStr, "--:--");
    snprintf(dateStr, sizeof dateStr, "sin ntp");
  }

  // Calcula si algo cambio (para redibujo eficiente: solo al cambiar el minuto o el dia).
  bool timeChanged = s_force || !s_last.valid || strcmp(s_last.timeStr, timeStr) != 0;
  bool dateChanged = s_force || !s_last.valid || local.tm_mday != s_last.mday || local.tm_mon != s_last.mon;

  if (timeChanged) {
    // Limpia y repinta HH:MM (font 7, centrada, ~48px alto).
    tft.fillRect(0, TIME_Y - TIME_CLEAR_H / 2, 240, TIME_CLEAR_H, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(CC_DATUM);  // CC = center center
    tft.drawString(timeStr, 120, TIME_Y, 7);
  }

  if (dateChanged) {
    // Limpia y repinta fecha (font 2, centrada).
    tft.fillRect(0, DATE_Y - DATE_CLEAR_H / 2, 240, DATE_CLEAR_H, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(CC_DATUM);
    tft.drawString(dateStr, 120, DATE_Y, 2);
  }

  // Actualiza el estado guardado.
  if (timeChanged || dateChanged) {
    s_last.valid = true;
    s_last.mday = local.tm_mday;
    s_last.mon = local.tm_mon;
    snprintf(s_last.timeStr, sizeof s_last.timeStr, "%s", timeStr);
    s_force = false;
  }
}
