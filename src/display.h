#pragma once
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

void displayBegin(uint8_t brightnessPct);
void displayBrightness(uint8_t pct);
