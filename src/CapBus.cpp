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
//   MIX     the result times LEVEL and placed by PAN, which is also passed on
//           to the right.
//
// So a BUS in the middle of a row is an insert on the CAPs to its left, and
// the chain carries on past it: CAP CAP BUS(reverb) CAP CAP BUS(master).
// ============================================================================

struct CapBus : Module {
    // New entries at the end: the indices are what patches store.
    enum ParamId { WET_PARAM, LEVEL_PARAM, PAN_PARAM, PAN_CV_PARAM, PARAMS_LEN };
    enum InputId { RETURN_L_INPUT, RETURN_R_INPUT, PAN_CV_INPUT, INPUTS_LEN };
    enum OutputId { SEND_L_OUTPUT, SEND_R_OUTPUT, MIX_L_OUTPUT, MIX_R_OUTPUT, OUTPUTS_LEN };
    enum LightId { LINK_LIGHT, LIGHTS_LEN };

    CapBusMessage busMessages[2];

    // Peak of each side of MIX for the meter. Written by the audio thread and
    // read by the UI; a torn float is one wrong frame of a meter at worst.
    float meterL = 0.f;
    float meterR = 0.f;

    // The name at the top of the panel. Empty and not custom, the panel shows
    // the effect patched to it instead; anything typed in wins.
    std::string label;
    bool customLabel = false;

    CapBus() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // 100% by default because this is an insert: what comes back replaces
        // the bus, and an effect with its own mix control keeps that job. Turn
        // it down to blend a fully wet effect (a reverb at 100%) with the dry.
        configParam(WET_PARAM, 0.f, 1.f, 1.f, "Return wet", "%", 0.f, 100.f);
        configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Master level", "%", 0.f, 100.f);
        configParam(PAN_PARAM, -1.f, 1.f, 0.f, "Pan", "%", 0.f, 100.f);
        configParam(PAN_CV_PARAM, -1.f, 1.f, 0.f, "Pan CV amount", "%", 0.f, 100.f);

        configInput(RETURN_L_INPUT, "Return left");
        configInput(RETURN_R_INPUT, "Return right (normalled to left)");
        configInput(PAN_CV_INPUT, "Pan CV (±5 V sweeps it all at full amount)");
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

        // Balance, as on CAP: unity in the centre, the far side fading out.
        float pan = params[PAN_PARAM].getValue()
                    + params[PAN_CV_PARAM].getValue() * inputs[PAN_CV_INPUT].getVoltage() / 5.f;
        pan = clamp(pan, -1.f, 1.f);
        float level = params[LEVEL_PARAM].getValue();
        bus.left *= level * ((pan > 0.f) ? 1.f - pan : 1.f);
        bus.right *= level * ((pan < 0.f) ? 1.f + pan : 1.f);

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

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        label.clear();
        customLabel = false;
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "label", json_string(label.c_str()));
        json_object_set_new(root, "customLabel", json_boolean(customLabel));
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root)
            return;
        if (json_t* j = json_object_get(root, "label"))
            label = json_string_value(j) ? json_string_value(j) : "";
        if (json_t* j = json_object_get(root, "customLabel"))
            customLabel = json_boolean_value(j);
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


/** The master fader, which doubles as the stereo meter, as on CAP: the
handle is LEVEL and the two bars are the peak at MIX, left and right. The bars
are in dB from -36 to +6 relative to 5 V, Rack's nominal audio level; the part
above 5 V turns amber, where whatever comes next may start to clip. */
struct BusLevelSlider : app::SliderKnob {
    CapBus* bus = NULL;

    static constexpr float DB_MIN = -36.f;
    static constexpr float DB_MAX = 6.f;
    static constexpr float INSET = 1.5f;

    static float toNorm(float volts) {
        if (volts <= 0.f)
            return 0.f;
        float db = 20.f * std::log10(volts / 5.f);
        return clamp((db - DB_MIN) / (DB_MAX - DB_MIN), 0.f, 1.f);
    }

