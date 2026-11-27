#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"
#include "EclipseShared.hpp"
#include "Scales.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using AnimatekUI::TextLabel;
using namespace EclipseShared;

// ECLIPSE PITCH: the note section of ECLIPSE, as an expander glued to its right.
// It holds the controls and the jacks; ECLIPSE does the work (see
// EclipseShared.hpp) and sends the note back. On its own it does nothing.

struct EclipsePitch : AnimatekUI::AccentModule {
    enum ParamId {
        MODE_PARAM,
        SOURCE_PARAM,
        RANGE_PARAM,
        SCALE_PARAM,
        KEY_PARAM,
        OCT_PARAM,
        STEP_A_PARAM,
        STEP_B_PARAM,
        LOOP_PARAM,
        LOCK_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        ROOT_INPUT,
        LOCK_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        VOCT_OUTPUT,
        ROOT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LOCK_LIGHT,
        LIGHTS_LEN
    };

    // ECLIPSE writes the note into these.
    PitchOutput leftMessages[2];
    // What ECLIPSE last sent, for the display; unlinked when it is not there.
    PitchOutput last;

    // The last GRAPH_LEN notes picked, oldest first from graphHead, for the
    // graph. Kept here and not in the widget: notes can come faster than frames.
    static constexpr int GRAPH_LEN = 32;
    int graphDegree[GRAPH_LEN] = {};
    int graphHead = 0;
    int graphCount = 0;
    uint32_t graphSeen = 0;

    EclipsePitch() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configSwitch(MODE_PARAM, 0.f, (float)(NUM_MODES - 1), (float)MODE_ACUM, "Note mode",
                     {"Random: a new note anywhere in the range",
                      "Accumulate: A's and B's hits move the note"});
        configSwitch(SOURCE_PARAM, 0.f, (float)(NUM_GATES - 1), 3.f, "Note follows",
                     {"A", "B", "AND", "OR", "XOR"});
        configParam(RANGE_PARAM, 1.f, 4.f, 2.f, "Range", " oct")->snapEnabled = true;
        std::vector<std::string> scaleNames;
        for (int i = 0; i < AnimatekScales::NUM_SCALES; ++i)
            scaleNames.push_back(AnimatekScales::SCALES[i].name);
        configSwitch(SCALE_PARAM, 0.f, (float)(AnimatekScales::NUM_SCALES - 1), 0.f, "Scale", scaleNames);
        std::vector<std::string> keyNames(AnimatekScales::ROOT_NAMES, AnimatekScales::ROOT_NAMES + 12);
        configSwitch(KEY_PARAM, 0.f, 11.f, 0.f, "Key", keyNames);
        configParam(OCT_PARAM, -2.f, 2.f, 0.f, "Octave")->snapEnabled = true;
        configParam(STEP_A_PARAM, -7.f, 7.f, 2.f, "Accumulate: each A hit moves", " degrees")->snapEnabled = true;
        configParam(STEP_B_PARAM, -7.f, 7.f, -1.f, "Accumulate: each B hit moves", " degrees")->snapEnabled = true;
        configParam(LOOP_PARAM, 1.f, 64.f, 16.f, "Lock length", " steps")->snapEnabled = true;
        configSwitch(LOCK_PARAM, 0.f, 1.f, 0.f, "Lock", {"Off", "On: loop the last steps"});

        configInput(ROOT_INPUT, "Root transpose (V/oct)");
        configInput(LOCK_INPUT, "Lock gate (high = locked)");
        configOutput(VOCT_OUTPUT, "Note (V/oct)");
        configOutput(ROOT_OUTPUT, "Root note (V/oct)");

        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
    }

    int paramInt(int id) {
        return (int)std::round(params[id].getValue());
    }

    bool lockOn() {
        return params[LOCK_PARAM].getValue() > 0.5f || inputs[LOCK_INPUT].getVoltage() >= 1.f;
    }

    Module* eclipse() {
        Module* left = leftExpander.module;
        return (left && left->model == modelEclipse) ? left : nullptr;
    }

    void process(const ProcessArgs& args) override {
        Module* host = eclipse();
        if (host) {
            PitchSettings* s = static_cast<PitchSettings*>(host->rightExpander.producerMessage);
            s->attached = true;
            s->mode = paramInt(MODE_PARAM);
            s->source = paramInt(SOURCE_PARAM);
            s->range = paramInt(RANGE_PARAM);
            s->octave = paramInt(OCT_PARAM);
            s->scale = paramInt(SCALE_PARAM);
            s->rootSemitones = paramInt(KEY_PARAM) + (int)std::round(inputs[ROOT_INPUT].getVoltage() * 12.f);
            s->stepA = paramInt(STEP_A_PARAM);
            s->stepB = paramInt(STEP_B_PARAM);
            s->lock = lockOn();
            s->loop = paramInt(LOOP_PARAM);
            host->rightExpander.requestMessageFlip();
            last = *static_cast<const PitchOutput*>(leftExpander.consumerMessage);
            if (last.linked && last.noteCount != graphSeen) {
                graphSeen = last.noteCount;
                graphDegree[graphHead] = last.degree;
                graphHead = (graphHead + 1) % GRAPH_LEN;
                graphCount = std::min(graphCount + 1, GRAPH_LEN);
            }
        } else {
            last = PitchOutput();
        }
        outputs[VOCT_OUTPUT].setVoltage(last.linked ? last.voct : 0.f);
        outputs[ROOT_OUTPUT].setVoltage(last.linked ? last.root : 0.f);
        lights[LOCK_LIGHT].setBrightness(lockOn() ? 1.f : 0.f);
    }
};

