#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"
#include "FilterTekTables.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

using AnimatekUI::ConnectorLine;
using AnimatekUI::TekInputPort;
using AnimatekUI::TekOutputPort;
using AnimatekUI::TextLabel;

// ============================================================================
// FILTERtek - the multimode filter of a classic 90s virtual-analogue modular
// ============================================================================
//
// Modelled on measurements, not on anybody's code. What the recordings show:
//
// - A Chamberlin state-variable filter: one section gives LP, BP and HP at
//   12 dB/octave; 24 dB is two identical sections in cascade, not a ladder.
//   It matches the recordings to 0.02 dB and a degree of phase across the
//   whole cutoff range.
// - Band reject is a notch (HP + LP) with a damping of its own, far gentler
//   than the resonance: its width hardly follows the RES knob.
// - Cutoff 330 Hz * 2^((step - 60) / 12), one semitone per knob step, and
//   f = 2 sin(pi fc / 96 kHz): the original runs at 96 kHz, so this module
//   steps the filter at (close to) that rate whatever Rack's rate is.
// - The damping follows the cutoff: q = q0(resonance) * (1 - f/2), q0 from a
//   measured table. Q runs from 0.5 to several thousand at full resonance,
//   where it rings for most of a second without quite self-oscillating.
// - Gain control lowers the level as the resonance rises (to -40 dB), so the
//   peak stays in check; off, the passband stays at unity. At 12 dB it acts
//   on the input, at 24 dB between the two sections, so the first one takes
//   the full signal and saturates sooner: that is what the recordings of a
//   driven 24 dB filter only match with. Band reject gets (1 + g) / 2.
// - Fixed-point arithmetic: lp, bp and hp each saturate at full scale when
//   stored. That hard edge is where the filter's character under resonance
//   comes from, and it reproduces the measured gain and the third harmonic
//   of every output to within a fraction of a dB.
// ============================================================================

namespace filtertek {

// The original's internal full scale. Its oscillators peak at a quarter of
// it, so an ordinary +-5 V oscillator here sits where one sits there, and
// the filter saturates where the original would.
static constexpr float FULL_SCALE_V = 20.f;
static constexpr float G1_RATE = 96000.f;

enum FilterType { TYPE_LP, TYPE_BP, TYPE_HP, TYPE_BR, NUM_TYPES };

/** A per-step table read at a continuous step, interpolated in log so a
knob between steps moves smoothly. */
static float readLogTable(const float* table, float step) {
    step = clamp(step, 0.f, 127.f);
    int i = std::min((int)step, 126);
    float t = step - (float)i;
    return table[i] * std::pow(table[i + 1] / table[i], t);
}

static float readLinTable(const float* table, float step) {
    step = clamp(step, 0.f, 127.f);
    int i = std::min((int)step, 126);
    float t = step - (float)i;
    return table[i] + (table[i + 1] - table[i]) * t;
}

static float cutoffHz(float step) {
    return 330.f * std::pow(2.f, (clamp(step, 0.f, 127.f) - 60.f) / 12.f);
}

// The filter really sits 0.125% (about 2 cents) under the editor's figure, at
// every step from 48 up where it can be measured to that precision. It only
// shows near a sharp peak, where the phase depends on it.
static constexpr float CUTOFF_TRIM = 0.99876f;

static inline float sat(float v) {
    return clamp(v, -1.f, 1.f);
}

/** One Chamberlin section, saturating every stored value as fixed point does. */
struct Section {
    float lp = 0.f;
    float bp = 0.f;

    float process(float x, float f, float q, int type) {
        lp = sat(lp + f * bp);
        float hp = sat(x - lp - q * bp);
        bp = sat(bp + f * hp);
        switch (type) {
            case TYPE_BP: return bp;
            case TYPE_HP: return hp;
            case TYPE_BR: return sat(hp + lp);
            default: return lp;
        }
    }
};

struct CutoffQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        float hz = cutoffHz(getValue());
        return hz < 1000.f ? string::f("%.1f Hz", hz) : string::f("%.2f kHz", hz / 1000.f);
    }
};

} // namespace filtertek

using namespace filtertek;


struct FilterTek : Module {
    // Not released yet, so the order is free; once it is, append only.
    enum ParamId { FREQ_PARAM, RES_PARAM, CUT_CV_PARAM, RES_CV_PARAM, TYPE_PARAM, SLOPE_PARAM,
                   GAIN_CONTROL_PARAM, PARAMS_LEN };
    enum InputId { IN_L_INPUT, IN_R_INPUT, VOCT_INPUT, CUT_CV_INPUT, RES_CV_INPUT, INPUTS_LEN };
    enum OutputId { OUT_L_OUTPUT, OUT_R_OUTPUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    // [side][channel][section]: the two sides are two filters with the same settings.
    Section sections[2][16][2];

