#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"
#include "EclipseShared.hpp"
#include "Scales.hpp"

#include <algorithm>
#include <functional>
#include <vector>
#include <cmath>

using AnimatekUI::TextLabel;
using namespace EclipseShared;

// ECLIPSE: two Euclidean rings in one circle. A (outer) runs on the incoming
// clock, B (inner) on that clock multiplied or divided. Besides each ring's own
// gates, AND / OR / XOR of the two come out as gates too. With ECLIPSE PITCH
// glued to its right, one of the five gates also picks a note in a scale, and
// LOCK loops the last steps. See docs/ECLIPSE-SPEC.md.

namespace {

static constexpr int MAX_STEPS = 64;
static constexpr float DEFAULT_CLOCK_PERIOD = 0.125f;
static constexpr float MIN_CLOCK_PERIOD = 1e-3f;
static constexpr float MAX_CLOCK_PERIOD = 5.f;
static constexpr float RETRIGGER_GAP = 1e-3f;
static constexpr int HISTORY_LEN = MAX_STEPS; // clock edges kept for LOCK

// RATIO switch positions: B's clock relative to A's. Negative = divide.
static constexpr int NUM_RATIOS = 7;
static constexpr int RATIOS[NUM_RATIOS] = {-4, -3, -2, 1, 2, 3, 4};
static const char* const RATIO_TEXT[NUM_RATIOS] = {"/4", "/3", "/2", "x1", "x2", "x3", "x4"};
// A ticks are counted modulo this (a multiple of every divisor), so the
// count stays small and B's division stays on the same beats.
static constexpr int RATIO_CYCLE = 12;

/** Whether `step` of a ring is a hit: `hits` spread as evenly as possible over
`len` steps (Bresenham form of Bjorklund's pattern, first hit on step 0), the
whole pattern turned `rot` steps clockwise. */
static bool euclid_hit(int step, int len, int hits, int rot) {
    if (len < 1 || hits < 1)
        return false;
    hits = std::min(hits, len);
    int i = ((step - rot) % len + len) % len;
    return (i * hits) % len < hits;
}

/** A gate output that can be retriggered while still high: it drops for 1 ms
first, so whatever is downstream sees a new rising edge. */
struct GateOut {
    float high = 0.f;
    float gap = 0.f;

    void fire(float length) {
        if (high > 0.f)
            gap = RETRIGGER_GAP;
        high = std::max(length, RETRIGGER_GAP);
    }

    bool process(float dt) {
        if (gap > 0.f) {
            gap -= dt;
            return false;
        }
        if (high > 0.f) {
            high -= dt;
            return true;
        }
        return false;
    }

    void reset() {
        high = 0.f;
        gap = 0.f;
    }
};

} // namespace

struct Eclipse : AnimatekUI::AccentModule {
    enum ParamId {
        LEN_A_PARAM,
        HITS_A_PARAM,
        ROT_A_PARAM,
        LEN_B_PARAM,
        HITS_B_PARAM,
        ROT_B_PARAM,
        RATIO_PARAM,
        GATE_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        CLOCK_INPUT,
        RESET_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        A_OUTPUT,
        B_OUTPUT,
        AND_OUTPUT,
        OR_OUTPUT,
        XOR_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        A_LIGHT,
        B_LIGHT,
        AND_LIGHT,
        OR_LIGHT,
        XOR_LIGHT,
        LIGHTS_LEN
    };

    /** Everything that decides what the next steps play. A copy is kept at
    every clock edge; LOCK goes back to one and replays from there. */
    struct Snapshot {
        int posA = -1;
        int posB = -1;
        int ticksA = 0;
        int acc = 0;
        uint32_t rng = 1;
        bool stateA = false;
        bool stateB = false;
    };

    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    GateOut gates[NUM_GATES];

    // The running state. -1 positions after a reset: the next clock plays step 0.
    Snapshot st;

    float sinceEdge = 0.f;
    bool haveEdge = false;
    bool havePeriod = false;
    float periodA = DEFAULT_CLOCK_PERIOD;

    // B's in-between ticks when its clock is multiplied.
    int subLeft = 0;
    float subTimer = 0.f;
    float subPeriod = 0.f;

    // The note controls, as the expander last sent them (defaults without one).
    PitchSettings pitch;
    // The note: a degree of the scale, held until the source gate fires again.
    int noteDegree = 0;
    uint32_t noteCount = 0;    // notes picked so far, for the expander's graph

