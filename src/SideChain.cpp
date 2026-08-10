#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"

#include <algorithm>
#include <cmath>

using AnimatekUI::TekInputPort;
using AnimatekUI::TekOutputPort;
using AnimatekUI::TextLabel;

// ============================================================================
// SideChain - trigger-fired ducking VCA
// ============================================================================
//
// Feed it the same trigger that fires the kick and run the audio through it:
// the signal drops and recovers, no compressor involved. The VCA is built in,
// so nothing else is needed, but ENV still carries the envelope as CV for
// whatever else you want to duck in step.
//
// What separates it from any inverted envelope patched by hand is that every
// hit is slightly different from the last, so the ducking breathes the way an
// analogue compressor does instead of stamping an identical curve forever.
//
// The variation is a random walk, not white noise: each hit is correlated
// with the previous one. Pure randomness sounds random; correlated variation
// sounds human, because a real compressor also drags state between hits.
// Every polyphony channel keeps its own generator, so ducking several tracks
// makes each of them breathe differently - which is exactly what cannot be
// built by patching envelopes by hand.
// ============================================================================

static constexpr float ATTACK_TIME = 0.002f;  // fall time, fixed
static constexpr float HOLD_TIME = 0.012f;    // time spent at the floor

// Jitter ranges at JITTER = 100%, scaled linearly by the knob. Recovery gets
// the widest range because it is the most natural sounding and the least
// distracting; depth gets the narrowest because it moves the perceived level
// of the mix.
static constexpr float JITTER_RECOVERY = 0.20f;
static constexpr float JITTER_DEPTH = 0.12f;
static constexpr float JITTER_CURVE = 0.15f;

// Random walk: x = A*x + B*u, with u uniform in [-1, 1].
//
// B is sqrt(1 - A^2) rather than the more obvious (1 - A). With (1 - A) the
// walk still has the right correlation but its steady-state deviation
// collapses to about 0.23, so the nominal ranges above would only ever be
// heard at a quarter of their size. This keeps unit variance, so ±20% of
// recovery really means ±20%.
static constexpr float WALK_A = 0.7f;
static constexpr float WALK_B = 0.714143f;  // sqrt(1 - 0.7^2)

enum CurveShape {
    CURVE_EXPONENTIAL,
    CURVE_LINEAR,
    CURVE_LOGARITHMIC,
    NUM_CURVE_SHAPES
};

// Recovery is level = floor + (1 - floor) * phase^k. k > 1 stays down and
// snaps back at the end (the classic pump), k < 1 lifts immediately and
// settles slowly (closer to how an analogue release actually behaves).
static const float CURVE_EXPONENTS[NUM_CURVE_SHAPES] = {2.5f, 1.0f, 0.4f};

static const char* CURVE_NAMES[NUM_CURVE_SHAPES] = {
    "Exponential", "Linear", "Logarithmic"};


struct SideChain : Module {
    // New entries go at the end of each enum: the indices are what patches
    // store, so appending keeps older patches loading onto the right jacks.
    enum ParamId { RECOVERY_PARAM, DEPTH_PARAM, JITTER_PARAM, LEVEL_PARAM, PARAMS_LEN };
    enum InputId { TRIG_INPUT, DEPTH_CV_INPUT, IN_L_INPUT, IN_R_INPUT, INPUTS_LEN };
    enum OutputId { ENV_OUTPUT, OUT_L_OUTPUT, OUT_R_OUTPUT, EOC_OUTPUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    enum Stage { STAGE_IDLE, STAGE_ATTACK, STAGE_HOLD, STAGE_RECOVER };

    struct Voice {
        dsp::SchmittTrigger trigger;
        int stage = STAGE_IDLE;
        float level = 1.f;
        float phase = 0.f;
        float attackStart = 1.f;
        float floorLevel = 1.f;
        // Drawn once per hit, never modulated continuously: a value that
        // drifts between hits sounds like an LFO, not like a compressor.
        float recTime = 0.25f;
        float exponent = 2.5f;
        float walkRecovery = 0.f;
        float walkDepth = 0.f;
        float walkCurve = 0.f;
        dsp::PulseGenerator eocPulse;
        random::Xoroshiro128Plus rng;
    };

