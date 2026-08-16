// screen_tama.cpp
#include "screen_tama.h"
#include "display.h"
#include "mascot.h"
// FreeSans9pt7b lo declara TFT_eSPI al estar LOAD_GFXFF activo (no re-incluir).
#include <stdio.h>
#include <string.h>

static const int MASCOT_X = 72, MASCOT_Y = 18, MASCOT_SIZE = 96;
static const int MASCOT_CLEAR_Y = 16, MASCOT_CLEAR_H = 100;  // franja para el rebote ±2px
// Cada "bloque" de cuota = fila de etiqueta ("5h (2h 57m)" a la izq, "18%" a la
// derecha) + barra a lo ancho maximo debajo. Coordenadas relativas al top del
// bloque para que la fuente GFX (mas alta que Font 2) no se corte por arriba.
static const int MARGIN_X = 10;
static const int BAR_W = 240 - 2 * MARGIN_X;      // 220: barra a lo ancho
static const int BAR_H = 14;
static const int BLOCK1_Y = 132, BLOCK2_Y = 182;  // top de cada bloque (menos hueco entre barras)
static const int LABEL_BASE = 17;                 // baseline de la etiqueta (rel. al top)
static const int BAR_TOP = 22;                    // top de la barra (rel. al top)
static const int BLOCK_H = 38;                    // alto a limpiar por bloque

static bool s_force = true;

void tamaInvalidate() { s_force = true; }

static uint16_t colorForPct(uint8_t pct) {
  if (pct >= 90) return TFT_RED;
  if (pct >= 75) return TFT_ORANGE;
  if (pct >= 50) return TFT_YELLOW;
  return TFT_GREEN;
}

// 'line' ya viene formateada (etiqueta+countdown) por el caller. 'top' es el
// top del bloque; aquí se pinta la fila de texto + la barra debajo.
static void drawBar(int top, const char* line, uint8_t pct, bool ok) {
  tft.fillRect(0, top, 240, BLOCK_H, TFT_BLACK);
  tft.setFreeFont(&FreeMono9pt7b);           // etiqueta y % en monoespaciada
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // etiqueta a la izquierda (baseline)
  tft.setTextDatum(BL_DATUM);
  tft.drawString(line, MARGIN_X, top + LABEL_BASE);
  // % a la derecha del todo
  if (ok) {
    char pctbuf[8];
    snprintf(pctbuf, sizeof pctbuf, "%u%%", pct);
    tft.setTextDatum(BR_DATUM);
    tft.drawString(pctbuf, 240 - MARGIN_X, top + LABEL_BASE);
  }
  // barra a lo ancho maximo, debajo de la fila de texto
  int by = top + BAR_TOP;
  tft.drawRect(MARGIN_X, by, BAR_W, BAR_H, TFT_DARKGREY);
  if (ok) {
    int fillW = (int)((BAR_W - 2) * (pct > 100 ? 100 : pct) / 100);
    if (fillW > 0) tft.fillRect(MARGIN_X + 1, by + 1, fillW, BAR_H - 2, colorForPct(pct));
  }
  tft.setTextFont(2);          // restaura fuente por defecto para el resto
  tft.setTextDatum(TL_DATUM);
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
  // Antiparpadeo: se dibuja OPACA (su fondo 0x0000 es negro, igual que la
  // pantalla) para que sobrescriba el frame anterior en una sola pasada, sin el
  // borrado-a-negro previo que causaba el parpadeo. Solo se limpia la franja de
  // ~2px que el rebote deja fuera del sprite (arriba o abajo).
  int8_t bounce = (frame & 1) ? -2 : 2;
  int my = MASCOT_Y + bounce;
  int clrTop = MASCOT_CLEAR_Y, clrBot = MASCOT_CLEAR_Y + MASCOT_CLEAR_H;
  if (my > clrTop) tft.fillRect(MASCOT_X, clrTop, MASCOT_SIZE, my - clrTop, TFT_BLACK);
  if (my + MASCOT_SIZE < clrBot) tft.fillRect(MASCOT_X, my + MASCOT_SIZE, MASCOT_SIZE, clrBot - (my + MASCOT_SIZE), TFT_BLACK);
  tft.setSwapBytes(true);
  tft.pushImage(MASCOT_X, my, MASCOT_SIZE, MASCOT_SIZE, mascotFrame(m, frame));
  tft.setSwapBytes(false);

  bool sleeping = (m == Mood::SLEEPING);
  bool offline = (m == Mood::OFFLINE);

  // --- construye las líneas de etiqueta+countdown de hoy ---
  // Guard NTP: si el reloj local aun no sincronizo (epoca ~1970), reset5h/
  // resetWeek son epochs reales pero "nowEpoch" no lo es -> formatCountdown
  // pintaria basura tipo "20000d Xh". Se trata igual que !q.valid.
  bool ntpOk = nowEpoch >= 1600000000;
  char cd5h[28], cdWk[28];
  if (q.valid && ntpOk) {
    char c[16];
    formatCountdown(nowEpoch, q.reset5h, c, sizeof c);
    if (sleeping) snprintf(cd5h, sizeof cd5h, "despierta en %s", c);
    else          snprintf(cd5h, sizeof cd5h, "5h (%s)", c);
    formatCountdown(nowEpoch, q.resetWeek, c, sizeof c);
    if (sleeping) snprintf(cdWk, sizeof cdWk, "despierta en %s", c);
    else          snprintf(cdWk, sizeof cdWk, "Semanal (%s)", c);
  } else {
    snprintf(cd5h, sizeof cd5h, "5h  --");
    snprintf(cdWk, sizeof cdWk, "Semanal  --");
  }

  bool changed = s_force || !s_last.valid || s_last.mood != m ||
                 s_last.pct5h != q.pct5h || s_last.pctWeek != q.pctWeek ||
                 strcmp(s_last.line5h, cd5h) != 0 || strcmp(s_last.lineWk, cdWk) != 0;

  if (changed) {
    // Limpia el hueco entre la mascota y el bloque 1 ANTES de las barras: si se
    // hace despues, este fillRect repinta de negro las astas altas de la
    // etiqueta "5h" (que suben por encima del top del bloque) -> texto cortado.
    tft.fillRect(0, MASCOT_CLEAR_Y + MASCOT_CLEAR_H, 240, BLOCK1_Y - (MASCOT_CLEAR_Y + MASCOT_CLEAR_H), TFT_BLACK);
    if (offline) {
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("sin conexion", 120, MASCOT_CLEAR_Y + MASCOT_CLEAR_H, 1);
      tft.setTextDatum(TL_DATUM);
    }

    drawBar(BLOCK1_Y, cd5h, q.pct5h, q.valid);
    drawBar(BLOCK2_Y, cdWk, q.pctWeek, q.valid);

    s_last.valid = true;
    s_last.mood = m;
    s_last.pct5h = q.pct5h;
    s_last.pctWeek = q.pctWeek;
    snprintf(s_last.line5h, sizeof s_last.line5h, "%s", cd5h);
    snprintf(s_last.lineWk, sizeof s_last.lineWk, "%s", cdWk);
    s_force = false;
  }
}