    // LOCK: the last HISTORY_LEN edges, and the one the loop goes back to.
    Snapshot history[HISTORY_LEN];
    int historyHead = 0;
    int historyCount = 0;
    bool lockActive = false;
    Snapshot lockStart;
    int lockCounter = 0;

    // Hold: a ring's state lasts its whole step, so a faster B fills A's hits.
    // Trigger: a ring only counts on the tick it advances.
    bool logicHold = true;

    // Ring B's colour, an index into the palette; -1 until one is given. In
    // auto mode the module takes a colour no other ECLIPSE in the patch has.
    int colorB = -1;
    bool colorAuto = true;
    // Not saved: an auto module looks for a free colour once, when it appears
    // (added, duplicated or loaded), and keeps it from then on.
    bool colorSettled = false;

    // The expander writes its settings into these.
    PitchSettings rightMessages[2];

    Eclipse() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(LEN_A_PARAM, 1.f, (float)MAX_STEPS, 16.f, "A length", " steps")->snapEnabled = true;
        configParam(HITS_A_PARAM, 0.f, (float)MAX_STEPS, 5.f, "A hits")->snapEnabled = true;
        configParam(ROT_A_PARAM, 0.f, (float)(MAX_STEPS - 1), 0.f, "A rotation", " steps")->snapEnabled = true;
        configParam(LEN_B_PARAM, 1.f, (float)MAX_STEPS, 8.f, "B length", " steps")->snapEnabled = true;
        configParam(HITS_B_PARAM, 0.f, (float)MAX_STEPS, 3.f, "B hits")->snapEnabled = true;
        configParam(ROT_B_PARAM, 0.f, (float)(MAX_STEPS - 1), 0.f, "B rotation", " steps")->snapEnabled = true;
        configSwitch(RATIO_PARAM, 0.f, (float)(NUM_RATIOS - 1), 3.f, "B clock",
                     {"÷4", "÷3", "÷2", "×1", "×2", "×3", "×4"});
        configParam(GATE_PARAM, 0.05f, 0.95f, 0.5f, "Gate length", "%", 0.f, 100.f);

        configInput(CLOCK_INPUT, "Clock");
        configInput(RESET_INPUT, "Reset");

        configOutput(A_OUTPUT, "A (outer ring)");
        configOutput(B_OUTPUT, "B (inner ring)");
        configOutput(AND_OUTPUT, "A AND B");
        configOutput(OR_OUTPUT, "A OR B");
        configOutput(XOR_OUTPUT, "A XOR B");

        rightExpander.producerMessage = &rightMessages[0];
        rightExpander.consumerMessage = &rightMessages[1];

