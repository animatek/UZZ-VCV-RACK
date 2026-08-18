#include "plugin.hpp"

#include <array>
#include <map>

static bool blank3AnimationPaused = false;

// Paleta de las marcas, sacada de la guía de estilo de animatek.net (el artifact
// "Animatek — Paleta de colores"). Empieza por los tres colores de marca —el azul
// primario, el naranja secundario que es justo su complementario, y el verde de
// acento— y sigue con acentos de chip. Cada blank de un grupo coge el siguiente,
// así que pegar paneles va sumando colores en vez de repetir el azul.
struct BlankMarkColor {
    const char* name;
    unsigned char r, g, b;
};
static const BlankMarkColor BLANK_PALETTE[] = {
    {"Blue",   0x2C, 0x7F, 0xFF},   // Primary
    {"Orange", 0xFD, 0x9A, 0x00},   // Secondary — complementario del azul
    {"Green",  0x24, 0xB9, 0x79},   // Accent
    {"Purple", 0xC0, 0x84, 0xFC},
    {"Cyan",   0x22, 0xD3, 0xEE},
    {"Rose",   0xFB, 0x71, 0x85},
};
static const int BLANK_PALETTE_LEN = (int)(sizeof(BLANK_PALETTE) / sizeof(BLANK_PALETTE[0]));

// Una copia privada del SVG con el color cambiado. Svg::load() cachea y comparte la
// instancia, así que teñir esa rompería el resto del plugin: aquí se carga aparte y se
// reescribe el fill de cada trazo. Se guarda en una caché propia por (fichero, color)
// para no reparsear en cada blank.
static std::shared_ptr<window::Svg> blankTintedSvg(const char* path, int colorIdx) {
    static std::map<std::pair<std::string, int>, std::shared_ptr<window::Svg>> cache;
    auto key = std::make_pair(std::string(path), colorIdx);
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    auto svg = std::make_shared<window::Svg>();
    svg->loadFile(asset::plugin(pluginInstance, path));
    if (svg->handle) {
        const BlankMarkColor& c = BLANK_PALETTE[clamp(colorIdx, 0, BLANK_PALETTE_LEN - 1)];
        // nanosvg guarda el color como 0xAABBGGRR, no como 0xAARRGGBB.
        const unsigned int abgr = 0xff000000u
                                | ((unsigned int) c.b << 16)
                                | ((unsigned int) c.g << 8)
                                | (unsigned int) c.r;
        for (NSVGshape* shape = svg->handle->shapes; shape; shape = shape->next) {
            if (shape->fill.type == NSVG_PAINT_COLOR)
                shape->fill.color = abgr;
            if (shape->stroke.type == NSVG_PAINT_COLOR)
                shape->stroke.color = abgr;
        }
    }
    cache[key] = svg;
    return svg;
}

// Ancho de un blank, en mm. Todo el lienzo compartido se mide en múltiplos de esto.
static constexpr float BLANK_PANEL_W = 15.24f;
// Evita bloquear Rack si la UI observa punteros de expander transitorios durante un
// movimiento. Es muy superior a cualquier grupo práctico, pero todos los recorridos
// comparten el mismo límite para no producir geometrías distintas.
static constexpr int BLANK_GROUP_LIMIT = 1024;

// ¿Es este módulo uno de los blanks? Las dos variantes comparten clase de módulo y
// solo se distinguen por el Model, así que la cadena admite mezclar logos y caritas.
static bool isBlankModule(const Module* m) {
    return m && (m->model == modelBlank3 || m->model == modelBlankAcid);
}

struct Blank3;

// Aplica algo a todos los blanks del grupo contiguo. Velocidad y color son ajustes del
// lienzo, no del panel: si el lienzo es común, sus mandos también lo son.
template <typename F>
static void blankForEachInGroup(Module* start, F fn);

