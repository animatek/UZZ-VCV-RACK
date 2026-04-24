// ============================================================================
// OXI-CV — Animatek Collection
// 6HP MIDI-to-CV bridge for Oxi One (MK1 / MK2)
// Modes: Mono, Poly, Chord, Multitrack, Matriceal
// ============================================================================
#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"
#include <array>

using AnimatekUI::HorizontalSeparator;
using AnimatekUI::TextLabel;
using AnimatekUI::displayBlue;
using AnimatekUI::panelTextColor;

// ============================================================================
// Constants
// ============================================================================
static constexpr int MAX_VOICES = 8;      // Oxi One max polyphony
static constexpr int NUM_CC     = 8;      // always 8 CC outputs

// ============================================================================
// Play Mode
// ============================================================================
enum PlayMode {
    PM_MONO = 0,
    PM_POLY,
    PM_CHORD,
    PM_MULTITRACK,
    PM_MATRICEAL,
    PM_COUNT
};

static const char* const PLAY_MODE_LABELS[PM_COUNT] = {
    "Mono", "Poly", "Chord", "Multitrack", "Matriceal"
};

static const char* const PLAY_MODE_SHORT[PM_COUNT] = {
    "MONO", "POLY", "CHORD", "MULTI", "MATRX"
};

// ============================================================================
// Clock Divisions
// ============================================================================
static constexpr int NUM_DIVS = 6;
static const int   CLK_DIV_VALUES[NUM_DIVS] = { 1, 2, 4, 8, 16, 24 };
static const char* CLK_DIV_LABELS[NUM_DIVS] = { "÷1", "÷2", "÷4", "÷8", "÷16", "÷24" };

// ============================================================================
// Voice Allocator
// ============================================================================
// (VoiceState is now defined in plugin.hpp)

struct VoiceAllocator {
    VoiceState voices[MAX_VOICES];
    int rrIdx = 0;

    VoiceAllocator() { reset(); }

    void reset() {
        for (auto& v : voices) { v.note = -1; v.vel = 0; v.gate = false; }
        rrIdx = 0;
    }

    void noteOn(uint8_t note, uint8_t vel) {
        for (int i = 0; i < MAX_VOICES; i++) {
            if (voices[i].note == (int8_t)note && voices[i].gate) {
                voices[i].vel = vel;
                return;
            }
        }
        for (int i = 0; i < MAX_VOICES; i++) {
            if (!voices[i].gate) {
                voices[i].note = (int8_t)note;
                voices[i].vel  = vel;
                voices[i].gate = true;
                return;
            }
        }
        voices[rrIdx].note = (int8_t)note;
        voices[rrIdx].vel  = vel;
        voices[rrIdx].gate = true;
        rrIdx = (rrIdx + 1) % MAX_VOICES;
    }

    void noteOff(uint8_t note) {
        for (auto& v : voices)
            if (v.note == (int8_t)note) v.gate = false;
    }

    void allNotesOff() {
        for (auto& v : voices) v.gate = false;
    }
};

