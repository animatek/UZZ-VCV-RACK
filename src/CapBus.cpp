#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"

#include <algorithm>
#include <cmath>

using AnimatekUI::TekInputPort;
using AnimatekUI::TekOutputPort;
using AnimatekUI::TextLabel;

// ============================================================================
// BUS - where a chain of CAPs comes out
// ============================================================================
//
// CAPs placed side by side pass their stereo, post-fader and panned, along to
// the right without cables (see CapBusMessage in plugin.hpp). Nothing comes
// out of the chain until it reaches a BUS: that is what keeps two CAPs that
// already sat together in an older patch sounding exactly as they did.
//
// A BUS does three things with what arrives from its left:
//   SEND    the bus as it is, to feed an effect,
//   RETURN  what comes back replaces the bus, crossfaded by WET,
//   MIX     the result times LEVEL, which is also passed on to the right.
//
// So a BUS in the middle of a row is an insert on the CAPs to its left, and
// the chain carries on past it: CAP CAP BUS(reverb) CAP CAP BUS(master).
// ============================================================================

struct CapBus : Module {
    enum ParamId { WET_PARAM, LEVEL_PARAM, PARAMS_LEN };
    enum InputId { RETURN_L_INPUT, RETURN_R_INPUT, INPUTS_LEN };
    enum OutputId { SEND_L_OUTPUT, SEND_R_OUTPUT, MIX_L_OUTPUT, MIX_R_OUTPUT, OUTPUTS_LEN };
    enum LightId { LINK_LIGHT, LIGHTS_LEN };

    CapBusMessage busMessages[2];

    // Peak of each side of MIX for the meter. Written by the audio thread and
    // read by the UI; a torn float is one wrong frame of a meter at worst.
    float meterL = 0.f;
    float meterR = 0.f;

    CapBus() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // 100% by default because this is an insert: what comes back replaces
        // the bus, and an effect with its own mix control keeps that job. Turn
        // it down to blend a fully wet effect (a reverb at 100%) with the dry.
        configParam(WET_PARAM, 0.f, 1.f, 1.f, "Return wet", "%", 0.f, 100.f);
        configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Master level", "%", 0.f, 100.f);

        configInput(RETURN_L_INPUT, "Return left");
        configInput(RETURN_R_INPUT, "Return right (normalled to left)");
        configOutput(SEND_L_OUTPUT, "Send left");
        configOutput(SEND_R_OUTPUT, "Send right");
        configOutput(MIX_L_OUTPUT, "Mix left");
        configOutput(MIX_R_OUTPUT, "Mix right");
        configLight(LINK_LIGHT, "Chain linked on the left");

        leftExpander.producerMessage = &busMessages[0];
        leftExpander.consumerMessage = &busMessages[1];
    }

    bool chainedOnLeft() {
        Module* left = leftExpander.module;
        return left && (left->model == modelSideChain || left->model == modelCapBus);
    }

    void process(const ProcessArgs& args) override {
        CapBusMessage bus = capBusReceive(*this);

        outputs[SEND_L_OUTPUT].setVoltage(bus.left);
        outputs[SEND_R_OUTPUT].setVoltage(bus.right);

        // Unpatched, the return is not silence but no return at all: the bus
        // goes straight through, so a BUS with no effect is just the master.
        if (inputs[RETURN_L_INPUT].isConnected() || inputs[RETURN_R_INPUT].isConnected()) {
            float retL = inputs[RETURN_L_INPUT].getVoltageSum();
            float retR = inputs[RETURN_R_INPUT].isConnected()
                             ? inputs[RETURN_R_INPUT].getVoltageSum()
                             : retL;
            float wet = params[WET_PARAM].getValue();
            bus.left = bus.left + (retL - bus.left) * wet;
            bus.right = bus.right + (retR - bus.right) * wet;
        }

        float level = params[LEVEL_PARAM].getValue();
        bus.left *= level;
        bus.right *= level;

        outputs[MIX_L_OUTPUT].setVoltage(bus.left);
        outputs[MIX_R_OUTPUT].setVoltage(bus.right);
        capBusSend(*this, bus);

        // Instant attack, about 300 ms to fall 20 dB: fast enough to follow a
        // kick, slow enough to read.
        const float fall = std::exp(-args.sampleTime / 0.13f);
        meterL = std::max(std::abs(bus.left), meterL * fall);
        meterR = std::max(std::abs(bus.right), meterR * fall);

        lights[LINK_LIGHT].setBrightness(chainedOnLeft() ? 1.f : 0.f);
    }

    /** Bypassed, the BUS steps out of the way: the chain passes through it
    untouched, so nothing to its right goes silent. */
    void processBypass(const ProcessArgs& args) override {
        CapBusMessage bus = capBusReceive(*this);
        outputs[MIX_L_OUTPUT].setVoltage(bus.left);
        outputs[MIX_R_OUTPUT].setVoltage(bus.right);
        capBusSend(*this, bus);
    }
};


/** Two bars, left and right, for the peak level at MIX. The scale is in dB
from -36 to +6 relative to 5 V, Rack's nominal audio level; the part above
5 V turns amber, since that is where whatever comes next may start to clip. */
struct BusMeter : TransparentWidget {
    CapBus* bus = NULL;