struct Blank3 : Module {
    struct LogoMark {
        float x = 0.f;          // en mm desde el borde izquierdo del GRUPO, no del panel
        float y = 0.f;
        float size = 0.f;
        float rotation = 0.f;
        float opacity = 0.f;
        float vx = 0.f;
        float vy = 0.f;
        float vr = 0.f;
    };

    // Dos marcas por panel: con siete se solapaban tanto que el conjunto se leía como
    // una mancha y no como caras. Con dos se distinguen, y por eso pueden ir a más
    // opacidad que antes.
    std::array<LogoMark, 2> logos;
    float walkTimer = 0.f;

    float speed = 1.f;      // multiplicador de la deriva, desde el menú
    // Color de las marcas. -1 = sin asignar: al entrar en un grupo se coge el primer
    // color libre y se fija ahí. Al fijarse, viaja en el JSON, así que duplicar o
    // copiar el módulo conserva su color, y moverlo de sitio tampoco se lo cambia.
    int colorMode = -1;

    // Geometría del grupo de blanks contiguos: cuántos somos y cuál soy yo. Las marcas
    // recorren el grupo entero, así que al pegar otro blank el lienzo se ensancha y las
    // caritas cruzan de un panel al de al lado.
    int groupIndex = 0;
    int groupCount = 1;
    bool chainReady = false;
    int chainStable = 0;        // cuántas comprobaciones lleva la cadena sin cambiar
    dsp::ClockDivider chainDivider;

    Blank3() {
        config(0, 0, 0, 0);
        chainDivider.setDivision(2048);

        for (LogoMark& logo : logos) {
            logo.x = 2.0f + random::uniform() * 11.24f;
            logo.y = 8.0f + random::uniform() * 104.0f;
            logo.size = 14.0f + random::uniform() * 42.0f;
            logo.rotation = (-45.0f + random::uniform() * 90.0f) * M_PI / 180.0f;
            logo.opacity = 0.10f + random::uniform() * 0.12f;
            logo.vx = -0.22f + random::uniform() * 0.44f;
            logo.vy = -0.22f + random::uniform() * 0.44f;
            logo.vr = (-1.0f + random::uniform() * 2.0f) * M_PI / 180.0f;
        }
    }

    float groupWidth() const { return groupCount * BLANK_PANEL_W; }