    Voice voices[16];
    int curveShape = CURVE_EXPONENTIAL;
    bool freezeJitter = false;
    uint64_t baseSeed = 0x5C1DECA1ULL;
    // Off by default: a stereo pair must duck identically. Giving L and R
    // their own jittered envelopes decorrelates them and the image wobbles
    // sideways on every hit. Worth turning on only to duck several unrelated
    // tracks through one polyphonic cable.
    bool perChannelEnvelopes = false;
    // Live VCA gain of channel 0, for the meter to draw. Written from the
    // audio thread and read from the UI thread; a torn float here would just
    // mean one wrong frame of a meter, so no synchronisation.
    float meterGain = 1.f;

    SideChain() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Exponential scale: the stored value is log2(seconds), so the knob
        // spends as much travel on 40-200 ms as on 200-1000 ms.
        //
        // 40 ms floor rather than 20: below that the duck is shorter than the
        // 12 ms hold plus the 2 ms fall, so the knob stops doing anything
        // useful. 1 s ceiling rather than 2: past a second the recovery
        // outlasts a bar at most tempos and the pumping stops reading as such.
        configParam(RECOVERY_PARAM, std::log2(0.040f), std::log2(1.0f),
                    std::log2(0.250f), "Recovery", " ms", 2.f, 1000.f);
        configParam(DEPTH_PARAM, 0.f, 1.f, 0.80f, "Depth", "%", 0.f, 100.f);
        configParam(JITTER_PARAM, 0.f, 1.f, 0.25f, "Jitter", "%", 0.f, 100.f);
        // Ceiling of the VCA. At 100% the module behaves exactly as before it
        // had a slider, so old patches sound unchanged.
        configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f);

        configInput(TRIG_INPUT, "Trigger");
        configInput(DEPTH_CV_INPUT, "Depth CV");
        configInput(IN_L_INPUT, "Audio left");
        configInput(IN_R_INPUT, "Audio right (normalled to left)");
        configOutput(ENV_OUTPUT, "Ducked envelope");
        configOutput(OUT_L_OUTPUT, "Ducked audio left");
        configOutput(OUT_R_OUTPUT, "Ducked audio right");
        configOutput(EOC_OUTPUT, "End of cycle");
        configBypass(IN_L_INPUT, OUT_L_OUTPUT);
        configBypass(IN_R_INPUT, OUT_R_OUTPUT);

