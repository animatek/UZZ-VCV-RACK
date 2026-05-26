#include "plugin.hpp"

#include <array>

static bool blank3AnimationPaused = false;

struct Blank3 : Module {
    struct LogoMark {
        float x = 0.f;
        float y = 0.f;
        float size = 0.f;
        float rotation = 0.f;
        float opacity = 0.f;
        float vx = 0.f;
        float vy = 0.f;
        float vr = 0.f;
    };

    std::array<LogoMark, 7> logos;
    float walkTimer = 0.f;

    Blank3() {
        config(0, 0, 0, 0);

        for (LogoMark& logo : logos) {
            logo.x = 2.0f + random::uniform() * 11.24f;
            logo.y = 8.0f + random::uniform() * 104.0f;
            logo.size = 14.0f + random::uniform() * 42.0f;
            logo.rotation = (-45.0f + random::uniform() * 90.0f) * M_PI / 180.0f;
            logo.opacity = 0.025f + random::uniform() * 0.075f;
            logo.vx = -0.22f + random::uniform() * 0.44f;
            logo.vy = -0.22f + random::uniform() * 0.44f;
            logo.vr = (-1.0f + random::uniform() * 2.0f) * M_PI / 180.0f;
        }
    }

    void process(const ProcessArgs& args) override {
        if (blank3AnimationPaused)
            return;

        walkTimer += args.sampleTime;

        if (walkTimer >= 0.25f) {
            walkTimer -= 0.25f;
            for (LogoMark& logo : logos) {
                logo.vx = clamp(logo.vx + (-0.08f + random::uniform() * 0.16f), -0.45f, 0.45f);
                logo.vy = clamp(logo.vy + (-0.08f + random::uniform() * 0.16f), -0.45f, 0.45f);
                constexpr float DEG_TO_RAD = (float)M_PI / 180.0f;
                logo.vr = clamp(logo.vr + (-0.25f + random::uniform() * 0.5f) * DEG_TO_RAD,
                                -2.0f * DEG_TO_RAD,
                                2.0f * DEG_TO_RAD);
            }
        }

        for (LogoMark& logo : logos) {
            logo.x += logo.vx * args.sampleTime;
            logo.y += logo.vy * args.sampleTime;
            logo.rotation += logo.vr * args.sampleTime;

            if (logo.x < 1.0f || logo.x > 14.24f) {
                logo.x = clamp(logo.x, 1.0f, 14.24f);
                logo.vx *= -0.8f;
            }
            if (logo.y < 5.0f || logo.y > 116.0f) {
                logo.y = clamp(logo.y, 5.0f, 116.0f);
                logo.vy *= -0.8f;
            }
        }
    }
};

struct BlankLogoPattern : TransparentWidget {
    Blank3* module = nullptr;
    std::shared_ptr<window::Svg> logoSvg;
    std::array<Blank3::LogoMark, 7> previewLogos;

    BlankLogoPattern() {
        box.size = mm2px(Vec(15.24f, 128.5f));
        logoSvg = Svg::load(asset::plugin(pluginInstance, "res/AnimatekLogo.svg"));

        for (Blank3::LogoMark& logo : previewLogos) {
            logo.x = 2.0f + random::uniform() * 11.24f;
            logo.y = 8.0f + random::uniform() * 104.0f;
            logo.size = 14.0f + random::uniform() * 42.0f;
            logo.rotation = (-45.0f + random::uniform() * 90.0f) * M_PI / 180.0f;
            logo.opacity = 0.025f + random::uniform() * 0.075f;
        }
    }

    void draw(const DrawArgs& args) override {
        if (!logoSvg)
            return;

        Vec svgSize = logoSvg->getSize();
        if (svgSize.x <= 0.f || svgSize.y <= 0.f)
            return;

        nvgSave(args.vg);
        nvgScissor(args.vg, 0.f, 0.f, box.size.x, box.size.y);

        const auto& logos = module ? module->logos : previewLogos;
        for (const Blank3::LogoMark& logo : logos) {
            float targetSize = mm2px(logo.size);
            float scale = targetSize / std::max(svgSize.x, svgSize.y);

            nvgSave(args.vg);
            nvgTranslate(args.vg, mm2px(logo.x), mm2px(logo.y));
            nvgRotate(args.vg, logo.rotation);
            nvgScale(args.vg, scale, scale);
            nvgTranslate(args.vg, -svgSize.x * 0.5f, -svgSize.y * 0.5f);
            nvgGlobalAlpha(args.vg, logo.opacity);
            logoSvg->draw(args.vg);
            nvgRestore(args.vg);
        }

        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }
};

struct Blank3Widget : ModuleWidget {
    Blank3Widget(Blank3* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/Blank3-light.svg"),
            asset::plugin(pluginInstance, "res/Blank3.svg")));

        auto* pattern = new BlankLogoPattern();
        pattern->module = module;
        addChild(pattern);
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        appendPanelThemeMenu(menu);

        menu->addChild(new ui::MenuSeparator());
        menu->addChild(createCheckMenuItem(
            "Pause animation", "",
            []() { return blank3AnimationPaused; },
            []() { blank3AnimationPaused = !blank3AnimationPaused; }));
    }
};

Model* modelBlank3 = createModel<Blank3, Blank3Widget>("Blank3");