    void draw(const DrawArgs& args) override {
        const float w = box.size.x;
        const float h = box.size.y;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 2.f);
        nvgFillColor(args.vg, nvgRGB(0x14, 0x16, 0x1c));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(0x33, 0x38, 0x4a));
        nvgStroke(args.vg);

        float value = 1.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            value = pq->getScaledValue();
        float y = h - INSET - (h - INSET * 2.f) * value;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.5f, y - 1.f, w - 1.f, 2.f);
        nvgFillColor(args.vg, nvgRGB(0xe8, 0xe8, 0xe8));
        nvgFill(args.vg);
    }

    /** The bars go on the light layer so they stay lit in a dimmed room. */
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            const float gap = 0.8f;
            const float track = box.size.y - INSET * 2.f;
            const float bw = (box.size.x - INSET * 2.f - gap) * 0.5f;
            const float zeroDb = toNorm(5.f);
            // Without a module (the browser, the library site) show a still,
            // plausible level rather than an empty box.
            float levels[2] = {bus ? toNorm(bus->meterL) : 0.62f,
                               bus ? toNorm(bus->meterR) : 0.58f};

            for (int i = 0; i < 2; i++) {
                float x = INSET + i * (bw + gap);
                float h = track * levels[i];
                if (h <= 0.5f)
                    continue;
                float safeH = std::min(h, track * zeroDb);
                nvgBeginPath(args.vg);
                nvgRect(args.vg, x, box.size.y - INSET - safeH, bw, safeH);
                nvgFillColor(args.vg, AnimatekUI::logoBlue());
                nvgFill(args.vg);
                if (h > safeH) {
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, x, box.size.y - INSET - h, bw, h - safeH);
                    nvgFillColor(args.vg, nvgRGB(0xFF, 0xA0, 0x28));
                    nvgFill(args.vg);
                }
            }

            // Tick at 0 dB (5 V).
            float y = box.size.y - INSET - track * zeroDb;
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.5f, y - 0.4f, box.size.x - 1.f, 0.8f);
            nvgFillColor(args.vg, nvgRGBA(0xe8, 0xe8, 0xe8, 160));
            nvgFill(args.vg);
        }
        app::SliderKnob::drawLayer(args, layer);
    }
};


/** The name at the top: click to type one ("MIX", "REVERB"), Enter or a click
elsewhere to leave it. Left empty, it shows the module patched to RETURN (or
fed by SEND), so a BUS names itself after its effect; with nothing patched it
shows "MIX" in grey, since a BUS with no effect is a master. */
struct BusNameField : ui::TextField {
    CapBus* bus = NULL;
    std::string autoName;
    static constexpr float FONT_SIZE = 9.f;

    BusNameField() {
        placeholder = "MIX";
    }

    bool editing() {
        return APP->event->selectedWidget == this;
    }

    void step() override {
        // While nothing is typed in, the field shows the detected name. It is
        // kept out of `text` so that typing starts from an empty field.
        if (bus && !bus->customLabel && !editing())
            text = autoName;
        ui::TextField::step();
    }

    void onChange(const ChangeEvent& e) override {
        if (bus && editing()) {
            bus->label = text;
            bus->customLabel = !text.empty();
        }
        ui::TextField::onChange(e);
    }

    void onSelect(const SelectEvent& e) override {
        // Starting to type replaces the detected name rather than editing it.
        if (bus && !bus->customLabel)
            setText("");
        ui::TextField::onSelect(e);
    }

    void onAction(const ActionEvent& e) override {
        APP->event->setSelectedWidget(NULL);
        e.consume(this);
    }

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 1.5f);
        nvgFillColor(args.vg, nvgRGB(0x14, 0x16, 0x1c));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, editing() ? AnimatekUI::logoBlue() : nvgRGB(0x33, 0x38, 0x4a));
        nvgStroke(args.vg);
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            std::shared_ptr<window::Font> font = APP->window->uiFont;
            if (font && font->handle >= 0) {
                nvgScissor(args.vg, 0.f, 0.f, box.size.x, box.size.y);
                if (editing()) {
                    // While typing: left-aligned with a caret, Rack's own way.
                    bndSetFont(font->handle);
                    NVGcolor color = nvgRGB(0xe8, 0xe8, 0xe8);
                    NVGcolor highlight = AnimatekUI::logoBlue(128);
                    bndIconLabelCaret(args.vg, 1.f, 0.f, box.size.x - 2.f, box.size.y, -1, color,
                                      FONT_SIZE, text.c_str(), highlight,
                                      std::min(cursor, selection), std::max(cursor, selection));
                }
                else {
                    bool empty = text.empty();
                    nvgFontSize(args.vg, FONT_SIZE);
                    nvgFontFaceId(args.vg, font->handle);
                    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                    nvgFillColor(args.vg, empty ? nvgRGB(0x5a, 0x60, 0x70) : AnimatekUI::logoBlue());
                    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f,
                            empty ? placeholder.c_str() : text.c_str(), NULL);
                }
                nvgResetScissor(args.vg);
            }
        }
        Widget::drawLayer(args, layer);
    }

    int getTextPosition(math::Vec mousePos) override {
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font || font->handle < 0)
            return 0;
        bndSetFont(font->handle);
        return bndIconLabelTextPosition(APP->window->vg, 1.f, 0.f, box.size.x - 2.f, box.size.y,
                                        -1, FONT_SIZE, text.c_str(), mousePos.x, mousePos.y);
    }
};


