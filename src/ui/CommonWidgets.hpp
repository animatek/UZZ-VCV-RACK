#pragma once

#include "../plugin.hpp"
#include "Palette.hpp"

#include <cctype>
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace AnimatekUI {

static constexpr uint8_t LOGO_BLUE_R = 0x2C;
static constexpr uint8_t LOGO_BLUE_G = 0x7F;
static constexpr uint8_t LOGO_BLUE_B = 0xFF;

static constexpr uint8_t DISPLAY_BLUE_R = 0x5D;
static constexpr uint8_t DISPLAY_BLUE_G = 0xB7;
static constexpr uint8_t DISPLAY_BLUE_B = 0xFF;

// ---------------------------------------------------------------------------
// Accent colour
// ---------------------------------------------------------------------------
//
// Every module can swap the logo blue for another colour of the palette (see
// AccentModuleWidget below). The widget sets the accent around its own draw,
// so everything drawn inside it that asks for logoBlue() or displayBlue() gets
// the module's colour instead, without each widget knowing about it. -1 is
// the plain blue. Drawing happens on the UI thread only, so one value is enough.
inline int &currentAccent() {
  static int accent = -1;
  return accent;
}

/** Sets the accent for the life of the object, and puts the old one back. */
struct AccentScope {
  int previous;
  explicit AccentScope(int accent) : previous(currentAccent()) { currentAccent() = accent; }
  ~AccentScope() { currentAccent() = previous; }
};

inline NVGcolor logoBlue(uint8_t alpha = 255) {
  const int accent = currentAccent();
  if (accent > 0 && accent < PALETTE_LEN)
    return paletteRGBA(accent, alpha);
  return nvgRGBA(LOGO_BLUE_R, LOGO_BLUE_G, LOGO_BLUE_B, alpha);
}

/** The lighter blue of the displays' text; with an accent, the accent a third
of the way to white, which is about how the two blues relate. */
inline NVGcolor displayBlue(uint8_t alpha = 255) {
  const int accent = currentAccent();
  if (accent > 0 && accent < PALETTE_LEN) {
    const PaletteColor &c = paletteAt(accent);
    auto lift = [](unsigned char v) { return (unsigned char)(v + (255 - v) / 3); };
    return nvgRGBA(lift(c.r), lift(c.g), lift(c.b), alpha);
  }
  return nvgRGBA(DISPLAY_BLUE_R, DISPLAY_BLUE_G, DISPLAY_BLUE_B, alpha);
}

/** A colour stored as the logo blue (a label set to it, say) follows the accent. */
inline NVGcolor followAccent(NVGcolor c) {
  auto byte = [](float v) { return (int)std::round(v * 255.f); };
  if (byte(c.r) == LOGO_BLUE_R && byte(c.g) == LOGO_BLUE_G && byte(c.b) == LOGO_BLUE_B)
    return logoBlue((uint8_t)byte(c.a));
  return c;
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

/** Where each SVG our widgets use was loaded from, so that a module's accent
colour can load a tinted copy of it (see AccentModuleWidget). */
inline std::map<const window::Svg *, std::string> &svgPaths() {
  static std::map<const window::Svg *, std::string> paths;
  return paths;
}

inline std::shared_ptr<window::Svg> loadPluginSvg(const char *relPath) {
  std::string path = asset::plugin(pluginInstance, relPath);
  if (!system::exists(path))
    return nullptr;
  std::shared_ptr<window::Svg> svg = Svg::load(path);
  if (svg)
    svgPaths()[svg.get()] = path;
  return svg;
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

// Panel lettering is drawn in the ordinary layer, not the light layer: in a
// dimmed room it fades with the panel, as printed text would, and only
// lights and displays stay bright.
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

  void draw(const DrawArgs &args) override {
    std::shared_ptr<Font> font =
        fontPath.empty() ? APP->window->uiFont : APP->window->loadFont(fontPath);
    if (!font)
      return;

    nvgFontSize(args.vg, fontSize);
    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, color.a > 0.f ? followAccent(color) : panelTextColor());
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

  void draw(const DrawArgs &args) override {
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

/** A plain momentary push button in the flat style: the knobs' grey rim and
face, darker while held. TL1105's size by default. */
struct FlatPushButton : app::Switch {
  FlatPushButton() {
    momentary = true;
    setDiameter(5.2f);   // TL1105
  }

  void setDiameter(float mm) { box.size = mm2px(Vec(mm, mm)); }

  void draw(const DrawArgs &args) override {
    NVGcontext *vg = args.vg;
    const float d = box.size.x, r = d * 0.5f;
    const float rimW = std::max(1.2f, d * 0.045f);
    bool pressed = false;
    if (engine::ParamQuantity *pq = getParamQuantity())
      pressed = pq->getValue() > 0.5f;
    nvgBeginPath(vg);
    nvgCircle(vg, r, r, r - rimW * 0.5f);
    nvgFillColor(vg, nvgRGB(0x1b, 0x1b, 0x1b));
    nvgFill(vg);
    nvgStrokeWidth(vg, rimW);
    nvgStrokeColor(vg, nvgRGB(0x5a, 0x5a, 0x5a));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, r, r, r * 0.8f);
    nvgFillColor(vg, pressed ? nvgRGB(0x22, 0x22, 0x22) : nvgRGB(0x2c, 0x2c, 0x2c));
    nvgFill(vg);
    app::Switch::draw(args);
  }
};

template <typename ModuleT, void (ModuleT::*ResetFunc)(), int SkipIdx,
          int ScalePct = 100>
struct RandomResetButton : FlatPushButton {
  RandomResetButton() { setDiameter(5.2f * (float)ScalePct / 100.f); }

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
// lighter face and a white pointer. The value is painted in blue on the rim
// itself (bipolar ranges fill from the centre). Being drawn, they stay
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

    // Body: rim, then face. The rim is also the arc's track: the value is
    // painted in blue right over it, a touch wider so it covers it, rather
    // than as a ring floating outside the knob.
    const float rimW = std::max(1.2f, d * 0.045f);
    const float rimR = r - rimW * 0.5f;
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, rimR);
    nvgFillColor(vg, nvgRGB(0x1b, 0x1b, 0x1b));
    nvgFill(vg);
    nvgStrokeWidth(vg, rimW);
    nvgStrokeColor(vg, nvgRGB(0x5a, 0x5a, 0x5a));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r * 0.8f);
    nvgFillColor(vg, nvgRGB(0x2c, 0x2c, 0x2c));
    nvgFill(vg);

    if (arc) {
      float o = clamp(arcOrigin(), 0.f, 1.f);
      float t0 = std::min(o, t), t1 = std::max(o, t);
      if (t1 > t0 + 1e-4f) {
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, rimR, a0 + t0 * sweep, a0 + t1 * sweep, NVG_CW);
        nvgStrokeColor(vg, logoBlue());
        nvgStrokeWidth(vg, rimW + 0.6f);
        nvgLineCap(vg, NVG_BUTT);
        nvgStroke(vg);
      }
    }

    // Pointer: wholly inside the face, round end included, with a little
    // air before its edge.
    const float a = a0 + t * sweep;
    const float pw = std::max(1.2f, d * 0.055f);
    const float pOut = r * 0.8f - pw * 0.5f - std::max(0.8f, d * 0.04f);
    const float pIn = r * 0.15f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx + std::cos(a) * pIn, cy + std::sin(a) * pIn);
    nvgLineTo(vg, cx + std::cos(a) * pOut, cy + std::sin(a) * pOut);
    nvgStrokeColor(vg, nvgRGB(0xff, 0xff, 0xff));
    nvgStrokeWidth(vg, pw);
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

