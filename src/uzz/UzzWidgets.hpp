#pragma once

#include "../ui/CommonWidgets.hpp"
#include "UzzLayout.hpp"
#include "UzzTypes.hpp"

using AnimatekUI::displayBlue;
using AnimatekUI::drawScaled;
using AnimatekUI::loadPluginSvg;
using AnimatekUI::loadPluginSvgOr;
using AnimatekUI::panelTextColor;

// Generic Random Button Template
template <void (UZZ::*ResetFunc)(), int SkipIdx>
using RndButton =
    AnimatekUI::RandomResetButton<UZZ, ResetFunc, SkipIdx, 90>;

using RndPitchButton = RndButton<&UZZ::resetPitchRow, 0>;
using RndOctButton = RndButton<&UZZ::resetOctaveRow, 1>;
using RndStepButton = RndButton<&UZZ::resetStepModeRow, 2>;
using RndDurButton = RndButton<&UZZ::resetDurRow, 3>;
using RndM1Button = RndButton<&UZZ::resetM1Row, 4>;
using RndM2Button = RndButton<&UZZ::resetM2Row, 5>;
using RndProbButton = RndButton<&UZZ::resetProbRow, 6>;

// Custom ports with optional SVG and scaling
enum class PortType { INPUT, OUTPUT };

template <PortType Type> struct UzzPort : PJ301MPort {
  UzzPort() {
    const char *svgPath = (Type == PortType::INPUT) ? "res/port_input.svg"
                                                    : "res/port_output.svg";
    if (auto svg = loadPluginSvg(svgPath))
      setSvg(svg);
  }

  void draw(const DrawArgs &args) override {
    drawScaled(args.vg, box.size, UI::PORT_SCALE,
               [&] { PJ301MPort::draw(args); });
  }
  void drawLayer(const DrawArgs &args, int layer) override {
    drawScaled(args.vg, box.size, UI::PORT_SCALE,
               [&] { PJ301MPort::drawLayer(args, layer); });
  }
};

using UzzInputPort = UzzPort<PortType::INPUT>;
using UzzOutputPort = UzzPort<PortType::OUTPUT>;

// Row shift buttons (unified base class)
struct RowShiftButton : app::SvgSwitch {
  RowShiftButton() {
    momentary = true;
    shadow->visible = false;
  }

  void loadFrames(const char *pluginPath, const char *fallbackPath) {
    if (auto svg = loadPluginSvgOr(pluginPath, fallbackPath)) {
      addFrame(svg);
      addFrame(svg);
    }
  }

  void draw(const DrawArgs &args) override {
    drawScaled(args.vg, box.size, UI::ROW_SHIFT_SCALE,
               [&] { SvgSwitch::draw(args); });
  }
  void drawLayer(const DrawArgs &args, int layer) override {
    drawScaled(args.vg, box.size, UI::ROW_SHIFT_SCALE,
               [&] { SvgSwitch::drawLayer(args, layer); });
  }
};

struct RowShiftUpButton : RowShiftButton {
  RowShiftUpButton() {
    loadFrames("res/step_accum.svg", "res/ComponentLibrary/TL1105_0.svg");
  }
};

struct RowShiftDownButton : RowShiftButton {
  RowShiftDownButton() {
    loadFrames("res/step_accum_down.svg", "res/ComponentLibrary/TL1105_1.svg");
  }
};