        st.rng = random::u32() | 1u;
    }

    int paramInt(int id) {
        return (int)std::round(params[id].getValue());
    }

    int lenA() { return clamp(paramInt(LEN_A_PARAM), 1, MAX_STEPS); }
    int lenB() { return clamp(paramInt(LEN_B_PARAM), 1, MAX_STEPS); }

    int ratio() {
        int idx = paramInt(RATIO_PARAM);
        if (idx < 0 || idx >= NUM_RATIOS)
            return 1;
        return RATIOS[idx];
    }

    float periodB() {
        int r = ratio();
        return r > 0 ? periodA / (float)r : periodA * (float)(-r);
    }

    const AnimatekScales::Scale& scale() {
        int idx = pitch.scale;
        if (idx < 0 || idx >= AnimatekScales::NUM_SCALES)
            idx = 0;
        return AnimatekScales::SCALES[idx];
    }

    /** How many scale degrees the note can move over. */
    int span() {
        return scale().size * clamp(pitch.range, 1, 4);
    }

    int loopLength() {
        return clamp(pitch.loop, 1, MAX_STEPS);
    }

    /** Locked only with the expander there: the lock's controls live on it. */
    bool locked() {
        return pitch.attached && pitch.lock;
    }

    uint32_t nextRandom() {
        // xorshift32: small, and its whole state fits in a snapshot.
        uint32_t x = st.rng;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        st.rng = x;
        return x;
    }

    void restart() {
        uint32_t rng = st.rng;
        st = Snapshot();
        st.rng = rng;
        subLeft = 0;
        subTimer = 0.f;
        historyHead = 0;
        historyCount = 0;
        // A lock that is still on starts again from here: it loops the next steps.
        lockActive = false;
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        logicHold = true;
        restart();
        noteDegree = 0;
        for (GateOut& g : gates)
            g.reset();
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "logicHold", json_boolean(logicHold));
        json_object_set_new(root, "colorB", json_integer(colorB));
        json_object_set_new(root, "colorAuto", json_boolean(colorAuto));
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "logicHold"))
            logicHold = json_is_true(j);
        if (json_t* j = json_object_get(root, "colorB"))
            colorB = (int)json_integer_value(j);
        if (json_t* j = json_object_get(root, "colorAuto"))
            colorAuto = json_is_true(j);
    }

    Module* pitchExpander() {
        Module* right = rightExpander.module;
        return (right && right->model == modelEclipsePitch) ? right : nullptr;
    }

    /** At each clock edge, before it plays: engage or run the lock, then keep
    a copy of the state for a later lock. Locking at edge t goes back to the
    copy from LOOP edges ago, so it loops the steps just heard; every LOOP
    edges after that it goes back to the same copy. */
    void lockAtEdge() {
        const int loop = loopLength();
        if (locked()) {
            if (!lockActive) {
                int back = std::min(loop, historyCount);
                if (back > 0) {
                    int idx = ((historyHead - back) % HISTORY_LEN + HISTORY_LEN) % HISTORY_LEN;
                    lockStart = history[idx];
                } else {
                    lockStart = st;
                }
                lockActive = true;
                lockCounter = 0;
            }
            if (lockCounter % loop == 0)
                st = lockStart;
            lockCounter++;
        } else {
            lockActive = false;
        }
        history[historyHead] = st;
        historyHead = (historyHead + 1) % HISTORY_LEN;
        historyCount = std::min(historyCount + 1, HISTORY_LEN);
    }

    void process(const ProcessArgs& args) override {
        const float dt = args.sampleTime;
        sinceEdge += dt;

        Module* expander = pitchExpander();
        if (expander)
            pitch = *static_cast<const PitchSettings*>(rightExpander.consumerMessage);
        else
            pitch = PitchSettings();

        if (resetTrigger.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.f))
            restart();

        bool tickA = false;
        bool tickB = false;
        const int r = ratio();

        if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
            // The first edge has no previous one to measure against, and a very
            // long gap means the clock was stopped, not a real tempo.
            if (haveEdge && sinceEdge > MIN_CLOCK_PERIOD && sinceEdge <= MAX_CLOCK_PERIOD) {
                periodA = sinceEdge;
                havePeriod = true;
            }
            haveEdge = true;
            sinceEdge = 0.f;
            tickA = true;

            // In-between ticks still pending when the edge arrives (the tempo
            // rose, or there was no period yet to space them) are skipped in
            // silence: B steps over them, so it stays on A's grid.
            if (subLeft > 0 && st.posB >= 0)
                st.posB = (st.posB + subLeft) % lenB();
            subLeft = 0;
            subTimer = 0.f;

            lockAtEdge();

            if (r > 1) {
                tickB = true;
                subLeft = r - 1;
                subPeriod = havePeriod ? periodA / (float)r : 0.f;
            } else if (r < -1) {
                tickB = (st.ticksA % (-r)) == 0;
            } else {
                tickB = true;
            }
            st.ticksA = (st.ticksA + 1) % RATIO_CYCLE;
        } else if (subLeft > 0 && subPeriod > 0.f) {
            subTimer += dt;
            if (subTimer >= subPeriod) {
                subTimer -= subPeriod;
                subLeft--;
                tickB = true;
            }
        }

        if (tickA) {
            int len = lenA();
            st.posA = (st.posA + 1) % len;
            st.stateA = euclid_hit(st.posA, len, paramInt(HITS_A_PARAM), paramInt(ROT_A_PARAM));
        }
        if (tickB) {
            int len = lenB();
            st.posB = (st.posB + 1) % len;
            st.stateB = euclid_hit(st.posB, len, paramInt(HITS_B_PARAM), paramInt(ROT_B_PARAM));
        }

        if (tickA || tickB) {
            const int n = span();
            if (tickA && st.stateA)
                st.acc += pitch.stepA;
            if (tickB && st.stateB)
                st.acc += pitch.stepB;
            st.acc = ((st.acc % n) + n) % n;

            bool fired[NUM_GATES];
            fired[A_OUTPUT] = tickA && st.stateA;
            fired[B_OUTPUT] = tickB && st.stateB;
            bool a = logicHold ? st.stateA : fired[A_OUTPUT];
            bool b = logicHold ? st.stateB : fired[B_OUTPUT];
            fired[AND_OUTPUT] = a && b;
            fired[OR_OUTPUT] = a || b;
            fired[XOR_OUTPUT] = a != b;

            const float gate = params[GATE_PARAM].getValue();
            const float pB = periodB();
            const float pFast = std::min(periodA, pB);
            const float lengths[NUM_GATES] = {periodA, pB, pFast, pFast, pFast};
            for (int i = 0; i < NUM_GATES; ++i)
                if (fired[i])
                    gates[i].fire(gate * lengths[i]);

            int source = clamp(pitch.source, 0, NUM_GATES - 1);
            if (pitch.attached && fired[source]) {
                if (pitch.mode == MODE_RANDOM)
                    noteDegree = (int)(nextRandom() % (uint32_t)n);
                else
                    noteDegree = st.acc;
                noteCount++;
            }
        }

        for (int i = 0; i < NUM_GATES; ++i) {
            bool high = gates[i].process(dt);
            outputs[i].setVoltage(high ? 10.f : 0.f);
            lights[i].setBrightnessSmooth(high ? 1.f : 0.f, dt);
        }

        if (expander) {
            // The pitch is worked out every sample from the held degree, so the
            // scale, key, octave and ROOT IN reach a held note at once.
            const AnimatekScales::Scale& sc = scale();
            const float oct = (float)pitch.octave;
            const int degree = clamp(noteDegree, 0, span() - 1);
            const int octave = degree / sc.size;
            const int semis = sc.degrees[degree % sc.size];
            PitchOutput* out = static_cast<PitchOutput*>(expander->leftExpander.producerMessage);
            out->linked = true;
            out->voct = oct + (float)octave + (float)(pitch.rootSemitones + semis) / 12.f;
            out->root = oct + (float)pitch.rootSemitones / 12.f;
            out->colorB = colorB;
            out->colorA = accentColor;
            out->locked = locked();
            out->noteCount = noteCount;
            out->degree = degree;
            out->span = span();
            out->scaleSize = sc.size;
            expander->leftExpander.requestMessageFlip();
        }
    }
};

