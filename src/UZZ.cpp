#include "plugin.hpp"
#include <algorithm>
#include <vector>
#include <cmath>
using namespace rack;
using namespace rack::componentlibrary;

// ============================================================================

// LAYOUT (toca solo esto para mover la GUI)
// ============================================================================
namespace UI {
    static constexpr int   COLS            = 16;
    static constexpr float LEFT            = 48.f;
    static constexpr float RIGHT           = 1.f;

    static constexpr float Y_STEP_LED      = 10.f;
    static constexpr float Y_STEP_MODE     = 10.f;
    static constexpr float Y_NOTE          = 30.f;

    static constexpr float Y_PITCH         = 72.f;
    static constexpr float Y_OCT           = 120.f;
    static constexpr float Y_DUR           = 168.f;
    static constexpr float Y_C1            = 216.f;
    static constexpr float Y_C2            = 264.f;

    static constexpr float RAND_X          = LEFT - 10.f;
    static constexpr float SHIFT_X         = RAND_X - 18.f;
    static constexpr float SHIFT_X_OFFSET  = 15.f;
    static constexpr float SHIFT_Y_DELTA   = 14.f;
    static constexpr float SHIFT_Y_FINE    = -1.f;
    static constexpr float ROW_SHIFT_SCALE = 0.85f;
    static constexpr float RAND_BTN_SCALE  = 0.90f;
    static constexpr float RAND_BTN_X_OFFSET = -3.f;
    static constexpr float PORT_SCALE      = 0.90f;
    static constexpr float TRIG_RIGHT_PAD  = 14.f;
    static constexpr float BOTTOM_MARGIN   = 28.f;

    static constexpr float TRIG_LEFT_GAP = 23.f;
    inline float trigLeftX() { return RAND_X - TRIG_LEFT_GAP; }
    inline float rowShiftX() { return SHIFT_X + SHIFT_X_OFFSET; }
    inline float rowShiftYUp(float centerY) { return centerY - SHIFT_Y_DELTA + SHIFT_Y_FINE; }
    inline float rowShiftYDown(float centerY) { return centerY + SHIFT_Y_DELTA - SHIFT_Y_FINE; }
    inline float randButtonX() { return RAND_X + RAND_BTN_X_OFFSET; }
    inline float usable(float boxW) { return boxW - LEFT - RIGHT; }
    inline float colW(float boxW)   { return usable(boxW) / float(COLS); }
    inline float colCenter(float boxW, int i) { return LEFT + (i + 0.5f) * colW(boxW); }
    inline float trigX(float boxW)  { return colCenter(boxW, 15) + (colW(boxW) * 0.5f) + TRIG_RIGHT_PAD; }
}

namespace UIAssets {
    static constexpr const char* INPUT_PORT_SVG  = "res/port_input.svg";
    static constexpr const char* OUTPUT_PORT_SVG = "res/port_output.svg";
}

// ============================================================================

std::shared_ptr<window::Svg> loadPluginSvgIfExists(const char* relPath) {
    if (!relPath)
        return nullptr;
    std::string path = asset::plugin(pluginInstance, relPath);
    if (system::exists(path))
        return APP->window->loadSvg(path);
    return nullptr;
}

// ============================================================================

// Helpers
// ============================================================================
static inline int wrap16(int x) { return x & 15; }

// ============================================================================

// CLOCK RATIO
// ============================================================================
static constexpr float RATIO_TABLE[] = {
    1.f/48.f, 1.f/32.f, 1.f/24.f, 1.f/16.f, 1.f/12.f, 1.f/10.f,
    1.f/8.f,  1.f/6.f,  1.f/5.f,  1.f/4.f,  1.f/3.f,  1.f/2.5f, 1.f/2.f,  1.f/1.5f,
    1.f,
    1.5f, 2.f, 2.5f, 3.f, 4.f, 5.f, 6.f, 8.f, 10.f, 12.f, 16.f, 24.f, 32.f, 48.f
};
static constexpr int   NUM_RATIOS = (int)(sizeof(RATIO_TABLE)/sizeof(RATIO_TABLE[0]));
static constexpr int   RATIO_DEFAULT_INDEX = 16;

static constexpr const char* RATIO_LABELS[NUM_RATIOS] = {
    "÷48","÷32","÷24","÷16","÷12","÷10",
    "÷8","÷6","÷5","÷4","÷3","÷2.5","÷2","÷1.5",
    "×1",
    "×1.5","×2","×2.5","×3","×4","×5","×6","×8","×10","×12","×16","×24","×32","×48"
};

struct RatioQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        int idx = clamp((int) std::round(getValue()), 0, NUM_RATIOS - 1);
        return RATIO_LABELS[idx];
    }
    std::string getUnit() override { return ""; }
};

// ============================================================================

// Dirección
// ============================================================================
enum DirectionMode { DIR_FWD=0, DIR_REV=1, DIR_PINGPONG=2, DIR_RANDOM=3, DIR_DRUNK=4 };
static constexpr const char* DIR_LABELS[] = { "Forward","Backward","Ping-Pong","Random","Drunk" };

struct DirModeQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        int idx = clamp((int)std::round(getValue()), 0, 4);
        return DIR_LABELS[idx];
    }
    std::string getUnit() override { return ""; }
};

// ============================================================================

// Rangos de M1/M2 (global, usado por módulo y menú contextual)
// ============================================================================
namespace UZZRanges {
    enum ModRange {
        MR_PM10, MR_PM5, MR_PM3, MR_PM2, MR_PM1,
        MR_0_10, MR_0_5, MR_0_3, MR_0_2, MR_0_1,
        MR_COUNT
    };
    struct RangeDef { const char* label; float minV; float maxV; };

    static const RangeDef RANGE_DEFS[MR_COUNT] = {
        { "+/-10V", -10.f,  10.f },
        { "+/-5V" , -5.f ,   5.f },
        { "+/-3V" , -3.f ,   3.f },
        { "+/-2V" , -2.f ,   2.f },
        { "+/-1V" , -1.f ,   1.f },
        { "0V-10V",  0.f ,  10.f },
        { "0V-5V" ,  0.f ,   5.f },
        { "0V-3V" ,  0.f ,   3.f },
        { "0V-2V" ,  0.f ,   2.f },
        { "0V-1V" ,  0.f ,   1.f },
    };

    inline float mapMod0_10ToRange(float raw0_10, int r) {
        const RangeDef& d = RANGE_DEFS[ clamp(r, 0, MR_COUNT-1) ];
        float t = clamp(raw0_10, 0.f, 10.f) / 10.f;  // 0..1
        return d.minV + (d.maxV - d.minV) * t;
    }
} // namespace UZZRanges

// ============================================================================

// Módulo
// ============================================================================
struct UZZ : Module {
    enum ParamIds {
        PITCH_0, PITCH_1, PITCH_2, PITCH_3, PITCH_4, PITCH_5, PITCH_6, PITCH_7,
        PITCH_8, PITCH_9, PITCH_10, PITCH_11, PITCH_12, PITCH_13, PITCH_14, PITCH_15,

        OCT_0, OCT_1, OCT_2, OCT_3, OCT_4, OCT_5, OCT_6, OCT_7,
        OCT_8, OCT_9, OCT_10, OCT_11, OCT_12, OCT_13, OCT_14, OCT_15,