    /** Color de las marcas. Mientras no se haya asignado, uno provisional por posición. */
    int markColor() const {
        if (colorMode >= 0)
            return colorMode % BLANK_PALETTE_LEN;
        return groupIndex % BLANK_PALETTE_LEN;
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "speed", json_real(speed));
        json_object_set_new(rootJ, "colorMode", json_integer(colorMode));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        if (json_t* j = json_object_get(rootJ, "speed"))
            speed = (float) json_number_value(j);
        if (json_t* j = json_object_get(rootJ, "colorMode"))
            colorMode = (int) json_integer_value(j);
    }

    // Recorre los vecinos a los dos lados. Los punteros de expander son válidos dentro
    // de process(), que es donde se llama a esto.
    void updateChain() {
        int left = 0, right = 0;
        const Module* m = leftExpander.module;
        for (; left < BLANK_GROUP_LIMIT && isBlankModule(m); m = m->leftExpander.module)
            left++;
        m = rightExpander.module;
        for (; right < BLANK_GROUP_LIMIT && isBlankModule(m); m = m->rightExpander.module)
            right++;

        const int newIndex = left;
        const int newCount = left + right + 1;

        if (newIndex != groupIndex || newCount != groupCount || !chainReady) {
            // Al entrar otro blank por la izquierda mi panel se corre a la derecha. Las
            // marcas se desplazan con él para que no den un salto en pantalla: siguen
            // donde estaban, es el lienzo el que ha crecido por detrás.
            const float shift = (newIndex - (chainReady ? groupIndex : 0)) * BLANK_PANEL_W;
            groupIndex = newIndex;
            groupCount = newCount;
            chainReady = true;
            chainStable = 0;

            const float w = groupWidth();
            for (LogoMark& logo : logos)
                logo.x = clamp(logo.x + shift, 1.0f, w - 1.0f);
        }
        else if (chainStable < 4) {
            chainStable++;
        }

        // El reparto de colores lo hace SOLO el primero del grupo, y solo cuando la
        // cadena lleva un rato quieta. Si cada módulo se asignase el suyo, varios se
        // repartirían a la vez leyendo un estado a medias —y acababan todos del mismo
        // color—; y al soltar un blank en el rack hay unos milisegundos en los que
        // todavía está solo, antes de que Rack le enganche los vecinos.
        if (chainStable >= 3 && groupIndex == 0) {
            // Al unir grupos puede haber velocidades distintas. El primer blank manda,
            // igual que cuando el menú aplica un ajuste a todo el lienzo compartido.
            Module* m = rightExpander.module;
            for (int guard = 0; guard < BLANK_GROUP_LIMIT && isBlankModule(m); guard++) {
                static_cast<Blank3*>(m)->speed = speed;
                m = m->rightExpander.module;
            }
            assignGroupColors();
        }
    }

    /** Reparte colores libres a los blanks del grupo que aún no tengan. Respeta los ya
        asignados, así que un módulo que llega nuevo se coloca en un hueco de la paleta. */
    void assignGroupColors() {
        bool used[BLANK_PALETTE_LEN] = {};
        Module* m = this;
        for (int guard = 0; guard < BLANK_GROUP_LIMIT && isBlankModule(m); guard++) {
            const Blank3* b = static_cast<const Blank3*>(m);
            if (b->colorMode >= 0)
                used[b->colorMode % BLANK_PALETTE_LEN] = true;
            m = m->rightExpander.module;
        }
        int seen = 0;
        m = this;
        for (int guard = 0; guard < BLANK_GROUP_LIMIT && isBlankModule(m); guard++) {
            Blank3* b = static_cast<Blank3*>(m);
            if (b->colorMode < 0) {
                int c = -1;
                for (int i = 0; i < BLANK_PALETTE_LEN && c < 0; i++)
                    if (!used[i])
                        c = i;
                if (c < 0)
                    c = seen % BLANK_PALETTE_LEN;   // grupo más largo que la paleta
                used[c] = true;
                b->colorMode = c;
            }
            seen++;
            m = m->rightExpander.module;
        }
    }

    void process(const ProcessArgs& args) override {
        if (chainDivider.process())
            updateChain();

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

        const float xMax = groupWidth() - 1.0f;
        const float dt = args.sampleTime * speed;
        for (LogoMark& logo : logos) {
            logo.x += logo.vx * dt;
            logo.y += logo.vy * dt;
            logo.rotation += logo.vr * dt;

            if (logo.x < 1.0f || logo.x > xMax) {
                logo.x = clamp(logo.x, 1.0f, xMax);
                logo.vx *= -0.8f;
            }
            if (logo.y < 5.0f || logo.y > 116.0f) {
                logo.y = clamp(logo.y, 5.0f, 116.0f);
                logo.vy *= -0.8f;
            }
        }
    }

    void processBypass(const ProcessArgs& args) override {
        Module::processBypass(args);
        if (chainDivider.process())
            updateChain();
    }
};

template <typename F>
static void blankForEachInGroup(Module* start, F fn) {
    if (!isBlankModule(start))
        return;
    Module* first = start;
    for (int guard = 0;
         guard < BLANK_GROUP_LIMIT && isBlankModule(first->leftExpander.module);
         guard++)
        first = first->leftExpander.module;
    Module* m = first;
    for (int guard = 0; guard < BLANK_GROUP_LIMIT && isBlankModule(m); guard++) {
        fn(static_cast<Blank3*>(m));
        m = m->rightExpander.module;
    }
}