// ---------------------------------------------------------------------------
// The ring display
// ---------------------------------------------------------------------------

struct EclipseRingDisplay : TransparentWidget {
    Eclipse* module = nullptr;

    explicit EclipseRingDisplay(Eclipse* module) : module(module) {}

    struct RingView {
        int len, hits, rot, pos;
        bool firing;
    };

    /** What a ring shows: the module's live state, or with no module (the
    browser, the library site) the parameter defaults with the playhead on
    step 0. */
    RingView ring(bool outer) {
        const int lenId = outer ? Eclipse::LEN_A_PARAM : Eclipse::LEN_B_PARAM;
        const int hitsId = outer ? Eclipse::HITS_A_PARAM : Eclipse::HITS_B_PARAM;
        const int rotId = outer ? Eclipse::ROT_A_PARAM : Eclipse::ROT_B_PARAM;
        RingView v;
        if (module) {
            v.len = clamp(module->paramInt(lenId), 1, MAX_STEPS);
            v.hits = std::min(std::max(module->paramInt(hitsId), 0), v.len);
            v.rot = module->paramInt(rotId);
            v.pos = outer ? module->st.posA : module->st.posB;
            v.firing = module->lights[outer ? Eclipse::A_LIGHT : Eclipse::B_LIGHT].getBrightness() > 0.5f;
        } else {
            v.len = outer ? 16 : 8;
            v.hits = outer ? 5 : 3;
            v.rot = 0;
            v.pos = 0;
            v.firing = false;
        }
        if (v.pos >= v.len)
            v.pos = -1;
        return v;
    }

    static NVGcolor withAlpha(NVGcolor c, uint8_t a) {
        c.a = a / 255.f;
        return c;
    }

