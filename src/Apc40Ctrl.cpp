#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"

#include <array>

using AnimatekUI::TextLabel;

static constexpr int NUM_TRACK_CONTROLS = 8;
static constexpr int NUM_DEVICE_KNOBS = 8;
static constexpr int NUM_EXTRA_CONTROLS = 3;
static constexpr int NUM_FADERS = 8;
static constexpr int NUM_APC_CONTROLS = NUM_TRACK_CONTROLS + NUM_DEVICE_KNOBS + NUM_EXTRA_CONTROLS;
static constexpr int APC_MIDI_CHANNEL = 0; // Knob/CC controls live on channel 1.
static constexpr uint8_t FADER_CC = 7;     // Faders share CC7 across channels 1-8.

static constexpr std::array<int, NUM_APC_CONTROLS> APC_CC_NUMBERS = {
    48, 49, 50, 51,
    52, 53, 54, 55,
    16, 17, 18, 19,
    20, 21, 22, 23,
    47, 14, 11
};

static constexpr std::array<const char*, NUM_APC_CONTROLS> APC_OUTPUT_NAMES = {
    "Track control 1", "Track control 2", "Track control 3", "Track control 4",
    "Track control 5", "Track control 6", "Track control 7", "Track control 8",
    "Device knob 1", "Device knob 2", "Device knob 3", "Device knob 4",
    "Device knob 5", "Device knob 6", "Device knob 7", "Device knob 8",
    "Cue level", "Master", "Crossfader"
};

static constexpr std::array<const char*, NUM_TRACK_CONTROLS> APC_TRACK_LABELS = {
    "T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8",
};

static constexpr std::array<const char*, NUM_DEVICE_KNOBS> APC_DEVICE_LABELS = {
    "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8",
};

static constexpr std::array<const char*, NUM_EXTRA_CONTROLS> APC_EXTRA_LABELS = {
    "CUE", "MASTER", "XFAD",
};

struct Apc40Ctrl : Module {
    enum ParamIds {
        ENUMS(ATTENUVERT_PARAM, NUM_APC_CONTROLS),
        NUM_PARAMS
    };
    enum InputIds { NUM_INPUTS };
    enum OutputIds {
        ENUMS(CV_OUTPUT, NUM_APC_CONTROLS),
        ENUMS(FADER_OUTPUT, NUM_FADERS),
        NUM_OUTPUTS
    };
    enum LightIds {
        MIDI_LIGHT,
        NUM_LIGHTS
    };

    midi::InputQueue midiInput;
    float values[NUM_APC_CONTROLS] = {};
    float faderValues[NUM_FADERS] = {};
    float midiLightLevel = 0.f;

