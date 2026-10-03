#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"
#include "AdsrTekCurves.hpp"

#include <algorithm>
#include <cmath>

using AnimatekUI::TekInputPort;
using AnimatekUI::TekOutputPort;
using AnimatekUI::TextLabel;

// ============================================================================
// ADSRtek - the envelope of a classic 90s virtual-analogue modular
// ============================================================================
//
// Modelled on measurements, not on anybody's code: the curves below come
// from recording that machine's envelopes, value by value, and fitting what
// came out. What makes it feel the way it does, as far as the measurements go:
//
// - Times follow its 128 knob steps, from 0.5 ms to about 50 s, as measured:
//   the published table to within half a percent in the median, except at
//   the top, where the measured times step and run long and are used as
//   they are. Decay and release are one exponential, timed to reach 1% of
//   the way. Below -76 dB, where the recordings could no longer resolve
//   anything, it settles on its target rather than crawl on forever.
// - Three attack shapes. Lin is a straight ramp. Log is an exponential
//   approach to a target above full scale, cut when it gets there; Exp is an
//   exponential growth from zero. Both bend differently at every knob step,
//   and one number per step, fitted to the recordings, reproduces them to
//   within 0.2% (Log) and 0.6% (Exp).
// - Each step depends only on where the envelope is, so a new gate never
//   restarts from zero: it carries on from the current level, and a
//   retrigger mid-release takes as long as the curve takes from there. That
//   also matches the recordings, without having been fitted to them.
// - It runs at a 24 kHz control rate, holding each value for one tick. That
//   is on by default and can be turned off in the menu.
// ============================================================================

namespace adsrtek {

// Control rate of the original: one value every 4 samples at 96 kHz.
static constexpr float CONTROL_RATE = 24000.f;
// An exponential has fallen to 1% of the way after ln(100) time constants.
static constexpr float LN_100 = 4.605170186f;

enum Stage { STAGE_IDLE, STAGE_ATTACK, STAGE_DECAY, STAGE_SUSTAIN, STAGE_RELEASE };
enum Mode { MODE_ADSR, MODE_AD };
enum Shape { SHAPE_LOG, SHAPE_LIN, SHAPE_EXP };

/** A measured table read at a knob value 0-127, interpolated exponentially
between steps so the knob is smooth and lands on the measurement at every
integer. */
static float readTable(const float* table, float value) {
    value = clamp(value, 0.f, 127.f);
    int i = std::min((int)value, 126);
    float f = value - (float)i;
    return table[i] * std::pow(table[i + 1] / table[i], f);
}

static float attackTime(int shape, float value) {
    if (shape < 0 || shape > SHAPE_EXP)
        shape = SHAPE_LIN;
    return readTable(ATTACK_TIME[shape], value);
}

/** Time constant of decay and release: the same measured curve. */
static float decayTau(float value) {
    return readTable(DECAY_TAU, value);
}

/** Per-step shape parameter: Log's target above full scale, Exp's offset. */
static float shapeParameter(int shape, float value) {
    return readTable(shape == SHAPE_LOG ? LOG_TARGET : EXP_OFFSET, value);
}

struct Envelope {
    int stage = STAGE_IDLE;
    float level = 0.f;
    bool gate = false;

    void startAttack() {
        stage = STAGE_ATTACK;
    }

    /** Advances by dt seconds. The attack is a time and shapeParam its Log
    target or Exp offset; decay and release are time constants, in seconds;
    sustain is in [0, 1]. */
    void step(float dt, int mode, int shape, float attack, float shapeParam, float decayTau,
              float sustain, float releaseTau, bool gateCutsAttack) {
        switch (stage) {
            case STAGE_ATTACK:
                if (mode == MODE_ADSR && !gate) {
                    stage = STAGE_RELEASE;
                    break;
                }
                if (mode == MODE_AD && gateCutsAttack && !gate) {
                    stage = STAGE_DECAY;
                    break;
                }
                // Each shape written as a step from the current level, with
                // its rate set so that from zero it reaches 1 in `attack`.
                if (shape == SHAPE_LOG) {
                    float target = shapeParam;
                    float rate = std::log(target / (target - 1.f));
                    level = target - (target - level) * std::exp(-rate * dt / attack);
                }
                else if (shape == SHAPE_EXP) {
                    float offset = shapeParam;
                    float rate = std::log(1.f + 1.f / offset);
                    level = (level + offset) * std::exp(rate * dt / attack) - offset;
                }
                else {
                    level += dt / attack;
                }
                if (level >= 1.f) {
                    level = 1.f;
                    stage = STAGE_DECAY;
                }
                break;

            case STAGE_DECAY: {
                if (mode == MODE_ADSR && !gate) {
                    stage = STAGE_RELEASE;
                    break;
                }
                float target = (mode == MODE_ADSR) ? sustain : 0.f;
                level = target + (level - target) * std::exp(-dt / decayTau);
                if (std::abs(level - target) <= SNAP_LEVEL) {
                    level = target;
                    stage = (mode == MODE_ADSR) ? STAGE_SUSTAIN : STAGE_IDLE;
                }
                break;
            }

            case STAGE_SUSTAIN:
                if (!gate) {
                    stage = STAGE_RELEASE;
                    break;
                }
                // Follows the knob while held, as the original does.
                level = sustain;
                break;

            case STAGE_RELEASE:
                level *= std::exp(-dt / releaseTau);
                if (level <= SNAP_LEVEL) {
                    level = 0.f;
                    stage = STAGE_IDLE;
                }
                break;

            default:
                break;
        }
    }
};

/** Shows a time knob as the time it really takes: start to full scale for
the attack (which depends on the shape), down to 1% for decay and release. */
struct TimeQuantity : ParamQuantity {
    bool isAttack = false;
    int shapeParam = -1;