// ---------------------------------------------------------------------------
// Flat lit buttons, to go with the flat knobs
// ---------------------------------------------------------------------------
//
// The same rim and face as a FlatKnob, without a pointer. Pressing it darkens
// the face; its light fills the face in the logo blue, with Rack's halo. A
// drop-in for VCVLightBezel: createLightParamCentered<FlatLightButton>(...).
struct FlatButtonLight : app::ModuleLightWidget {
  FlatButtonLight() {
    addBaseColor(logoBlue());
    bgColor = nvgRGBA(0, 0, 0, 0);
    borderColor = nvgRGBA(0, 0, 0, 0);
  }
};

struct FlatLightButton : app::Switch {
  app::ModuleLightWidget *light;

  FlatLightButton() {
    momentary = true;
    box.size = mm2px(Vec(7.2f, 7.2f));   // VCVLightBezel's size
    light = new FlatButtonLight;
    light->box.size = box.size.mult(0.8f);
    light->box.pos = box.size.minus(light->box.size).div(2.f);
    addChild(light);
  }

  // The light is a child widget, owned by the widget tree: never copied.
  FlatLightButton(const FlatLightButton &) = delete;
  FlatLightButton &operator=(const FlatLightButton &) = delete;

  app::ModuleLightWidget *getLight() { return light; }