        STEP_MODE_0, STEP_MODE_1, STEP_MODE_2, STEP_MODE_3,
        STEP_MODE_4, STEP_MODE_5, STEP_MODE_6, STEP_MODE_7,
        STEP_MODE_8, STEP_MODE_9, STEP_MODE_10, STEP_MODE_11,
        STEP_MODE_12, STEP_MODE_13, STEP_MODE_14, STEP_MODE_15,

        DUR_0, DUR_1, DUR_2, DUR_3, DUR_4, DUR_5, DUR_6, DUR_7,
        DUR_8, DUR_9, DUR_10, DUR_11, DUR_12, DUR_13, DUR_14, DUR_15,

        M1_0, M1_1, M1_2, M1_3, M1_4, M1_5, M1_6, M1_7,
        M1_8, M1_9, M1_10, M1_11, M1_12, M1_13, M1_14, M1_15,

        M2_0, M2_1, M2_2, M2_3, M2_4, M2_5, M2_6, M2_7,
        M2_8, M2_9, M2_10, M2_11, M2_12, M2_13, M2_14, M2_15,

        STEPS_PARAM, START_PARAM, DIR_MODE_PARAM,
        GATE_MODE_PARAM, RATIO_IDX_PARAM,

        SWING_PARAM,   // 0..0.6 (0..60%)
        SLEW_PARAM,    // 0..2 s

        RND_PITCH_PARAM, RND_OCTAVE_PARAM, RND_STEP_PARAM,
        RND_DUR_PARAM,  RND_M1_PARAM,      RND_M2_PARAM,

        PITCH_SHIFT_DOWN_PARAM, PITCH_SHIFT_UP_PARAM,
        OCT_SHIFT_DOWN_PARAM,   OCT_SHIFT_UP_PARAM,
        DUR_SHIFT_DOWN_PARAM,   DUR_SHIFT_UP_PARAM,
        M1_SHIFT_DOWN_PARAM,    M1_SHIFT_UP_PARAM,
        M2_SHIFT_DOWN_PARAM,    M2_SHIFT_UP_PARAM,

        NUM_PARAMS
    };

    enum InputIds  {
        CLK_INPUT, RESET_INPUT,
        RND_PITCH_TRIG_INPUT, RND_OCT_TRIG_INPUT, RND_STEP_TRIG_INPUT,
        RND_DUR_TRIG_INPUT, RND_M1_TRIG_INPUT, RND_M2_TRIG_INPUT,
        XPOSE_INPUT, NUM_INPUTS
    };

    enum OutputIds {
        PITCH_OUTPUT, GATE_OUTPUT, STEP_GATES_OUTPUT, EOC_OUTPUT, M1_OUTPUT, M2_OUTPUT, NUM_OUTPUTS
    };

    enum LightIds {
        STEP_LIGHT_0, STEP_LIGHT_1, STEP_LIGHT_2, STEP_LIGHT_3,
        STEP_LIGHT_4, STEP_LIGHT_5, STEP_LIGHT_6, STEP_LIGHT_7,
        STEP_LIGHT_8, STEP_LIGHT_9, STEP_LIGHT_10, STEP_LIGHT_11,
        STEP_LIGHT_12, STEP_LIGHT_13, STEP_LIGHT_14, STEP_LIGHT_15,
        RND_LIGHT, RND_OCT_LIGHT, RND_STEP_LIGHT, RND_DUR_LIGHT, RND_M1_LIGHT, RND_M2_LIGHT, NUM_LIGHTS
    };

    // Estado
    int step = 0;
    dsp::SchmittTrigger clkTrig, rstTrig;
    dsp::PulseGenerator gatePulse, eocPulse, stepGateTrig[16];
    float trigLen = 0.010f, gateLen = 0.100f;

    float timeSinceClk = 0.f;
    float lastPeriod   = 0.f;

    dsp::BooleanTrigger rndPitchTrig, rndOctTrig, rndStepTrig;
    dsp::SchmittTrigger rndPitchCvTrig, rndOctCvTrig, rndStepCvTrig;

    dsp::BooleanTrigger rndDurTrig, rndM1Trig, rndM2Trig;
    dsp::SchmittTrigger rndDurCvTrig, rndM1CvTrig, rndM2CvTrig;

    dsp::BooleanTrigger shiftPitchUpTrig, shiftPitchDownTrig;
    dsp::BooleanTrigger shiftOctUpTrig,   shiftOctDownTrig;
    dsp::BooleanTrigger shiftDurUpTrig,   shiftDurDownTrig;
    dsp::BooleanTrigger shiftM1UpTrig,    shiftM1DownTrig;
    dsp::BooleanTrigger shiftM2UpTrig,    shiftM2DownTrig;

    bool skipNextPitchRandom = false;
    bool skipNextOctRandom   = false;
    bool skipNextStepRandom  = false;
    bool skipNextDurRandom   = false;
    bool skipNextM1Random    = false;
    bool skipNextM2Random    = false;

    // Primer tick tras RESET debe tocar el paso actual (sin avanzar)
    bool playCurrentOnNextTick = false;
    bool resetPending = false;
    int  resetTargetStep = 0;
    bool eocOnReset = false;

    float virtTimer  = 0.f;
    float virtPeriod = 0.125f;
    bool  clockWasConnected = false;

    bool  havePhase = false;
    float sinceLastEdge = 0.f;
    bool  prevClkConnected = false;

    int   pingDir = 0;
    int   drunkDir = 1;

    int   swingPhase = 0;
    int   queuedBaseTicks = 0;
    bool  tickPending = false;
    float pendingDelay = 0.f;
    float pendingTimer = 0.f;

    float pitchOut = 0.f;
    bool  pitchInit = false;
    // Anti-rebote para ticks (segundos desde el último tick REAL emitido)
    float sinceLastTick = 1e9f;

    // Rangos M1/M2 (estado)
    int m1Range = UZZRanges::MR_0_10;
    int m2Range = UZZRanges::MR_0_10;

    UZZ() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        for (int i = 0; i < 16; ++i) {
            configParam(PITCH_0 + i, 0.f, 11.f, 0.f, string::f("Pitch %d", i + 1), " semitones");
            paramQuantities[PITCH_0 + i]->snapEnabled = true;

            configParam(OCT_0 + i, -2.f, 2.f, 0.f, string::f("Octave %d", i + 1), " oct");
            paramQuantities[OCT_0 + i]->snapEnabled = true;

            configParam(STEP_MODE_0 + i, 0.f, 2.f, 0.f, string::f("Step mode %d", i + 1));
            paramQuantities[STEP_MODE_0 + i]->snapEnabled = true;

            configParam(DUR_0 + i, 0.005f, 2.0f, 0.100f, string::f("Duration %d", i + 1), " s");

            // 0..10 como valor base; luego se escala al rango elegido
            configParam(M1_0 + i, 0.f, 10.f, 0.f, string::f("Mod 1 %d", i + 1), " (0..10)");
            configParam(M2_0 + i, 0.f, 10.f, 0.f, string::f("Mod 2 %d", i + 1), " (0..10)");
        }

        configParam(STEPS_PARAM, 1.f, 16.f, 16.f, "Steps");  paramQuantities[STEPS_PARAM]->snapEnabled = true;
        configParam(START_PARAM, 1.f, 16.f,  1.f, "Start");  paramQuantities[START_PARAM]->snapEnabled = true;