        reseedVoices(baseSeed);
    }

    /** Each channel gets a distinct stream. Sharing one would make every
    ducked track breathe in lockstep, which defeats the point. */
    void reseedVoices(uint64_t seed) {
        for (int c = 0; c < 16; c++) {
            // Odd multiplier of the golden ratio: cheap way to scatter
            // neighbouring channel indices into unrelated seeds.
            uint64_t k = (uint64_t)(c + 1);
            voices[c].rng.seed(seed + 0x9E3779B97F4A7C15ULL * k, (seed ^ k) + 1);
            voices[c].walkRecovery = 0.f;
            voices[c].walkDepth = 0.f;
            voices[c].walkCurve = 0.f;
        }
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        curveShape = CURVE_EXPONENTIAL;
        freezeJitter = false;
        perChannelEnvelopes = false;
        baseSeed = 0x5C1DECA1ULL;
        for (int c = 0; c < 16; c++) {
            voices[c].trigger.reset();
            voices[c].stage = STAGE_IDLE;
            voices[c].level = 1.f;
            voices[c].phase = 0.f;
        }
        reseedVoices(baseSeed);
    }

    static float walkStep(random::Xoroshiro128Plus& rng, float x) {
        // Xoroshiro128Plus has weak low bits, so take the top 32.
        float u = (float)(uint32_t)(rng() >> 32) * 2.32830629e-10f;  // [0, 1)
        return clamp(WALK_A * x + WALK_B * (2.f * u - 1.f), -1.f, 1.f);
    }

    void process(const ProcessArgs& args) override {
        // A disconnected TRIG reports zero channels; forcing one keeps OUT
        // sitting at 10 V instead of leaving the VCA downstream silent.
        int channels = std::max(1, inputs[TRIG_INPUT].getChannels());

        float recovery = std::pow(2.f, params[RECOVERY_PARAM].getValue());
        float depthKnob = params[DEPTH_PARAM].getValue();
        float jitter = params[JITTER_PARAM].getValue();
        float baseExponent = CURVE_EXPONENTS[curveShape];

        for (int c = 0; c < channels; c++) {
            Voice& v = voices[c];

            if (v.trigger.process(inputs[TRIG_INPUT].getPolyVoltage(c), 0.1f, 1.0f)) {
                if (!freezeJitter) {
                    v.walkRecovery = walkStep(v.rng, v.walkRecovery);
                    v.walkDepth = walkStep(v.rng, v.walkDepth);
                    v.walkCurve = walkStep(v.rng, v.walkCurve);
                }

                float depth = depthKnob + inputs[DEPTH_CV_INPUT].getPolyVoltage(c) / 10.f;
                depth = clamp(depth, 0.f, 1.f);
                depth = clamp(depth * (1.f + JITTER_DEPTH * jitter * v.walkDepth), 0.f, 1.f);

                v.recTime = std::max(0.001f,
                                     recovery * (1.f + JITTER_RECOVERY * jitter * v.walkRecovery));
                v.exponent = std::max(0.05f,
                                      baseExponent * (1.f + JITTER_CURVE * jitter * v.walkCurve));

                // Retriggering mid-recovery must never step the level up, so
                // the floor can only ever be at or below where we already are.
                v.floorLevel = std::min(1.f - depth, v.level);
                v.attackStart = v.level;
                v.phase = 0.f;
                v.stage = STAGE_ATTACK;
            }

            switch (v.stage) {
                case STAGE_ATTACK:
                    v.phase += args.sampleTime / ATTACK_TIME;
                    if (v.phase >= 1.f) {
                        v.level = v.floorLevel;
                        v.phase = 0.f;
                        v.stage = STAGE_HOLD;
                    }
                    else {
                        v.level = v.attackStart + (v.floorLevel - v.attackStart) * v.phase;
                    }
                    break;

                case STAGE_HOLD:
                    v.level = v.floorLevel;
                    v.phase += args.sampleTime / HOLD_TIME;
                    if (v.phase >= 1.f) {
                        v.phase = 0.f;
                        v.stage = STAGE_RECOVER;
                    }
                    break;

                case STAGE_RECOVER:
                    v.phase += args.sampleTime / v.recTime;
                    if (v.phase >= 1.f) {
                        v.level = 1.f;
                        v.phase = 0.f;
                        v.stage = STAGE_IDLE;
                        // Only a recovery that ran to completion counts as a
                        // cycle. A retrigger cuts it short and fires nothing,
                        // otherwise EOC would degrade into a copy of TRIG at
                        // fast tempos.
                        v.eocPulse.trigger(1e-3f);
                    }
                    else {
                        v.level = v.floorLevel +
                                  (1.f - v.floorLevel) * std::pow(v.phase, v.exponent);
                    }
                    break;

                default:
                    v.level = 1.f;
                    break;
            }

            outputs[ENV_OUTPUT].setVoltage(10.f * v.level, c);
            outputs[EOC_OUTPUT].setVoltage(v.eocPulse.process(args.sampleTime) ? 10.f : 0.f, c);
        }

        outputs[ENV_OUTPUT].setChannels(channels);
        outputs[EOC_OUTPUT].setChannels(channels);

        float baseLevel = params[LEVEL_PARAM].getValue();
        meterGain = voices[0].level * baseLevel;

        // -- VCA ------------------------------------------------------------
        //
        // Nothing patched in means nothing to attenuate: leave both audio
        // outputs at zero channels so downstream sees an unconnected jack
        // rather than silence.
        bool leftPatched = inputs[IN_L_INPUT].isConnected();
        bool rightPatched = inputs[IN_R_INPUT].isConnected();
        if (!leftPatched && !rightPatched) {
            outputs[OUT_L_OUTPUT].setChannels(0);
            outputs[OUT_R_OUTPUT].setChannels(0);
            return;
        }

        int audioChannels = std::max(1, std::max(inputs[IN_L_INPUT].getChannels(),
                                                 inputs[IN_R_INPUT].getChannels()));

        for (int c = 0; c < audioChannels; c++) {
            // One envelope for everything unless the user asked otherwise, so
            // a stereo pair ducks symmetrically.
            int e = perChannelEnvelopes ? std::min(c, channels - 1) : 0;
            float gain = voices[e].level * baseLevel;

            float left = inputs[IN_L_INPUT].getPolyVoltage(c);
            // Right is normalled to left: one cable feeds both outputs, which
            // turns the module into a mono-to-stereo ducker for free.
            float right = rightPatched ? inputs[IN_R_INPUT].getPolyVoltage(c) : left;

            outputs[OUT_L_OUTPUT].setVoltage(left * gain, c);
            outputs[OUT_R_OUTPUT].setVoltage(right * gain, c);
        }

        outputs[OUT_L_OUTPUT].setChannels(audioChannels);
        outputs[OUT_R_OUTPUT].setChannels(audioChannels);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "curveShape", json_integer(curveShape));
        json_object_set_new(root, "freezeJitter", json_boolean(freezeJitter));
        json_object_set_new(root, "perChannelEnvelopes", json_boolean(perChannelEnvelopes));
        // Stored as a string: a 64-bit seed does not survive JSON's double.
        json_object_set_new(root, "baseSeed",
                            json_string(string::f("%" PRIu64, baseSeed).c_str()));
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root)
            return;
        if (json_t* j = json_object_get(root, "curveShape"))
            curveShape = clamp((int) json_integer_value(j), 0, NUM_CURVE_SHAPES - 1);
        if (json_t* j = json_object_get(root, "freezeJitter"))
            freezeJitter = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "perChannelEnvelopes"))
            perChannelEnvelopes = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "baseSeed")) {
            if (json_is_string(j))
                baseSeed = strtoull(json_string_value(j), NULL, 10);
            reseedVoices(baseSeed);
        }
    }
};