struct BlankLogoPattern : TransparentWidget {
    Blank3* module = nullptr;
    bool acidSelf = false;                  // qué variante soy yo (para la previsualización)
    std::array<Blank3::LogoMark, 2> previewLogos;

    // Cuánto pesa la carita comparada con el logo Animatek. El smiley es un disco macizo
    // (≈78 % de su caja, frente al ~40 % de un trazo), así que a la misma opacidad tapa
    // el doble de área. Sin corregirlo el blank acid sale mucho más denso que el otro.
    static constexpr float ACID_ALPHA = 0.55f;

    static std::shared_ptr<window::Svg> markSvg(bool acid, int colorIdx) {
        return blankTintedSvg(acid ? "res/AcidLogo.svg" : "res/AnimatekLogo.svg", colorIdx);
    }

    explicit BlankLogoPattern(bool acid) : acidSelf(acid) {
        box.size = mm2px(Vec(BLANK_PANEL_W, 128.5f));

        for (Blank3::LogoMark& logo : previewLogos) {
            logo.x = 2.0f + random::uniform() * 11.24f;
            logo.y = 8.0f + random::uniform() * 104.0f;
            logo.size = 14.0f + random::uniform() * 42.0f;
            logo.rotation = (-45.0f + random::uniform() * 90.0f) * M_PI / 180.0f;
            logo.opacity = 0.10f + random::uniform() * 0.12f;
        }
    }

    void drawMark(const DrawArgs& args, const Blank3::LogoMark& logo,
                  const std::shared_ptr<window::Svg>& svg, float offsetPx, bool acid) {
        if (!svg)
            return;
        Vec svgSize = svg->getSize();
        if (svgSize.x <= 0.f || svgSize.y <= 0.f)
            return;

        const float targetSize = mm2px(logo.size);
        const float cx = mm2px(logo.x) - offsetPx;
        // La marca puede venir de un panel de al lado: si no roza el mío, ni se dibuja.
        if (cx + targetSize < 0.f || cx - targetSize > box.size.x)
            return;

        const float scale = targetSize / std::max(svgSize.x, svgSize.y);
        nvgSave(args.vg);
        nvgTranslate(args.vg, cx, mm2px(logo.y));
        nvgRotate(args.vg, logo.rotation);
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -svgSize.x * 0.5f, -svgSize.y * 0.5f);
        nvgGlobalAlpha(args.vg, logo.opacity * (acid ? ACID_ALPHA : 1.f));
        svg->draw(args.vg);
        nvgRestore(args.vg);
    }

    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        nvgScissor(args.vg, 0.f, 0.f, box.size.x, box.size.y);

        if (!module) {
            // En el navegador no hay cadena: solo el panel y sus propias marcas.
            for (const Blank3::LogoMark& logo : previewLogos)
                drawMark(args, logo, markSvg(acidSelf, 0), 0.f, acidSelf);
        }
        else {
            // Todas las marcas del grupo viven en el mismo sistema de coordenadas, así
            // que cada panel dibuja su rodaja y una carita se ve cruzar de uno a otro.
            const float offsetPx = mm2px(module->groupIndex * BLANK_PANEL_W);
            const Module* first = module;
            for (int guard = 0;
                 guard < BLANK_GROUP_LIMIT && isBlankModule(first->leftExpander.module);
                 guard++)
                first = first->leftExpander.module;

            const Module* m = first;
            for (int guard = 0; guard < BLANK_GROUP_LIMIT && isBlankModule(m); guard++) {
                const Blank3* b = static_cast<const Blank3*>(m);
                const bool acid = (m->model == modelBlankAcid);
                // Cada panel aporta sus marcas con SU color y SU forma, así que un grupo
                // mezcla logos y caritas en colores distintos sobre el mismo lienzo.
                auto svg = markSvg(acid, b->markColor());
                for (const Blank3::LogoMark& logo : b->logos)
                    drawMark(args, logo, svg, offsetPx, acid);
                m = m->rightExpander.module;
            }
        }

        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }
};

