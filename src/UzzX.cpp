#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"

using AnimatekUI::TextLabel;

// ============================================================================
// UZZ-X — CV expander for UZZ. Attach to UZZ's left side.
// CV inputs are bipolar offsets around the corresponding UZZ knob; ROT +/-
// triggers rotate the whole sequence one step (wrapping) within the active
// window; RST clears accumulators; REV reverses direction while its gate is
// high. Events travel to UZZ as monotonic counters so no trigger is lost or
// double-fired regardless of engine ordering.
// ============================================================================

struct UzzX : Module {
    enum ParamIds { NUM_PARAMS };
    enum InputIds {
        ENUMS(CV_INPUTS, UZZX_NUM_CVS),
        REV_INPUT,
        ACCUM_RST_INPUT,
        ROT_FWD_INPUT,
        ROT_BACK_INPUT,
        NUM_INPUTS
    };
    enum OutputIds { NUM_OUTPUTS };
    enum LightIds { LINK_LIGHT, NUM_LIGHTS };

    UzzExpMsg msg;
    dsp::SchmittTrigger rstTrig;
    dsp::SchmittTrigger rotFwdTrig, rotBackTrig;
    dsp::ClockDivider lightDivider;

    UzzX() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        lightDivider.setDivision(256);

        configInput(CV_INPUTS + UZZX_CV_STEPS, "Steps offset (1V/step)");
        configInput(CV_INPUTS + UZZX_CV_START, "Start offset (1V/step)");
        configInput(CV_INPUTS + UZZX_CV_DIR, "Direction offset (1V/mode)");
        configInput(CV_INPUTS + UZZX_CV_ADDR,
                    "Step address (0-10V over active window)");
        configInput(CV_INPUTS + UZZX_CV_RATIO, "Clock ratio offset (1V/index)");
        configInput(CV_INPUTS + UZZX_CV_SWING, "Swing offset (±5V full range)");
        configInput(CV_INPUTS + UZZX_CV_PROB,
                    "Global probability offset (±10V = ±100%)");
        configInput(CV_INPUTS + UZZX_CV_ACCUM,
                    "Accumulator amount offset (1V/semitone)");
        configInput(REV_INPUT, "Reverse direction (gate)");
        configInput(ACCUM_RST_INPUT, "Accumulator reset (trig)");
        configInput(ROT_FWD_INPUT, "Rotate sequence forward (trig)");
        configInput(ROT_BACK_INPUT, "Rotate sequence backward (trig)");
    }

    void process(const ProcessArgs& args) override {
        for (int i = 0; i < UZZX_NUM_CVS; ++i) {
            bool con = inputs[CV_INPUTS + i].isConnected();
            float v = con ? inputs[CV_INPUTS + i].getVoltage() : 0.f;
            msg.connected[i] = con;
            msg.cv[i] = std::isfinite(v) ? v : 0.f;
        }
        msg.revGate = inputs[REV_INPUT].getVoltage() >= 1.f;
        if (rstTrig.process(inputs[ACCUM_RST_INPUT].getVoltage()))
            ++msg.accumRstCount;
        if (rotFwdTrig.process(inputs[ROT_FWD_INPUT].getVoltage()))
            ++msg.rotFwdCount;
        if (rotBackTrig.process(inputs[ROT_BACK_INPUT].getVoltage()))
            ++msg.rotBackCount;

        bool linked =
            rightExpander.module && rightExpander.module->model == modelUZZ;
        if (linked)
            rightExpander.module->leftExpander.consumerMessage = &msg;
        if (lightDivider.process())
            lights[LINK_LIGHT].setBrightness(linked ? 1.f : 0.f);
    }

};

struct UzzXWidget : ModuleWidget {
    UzzXWidget(UzzX* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/UzzX.svg")));

        constexpr float W = 30.48f;  // 6 HP
        constexpr float C = W * 0.5f;
        constexpr float X1 = 8.6f;
        constexpr float X2 = W - 8.6f;

        auto* title = new TextLabel("UZZ-X", mm2px(Vec(C - 10.f, 3.2f)),
                                    mm2px(Vec(20.f, 5.f)));
        title->fontSize = 11.f;
        title->color = nvgRGB(0x2C, 0x7F, 0xFF);
        addChild(title);
        addChild(createLightCentered<SmallLight<GreenLight>>(
            mm2px(Vec(W - 3.5f, 5.5f)), module, UzzX::LINK_LIGHT));

        auto addLabel = [&](const char* text, float cx, float y) {
            auto label = createWidget<TextLabel>(mm2px(Vec(cx - 6.f, y)));
            label->box.size = mm2px(Vec(12.f, 3.f));
            label->text = text;
            label->fontSize = 7.f;
            addChild(label);
        };
        auto addJack = [&](const char* text, float cx, int row, int inputId) {
            float labelY = 14.0f + 17.2f * (float)row;
            float portY = 21.0f + 17.2f * (float)row;
            addLabel(text, cx, labelY);
            addInput(createInputCentered<AnimatekUI::TekInputPort>(
                mm2px(Vec(cx, portY)), module, inputId));
        };

        addJack("STEPS", X1, 0, UzzX::CV_INPUTS + UZZX_CV_STEPS);
        addJack("START", X2, 0, UzzX::CV_INPUTS + UZZX_CV_START);
        addJack("DIR", X1, 1, UzzX::CV_INPUTS + UZZX_CV_DIR);
        addJack("ADDR", X2, 1, UzzX::CV_INPUTS + UZZX_CV_ADDR);
        addJack("RATIO", X1, 2, UzzX::CV_INPUTS + UZZX_CV_RATIO);
        addJack("SWING", X2, 2, UzzX::CV_INPUTS + UZZX_CV_SWING);
        addJack("PROB", X1, 3, UzzX::CV_INPUTS + UZZX_CV_PROB);
        addJack("ACCUM", X2, 3, UzzX::CV_INPUTS + UZZX_CV_ACCUM);
        addJack("ROT -", X1, 4, UzzX::ROT_BACK_INPUT);
        addJack("ROT +", X2, 4, UzzX::ROT_FWD_INPUT);
        addJack("RST", X1, 5, UzzX::ACCUM_RST_INPUT);
        addJack("REV", X2, 5, UzzX::REV_INPUT);
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
    }
};

Model* modelUzzX = createModel<UzzX, UzzXWidget>("UzzX");
