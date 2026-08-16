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