// La marca fija de la esquina inferior. Antes iba dentro del SVG del panel, pero ahí no
// se puede teñir: ahora se dibuja aquí, con el color del módulo y con la forma de su
// variante — logo Animatek en el BLANK 3, carita en el BLANK ACID—, para distinguirlos
// de un vistazo. El tamaño reproduce el que tenía en el SVG.
struct BlankBottomLogo : TransparentWidget {
    Blank3* module = nullptr;
    bool acid = false;
    static constexpr float CX = 15.24f / 2.f;
    static constexpr float CY = 123.0f;
    static constexpr float SIZE = 6.14f;    // ancho en mm, el del trazado original

    void draw(const DrawArgs& args) override {
        auto svg = blankTintedSvg(acid ? "res/AcidLogo.svg" : "res/AnimatekLogo.svg",
                                  module ? module->markColor() : 0);
        if (!svg || !svg->handle)
            return;
        Vec svgSize = svg->getSize();
        if (svgSize.x <= 0.f || svgSize.y <= 0.f)
            return;
        const float scale = mm2px(SIZE) / std::max(svgSize.x, svgSize.y);
        nvgSave(args.vg);
        nvgTranslate(args.vg, mm2px(CX), mm2px(CY));
        nvgScale(args.vg, scale, scale);
        nvgTranslate(args.vg, -svgSize.x * 0.5f, -svgSize.y * 0.5f);
        svg->draw(args.vg);
        nvgRestore(args.vg);
    }
};

// El borde del módulo, pero saltándose las juntas internas de un grupo. Rack pinta un
// rectángulo gris alrededor de cada panel; con varios blanks pegados eso deja una reja
// de líneas por el medio del lienzo y rompe la ilusión de que es uno solo. Aquí se
// dibujan solo los lados que dan al exterior del grupo.
// Quitar el borde de Rack no basta: en la junta entre dos paneles queda una hilera
// tenue del corte entre sus framebuffers. Se tapa con una tira del color del fondo,
// que cada panel pone en su mitad. Va DEBAJO de las marcas: si fuese encima partiría
// el lienzo justo en la junta, que es lo que estamos intentando disimular.
struct BlankSeamCover : TransparentWidget {
    Blank3* module = nullptr;

    void draw(const DrawArgs& args) override {
        if (!module)
            return;
        const bool openLeft  = !isBlankModule(module->leftExpander.module);
        const bool openRight = !isBlankModule(module->rightExpander.module);
        if (openLeft && openRight)
            return;

        const float bar = mm2px(0.8f);   // sin tapar las barras azules de arriba y abajo
        nvgBeginPath(args.vg);
        if (!openLeft)
            nvgRect(args.vg, 0.f, bar, 1.5f, box.size.y - 2.f * bar);
        if (!openRight)
            nvgRect(args.vg, box.size.x - 1.5f, bar, 1.5f, box.size.y - 2.f * bar);
        nvgFillColor(args.vg, nvgRGB(0x21, 0x21, 0x21));
        nvgFill(args.vg);
    }
};

struct BlankGroupBorder : TransparentWidget {
    Blank3* module = nullptr;

    void draw(const DrawArgs& args) override {
        const bool openLeft  = !module || !isBlankModule(module->leftExpander.module);
        const bool openRight = !module || !isBlankModule(module->rightExpander.module);

        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0.f, 0.5f);
        nvgLineTo(args.vg, box.size.x, 0.5f);
        nvgMoveTo(args.vg, 0.f, box.size.y - 0.5f);
        nvgLineTo(args.vg, box.size.x, box.size.y - 0.5f);
        if (openLeft) {
            nvgMoveTo(args.vg, 0.5f, 0.f);
            nvgLineTo(args.vg, 0.5f, box.size.y);
        }
        if (openRight) {
            nvgMoveTo(args.vg, box.size.x - 0.5f, 0.f);
            nvgLineTo(args.vg, box.size.x - 0.5f, box.size.y);
        }
        nvgStrokeColor(args.vg, nvgRGBAf(0.5f, 0.5f, 0.5f, 0.5f));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);
    }
};

