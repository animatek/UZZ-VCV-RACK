#pragma once

#include "../plugin.hpp"

#include <cctype>

namespace AnimatekUI {

static constexpr uint8_t LOGO_BLUE_R = 0x2C;
static constexpr uint8_t LOGO_BLUE_G = 0x7F;
static constexpr uint8_t LOGO_BLUE_B = 0xFF;

static constexpr uint8_t DISPLAY_BLUE_R = 0x5D;
static constexpr uint8_t DISPLAY_BLUE_G = 0xB7;
static constexpr uint8_t DISPLAY_BLUE_B = 0xFF;

inline NVGcolor logoBlue(uint8_t alpha = 255) {
  return nvgRGBA(LOGO_BLUE_R, LOGO_BLUE_G, LOGO_BLUE_B, alpha);
}

inline NVGcolor displayBlue(uint8_t alpha = 255) {
  return nvgRGBA(DISPLAY_BLUE_R, DISPLAY_BLUE_G, DISPLAY_BLUE_B, alpha);
}

// Los paneles Animatek son oscuros y punto: no hay variante clara. Estos colores no
// pueden depender de settings::preferDarkPanels, que es un ajuste de Rack entero — si
// alguien lo pone en claro por otros plugins, nuestros paneles seguirían siendo negros
// y el texto se volvería negro sobre negro.
inline NVGcolor panelTextColor() {
  return nvgRGB(0xC8, 0xD4, 0xE3);
}

inline NVGcolor panelSeparatorColor(uint8_t alpha = 180) {
  return nvgRGBA(0x9A, 0xA2, 0xB5, alpha);
}

inline std::shared_ptr<window::Svg> loadPluginSvg(const char *relPath) {
  std::string path = asset::plugin(pluginInstance, relPath);
  return system::exists(path) ? Svg::load(path) : nullptr;
}

inline std::shared_ptr<window::Svg> loadPluginSvgOr(const char *pluginRel,
                                                   const char *systemFallback) {
  if (auto svg = loadPluginSvg(pluginRel))
    return svg;
  return Svg::load(asset::system(systemFallback));
}

template <typename F>
inline void drawScaled(NVGcontext *vg, Vec boxSize, float scale, F fn) {
  nvgSave(vg);
  Vec c = boxSize.mult(0.5f);
  nvgTranslate(vg, c.x * (1.f - scale), c.y * (1.f - scale));
  nvgScale(vg, scale, scale);
  fn();
  nvgRestore(vg);
}

inline void uppercaseAscii(std::string &text) {
  for (char &c : text)
    c = (char)std::toupper((unsigned char)c);
}

struct TekInputPort : PJ301MPort {
  TekInputPort() {
    if (auto svg = loadPluginSvg("res/PJ310M_TEK_IN.svg"))
      setSvg(svg);
  }
};

struct TekOutputPort : PJ301MPort {
  TekOutputPort() {
    if (auto svg = loadPluginSvg("res/PJ310M_TEK.svg"))
      setSvg(svg);
  }
};

struct TextLabel : TransparentWidget {
  std::string text;
  float fontSize = 9.f;
  NVGcolor color = nvgRGBA(0, 0, 0, 0); // alpha==0 uses panelTextColor()
  bool uppercase = true;
  bool fakeBold = true;
  // Empty: Rack's UI font. Otherwise a font file, e.g. the Nunito Bold that
  // Rack's own panels are lettered in (asset::system("res/fonts/Nunito-Bold.ttf")).
  std::string fontPath;

  TextLabel() = default;

  TextLabel(const char *t, Vec pos, Vec size = Vec(40.f, 12.f)) : text(t) {
    box.pos = pos;
    box.size = size;
  }

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1)
      return;
    std::shared_ptr<Font> font =
        fontPath.empty() ? APP->window->uiFont : APP->window->loadFont(fontPath);
    if (!font)
      return;

    nvgFontSize(args.vg, fontSize);
    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, color.a > 0.f ? color : panelTextColor());
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
    float cx = box.size.x * .5f;
    float by = box.size.y;
    std::string renderText = text;
    if (uppercase)
      uppercaseAscii(renderText);
    nvgText(args.vg, cx, by, renderText.c_str(), nullptr);
    if (fakeBold)
      nvgText(args.vg, cx + 0.15f, by, renderText.c_str(), nullptr);
  }
};