        configParam<DirModeQuantity>(DIR_MODE_PARAM, 0.f, 4.f, 0.f, "Direction mode");
        paramQuantities[DIR_MODE_PARAM]->snapEnabled = true;

        configParam(GATE_MODE_PARAM, 0.f, 1.f, 0.f, "Gate mode (0=Gate,1=Trig)");
        paramQuantities[GATE_MODE_PARAM]->snapEnabled = true;

        configParam<RatioQuantity>(RATIO_IDX_PARAM, 0.f, (float)(NUM_RATIOS - 1),
                                   (float)RATIO_DEFAULT_INDEX, "Clock ratio");
        paramQuantities[RATIO_IDX_PARAM]->snapEnabled = true;

        configParam(SWING_PARAM, 0.f, 0.6f, 0.f, "Swing", "%", 0.f, 100.f, 0.f);
        configParam(SLEW_PARAM,  0.f, 2.0f, 0.f, "Glide (slew)", " s");

        configParam(RND_PITCH_PARAM,  0.f, 1.f, 0.f, "Randomize pitch");
        configParam(RND_OCTAVE_PARAM, 0.f, 1.f, 0.f, "Randomize octave");
        configParam(RND_STEP_PARAM,   0.f, 1.f, 0.f, "Randomize step-mode");
        configParam(RND_DUR_PARAM,    0.f, 1.f, 0.f, "Randomize duration");
        configParam(RND_M1_PARAM,     0.f, 1.f, 0.f, "Randomize mod1");
        configParam(RND_M2_PARAM,     0.f, 1.f, 0.f, "Randomize mod2");

        configButton(PITCH_SHIFT_DOWN_PARAM, "Shift pitch row down");
        configButton(PITCH_SHIFT_UP_PARAM,   "Shift pitch row up");
        configButton(OCT_SHIFT_DOWN_PARAM,   "Shift octave row down");
        configButton(OCT_SHIFT_UP_PARAM,     "Shift octave row up");
        configButton(DUR_SHIFT_DOWN_PARAM,   "Shift duration row down");
        configButton(DUR_SHIFT_UP_PARAM,     "Shift duration row up");
        configButton(M1_SHIFT_DOWN_PARAM,    "Shift mod1 row down");
        configButton(M1_SHIFT_UP_PARAM,      "Shift mod1 row up");
        configButton(M2_SHIFT_DOWN_PARAM,    "Shift mod2 row down");
        configButton(M2_SHIFT_UP_PARAM,      "Shift mod2 row up");

        // Entradas/Salidas
        configInput (CLK_INPUT,   "Clock");
        configInput (RESET_INPUT, "Reset");
        configInput (RND_PITCH_TRIG_INPUT, "Randomize pitch (trig)");
        configInput (RND_OCT_TRIG_INPUT,   "Randomize octave (trig)");
        configInput (RND_STEP_TRIG_INPUT,  "Randomize step-mode (trig)");
        configInput (RND_DUR_TRIG_INPUT,   "Randomize duration (trig)");
        configInput (RND_M1_TRIG_INPUT,    "Randomize mod1 (trig)");
        configInput (RND_M2_TRIG_INPUT,    "Randomize mod2 (trig)");
        configInput (XPOSE_INPUT,          "Transpose (1V/oct)");