    static constexpr float DB_MIN = -36.f;
    static constexpr float DB_MAX = 6.f;

    static float toNorm(float volts) {
        if (volts <= 0.f)
            return 0.f;
        float db = 20.f * std::log10(volts / 5.f);
        return clamp((db - DB_MIN) / (DB_MAX - DB_MIN), 0.f, 1.f);
    }

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 2.f);
        nvgFillColor(args.vg, nvgRGB(0x14, 0x16, 0x1c));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(0x33, 0x38, 0x4a));
        nvgStroke(args.vg);
    }

    /** The bars go on the light layer so they stay lit in a dimmed room,
    like CAP's meter. */
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            const float inset = 1.5f;
            const float gap = 0.8f;
            const float track = box.size.y - inset * 2.f;
            const float bw = (box.size.x - inset * 2.f - gap) * 0.5f;
            const float zeroDb = toNorm(5.f);
            // Without a module (the browser, the library site) show a still,
            // plausible level rather than an empty box.
            float levels[2] = {bus ? toNorm(bus->meterL) : 0.62f,
                               bus ? toNorm(bus->meterR) : 0.58f};

            for (int i = 0; i < 2; i++) {
                float x = inset + i * (bw + gap);
                float h = track * levels[i];
                if (h <= 0.5f)
                    continue;
                float safeH = std::min(h, track * zeroDb);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, x, box.size.y - inset - safeH, bw, safeH);
                nvgFillColor(args.vg, AnimatekUI::logoBlue());
                nvgFill(args.vg);
                if (h > safeH) {
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, x, box.size.y - inset - h, bw, h - safeH);
                    nvgFillColor(args.vg, nvgRGB(0xFF, 0xA0, 0x28));
                    nvgFill(args.vg);
                }
            }

            // Tick at 0 dB (5 V).
            float y = box.size.y - inset - track * zeroDb;
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.5f, y - 0.4f, box.size.x - 1.f, 0.8f);
            nvgFillColor(args.vg, nvgRGBA(0xe8, 0xe8, 0xe8, 160));
            nvgFill(args.vg);
        }
        TransparentWidget::drawLayer(args, layer);
    }
};


struct CapBusWidget : ModuleWidget {
    CapBusWidget(CapBus* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/CapBus.svg")));

        constexpr float W = 20.32f;  // 4 HP
        constexpr float CX = W * 0.5f;
        constexpr float X1 = 5.6f;
        constexpr float X2 = W - 5.6f;

        auto* moduleName = new TextLabel("BUS", mm2px(Vec(1.2f, 119.9f)),
                                         mm2px(Vec(11.f, 5.8f)));
        moduleName->fontSize = 16.f;
        moduleName->color = nvgRGB(0x2C, 0x7F, 0xFF);
        addChild(moduleName);

        auto addLabel = [&](const char* text, float cx, float y, float w) {
            auto label = createWidget<TextLabel>(mm2px(Vec(cx - w * 0.5f, y)));
            label->box.size = mm2px(Vec(w, 3.f));
            label->text = text;
            label->fontSize = 7.f;
            addChild(label);
        };

        // The LED says whether a CAP or another BUS sits against the left edge,
        // which is the only thing that can go wrong with a chain made without
        // cables: a gap of one HP and nothing arrives.
        addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(CX, 4.0f)), module,
                                                            CapBus::LINK_LIGHT));

        // Same spacing as CAP's knobs: label, then the knob centre 8.5 mm down.
        addLabel("WET", CX, 8.0f, 16.f);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(CX, 16.5f)), module,
                                                          CapBus::WET_PARAM));
        addLabel("LEVEL", CX, 24.0f, 16.f);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(CX, 32.5f)), module,
                                                          CapBus::LEVEL_PARAM));

        // One label per stereo pair, left jack on the left. As on CAP, what
        // comes in sits above the panel line and what goes out below it.
        auto addPair = [&](const char* text, float y, int leftId, int rightId, bool input) {
            addLabel(text, CX, y, 18.f);
            if (input) {
                addInput(createInputCentered<TekInputPort>(mm2px(Vec(X1, y + 7.5f)), module, leftId));
                addInput(createInputCentered<TekInputPort>(mm2px(Vec(X2, y + 7.5f)), module, rightId));
            }
            else {
                addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(X1, y + 7.5f)), module, leftId));
                addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(X2, y + 7.5f)), module, rightId));
            }
        };

        auto* meter = createWidget<BusMeter>(mm2px(Vec(CX - 4.f, 39.5f)));
        meter->box.size = mm2px(Vec(8.f, 18.f));
        meter->bus = module;
        addChild(meter);

        // The panel line at y = 74 separates RETURN from SEND and MIX, with the
        // same 14 mm rows and the same air around the line as CAP.
        addPair("RETURN", 60.0f, CapBus::RETURN_L_INPUT, CapBus::RETURN_R_INPUT, true);
        addPair("SEND", 76.5f, CapBus::SEND_L_OUTPUT, CapBus::SEND_R_OUTPUT, false);
        addPair("MIX", 90.5f, CapBus::MIX_L_OUTPUT, CapBus::MIX_R_OUTPUT, false);
    }
};


Model* modelCapBus = createModel<CapBus, CapBusWidget>("CapBus");