    static void dot(NVGcontext* vg, float x, float y, float r, NVGcolor c) {
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r);
        nvgFillColor(vg, c);
        nvgFill(vg);
    }

    /** One ring of LEDs, step 0 at the top, running clockwise. */
    void drawRing(NVGcontext* vg, float cx, float cy, float radius, const RingView& v, NVGcolor color) {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, radius);
        nvgStrokeColor(vg, nvgRGBA(0x33, 0x38, 0x4a, 120));
        nvgStrokeWidth(vg, 0.6f);
        nvgStroke(vg);

        const float spacing = 2.f * (float)M_PI * radius / (float)v.len;
        const float baseR = clamp(spacing * 0.32f, 0.9f, 3.4f);
        for (int i = 0; i < v.len; ++i) {
            // Where the pattern starts is drawn a little larger and brighter. It
            // turns with ROT, so the rotation can be read off the ring.
            const bool start = i == ((v.rot % v.len) + v.len) % v.len;
            const float r = start ? baseR * 1.4f : baseR;
            float a = -0.5f * (float)M_PI + 2.f * (float)M_PI * (float)i / (float)v.len;
            float x = cx + radius * std::cos(a);
            float y = cy + radius * std::sin(a);
            bool hit = euclid_hit(i, v.len, v.hits, v.rot);
            if (i == v.pos) {
                if (hit) {
                    dot(vg, x, y, r + (v.firing ? 3.5f : 2.2f), withAlpha(color, v.firing ? 110 : 60));
                    dot(vg, x, y, r * 1.15f, nvgRGB(0xff, 0xff, 0xff));
                } else {
                    dot(vg, x, y, r * 0.95f, nvgRGBA(0xd0, 0xd6, 0xe0, 200));
                }
            } else if (hit) {
                dot(vg, x, y, r, withAlpha(color, start ? 255 : 190));
            } else {
                dot(vg, x, y, r * 0.55f, start ? nvgRGBA(0x8a, 0x95, 0xa8, 200) : nvgRGBA(0x5a, 0x63, 0x75, 130));
            }
        }
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

    /** The rings on the light layer, so they stay lit in a dimmed room. */
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) {
            TransparentWidget::drawLayer(args, layer);
            return;
        }
        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float size = std::min(box.size.x, box.size.y);
        const RingView a = ring(true);
        const RingView b = ring(false);

        // The eclipse: a soft white glow in the middle while AND is high.
        float andLevel = module ? module->lights[Eclipse::AND_LIGHT].getBrightness() : 0.f;
        if (andLevel > 0.01f) {
            NVGpaint glow = nvgRadialGradient(vg, cx, cy, size * 0.05f, size * 0.26f,
                                              nvgRGBA(0xff, 0xff, 0xff, (uint8_t)(70.f * andLevel)),
                                              nvgRGBA(0xff, 0xff, 0xff, 0));
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, size * 0.26f);
            nvgFillPaint(vg, glow);
            nvgFill(vg);
        }

        drawRing(vg, cx, cy, size * 0.43f, a, AnimatekUI::logoBlue());
        const NVGcolor colorB = paletteColor(module ? module->colorB : DEFAULT_B_COLOR);
        drawRing(vg, cx, cy, size * 0.30f, b, colorB);

        std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
        if (font && font->handle >= 0) {
            int ratioIdx = module ? module->paramInt(Eclipse::RATIO_PARAM) : 3;
            if (ratioIdx < 0 || ratioIdx >= NUM_RATIOS)
                ratioIdx = 3;
            std::string textA = string::f("%d/%d", a.hits, a.len);
            std::string textB = string::f("%d/%d", b.hits, b.len);
            std::string textR = RATIO_TEXT[ratioIdx];

            nvgFontFaceId(vg, font->handle);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFontSize(vg, size * 0.105f);
            nvgFillColor(vg, AnimatekUI::logoBlue());
            nvgText(vg, cx, cy - size * 0.085f, textA.c_str(), nullptr);
            nvgFillColor(vg, colorB);
            nvgText(vg, cx, cy + size * 0.035f, textB.c_str(), nullptr);
            nvgFontSize(vg, size * 0.075f);
            nvgFillColor(vg, withAlpha(colorB, 170));
            nvgText(vg, cx, cy + size * 0.13f, textR.c_str(), nullptr);

            // The lock rewinds the rings, so it is shown here too.
            if (module && module->locked()) {
                nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
                nvgFontSize(vg, size * 0.07f);
                nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
                std::string lock = string::f("LOCK %d", module->loopLength());
                nvgText(vg, box.size.x - size * 0.04f, size * 0.04f, lock.c_str(), nullptr);
            }
        }
        TransparentWidget::drawLayer(args, layer);
    }
};

// ---------------------------------------------------------------------------
// Panel: 8 HP
// ---------------------------------------------------------------------------