  void draw(const DrawArgs &args) override {
    NVGcontext *vg = args.vg;
    const float d = box.size.x, r = d * 0.5f;
    const float rimW = std::max(1.2f, d * 0.045f);
    bool pressed = false;
    if (engine::ParamQuantity *pq = getParamQuantity())
      pressed = momentary && pq->getValue() > 0.5f;
    nvgBeginPath(vg);
    nvgCircle(vg, r, r, r - rimW * 0.5f);
    nvgFillColor(vg, nvgRGB(0x1b, 0x1b, 0x1b));
    nvgFill(vg);
    nvgStrokeWidth(vg, rimW);
    nvgStrokeColor(vg, nvgRGB(0x5a, 0x5a, 0x5a));
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, r, r, r * 0.8f);
    nvgFillColor(vg, pressed ? nvgRGB(0x22, 0x22, 0x22) : nvgRGB(0x2c, 0x2c, 0x2c));
    nvgFill(vg);
    app::Switch::draw(args);
  }
};

/** Stays down on one click and comes back up on the next, like VCVLightBezelLatch. */
struct FlatLightLatch : FlatLightButton {
  FlatLightLatch() { momentary = false; }
};

// ---------------------------------------------------------------------------
// Flat toggle switches, to go with the flat knobs
// ---------------------------------------------------------------------------
//
// A dark slot with the knobs' grey rim, and a lever with their face colour and
// a white bar across it, like the knobs' pointer. Same sizes and convention as
// Rack's CKSS family: position 0 at the bottom (or the left, horizontal), and a
// click steps to the next position.
struct FlatSwitch : app::Switch {
  int positions = 2;
  bool horizontal = false;

  FlatSwitch() { box.size = mm2px(Vec(4.74f, 6.99f)); }   // CKSS

  void draw(const DrawArgs &args) override {
    NVGcontext *vg = args.vg;
    const float w = box.size.x, h = box.size.y;
    const float rimW = 1.2f;
    int index = 0;
    if (engine::ParamQuantity *pq = getParamQuantity())
      index = clamp((int)std::round(pq->getValue() - pq->getMinValue()), 0, positions - 1);

    // Slot.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rimW * 0.5f, rimW * 0.5f, w - rimW, h - rimW, 2.f);
    nvgFillColor(vg, nvgRGB(0x1b, 0x1b, 0x1b));
    nvgFill(vg);
    nvgStrokeWidth(vg, rimW);
    nvgStrokeColor(vg, nvgRGB(0x5a, 0x5a, 0x5a));
    nvgStroke(vg);

    // Lever: one position's share of the slot, inset from the rim.
    const float inset = rimW + 1.f;
    const float len = (horizontal ? w : h) - 2.f * inset;
    const float cell = len / (float)positions;
    float lx, ly, lw, lh;
    if (horizontal) {
      lx = inset + cell * (float)index;
      ly = inset;
      lw = cell;
      lh = h - 2.f * inset;
    }
    else {
      lx = inset;
      ly = inset + cell * (float)(positions - 1 - index);   // 0 is the bottom
      lw = w - 2.f * inset;
      lh = cell;
    }
    nvgBeginPath(vg);
    nvgRoundedRect(vg, lx, ly, lw, lh, 1.2f);
    nvgFillColor(vg, nvgRGB(0x3a, 0x3a, 0x3a));
    nvgFill(vg);

    // The white bar across the lever.
    nvgBeginPath(vg);
    if (horizontal) {
      nvgMoveTo(vg, lx + lw * 0.5f, ly + lh * 0.22f);
      nvgLineTo(vg, lx + lw * 0.5f, ly + lh * 0.78f);
    }
    else {
      nvgMoveTo(vg, lx + lw * 0.22f, ly + lh * 0.5f);
      nvgLineTo(vg, lx + lw * 0.78f, ly + lh * 0.5f);
    }
    nvgStrokeColor(vg, nvgRGB(0xff, 0xff, 0xff));
    nvgStrokeWidth(vg, 1.3f);
    nvgLineCap(vg, NVG_ROUND);
    nvgStroke(vg);

    app::Switch::draw(args);
  }
};

