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

inline NVGcolor panelTextColor() {
  return settings::preferDarkPanels ? nvgRGB(0xC8, 0xD4, 0xE3)
                                    : nvgRGB(0x14, 0x18, 0x22);
}

inline NVGcolor panelSeparatorColor(uint8_t alpha = 180) {
  return settings::preferDarkPanels ? nvgRGBA(0x9A, 0xA2, 0xB5, alpha)
                                    : nvgRGBA(0x55, 0x5A, 0x6A, alpha);
}

inline std::shared_ptr<window::Svg> loadPluginSvg(const char *relPath) {
  std::string path = asset::plugin(pluginInstance, relPath);
  return system::exists(path) ? Svg::load(path) : nullptr;
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

struct TextLabel : TransparentWidget {
  std::string text;
  float fontSize = 9.f;
  NVGcolor color = nvgRGBA(0, 0, 0, 0); // alpha==0 uses panelTextColor()
  bool uppercase = true;
  bool fakeBold = true;

  TextLabel() = default;

  TextLabel(const char *t, Vec pos, Vec size = Vec(40.f, 12.f)) : text(t) {
    box.pos = pos;
    box.size = size;
  }

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1)
      return;
    std::shared_ptr<Font> font = APP->window->uiFont;
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
                                  : (settings::preferDarkPanels ? 160 : 180));
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

} // namespace AnimatekUI
