#pragma once
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

void displayBegin(uint8_t brightnessPct);
void displayBrightness(uint8_t pct);
void displayFade(uint8_t fromPct, uint8_t toPct, uint16_t durationMs);  // rampa del backlight en durationMs (0=directo)