    // What the response display draws: the first channel's cutoff step and
    // resonance, CV included. Written by the audio thread, read by the UI; a
    // torn float is one wrong frame of a curve at worst.
    float displayStep = 60.f;
    float displayRes = 0.f;

    FilterTek() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<CutoffQuantity>(FREQ_PARAM, 0.f, 127.f, 60.f, "Cutoff");
        configParam(RES_PARAM, 0.f, 127.f, 0.f, "Resonance");
        // Bipolar, 100% being the original's amount 127: two semitones per
        // twelfth of a volt, so 50% is 1 V/octave.
        configParam(CUT_CV_PARAM, -1.f, 1.f, 0.f, "Cutoff CV amount", "%", 0.f, 100.f);
        configParam(RES_CV_PARAM, -1.f, 1.f, 0.f, "Resonance CV amount", "%", 0.f, 100.f);
        configSwitch(TYPE_PARAM, 0.f, 3.f, 0.f, "Type", {"Lowpass", "Bandpass", "Highpass", "Band reject"});
        configSwitch(SLOPE_PARAM, 0.f, 1.f, 1.f, "Slope", {"12 dB/oct", "24 dB/oct"});
        configSwitch(GAIN_CONTROL_PARAM, 0.f, 1.f, 1.f, "Gain control", {"Off", "On"});

        configInput(IN_L_INPUT, "Audio left");
        configInput(IN_R_INPUT, "Audio right (normalled to left)");
        configInput(VOCT_INPUT, "Cutoff V/oct");
        configInput(CUT_CV_INPUT, "Cutoff CV (1 V = 2 octaves at full amount)");
        configInput(RES_CV_INPUT, "Resonance CV (1 V = 24 steps at full amount)");
        configOutput(OUT_L_OUTPUT, "Audio left");
        configOutput(OUT_R_OUTPUT, "Audio right");
        configBypass(IN_L_INPUT, OUT_L_OUTPUT);
        configBypass(IN_R_INPUT, OUT_R_OUTPUT);
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        for (auto& side : sections)
            for (auto& ch : side)
                ch[0] = ch[1] = Section();
    }

    void process(const ProcessArgs& args) override {
        const int type = clamp((int)std::round(params[TYPE_PARAM].getValue()), 0, NUM_TYPES - 1);
        const bool slope24 = params[SLOPE_PARAM].getValue() > 0.5f;
        const bool gainControl = params[GAIN_CONTROL_PARAM].getValue() > 0.5f;

        // Steps per Rack sample that bring the filter to about 96 kHz: two at
        // 48 kHz, one at 96. The input is held across them and the outputs
        // averaged, which is all the original's own converters do.
        const int over = std::max(1, (int)std::round(G1_RATE / args.sampleRate));
        const float rate = args.sampleRate * (float)over;

        const bool leftPatched = inputs[IN_L_INPUT].isConnected();
        const bool rightPatched = inputs[IN_R_INPUT].isConnected();
        const int channels = std::max(1, std::max(inputs[IN_L_INPUT].getChannels(),
                                                  inputs[IN_R_INPUT].getChannels()));
        // Right is normalled to left; with nothing in at all, both sides idle.
        const bool anyIn = leftPatched || rightPatched;

        const float freqKnob = params[FREQ_PARAM].getValue();
        const float resKnob = params[RES_PARAM].getValue();
        const float cutAmount = params[CUT_CV_PARAM].getValue();
        const float resAmount = params[RES_CV_PARAM].getValue();

        for (int c = 0; c < channels; c++) {
            // The original counts modulation in units, twelve to the octave
            // on the cutoff; one volt here is twelve units. Full amount
            // doubles them, as the original's amount 127 does.
            float step = freqKnob + inputs[VOCT_INPUT].getPolyVoltage(c) * 12.f
                         + inputs[CUT_CV_INPUT].getPolyVoltage(c) * 12.f * 2.f * cutAmount;
            float res = clamp(resKnob + inputs[RES_CV_INPUT].getPolyVoltage(c) * 12.f * 2.f * resAmount,
                              0.f, 127.f);
            if (c == 0) {
                displayStep = clamp(step, 0.f, 127.f);
                displayRes = res;
            }

            float f = 2.f * std::sin((float)M_PI * std::min(cutoffHz(step) * CUTOFF_TRIM, 0.45f * rate) / rate);
            const float* qTable = (type == TYPE_BR) ? (slope24 ? BR_Q24 : BR_Q12)
                                                    : (slope24 ? Q24 : Q12);
            float q = readLogTable(qTable, res) * (1.f - 0.5f * f);

            float g = 1.f;
            if (gainControl) {
                g = std::pow(10.f, readLinTable(GAIN_CONTROL_DB, res) / 20.f);
                if (type == TYPE_BR)
                    g = 0.5f * (1.f + g);
            }

            float in[2];
            in[0] = inputs[IN_L_INPUT].getPolyVoltage(c);
            in[1] = rightPatched ? inputs[IN_R_INPUT].getPolyVoltage(c) : in[0];

            for (int side = 0; side < 2; side++) {
                float x = in[side] / FULL_SCALE_V;
                if (!slope24)
                    x *= g;
                float acc = 0.f;
                for (int k = 0; k < over; k++) {
                    float y = sections[side][c][0].process(x, f, q, type);
                    if (slope24)
                        y = sections[side][c][1].process(y * g, f, q, type);
                    acc += y;
                }
                float out = anyIn ? acc / (float)over * FULL_SCALE_V : 0.f;
                outputs[side == 0 ? OUT_L_OUTPUT : OUT_R_OUTPUT].setVoltage(out, c);
            }
        }
        outputs[OUT_L_OUTPUT].setChannels(channels);
        outputs[OUT_R_OUTPUT].setChannels(channels);
    }
};