struct BlankWidgetBase : ModuleWidget {
    BlankWidgetBase(Blank3* module, bool acid) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Blank3.svg")));

        auto* seam = new BlankSeamCover();
        seam->module = module;
        seam->box.size = mm2px(Vec(BLANK_PANEL_W, 128.5f));
        addChild(seam);

        auto* pattern = new BlankLogoPattern(acid);
        pattern->module = module;
        addChild(pattern);

        auto* logo = new BlankBottomLogo();
        logo->module = module;
        logo->acid = acid;
        logo->box.size = mm2px(Vec(BLANK_PANEL_W, 128.5f));
        addChild(logo);

        // El borde de serie vive dentro del framebuffer del panel, así que no puede
        // reaccionar a los vecinos: se apaga y se pone uno propio como hijo directo,
        // que sí se redibuja cada cuadro.
        if (auto* svgPanel = dynamic_cast<app::SvgPanel*>(getPanel()))
            if (svgPanel->panelBorder)
                svgPanel->panelBorder->hide();

        auto* border = new BlankGroupBorder();
        border->module = module;
        border->box.size = box.size;
        addChild(border);
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);

        auto* m = dynamic_cast<Blank3*>(module);

        menu->addChild(new ui::MenuSeparator());
        menu->addChild(createCheckMenuItem(
            "Pause animation", "",
            []() { return blank3AnimationPaused; },
            []() { blank3AnimationPaused = !blank3AnimationPaused; }));

        if (!m)
            return;

        // Velocidad y color se aplican a todo el grupo, no solo a este panel.
        static const float SPEEDS[] = {0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f};
        static const std::vector<std::string> SPEED_NAMES =
            {"0.25x", "0.5x", "1x", "2x", "4x", "8x"};
        menu->addChild(createIndexSubmenuItem(
            "Speed",
            SPEED_NAMES,
            [m]() {
                for (int i = 0; i < (int)SPEED_NAMES.size(); i++)
                    if (m->speed == SPEEDS[i])
                        return i;
                return 2;
            },
            [m](int i) {
                blankForEachInGroup(m, [i](Blank3* b) { b->speed = SPEEDS[i]; });
            }));

        // Cada blank se queda con su color, así que duplicarlo o moverlo no se lo cambia.
        // "Auto" los suelta a todos y cada uno vuelve a coger uno libre, que es la forma
        // de repartir la paleta de nuevo en un grupo.
        std::vector<std::string> colorNames = {"Auto (pick a free one)"};
        for (int i = 0; i < BLANK_PALETTE_LEN; i++)
            colorNames.push_back(BLANK_PALETTE[i].name);
        menu->addChild(createIndexSubmenuItem(
            "Mark colour",
            colorNames,
            [m]() { return m->colorMode + 1; },
            [m](int i) {
                blankForEachInGroup(m, [i](Blank3* b) { b->colorMode = i - 1; });
            }));
    }
};

// Cada variante usa su propia marca tanto abajo como en el lienzo compartido, así que
// un grupo mezclado tiene logos y caritas a la vez.
struct Blank3Widget : BlankWidgetBase {
    explicit Blank3Widget(Blank3* module) : BlankWidgetBase(module, false) {}
};

struct BlankAcidWidget : BlankWidgetBase {
    explicit BlankAcidWidget(Blank3* module) : BlankWidgetBase(module, true) {}
};

Model* modelBlank3 = createModel<Blank3, Blank3Widget>("Blank3");
Model* modelBlankAcid = createModel<Blank3, BlankAcidWidget>("BlankAcid");
