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

// Rampa suave del backlight de un brillo a otro (~ms*pasos de duracion).
// Bloquea el loop unas centesimas: aceptable para una transicion puntual.
void displayFade(uint8_t fromPct, uint8_t toPct) {
  const int STEPS = 16;
  for (int i = 0; i <= STEPS; i++) {
    int pct = fromPct + (toPct - fromPct) * i / STEPS;
    analogWrite(PIN_TFT_BL, BL_ON_DUTY(pct < 0 ? 0 : pct));
    delay(12);
  }
}