// ============================================================================
// CC name table
// ============================================================================
static const char* ccName(int cc) {
    switch (cc) {
        case 0:   return "Bank Select MSB";
        case 1:   return "Mod Wheel";
        case 2:   return "Breath";
        case 4:   return "Foot";
        case 5:   return "Portamento Time";
        case 6:   return "Data Entry MSB";
        case 7:   return "Volume";
        case 8:   return "Balance";
        case 10:  return "Pan";
        case 11:  return "Expression";
        case 12:  return "Effect Ctrl 1";
        case 13:  return "Effect Ctrl 2";
        case 32:  return "Bank Select LSB";
        case 33:  return "Mod Wheel LSB";
        case 38:  return "Data Entry LSB";
        case 64:  return "Sustain Pedal";
        case 65:  return "Portamento On/Off";
        case 66:  return "Sostenuto";
        case 67:  return "Soft Pedal";
        case 68:  return "Legato";
        case 69:  return "Hold 2";
        case 70:  return "Sound Variation";
        case 71:  return "Resonance";
        case 72:  return "Amp Release";
        case 73:  return "Amp Attack";
        case 74:  return "Filter Cutoff";
        case 75:  return "Amp Decay";
        case 76:  return "Vibrato Rate";
        case 77:  return "Vibrato Depth";
        case 78:  return "Vibrato Delay";
        case 79:  return "Sound Ctrl 10";
        case 80:  return "General Ctrl 5";
        case 81:  return "General Ctrl 6";
        case 82:  return "General Ctrl 7";
        case 83:  return "General Ctrl 8";
        case 84:  return "Portamento Ctrl";
        case 88:  return "HR Velocity";
        case 91:  return "Reverb Send";
        case 92:  return "Tremolo Depth";
        case 93:  return "Chorus Send";
        case 94:  return "Detune";
        case 95:  return "Phaser Depth";
        case 96:  return "Data Increment";
        case 97:  return "Data Decrement";
        case 98:  return "NRPN LSB";
        case 99:  return "NRPN MSB";
        case 100: return "RPN LSB";
        case 101: return "RPN MSB";
        case 120: return "All Sound Off";
        case 121: return "Reset Controllers";
        case 122: return "Local Control";
        case 123: return "All Notes Off";
        case 124: return "Omni Off";
        case 125: return "Omni On";
        case 126: return "Mono Mode";
        case 127: return "Poly Mode";
        default:  return nullptr;
    }
}

static std::string ccLabel(int cc) {
    const char* name = ccName(cc);
    if (name) return string::f("%d: %s", cc, name);
    return string::f("%d", cc);
}

static const int DEFAULT_CC_NUMBERS[NUM_CC] = { 1, 7, 10, 74, 71, 91, 93, 11 };

// (OxiCvExpMsg is now defined in plugin.hpp)


// ============================================================================
// OxiCv module
// ============================================================================
struct OxiCv : Module {

    enum ParamIds  { NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        VOCT_OUTPUT = 0,
        GATE_OUTPUT,
        VEL_OUTPUT,
        CC1_OUTPUT,
        CC2_OUTPUT,
        CC3_OUTPUT,
        CC4_OUTPUT,
        CC5_OUTPUT,
        CC6_OUTPUT,
        CC7_OUTPUT,
        CC8_OUTPUT,
        CLK_OUTPUT,
        CLKDIV_OUTPUT,
        RUN_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        MIDI_LIGHT = 0,
        NUM_LIGHTS
    };

    // --- MIDI ---
    midi::InputQueue midiInput;

    // --- Config (serialized) ---
    PlayMode playMode  = PM_MONO;
    int      ccNumbers[NUM_CC];
    int      clkDivIdx = 5;      // default ÷24 (= quarter note at 24 PPQN)
    int      pitchBendRange = 2; // ± semitones (1..12)
    bool     chordUnisonGate = false;

    // --- Runtime state ---
    VoiceAllocator allocator;               // for Poly / Chord modes
    VoiceState     tracks[MAX_VOICES];      // for Multitrack / Matriceal modes (indexed by MIDI channel)
    VoiceState     allChannels[16];         // full 16-channel tracking for expanders
    float          ccValues[NUM_CC] = {};

    int8_t  monoNote = 60;
    uint8_t monoVel  = 0;
    bool    monoGate = false;

    bool running = false;
    dsp::PulseGenerator clkPulse;
    dsp::PulseGenerator clkDivPulse;
    int tickCounter = 0;

    float pitchBend = 0.f;       // current bend expressed in V/Oct
    float midiLightLevel = 0.f;  // MIDI activity LED (decays each frame)
    OxiCvExpMsg expanderMsg;     // for right expander

    // -------------------------------------------------------------------------
    OxiCv() {
        for (int i = 0; i < NUM_CC; i++)
            ccNumbers[i] = DEFAULT_CC_NUMBERS[i];

        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configOutput(VOCT_OUTPUT, "V/Oct (Pitch)");
        configOutput(GATE_OUTPUT, "Gate");
        configOutput(VEL_OUTPUT,  "Velocity (0–10 V)");
        for (int i = 0; i < NUM_CC; i++)
            configOutput(CC1_OUTPUT + i, string::f("CC%d", i + 1));
        configOutput(CLK_OUTPUT,    "Clock (24 PPQN tick)");
        configOutput(CLKDIV_OUTPUT, "Clock divided");
        configOutput(RUN_OUTPUT,    "Run / Start-Stop Gate");
    }

