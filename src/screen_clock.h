// screen_clock.h
#pragma once
#include <ctime>

// Dibuja la pantalla reloj (240x240): hora HH:MM centrada (font 7, 48px),
// segundos (font 2) a la derecha, fecha en español "sab 16 ago" (font 2) centrada.
// Redibujo eficiente: solo segundos cada segundo, HH:MM cuando cambie minuto,
// fecha cuando cambie día. clockInvalidate() fuerza repintado completo.
// Guard NTP: si time(nullptr)<1600000000, muestra "--:--".
void drawClock(const tm& local);

// Fuerza un repintado completo en la siguiente llamada (usar al entrar a esta
// pantalla, p.ej. tras cambiar de modo).
void clockInvalidate();
