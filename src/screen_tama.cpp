// screen_tama.cpp
#include "screen_tama.h"
#include "display.h"
#include "mascot.h"
#include <stdio.h>
#include <string.h>

// Layout (fijado en el brief de la task 12, no tocar sin actualizar el brief).
static const int MASCOT_X = 72, MASCOT_Y = 18, MASCOT_SIZE = 96;
static const int MASCOT_CLEAR_Y = 16, MASCOT_CLEAR_H = 100;  // franja para el rebote ±2px
static const int BAR_X = 20, BAR_W = 200, BAR_H = 18;
static const int BAR_5H_Y = 140, BAR_WK_Y = 185;
static const int LABEL_H = 16;                    // alto de font 2
static const int ROW_CLEAR_W = 240 - BAR_X;        // hasta el borde derecho (x=240)

static bool s_force = true;

void tamaInvalidate() { s_force = true; }

static uint16_t colorForPct(uint8_t pct) {
  if (pct >= 90) return TFT_RED;
  if (pct >= 75) return TFT_ORANGE;
  if (pct >= 50) return TFT_YELLOW;
  return TFT_GREEN;
}

// 'line' ya viene formateada (etiqueta+countdown, o el "despierta en X" de
// SLEEPING) por el caller; aquí solo se pinta.
static void drawBar(int y, const char* line, uint8_t pct, bool ok) {
  tft.fillRect(BAR_X, y - LABEL_H, ROW_CLEAR_W, LABEL_H + BAR_H, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(line, BAR_X, y - LABEL_H, 2);

  tft.drawRect(BAR_X, y, BAR_W, BAR_H, TFT_DARKGREY);
  if (ok) {
    int fillW = (int)((BAR_W - 2) * (pct > 100 ? 100 : pct) / 100);
    if (fillW > 0) tft.fillRect(BAR_X + 1, y + 1, fillW, BAR_H - 2, colorForPct(pct));
    char pctbuf[8];
    snprintf(pctbuf, sizeof pctbuf, "%u%%", pct);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(pctbuf, BAR_X + BAR_W + 4, y + 2, 2);
  }
}

// Copia del último estado pintado, para no repintar barras/texto cada 500ms
// (solo la mascota se repinta siempre, por la animación).
struct PaintedState {
  bool valid = false;
  uint8_t pct5h = 0xFF, pctWeek = 0xFF;  // valores imposibles -> fuerza 1er paint
  Mood mood = (Mood)0xFF;
  char line5h[40] = "";
  char lineWk[40] = "";
};
static PaintedState s_last;

void drawTamagotchi(const QuotaSnapshot& q, Mood m, uint32_t nowEpoch, uint8_t frame) {
  // --- mascota: siempre se repinta (anima cada llamada) ---
  int8_t bounce = (frame & 1) ? -2 : 2;
  tft.fillRect(MASCOT_X, MASCOT_CLEAR_Y, MASCOT_SIZE, MASCOT_CLEAR_H, TFT_BLACK);
  tft.setSwapBytes(true);
  tft.pushImage(MASCOT_X, MASCOT_Y + bounce, MASCOT_SIZE, MASCOT_SIZE, mascotFrame(m, frame), 0x0000);
  tft.setSwapBytes(false);

  bool sleeping = (m == Mood::SLEEPING);
  bool offline = (m == Mood::OFFLINE);

  // --- construye las líneas de etiqueta+countdown de hoy ---
  // Guard NTP: si el reloj local aun no sincronizo (epoca ~1970), reset5h/
  // resetWeek son epochs reales pero "nowEpoch" no lo es -> formatCountdown
  // pintaria basura tipo "20000d Xh". Se trata igual que !q.valid.
  bool ntpOk = nowEpoch >= 1600000000;
  char cd5h[24], cdWk[24];
  if (q.valid && ntpOk) {
    if (sleeping && q.pct5h >= 100) {
      char c[16]; formatCountdown(nowEpoch, q.reset5h, c, sizeof c);
      snprintf(cd5h, sizeof cd5h, "despierta en %s", c);
    } else {
      char c[16]; formatCountdown(nowEpoch, q.reset5h, c, sizeof c);
      snprintf(cd5h, sizeof cd5h, "5h  %s", c);
    }
    if (sleeping && q.pctWeek >= 100) {
      char c[16]; formatCountdown(nowEpoch, q.resetWeek, c, sizeof c);
      snprintf(cdWk, sizeof cdWk, "despierta en %s", c);
    } else {
      char c[16]; formatCountdown(nowEpoch, q.resetWeek, c, sizeof c);
      snprintf(cdWk, sizeof cdWk, "semana  %s", c);
    }
  } else {
    snprintf(cd5h, sizeof cd5h, "5h  --");
    snprintf(cdWk, sizeof cdWk, "semana  --");
  }

  bool changed = s_force || !s_last.valid || s_last.mood != m ||
                 s_last.pct5h != q.pct5h || s_last.pctWeek != q.pctWeek ||
                 strcmp(s_last.line5h, cd5h) != 0 || strcmp(s_last.lineWk, cdWk) != 0;

  if (changed) {
    drawBar(BAR_5H_Y, cd5h, q.pct5h, q.valid);
    drawBar(BAR_WK_Y, cdWk, q.pctWeek, q.valid);

    // texto "sin conexión" bajo la mascota, en la franja libre encima de las barras
    tft.fillRect(0, MASCOT_CLEAR_Y + MASCOT_CLEAR_H, 240, BAR_5H_Y - LABEL_H - (MASCOT_CLEAR_Y + MASCOT_CLEAR_H), TFT_BLACK);
    if (offline) {
      // solo 8px libres entre la mascota (fin y=116) y la etiqueta de la
      // barra 5h (empieza y=124): font 1 (6x8) es el único que cabe ahí.
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("sin conexion", 120, MASCOT_CLEAR_Y + MASCOT_CLEAR_H, 1);
      tft.setTextDatum(TL_DATUM);
    }

    s_last.valid = true;
    s_last.mood = m;
    s_last.pct5h = q.pct5h;
    s_last.pctWeek = q.pctWeek;
    snprintf(s_last.line5h, sizeof s_last.line5h, "%s", cd5h);
    snprintf(s_last.lineWk, sizeof s_last.lineWk, "%s", cdWk);
    s_force = false;
  }
}