    float seconds() {
        if (!isAttack)
            return decayTau(getValue()) * LN_100;
        int shape = SHAPE_LIN;
        if (module && shapeParam >= 0)
            shape = (int)std::round(module->params[shapeParam].getValue());
        return attackTime(shape, getValue());
    }
    std::string getDisplayValueString() override {
        float t = seconds();
        if (t < 1.f)
            return string::f(t < 0.01f ? "%.1f" : "%.0f", t * 1000.f);
        return string::f(t < 10.f ? "%.2f" : "%.1f", t);
    }
    std::string getUnit() override {
        return seconds() < 1.f ? " ms" : " s";
    }
};

} // namespace adsrtek

using namespace adsrtek;


struct AdsrTek : Module {
    enum ParamId { ATTACK_PARAM, DECAY_PARAM, SUSTAIN_PARAM, RELEASE_PARAM,
                   SHAPE_PARAM, MODE_PARAM, PARAMS_LEN };
    enum InputId { ATTACK_CV_INPUT, DECAY_CV_INPUT, SUSTAIN_CV_INPUT, RELEASE_CV_INPUT,
                   GATE_INPUT, RETRIG_INPUT, AMP_INPUT, IN_INPUT, INPUTS_LEN };
    enum OutputId { ENV_OUTPUT, OUT_OUTPUT, OUTPUTS_LEN };
    enum LightId { GATE_LIGHT, LIGHTS_LEN };

    Envelope envelopes[16];
    dsp::SchmittTrigger gateTriggers[16];
    dsp::SchmittTrigger retrigTriggers[16];
    float controlClock = 0.f;
    // Last value of each channel, held between control ticks.
    float held[16] = {};

    bool controlRateSteps = true;
    bool invert = false;
    bool gateCutsAttack = false;

    AdsrTek() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // 0-127, the original's knob steps. Defaults: a short pluck that rings.
        configParam<TimeQuantity>(ATTACK_PARAM, 0.f, 127.f, 10.f, "Attack");
        configParam<TimeQuantity>(DECAY_PARAM, 0.f, 127.f, 70.f, "Decay");
        configParam(SUSTAIN_PARAM, 0.f, 1.f, 0.5f, "Sustain", "%", 0.f, 100.f);
        configParam<TimeQuantity>(RELEASE_PARAM, 0.f, 127.f, 60.f, "Release");
        configSwitch(SHAPE_PARAM, 0.f, 2.f, 1.f, "Attack shape", {"Log", "Lin", "Exp"});
        configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Mode", {"ADSR", "AD"});
        auto* attackQ = dynamic_cast<TimeQuantity*>(paramQuantities[ATTACK_PARAM]);
        attackQ->isAttack = true;
        attackQ->shapeParam = SHAPE_PARAM;

