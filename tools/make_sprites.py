#!/usr/bin/env python3
"""Genera el pixel-art de Clawd (la mascota) y src/sprites.h.

Dibuja en una rejilla logica de 24x24 y escala x4 (nearest-neighbour) a
96x96, que es lo que se guarda en assets/*.png y se vuelca a sprites.h.

Uso: python3 tools/make_sprites.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS_DIR = os.path.join(ROOT, "assets")
SPRITES_H = os.path.join(ROOT, "src", "sprites.h")

GRID = 24
SCALE = 4
SIZE = GRID * SCALE  # 96

# --- paleta -----------------------------------------------------------
BASE = (0xD9, 0x77, 0x57)     # #D97757 naranja Clawd
SHADOW = (0xB8, 0x5C, 0x3E)   # #B85C3E
HILITE = (0xE8, 0x9B, 0x7D)   # #E89B7D

GRAY_BASE = (0x88, 0x88, 0x88)
GRAY_SHADOW = (0x5A, 0x5A, 0x5A)
GRAY_HILITE = (0xAD, 0xAD, 0xAD)

INK = (0x3D, 0x26, 0x1B)      # ojos/boca, marron muy oscuro (no negro puro)
SWEAT = (0x6E, 0xBE, 0xE6)    # gotas de sudor
ZZZ = (0xEB, 0xEB, 0xEB)      # texto Zzz
GRAY_INK = (0x40, 0x40, 0x40) # ojos/boca en modo offline

BG = None  # celda vacia -> fondo negro puro (color-key de transparencia)


# --- forma del cuerpo --------------------------------------------------
# Semi-anchura (en pixeles logicos) por fila, centrada entre las columnas
# 11 y 12 (rejilla de 24 columnas). Da una gota/lagrima redondeada.
BODY_HALF_WIDTH = {
    4: 1, 5: 3, 6: 5, 7: 6, 8: 7, 9: 8, 10: 8, 11: 9, 12: 9, 13: 9,
    14: 9, 15: 8, 16: 8, 17: 7, 18: 6, 19: 5,
}
# Patitas tipo pinza: dos muñones bajo el cuerpo, mas juntos en la fila
# inferior para sugerir una pinza.
LEG_ROWS = {
    20: [(8, 10), (13, 15)],
    21: [(8, 9), (14, 15)],
}


def build_mask():
    """Devuelve un set de (row, col) con las celdas que son "cuerpo"."""
    cells = set()
    for row, half in BODY_HALF_WIDTH.items():
        for col in range(12 - half, 12 + half):
            cells.add((row, col))
    for row, spans in LEG_ROWS.items():
        for c0, c1 in spans:
            for col in range(c0, c1 + 1):
                cells.add((row, col))
    return cells


MASK = build_mask()

# Zona (arriba-izquierda del cuerpo) donde se talla el parche de luz.
HILITE_ROWS = range(6, 12)
HILITE_COLS = range(6, 12)


def shade_body(base, shadow, hilite):
    """Cuerpo base + contorno de sombra + parche de luz arriba-izq."""
    canvas = [[BG] * GRID for _ in range(GRID)]
    for (row, col) in MASK:
        neighbours = ((row - 1, col), (row + 1, col), (row, col - 1), (row, col + 1))
        on_edge = any(n not in MASK for n in neighbours)
        if on_edge:
            canvas[row][col] = shadow
        elif row in HILITE_ROWS and col in HILITE_COLS:
            canvas[row][col] = hilite
        else:
            canvas[row][col] = base
    return canvas


def new_body():
    return shade_body(BASE, SHADOW, HILITE)


def new_body_gray():
    return shade_body(GRAY_BASE, GRAY_SHADOW, GRAY_HILITE)


# --- helpers de dibujo --------------------------------------------------
def paint(canvas, coords, color):
    for row, col in coords:
        if 0 <= row < GRID and 0 <= col < GRID:
            canvas[row][col] = color


def paint_rect(canvas, r0, r1, c0, c1, color):
    paint(canvas, ((r, c) for r in range(r0, r1 + 1) for c in range(c0, c1 + 1)), color)


def shift(canvas, dr, dc):
    """Traslada todo el sprite dr filas / dc columnas (bordes -> fondo)."""
    out = [[BG] * GRID for _ in range(GRID)]
    for row in range(GRID):
        for col in range(GRID):
            sr, sc = row - dr, col - dc
            if 0 <= sr < GRID and 0 <= sc < GRID:
                out[row][col] = canvas[sr][sc]
    return out


def eyes_open(canvas, color=INK):
    paint_rect(canvas, 10, 11, 8, 9, color)
    paint_rect(canvas, 10, 11, 14, 15, color)


def eyes_closed(canvas, color=INK):
    paint(canvas, [(11, 8), (11, 9)], color)
    paint(canvas, [(11, 14), (11, 15)], color)


def zzz_glyph(canvas, r, c, color):
    """Una "Z" de 3x3 logicos con esquina superior-izq en (r, c)."""
    paint(canvas, [(r, c), (r, c + 1), (r, c + 2)], color)      # barra arriba
    paint(canvas, [(r + 1, c + 1)], color)                       # diagonal
    paint(canvas, [(r + 2, c), (r + 2, c + 1), (r + 2, c + 2)], color)  # barra abajo


# --- una funcion "draw" por humor: (frame) -> canvas 24x24 -------------
def draw_happy(frame):
    c = new_body()
    # ojos "^ ^" felices
    paint(c, [(10, 8), (9, 9), (10, 10)], INK)
    paint(c, [(10, 14), (9, 15), (10, 16)], INK)
    # sonrisa: comisuras arriba, centro abajo
    paint(c, [(13, 10), (13, 13)], INK)
    paint(c, [(14, 11), (14, 12)], INK)
    if frame == 1:
        c = shift(c, dr=-1, dc=0)  # rebote hacia arriba
    return c


def draw_uneasy(frame):
    c = new_body()
    if frame == 0:
        eyes_open(c)
    else:
        eyes_closed(c)  # parpadeo
    paint(c, [(14, 10), (14, 11), (14, 12), (14, 13)], INK)  # boca recta
    drop_row = 6 if frame == 0 else 7  # una gota que resbala
    paint(c, [(drop_row, 18), (drop_row + 1, 18), (drop_row + 1, 19)], SWEAT)
    return c


def draw_stressed(frame):
    c = new_body()
    # cejas preocupadas, inclinadas hacia el centro
    paint(c, [(8, 7), (9, 8)], INK)
    paint(c, [(8, 17), (9, 16)], INK)
    eyes_open(c)
    # boca ondulada (zigzag)
    paint(c, [(14, 10), (13, 11), (14, 12), (13, 13)], INK)
    drop_row = 6 if frame == 0 else 7  # dos gotas cayendo
    paint(c, [(drop_row, 5), (drop_row + 1, 5), (drop_row + 1, 6)], SWEAT)
    paint(c, [(drop_row, 18), (drop_row + 1, 18), (drop_row + 1, 19)], SWEAT)
    return c


def draw_panic(frame):
    c = new_body()
    # ojos muy abiertos (3x3) con brillo interior
    paint_rect(c, 9, 11, 7, 9, INK)
    paint_rect(c, 9, 11, 14, 16, INK)
    paint(c, [(9, 8)], HILITE)
    paint(c, [(9, 15)], HILITE)
    # boca abierta (ovalo relleno)
    paint_rect(c, 13, 15, 10, 13, INK)
    # gotas volando lejos del cuerpo
    paint(c, [(7, 2), (8, 2)], SWEAT)
    paint(c, [(6, 21), (7, 21)], SWEAT)
    paint(c, [(3, 12)], SWEAT)
    if frame == 1:
        c = shift(c, dr=0, dc=-1)  # temblor lateral
    return c


def draw_sleeping(frame):
    c = new_body()
    eyes_closed(c)
    paint(c, [(14, 11), (14, 12)], INK)  # boca neutra
    if frame == 0:
        zzz_glyph(c, 0, 15, ZZZ)
        zzz_glyph(c, 2, 19, ZZZ)
    else:
        zzz_glyph(c, 1, 16, ZZZ)  # Zzz "flotando" (desplazada)
        zzz_glyph(c, 3, 20, ZZZ)
    return c


def draw_offline(frame):
    c = new_body_gray()
    if frame == 0:
        # ojos en X (apagados)
        paint(c, [(9, 7), (10, 8), (11, 9)], GRAY_INK)
        paint(c, [(9, 9), (10, 8), (11, 7)], GRAY_INK)
        paint(c, [(9, 13), (10, 14), (11, 15)], GRAY_INK)
        paint(c, [(9, 15), (10, 14), (11, 13)], GRAY_INK)
    else:
        eyes_closed(c, color=GRAY_INK)  # parpadeo tenue
    paint(c, [(14, 11), (14, 12)], GRAY_INK)
    return c


MOODS = [
    ("HAPPY", draw_happy),
    ("UNEASY", draw_uneasy),
    ("STRESSED", draw_stressed),
    ("PANIC", draw_panic),
    ("SLEEPING", draw_sleeping),
    ("OFFLINE", draw_offline),
]


def rgb565(rgb):
    r, g, b = rgb
    value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    if value == 0:
        value = 0x0841  # nunca devolver la clave de transparencia
    return value


def verify_no_pure_black(canvas, name):
    for row in range(GRID):
        for col in range(GRID):
            px = canvas[row][col]
            if px is not None:
                assert px != (0, 0, 0), f"pixel negro puro en {name} ({row},{col})"


def upscale(canvas):
    """24x24 (con None = fondo) -> lista 96x96 de tuplas RGB u None (fondo).

    None se conserva (no se convierte a (0,0,0) todavia) para que el fondo
    siga siendo distinguible del cuerpo hasta el punto en que cada uno se
    cuantiza a su valor final (PNG vs RGB565).
    """
    out = []
    for row in range(SIZE):
        src_row = canvas[row // SCALE]
        line = [src_row[col // SCALE] for col in range(SIZE)]
        out.append(line)
    return out


def save_png(pixels96, path):
    from PIL import Image
    img = Image.new("RGB", (SIZE, SIZE), (0, 0, 0))
    for row in range(SIZE):
        for col in range(SIZE):
            px = pixels96[row][col]
            img.putpixel((col, row), px if px is not None else (0, 0, 0))
    img.save(path)


def pixel_rgb565(px):
    """Fondo (None) -> clave de transparencia 0x0000 literal. Cuerpo -> rgb565
    (con el bump anti-negro, que nunca debe pisar la clave de fondo)."""
    return 0x0000 if px is None else rgb565(px)


def verify_transparency_key(pixels96, name):
    """Assert sobre el artefacto final: el fondo (>=1 pixel) emite 0x0000 y
    ningun pixel de cuerpo lo hace (si lo hiciera, se volveria invisible)."""
    values = [pixel_rgb565(px) for row in pixels96 for px in row]
    assert 0x0000 in values, f"{name}: no quedo ningun pixel de fondo (0x0000)"
    for px, v in zip((px for row in pixels96 for px in row), values):
        if px is not None:
            assert v != 0x0000, f"{name}: un pixel de cuerpo emite la clave de fondo"


def emit_array(f, name, pixels96):
    f.write(f"const uint16_t {name}[{SIZE * SIZE}] PROGMEM = {{\n")
    values = [f"0x{pixel_rgb565(px):04X}" for row in pixels96 for px in row]
    for i in range(0, len(values), 16):
        f.write("  " + ",".join(values[i:i + 16]) + ",\n")
    f.write("};\n\n")


def main():
    os.makedirs(ASSETS_DIR, exist_ok=True)
    sprites = {}  # (mood, frame) -> pixels96
    for mood, draw in MOODS:
        for frame in (0, 1):
            canvas = draw(frame)
            verify_no_pure_black(canvas, f"{mood}_{frame}")
            pixels96 = upscale(canvas)
            verify_transparency_key(pixels96, f"{mood}_{frame}")
            sprites[(mood, frame)] = pixels96
            png_path = os.path.join(ASSETS_DIR, f"sprite_{mood.lower()}_{frame}.png")
            save_png(pixels96, png_path)
            print(f"wrote {png_path}")

    with open(SPRITES_H, "w") as f:
        f.write("// sprites.h\n")
        f.write("// GENERADO por tools/make_sprites.py — no editar a mano\n")
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n")
        f.write("#ifdef ARDUINO\n#include <pgmspace.h>\n#else\n#define PROGMEM\n#endif\n")
        f.write('#include "quota.h"\n\n')
        f.write(
            "// Empaquetado RGB565 estandar: value = (r5<<11)|(g6<<5)|b5.\n"
            "// El ESP8266 es little-endian, asi que antes de tft.pushImage()\n"
            "// hay que llamar a tft.setSwapBytes(true) para que TFT_eSPI\n"
            "// intercambie los bytes al orden que espera el bus SPI del panel.\n"
            "// El fondo (fuera de la mascota) es la clave de transparencia 0x0000.\n"
            "// ponytail: los arrays son `const` a nivel de fichero -> linkage interno,\n"
            "// asi que cada .cpp que incluya este header se lleva su propia copia\n"
            "// (~180 KB) de las 12 tablas. Vale para el TU unico de hoy (sprites.cpp);\n"
            "// si T12 necesita usarlas desde otro .cpp, declarar `extern` aqui y mover\n"
            "// las definiciones a sprites.cpp para no duplicar flash.\n\n"
        )
        for mood, _ in MOODS:
            for frame in (0, 1):
                emit_array(f, f"SPRITE_{mood}_{frame}", sprites[(mood, frame)])

        # __attribute__((used)): sin esto, como nadie llama a mascotFrame()
        # todavia (T12 lo hara), el enlazador (--gc-sections) tira la funcion
        # Y los 12 arrays PROGMEM que referencia, dejando el build igual de
        # "pequeño" que antes de esta tarea sin avisar de nada. Con el
        # atributo, el binario de T11 refleja el coste de Flash real.
        f.write("__attribute__((used))\n")
        f.write("inline const uint16_t* mascotFrame(Mood m, uint8_t frame) {\n")
        f.write("  frame = frame % 2;\n")
        f.write("  switch (m) {\n")
        for mood, _ in MOODS:
            f.write(f"    case Mood::{mood}:\n")
            f.write(f"      return frame ? SPRITE_{mood}_1 : SPRITE_{mood}_0;\n")
        f.write("  }\n")
        f.write("  return SPRITE_OFFLINE_0;\n")
        f.write("}\n")

    print(f"wrote {SPRITES_H}")


if __name__ == "__main__":
    main()
