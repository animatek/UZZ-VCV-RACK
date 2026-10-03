#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"
#include "FilterTekTables.hpp"

#include <algorithm>
#include <cmath>

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
static const char* TYPE_NAMES[NUM_TYPES] = {"Lowpass", "Bandpass", "Highpass", "Band reject"};

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
    enum ParamId { FREQ_PARAM, RES_PARAM, FM_PARAM, RES_CV_PARAM, TYPE_PARAM, SLOPE_PARAM,
                   GAIN_CONTROL_PARAM, TYPE_BUTTON_PARAM, PARAMS_LEN };
    enum InputId { IN_INPUT, VOCT_INPUT, FM_INPUT, RES_CV_INPUT, INPUTS_LEN };
    enum OutputId { OUT_OUTPUT, OUTPUTS_LEN };
    enum LightId { ENUMS(TYPE_LIGHTS, NUM_TYPES), LIGHTS_LEN };

    Section sections[16][2];
    dsp::SchmittTrigger typeButton;

    FilterTek() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<CutoffQuantity>(FREQ_PARAM, 0.f, 127.f, 60.f, "Cutoff");
        configParam(RES_PARAM, 0.f, 127.f, 0.f, "Resonance");
        // Bipolar, 100% being the original's amount 127: two semitones per
        // twelfth of a volt, so 50% is 1 V/octave.
        configParam(FM_PARAM, -1.f, 1.f, 0.f, "FM amount", "%", 0.f, 100.f);
        configParam(RES_CV_PARAM, -1.f, 1.f, 0.f, "Resonance CV amount", "%", 0.f, 100.f);
        configSwitch(TYPE_PARAM, 0.f, 3.f, 0.f, "Type", {"Lowpass", "Bandpass", "Highpass", "Band reject"});
        configSwitch(SLOPE_PARAM, 0.f, 1.f, 1.f, "Slope", {"12 dB/oct", "24 dB/oct"});
        configSwitch(GAIN_CONTROL_PARAM, 0.f, 1.f, 1.f, "Gain control", {"Off", "On"});
        configButton(TYPE_BUTTON_PARAM, "Next filter type");

        configInput(IN_INPUT, "Audio");
        configInput(VOCT_INPUT, "Cutoff V/oct");
        configInput(FM_INPUT, "FM (1 V = 2 octaves at full amount)");
        configInput(RES_CV_INPUT, "Resonance CV (1 V = 24 steps at full amount)");
        configOutput(OUT_OUTPUT, "Audio");
        configBypass(IN_INPUT, OUT_OUTPUT);
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        for (auto& ch : sections)
            ch[0] = ch[1] = Section();
    }

    void process(const ProcessArgs& args) override {
        if (typeButton.process(params[TYPE_BUTTON_PARAM].getValue()))
            params[TYPE_PARAM].setValue((float)(((int)params[TYPE_PARAM].getValue() + 1) % NUM_TYPES));
        const int type = clamp((int)std::round(params[TYPE_PARAM].getValue()), 0, NUM_TYPES - 1);
        const bool slope24 = params[SLOPE_PARAM].getValue() > 0.5f;
        const bool gainControl = params[GAIN_CONTROL_PARAM].getValue() > 0.5f;

        // Steps per Rack sample that bring the filter to about 96 kHz: two at
        // 48 kHz, one at 96. The input is held across them and the outputs
        // averaged, which is all the original's own converters do.
        const int over = std::max(1, (int)std::round(G1_RATE / args.sampleRate));
        const float rate = args.sampleRate * (float)over;

        const int channels = std::max(1, inputs[IN_INPUT].getChannels());
        const float freqKnob = params[FREQ_PARAM].getValue();
        const float resKnob = params[RES_PARAM].getValue();
        const float fmAmount = params[FM_PARAM].getValue();
        const float resAmount = params[RES_CV_PARAM].getValue();

        for (int c = 0; c < channels; c++) {
            // The original counts modulation in units, twelve to the octave
            // on the cutoff; one volt here is twelve units. FM at full amount
            // doubles them, as the original's amount 127 does.
            float step = freqKnob + inputs[VOCT_INPUT].getPolyVoltage(c) * 12.f
                         + inputs[FM_INPUT].getPolyVoltage(c) * 12.f * 2.f * fmAmount;
            float res = clamp(resKnob + inputs[RES_CV_INPUT].getPolyVoltage(c) * 12.f * 2.f * resAmount,
                              0.f, 127.f);

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

            float x = inputs[IN_INPUT].getPolyVoltage(c) / FULL_SCALE_V;
            if (!slope24)
                x *= g;

            float acc = 0.f;
            for (int k = 0; k < over; k++) {
                float y = sections[c][0].process(x, f, q, type);
                if (slope24)
                    y = sections[c][1].process(y * g, f, q, type);
                acc += y;
            }
            outputs[OUT_OUTPUT].setVoltage(acc / (float)over * FULL_SCALE_V, c);
        }
        outputs[OUT_OUTPUT].setChannels(channels);

        for (int i = 0; i < NUM_TYPES; i++)
            lights[TYPE_LIGHTS + i].setBrightness(i == type ? 1.f : 0.f);
    }
};