/** A lit, labelled button bound to a parameter. As a radio button it sets the
parameter to its own value (the four types, 12 and 24); as a toggle it flips
it (GC). Lit while the parameter is at its value, on the light layer so it
stays lit in a dimmed room. Clicks go through Rack's history, so undo works. */
struct TekButton : app::ParamWidget {
    std::string label;
    int value = 0;
    bool toggle = false;

    bool active() {
        float v = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
        // Without a module (the browser, the library site), the defaults.
        if (!getParamQuantity())
            return toggle ? true : value == 0;
        return toggle ? v > 0.5f : (int)std::round(v) == value;
    }

    void onButton(const ButtonEvent& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            if (ParamQuantity* pq = getParamQuantity()) {
                float oldValue = pq->getValue();
                float newValue = toggle ? (oldValue > 0.5f ? 0.f : 1.f) : (float)value;
                if (newValue != oldValue) {
                    pq->setValue(newValue);
                    auto* h = new history::ParamChange;
                    h->name = "change " + pq->getLabel();
                    h->moduleId = module->id;
                    h->paramId = paramId;
                    h->oldValue = oldValue;
                    h->newValue = newValue;
                    APP->history->push(h);
                }
            }
            e.consume(this);
            return;
        }
        app::ParamWidget::onButton(e);
    }

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.f);
        nvgFillColor(args.vg, nvgRGB(0x2b, 0x2b, 0x2b));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.f);
        nvgStrokeColor(args.vg, nvgRGB(0x55, 0x55, 0x55));
        nvgStroke(args.vg);
        if (!active())
            drawLabel(args, nvgRGB(0x9a, 0x9a, 0x9a));
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1 && active()) {
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 2.f);
            nvgFillColor(args.vg, nvgRGB(0x1f, 0x4f, 0x99));
            nvgFill(args.vg);
            nvgStrokeWidth(args.vg, 1.f);
            nvgStrokeColor(args.vg, AnimatekUI::logoBlue());
            nvgStroke(args.vg);
            drawLabel(args, nvgRGB(0xff, 0xff, 0xff));
        }
        app::ParamWidget::drawLayer(args, layer);
    }

    void drawLabel(const DrawArgs& args, NVGcolor color) {
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font || font->handle < 0)
            return;
        nvgFontSize(args.vg, 8.5f);
        nvgFontFaceId(args.vg, font->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, color);
        nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f + 0.5f, label.c_str(), NULL);
    }
};


/** The response curve, drawn from the same model the audio runs through:
cutoff and resonance with their CV, type, slope and gain control. 20 Hz to
20 kHz across, -54 to +30 dB up; the grey line is 0 dB. */
struct ResponseDisplay : TransparentWidget {
    FilterTek* module = NULL;
    static constexpr int POINTS = 160;
    static constexpr float DB_MIN = -54.f;
    static constexpr float DB_MAX = 30.f;