/** Vertical slider that sets the VCA ceiling and doubles as the gain meter,
the way Fundamental's VCA-1 does. The bar is the gain actually applied, so the
duck is visible on every hit; the handle is where the ceiling sits. */
struct LevelSlider : app::SliderKnob {
    SideChain* module = NULL;

    LevelSlider() {
        box.size = mm2px(Vec(8.f, 40.f));
    }

    void draw(const DrawArgs& args) override {
        const float w = box.size.x;
        const float h = box.size.y;
        const float inset = 1.5f;

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 2.f);
        nvgFillColor(args.vg, nvgRGB(0x14, 0x16, 0x1c));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(0x33, 0x38, 0x4a));
        nvgStroke(args.vg);

        const float track = h - inset * 2.f;
        float gain = module ? clamp(module->meterGain, 0.f, 1.f) : 1.f;
        float barH = track * gain;
        if (barH > 0.5f) {
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, inset, h - inset - barH, w - inset * 2.f, barH, 1.f);
            nvgFillColor(args.vg, AnimatekUI::logoBlue(210));
            nvgFill(args.vg);
        }

        float value = 1.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            value = pq->getScaledValue();
        float y = h - inset - track * value;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.5f, y - 1.f, w - 1.f, 2.f);
        nvgFillColor(args.vg, nvgRGB(0xe8, 0xe8, 0xe8));
        nvgFill(args.vg);
    }
};


