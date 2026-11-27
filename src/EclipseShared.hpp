#pragma once

#include "plugin.hpp"
#include "ui/Palette.hpp"

// What ECLIPSE and its pitch expander (ECLIPSE PITCH, glued to its right) share.
// All the sequencing lives in ECLIPSE, the lock included, because a lock has to
// rewind the rings too. The expander only holds the note controls and jacks: it
// sends their values to ECLIPSE, and ECLIPSE sends back the note to output.
// Both travel as Rack expander messages, one sample late each way.

namespace EclipseShared {

static constexpr int NUM_GATES = 5;   // A, B, AND, OR, XOR

// How the note is chosen each time the source gate fires.
enum NoteMode { MODE_RANDOM, MODE_ACUM, NUM_MODES };
static const char* const MODE_TEXT[NUM_MODES] = {"RND", "ACUM"};
static const char* const SOURCE_TEXT[NUM_GATES] = {"A", "B", "AND", "OR", "XOR"};

// Ring B's colour is one of the Animatek palette, chosen per module, so two
// ECLIPSEs in a patch can be told apart. Blue is ring A's and is not offered.
static constexpr int FIRST_B_COLOR = 1;     // Orange
static constexpr int DEFAULT_B_COLOR = 1;

inline NVGcolor paletteColor(int idx) {
    if (idx < FIRST_B_COLOR || idx >= AnimatekUI::PALETTE_LEN)
        idx = DEFAULT_B_COLOR;
    const AnimatekUI::PaletteColor& c = AnimatekUI::PALETTE[idx];
    return nvgRGB(c.r, c.g, c.b);
}

/** Expander → ECLIPSE: the note controls, already read and rounded. */
struct PitchSettings {
    bool attached = false;
    int mode = MODE_ACUM;
    int source = 3;          // OR
    int range = 2;           // octaves
    int octave = 0;
    int scale = 0;
    int rootSemitones = 0;   // KEY plus ROOT IN
    int stepA = 2;
    int stepB = -1;
    bool lock = false;
    int loop = 16;
};

/** ECLIPSE → expander: the note to put out, and what its display shows. */
struct PitchOutput {
    bool linked = false;
    float voct = 0.f;
    float root = 0.f;
    int colorB = DEFAULT_B_COLOR;
    int colorA = -1;   // ECLIPSE's own colour (ring A), which the expander takes
    bool locked = false;
    // For the note graph: counts every note the source gate picks (repeats
    // included), with its scale degree, out of `span` degrees of `scaleSize`.
    uint32_t noteCount = 0;
    int degree = 0;
    int span = 14;
    int scaleSize = 7;
};

} // namespace EclipseShared