    float magnitudeDb(float hz, float f, float q, int type, bool slope24, float g) {
        // Chamberlin closed forms at 96 kHz, the rate the original runs at.
        float w = 2.f * (float)M_PI * hz / G1_RATE;
        std::complex<float> z1 = std::polar(1.f, -w);
        std::complex<float> d = 1.f - (2.f - f * q - f * f) * z1 + (1.f - f * q) * z1 * z1;
        std::complex<float> h;
        switch (type) {
            case TYPE_BP: h = f * (1.f - z1) / d; break;
            case TYPE_HP: h = (1.f - z1) * (1.f - z1) / d; break;
            case TYPE_BR: h = ((1.f - z1) * (1.f - z1) + f * f * z1) / d; break;
            default: h = f * f * z1 / d; break;
        }
        float mag = std::abs(h);
        if (slope24)
            mag *= mag;
        return 20.f * std::log10(std::max(mag * g, 1e-9f));
    }

    void draw(const DrawArgs& args) override {
        const float w = box.size.x, h = box.size.y;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 2.5f);
        nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x13));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.f);
        nvgStrokeColor(args.vg, nvgRGB(0x33, 0x38, 0x4a));
        nvgStroke(args.vg);

        auto xOf = [&](float hz) { return w * (std::log10(hz) - std::log10(20.f)) / 3.f; };
        auto yOf = [&](float db) { return h * (1.f - (clamp(db, DB_MIN, DB_MAX) - DB_MIN) / (DB_MAX - DB_MIN)); };
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(0x1d, 0x21, 0x29));
        for (float hz : {100.f, 1000.f, 10000.f}) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, xOf(hz), 1.f);
            nvgLineTo(args.vg, xOf(hz), h - 1.f);
            nvgStroke(args.vg);
        }
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 1.f, yOf(0.f));
        nvgLineTo(args.vg, w - 1.f, yOf(0.f));
        nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2f, 0x3a));
        nvgStroke(args.vg);
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            const float w = box.size.x, h = box.size.y;
            // Without a module, a plausible still: the defaults with some resonance.
            float step = module ? module->displayStep : 72.f;
            float res = module ? module->displayRes : 100.f;
            int type = module ? clamp((int)std::round(module->params[FilterTek::TYPE_PARAM].getValue()), 0, NUM_TYPES - 1) : TYPE_LP;
            bool slope24 = module ? module->params[FilterTek::SLOPE_PARAM].getValue() > 0.5f : true;
            bool gc = module ? module->params[FilterTek::GAIN_CONTROL_PARAM].getValue() > 0.5f : true;

            float f = 2.f * std::sin((float)M_PI * cutoffHz(step) * CUTOFF_TRIM / G1_RATE);
            const float* qTable = (type == TYPE_BR) ? (slope24 ? BR_Q24 : BR_Q12) : (slope24 ? Q24 : Q12);
            float q = readLogTable(qTable, res) * (1.f - 0.5f * f);
            float g = 1.f;
            if (gc) {
                g = std::pow(10.f, readLinTable(GAIN_CONTROL_DB, res) / 20.f);
                if (type == TYPE_BR)
                    g = 0.5f * (1.f + g);
            }

            nvgScissor(args.vg, 1.f, 1.f, w - 2.f, h - 2.f);
            nvgBeginPath(args.vg);
            for (int i = 0; i < POINTS; i++) {
                float hz = 20.f * std::pow(1000.f, (float)i / (float)(POINTS - 1));
                float x = w * (float)i / (float)(POINTS - 1);
                float y = h * (1.f - (clamp(magnitudeDb(hz, f, q, type, slope24, g), DB_MIN, DB_MAX) - DB_MIN) / (DB_MAX - DB_MIN));
                if (i == 0)
                    nvgMoveTo(args.vg, x, y);
                else
                    nvgLineTo(args.vg, x, y);
            }
            nvgStrokeWidth(args.vg, 1.6f);
            nvgLineJoin(args.vg, NVG_ROUND);
            nvgStrokeColor(args.vg, AnimatekUI::logoBlue());
            nvgStroke(args.vg);
            nvgResetScissor(args.vg);
        }
        TransparentWidget::drawLayer(args, layer);
    }
};


