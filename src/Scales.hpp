#pragma once

#include <array>

// Scales shared by the pitch-making modules (UNIT-D, ECLIPSE). The order is
// saved in patches as an index, so new scales go at the end. Natural minor
// stays first: it was UNIT-D's only scale before the selector existed, so it
// is what older patches load into.

namespace AnimatekScales {

struct Scale {
    const char* name;
    int size;
    std::array<int, 12> degrees;
    const char* shortName;   // for small displays
};

static const Scale SCALES[] = {
    {"Minor", 7, {0, 2, 3, 5, 7, 8, 10}, "MIN"},
    {"Major", 7, {0, 2, 4, 5, 7, 9, 11}, "MAJ"},
    {"Dorian", 7, {0, 2, 3, 5, 7, 9, 10}, "DOR"},
    {"Phrygian", 7, {0, 1, 3, 5, 7, 8, 10}, "PHR"},
    {"Lydian", 7, {0, 2, 4, 6, 7, 9, 11}, "LYD"},
    {"Mixolydian", 7, {0, 2, 4, 5, 7, 9, 10}, "MIX"},
    {"Harmonic minor", 7, {0, 2, 3, 5, 7, 8, 11}, "HMIN"},
    {"Minor pentatonic", 5, {0, 3, 5, 7, 10}, "mPENT"},
    {"Major pentatonic", 5, {0, 2, 4, 7, 9}, "MPENT"},
    {"Blues", 6, {0, 3, 5, 6, 7, 10}, "BLUES"},
    {"Chromatic", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, "CHROM"},
};
static constexpr int NUM_SCALES = sizeof(SCALES) / sizeof(SCALES[0]);

static const char* const ROOT_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

} // namespace AnimatekScales