    // -------------------------------------------------------------------------
    bool isPolyMode() const {
        return playMode == PM_POLY || playMode == PM_CHORD;
    }

    bool isMultiMode() const {
        return playMode == PM_MULTITRACK || playMode == PM_MATRICEAL;
    }

    int numTracks() const {
        return playMode == PM_MATRICEAL ? 4 : MAX_VOICES;
    }

    void resetTracks() {
        for (auto& t : tracks) { t.note = -1; t.vel = 0; t.gate = false; }
        for (auto& t : allChannels) { t.note = -1; t.vel = 0; t.gate = false; }
    }

    int clkDivValue() const {
        return CLK_DIV_VALUES[clamp(clkDivIdx, 0, NUM_DIVS - 1)];
    }

    void silenceAllVoices() {
        allocator.allNotesOff();
        for (auto& t : tracks)      t.gate = false;
        for (auto& t : allChannels) t.gate = false;
        monoGate = false;
    }

    // -------------------------------------------------------------------------
    void onReset() override {
        midiInput.reset();
        allocator.reset();
        resetTracks();
        monoNote = 60;
        monoVel  = 0;
        monoGate = false;
        running  = false;
        clkPulse.reset();
        clkDivPulse.reset();
        tickCounter = 0;
        pitchBend = 0.f;
        for (int i = 0; i < NUM_CC; i++) ccValues[i] = 0.f;
    }