        configOutput(PITCH_OUTPUT,      "Pitch (1V/oct)");
        configOutput(GATE_OUTPUT,       "Gate/Trig");
        configOutput(STEP_GATES_OUTPUT, "Step gates (poly)");
        configOutput(EOC_OUTPUT,        "End of cycle");
        configOutput(M1_OUTPUT,         "Mod 1");
        configOutput(M2_OUTPUT,         "Mod 2");
    }

    void onReset() override {
        int start = clamp((int) std::round(params[START_PARAM].getValue()) - 1, 0, 15);
        step = start;

        // Limpieza completa de generadores
        gatePulse.reset();
        eocPulse.reset();
        for (int i = 0; i < 16; ++i) stepGateTrig[i].reset();

        for (int i = 0; i < 16; ++i) lights[STEP_LIGHT_0 + i].setBrightness(0.f);

        timeSinceClk = 0.f;
        lastPeriod   = 0.f;
        virtTimer    = 0.f;
        virtPeriod   = 0.125f;
        clockWasConnected = false;
        havePhase = false;
        sinceLastEdge = 0.f;
        prevClkConnected = false;

        pingDir = 0;
        drunkDir = 1;

        swingPhase = 0;
        queuedBaseTicks = 0;
        tickPending = false;
        pendingDelay = 0.f;
        pendingTimer = 0.f;

        pitchInit = false;
        pitchOut  = 0.f;

        sinceLastTick = 1e9f;

        playCurrentOnNextTick = false; // se armará en un RESET externo
        resetPending = false;
        resetTargetStep = start;
    }

    // --- Random helpers ---
    void randomizePitch()     { for (int i = 0; i < 16; ++i) params[PITCH_0 + i].setValue((int)std::floor(random::uniform()*12.f)); }
    void randomizeOctaves()   { for (int i = 0; i < 16; ++i) params[OCT_0   + i].setValue((int)std::floor(random::uniform()*5.f)-2); }
    void randomizeStepMode()  { for (int i = 0; i < 16; ++i){ float r=random::uniform(); int m=(r<0.70f)?0:((r<0.90f)?1:2); params[STEP_MODE_0+i].setValue(m);} }

    void resetPitchRow()      { for (int i = 0; i < 16; ++i) params[PITCH_0 + i].setValue(0.f); }
    void resetOctaveRow()     { for (int i = 0; i < 16; ++i) params[OCT_0   + i].setValue(0.f); }
    void resetStepModeRow()   { for (int i = 0; i < 16; ++i) params[STEP_MODE_0 + i].setValue(0.f); }

    void randomizeDurations() { for (int i = 0; i < 16; ++i) params[DUR_0 + i].setValue(0.005f + random::uniform() * (2.0f - 0.005f)); }
    void randomizeM1()        { for (int i = 0; i < 16; ++i) params[M1_0  + i].setValue(random::uniform() * 10.f); }
    void randomizeM2()        { for (int i = 0; i < 16; ++i) params[M2_0  + i].setValue(random::uniform() * 10.f); }

    void resetDurRow()  { for (int i = 0; i < 16; ++i) params[DUR_0 + i].setValue(0.100f); }
    void resetM1Row()   { for (int i = 0; i < 16; ++i) params[M1_0  + i].setValue(0.f); }
    void resetM2Row()   { for (int i = 0; i < 16; ++i) params[M2_0  + i].setValue(0.f); }

    static float quantize_to_step(float value, float min_val, float step) {
        if (step <= 0.f)
            return value;
        float steps_from_min = std::round((value - min_val) / step);
        return min_val + steps_from_min * step;
    }

    void get_active_window(int& start_idx, int& count) {
        count = clamp((int) std::round(params[STEPS_PARAM].getValue()), 1, 16);
        start_idx = clamp((int) std::round(params[START_PARAM].getValue()) - 1, 0, 15);
    }

    void shift_row_int(int base_param, int dir, int start_idx, int count, int min_val, int max_val) {
        if (count <= 0)
            return;
        int step_dir = (dir >= 0) ? 1 : -1;
        for (int i = 0; i < count; ++i) {
            int idx = wrap16(start_idx + i);
            int param_id = base_param + idx;
            int current = (int) std::round(params[param_id].getValue());
            int next = current + step_dir;
            next = clamp(next, min_val, max_val);
            params[param_id].setValue((float) next);
        }
    }

    void shift_row_float(int base_param, int dir, int start_idx, int count, float step_amount,
                         float min_val, float max_val, bool quantize) {
        if (count <= 0)
            return;
        int step_dir = (dir >= 0) ? 1 : -1;
        for (int i = 0; i < count; ++i) {
            int idx = wrap16(start_idx + i);
            int param_id = base_param + idx;
            float current = params[param_id].getValue();
            if (quantize)
                current = quantize_to_step(current, min_val, step_amount);
            float next = current + (float) step_dir * step_amount;
            if (quantize)
                next = quantize_to_step(next, min_val, step_amount);
            next = clamp(next, min_val, max_val);
            params[param_id].setValue(next);
        }
    }

    void shift_pitch_row(int dir) {
        int start_idx = 0, count = 0;
        get_active_window(start_idx, count);
        shift_row_int(PITCH_0, dir, start_idx, count, 0, 11);
    }

    void shift_oct_row(int dir) {
        int start_idx = 0, count = 0;
        get_active_window(start_idx, count);
        shift_row_int(OCT_0, dir, start_idx, count, -2, 2);
    }

    void shift_dur_row(int dir) {
        int start_idx = 0, count = 0;
        get_active_window(start_idx, count);
        shift_row_float(DUR_0, dir, start_idx, count, 0.1f, 0.005f, 2.0f, true);
    }

    void shift_m1_row(int dir) {
        int start_idx = 0, count = 0;
        get_active_window(start_idx, count);
        shift_row_float(M1_0, dir, start_idx, count, 1.f, 0.f, 10.f, true);
    }

    void shift_m2_row(int dir) {
        int start_idx = 0, count = 0;
        get_active_window(start_idx, count);
        shift_row_float(M2_0, dir, start_idx, count, 1.f, 0.f, 10.f, true);
    }

    // Persistencia de rangos
    json_t* dataToJson() override {
        json_t* root = Module::dataToJson();
        json_object_set_new(root, "m1Range", json_integer(m1Range));
        json_object_set_new(root, "m2Range", json_integer(m2Range));
        json_object_set_new(root, "eocOnReset", json_boolean(eocOnReset));
        return root;
    }
    void dataFromJson(json_t* root) override {
        Module::dataFromJson(root);
        if (auto* j = json_object_get(root, "m1Range")) m1Range = clamp((int)json_integer_value(j), 0, UZZRanges::MR_COUNT-1);
        if (auto* j = json_object_get(root, "m2Range")) m2Range = clamp((int)json_integer_value(j), 0, UZZRanges::MR_COUNT-1);
        if (auto* j = json_object_get(root, "eocOnReset")) eocOnReset = json_is_true(j);
    }

    // Navegación entre pasos
    int findNextPlayable(int start, int len, int dir, int currentRel, bool& allSkip) {
        allSkip = true;
        int rel = currentRel;
        for (int tries = 0; tries < len; ++tries) {
            rel = (dir == 0) ? (rel + 1) % len : (rel - 1 + len) % len;
            int cand = wrap16(start + rel);
            if ((int) std::round(params[STEP_MODE_0 + cand].getValue()) != 2) { allSkip = false; return cand; }
        }
        int relTheo = (dir == 0) ? (currentRel + 1) % len : (currentRel - 1 + len) % len;
        return wrap16(start + relTheo);
    }
    int findNeighborPlayable(int start, int len, int currentRel, int dirSign, bool& allSkip) {
        allSkip = true;
        int rel = currentRel;
        for (int tries = 0; tries < len; ++tries) {
            rel = (dirSign > 0) ? (rel + 1) % len : (rel - 1 + len) % len;
            int cand = wrap16(start + rel);
            if ((int) std::round(params[STEP_MODE_0 + cand].getValue()) != 2) { allSkip = false; return cand; }
        }
        int relTheo = (dirSign > 0) ? (currentRel + 1) % len : (currentRel - 1 + len) % len;
        return wrap16(start + relTheo);
    }

    void hardStop(int steps) {
        gatePulse.reset();
        eocPulse.reset();
        for (int ch = 0; ch < 16; ++ch) stepGateTrig[ch].reset();
        virtTimer = 0.f;
        lastPeriod = 0.f;
        havePhase = false;
        sinceLastEdge = 0.f;
        clockWasConnected = false;

        outputs[GATE_OUTPUT].setVoltage(0.f);
        outputs[STEP_GATES_OUTPUT].setChannels(steps);
        for (int ch = 0; ch < steps; ++ch)
            outputs[STEP_GATES_OUTPUT].setVoltage(0.f, ch);
        outputs[EOC_OUTPUT].setVoltage(0.f);
    }

    void process(const ProcessArgs& args) override {
        // ========= Estado cable clock =========
        bool clkConnected = inputs[CLK_INPUT].isConnected();
        if (clkConnected) {
            timeSinceClk  += args.sampleTime;
            sinceLastEdge += args.sampleTime;
        } else {
            timeSinceClk = 0.f;
        }

        // Llevar la cuenta del tiempo desde el último tick real (anti-rebote)
        sinceLastTick += args.sampleTime;

        // Random por botón
        if (rndPitchTrig.process(params[RND_PITCH_PARAM].getValue()  > .5f)) { if (!skipNextPitchRandom) randomizePitch();   skipNextPitchRandom = false; }
        if (rndOctTrig  .process(params[RND_OCTAVE_PARAM].getValue() > .5f)) { if (!skipNextOctRandom)   randomizeOctaves(); skipNextOctRandom   = false; }
        if (rndStepTrig .process(params[RND_STEP_PARAM].getValue()   > .5f)) { if (!skipNextStepRandom)  randomizeStepMode();skipNextStepRandom  = false; }
        if (rndDurTrig  .process(params[RND_DUR_PARAM].getValue()    > .5f)) { if (!skipNextDurRandom)   randomizeDurations();skipNextDurRandom  = false; }
        if (rndM1Trig   .process(params[RND_M1_PARAM].getValue()     > .5f)) { if (!skipNextM1Random)    randomizeM1();      skipNextM1Random   = false; }
        if (rndM2Trig   .process(params[RND_M2_PARAM].getValue()     > .5f)) { if (!skipNextM2Random)    randomizeM2();      skipNextM2Random   = false; }

        lights[RND_LIGHT].setSmoothBrightness(params[RND_PITCH_PARAM].getValue(),  args.sampleTime);
        lights[RND_OCT_LIGHT].setSmoothBrightness(params[RND_OCTAVE_PARAM].getValue(), args.sampleTime);
        lights[RND_STEP_LIGHT].setSmoothBrightness(params[RND_STEP_PARAM].getValue(),   args.sampleTime);
        lights[RND_DUR_LIGHT].setSmoothBrightness(params[RND_DUR_PARAM].getValue(),     args.sampleTime);
        lights[RND_M1_LIGHT].setSmoothBrightness(params[RND_M1_PARAM].getValue(),       args.sampleTime);
        lights[RND_M2_LIGHT].setSmoothBrightness(params[RND_M2_PARAM].getValue(),       args.sampleTime);

        if (shiftPitchUpTrig.process(params[PITCH_SHIFT_UP_PARAM].getValue() > 0.5f)) shift_pitch_row(+1);
        if (shiftPitchDownTrig.process(params[PITCH_SHIFT_DOWN_PARAM].getValue() > 0.5f)) shift_pitch_row(-1);
        if (shiftOctUpTrig.process(params[OCT_SHIFT_UP_PARAM].getValue() > 0.5f)) shift_oct_row(+1);
        if (shiftOctDownTrig.process(params[OCT_SHIFT_DOWN_PARAM].getValue() > 0.5f)) shift_oct_row(-1);
        if (shiftDurUpTrig.process(params[DUR_SHIFT_UP_PARAM].getValue() > 0.5f)) shift_dur_row(+1);
        if (shiftDurDownTrig.process(params[DUR_SHIFT_DOWN_PARAM].getValue() > 0.5f)) shift_dur_row(-1);
        if (shiftM1UpTrig.process(params[M1_SHIFT_UP_PARAM].getValue() > 0.5f)) shift_m1_row(+1);
        if (shiftM1DownTrig.process(params[M1_SHIFT_DOWN_PARAM].getValue() > 0.5f)) shift_m1_row(-1);
        if (shiftM2UpTrig.process(params[M2_SHIFT_UP_PARAM].getValue() > 0.5f)) shift_m2_row(+1);
        if (shiftM2DownTrig.process(params[M2_SHIFT_DOWN_PARAM].getValue() > 0.5f)) shift_m2_row(-1);

        // Random por TRIG (CV)
        if (rndPitchCvTrig.process(inputs[RND_PITCH_TRIG_INPUT].getVoltage())) randomizePitch();
        if (rndOctCvTrig  .process(inputs[RND_OCT_TRIG_INPUT].getVoltage()))   randomizeOctaves();
        if (rndStepCvTrig .process(inputs[RND_STEP_TRIG_INPUT].getVoltage()))  randomizeStepMode();
        if (rndDurCvTrig  .process(inputs[RND_DUR_TRIG_INPUT].getVoltage()))   randomizeDurations();
        if (rndM1CvTrig   .process(inputs[RND_M1_TRIG_INPUT].getVoltage()))    randomizeM1();
        if (rndM2CvTrig   .process(inputs[RND_M2_TRIG_INPUT].getVoltage()))    randomizeM2();

        // Ventana
        int steps = clamp((int) std::round(params[STEPS_PARAM].getValue()), 1, 16);
        int start = clamp((int) std::round(params[START_PARAM].getValue()) - 1, 0, 15);

        int rel = (step - start + 16) & 15;
        if (rel >= steps) { rel = rel % steps; step = wrap16(start + rel); }

        // RESET — limpieza completa pero sin “romper” el enganche del reloj
        if (rstTrig.process(inputs[RESET_INPUT].getVoltage())) {
            resetPending = true;
            resetTargetStep = start;
            playCurrentOnNextTick = true;

            gatePulse.reset();
            eocPulse.reset();
            for (int i = 0; i < 16; ++i) stepGateTrig[i].reset();

            if (eocOnReset)
                eocPulse.trigger(trigLen);

            pingDir = 0;
            drunkDir = 1;
            swingPhase = 0;

            queuedBaseTicks = 0;
            tickPending = false;
            pendingDelay = 0.f;
            pendingTimer = 0.f;

            virtTimer = 0.f;          // la próxima arista real relockea fase
            // havePhase se mantiene; si no llega borde a tiempo saltará el timeout
        }

        // XPOSE
        int xposeSemis = 0;
        if (inputs[XPOSE_INPUT].isConnected()) {
            float v = inputs[XPOSE_INPUT].getVoltage();
            if (std::isfinite(v)) {
                xposeSemis = (int) std::round(v * 12.f);
                xposeSemis = clamp(xposeSemis, -48, 48);
            }
        }

        // Pitch (pre-slew)
        float semis = params[PITCH_0 + step].getValue();
        int   octIv = (int) std::round(params[OCT_0 + step].getValue());
        float pitchV = ((semis + (float)xposeSemis) / 12.f) + (float)octIv;

        // MOD1/MOD2 siempre activos (independiente del clock)
        outputs[M1_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(params[M1_0 + step].getValue(), m1Range));
        outputs[M2_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(params[M2_0 + step].getValue(), m2Range));

        // ========== SIN CLOCK ==========
        if (!clkConnected) {
            if (prevClkConnected) {
                hardStop(steps);
            } else {
                outputs[GATE_OUTPUT].setVoltage(0.f);
                outputs[STEP_GATES_OUTPUT].setChannels(steps);
                for (int ch = 0; ch < steps; ++ch)
                    outputs[STEP_GATES_OUTPUT].setVoltage(0.f, ch);
                outputs[EOC_OUTPUT].setVoltage(0.f);
            }
            for (int i = 0; i < 16; ++i)
                lights[STEP_LIGHT_0 + i].setSmoothBrightness(i == step ? 1.f : 0.f, args.sampleTime);

            float slewSec = params[SLEW_PARAM].getValue();
            if (slewSec <= 1e-6f) {
                pitchOut = pitchV; pitchInit = true;
            } else {
                float tau = std::max(1e-5f, slewSec);
                float alpha = 1.f - std::exp(-args.sampleTime / tau);
                if (!pitchInit) { pitchOut = pitchV; pitchInit = true; }
                pitchOut += (pitchV - pitchOut) * alpha;
            }
            outputs[PITCH_OUTPUT].setVoltage(pitchOut);

            prevClkConnected = false;
            return;
        }

        // =================== CLOCK + RATIO ===================
        int   ratioIdx = clamp((int) std::round(params[RATIO_IDX_PARAM].getValue()), 0, NUM_RATIOS - 1);
        float ratio    = RATIO_TABLE[ratioIdx];

        bool extPulse = clkTrig.process(inputs[CLK_INPUT].getVoltage());
        if (extPulse) {
            lastPeriod    = timeSinceClk;
            timeSinceClk  = 0.f;
            sinceLastEdge = 0.f;

            if (lastPeriod > 1e-4f)
                virtPeriod = lastPeriod / std::max(ratio, 1e-6f);

            // ¿multiplicador entero? (×2, ×3, ×4, …)
            bool isIntMultiplier = (ratio >= 1.f) && (std::fabs(ratio - std::round(ratio)) < 1e-4f);

            // Para multiplicadores realineamos fase; para divisores no tocamos virtTimer
            if (ratio >= 1.f) {
                virtTimer = 0.f;
                // En ×N enteros, encolar tick inmediato en el mismo flanco externo
                if (isIntMultiplier) {
                    queuedBaseTicks++;
                }
            }

            clockWasConnected = true;
            havePhase = true;
        }

        // Timeout de pérdida de fase
        float timeout = 0.5f;
        if (lastPeriod > 1e-4f)
            timeout = clamp(lastPeriod * 2.f, 0.1f, 1.0f);

        if (havePhase && sinceLastEdge > timeout) {
            havePhase = false;
            virtTimer = 0.f;
        }

        // El oscilador virtual es la fuente de subdivisiones/divisiones (sin duplicar)
        if (havePhase && virtPeriod > 0.f) {
            virtTimer += args.sampleTime;
            while (virtTimer >= virtPeriod) {
                virtTimer -= virtPeriod;
                queuedBaseTicks++;
            }
        }

        bool clockNow = false;

        // Tick diferido por swing
        if (tickPending) {
            pendingTimer += args.sampleTime;
            if (pendingTimer >= pendingDelay) {
                tickPending = false;
                pendingTimer = 0.f;
                clockNow = true;
                swingPhase++;
                sinceLastTick = 0.f;    // marca tick REAL
            }
        }

        // Consumir colas si no hay tick pendiente
        if (!tickPending && !clockNow && queuedBaseTicks > 0) {
            // Diseña el retardo de swing
            float swingAmt = clamp(params[SWING_PARAM].getValue(), 0.f, 0.6f);
            float s = (1.f/3.f) * swingAmt; // 0..0.2 del periodo
            bool isOdd = (swingPhase & 1) == 1;
            pendingDelay = isOdd ? (s * virtPeriod) : 0.f;

            // Si no hay retardo, aplicamos anti-rebote (ticks demasiado juntos)
            if (pendingDelay <= 1e-9f) {
                if (sinceLastTick < 0.0005f) {
                    // Demasiado pronto: descarta este tick
                    queuedBaseTicks--;
                } else {
                    queuedBaseTicks--;
                    clockNow = true;
                    swingPhase++;
                    sinceLastTick = 0.f; // tick REAL
                }
            } else {
                tickPending = true;
                pendingTimer = 0.f;
                queuedBaseTicks--;
            }
        }

        if (clockNow) {
            int modeDir = clamp((int) std::round(params[DIR_MODE_PARAM].getValue()), 0, 4);

            int relBefore = (step - start + 16) & 15;
            bool allSkip = false;
            int nextStep = step;
            bool wrapped = false;

            if (!playCurrentOnNextTick) {
                if (modeDir == DIR_FWD || modeDir == DIR_REV) {
                    int dir = (modeDir == DIR_FWD) ? 0 : 1;
                    nextStep = findNextPlayable(start, steps, dir, relBefore, allSkip);
                    int relAfter = (nextStep - start + 16) & 15;
                    wrapped = (dir == 0) ? (relAfter < relBefore) : (relAfter > relBefore);
                }
                else if (modeDir == DIR_PINGPONG) {
                    int dir = pingDir;
                    bool all1 = false;
                    int cand1 = findNextPlayable(start, steps, dir, relBefore, all1);
                    int relAfter1 = (cand1 - start + 16) & 15;
                    bool wouldWrap = (dir == 0) ? (relAfter1 < relBefore) : (relAfter1 > relBefore);
                    if (!all1 && !wouldWrap) {
                        nextStep = cand1; wrapped = false;
                    } else {
                        pingDir = 1 - pingDir;
                        int dir2 = pingDir;
                        bool all2 = false;
                        int cand2 = findNextPlayable(start, steps, dir2, relBefore, all2);
                        nextStep = cand2; wrapped = true;
                    }
                }
                else if (modeDir == DIR_RANDOM) {
                    std::vector<int> pool; pool.reserve(steps);
                    for (int k = 0; k < steps; ++k) {
                        int sIdx = wrap16(start + k);
                        int m = (int) std::round(params[STEP_MODE_0 + sIdx].getValue());
                        if (m != 2) pool.push_back(sIdx);
                    }
                    if (!pool.empty()) {
                        int idx = (int) std::floor(random::uniform() * pool.size());
                        idx = clamp(idx, 0, (int)pool.size() - 1);
                        nextStep = pool[idx];
                    } else {
                        bool dummy = false;
                        nextStep = findNextPlayable(start, steps, 0, relBefore, dummy);
                    }
                    wrapped = false;
                }
                else { // DRUNK
                    drunkDir = (random::uniform() < 0.5f) ? -1 : 1;
                    bool allA = false;
                    int candA = findNeighborPlayable(start, steps, relBefore, drunkDir, allA);
                    if (!allA) nextStep = candA;
                    else {
                        bool allB = false;
                        int candB = findNeighborPlayable(start, steps, relBefore, -drunkDir, allB);
                        nextStep = candB;
                    }
                    int relAfter = (nextStep - start + 16) & 15;
                    wrapped = (relBefore == 0 && relAfter == (steps - 1)) || (relBefore == (steps - 1) && relAfter == 0);
                }
            } else {
                // Primer tick tras RESET: no avances; toca el paso actual
                nextStep = step;
                wrapped = false;
                allSkip = ((int) std::round(params[STEP_MODE_0 + nextStep].getValue()) == 2);
                playCurrentOnNextTick = false; // desarmar para ticks siguientes
            }

            step = nextStep;
            if (wrapped) eocPulse.trigger(trigLen);

            bool muteGlobal = false;
            if (modeDir == DIR_FWD || modeDir == DIR_REV) muteGlobal = allSkip;
            else {
                bool anyPlayable = false;
                for (int k = 0; k < steps; ++k) {
                    int sIdx = wrap16(start + k);
                    if ((int) std::round(params[STEP_MODE_0 + sIdx].getValue()) != 2) { anyPlayable = true; break; }
                }
                muteGlobal = !anyPlayable;
            }

            int mode = (int) std::round(params[STEP_MODE_0 + step].getValue());
            int k    = (step - start + 16) & 15;

            bool resetFiresAfterGate = resetPending;
            if (!muteGlobal && mode == 0) {
                int   gateMode = (int) std::round(params[GATE_MODE_PARAM].getValue());
                float userDur  = clamp(params[DUR_0 + step].getValue(), 0.001f, 10.0f);

                // Duración efectiva base
                float gLen = (gateMode == 0) ? userDur : trigLen;

                // --- ANTI-CONTINUO EN MULTIPLICACIÓN ---
                // Exige un tiempo "off" mínimo entre sub-pulsos.
                if (gateMode == 0 && ratio > 1.f && virtPeriod > 0.f) {
                    // 1) no ocupar más del 90% del periodo
                    float maxDuty = virtPeriod * 0.90f;
                    if (gLen > maxDuty) gLen = maxDuty;

                    // 2) deja al menos 1 ms (o 2 samples) de LOW garantizado
                    float minOff = std::max(0.001f, 2.f * args.sampleTime);   // 1 ms o 2 muestras
                    float maxLen = virtPeriod - minOff;
                    if (gLen > maxLen) gLen = std::max(trigLen, maxLen);      // nunca negativo
                }

                // Disparo (idéntico para salida principal y poly por paso)
                gatePulse.trigger(gLen);
                stepGateTrig[k].trigger(gLen);
            }
            else {
                gatePulse.reset();
            }

            if (resetFiresAfterGate) {
                step = resetTargetStep;
                playCurrentOnNextTick = true;
                resetPending = false;
            }
        }

        if (!clockNow && resetPending) {
            step = resetTargetStep;
            playCurrentOnNextTick = true;
            resetPending = false;
        }

        // Gate principal
        outputs[GATE_OUTPUT].setVoltage(gatePulse.process(args.sampleTime) ? 10.f : 0.f);

        // Poly step gates
        outputs[STEP_GATES_OUTPUT].setChannels(steps);
        for (int ch = 0; ch < steps; ++ch) {
            bool h = stepGateTrig[ch].process(args.sampleTime);
            outputs[STEP_GATES_OUTPUT].setVoltage(h ? 10.f : 0.f, ch);
        }

        // EOC
        outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);

        // Slew (glide) sobre PITCH
        {
            float slewSec = params[SLEW_PARAM].getValue();
            if (slewSec <= 1e-6f) {
                pitchOut = pitchV; pitchInit = true;
            } else {
                float tau = std::max(1e-5f, slewSec);
                float alpha;
                float expAlpha = 1.f - std::exp(-args.sampleTime / tau);
                alpha = std::sqrt(std::min(expAlpha, 1.f));
                if (!pitchInit) { pitchOut = pitchV; pitchInit = true; }
                pitchOut += (pitchV - pitchOut) * alpha;
            }
            outputs[PITCH_OUTPUT].setVoltage(pitchOut);
        }

        for (int i = 0; i < 16; ++i)
            lights[STEP_LIGHT_0 + i].setSmoothBrightness(i == step ? 1.f : 0.f, args.sampleTime);

        prevClkConnected = true;
    }
};