namespace EclipseLayout {
static constexpr float TOP_LABEL_Y = 2.2f;
static constexpr float TOP_Y = 10.5f;
static constexpr float DISPLAY_X = 1.32f;
static constexpr float DISPLAY_Y = 15.5f;
static constexpr float DISPLAY_SIZE = 38.f;
static constexpr float RING_A_Y = 63.f;
static constexpr float RING_B_Y = 77.f;
static constexpr float KNOB_X[3] = {14.f, 24.5f, 35.f};
static constexpr float RING_TAG_X = 4.5f;
// How far the A / B tags' label box sits above the knob centre: enough for the
// capital to look centred on the knob row.
static constexpr float RING_TAG_RISE = 2.9f;
static constexpr float OUT_ROW1_Y = 92.5f;
static constexpr float OUT_ROW2_Y = 107.f;
} // namespace EclipseLayout

/** The A / B tag beside each ring's knobs, which can be clicked: it rolls
that ring's LEN, HITS and ROT. Looks like any other label until hovered. */
struct RingDiceTag : TextLabel {
    std::function<void()> action;
    bool hovered = false;

    void onHover(const HoverEvent& e) override {
        e.consume(this);
    }
    void onEnter(const EnterEvent& e) override {
        hovered = true;
    }
    void onLeave(const LeaveEvent& e) override {
        hovered = false;
    }
    void onButton(const ButtonEvent& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action)
                action();
            e.consume(this);
        }
    }

    void draw(const DrawArgs& args) override {
        if (hovered) {
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y + 1.f, 2.f);
            NVGcolor c = color;
            c.a = 0.18f;
            nvgFillColor(args.vg, c);
            nvgFill(args.vg);
        }
        TextLabel::draw(args);
    }
};

struct EclipseWidget : AnimatekUI::AccentModuleWidget {
    // The module's colour is ring A's, beside Ring B colour in the menu.
    std::string accentMenuName() override { return "Ring A colour"; }

    // Everything drawn in ring B's colour, repainted when it changes.
    std::vector<TextLabel*> colorBLabels;
    MultiLightWidget* colorBLight = nullptr;