    // -------------------------------------------------------------------------
    void processMidiMessage(const midi::Message& msg) {
        uint8_t raw   = msg.bytes[0];
        uint8_t data1 = msg.bytes[1];
        uint8_t data2 = msg.bytes[2];

        // Only flash LED for channel messages (Notes, CC, etc.), ignore System Real-Time (Clock/Start/Stop)
        if (raw < 0xF0) {
            midiLightLevel = 1.f;
        }

        // ── System Real-Time ──────────────────────────────────────────────────
        if (raw == 0xF8) {
            clkPulse.trigger(1e-3f);
            if ((tickCounter % clkDivValue()) == 0)
                clkDivPulse.trigger(1e-3f);
            tickCounter++;
            return;
        }
        if (raw == 0xFA) {                         // Start
            running = true;
            tickCounter = 0;
            return;
        }
        if (raw == 0xFB) { running = true; return; }   // Continue
        if (raw == 0xFC) {                              // Stop
            running = false;
            silenceAllVoices();
            return;
        }

        // ── Channel messages ──────────────────────────────────────────────────
        uint8_t type = raw & 0xF0;

        if (type == 0x90 && data2 > 0) {
            int ch = raw & 0x0F;
            // Update all-channel state
            allChannels[ch].note = (int8_t)data1;
            allChannels[ch].vel  = data2;
            allChannels[ch].gate = true;

            if (isMultiMode()) {
                if (ch < numTracks()) {
                    tracks[ch].note = (int8_t)data1;
                    tracks[ch].vel  = data2;
                    tracks[ch].gate = true;
                }
            }
            else if (isPolyMode())
                allocator.noteOn(data1, data2);
            else {
                monoNote = (int8_t)data1;
                monoVel  = data2;
                monoGate = true;
            }
        }
        else if (type == 0x80 || (type == 0x90 && data2 == 0)) {
            int ch = raw & 0x0F;
            if (allChannels[ch].note == (int8_t)data1)
                allChannels[ch].gate = false;

            if (isMultiMode()) {
                if (ch < numTracks() && tracks[ch].note == (int8_t)data1)
                    tracks[ch].gate = false;
            }
            else if (isPolyMode())
                allocator.noteOff(data1);
            else if (monoNote == (int8_t)data1)
                monoGate = false;
        }
        else if (type == 0xE0) {
            // Pitch Bend: 14-bit value, center = 8192
            int raw14 = ((int)data2 << 7) | (int)data1;
            float norm = (raw14 - 8192) / 8192.f;      // [-1, 1)
            pitchBend = norm * (pitchBendRange / 12.f); // V/Oct offset
        }
        else if (type == 0xB0) {
            if (data1 == 123) {
                silenceAllVoices();
            } else {
                for (int i = 0; i < NUM_CC; i++) {
                    if (data1 == (uint8_t)ccNumbers[i])
                        ccValues[i] = (data2 / 127.f) * 10.f;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    void process(const ProcessArgs& args) override {
        midi::Message msg;
        while (midiInput.tryPop(&msg, args.frame))
            processMidiMessage(msg);

        // ── Voice outputs ─────────────────────────────────────────────────────
        if (isMultiMode()) {
            int n = numTracks();
            outputs[VOCT_OUTPUT].setChannels(n);
            outputs[GATE_OUTPUT].setChannels(n);
            outputs[VEL_OUTPUT ].setChannels(n);
            for (int i = 0; i < n; i++) {
                const VoiceState& v = tracks[i];
                outputs[VOCT_OUTPUT].setVoltage(noteToVoct(v.note) + pitchBend, i);
                outputs[GATE_OUTPUT].setVoltage(v.gate ? 10.f : 0.f,  i);
                outputs[VEL_OUTPUT ].setVoltage((v.vel / 127.f) * 10.f, i);
            }
        }
        else if (isPolyMode()) {
            outputs[VOCT_OUTPUT].setChannels(MAX_VOICES);
            outputs[GATE_OUTPUT].setChannels(MAX_VOICES);
            outputs[VEL_OUTPUT ].setChannels(MAX_VOICES);

            // Chord unison gate: all gates follow the OR of any active voice
            bool unison = (playMode == PM_CHORD && chordUnisonGate);
            bool anyGate = false;
            if (unison) {
                for (auto& v : allocator.voices) if (v.gate) { anyGate = true; break; }
            }

            for (int i = 0; i < MAX_VOICES; i++) {
                const VoiceState& v = allocator.voices[i];
                outputs[VOCT_OUTPUT].setVoltage(noteToVoct(v.note) + pitchBend, i);
                outputs[GATE_OUTPUT].setVoltage((unison ? anyGate : v.gate) ? 10.f : 0.f, i);
                outputs[VEL_OUTPUT ].setVoltage((v.vel / 127.f) * 10.f, i);
            }
        } else {
            outputs[VOCT_OUTPUT].setChannels(1);
            outputs[GATE_OUTPUT].setChannels(1);
            outputs[VEL_OUTPUT ].setChannels(1);
            outputs[VOCT_OUTPUT].setVoltage((monoNote - 60) / 12.f + pitchBend);
            outputs[GATE_OUTPUT].setVoltage(monoGate ? 10.f : 0.f);
            outputs[VEL_OUTPUT ].setVoltage((monoVel / 127.f) * 10.f);
        }

        // ── CC outputs ────────────────────────────────────────────────────────
        for (int i = 0; i < NUM_CC; i++)
            outputs[CC1_OUTPUT + i].setVoltage(ccValues[i]);

        // ── Sync outputs ──────────────────────────────────────────────────────
        outputs[CLK_OUTPUT   ].setVoltage(clkPulse   .process(args.sampleTime) ? 10.f : 0.f);
        outputs[CLKDIV_OUTPUT].setVoltage(clkDivPulse.process(args.sampleTime) ? 10.f : 0.f);
        outputs[RUN_OUTPUT   ].setVoltage(running ? 10.f : 0.f);

        // ── MIDI activity LED (decay ~100 ms) ─────────────────────────────────
        midiLightLevel = std::max(0.f, midiLightLevel - args.sampleTime * 10.f);
        lights[MIDI_LIGHT].setBrightness(midiLightLevel);

        // ── Expander ──────────────────────────────────────────────────────────
        if (rightExpander.module && rightExpander.module->model == modelOxiCvExp) {
            memcpy(expanderMsg.channels, allChannels, sizeof(allChannels));
            expanderMsg.pitchBend = pitchBend;
            rightExpander.module->leftExpander.consumerMessage = &expanderMsg;
        }
    }

    // -------------------------------------------------------------------------
    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "playMode",       json_integer(playMode));
        json_object_set_new(root, "clkDivIdx",      json_integer(clkDivIdx));
        json_object_set_new(root, "pitchBendRange", json_integer(pitchBendRange));
        json_object_set_new(root, "chordUnisonGate", json_boolean(chordUnisonGate));

        json_t* ccArr = json_array();
        for (int i = 0; i < NUM_CC; i++)
            json_array_append_new(ccArr, json_integer(ccNumbers[i]));
        json_object_set_new(root, "ccNumbers", ccArr);

        json_object_set_new(root, "midiInput", midiInput.toJson());
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root) return;

        if (json_t* j = json_object_get(root, "playMode"))
            playMode = (PlayMode)clamp((int)json_integer_value(j), 0, (int)PM_COUNT - 1);

        if (json_t* j = json_object_get(root, "clkDivIdx"))
            clkDivIdx = clamp((int)json_integer_value(j), 0, NUM_DIVS - 1);

        if (json_t* j = json_object_get(root, "pitchBendRange"))
            pitchBendRange = clamp((int)json_integer_value(j), 1, 12);

        if (json_t* j = json_object_get(root, "chordUnisonGate"))
            chordUnisonGate = json_is_true(j);

        if (json_t* j = json_object_get(root, "ccNumbers")) {
            for (int i = 0; i < NUM_CC; i++) {
                json_t* el = json_array_get(j, i);
                if (el) ccNumbers[i] = clamp((int)json_integer_value(el), 0, 127);
            }
        }

        if (json_t* j = json_object_get(root, "midiInput"))
            midiInput.fromJson(j);
    }
};


// ============================================================================
// Context menu
// ============================================================================
static void appendOxiContextMenu(ui::Menu* menu, OxiCv* m) {
    if (!m) return;

    // ── Play Mode ─────────────────────────────────────────────────────────────
    menu->addChild(new MenuSeparator);
    menu->addChild(createSubmenuItem(
        "Play Mode", PLAY_MODE_LABELS[m->playMode],
        [m](ui::Menu* sub) {
            for (int i = 0; i < PM_COUNT; i++) {
                int mode = i;
                sub->addChild(createCheckMenuItem(
                    PLAY_MODE_LABELS[i], "",
                    [m, mode]() { return m->playMode == (PlayMode)mode; },
                    [m, mode]() {
                        m->playMode = (PlayMode)mode;
                        m->allocator.reset();
                        m->resetTracks();
                        m->monoGate = false;
                        // Multi/Matriceal modes demux by MIDI channel → omni input
                        if (m->playMode == PM_MULTITRACK || m->playMode == PM_MATRICEAL)
                            m->midiInput.channel = -1;
                    }
                ));
            }
        }
    ));

    // ── Chord unison gate ─────────────────────────────────────────────────────
    menu->addChild(createCheckMenuItem(
        "Chord: unison gate", "",
        [m]() { return m->chordUnisonGate; },
        [m]() { m->chordUnisonGate = !m->chordUnisonGate; }
    ));

    // ── Pitch Bend Range ──────────────────────────────────────────────────────
    menu->addChild(createSubmenuItem(
        "Pitch Bend Range", string::f("± %d st", m->pitchBendRange),
        [m](ui::Menu* sub) {
            for (int st = 1; st <= 12; st++) {
                int n = st;
                sub->addChild(createCheckMenuItem(
                    string::f("± %d semitone%s", n, n == 1 ? "" : "s"), "",
                    [m, n]() { return m->pitchBendRange == n; },
                    [m, n]() { m->pitchBendRange = n; }
                ));
            }
        }
    ));

    // ── Clock Division ────────────────────────────────────────────────────────
    menu->addChild(createSubmenuItem(
        "CLK/n Division", CLK_DIV_LABELS[clamp(m->clkDivIdx, 0, NUM_DIVS - 1)],
        [m](ui::Menu* sub) {
            for (int i = 0; i < NUM_DIVS; i++) {
                int idx = i;
                sub->addChild(createCheckMenuItem(
                    CLK_DIV_LABELS[i], "",
                    [m, idx]() { return m->clkDivIdx == idx; },
                    [m, idx]() { m->clkDivIdx = idx; m->tickCounter = 0; }
                ));
            }
        }
    ));

    // ── CC assignments (grouped 0–127 in 16-banks) ────────────────────────────
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel("CC Assignments"));