struct FilterTekWidget : ModuleWidget {
    FilterTekWidget(FilterTek* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/FilterTek.svg")));

        constexpr float W = 40.64f;  // 8 HP
        constexpr float CX = W * 0.5f;
        // Three columns for the CV row, two for audio in and out.
        const float COL3[3] = {7.3f, CX, W - 7.3f};
        const float COL2[2] = {12.5f, W - 12.5f};

        auto* moduleName = new TextLabel("FILTERtek", mm2px(Vec(1.8f, 119.9f)),
                                         mm2px(Vec(28.f, 5.8f)));
        moduleName->fontSize = 14.f;
        moduleName->uppercase = false;
        moduleName->color = nvgRGB(0x2C, 0x7F, 0xFF);
        addChild(moduleName);

        auto addLabel = [&](const char* text, float cx, float y, float w) {
            auto label = createWidget<TextLabel>(mm2px(Vec(cx - w * 0.5f, y)));
            label->box.size = mm2px(Vec(w, 3.f));
            label->text = text;
            label->fontSize = 7.f;
            addChild(label);
        };
        auto line = [&](float ax, float ay, float bx, float by) {
            addChild(new ConnectorLine(mm2px(ax), mm2px(ay), mm2px(bx), mm2px(by)));
        };
        auto addButton = [&](int paramId, int value, bool toggle, const char* text,
                             float cx, float cy, float w, float h) {
            auto* b = createParam<TekButton>(mm2px(Vec(cx - w * 0.5f, cy - h * 0.5f)), module, paramId);
            b->box.size = mm2px(Vec(w, h));
            b->value = value;
            b->toggle = toggle;
            b->label = text;
            addParam(b);
        };

        addLabel("CUTOFF", CX, 2.2f, 16.f);
        addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(CX, 14.5f)), module,
                                                         FilterTek::FREQ_PARAM));

        // The four types as buttons, the lit one is the one playing.
        const char* types[NUM_TYPES] = {"LP", "BP", "HP", "BR"};
        for (int i = 0; i < NUM_TYPES; i++)
            addButton(FilterTek::TYPE_PARAM, i, false, types[i], 5.6f + 9.8f * (float)i, 27.5f, 7.2f, 4.6f);

        addLabel("RES", 11.0f, 31.0f, 12.f);
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(11.0f, 40.5f)), module,
                                                          FilterTek::RES_PARAM));
        // Gain control, and the slope under it as two buttons, beside RES.
        addButton(FilterTek::GAIN_CONTROL_PARAM, 1, true, "GC", 30.0f, 34.6f, 13.6f, 4.6f);
        addLabel("dB/OCT", 30.0f, 37.6f, 14.f);
        addButton(FilterTek::SLOPE_PARAM, 0, false, "12", 26.6f, 43.2f, 6.8f, 4.6f);
        addButton(FilterTek::SLOPE_PARAM, 1, false, "24", 33.4f, 43.2f, 6.8f, 4.6f);

        auto* display = createWidget<ResponseDisplay>(mm2px(Vec(3.0f, 49.0f)));
        display->box.size = mm2px(Vec(W - 6.f, 14.5f));
        display->module = module;
        addChild(display);

        // The CV row: each amount over its jack, the hairline between them and
        // the name under the line. V/OCT has no amount: it is always 1 V/oct.
        const char* cvNames[2] = {"CUT", "RES"};
        const int cvParams[2] = {FilterTek::CUT_CV_PARAM, FilterTek::RES_CV_PARAM};
        const int cvInputs[2] = {FilterTek::CUT_CV_INPUT, FilterTek::RES_CV_INPUT};
        for (int i = 0; i < 2; i++) {
            addParam(createParamCentered<Trimpot>(mm2px(Vec(COL3[i], 69.0f)), module, cvParams[i]));
            line(COL3[i], 72.3f, COL3[i], 74.1f);
            addLabel(cvNames[i], COL3[i], 74.5f, 12.f);
            addInput(createInputCentered<TekInputPort>(mm2px(Vec(COL3[i], 82.0f)), module, cvInputs[i]));
        }
        addLabel("V/OCT", COL3[2], 74.5f, 12.f);
        addInput(createInputCentered<TekInputPort>(mm2px(Vec(COL3[2], 82.0f)), module, FilterTek::VOCT_INPUT));

        addLabel("IN L", COL2[0], 88.2f, 12.f);
        addInput(createInputCentered<TekInputPort>(mm2px(Vec(COL2[0], 95.5f)), module, FilterTek::IN_L_INPUT));
        addLabel("IN R", COL2[1], 88.2f, 12.f);
        addInput(createInputCentered<TekInputPort>(mm2px(Vec(COL2[1], 95.5f)), module, FilterTek::IN_R_INPUT));
        // The outputs sit on dark plates drawn in the SVG.
        addLabel("OUT L", COL2[0], 101.2f, 12.f);
        addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(COL2[0], 108.5f)), module, FilterTek::OUT_L_OUTPUT));
        addLabel("OUT R", COL2[1], 101.2f, 12.f);
        addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(COL2[1], 108.5f)), module, FilterTek::OUT_R_OUTPUT));
    }
};


Model* modelFilterTek = createModel<FilterTek, FilterTekWidget>("FilterTek");