// Arc knob — a RoundSmallBlackKnob with a value-indicator arc around it.
// Matches the "ring of progress" look from the original UZZ.
struct UzzArcKnob : RoundSmallBlackKnob {
  void draw(const DrawArgs &args) override {
    // 1) Draw the value arc BEHIND the knob body.
    if (auto q = getParamQuantity()) {
      float minV = q->getMinValue();
      float maxV = q->getMaxValue();
      if (maxV > minV) {
        float val = q->getValue();
        NVGcontext *vg = args.vg;
        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float r = box.size.x * 0.5f + 2.0f;

        // Knob sweep in NanoVG coords (Y-down): bottom-left → bottom-right CW,
        // 270°.
        const float a0 = 0.75f * (float)M_PI;
        const float sweep = 1.5f * (float)M_PI;

        // Track (faint full arc).
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, r, a0, a0 + sweep, NVG_CW);
        nvgStrokeColor(vg, settings::preferDarkPanels
                               ? nvgRGBA(0xFF, 0xFF, 0xFF, 40)
                               : nvgRGBA(0x00, 0x00, 0x00, 50));
        nvgStrokeWidth(vg, 1.4f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);

        // Value arc.
        bool bipolar = (minV < 0.f && maxV > 0.f);
        float t0, t1;
        if (bipolar) {
          float zeroT = (0.f - minV) / (maxV - minV);
          float curT = (val - minV) / (maxV - minV);
          t0 = std::min(zeroT, curT);
          t1 = std::max(zeroT, curT);
        } else {
          t0 = 0.f;
          t1 = (val - minV) / (maxV - minV);
        }
        if (t1 > t0 + 1e-4f) {
          nvgBeginPath(vg);
          nvgArc(vg, cx, cy, r, a0 + t0 * sweep, a0 + t1 * sweep, NVG_CW);
          nvgStrokeColor(vg, nvgRGBA(0x2C, 0x7F, 0xFF, 230));
          nvgStrokeWidth(vg, 1.8f);
          nvgLineCap(vg, NVG_ROUND);
          nvgStroke(vg);
        }
      }
    }
    // 2) Draw the knob on top.
    RoundSmallBlackKnob::draw(args);
  }
};

// Bipolar Trimpot: value 0 always at center regardless of range asymmetry.
// Left side maps to [minV, 0], right side maps to [0, maxV], each filling half the sweep.
struct ProbPulseKnob : Trimpot {
  void syncAngle() {
    auto* pq = getParamQuantity();
    if (fb && tw && pq) {
      float v    = pq->getValue();
      float vMin = pq->getMinValue();
      float vMax = pq->getMaxValue();
      float t = (v <= 0.f)
          ? math::rescale(v, vMin, 0.f, 0.f, 0.5f)
          : math::rescale(v, 0.f, vMax, 0.5f, 1.f);
      float angle = math::rescale(t, 0.f, 1.f, minAngle, maxAngle);
      tw->identity();
      tw->rotate(angle, sw->box.size.div(2));
      fb->dirty = true;
    }
  }

  void onChange(const ChangeEvent& e) override {
    syncAngle();
    Knob::onChange(e);
  }

  void step() override {
    syncAngle();
    Trimpot::step();
  }
};

// Step mode button (play/mute/skip)
struct StepModeButton : app::SvgSwitch {
  StepModeButton() {
    momentary = false;
    shadow->visible = false;

    auto loadFrame = [&](const char *pluginPath, const char *fallbackPath) {
      if (auto svg = loadPluginSvgOr(pluginPath, fallbackPath))
        addFrame(svg);
    };

    loadFrame("res/step_play.svg", "res/ComponentLibrary/TL1105_0.svg");
    loadFrame("res/step_mute.svg", "res/ComponentLibrary/TL1105_1.svg");
    loadFrame("res/step_skip.svg", "res/ComponentLibrary/TL1105_2.svg");
    loadFrame("res/step_accum.svg", "res/ComponentLibrary/TL1105_0.svg");
    loadFrame("res/step_accum_down.svg", "res/ComponentLibrary/TL1105_0.svg");
    loadFrame("res/step_pulse.svg", "res/ComponentLibrary/TL1105_0.svg");
    loadFrame("res/step_gated.svg", "res/ComponentLibrary/TL1105_0.svg");
    loadFrame("res/step_hold.svg", "res/ComponentLibrary/TL1105_0.svg");
  }
};

// Dark rounded-rect background + blue text. Subclasses override drawContent
// to render text on top with the font/color/alignment already set.
struct BasicDisplay : TransparentWidget {
  UZZ *module = nullptr;

  BasicDisplay(Vec pos, Vec size, UZZ *m) : module(m) {
    box.pos = pos;
    box.size = size;
  }

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1)
      return;
    std::shared_ptr<Font> font = APP->window->uiFont;
    if (!font)
      return;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.5f);
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 180));
    nvgFill(args.vg);

    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, displayBlue());
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    drawContent(args.vg);
  }

  virtual void drawContent(NVGcontext *vg) = 0;
};

struct ParamDisplay : BasicDisplay {
  int paramId = 0;

  ParamDisplay(Vec pos, Vec size, UZZ *m, int pid)
      : BasicDisplay(pos, size, m), paramId(pid) {}