    for (int i = 0; i < NUM_CC; i++) {
        int idx = i;
        menu->addChild(createSubmenuItem(
            string::f("CC%d", i + 1), ccLabel(m->ccNumbers[i]),
            [m, idx](ui::Menu* sub) {
                for (int base = 0; base < 128; base += 16) {
                    int lo = base, hi = base + 15;
                    sub->addChild(createSubmenuItem(
                        string::f("%d–%d", lo, hi), "",
                        [m, idx, lo, hi](ui::Menu* sub2) {
                            for (int n = lo; n <= hi; n++) {
                                int num = n;
                                sub2->addChild(createCheckMenuItem(
                                    ccLabel(n), "",
                                    [m, idx, num]() { return m->ccNumbers[idx] == num; },
                                    [m, idx, num]() { m->ccNumbers[idx] = num; }
                                ));
                            }
                        }
                    ));
                }
            }
        ));
    }
}

// ============================================================================
// Custom UI widgets
// ============================================================================

using StaticLabel = TextLabel;

struct DynamicModeLabel : widget::TransparentWidget {
    OxiCv* module;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        std::shared_ptr<Font> font = APP->window->uiFont;
        if (!font) return;
        const char* modeStr = (module && module->playMode < PM_COUNT)
            ? PLAY_MODE_SHORT[module->playMode] : "MONO";
        std::string topTxt = std::string("OXI | ") + modeStr;