    explicit EclipseWidget(Eclipse* module) {
        using namespace EclipseLayout;
        setModule(module);
        setAccentPanel("res/Eclipse.svg");

        auto label = [&](const char* text, float x, float y, float w = 12.f, NVGcolor color = nvgRGBA(0, 0, 0, 0),
                         float fontSize = 7.5f) {
            auto* l = new TextLabel(text, mm2px(Vec(x - w * 0.5f, y)), mm2px(Vec(w, 4.f)));
            l->fontSize = fontSize;
            l->color = color;
            addChild(l);
            return l;
        };
        const NVGcolor colorB = paletteColor(DEFAULT_B_COLOR);

        AnimatekUI::addModuleName(this, "ECLIPSE");

        // Top row: clock, reset, B's clock ratio and the gate length.
        label("CLK", 6.f, TOP_LABEL_Y, 10.f);
        label("RST", 15.5f, TOP_LABEL_Y, 10.f);
        colorBLabels.push_back(label("RATIO", 25.f, TOP_LABEL_Y, 12.f, colorB));
        label("GATE", 34.6f, TOP_LABEL_Y, 10.f);
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(6.f, TOP_Y)), module, Eclipse::CLOCK_INPUT));
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(15.5f, TOP_Y)), module, Eclipse::RESET_INPUT));
        addParam(createParamCentered<AnimatekUI::FlatSmallKnob>(mm2px(Vec(25.f, TOP_Y)), module, Eclipse::RATIO_PARAM));
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(34.6f, TOP_Y)), module, Eclipse::GATE_PARAM));

        auto* display = new EclipseRingDisplay(module);
        display->box.pos = mm2px(Vec(DISPLAY_X, DISPLAY_Y));
        display->box.size = mm2px(Vec(DISPLAY_SIZE, DISPLAY_SIZE));
        addChild(display);

        // One row of knobs per ring, tagged with the ring's colour.
        const char* knobNames[3] = {"LEN", "HITS", "ROT"};
        const int knobsA[3] = {Eclipse::LEN_A_PARAM, Eclipse::HITS_A_PARAM, Eclipse::ROT_A_PARAM};
        const int knobsB[3] = {Eclipse::LEN_B_PARAM, Eclipse::HITS_B_PARAM, Eclipse::ROT_B_PARAM};
        // The A and B tags roll their ring when clicked.
        auto tag = [&](const char* text, float y, NVGcolor color, bool outer) {
            auto* t = new RingDiceTag;
            t->text = text;
            t->box.pos = mm2px(Vec(RING_TAG_X - 3.f, y - RING_TAG_RISE));
            t->box.size = mm2px(Vec(6.f, 4.f));
            t->fontSize = 11.f;
            t->color = color;
            t->action = [this, outer]() { randomizeRing(outer); };
            addChild(t);
            return t;
        };
        tag("A", RING_A_Y, AnimatekUI::logoBlue(), true);
        colorBLabels.push_back(tag("B", RING_B_Y, colorB, false));
        for (int i = 0; i < 3; ++i) {
            label(knobNames[i], KNOB_X[i], RING_A_Y - 8.4f, 10.f);
            addParam(createParamCentered<AnimatekUI::FlatSmallKnob>(mm2px(Vec(KNOB_X[i], RING_A_Y)), module, knobsA[i]));
            label(knobNames[i], KNOB_X[i], RING_B_Y - 8.4f, 10.f);
            addParam(createParamCentered<AnimatekUI::FlatSmallKnob>(mm2px(Vec(KNOB_X[i], RING_B_Y)), module, knobsB[i]));
        }

        // Outputs: each ring's own gates, then the logic. A light beside each
        // label shows its gate.
        struct Out {
            const char* name;
            float x, y;
            int output, light;
            NVGcolor color;
            int lightKind; // 0 blue, 1 ring B's colour, 2 white
        };
        const NVGcolor plain = nvgRGBA(0, 0, 0, 0);
        const Out outs[] = {
            {"A", 7.f, OUT_ROW1_Y, Eclipse::A_OUTPUT, Eclipse::A_LIGHT, AnimatekUI::logoBlue(), 0},
            {"B", 20.32f, OUT_ROW1_Y, Eclipse::B_OUTPUT, Eclipse::B_LIGHT, colorB, 1},
            {"AND", 33.64f, OUT_ROW1_Y, Eclipse::AND_OUTPUT, Eclipse::AND_LIGHT, plain, 2},
            {"OR", 13.66f, OUT_ROW2_Y, Eclipse::OR_OUTPUT, Eclipse::OR_LIGHT, plain, 2},
            {"XOR", 26.98f, OUT_ROW2_Y, Eclipse::XOR_OUTPUT, Eclipse::XOR_LIGHT, plain, 2},
        };
        for (const Out& o : outs) {
            TextLabel* l = label(o.name, o.x, o.y - 8.3f, 10.f, o.color);
            if (o.lightKind == 1)
                colorBLabels.push_back(l);
            addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(o.x, o.y)), module, o.output));
            Vec lp = mm2px(Vec(o.x + 4.6f, o.y - 5.6f));
            if (o.lightKind == 0)
                addChild(createLightCentered<SmallLight<BlueLight>>(lp, module, o.light));
            else if (o.lightKind == 1) {
                colorBLight = createLightCentered<SmallLight<YellowLight>>(lp, module, o.light);
                colorBLight->baseColors[0] = colorB;
                addChild(colorBLight);
            } else
                addChild(createLightCentered<SmallLight<WhiteLight>>(lp, module, o.light));
        }
    }

    /** In auto mode, a module that has just appeared takes a colour that no
    other ECLIPSE in the patch is using. One that clashes with a module already
    settled (the original, when this one is a duplicate) moves; between two
    newcomers, the one with the higher id does. A module never changes colour
    after that, so manual choices and older modules are left alone. */
    void settleColor(Eclipse* m) {
        if (m->colorSettled)
            return;
        m->colorSettled = true;
        if (!m->colorAuto)
            return;
        bool used[AnimatekUI::PALETTE_LEN] = {};
        bool clash = false;
        int others = 0;
        for (int64_t id : APP->engine->getModuleIds()) {
            Eclipse* other = dynamic_cast<Eclipse*>(APP->engine->getModule(id));
            if (!other || other == m)
                continue;
            others++;
            int c = other->colorB;
            if (c >= 0 && c < AnimatekUI::PALETTE_LEN)
                used[c] = true;
            if (c == m->colorB && (other->colorSettled || other->id < m->id))
                clash = true;
        }
        if (m->colorB >= FIRST_B_COLOR && m->colorB < AnimatekUI::PALETTE_LEN && !clash)
            return;
        int pick = -1;
        for (int i = FIRST_B_COLOR; i < AnimatekUI::PALETTE_LEN && pick < 0; ++i)
            if (!used[i])
                pick = i;
        if (pick < 0)   // more ECLIPSEs than colours: go round the palette again
            pick = FIRST_B_COLOR + others % (AnimatekUI::PALETTE_LEN - FIRST_B_COLOR);
        m->colorB = pick;
    }

    void step() override {
        if (auto* m = dynamic_cast<Eclipse*>(module)) {
            settleColor(m);
            const NVGcolor c = paletteColor(m->colorB);
            for (TextLabel* l : colorBLabels)
                l->color = c;
            if (colorBLight)
                colorBLight->baseColors[0] = c;
        }
        AnimatekUI::AccentModuleWidget::step();
    }

    /** Rolls a ring: LEN from 3 to 32, HITS from 1 to one less than LEN (a
    ring full of hits says nothing), ROT anywhere in it. One undo step. */
    void randomizeRing(bool outer) {
        if (!module)
            return;
        const int ids[3] = {outer ? Eclipse::LEN_A_PARAM : Eclipse::LEN_B_PARAM,
                            outer ? Eclipse::HITS_A_PARAM : Eclipse::HITS_B_PARAM,
                            outer ? Eclipse::ROT_A_PARAM : Eclipse::ROT_B_PARAM};
        const int len = 3 + (int)(random::u32() % 30u);
        const int hits = 1 + (int)(random::u32() % (uint32_t)(len - 1));
        const int rot = (int)(random::u32() % (uint32_t)len);
        const float values[3] = {(float)len, (float)hits, (float)rot};

        history::ComplexAction* h = new history::ComplexAction;
        h->name = outer ? "roll ring A" : "roll ring B";
        for (int i = 0; i < 3; ++i) {
            engine::ParamQuantity* pq = module->paramQuantities[ids[i]];
            history::ParamChange* c = new history::ParamChange;
            c->name = h->name;
            c->moduleId = module->id;
            c->paramId = ids[i];
            c->oldValue = pq->getValue();
            c->newValue = values[i];
            pq->setValue(values[i]);
            h->push(c);
        }
        APP->history->push(h);
    }

    /** Creates ECLIPSE PITCH and glues it to this module's right, pushing
    whatever was there aside, so it links at once. One undo removes it. */
    void addPitchExpander() {
        engine::Module* em = modelEclipsePitch->createModule();
        APP->engine->addModule(em);
        ModuleWidget* mw = modelEclipsePitch->createModuleWidget(em);
        APP->scene->rack->addModule(mw);
        APP->scene->rack->setModulePosForce(mw, box.pos.plus(Vec(box.size.x, 0.f)));
        mw->loadTemplate();

        history::ModuleAdd* h = new history::ModuleAdd;
        h->name = "add pitch expander";
        h->setModule(mw);
        APP->history->push(h);
    }

    void appendContextMenu(ui::Menu* menu) override {
        AnimatekUI::AccentModuleWidget::appendContextMenu(menu);
        auto* m = dynamic_cast<Eclipse*>(module);
        if (!m)
            return;
        menu->addChild(new ui::MenuSeparator());

        bool hasExpander = m->pitchExpander() != nullptr;
        menu->addChild(createMenuItem(hasExpander ? "Pitch expander attached" : "Add pitch expander", "",
                                      [=]() { addPitchExpander(); }, hasExpander));

        std::vector<std::string> colorNames = {"Auto (pick a free one)"};
        for (int i = FIRST_B_COLOR; i < AnimatekUI::PALETTE_LEN; ++i)
            colorNames.push_back(AnimatekUI::PALETTE[i].name);
        menu->addChild(createIndexSubmenuItem(
            "Ring B colour",
            colorNames,
            [=]() { return m->colorAuto ? 0 : (size_t)std::max(m->colorB - FIRST_B_COLOR + 1, 0); },
            [=](size_t i) {
                if (i == 0) {
                    // Let go of the colour and look for a free one again.
                    m->colorAuto = true;
                    m->colorB = -1;
                    m->colorSettled = false;
                } else {
                    m->colorAuto = false;
                    m->colorB = FIRST_B_COLOR + (int)i - 1;
                }
            }));

        menu->addChild(createIndexSubmenuItem(
            "Logic",
            {"Hold (a hit lasts its whole step)", "Trigger (only the tick it fires)"},
            [=]() { return m->logicHold ? 0 : 1; },
            [=](size_t i) { m->logicHold = (i == 0); }));
    }
};

Model* modelEclipse = createModel<Eclipse, EclipseWidget>("Eclipse");