// ============================================================================
// Widgets custom
// ============================================================================
struct RndPitchButton : TL1105 {
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
    void onDoubleClick(const event::DoubleClick& e) override {
        if (auto q = getParamQuantity())
            if (auto m = dynamic_cast<UZZ*>(q->module)) { m->resetPitchRow(); m->skipNextPitchRandom = true; }
        e.consume(this);
    }
};
struct RndOctButton : TL1105 {
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
    void onDoubleClick(const event::DoubleClick& e) override {
        if (auto q = getParamQuantity())
            if (auto m = dynamic_cast<UZZ*>(q->module)) { m->resetOctaveRow(); m->skipNextOctRandom = true; }
        e.consume(this);
    }
};
struct RndStepButton : TL1105 {
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
    void onDoubleClick(const event::DoubleClick& e) override {
        if (auto q = getParamQuantity())
            if (auto m = dynamic_cast<UZZ*>(q->module)) { m->resetStepModeRow(); m->skipNextStepRandom = true; }
        e.consume(this);
    }
};
struct RndDurButton : TL1105 {
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
    void onDoubleClick(const event::DoubleClick& e) override {
        if (auto q = getParamQuantity())
            if (auto m = dynamic_cast<UZZ*>(q->module)) { m->resetDurRow(); m->skipNextDurRandom = true; }
        e.consume(this);
    }
};
struct RndM1Button : TL1105 {
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
    void onDoubleClick(const event::DoubleClick& e) override {
        if (auto q = getParamQuantity())
            if (auto m = dynamic_cast<UZZ*>(q->module)) { m->resetM1Row(); m->skipNextM1Random = true; }
        e.consume(this);
    }
};
struct RndM2Button : TL1105 {
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::RAND_BTN_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
    void onDoubleClick(const event::DoubleClick& e) override {
        if (auto q = getParamQuantity())
            if (auto m = dynamic_cast<UZZ*>(q->module)) { m->resetM2Row(); m->skipNextM2Random = true; }
        e.consume(this);
    }
};