        int ch = module ? module->midiInput.channel : -1;
        std::string botTxt = (ch < 0) ? "CHANNEL OMNI"
                                      : string::f("CHANNEL %d", ch + 1);

        float cx = box.size.x / 2.f;

        nvgFontFaceId(args.vg, font->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        // Top line — mode (large lighter corporate blue, simulated bold)
        nvgFontSize(args.vg, 13.0f);
        nvgTextLetterSpacing(args.vg, 1.2f);
        nvgFillColor(args.vg, displayBlue()); // Lighter Corporate Blue
        nvgText(args.vg, cx, box.size.y * 0.38f, topTxt.c_str(), NULL);
        nvgText(args.vg, cx + 0.2f, box.size.y * 0.38f, topTxt.c_str(), NULL); // Fake bold

        // Bottom line — MIDI channel (small, dimmer lighter corporate blue, simulated bold)
        nvgFontSize(args.vg, 7.0f);
        nvgTextLetterSpacing(args.vg, 0.6f);
        nvgFillColor(args.vg, displayBlue(210));
        nvgText(args.vg, cx, box.size.y * 0.78f, botTxt.c_str(), NULL);
        nvgText(args.vg, cx + 0.15f, box.size.y * 0.78f, botTxt.c_str(), NULL); // Fake bold

        // MIDI Activity dot to the left of Channel text
        float light = module ? module->lights[OxiCv::MIDI_LIGHT].getBrightness() : 0.f;
        if (light > 0.01f) {
            float bounds[4];
            nvgTextBounds(args.vg, 0, 0, botTxt.c_str(), NULL, bounds);
            float txtW = bounds[2] - bounds[0];
            float dotX = cx - (txtW / 2.f) - 4.0f;
            float dotY = box.size.y * 0.78f;

            nvgBeginPath(args.vg);
            nvgCircle(args.vg, dotX, dotY, 1.8f);
            nvgFillColor(args.vg, displayBlue((uint8_t)(255 * light)));
            nvgFill(args.vg);

            // Subtle glow
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, dotX, dotY, 3.0f);
            nvgFillColor(args.vg, displayBlue((uint8_t)(80 * light)));
            nvgFill(args.vg);
        }

        nvgTextLetterSpacing(args.vg, 0.f);
    }
};

