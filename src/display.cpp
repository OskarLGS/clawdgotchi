#include "display.h"
#include "board.h"

TFT_eSPI tft;

void displayBegin(uint8_t pct) {
  pinMode(PIN_TFT_BL, OUTPUT);
  analogWriteRange(1023);
  displayBrightness(pct);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
}

void displayBrightness(uint8_t pct) {
  analogWrite(PIN_TFT_BL, BL_ON_DUTY(pct));
}

// Rampa suave del backlight de un brillo a otro en durationMs. Bloquea el
// loop ese tiempo: aceptable para una transicion puntual. durationMs=0 -> salto
// directo.
void displayFade(uint8_t fromPct, uint8_t toPct, uint16_t durationMs) {
  if (durationMs == 0) { displayBrightness(toPct); return; }
  const int STEP_MS = 12;
  int steps = durationMs / STEP_MS;
  if (steps < 1) steps = 1;
  for (int i = 0; i <= steps; i++) {
    int pct = fromPct + (toPct - fromPct) * i / steps;
    analogWrite(PIN_TFT_BL, BL_ON_DUTY(pct < 0 ? 0 : pct));
    delay(STEP_MS);
  }
}
