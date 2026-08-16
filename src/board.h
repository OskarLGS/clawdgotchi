// board.h — GeekMagic SmallTV Ultra (ESP-12F + ST7789 240x240)
#pragma once
#define PIN_TFT_BL 5           // backlight, PWM, ACTIVO-BAJO
#define BL_ON_DUTY(pct) (1023 - ((pct) * 1023 / 100))  // 100% brillo -> duty 0