    Apc40Ctrl() {
        // -1 = omni: faders arrive on channels 1-8.
        midiInput.channel = -1;

        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < NUM_APC_CONTROLS; i++) {
            configParam(ATTENUVERT_PARAM + i, -1.f, 1.f, 1.f,
                        string::f("%s attenuverter", APC_OUTPUT_NAMES[i]), "x");
        }
        for (int i = 0; i < NUM_APC_CONTROLS; i++) {
            configOutput(CV_OUTPUT + i,
                         string::f("%s (CC%d)", APC_OUTPUT_NAMES[i], APC_CC_NUMBERS[i]));
        }
        for (int i = 0; i < NUM_FADERS; i++) {
            configOutput(FADER_OUTPUT + i,
                         string::f("Fader %d (CC%d ch%d)", i + 1, FADER_CC, i + 1));
        }
    }

    void onReset() override {
        midiInput.reset();
        midiInput.channel = -1;
        for (float& v : values)
            v = 0.f;
        for (float& v : faderValues)
            v = 0.f;
        midiLightLevel = 0.f;
    }

    void processMidiMessage(const midi::Message& msg) {
        uint8_t raw = msg.bytes[0];
        if ((raw & 0xF0) != 0xB0)
            return;
        uint8_t channel = raw & 0x0F;
        uint8_t cc = msg.bytes[1];
        uint8_t value = msg.bytes[2];

        if (cc == FADER_CC && channel < NUM_FADERS) {
            faderValues[channel] = (value / 127.f) * 10.f;
            midiLightLevel = 1.f;
            return;
        }

        if (channel != APC_MIDI_CHANNEL)
            return;
        for (int i = 0; i < NUM_APC_CONTROLS; i++) {
            if (cc == APC_CC_NUMBERS[i]) {
                values[i] = (value / 127.f) * 10.f;
                midiLightLevel = 1.f;
                return;
            }
        }
    }

    void process(const ProcessArgs& args) override {
        midiInput.channel = -1;

        midi::Message msg;
        while (midiInput.tryPop(&msg, args.frame))
            processMidiMessage(msg);

        for (int i = 0; i < NUM_APC_CONTROLS; i++) {
            float scale = params[ATTENUVERT_PARAM + i].getValue();
            outputs[CV_OUTPUT + i].setVoltage(values[i] * scale);
        }
        for (int i = 0; i < NUM_FADERS; i++) {
            outputs[FADER_OUTPUT + i].setVoltage(faderValues[i]);
        }

        midiLightLevel = std::max(0.f, midiLightLevel - args.sampleTime * 10.f);
        lights[MIDI_LIGHT].setBrightness(midiLightLevel);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "midiInput", midiInput.toJson());
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root)
            return;
        if (json_t* j = json_object_get(root, "midiInput"))
            midiInput.fromJson(j);
        midiInput.channel = -1;
    }
};

using ApcLabel = TextLabel;
struct ApcWhiteSeparator : widget::TransparentWidget {
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1)
            return;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0.f, box.size.y * 0.5f);
        nvgLineTo(args.vg, box.size.x, box.size.y * 0.5f);
        nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 220));
        nvgStrokeWidth(args.vg, 0.7f);
        nvgStroke(args.vg);
    }
};

struct ApcVerticalSeparator : widget::TransparentWidget {
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1)
            return;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, box.size.x * 0.5f, 0.f);
        nvgLineTo(args.vg, box.size.x * 0.5f, box.size.y);
        nvgStrokeColor(args.vg, nvgRGBA(255, 255, 255, 220));
        nvgStrokeWidth(args.vg, 0.7f);
        nvgStroke(args.vg);
    }
};

