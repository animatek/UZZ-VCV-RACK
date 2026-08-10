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

// Shared data for UZZ and its CV expander (UZZ-X, attaches to UZZ's left).
// CVs are bipolar offsets added to the corresponding UZZ knob value.
enum UzzXCvIds {
    UZZX_CV_STEPS,   // 1 V = 1 step
    UZZX_CV_START,   // 1 V = 1 step
    UZZX_CV_DIR,     // 1 V = 1 direction mode
    UZZX_CV_ADDR,    // absolute step address: 0-10 V spans the active window
    UZZX_CV_RATIO,   // 1 V = 1 ratio-table index
    UZZX_CV_SWING,   // ±5 V = full swing range
    UZZX_CV_PROB,    // ±10 V = ±100% global probability
    UZZX_CV_ACCUM,   // 1 V = 1 semitone of accumulator amount
    UZZX_NUM_CVS
};

struct UzzExpMsg {
    float cv[UZZX_NUM_CVS] = {};
    bool connected[UZZX_NUM_CVS] = {};
    bool revGate = false;          // momentary direction reverse while high
    uint32_t accumRstCount = 0;    // event counters: UZZ acts on increments
    uint32_t rotFwdCount = 0;      // rotate whole sequence +1 step (wraps)
    uint32_t rotBackCount = 0;     // rotate whole sequence -1 step (wraps)
};

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Adds a "Panel" submenu (Light / Dark) that toggles the global
// settings::preferDarkPanels, so each module can switch the Rack-wide
// theme from its right-click menu.
inline void appendPanelThemeMenu(ui::Menu* menu) {
    menu->addChild(new ui::MenuSeparator);
    menu->addChild(createSubmenuItem("Panel", "", [](ui::Menu* sub) {
        sub->addChild(createCheckMenuItem(
            "Light", "",
            []() { return !settings::preferDarkPanels; },
            []() { settings::preferDarkPanels = false; }));
        sub->addChild(createCheckMenuItem(
            "Dark", "",
            []() { return settings::preferDarkPanels; },
            []() { settings::preferDarkPanels = true; }));
    }));
}

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* modelUZZ;    // UZZ step sequencer
extern Model* modelUzzX;   // UZZ-X CV expander for UZZ
extern Model* modelOxiCv;  // OXI-CV (6HP MIDI-to-CV, Oxi One)
extern Model* modelOxiCvExp; // OXI-CV EXPANSOR
extern Model* modelApc40Ctrl; // APC40 controller CV bridge
extern Model* modelUnitDistanceSeq; // UNIT-D unit-distance graph sequencer
extern Model* modelBlank3; // 3HP blank panel



