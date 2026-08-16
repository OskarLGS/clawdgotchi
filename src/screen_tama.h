// screen_tama.h
#pragma once
#include <stdint.h>
#include "quota.h"

// Dibuja la pantalla tamagotchi (240x240): mascota animada 96x96 arriba,
// barras de cuota 5h/semanal con countdown debajo. La usara T14 (dueño del
// loop principal / cambio de pantallas).
void drawTamagotchi(const QuotaSnapshot& q, Mood m, uint32_t nowEpoch, uint8_t frame);

// Fuerza un repintado completo de barras+texto en la siguiente llamada
// (usar al entrar a esta pantalla, p.ej. tras cambiar de modo).
void tamaInvalidate();