        configInput(ATTACK_CV_INPUT, "Attack CV (1 V = 12.7 steps)");
        configInput(DECAY_CV_INPUT, "Decay CV (1 V = 12.7 steps)");
        configInput(SUSTAIN_CV_INPUT, "Sustain CV (1 V = 10%)");
        configInput(RELEASE_CV_INPUT, "Release CV (1 V = 12.7 steps)");
        configInput(GATE_INPUT, "Gate");
        configInput(RETRIG_INPUT, "Retrigger");
        configInput(AMP_INPUT, "Amplitude (10 V = full, normalled to 10 V)");
        configInput(IN_INPUT, "Audio");
        configOutput(ENV_OUTPUT, "Envelope");
        configOutput(OUT_OUTPUT, "Audio through the envelope");
        configLight(GATE_LIGHT, "Envelope");
        configBypass(IN_INPUT, OUT_OUTPUT);
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        controlRateSteps = true;
        invert = false;
        gateCutsAttack = false;
        for (int c = 0; c < 16; c++) {
            envelopes[c] = Envelope();
            held[c] = 0.f;
        }
    }

    /** A time knob plus its CV, in knob steps: 10 V sweeps all 128. */
    float knobFor(int paramId, int inputId, int c) {
        return params[paramId].getValue() + inputs[inputId].getPolyVoltage(c) * 12.7f;
    }

    void process(const ProcessArgs& args) override {
        int channels = std::max(1, std::max(inputs[GATE_INPUT].getChannels(),
                                            inputs[RETRIG_INPUT].getChannels()));
        const int mode = (int)std::round(params[MODE_PARAM].getValue());
        const int shape = (int)std::round(params[SHAPE_PARAM].getValue());

        // With the steps on, the envelope only moves on a 24 kHz tick and
        // advances one tick's worth each time; off, it moves every sample.
        bool tick = true;
        float dt = args.sampleTime;
        if (controlRateSteps) {
            controlClock += args.sampleTime * CONTROL_RATE;
            tick = controlClock >= 1.f;
            if (tick)
                controlClock -= std::floor(controlClock);
            dt = 1.f / CONTROL_RATE;
        }

        for (int c = 0; c < channels; c++) {
            Envelope& env = envelopes[c];

            // Edges are caught every sample, so a short trigger between two
            // ticks is never lost; the envelope reacts on the next tick.
            bool gateHigh = inputs[GATE_INPUT].getPolyVoltage(c) >= 1.f;
            if (gateTriggers[c].process(inputs[GATE_INPUT].getPolyVoltage(c), 0.1f, 1.f))
                env.startAttack();
            if (retrigTriggers[c].process(inputs[RETRIG_INPUT].getPolyVoltage(c), 0.1f, 1.f)
                && (gateHigh || mode == MODE_AD))
                env.startAttack();
            env.gate = gateHigh;

            if (tick) {
                float sustain = clamp(params[SUSTAIN_PARAM].getValue()
                                      + inputs[SUSTAIN_CV_INPUT].getPolyVoltage(c) / 10.f,
                                      0.f, 1.f);
                float attackKnob = knobFor(ATTACK_PARAM, ATTACK_CV_INPUT, c);
                env.step(dt, mode, shape,
                         attackTime(shape, attackKnob),
                         shapeParameter(shape, attackKnob),
                         decayTau(knobFor(DECAY_PARAM, DECAY_CV_INPUT, c)),
                         sustain,
                         decayTau(knobFor(RELEASE_PARAM, RELEASE_CV_INPUT, c)),
                         gateCutsAttack);
                held[c] = env.level;
            }

            float value = invert ? 1.f - held[c] : held[c];
            if (inputs[AMP_INPUT].isConnected())
                value *= clamp(inputs[AMP_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);

            outputs[ENV_OUTPUT].setVoltage(10.f * value, c);
            outputs[OUT_OUTPUT].setVoltage(inputs[IN_INPUT].getPolyVoltage(c) * value, c);
        }
        outputs[ENV_OUTPUT].setChannels(channels);
        outputs[OUT_OUTPUT].setChannels(std::max(channels, inputs[IN_INPUT].getChannels()));

        lights[GATE_LIGHT].setBrightness(held[0]);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "controlRateSteps", json_boolean(controlRateSteps));
        json_object_set_new(root, "invert", json_boolean(invert));
        json_object_set_new(root, "gateCutsAttack", json_boolean(gateCutsAttack));
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root)
            return;
        if (json_t* j = json_object_get(root, "controlRateSteps"))
            controlRateSteps = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "invert"))
            invert = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "gateCutsAttack"))
            gateCutsAttack = json_boolean_value(j);
    }
};