  std::string formatValue() const {
    if (!module)
      return "--";
    float v = module->params[paramId].getValue();
    switch (paramId) {
    case UZZ::RATIO_IDX_PARAM: {
      int idx = clamp((int)std::round(v), 0, NUM_RATIOS - 1);
      return RATIO_LABELS[idx];
    }
    case UZZ::STEPS_PARAM:
      return std::to_string(clamp((int)std::round(v), 1, 16));
    case UZZ::START_PARAM:
      return std::to_string(clamp((int)std::round(v), 1, 16));
    case UZZ::DIR_MODE_PARAM: {
      int dv = (int)std::round(v);
      if (dv == DIR_JUMP)
        return string::f("J\xc3\xb7%d", module->jumpN);
      return dirShort(dv);
    }
    case UZZ::SWING_PARAM: {
      int pct = (int)std::round(v * 100.f);
      return std::to_string(pct) + "%";
    }
    case UZZ::ACCUM_AMT_PARAM: {
      int st = (int)std::round(v);
      return std::to_string(st) + "st";
    }
    case UZZ::ACCUM_CLIP_PARAM: {
      int n = (int)std::round(v);
      return (n > 0) ? std::to_string(n) + "st" : "OFF";
    }
    case UZZ::GATE_MODE_PARAM:
      return (v < 0.5f) ? "GATE" : "TRIG";
    default:
      return std::to_string((int)std::round(v));
    }
  }

  void drawContent(NVGcontext *vg) override {
    nvgFontSize(vg, 9.5f);
    std::string txt = formatValue();
    nvgText(vg, box.size.x * 0.5f, box.size.y * 0.5f, txt.c_str(), nullptr);
  }
};

// Muestra en dos líneas: semitones (arriba) y clip count (abajo).
struct AccumDisplay : BasicDisplay {
  AccumDisplay(Vec pos, Vec size, UZZ *m) : BasicDisplay(pos, size, m) {}

  void drawContent(NVGcontext *vg) override {
    std::string stTxt = "--";
    std::string clipTxt = "OFF";
    if (module) {
      int st = (int)std::round(module->params[UZZ::ACCUM_AMT_PARAM].getValue());
      int clip =
          (int)std::round(module->params[UZZ::ACCUM_CLIP_PARAM].getValue());
      stTxt = std::to_string(st) + "st";
      clipTxt = (clip > 0) ? std::to_string(clip) + "st" : "OFF";
    }
    nvgFontSize(vg, 8.5f);
    float cx = box.size.x * 0.5f;
    nvgText(vg, cx, box.size.y * 0.30f, stTxt.c_str(), nullptr);
    nvgText(vg, cx, box.size.y * 0.72f, clipTxt.c_str(), nullptr);
  }
};

// Note label
struct NoteLabel : TransparentWidget {
  UZZ *module = nullptr;
  int stepIndex = 0;
  std::shared_ptr<Font> font;

  NoteLabel(UZZ *m, int i) : module(m), stepIndex(i) {
    box.size = Vec(24.f, 12.f);
  }

  void draw(const DrawArgs &args) override {
    if (!module)
      return;
    if (!font)
      font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
    if (!font)
      return;

    int s = (int)std::round(
        module->params[UZZ::PITCH_PARAMS + stepIndex].getValue());
    int oct = (int)std::round(
                  module->params[UZZ::OCT_PARAMS + stepIndex].getValue()) +
              4;
    static const char *N[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
    std::string txt = string::f("%s%d", N[(s % 12 + 12) % 12], oct);

    nvgFontSize(args.vg, 10.f);
    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, panelTextColor());
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, box.size.x * .5f, box.size.y * .5f, txt.c_str(), nullptr);
  }
};

struct CapybaraWidget : Widget {
  UZZ *module = nullptr;
  std::shared_ptr<window::Svg> svg;

  CapybaraWidget(UZZ *module) : module(module) {
    svg = loadPluginSvg("res/capybara.svg");
    box.size = Vec(48.4f, 53.2f);
  }

  void draw(const DrawArgs &args) override {
    if (!module || !svg)
      return;

    float flash = clamp(module->capiFlash, 0.f, 1.f);
    nvgSave(args.vg);
    nvgTranslate(args.vg, box.size.x, 0.f);
    nvgScale(args.vg, -box.size.x / 200.f, box.size.y / 200.f);
    svg->draw(args.vg);

    if (flash > 0.f) {
      nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
      nvgGlobalAlpha(args.vg, flash * 0.65f);
      svg->draw(args.vg);
    }

    nvgRestore(args.vg);
  }
};