struct SideChainWidget : ModuleWidget {
    SideChainWidget(SideChain* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/SideChain-light.svg"),
                             asset::plugin(pluginInstance, "res/SideChain.svg")));

        constexpr float W = 30.48f;  // 6 HP
        constexpr float C = W * 0.5f;
        constexpr float X1 = 8.6f;
        constexpr float X2 = W - 8.6f;

        auto* title = new TextLabel("SIDECHAIN", mm2px(Vec(C - 12.f, 3.2f)),
                                    mm2px(Vec(24.f, 5.f)));
        title->fontSize = 10.f;
        title->color = nvgRGB(0x2C, 0x7F, 0xFF);
        addChild(title);

        auto addLabel = [&](const char* text, float cx, float y, float w) {
            auto label = createWidget<TextLabel>(mm2px(Vec(cx - w * 0.5f, y)));
            label->box.size = mm2px(Vec(w, 3.f));
            label->text = text;
            label->fontSize = 7.f;
            addChild(label);
        };

        // Knobs stacked down the left, meter-slider filling the right.
        constexpr float KX = 9.5f;
        auto addKnob = [&](const char* text, float y, int paramId) {
            addLabel(text, KX, y, 16.f);
            addParam(createParamCentered<RoundBlackKnob>(
                mm2px(Vec(KX, y + 7.0f)), module, paramId));
        };

        addKnob("RECOVERY", 12.0f, SideChain::RECOVERY_PARAM);
        addKnob("DEPTH", 27.0f, SideChain::DEPTH_PARAM);
        addKnob("JITTER", 42.0f, SideChain::JITTER_PARAM);

        auto* slider = createParam<LevelSlider>(mm2px(Vec(18.0f, 13.0f)), module,
                                                SideChain::LEVEL_PARAM);
        slider->module = module;
        addParam(slider);

        // Four jack rows 16 mm apart, read top to bottom as signal flow:
        // control in, audio in, audio out, CV out.
        auto addIn = [&](const char* text, float cx, float y, int inputId) {
            addLabel(text, cx, y, 14.f);
            addInput(createInputCentered<TekInputPort>(
                mm2px(Vec(cx, y + 7.5f)), module, inputId));
        };
        auto addOut = [&](const char* text, float cx, float y, int outputId) {
            addLabel(text, cx, y, 14.f);
            addOutput(createOutputCentered<TekOutputPort>(
                mm2px(Vec(cx, y + 7.5f)), module, outputId));
        };

        addIn("TRIG", X1, 59.0f, SideChain::TRIG_INPUT);
        addIn("D-CV", X2, 59.0f, SideChain::DEPTH_CV_INPUT);
        addIn("IN L", X1, 75.0f, SideChain::IN_L_INPUT);
        addIn("IN R", X2, 75.0f, SideChain::IN_R_INPUT);
        addOut("OUT L", X1, 91.0f, SideChain::OUT_L_OUTPUT);
        addOut("OUT R", X2, 91.0f, SideChain::OUT_R_OUTPUT);
        addOut("ENV", X1, 107.0f, SideChain::ENV_OUTPUT);
        addOut("EOC", X2, 107.0f, SideChain::EOC_OUTPUT);
    }

    void appendContextMenu(ui::Menu* menu) override {
        SideChain* module = dynamic_cast<SideChain*>(this->module);
        if (!module)
            return;

        menu->addChild(new ui::MenuSeparator);
        menu->addChild(createSubmenuItem("Recovery curve", CURVE_NAMES[module->curveShape],
                                         [=](ui::Menu* sub) {
            for (int i = 0; i < NUM_CURVE_SHAPES; i++) {
                sub->addChild(createCheckMenuItem(
                    CURVE_NAMES[i], "",
                    [=]() { return module->curveShape == i; },
                    [=]() { module->curveShape = i; }));
            }
        }));

        menu->addChild(createCheckMenuItem(
            "Freeze jitter", "",
            [=]() { return module->freezeJitter; },
            [=]() { module->freezeJitter ^= true; }));

        menu->addChild(createCheckMenuItem(
            "Per-channel envelopes", "",
            [=]() { return module->perChannelEnvelopes; },
            [=]() { module->perChannelEnvelopes ^= true; }));

        menu->addChild(createMenuItem("Reset jitter seed", "", [=]() {
            module->baseSeed = random::u64();
            module->reseedVoices(module->baseSeed);
        }));

        appendPanelThemeMenu(menu);
    }
};


Model* modelSideChain = createModel<SideChain, SideChainWidget>("SideChain");