struct UzzInputPort : PJ301MPort {
    UzzInputPort() {
        if (auto svg = loadPluginSvgIfExists(UIAssets::INPUT_PORT_SVG))
            setSvg(svg);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::PORT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        PJ301MPort::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::PORT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        PJ301MPort::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
};

struct UzzOutputPort : PJ301MPort {
    UzzOutputPort() {
        if (auto svg = loadPluginSvgIfExists(UIAssets::OUTPUT_PORT_SVG))
            setSvg(svg);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::PORT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        PJ301MPort::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::PORT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        PJ301MPort::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
};

struct RowShiftUpButton : app::SvgSwitch {
    RowShiftUpButton() {
        momentary = true;
        shadow->visible = false;
        auto svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/Up.svg"));
        addFrame(svg);
        addFrame(svg);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::ROW_SHIFT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::ROW_SHIFT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
};

struct RowShiftDownButton : app::SvgSwitch {
    RowShiftDownButton() {
        momentary = true;
        shadow->visible = false;
        auto svg = APP->window->loadSvg(asset::plugin(pluginInstance, "res/Down.svg"));
        addFrame(svg);
        addFrame(svg);
    }
    void draw(const DrawArgs& args) override {
        nvgSave(args.vg);
        float scale = UI::ROW_SHIFT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::draw(args);
        nvgRestore(args.vg);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        nvgSave(args.vg);
        float scale = UI::ROW_SHIFT_SCALE;
        Vec center = box.size.mult(0.5f);
        nvgTranslate(args.vg, center.x * (1.f - scale), center.y * (1.f - scale));
        nvgScale(args.vg, scale, scale);
        SvgSwitch::drawLayer(args, layer);
        nvgRestore(args.vg);
    }
};

struct StepModeButton : app::SvgSwitch {
    StepModeButton() {
        momentary = false;
        shadow->visible = false;
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/step_play.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/step_mute.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/step_skip.svg"))); 
    }
};

struct NoteLabel : TransparentWidget {
    UZZ* module = nullptr;
    int stepIndex = 0;
    static std::shared_ptr<Font> font;