struct FlatSwitch3 : FlatSwitch {                                   // CKSSThree
  FlatSwitch3() {
    positions = 3;
    box.size = mm2px(Vec(4.56f, 9.6f));
  }
};

struct FlatSwitchH : FlatSwitch {                                   // two-way, horizontal
  FlatSwitchH() {
    horizontal = true;
    box.size = mm2px(Vec(9.6f, 4.56f));
  }
};

// ---------------------------------------------------------------------------
// The module's name, bottom left
// ---------------------------------------------------------------------------
//
// One widget for every panel that carries its name beside the logo, so they
// all match: Nunito Bold, capitals, one size, starting 1.8 mm from the left
// edge and centred on the logo's axis at y = 123 mm. 14 px is the largest size
// at which the longest name, FILTERTEK, still clears the logo in 8 HP.
struct ModuleName : TransparentWidget {
  static constexpr float FONT_SIZE = 14.f;
  static constexpr float AXIS_MM = 123.f;   // the logo's centre on every panel
  std::string text;

  void draw(const DrawArgs &args) override {
    std::shared_ptr<window::Font> font =
        APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
    if (!font || font->handle < 0)
      return;
    nvgFontSize(args.vg, FONT_SIZE);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    nvgFillColor(args.vg, logoBlue());
    // Baseline half a cap height below the axis, so the capitals sit centred
    // on it (Nunito's capitals are 0.705 em tall).
    nvgText(args.vg, 0.f, box.size.y * 0.5f + FONT_SIZE * 0.705f * 0.5f, text.c_str(), nullptr);
  }
};

inline void addModuleName(widget::Widget *panel, const char *name) {
  auto *label = new ModuleName;
  label->text = name;
  uppercaseAscii(label->text);
  label->box.pos = mm2px(Vec(1.8f, ModuleName::AXIS_MM - 3.f));
  label->box.size = mm2px(Vec(36.f, 6.f));
  panel->addChild(label);
}

// ---------------------------------------------------------------------------
// Accent colour: the module side and the widget side
// ---------------------------------------------------------------------------

/** A module whose colour can be chosen. The choice is saved with the patch
(and so travels with presets and copies) at the top level of the module's
JSON, so modules with their own dataToJson need nothing more. */
struct AccentModule : engine::Module {
  int accentColor = -1;   // palette index; -1 the plain blue

  json_t *toJson() override {
    json_t *root = engine::Module::toJson();
    if (accentColor >= 0)
      json_object_set_new(root, "accentColor", json_integer(accentColor));
    return root;
  }

  void fromJson(json_t *root) override {
    engine::Module::fromJson(root);
    json_t *j = json_object_get(root, "accentColor");
    accentColor = j ? (int)json_integer_value(j) : -1;
  }
};

/** Recolours one nanosvg colour (0xAABBGGRR) if it is a blue: the accent at
the same depth, so a darker blue becomes a darker accent. Greys, whites and
any other hue are left alone. */
inline unsigned int tintBlue(unsigned int abgr, const PaletteColor &to) {
  const int r = abgr & 0xff, g = (abgr >> 8) & 0xff, b = (abgr >> 16) & 0xff;
  if (!(b > r + 30 && b > g + 25))
    return abgr;
  const float depth = (float)b / 255.f;
  auto ch = [&](unsigned char v) { return (unsigned int)std::min(255.f, std::round(v * depth)); };
  return (abgr & 0xff000000u) | (ch(to.b) << 16) | (ch(to.g) << 8) | ch(to.r);
}