struct AdsrTekWidget : ModuleWidget {
    AdsrTekWidget(AdsrTek* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/AdsrTek.svg")));

        constexpr float W = 40.64f;  // 8 HP
        constexpr float XL = 11.0f;
        constexpr float XR = W - 11.0f;

        auto* moduleName = new TextLabel("ADSRtek", mm2px(Vec(1.8f, 119.9f)),
                                         mm2px(Vec(26.f, 5.8f)));
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

        // Header: the mode on the left, the attack shape on the right, each
        // with its positions written beside it. A Rack switch at 0 points
        // down, so the first option is the bottom label.
        addLabel("AD", XL - 5.0f, 4.0f, 10.f);
        addLabel("ADSR", XL - 5.0f, 9.5f, 10.f);
        addParam(createParamCentered<CKSS>(mm2px(Vec(XL + 2.5f, 8.5f)), module,
                                           AdsrTek::MODE_PARAM));
        addLabel("EXP", XR + 5.5f, 2.6f, 10.f);
        addLabel("LIN", XR + 5.5f, 6.6f, 10.f);
        addLabel("LOG", XR + 5.5f, 10.6f, 10.f);
        addParam(createParamCentered<CKSSThree>(mm2px(Vec(XR - 1.5f, 8.5f)), module,
                                                AdsrTek::SHAPE_PARAM));

        addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(W * 0.5f, 8.5f)), module,
                                                            AdsrTek::GATE_LIGHT));

        // Four knobs in a square; their CV jacks sit in a row below, in the
        // order the stages run.
        auto addKnob = [&](const char* text, float cx, float y, int paramId) {
            addLabel(text, cx, y, 16.f);
            addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(cx, y + 10.0f)), module,
                                                         paramId));
        };
        addKnob("ATTACK", XL, 17.0f, AdsrTek::ATTACK_PARAM);
        addKnob("DECAY", XR, 17.0f, AdsrTek::DECAY_PARAM);
        addKnob("SUSTAIN", XL, 39.0f, AdsrTek::SUSTAIN_PARAM);
        addKnob("RELEASE", XR, 39.0f, AdsrTek::RELEASE_PARAM);

        constexpr float CV_Y = 70.0f;
        const float cvX[4] = {5.6f, 15.6f, 25.0f, 35.0f};
        const int cvIds[4] = {AdsrTek::ATTACK_CV_INPUT, AdsrTek::DECAY_CV_INPUT,
                              AdsrTek::SUSTAIN_CV_INPUT, AdsrTek::RELEASE_CV_INPUT};
        const char* cvNames[4] = {"A", "D", "S", "R"};
        for (int i = 0; i < 4; i++) {
            addLabel(cvNames[i], cvX[i], CV_Y - 7.5f, 6.f);
            addInput(createInputCentered<TekInputPort>(mm2px(Vec(cvX[i], CV_Y)), module, cvIds[i]));
        }
        addLabel("CV", W * 0.5f, 57.0f, 10.f);

        auto addIn = [&](const char* text, float cx, float y, int inputId) {
            addLabel(text, cx, y, 12.f);
            addInput(createInputCentered<TekInputPort>(mm2px(Vec(cx, y + 7.5f)), module, inputId));
        };
        auto addOut = [&](const char* text, float cx, float y, int outputId) {
            addLabel(text, cx, y, 12.f);
            addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(cx, y + 7.5f)), module, outputId));
        };

        // Everything that comes in above the panel line (y = 93 in the SVG),
        // what goes out below.
        addIn("GATE", cvX[0], 78.0f, AdsrTek::GATE_INPUT);
        addIn("RETRIG", cvX[1], 78.0f, AdsrTek::RETRIG_INPUT);
        addIn("AMP", cvX[2], 78.0f, AdsrTek::AMP_INPUT);
        addIn("IN", cvX[3], 78.0f, AdsrTek::IN_INPUT);
        addOut("ENV", XL, 96.0f, AdsrTek::ENV_OUTPUT);
        addOut("OUT", XR, 96.0f, AdsrTek::OUT_OUTPUT);
    }

    void appendContextMenu(ui::Menu* menu) override {
        AdsrTek* module = dynamic_cast<AdsrTek*>(this->module);
        if (!module)
            return;

        menu->addChild(new ui::MenuSeparator);
        menu->addChild(createCheckMenuItem(
            "24 kHz control-rate steps", "",
            [=]() { return module->controlRateSteps; },
            [=]() { module->controlRateSteps ^= true; }));
        menu->addChild(createCheckMenuItem(
            "Invert envelope", "",
            [=]() { return module->invert; },
            [=]() { module->invert ^= true; }));
        menu->addChild(createCheckMenuItem(
            "AD: gate release cuts the attack", "",
            [=]() { return module->gateCutsAttack; },
            [=]() { module->gateCutsAttack ^= true; }));
    }
};


Model* modelAdsrTek = createModel<AdsrTek, AdsrTekWidget>("AdsrTek");
