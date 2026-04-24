#pragma once
#include <rack.hpp>


using namespace rack;

// Shared data structures for OXI-CV and its expander
struct VoiceState {
    int8_t  note = -1;
    uint8_t vel  = 0;
    bool    gate = false;
};

struct OxiCvExpMsg {
    VoiceState channels[16];
    float pitchBend = 0.f;
};

// Converts a MIDI note (int8_t, -1 = inactive) to V/Oct relative to C4 (60)
static inline float noteToVoct(int8_t note) {
    return (note >= 0) ? (note - 60) / 12.f : 0.f;
}

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* modelUZZ;    // UZZ step sequencer
extern Model* modelOxiCv;  // OXI-CV (6HP MIDI-to-CV, Oxi One)
extern Model* modelOxiCvExp; // OXI-CV EXPANSOR