inline void tintPaint(NSVGpaint &paint, const PaletteColor &to) {
  if (paint.type == NSVG_PAINT_COLOR)
    paint.color = tintBlue(paint.color, to);
  else if ((paint.type == NSVG_PAINT_LINEAR_GRADIENT || paint.type == NSVG_PAINT_RADIAL_GRADIENT) &&
           paint.gradient)
    for (int i = 0; i < paint.gradient->nstops; i++)
      paint.gradient->stops[i].color = tintBlue(paint.gradient->stops[i].color, to);
}

/** A private copy of an SVG file with its blues turned to a palette colour.
Svg::load() caches and shares, so tinting that one would tint every module
using the file: the copy is loaded apart and kept per file and colour. */
inline std::shared_ptr<window::Svg> accentPanelSvg(const std::string &path, int accent) {
  if (accent <= 0 || accent >= PALETTE_LEN)
    return window::Svg::load(path);
  static std::map<std::pair<std::string, int>, std::shared_ptr<window::Svg>> cache;
  std::shared_ptr<window::Svg> &slot = cache[std::make_pair(path, accent)];
  if (slot)
    return slot;
  slot = std::make_shared<window::Svg>();
  slot->loadFile(path);
  if (slot->handle) {
    const PaletteColor &c = paletteAt(accent);
    for (NSVGshape *shape = slot->handle->shapes; shape; shape = shape->next) {
      tintPaint(shape->fill, c);
      tintPaint(shape->stroke, c);
    }
  }
  return slot;
}

/** The tinted copy of an SVG our widgets loaded, or the SVG itself when its
file is not known or there is no accent. */
inline std::shared_ptr<window::Svg> accentSvg(const std::shared_ptr<window::Svg> &original, int accent) {
  if (!original || accent <= 0)
    return original;
  auto it = svgPaths().find(original.get());
  if (it == svgPaths().end())
    return original;
  return accentPanelSvg(it->second, accent);
}

/** The widget of a module with an accent colour: draws itself under that
accent, tints the blue of its panel and of its blue lights, and offers the
palette in its context menu. Subclasses use setAccentPanel() instead of
setPanel(), and call this class's step() and appendContextMenu(). */
struct AccentModuleWidget : app::ModuleWidget {
  std::string panelPath;
  int shownAccent = -2;
  bool lightsScanned = false;
  // Each blue light with the colour it was built with, to go back to.
  std::vector<std::pair<app::MultiLightWidget *, NVGcolor>> accentLights;
  // Each SVG drawn inside the module (jacks, buttons, icons) and each switch's
  // frames, as they were loaded, to tint copies of.
  std::vector<std::pair<widget::SvgWidget *, std::shared_ptr<window::Svg>>> accentSvgs;
  std::vector<std::pair<app::SvgSwitch *, std::vector<std::shared_ptr<window::Svg>>>> accentSwitches;

  /** The colour to draw with. By default the module's own choice. */
  virtual int accentIndex() {
    AccentModule *m = dynamic_cast<AccentModule *>(module);
    return m ? m->accentColor : -1;
  }
  /** False where the colour comes from elsewhere (a CAP's track). */
  virtual bool hasAccentMenu() { return true; }
  virtual std::string accentMenuName() { return "Colour"; }

  void setAccentPanel(const char *relPath) {
    panelPath = asset::plugin(pluginInstance, relPath);
    setPanel(createPanel(panelPath));
  }

  void draw(const DrawArgs &args) override {
    AccentScope scope(accentIndex());
    app::ModuleWidget::draw(args);
  }

  void drawLayer(const DrawArgs &args, int layer) override {
    AccentScope scope(accentIndex());
    app::ModuleWidget::drawLayer(args, layer);
  }

