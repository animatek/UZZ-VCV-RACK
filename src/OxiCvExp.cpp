#include "plugin.hpp"

// Layout constants (must match res/OxiCvExp.svg)
static constexpr int   NUM_TRACKS    = 8;
static constexpr int   MIDI_CHANNELS = 16;
static constexpr float X_CH          = 6.0f;   // trimpot column
static constexpr float X_VOCT        = 20.0f;
static constexpr float X_LABEL       = (X_CH + X_VOCT) / 2.f;  // centered between knob and V/OCT
static constexpr float X_GATE        = 31.0f;
static constexpr float X_VEL         = 42.0f;
static constexpr float Y_FIRST_ROW   = 19.0f;
static constexpr float Y_ROW_STEP    = 13.0f;
static constexpr float Y_COL_HEADER  = 14.5f;  // column labels above first row
static constexpr float PANEL_W       = 50.8f;
static constexpr float Y_TITLE       = 7.5f;   // above divider at y=10.0

struct OxiCvExp : Module {
    enum ParamIds  { ENUMS(CH_PARAM, NUM_TRACKS), NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds {
        ENUMS(VOCT_OUTPUT, NUM_TRACKS),
        ENUMS(GATE_OUTPUT, NUM_TRACKS),
        ENUMS(VEL_OUTPUT,  NUM_TRACKS),
        NUM_OUTPUTS
    };
    enum LightIds  { NUM_LIGHTS };

    OxiCvExp() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int i = 0; i < NUM_TRACKS; i++) {
            configParam(CH_PARAM + i, 1.f, (float)MIDI_CHANNELS, (float)(i + 1),
                        string::f("Track %d Channel", i + 1));
            paramQuantities[CH_PARAM + i]->snapEnabled = true;
            configOutput(VOCT_OUTPUT + i, string::f("Track %d V/Oct", i + 1));
            configOutput(GATE_OUTPUT + i, string::f("Track %d Gate", i + 1));
            configOutput(VEL_OUTPUT  + i, string::f("Track %d Velocity", i + 1));
        }
    }

    void process(const ProcessArgs& args) override {
        OxiCvExpMsg* msg = (OxiCvExpMsg*)leftExpander.consumerMessage;
        if (leftExpander.module && leftExpander.module->model->slug != "OxiCv")
            msg = nullptr;

        for (int i = 0; i < NUM_TRACKS; i++) {
            int chIdx = clamp((int)params[CH_PARAM + i].getValue() - 1, 0, MIDI_CHANNELS - 1);
            if (msg) {
                const VoiceState& v = msg->channels[chIdx];
                float pitch = (v.note >= 0) ? (v.note - 60) / 12.f : 0.f;
                outputs[VOCT_OUTPUT + i].setVoltage(pitch + msg->pitchBend);
                outputs[GATE_OUTPUT + i].setVoltage(v.gate ? 10.f : 0.f);
                outputs[VEL_OUTPUT  + i].setVoltage((v.vel / 127.f) * 10.f);
            } else {
                outputs[VOCT_OUTPUT + i].setVoltage(0.f);
                outputs[GATE_OUTPUT + i].setVoltage(0.f);
                outputs[VEL_OUTPUT  + i].setVoltage(0.f);
            }
        }
    }
};

struct ExpLabel : widget::TransparentWidget {
    std::string text;
    float fontSize = 7.5f;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1 || !APP->window->uiFont) return;
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgFontSize(args.vg, fontSize);
        nvgFillColor(args.vg, settings::preferDarkPanels ? nvgRGB(220, 220, 230) : nvgRGB(35, 35, 45));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        std::string upper = text;
        for (auto& c : upper) c = toupper((unsigned char)c);
        nvgText(args.vg, box.size.x / 2.f, box.size.y, upper.c_str(), NULL);
    }
};

struct ExpChannelLabel : widget::TransparentWidget {
    OxiCvExp* module = nullptr;
    int trackIdx = 0;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        std::shared_ptr<Font> font = APP->window->uiFont;
        if (!font) return;

        int ch = module ? (int)module->params[OxiCvExp::CH_PARAM + trackIdx].getValue() : (trackIdx + 1);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.0f);
        nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 160));
        nvgFill(args.vg);

        nvgFontFaceId(args.vg, font->handle);
        nvgFontSize(args.vg, 9.0f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGB(0x5D, 0xB7, 0xFF));
        nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, std::to_string(ch).c_str(), nullptr);
    }
};

struct OxiCvExpWidget : ModuleWidget {
    OxiCvExpWidget(OxiCvExp* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/OxiCvExp-light.svg"),
            asset::plugin(pluginInstance, "res/OxiCvExp.svg")));

        auto title = createWidget<ExpLabel>(mm2px(Vec(0.f, Y_TITLE - 3.5f)));
        title->box.size = mm2px(Vec(PANEL_W, 3.5f));
        title->text = "MULTI - EXPANDER";
        title->fontSize = 10.0f;
        addChild(title);

        auto addHeader = [&](const std::string& txt, float x) {
            auto lbl = createWidget<ExpLabel>(mm2px(Vec(x - 6.0f, Y_COL_HEADER - 3.0f)));
            lbl->box.size = mm2px(Vec(12.0f, 3.0f));
            lbl->text = txt;
            addChild(lbl);
        };
        addHeader("CH",    X_CH);
        addHeader("V/OCT", X_VOCT);
        addHeader("GATE",  X_GATE);
        addHeader("VEL",   X_VEL);

        for (int i = 0; i < NUM_TRACKS; i++) {
            float y = Y_FIRST_ROW + i * Y_ROW_STEP;

            addParam(createParamCentered<Trimpot>(mm2px(Vec(X_CH, y)), module, OxiCvExp::CH_PARAM + i));

            static constexpr float LABEL_W = 5.0f, LABEL_H = 6.0f;
            auto num = createWidget<ExpChannelLabel>(mm2px(Vec(X_LABEL - LABEL_W / 2.f, y - LABEL_H / 2.f)));
            num->box.size = mm2px(Vec(LABEL_W, LABEL_H));
            num->module = module;
            num->trackIdx = i;
            addChild(num);

            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_VOCT, y)), module, OxiCvExp::VOCT_OUTPUT + i));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_GATE, y)), module, OxiCvExp::GATE_OUTPUT + i));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(X_VEL,  y)), module, OxiCvExp::VEL_OUTPUT  + i));
        }
    }
};

Model* modelOxiCvExp = createModel<OxiCvExp, OxiCvExpWidget>("OxiCvExp");