struct FilterTekWidget : ModuleWidget {
    FilterTekWidget(FilterTek* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/FilterTek.svg")));

        constexpr float W = 40.64f;  // 8 HP
        constexpr float XL = 10.5f;
        constexpr float XR = W - 10.5f;

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

        addLabel("FREQ", XL, 3.5f, 14.f);
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(XL, 14.0f)), module,
                                                          FilterTek::FREQ_PARAM));
        addLabel("RES", XR, 3.5f, 14.f);
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(XR, 14.0f)), module,
                                                          FilterTek::RES_PARAM));

        // Type: a button that steps through the four, each lit beside its name.
        addParam(createParamCentered<TL1105>(mm2px(Vec(5.0f, 30.0f)), module,
                                             FilterTek::TYPE_BUTTON_PARAM));
        const char* typeLabels[NUM_TYPES] = {"LP", "BP", "HP", "BR"};
        for (int i = 0; i < NUM_TYPES; i++) {
            float y = 25.0f + 3.4f * (float)i;
            addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(10.0f, y)), module,
                                                                FilterTek::TYPE_LIGHTS + i));
            auto* l = createWidget<TextLabel>(mm2px(Vec(11.2f, y - 1.6f)));
            l->box.size = mm2px(Vec(6.f, 3.f));
            l->text = typeLabels[i];
            l->fontSize = 7.f;
            addChild(l);
        }

        // Slope and gain control, each with its two positions written beside
        // it; a Rack switch at 0 points down.
        addLabel("SLOPE", 24.0f, 22.0f, 10.f);
        addLabel("24", 20.0f, 25.2f, 6.f);
        addLabel("12", 20.0f, 31.6f, 6.f);
        addParam(createParamCentered<CKSS>(mm2px(Vec(24.5f, 29.6f)), module, FilterTek::SLOPE_PARAM));
        addLabel("GAIN", 34.0f, 22.0f, 10.f);
        addLabel("ON", 37.6f, 25.2f, 6.f);
        addLabel("OFF", 37.6f, 31.6f, 6.f);
        addParam(createParamCentered<CKSS>(mm2px(Vec(33.5f, 29.6f)), module,
                                           FilterTek::GAIN_CONTROL_PARAM));

        // The two modulation amounts over their jacks, joined by hairlines.
        addLabel("FM", XL, 39.5f, 10.f);
        addParam(createParamCentered<Trimpot>(mm2px(Vec(XL, 46.0f)), module, FilterTek::FM_PARAM));
        addLabel("RES CV", XR, 39.5f, 14.f);
        addParam(createParamCentered<Trimpot>(mm2px(Vec(XR, 46.0f)), module, FilterTek::RES_CV_PARAM));

        auto addIn = [&](const char* text, float cx, float y, int id) {
            if (text)
                addLabel(text, cx, y, 12.f);
            addInput(createInputCentered<TekInputPort>(mm2px(Vec(cx, y + 7.5f)), module, id));
        };
        addIn(nullptr, XL, 51.0f, FilterTek::FM_INPUT);
        addIn(nullptr, XR, 51.0f, FilterTek::RES_CV_INPUT);
        line(XL, 48.8f, XL, 54.3f);
        line(XR, 48.8f, XR, 54.3f);
        addIn("V/OCT", W * 0.5f, 51.0f, FilterTek::VOCT_INPUT);

        // In above the panel line (y = 86 in the SVG), out below.
        addIn("IN", W * 0.5f, 70.0f, FilterTek::IN_INPUT);
        addLabel("OUT", W * 0.5f, 90.0f, 12.f);
        addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(W * 0.5f, 97.5f)), module,
                                                      FilterTek::OUT_OUTPUT));
    }

    void appendContextMenu(ui::Menu* menu) override {
        FilterTek* module = dynamic_cast<FilterTek*>(this->module);
        if (!module)
            return;
        menu->addChild(new ui::MenuSeparator);
        menu->addChild(createSubmenuItem("Type", TYPE_NAMES[(int)module->params[FilterTek::TYPE_PARAM].getValue()],
                                         [=](ui::Menu* sub) {
            for (int i = 0; i < NUM_TYPES; i++)
                sub->addChild(createCheckMenuItem(TYPE_NAMES[i], "",
                    [=]() { return (int)module->params[FilterTek::TYPE_PARAM].getValue() == i; },
                    [=]() { module->params[FilterTek::TYPE_PARAM].setValue((float)i); }));
        }));
    }
};


Model* modelFilterTek = createModel<FilterTek, FilterTekWidget>("FilterTek");