struct CapBusWidget : ModuleWidget {
    BusNameField* nameField = NULL;

    CapBusWidget(CapBus* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/CapBus.svg")));

        constexpr float W = 20.32f;  // 4 HP
        constexpr float X1 = 5.6f;
        constexpr float X2 = W - 5.6f;
        // The knob column and the fader beside it, as on CAP.
        constexpr float XK = 5.4f;

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
        auto line = [&](float ax, float ay, float bx, float by) {
            addChild(new AnimatekUI::ConnectorLine(mm2px(ax), mm2px(ay), mm2px(bx), mm2px(by)));
        };

        nameField = createWidget<BusNameField>(mm2px(Vec(1.2f, 1.6f)));
        nameField->box.size = mm2px(Vec(W - 2.4f, 4.6f));
        nameField->bus = module;
        if (module && module->customLabel)
            nameField->setText(module->label);
        addChild(nameField);

        // The LED says whether a CAP or another BUS sits against the left edge,
        // which is the only thing that can go wrong with a chain made without
        // cables: a gap of one HP and nothing arrives.
        addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(W * 0.5f, 8.6f)), module,
                                                            CapBus::LINK_LIGHT));

        addLabel("WET", XK, 11.0f, 10.f);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(XK, 19.0f)), module,
                                                          CapBus::WET_PARAM));
        addLabel("PAN", XK, 25.5f, 10.f);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(XK, 33.5f)), module,
                                                          CapBus::PAN_PARAM));
        // The CV and its amount, joined by a hairline: the line says that
        // trimmer is that jack's.
        addParam(createParamCentered<Trimpot>(mm2px(Vec(XK, 43.0f)), module,
                                              CapBus::PAN_CV_PARAM));
        addInput(createInputCentered<TekInputPort>(mm2px(Vec(XK, 52.5f)), module,
                                                   CapBus::PAN_CV_INPUT));
        line(XK, 45.6f, XK, 48.3f);

        auto* slider = createParam<BusLevelSlider>(mm2px(Vec(11.3f, 11.5f)), module,
                                                   CapBus::LEVEL_PARAM);
        slider->box.size = mm2px(Vec(7.6f, 45.f));
        slider->bus = module;
        addParam(slider);

        // One label per stereo pair, left jack on the left. As on CAP, what
        // comes in sits above the panel line (y = 77.5) and what goes out below.
        auto addPair = [&](const char* text, float y, int leftId, int rightId, bool input) {
            addLabel(text, W * 0.5f, y, 18.f);
            if (input) {
                addInput(createInputCentered<TekInputPort>(mm2px(Vec(X1, y + 7.5f)), module, leftId));
                addInput(createInputCentered<TekInputPort>(mm2px(Vec(X2, y + 7.5f)), module, rightId));
            }
            else {
                addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(X1, y + 7.5f)), module, leftId));
                addOutput(createOutputCentered<TekOutputPort>(mm2px(Vec(X2, y + 7.5f)), module, rightId));
            }
        };
        addPair("RETURN", 63.5f, CapBus::RETURN_L_INPUT, CapBus::RETURN_R_INPUT, true);
        addPair("SEND", 80.0f, CapBus::SEND_L_OUTPUT, CapBus::SEND_R_OUTPUT, false);
        addPair("MIX", 94.5f, CapBus::MIX_L_OUTPUT, CapBus::MIX_R_OUTPUT, false);
    }

    /** The module at the far end of the first cable on a port, if any. */
    static std::string moduleOnCable(PortWidget* port, bool wantOutputEnd) {
        if (!port || !APP->scene || !APP->scene->rack)
            return "";
        for (CableWidget* cw : APP->scene->rack->getCompleteCablesOnPort(port)) {
            if (!cw->cable)
                continue;
            Module* m = wantOutputEnd ? cw->cable->outputModule : cw->cable->inputModule;
            if (m && m->model)
                return m->model->name;
        }
        return "";
    }

    void step() override {
        // What the effect is called: whatever feeds RETURN, or failing that
        // whatever SEND feeds.
        if (nameField && module) {
            std::string name = moduleOnCable(getInput(CapBus::RETURN_L_INPUT), true);
            if (name.empty())
                name = moduleOnCable(getOutput(CapBus::SEND_L_OUTPUT), false);
            nameField->autoName = name;
        }
        ModuleWidget::step();
    }
};


Model* modelCapBus = createModel<CapBus, CapBusWidget>("CapBus");