struct Apc40CtrlWidget : ModuleWidget {
    Apc40CtrlWidget(Apc40Ctrl* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/Apc40Ctrl-light.svg"),
            asset::plugin(pluginInstance, "res/Apc40Ctrl.svg")));

        auto title = createWidget<ApcLabel>(mm2px(Vec(0.f, 4.0f)));
        title->box.size = mm2px(Vec(50.8f, 3.5f));
        title->text = "APC40 CONTROL";
        title->fontSize = 10.0f;
        addChild(title);

        // Vertical separator between knob/CC section and fader column.
        {
            auto sep = createWidget<ApcVerticalSeparator>(mm2px(Vec(50.3f, 4.f)));
            sep->box.size = mm2px(Vec(1.0f, 120.5f));
            addChild(sep);
        }

        // Fader column header.
        {
            auto lbl = createWidget<ApcLabel>(mm2px(Vec(50.8f, 4.0f)));
            lbl->box.size = mm2px(Vec(15.24f, 3.5f));
            lbl->text = "FADERS";
            lbl->fontSize = 9.0f;
            addChild(lbl);
        }

        // 8 fader outputs in the new 3HP column (CC7, MIDI ch 1-8).
        {
            const float faderX = 50.8f + 15.24f * 0.5f;
            const float faderLabelW = 8.5f;
            for (int i = 0; i < NUM_FADERS; i++) {
                float yOut = 19.0f + i * 14.f;
                float yLbl = yOut - 8.0f;

                auto lbl = createWidget<ApcLabel>(
                    mm2px(Vec(faderX - faderLabelW * 0.5f, yLbl)));
                lbl->box.size = mm2px(Vec(faderLabelW, 3.0f));
                lbl->text = string::f("F%d", i + 1);
                lbl->fontSize = 9.0f;
                addChild(lbl);

                addOutput(createOutputCentered<PJ301MPort>(
                    mm2px(Vec(faderX, yOut)), module, Apc40Ctrl::FADER_OUTPUT + i));
            }
        }

        static constexpr float XS[4] = {8.0f, 19.6f, 31.2f, 42.8f};
        static constexpr float EXTRA_XS[NUM_EXTRA_CONTROLS] = {13.8f, 25.4f, 37.0f};
        static constexpr float TRACK_LABEL_YS[2] = {12.6f, 33.6f};
        static constexpr float TRACK_KNOB_YS[2] = {19.0f, 40.0f};
        static constexpr float TRACK_OUTPUT_YS[2] = {27.8f, 48.8f};
        static constexpr float DEVICE_LABEL_YS[2] = {58.6f, 79.6f};
        static constexpr float DEVICE_KNOB_YS[2] = {65.0f, 86.0f};
        static constexpr float DEVICE_OUTPUT_YS[2] = {73.8f, 94.8f};
        static constexpr float EXTRA_LABEL_Y = 105.6f;
        static constexpr float EXTRA_KNOB_Y = 112.0f;
        static constexpr float EXTRA_OUTPUT_Y = 120.8f;
        static constexpr float LABEL_W = 8.5f;

        // y is the visual line position; box is centered around it.
        auto addSeparator = [&](float y) {
            auto sep = createWidget<ApcWhiteSeparator>(mm2px(Vec(4.2f, y - 0.5f)));
            sep->box.size = mm2px(Vec(42.4f, 1.0f));
            addChild(sep);
        };

        auto addCompactControl = [&](int output_idx, float x, float label_y,
                                     float knob_y, float output_y, const char* label) {
            auto lbl = createWidget<ApcLabel>(mm2px(Vec(x - LABEL_W / 2.f, label_y)));
            lbl->box.size = mm2px(Vec(LABEL_W, 3.0f));
            lbl->text = label;
            lbl->fontSize = 9.0f;
            addChild(lbl);

            addParam(createParamCentered<Trimpot>(mm2px(Vec(x, knob_y)),
                                                  module, Apc40Ctrl::ATTENUVERT_PARAM + output_idx));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(x, output_y)),
                                                       module, Apc40Ctrl::CV_OUTPUT + output_idx));
        };

        addSeparator(10.0f);
        for (int i = 0; i < NUM_TRACK_CONTROLS; i++) {
            int row = i / 4;
            float x = XS[i % 4];
            addCompactControl(i, x, TRACK_LABEL_YS[row], TRACK_KNOB_YS[row],
                              TRACK_OUTPUT_YS[row], APC_TRACK_LABELS[i]);
        }

        addSeparator(56.0f);

        for (int i = 0; i < NUM_DEVICE_KNOBS; i++) {
            int output_idx = NUM_TRACK_CONTROLS + i;
            int row = i / 4;
            float x = XS[i % 4];

            addCompactControl(output_idx, x, DEVICE_LABEL_YS[row], DEVICE_KNOB_YS[row],
                              DEVICE_OUTPUT_YS[row], APC_DEVICE_LABELS[i]);
        }

        addSeparator(101.8f);

        for (int i = 0; i < NUM_EXTRA_CONTROLS; i++) {
            int output_idx = NUM_TRACK_CONTROLS + NUM_DEVICE_KNOBS + i;
            float x = EXTRA_XS[i];

            addCompactControl(output_idx, x, EXTRA_LABEL_Y, EXTRA_KNOB_Y,
                              EXTRA_OUTPUT_Y, APC_EXTRA_LABELS[i]);
        }
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        if (auto* m = dynamic_cast<Apc40Ctrl*>(module)) {
            menu->addChild(new MenuSeparator);
            app::appendMidiMenu(menu, &m->midiInput);
        }
        appendPanelThemeMenu(menu);
    }
};

Model* modelApc40Ctrl = createModel<Apc40Ctrl, Apc40CtrlWidget>("Apc40Ctrl");