struct DynamicCcLabel : widget::TransparentWidget {
    OxiCv* module;
    int ccIndex;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        std::shared_ptr<Font> font = APP->window->uiFont;
        if (!font) return;
        int ccNum = module ? module->ccNumbers[ccIndex] : 0;
        std::string txt = string::f("CC%d · %d", ccIndex + 1, ccNum);

        nvgFontSize(args.vg, 9.0f);
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, panelTextColor());
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        for (auto & c: txt) c = toupper((unsigned char)c);
        nvgText(args.vg, box.size.x / 2.f, box.size.y, txt.c_str(), NULL);
        nvgText(args.vg, box.size.x / 2.f + 0.15f, box.size.y, txt.c_str(), NULL); // Fake bold
    }
};

struct DynamicClkDivLabel : widget::TransparentWidget {
    OxiCv* module;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        std::shared_ptr<Font> font = APP->window->uiFont;
        if (!font) return;
        int idx = module ? clamp(module->clkDivIdx, 0, NUM_DIVS - 1) : 0;
        std::string txt = std::string("CLK") + CLK_DIV_LABELS[idx];

        nvgFontSize(args.vg, 9.0f);
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, panelTextColor());
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        for (auto & c: txt) c = toupper((unsigned char)c);
        nvgText(args.vg, box.size.x / 2.f, box.size.y, txt.c_str(), NULL);
        nvgText(args.vg, box.size.x / 2.f + 0.15f, box.size.y, txt.c_str(), NULL); // Fake bold
    }
};

using SeparatorLine = HorizontalSeparator;


