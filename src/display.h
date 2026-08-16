#pragma once
#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern TFT_eSprite strip;

void displayBegin(uint8_t brightnessPct);
void displayBrightness(uint8_t pct);
