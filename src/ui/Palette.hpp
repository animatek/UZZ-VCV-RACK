#pragma once

// The Animatek palette, from animatek.net's style guide (the artifact
// "Animatek — Paleta de colores"). It starts with the three brand colours —the
// primary blue, the secondary orange that is its exact complement, and the
// accent green— and goes on with chip accents. Shared by every module that
// lets a colour be chosen (the blanks' marks, ECLIPSE's ring B), so they all
// offer the same list in the same order.

namespace AnimatekUI {

struct PaletteColor {
    const char* name;
    unsigned char r, g, b;
};

static const PaletteColor PALETTE[] = {
    {"Blue",   0x2C, 0x7F, 0xFF},   // Primary
    {"Orange", 0xFD, 0x9A, 0x00},   // Secondary — complement of the blue
    {"Green",  0x24, 0xB9, 0x79},   // Accent
    {"Purple", 0xC0, 0x84, 0xFC},
    {"Cyan",   0x22, 0xD3, 0xEE},
    {"Rose",   0xFB, 0x71, 0x85},
};
static constexpr int PALETTE_LEN = (int)(sizeof(PALETTE) / sizeof(PALETTE[0]));

} // namespace AnimatekUI