struct HorizontalSeparator : TransparentWidget {
  float strokeWidth = 1.f;
  uint8_t alpha = 180;

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1)
      return;
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, 0.f, box.size.y / 2.f);
    nvgLineTo(args.vg, box.size.x, box.size.y / 2.f);
    nvgStrokeColor(args.vg, panelSeparatorColor(alpha));
    nvgStrokeWidth(args.vg, strokeWidth);
    nvgStroke(args.vg);
  }
};

struct ConnectorLine : TransparentWidget {
  float x1 = 0.f;
  float y1 = 0.f;
  float x2 = 0.f;
  float y2 = 0.f;
  int fixedAlpha = -1; // -1 = derive from theme each frame; >=0 = fixed value
  float strokeWidth = 0.4f;

  ConnectorLine(float ax1, float ay1, float ax2, float ay2, int alpha = -1)
      : x1(ax1), y1(ay1), x2(ax2), y2(ay2), fixedAlpha(alpha) {
    box.pos = Vec(std::min(x1, x2) - 2.f, std::min(y1, y2) - 2.f);
    box.size = Vec(std::abs(x2 - x1) + 4.f, std::abs(y2 - y1) + 4.f);
  }

  void draw(const DrawArgs &args) override {
    uint8_t alpha = (uint8_t)((fixedAlpha >= 0)
                                  ? fixedAlpha
                                  : 160);
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, x1 - box.pos.x, y1 - box.pos.y);
    nvgLineTo(args.vg, x2 - box.pos.x, y2 - box.pos.y);
    nvgStrokeColor(args.vg, panelSeparatorColor(alpha));
    nvgStrokeWidth(args.vg, strokeWidth);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgStroke(args.vg);
  }
};

struct DisplayBox : TransparentWidget {
  std::string text;
  float fontSize = 9.5f;
  float cornerRadius = 2.5f;
  uint8_t backgroundAlpha = 180;

  DisplayBox(Vec pos, Vec size) {
    box.pos = pos;
    box.size = size;
  }

  virtual std::string getText() const { return text; }

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1)
      return;
    std::shared_ptr<Font> font = APP->window->uiFont;
    if (!font)
      return;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, cornerRadius);
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, backgroundAlpha));
    nvgFill(args.vg);

    std::string displayText = getText();
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, fontSize);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, displayBlue());
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f,
            displayText.c_str(), nullptr);
  }
};

template <typename ModuleT, void (ModuleT::*ResetFunc)(), int SkipIdx,
          int ScalePct = 100>
struct RandomResetButton : TL1105 {
  static constexpr float scale() { return (float)ScalePct / 100.f; }

  void draw(const DrawArgs &args) override {
    drawScaled(args.vg, box.size, scale(), [&] { SvgSwitch::draw(args); });
  }

  void drawLayer(const DrawArgs &args, int layer) override {
    drawScaled(args.vg, box.size, scale(),
               [&] { SvgSwitch::drawLayer(args, layer); });
  }

  void onDoubleClick(const event::DoubleClick &e) override {
    if (auto q = getParamQuantity()) {
      if (auto m = dynamic_cast<ModuleT *>(q->module)) {
        (m->*ResetFunc)();
        m->skipNextRandom[SkipIdx] = true;
      }
    }
    e.consume(this);
  }
};

struct ScaledSvgSwitch : app::SvgSwitch {
  float scale = 1.f;

  void draw(const DrawArgs &args) override {
    drawScaled(args.vg, box.size, scale, [&] { SvgSwitch::draw(args); });
  }

  void drawLayer(const DrawArgs &args, int layer) override {
    drawScaled(args.vg, box.size, scale,
               [&] { SvgSwitch::drawLayer(args, layer); });
  }
};

// ---------------------------------------------------------------------------
// Flat knobs
// ---------------------------------------------------------------------------
//
// Drawn rather than loaded from SVG: a dark disc with a thin rim, a slightly
// lighter face and a white pointer, plus a value arc around it (blue on a
// faint track; bipolar ranges fill from the centre). Being drawn, they stay
// sharp at any zoom and one change here restyles every module. The sizes
// match Rack's knobs they replace, so nothing on a panel moves.
struct FlatKnob : app::Knob {
  bool arc = true;