// ---------------------------------------------------------------------------
// The note wheel, drawn like ECLIPSE's rings: one ring of LEDs per octave of
// RANGE (the lowest inside), a LED per scale degree with the key at the top,
// and the last notes lit and joined in the order they played. The note
// playing now sits in the middle, and the corners say how it is made.
// ---------------------------------------------------------------------------

struct EclipsePitchWheel : TransparentWidget {
    EclipsePitch* module = nullptr;
    static constexpr int TRAIL = 12;   // notes joined by the melody line

    explicit EclipsePitchWheel(EclipsePitch* module) : module(module) {}

    static void dot(NVGcontext* vg, float x, float y, float r, NVGcolor c) {
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r);
        nvgFillColor(vg, c);
        nvgFill(vg);
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
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) {
            TransparentWidget::drawLayer(args, layer);
            return;
        }
        NVGcontext* vg = args.vg;
        const float w = box.size.x, h = box.size.y;
        const float size = std::min(w, h);
        const float cx = w * 0.5f, cy = h * 0.5f;
        const int N = EclipsePitch::GRAPH_LEN;

        // Without a module (browser, library site): the defaults and a fixed
        // phrase, as a preview.
        int scaleIdx = 0, key = 0, mode = MODE_ACUM, source = 3, stepA = 2, stepB = -1, loop = 16;
        bool linked = true, locked = false;
        int colorB = DEFAULT_B_COLOR;
        float voct = 0.f;
        int span = 14, scaleSize = 7;
        int notes[N];
        int count = 0;
        if (module) {
            scaleIdx = clamp(module->paramInt(EclipsePitch::SCALE_PARAM), 0, AnimatekScales::NUM_SCALES - 1);
            key = module->paramInt(EclipsePitch::KEY_PARAM) +
                  (int)std::round(module->inputs[EclipsePitch::ROOT_INPUT].getVoltage() * 12.f);
            mode = clamp(module->paramInt(EclipsePitch::MODE_PARAM), 0, NUM_MODES - 1);
            source = clamp(module->paramInt(EclipsePitch::SOURCE_PARAM), 0, NUM_GATES - 1);
            stepA = module->paramInt(EclipsePitch::STEP_A_PARAM);
            stepB = module->paramInt(EclipsePitch::STEP_B_PARAM);
            loop = module->paramInt(EclipsePitch::LOOP_PARAM);
            linked = module->last.linked;
            locked = module->lockOn();
            colorB = module->last.colorB;
            voct = module->last.voct;
            span = std::max(module->last.span, 1);
            scaleSize = std::max(module->last.scaleSize, 1);
            count = module->graphCount;
            int start = (module->graphHead - count + N) % N;
            for (int i = 0; i < count; ++i)
                notes[i] = module->graphDegree[(start + i) % N];
        } else {
            static const int PREVIEW[8] = {2, 4, 9, 7, 3, 5, 8, 0};   // ends on C4, as the centre says
            count = 8;
            for (int i = 0; i < count; ++i)
                notes[i] = PREVIEW[i];
        }

        std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
        const bool haveFont = font && font->handle >= 0;
        const NVGcolor dim = nvgRGBA(0xd0, 0xd6, 0xe0, 190);
        if (haveFont)
            nvgFontFaceId(vg, font->handle);

        if (!linked) {
            if (haveFont) {
                nvgFontSize(vg, size * 0.075f);
                nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, dim);
                nvgText(vg, cx, cy - size * 0.05f, "GLUE TO THE RIGHT", nullptr);
                nvgText(vg, cx, cy + size * 0.05f, "OF AN ECLIPSE", nullptr);
            }
            TransparentWidget::drawLayer(args, layer);
            return;
        }

        // One ring per octave: the lowest inside, the highest at ECLIPSE's
        // outer ring size, so both displays line up side by side.
        const int octaves = std::max(1, (span + scaleSize - 1) / scaleSize);
        const float rOuter = size * 0.43f, rInner = size * 0.25f;
        auto ringRadius = [&](int octave) {
            if (octaves == 1)
                return rOuter;
            return rInner + (rOuter - rInner) * (float)octave / (float)(octaves - 1);
        };
        auto position = [&](int degree, float& x, float& y) {
            int d = clamp(degree, 0, span - 1);
            float a = -0.5f * (float)M_PI + 2.f * (float)M_PI * (float)(d % scaleSize) / (float)scaleSize;
            float r = ringRadius(d / scaleSize);
            x = cx + r * std::cos(a);
            y = cy + r * std::sin(a);
        };

        for (int o = 0; o < octaves; ++o) {
            const float r = ringRadius(o);
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r);
            nvgStrokeColor(vg, nvgRGBA(0x33, 0x38, 0x4a, 120));
            nvgStrokeWidth(vg, 0.6f);
            nvgStroke(vg);
            for (int i = 0; i < scaleSize && o * scaleSize + i < span; ++i) {
                float x, y;
                position(o * scaleSize + i, x, y);
                // The key, like ECLIPSE's cycle start: larger and brighter.
                if (i == 0)
                    dot(vg, x, y, 1.9f, AnimatekUI::logoBlue(200));
                else
                    dot(vg, x, y, 0.9f, nvgRGBA(0x5a, 0x63, 0x75, 150));
            }
        }

        // The melody: the last TRAIL notes joined in order, fading with age.
        const int first = std::max(0, count - TRAIL);
        for (int i = first + 1; i < count; ++i) {
            float x0, y0, x1, y1;
            position(notes[i - 1], x0, y0);
            position(notes[i], x1, y1);
            float fresh = (float)(i - first) / (float)(count - first);
            nvgBeginPath(vg);
            nvgMoveTo(vg, x0, y0);
            nvgLineTo(vg, x1, y1);
            nvgStrokeColor(vg, AnimatekUI::logoBlue((uint8_t)(30.f + 150.f * fresh)));
            nvgStrokeWidth(vg, 0.6f + 0.8f * fresh);
            nvgStroke(vg);
        }
        for (int i = first; i < count; ++i) {
            float x, y;
            position(notes[i], x, y);
            bool newest = i == count - 1;
            if (newest) {
                dot(vg, x, y, 4.2f, AnimatekUI::logoBlue(110));
                dot(vg, x, y, 2.3f, nvgRGB(0xff, 0xff, 0xff));
            } else {
                float fresh = (float)(i - first + 1) / (float)(count - first);
                dot(vg, x, y, 1.6f, AnimatekUI::logoBlue((uint8_t)(60.f + 170.f * fresh)));
            }
        }

        if (haveFont) {
            // The note playing now, in the middle.
            int semis = (int)std::round(voct * 12.f);
            int pc = ((semis % 12) + 12) % 12;
            int octave = 4 + (int)std::floor((float)semis / 12.f);
            std::string note = string::f("%s%d", AnimatekScales::ROOT_NAMES[pc], octave);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFontSize(vg, size * 0.13f);
            nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
            nvgText(vg, cx, cy, note.c_str(), nullptr);

            // The corners: key and scale, the lock, the mode and its source,
            // and the accumulate steps in their rings' colours.
            const float pad = size * 0.04f;
            nvgFontSize(vg, size * 0.07f);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgFillColor(vg, dim);
            std::string keyText = string::f("%s %s", AnimatekScales::ROOT_NAMES[((key % 12) + 12) % 12],
                                            AnimatekScales::SCALES[scaleIdx].shortName);
            nvgText(vg, pad, pad, keyText.c_str(), nullptr);
            if (locked) {
                nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
                nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
                std::string lock = string::f("LOCK %d", loop);
                nvgText(vg, w - pad, pad, lock.c_str(), nullptr);
            }
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
            nvgFillColor(vg, dim);
            std::string how = std::string(MODE_TEXT[mode]) + " " + SOURCE_TEXT[source];
            nvgText(vg, pad, h - pad, how.c_str(), nullptr);
            if (mode == MODE_ACUM) {
                std::string a = string::f("%+d", stepA);
                std::string b = string::f(" %+d", stepB);
                float bw = nvgTextBounds(vg, 0.f, 0.f, b.c_str(), nullptr, nullptr);
                nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
                nvgFillColor(vg, paletteColor(colorB));
                nvgText(vg, w - pad, h - pad, b.c_str(), nullptr);
                nvgFillColor(vg, AnimatekUI::logoBlue());
                nvgText(vg, w - pad - bw, h - pad, a.c_str(), nullptr);
            }
        }
        TransparentWidget::drawLayer(args, layer);
    }
};