    NoteLabel(UZZ* m, int i) {
        module = m; stepIndex = i; box.size = Vec(24.f, 12.f);
        if (!font) font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
    }
    void draw(const DrawArgs& args) override {
        if (!module) return;
        int s   = (int) std::round(module->params[UZZ::PITCH_0 + stepIndex].getValue());
        int oct = (int) std::round(module->params[UZZ::OCT_0   + stepIndex].getValue()) + 4;
        static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        std::string txt = string::f("%s%d", N[(s%12+12)%12], oct);
        nvgFontSize(args.vg, 10.f); if (font) nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, nvgRGB(0xC8,0xD4,0xE3));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(args.vg, box.size.x * .5f, box.size.y * .5f, txt.c_str(), nullptr);
    }
};
std::shared_ptr<Font> NoteLabel::font = nullptr;

// ============================================================================

// Widget del módulo
// ============================================================================
struct UZZWidget : ModuleWidget {
    UZZWidget(UZZ* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/UZZ.svg")));

        const int cols = UI::COLS;
        auto Xc = [&](int i){ return UI::colCenter(box.size.x, i); };

        for (int i = 0; i < cols; ++i)
            addChild(createLightCentered<SmallLight<BlueLight>>(Vec(Xc(i)-10, UI::Y_STEP_LED), module, UZZ::STEP_LIGHT_0 + i));
        for (int i = 0; i < cols; ++i)
            addParam(createParamCentered<StepModeButton>(Vec(Xc(i) + 10, UI::Y_STEP_MODE), module, UZZ::STEP_MODE_0 + i));
        for (int i = 0; i < cols; ++i) {
            auto* lbl = new NoteLabel(module, i);
            lbl->box.pos = Vec(Xc(i) - lbl->box.size.x * .2f - 18, UI::Y_NOTE +2 );
            addChild(lbl);
        }

        for (int i = 0; i < cols; ++i)
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(Xc(i), UI::Y_PITCH), module, UZZ::PITCH_0 + i));
        for (int i = 0; i < cols; ++i)
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(Xc(i), UI::Y_OCT), module, UZZ::OCT_0 + i));

        for (int i = 0; i < cols; ++i)
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(Xc(i), UI::Y_DUR), module, UZZ::DUR_0 + i));
        for (int i = 0; i < cols; ++i)
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(Xc(i), UI::Y_C1), module, UZZ::M1_0 + i));
        for (int i = 0; i < cols; ++i)
            addParam(createParamCentered<RoundSmallBlackKnob>(Vec(Xc(i), UI::Y_C2), module, UZZ::M2_0 + i));

        const float trigL = UI::trigLeftX();

        addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_STEP_MODE + 18), module, UZZ::RND_STEP_TRIG_INPUT));
        addParam(createParamCentered<RndStepButton>(Vec(UI::randButtonX(), UI::Y_STEP_MODE + 18), module, UZZ::RND_STEP_PARAM));

        addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_PITCH), module, UZZ::RND_PITCH_TRIG_INPUT));
        addParam(createParamCentered<RndPitchButton>(Vec(UI::randButtonX(), UI::Y_PITCH), module, UZZ::RND_PITCH_PARAM));

        addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_OCT), module, UZZ::RND_OCT_TRIG_INPUT));
        addParam(createParamCentered<RndOctButton>(Vec(UI::randButtonX(), UI::Y_OCT), module, UZZ::RND_OCTAVE_PARAM));

        addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_DUR), module, UZZ::RND_DUR_TRIG_INPUT));
        addParam(createParamCentered<RndDurButton>(Vec(UI::randButtonX(), UI::Y_DUR), module, UZZ::RND_DUR_PARAM));

        addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_C1), module, UZZ::RND_M1_TRIG_INPUT));
        addParam(createParamCentered<RndM1Button>(Vec(UI::randButtonX(), UI::Y_C1), module, UZZ::RND_M1_PARAM));

        addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_C2), module, UZZ::RND_M2_TRIG_INPUT));
        addParam(createParamCentered<RndM2Button>(Vec(UI::randButtonX(), UI::Y_C2), module, UZZ::RND_M2_PARAM));

        auto addShiftPair = [&](float y, int downParam, int upParam) {
            addParam(createParamCentered<RowShiftUpButton>(Vec(UI::rowShiftX(), UI::rowShiftYUp(y)), module, upParam));
            addParam(createParamCentered<RowShiftDownButton>(Vec(UI::rowShiftX(), UI::rowShiftYDown(y)), module, downParam));
        };

        addShiftPair(UI::Y_PITCH, UZZ::PITCH_SHIFT_DOWN_PARAM, UZZ::PITCH_SHIFT_UP_PARAM);
        addShiftPair(UI::Y_OCT,   UZZ::OCT_SHIFT_DOWN_PARAM,   UZZ::OCT_SHIFT_UP_PARAM);
        addShiftPair(UI::Y_DUR,   UZZ::DUR_SHIFT_DOWN_PARAM,   UZZ::DUR_SHIFT_UP_PARAM);
        addShiftPair(UI::Y_C1,    UZZ::M1_SHIFT_DOWN_PARAM,    UZZ::M1_SHIFT_UP_PARAM);
        addShiftPair(UI::Y_C2,    UZZ::M2_SHIFT_DOWN_PARAM,    UZZ::M2_SHIFT_UP_PARAM);

        const float bottomBase = box.size.y - UI::BOTTOM_MARGIN;
        const float yBottomRow = bottomBase + 5.f;
        const float yMiddleRow = bottomBase - 24.f;
        const float yTopRow    = bottomBase - 52.f;

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(229.f, yBottomRow), module, UZZ::START_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(229.f, yMiddleRow), module, UZZ::STEPS_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(229.f, yTopRow),    module, UZZ::RATIO_IDX_PARAM));

        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(332.f, yTopRow),    module, UZZ::DIR_MODE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(332.f, yMiddleRow), module, UZZ::SWING_PARAM));

        addInput(createInputCentered<UzzInputPort>(Vec(130.f, yTopRow),    module, UZZ::CLK_INPUT));
        addInput(createInputCentered<UzzInputPort>(Vec(130.f, yMiddleRow), module, UZZ::RESET_INPUT));
        addInput(createInputCentered<UzzInputPort>(Vec(130.f, yBottomRow), module, UZZ::XPOSE_INPUT));

        addOutput(createOutputCentered<UzzOutputPort>(Vec(690.f, yTopRow),    module, UZZ::M1_OUTPUT));
        addOutput(createOutputCentered<UzzOutputPort>(Vec(690.f, yMiddleRow), module, UZZ::M2_OUTPUT));
        addOutput(createOutputCentered<UzzOutputPort>(Vec(690.f, yBottomRow), module, UZZ::EOC_OUTPUT));

        addOutput(createOutputCentered<UzzOutputPort>(Vec(588.f, yTopRow),    module, UZZ::PITCH_OUTPUT));
        addOutput(createOutputCentered<UzzOutputPort>(Vec(588.f, yMiddleRow), module, UZZ::GATE_OUTPUT));
        addOutput(createOutputCentered<UzzOutputPort>(Vec(588.f, yBottomRow), module, UZZ::STEP_GATES_OUTPUT));//Poly
        addParam(createParamCentered<CKSS>(Vec(539.f, yMiddleRow), module, UZZ::GATE_MODE_PARAM));//Gate / Trigger
        addParam(createParamCentered<Trimpot>(Vec(539.f, yTopRow),    module, UZZ::SLEW_PARAM));
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* m = dynamic_cast<UZZ*>(module);

        menu->addChild(new ui::MenuSeparator());
        menu->addChild(createCheckMenuItem("EOC on reset", "",
            [m]() { return m && m->eocOnReset; },
            [m]() { if (m) m->eocOnReset = !m->eocOnReset; }
        ));

        menu->addChild(createSubmenuItem("Direction mode", "", [m](ui::Menu* sub){
            for (int i = 0; i <= 4; ++i) {
                sub->addChild(createCheckMenuItem(
                    DIR_LABELS[i], "",
                    [m, i]() {
                        if (!m) return false;
                        int cur = (int) std::round(m->params[UZZ::DIR_MODE_PARAM].getValue());
                        return cur == i;
                    },
                    [m, i]() {
                        if (!m) return;
                        m->params[UZZ::DIR_MODE_PARAM].setValue((float)i);
                        m->pingDir  = 0;
                        m->drunkDir = 1;
                    }
                ));
            }
        }));

        // ---- Rangos M1/M2 ----
        menu->addChild(new ui::MenuSeparator());
        menu->addChild(createSubmenuItem("Range Mod 1", "", [m](ui::Menu* sub){
            for (int r = 0; r < UZZRanges::MR_COUNT; ++r) {
                sub->addChild(createCheckMenuItem(
                    UZZRanges::RANGE_DEFS[r].label, "",
                    [m, r]() { return m && m->m1Range == r; },
                    [m, r]() { if (m) m->m1Range = r; }
                ));
            }
        }));
        menu->addChild(createSubmenuItem("Range Mod 2", "", [m](ui::Menu* sub){
            for (int r = 0; r < UZZRanges::MR_COUNT; ++r) {
                sub->addChild(createCheckMenuItem(
                    UZZRanges::RANGE_DEFS[r].label, "",
                    [m, r]() { return m && m->m2Range == r; },
                    [m, r]() { if (m) m->m2Range = r; }
                ));
            }
        }));
    }
};

Model* modelUZZ = createModel<UZZ, UZZWidget>("UZZ");