  FlatKnob() {
    // 270 degrees, as Rack's own knobs, so the radial setting agrees.
    minAngle = -0.75f * (float)M_PI;
    maxAngle = 0.75f * (float)M_PI;
    setDiameter(9.6f);
  }

  void setDiameter(float mm) { box.size = mm2px(Vec(mm, mm)); }

  /** Where the pointer is, 0 at the left stop and 1 at the right. Without a
  module (the browser, the library site), the parameter's default. */
  virtual float position() {
    if (engine::ParamQuantity *pq = getParamQuantity())
      return pq->getScaledValue();
    return 0.5f;
  }

  /** Where the arc starts: the centre for a bipolar range, else the left stop. */
  virtual float arcOrigin() {
    if (engine::ParamQuantity *pq = getParamQuantity()) {
      float lo = pq->getMinValue(), hi = pq->getMaxValue();
      if (lo < 0.f && hi > 0.f)
        return -lo / (hi - lo);
    }
    return 0.f;
  }

  void draw(const DrawArgs &args) override {
    NVGcontext *vg = args.vg;
    const float d = box.size.x;
    const float r = d * 0.5f;
    const float cx = r, cy = box.size.y * 0.5f;
    // NanoVG angles, y down: the left stop is at 135 degrees, sweeping 270 clockwise.
    const float a0 = 0.75f * (float)M_PI;
    const float sweep = 1.5f * (float)M_PI;
    const float t = clamp(position(), 0.f, 1.f);

    if (arc) {
      const float ra = r + 2.f;
      nvgBeginPath(vg);
      nvgArc(vg, cx, cy, ra, a0, a0 + sweep, NVG_CW);
      nvgStrokeColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, 40));
      nvgStrokeWidth(vg, 1.4f);
      nvgLineCap(vg, NVG_ROUND);
      nvgStroke(vg);
      float o = clamp(arcOrigin(), 0.f, 1.f);
      float t0 = std::min(o, t), t1 = std::max(o, t);
      if (t1 > t0 + 1e-4f) {
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, ra, a0 + t0 * sweep, a0 + t1 * sweep, NVG_CW);
        nvgStrokeColor(vg, logoBlue(230));
        nvgStrokeWidth(vg, 1.8f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);
      }
    }

    // Body: rim, then face.
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r - 0.5f);
    nvgFillColor(vg, nvgRGB(0x1b, 0x1b, 0x1b));
    nvgFill(vg);
    nvgStrokeWidth(vg, std::max(0.8f, d * 0.025f));
    nvgStrokeColor(vg, nvgRGB(0x5a, 0x5a, 0x5a));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r * 0.8f);
    nvgFillColor(vg, nvgRGB(0x2c, 0x2c, 0x2c));
    nvgFill(vg);

    // Pointer.
    const float a = a0 + t * sweep;
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx + std::cos(a) * r * 0.12f, cy + std::sin(a) * r * 0.12f);
    nvgLineTo(vg, cx + std::cos(a) * r * 0.78f, cy + std::sin(a) * r * 0.78f);
    nvgStrokeColor(vg, nvgRGB(0xff, 0xff, 0xff));
    nvgStrokeWidth(vg, std::max(1.3f, d * 0.07f));
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);

    Knob::draw(args);
  }
};

// The sizes of the Rack knobs they stand in for.
struct FlatSmallKnob : FlatKnob { FlatSmallKnob() { setDiameter(7.68f); } };    // RoundSmallBlackKnob
struct FlatLargeKnob : FlatKnob { FlatLargeKnob() { setDiameter(12.19f); } };   // RoundLargeBlackKnob
struct FlatHugeKnob : FlatKnob { FlatHugeKnob() { setDiameter(18.24f); } };     // RoundHugeBlackKnob, Davies1900hLarge
struct FlatTrimpot : FlatKnob {                                                 // Trimpot
  FlatTrimpot() {
    setDiameter(6.05f);
    // Trimmers are amounts and fine adjustments: no arc, the pointer is enough.
    arc = false;
  }
};

} // namespace AnimatekUI