// ---------------------------------------------------------------------------
// Panel: 8 HP
// ---------------------------------------------------------------------------

namespace EclipsePitchLayout {
static constexpr float TOP_LABEL_Y = 2.2f;
static constexpr float TOP_Y = 10.5f;
static constexpr float DISPLAY_X = 1.32f;
static constexpr float DISPLAY_Y = 15.5f;
static constexpr float DISPLAY_SIZE = 38.f;            // as ECLIPSE's, at the same height
static constexpr float COL_X[3] = {8.f, 20.32f, 32.64f};
static constexpr float ROW_Y[3] = {63.f, 77.f, 91.f};   // the first two level with ECLIPSE's rings
static constexpr float OUT_Y = 106.5f;
} // namespace EclipsePitchLayout

struct EclipsePitchWidget : AnimatekUI::AccentModuleWidget {
    // Glued to an ECLIPSE, it takes that ECLIPSE's colour (ring A's), so the
    // pair reads as one instrument; there is no menu of its own.
    int accentIndex() override {
        EclipsePitch* m = dynamic_cast<EclipsePitch*>(module);
        return (m && m->last.linked) ? m->last.colorA : -1;
    }
    bool hasAccentMenu() override { return false; }

    TextLabel* stepBLabel = nullptr;

    explicit EclipsePitchWidget(EclipsePitch* module) {
        using namespace EclipsePitchLayout;
        setModule(module);
        setAccentPanel("res/EclipsePitch.svg");

        auto label = [&](const char* text, float x, float y, float w = 12.f, NVGcolor color = nvgRGBA(0, 0, 0, 0)) {
            auto* l = new TextLabel(text, mm2px(Vec(x - w * 0.5f, y)), mm2px(Vec(w, 4.f)));
            l->fontSize = 7.5f;
            l->color = color;
            addChild(l);
            return l;
        };

        AnimatekUI::addModuleName(this, "PITCH");

        // Top row: the two inputs and the lock button.
        label("ROOT", COL_X[0], TOP_LABEL_Y, 10.f);
        label("LOCK", COL_X[1], TOP_LABEL_Y, 10.f);
        label("LOCK", COL_X[2], TOP_LABEL_Y, 10.f);
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(COL_X[0], TOP_Y)), module, EclipsePitch::ROOT_INPUT));
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(COL_X[1], TOP_Y)), module, EclipsePitch::LOCK_INPUT));
        addParam(createLightParamCentered<AnimatekUI::FlatLightLatch>(mm2px(Vec(COL_X[2], TOP_Y)), module,
                                                                       EclipsePitch::LOCK_PARAM, EclipsePitch::LOCK_LIGHT));

        auto* wheel = new EclipsePitchWheel(module);
        wheel->box.pos = mm2px(Vec(DISPLAY_X, DISPLAY_Y));
        wheel->box.size = mm2px(Vec(DISPLAY_SIZE, DISPLAY_SIZE));
        addChild(wheel);

        // How the note is made; where it sits; how the rings move it and the lock's length.
        const char* names[3][3] = {{"MODE", "SRC", "RANGE"}, {"SCALE", "KEY", "OCT"}, {"+A", "+B", "LOOP"}};
        const int ids[3][3] = {
            {EclipsePitch::MODE_PARAM, EclipsePitch::SOURCE_PARAM, EclipsePitch::RANGE_PARAM},
            {EclipsePitch::SCALE_PARAM, EclipsePitch::KEY_PARAM, EclipsePitch::OCT_PARAM},
            {EclipsePitch::STEP_A_PARAM, EclipsePitch::STEP_B_PARAM, EclipsePitch::LOOP_PARAM},
        };
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c) {
                NVGcolor color = nvgRGBA(0, 0, 0, 0);
                if (r == 2 && c == 0)
                    color = AnimatekUI::logoBlue();
                if (r == 2 && c == 1)
                    color = paletteColor(DEFAULT_B_COLOR);
                TextLabel* l = label(names[r][c], COL_X[c], ROW_Y[r] - 8.4f, 11.f, color);
                if (r == 2 && c == 1)
                    stepBLabel = l;
                addParam(createParamCentered<AnimatekUI::FlatSmallKnob>(mm2px(Vec(COL_X[c], ROW_Y[r])), module, ids[r][c]));
            }

        label("V/OCT", 13.66f, OUT_Y - 8.3f, 10.f);
        label("ROOT", 26.98f, OUT_Y - 8.3f, 10.f);
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(13.66f, OUT_Y)), module, EclipsePitch::VOCT_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(26.98f, OUT_Y)), module, EclipsePitch::ROOT_OUTPUT));
    }

    void step() override {
        // +B in ring B's colour, as the ECLIPSE on the left has it.
        if (auto* m = dynamic_cast<EclipsePitch*>(module))
            if (stepBLabel && m->last.linked)
                stepBLabel->color = paletteColor(m->last.colorB);
        AnimatekUI::AccentModuleWidget::step();
    }
};

Model* modelEclipsePitch = createModel<EclipsePitch, EclipsePitchWidget>("EclipsePitch");