  /** The single-colour lights that are blue, Rack's or ours: those follow the
  accent. Several-colour lights (an RGB one) keep their meaning. */
  void scanLights(widget::Widget *w) {
    for (widget::Widget *child : w->children) {
      if (widget::SvgWidget *sw = dynamic_cast<widget::SvgWidget *>(child))
        if (sw->svg)
          accentSvgs.push_back(std::make_pair(sw, sw->svg));
      if (app::SvgSwitch *sws = dynamic_cast<app::SvgSwitch *>(child))
        accentSwitches.push_back(std::make_pair(sws, sws->frames));
      if (app::MultiLightWidget *light = dynamic_cast<app::MultiLightWidget *>(child)) {
        if (light->baseColors.size() == 1) {
          NVGcolor c = light->baseColors[0];
          auto byte = [](float v) { return (int)std::round(v * 255.f); };
          bool rackBlue = byte(c.r) == 0x29 && byte(c.g) == 0xb2 && byte(c.b) == 0xef;
          bool ourBlue = byte(c.r) == LOGO_BLUE_R && byte(c.g) == LOGO_BLUE_G && byte(c.b) == LOGO_BLUE_B;
          if (rackBlue || ourBlue)
            accentLights.push_back(std::make_pair(light, c));
        }
      }
      scanLights(child);
    }
  }

  static void dirtyFramebuffers(widget::Widget *w) {
    if (widget::FramebufferWidget *fb = dynamic_cast<widget::FramebufferWidget *>(w))
      fb->setDirty();
    for (widget::Widget *child : w->children)
      dirtyFramebuffers(child);
  }

  void step() override {
    if (!lightsScanned) {
      scanLights(this);
      // A switch's picture is chosen from its frames by its value, so it is
      // refreshed from the frames, not remembered as it was at the scan.
      for (auto &sws : accentSwitches)
        accentSvgs.erase(std::remove_if(accentSvgs.begin(), accentSvgs.end(),
                                        [&](const std::pair<widget::SvgWidget *, std::shared_ptr<window::Svg>> &e) {
                                          return e.first == sws.first->sw;
                                        }),
                         accentSvgs.end());
      lightsScanned = true;
    }
    const int accent = accentIndex();
    if (accent != shownAccent) {
      shownAccent = accent;
      const bool tinted = accent > 0 && accent < PALETTE_LEN;
      for (auto &entry : accentLights)
        entry.first->baseColors[0] = tinted ? paletteRGBA(accent) : entry.second;
      // A switch's frames first, so its next change already picks a tinted one;
      // then whatever each SVG widget shows now.
      for (auto &entry : accentSwitches) {
        app::SvgSwitch *sws = entry.first;
        for (size_t i = 0; i < entry.second.size() && i < sws->frames.size(); i++)
          sws->frames[i] = accentSvg(entry.second[i], accent);
        if (sws->frames.empty())
          continue;
        int frame = 0;
        if (engine::ParamQuantity *pq = sws->getParamQuantity())
          frame = (int)std::round(pq->getValue() - pq->getMinValue());
        if (frame < 0 || frame >= (int)sws->frames.size())
          frame = 0;
        sws->sw->setSvg(sws->frames[frame]);
      }
      for (auto &entry : accentSvgs)
        entry.first->setSvg(accentSvg(entry.second, accent));
      if (!panelPath.empty())
        if (app::SvgPanel *panel = dynamic_cast<app::SvgPanel *>(getPanel()))
          panel->setBackground(accentPanelSvg(panelPath, accent));
      dirtyFramebuffers(this);
    }
    app::ModuleWidget::step();
  }

  void appendContextMenu(ui::Menu *menu) override {
    app::ModuleWidget::appendContextMenu(menu);
    AccentModule *m = dynamic_cast<AccentModule *>(module);
    if (!m || !hasAccentMenu())
      return;
    menu->addChild(new ui::MenuSeparator);
    std::vector<std::string> names;
    for (int i = 0; i < PALETTE_LEN; i++)
      names.push_back(PALETTE[i].name);
    menu->addChild(createIndexSubmenuItem(
        accentMenuName(), names,
        [=]() { return (size_t)std::max(m->accentColor, 0); },
        [=](size_t i) { m->accentColor = (int)i; }));
  }
};

} // namespace AnimatekUI