// ============================================================================
// OxiCvWidget — 6HP panel
// ============================================================================
struct OxiCvWidget : ModuleWidget {
    OxiCvWidget(OxiCv* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/OxiCv-light.svg"),
            asset::plugin(pluginInstance, "res/OxiCv.svg")));

        // Panel is 8HP = 40.64 mm wide (see res/OxiCv.svg)
        static constexpr float LX = 12.0f;
        static constexpr float RX = 28.64f;

        // Retro console display (top)
        auto dynMode = createWidget<DynamicModeLabel>(mm2px(Vec(3.0f, 3.5f)));
        dynMode->box.size = mm2px(Vec(34.64f, 11.0f));
        dynMode->module = module;
        addChild(dynMode);

        float y = 17.0f;

        // Row layout (total 13.0 mm):
        //   +0.4  separator (1 mm)
        //   +1.5  label box (3 mm) — baseline at +4.5, with fontSize 7.5
        //   +9.5  port center (top edge +5.4, so ~0.9 mm clear of label)
        auto addGroupSeparator = [&]() {
            y += 5.0f;   // ≈10px breathing room above separator
            auto sep = createWidget<SeparatorLine>(mm2px(Vec(4.0f, y)));
            sep->box.size = mm2px(Vec(32.64f, 1.0f));
            addChild(sep);
            y += 3.0f;   // line height + breathing room below
        };



        // Voice row: V/OCT | Gate | Vel — three ports side by side
        auto addVoiceRow = [&]() {
            static constexpr float TX1 = 8.0f, TX2 = 20.32f, TX3 = 32.64f;
            constexpr float LBLW = 11.0f;
            float yLbl = y + 1.5f;

            auto lblV = createWidget<StaticLabel>(mm2px(Vec(TX1 - LBLW / 2.f, yLbl)));
            lblV->box.size = mm2px(Vec(LBLW, 3.0f));
            lblV->text = "V/OCT";
            addChild(lblV);

            auto lblG = createWidget<StaticLabel>(mm2px(Vec(TX2 - LBLW / 2.f, yLbl)));
            lblG->box.size = mm2px(Vec(LBLW, 3.0f));
            lblG->text = "GATE";
            addChild(lblG);

            auto lblVl = createWidget<StaticLabel>(mm2px(Vec(TX3 - LBLW / 2.f, yLbl)));
            lblVl->box.size = mm2px(Vec(LBLW, 3.0f));
            lblVl->text = "VEL";
            addChild(lblVl);

            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TX1, y + 9.5f)), module, OxiCv::VOCT_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TX2, y + 9.5f)), module, OxiCv::GATE_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TX3, y + 9.5f)), module, OxiCv::VEL_OUTPUT));
            y += 13.0f;
        };

        // isDynCc = 0: both static; 1: both dynamic CC; 2: dynamic Clock + CLK/n
        auto addDualRow = [&](const std::string& nameL, int outL,
                              const std::string& nameR, int outR,
                              int mode, int ccL = -1, int ccR = -1) {
            float yLbl = y + 1.5f;
            constexpr float LBLW = 16.64f;
            float xLblL = LX - LBLW / 2.f;
            float xLblR = RX - LBLW / 2.f;

            if (mode == 1) {
                auto lblL = createWidget<DynamicCcLabel>(mm2px(Vec(xLblL, yLbl)));
                lblL->box.size = mm2px(Vec(LBLW, 3.0f));
                lblL->module = module;
                lblL->ccIndex = ccL;
                addChild(lblL);

                auto lblR = createWidget<DynamicCcLabel>(mm2px(Vec(xLblR, yLbl)));
                lblR->box.size = mm2px(Vec(LBLW, 3.0f));
                lblR->module = module;
                lblR->ccIndex = ccR;
                addChild(lblR);
            } else {
                auto lblL = createWidget<StaticLabel>(mm2px(Vec(xLblL, yLbl)));
                lblL->box.size = mm2px(Vec(LBLW, 3.0f));
                lblL->text = nameL;
                addChild(lblL);

                auto lblR = createWidget<StaticLabel>(mm2px(Vec(xLblR, yLbl)));
                lblR->box.size = mm2px(Vec(LBLW, 3.0f));
                lblR->text = nameR;
                addChild(lblR);
            }

            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(LX, y + 9.5f)), module, outL));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(RX, y + 9.5f)), module, outR));
            y += 14.0f;
        };

        // Sync row: 3 ports side by side (Clock | CLK÷n | Run)
        auto addSyncRow = [&]() {
            static constexpr float TX1 = 8.0f, TX2 = 20.32f, TX3 = 32.64f;

            float yLbl = y + 1.5f;
            constexpr float LBLW = 11.0f;

            auto lblL = createWidget<StaticLabel>(mm2px(Vec(TX1 - LBLW / 2.f, yLbl)));
            lblL->box.size = mm2px(Vec(LBLW, 3.0f));
            lblL->text = "CLOCK";
            addChild(lblL);

            auto lblC = createWidget<DynamicClkDivLabel>(mm2px(Vec(TX2 - LBLW / 2.f, yLbl)));
            lblC->box.size = mm2px(Vec(LBLW, 3.0f));
            lblC->module = module;
            addChild(lblC);

            auto lblR = createWidget<StaticLabel>(mm2px(Vec(TX3 - LBLW / 2.f, yLbl)));
            lblR->box.size = mm2px(Vec(LBLW, 3.0f));
            lblR->text = "RUN";
            addChild(lblR);

            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TX1, y + 9.5f)), module, OxiCv::CLK_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TX2, y + 9.5f)), module, OxiCv::CLKDIV_OUTPUT));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(TX3, y + 9.5f)), module, OxiCv::RUN_OUTPUT));
            y += 13.0f;
        };

        addVoiceRow();

        addGroupSeparator();
        addDualRow("", OxiCv::CC1_OUTPUT, "", OxiCv::CC2_OUTPUT, 1, 0, 1);
        addDualRow("", OxiCv::CC3_OUTPUT, "", OxiCv::CC4_OUTPUT, 1, 2, 3);
        addDualRow("", OxiCv::CC5_OUTPUT, "", OxiCv::CC6_OUTPUT, 1, 4, 5);
        addDualRow("", OxiCv::CC7_OUTPUT, "", OxiCv::CC8_OUTPUT, 1, 6, 7);

        addGroupSeparator();
        addSyncRow();
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* m = dynamic_cast<OxiCv*>(module);
        if (m) {
            menu->addChild(new MenuSeparator);
            app::appendMidiMenu(menu, &m->midiInput);
        }
        appendOxiContextMenu(menu, m);
    }
};


// ============================================================================
// Model registration
// ============================================================================
Model* modelOxiCv = createModel<OxiCv, OxiCvWidget>("OxiCv");
