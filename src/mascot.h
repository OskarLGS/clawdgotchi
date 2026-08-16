// mascot.h
// Prototipo de mascotFrame() SIN arrastrar los arrays de sprites.h.
//
// sprites.h define los 12 arrays PROGMEM (~216 KB) con linkage interno
// (const a nivel de fichero) mas un mascotFrame() `inline` marcado
// __attribute__((used)) para forzar su emision aunque sprites.cpp (su
// unico includer) no lo llame. Ese `used` es justo lo que permite que
// OTRO .cpp declare el prototipo aqui y enlace contra el simbolo ya
// emitido por sprites.cpp, sin volver a incluir sprites.h (que
// duplicaria los ~216 KB de arrays en cada TU que lo incluyera).
//
// screen_tama.cpp incluye este header en vez de sprites.h.
#pragma once
#include <stdint.h>
#include "quota.h"

const uint16_t* mascotFrame(Mood m, uint8_t frame);
