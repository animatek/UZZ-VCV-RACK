#include "plugin.hpp"

// ============================================================================
// LAYOUT
// ============================================================================
namespace UI {
static constexpr int COLS = 16;

// — Panel margins —
static constexpr float LEFT = 48.f;
static constexpr float RIGHT = 1.f;
static constexpr float BOTTOM_MARGIN = 28.f;

// — Top section —
static constexpr float Y_STEP_LED = 10.f;
static constexpr float Y_STEP_MODE = 10.f;
static constexpr float Y_NOTE = 30.f;
static constexpr float Y_PROB = 52.f;

// — Knob rows (5 rows, 48px apart) —
static constexpr float ROW_START = 72.f;
static constexpr float ROW_SPACE = 48.f;
static constexpr float Y_PITCH = ROW_START;
static constexpr float Y_OCT = ROW_START + ROW_SPACE;
static constexpr float Y_DUR = ROW_START + ROW_SPACE * 2;
static constexpr float Y_C1 = ROW_START + ROW_SPACE * 3;
static constexpr float Y_C2 = ROW_START + ROW_SPACE * 4;

// — Left-side: randomize buttons —
static constexpr float RAND_X = LEFT - 10.f;
static constexpr float RAND_BTN_SCALE = 0.90f;
static constexpr float RAND_BTN_X_OFFSET =
    1.f; // shift rand buttons right (was -3)

// — Left-side: shift buttons —
static constexpr float SHIFT_X = RAND_X - 18.f;
static constexpr float SHIFT_X_OFFSET = 19.f; // shift arrows right (was 15)
static constexpr float SHIFT_Y_DELTA = 14.f;
static constexpr float SHIFT_Y_FINE = -1.f;
static constexpr float ROW_SHIFT_SCALE = 0.85f;

// — Trigger inputs / ports —
static constexpr float TRIG_LEFT_GAP = 23.f;
static constexpr float TRIG_RIGHT_PAD = 14.f;
static constexpr float PORT_SCALE = 0.90f;

// — Bottom panel —
// X positions aligned to step-column centers (user-facing 1-indexed step).
//   X_IN     = step 2 (idx 1)
//   X_CTRL1  = step 4 (idx 3)
//   X_CTRL2  = step 7 (idx 6)
//   X_SWITCH = step 10 (idx 9)
//   X_OUT1   = step 13 (idx 12)
//   X_OUT2   = step 15 (idx 14)
// Values = LEFT + (idx + 0.5) * ((WIDTH - LEFT - RIGHT) / COLS), with
// WIDTH=870, LEFT=48, RIGHT=1, COLS=16 → colW = 51.3125.
// 3 rows, equal 30 px spacing
static constexpr float BOT_DY_TOP = -54.f; // y = 298
static constexpr float BOT_DY_MID = -24.f; // y = 328
static constexpr float BOT_DY_BOT = 6.f;   // y = 358
static constexpr float X_IN = 124.96875f;
static constexpr float X_CTRL1 = 227.59375f;
static constexpr float X_CTRL2 = 381.53125f;  // step 7 (idx 6)
static constexpr float X_SWITCH = 586.78125f; // step 11 (idx 10)
static constexpr float X_OUT1 = 689.40625f;   // step 13 (idx 12)
static constexpr float X_OUT2 = 792.03125f;   // step 15 (idx 14)

// — Helpers —
inline float trigLeftX() { return RAND_X - TRIG_LEFT_GAP; }
inline float rowShiftX() { return SHIFT_X + SHIFT_X_OFFSET; }
inline float rowShiftYUp(float cy) { return cy - SHIFT_Y_DELTA + SHIFT_Y_FINE; }
inline float rowShiftYDown(float cy) {
  return cy + SHIFT_Y_DELTA - SHIFT_Y_FINE;
}
inline float randButtonX() { return RAND_X + RAND_BTN_X_OFFSET; }
inline float usable(float boxW) { return boxW - LEFT - RIGHT; }
inline float colW(float boxW) { return usable(boxW) / float(COLS); }
inline float colCenter(float boxW, int i) {
  return LEFT + (i + 0.5f) * colW(boxW);
}
inline float trigX(float boxW) {
  return colCenter(boxW, 15) + colW(boxW) * 0.5f + TRIG_RIGHT_PAD;
}
} // namespace UI

template <typename F>
static inline void drawScaled(NVGcontext *vg, Vec boxSize, float scale, F fn) {
  nvgSave(vg);
  Vec c = boxSize.mult(0.5f);
  nvgTranslate(vg, c.x * (1.f - scale), c.y * (1.f - scale));
  nvgScale(vg, scale, scale);
  fn();
  nvgRestore(vg);
}

// ============================================================================
// Helpers
// ============================================================================
static inline int wrap16(int x) { return x & 15; }

enum StepMode {
  SM_PLAY = 0,
  SM_MUTE = 1,
  SM_SKIP = 2,
  SM_ACCUM_UP = 3,
  SM_ACCUM_DOWN = 4
};

static constexpr int NUM_RND_BANKS = 7;
static constexpr int NUM_SHIFT_ROWS = 6;
static constexpr float TRIG_LEN = 0.010f;

static std::shared_ptr<window::Svg> loadPluginSvg(const char *relPath) {
  std::string path = asset::plugin(pluginInstance, relPath);
  return system::exists(path) ? Svg::load(path) : nullptr;
}

// ============================================================================
// CLOCK RATIO
// ============================================================================
static constexpr float RATIO_TABLE[] = {
    1.f / 48.f, 1.f / 32.f, 1.f / 24.f, 1.f / 16.f, 1.f / 12.f, 1.f / 10.f,
    1.f / 8.f,  1.f / 6.f,  1.f / 5.f,  1.f / 4.f,  1.f / 3.f,  1.f / 2.5f,
    1.f / 2.f,  1.f / 1.5f, 1.f,        1.5f,       2.f,        2.5f,
    3.f,        4.f,        5.f,        6.f,        8.f,        10.f,
    12.f,       16.f,       24.f,       32.f,       48.f};
static constexpr int NUM_RATIOS =
    (int)(sizeof(RATIO_TABLE) / sizeof(RATIO_TABLE[0]));
static constexpr int RATIO_DEFAULT_INDEX = 14;

static constexpr const char *RATIO_LABELS[NUM_RATIOS] = {
    "÷48", "÷32",  "÷24", "÷16",  "÷12", "÷10",  "÷8",  "÷6",   "÷5", "÷4",
    "÷3",  "÷2.5", "÷2",  "÷1.5", "×1",  "×1.5", "×2",  "×2.5", "×3", "×4",
    "×5",  "×6",   "×8",  "×10",  "×12", "×16",  "×24", "×32",  "×48"};

struct RatioQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    int idx = clamp((int)std::round(getValue()), 0, NUM_RATIOS - 1);
    return RATIO_LABELS[idx];
  }
  std::string getUnit() override { return ""; }
};

// ============================================================================
// Direction
// ============================================================================
enum DirectionMode {
  DIR_PINGPONG = -2,
  DIR_REV = -1,
  DIR_FWD = 0,
  DIR_RANDOM = 1,
  DIR_DRUNK = 2
};

static const char *dirLabel(int v) {
  switch (clamp(v, -2, 2)) {
  case -2:
    return "Ping-Pong";
  case -1:
    return "Backward";
  case 0:
    return "Forward";
  case 1:
    return "Random";
  default:
    return "Drunk";
  }
}

struct DirModeQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    return dirLabel((int)std::round(getValue()));
  }
  std::string getUnit() override { return ""; }
};

// ============================================================================
// Mod Ranges
// ============================================================================
namespace UZZRanges {
enum ModRange {
  MR_PM10,
  MR_PM5,
  MR_PM3,
  MR_PM2,
  MR_PM1,
  MR_0_10,
  MR_0_5,
  MR_0_3,
  MR_0_2,
  MR_0_1,
  MR_COUNT
};
struct RangeDef {
  const char *label;
  float minV;
  float maxV;
};

static const RangeDef RANGE_DEFS[MR_COUNT] = {
    {"+/-10V", -10.f, 10.f}, {"+/-5V", -5.f, 5.f}, {"+/-3V", -3.f, 3.f},
    {"+/-2V", -2.f, 2.f},    {"+/-1V", -1.f, 1.f}, {"0V-10V", 0.f, 10.f},
    {"0V-5V", 0.f, 5.f},     {"0V-3V", 0.f, 3.f},  {"0V-2V", 0.f, 2.f},
    {"0V-1V", 0.f, 1.f},
};

inline float mapMod0_10ToRange(float raw0_10, int r) {
  const RangeDef &d = RANGE_DEFS[clamp(r, 0, MR_COUNT - 1)];
  float t = clamp(raw0_10, 0.f, 10.f) / 10.f;
  return d.minV + (d.maxV - d.minV) * t;
}
} // namespace UZZRanges

// ============================================================================
// Clock Processor
// ============================================================================
struct ClockProcessor {
  float timeSinceClk = 0.f;
  float lastPeriod = 0.f;
  float virtTimer = 0.f;
  float virtPeriod = 0.125f;
  float sinceLastEdge = 0.f;
  float sinceLastTick = 1e9f;

  bool clockWasConnected = false;
  bool havePhase = false;
  bool prevClkConnected = false;

  int swingPhase = 0;
  int queuedBaseTicks = 0;
  bool tickPending = false;
  float pendingDelay = 0.f;
  float pendingTimer = 0.f;

  dsp::SchmittTrigger clkTrig;

  void reset() {
    timeSinceClk = 0.f;
    lastPeriod = 0.f;
    virtTimer = 0.f;
    virtPeriod = 0.125f;
    clockWasConnected = false;
    havePhase = false;
    sinceLastEdge = 0.f;
    prevClkConnected = false;
    swingPhase = 0;
    queuedBaseTicks = 0;
    tickPending = false;
    pendingDelay = 0.f;
    pendingTimer = 0.f;
    sinceLastTick = 1e9f;
    clkTrig.reset();
  }

  void onDisconnect() {
    timeSinceClk = 0.f;
    lastPeriod = 0.f;
    virtTimer = 0.f;
    havePhase = false;
    sinceLastEdge = 0.f;
    clockWasConnected = false;
    queuedBaseTicks = 0;
    tickPending = false;
    pendingDelay = 0.f;
    pendingTimer = 0.f;
  }

  float getVirtPeriod() const { return virtPeriod; }

  bool process(const rack::engine::Module::ProcessArgs &args, Input &clkInput,
               float ratio, float swing, bool isConnected) {
    if (isConnected) {
      timeSinceClk += args.sampleTime;
      sinceLastEdge += args.sampleTime;
    } else {
      timeSinceClk = 0.f;
    }
    sinceLastTick += args.sampleTime;

    bool needsVirtualClock = std::fabs(ratio - 1.f) > 1e-6f;
    if (!needsVirtualClock)
      havePhase = false;

    bool extPulse = clkTrig.process(clkInput.getVoltage());
    if (extPulse) {
      lastPeriod = timeSinceClk;
      timeSinceClk = 0.f;
      sinceLastEdge = 0.f;

      if (lastPeriod > 1e-4f)
        virtPeriod = lastPeriod / std::max(ratio, 1e-6f);

      bool isIntMultiplier =
          (ratio >= 1.f) && (std::fabs(ratio - std::round(ratio)) < 1e-4f);

      if (ratio >= 1.f) {
        virtTimer = 0.f;
        if (isIntMultiplier)
          queuedBaseTicks++;
      }

      clockWasConnected = true;
      havePhase = needsVirtualClock;
    }

    float timeout = 0.5f;
    if (lastPeriod > 1e-4f)
      timeout = clamp(lastPeriod * 2.f, 0.1f, 1.0f);

    if (havePhase && sinceLastEdge > timeout) {
      havePhase = false;
      virtTimer = 0.f;
    }

    if (needsVirtualClock && havePhase && virtPeriod > 0.f) {
      virtTimer += args.sampleTime;
      while (virtTimer >= virtPeriod) {
        virtTimer -= virtPeriod;
        queuedBaseTicks++;
      }
    }

    bool clockNow = false;

    if (tickPending) {
      pendingTimer += args.sampleTime;
      if (pendingTimer >= pendingDelay) {
        tickPending = false;
        pendingTimer = 0.f;
        clockNow = true;
        swingPhase++;
        sinceLastTick = 0.f;
      }
    }

    if (!tickPending && !clockNow && queuedBaseTicks > 0) {
      float s = (1.f / 3.f) * swing;
      bool isOdd = (swingPhase & 1) == 1;
      pendingDelay = isOdd ? (s * virtPeriod) : 0.f;

      if (pendingDelay <= 1e-9f) {
        if (sinceLastTick < 0.0005f) {
          queuedBaseTicks--;
        } else {
          queuedBaseTicks--;
          clockNow = true;
          swingPhase++;
          sinceLastTick = 0.f;
        }
      } else {
        tickPending = true;
        pendingTimer = 0.f;
        queuedBaseTicks--;
      }
    }

    prevClkConnected = isConnected;
    return clockNow;
  }
};

// ============================================================================
// Step Navigator
// ============================================================================
struct StepNavigator {
  int pingDir = 0;
  int drunkDir = 1;

  void reset() {
    pingDir = 0;
    drunkDir = 1;
  }

  template <typename GetStepModeFn>
  int findNextPlayable(int start, int len, int currentRel, int direction,
                       GetStepModeFn getStepMode, bool &allSkip) {
    allSkip = true;
    int rel = currentRel;

    for (int tries = 0; tries < len; ++tries) {
      rel = (direction > 0) ? (rel + 1) % len : (rel - 1 + len) % len;
      int cand = wrap16(start + rel);

      if ((int)std::round(getStepMode(cand)) != SM_SKIP) {
        allSkip = false;
        return cand;
      }
    }

    int relTheo =
        (direction > 0) ? (currentRel + 1) % len : (currentRel - 1 + len) % len;
    return wrap16(start + relTheo);
  }

  template <typename GetStepModeFn>
  int getNextStep(int currentStep, int start, int steps, int dirMode,
                  GetStepModeFn getStepMode, bool playCurrentStep,
                  bool &wrapped, bool &allSkip) {

    int relBefore = (currentStep - start + 16) & 15;
    int nextStep = currentStep;
    wrapped = false;
    allSkip = false;

    if (playCurrentStep) {
      allSkip = ((int)std::round(getStepMode(currentStep)) == SM_SKIP);
      return currentStep;
    }

    if (dirMode == DIR_FWD || dirMode == DIR_REV) {
      int direction = (dirMode == DIR_FWD) ? 1 : -1;
      nextStep = findNextPlayable(start, steps, relBefore, direction,
                                  getStepMode, allSkip);

      int relAfter = (nextStep - start + 16) & 15;
      wrapped =
          (direction > 0) ? (relAfter < relBefore) : (relAfter > relBefore);
    } else if (dirMode == DIR_PINGPONG) {
      int direction = (pingDir == 0) ? 1 : -1;
      bool all1 = false;
      int cand1 = findNextPlayable(start, steps, relBefore, direction,
                                   getStepMode, all1);
      int relAfter1 = (cand1 - start + 16) & 15;
      bool wouldWrap =
          (direction > 0) ? (relAfter1 < relBefore) : (relAfter1 > relBefore);

      if (!all1 && !wouldWrap) {
        nextStep = cand1;
        wrapped = false;
      } else {
        pingDir = 1 - pingDir;
        int direction2 = (pingDir == 0) ? 1 : -1;
        bool all2 = false;
        int cand2 = findNextPlayable(start, steps, relBefore, direction2,
                                     getStepMode, all2);
        nextStep = cand2;
        wrapped = true;
      }
    }
    // Random (no heap allocation)
    else if (dirMode == DIR_RANDOM) {
      int pool[16];
      int poolSize = 0;

      for (int k = 0; k < steps; ++k) {
        int sIdx = wrap16(start + k);
        int m = (int)std::round(getStepMode(sIdx));
        if (m != SM_SKIP)
          pool[poolSize++] = sIdx;
      }

      if (poolSize > 0) {
        int idx = (int)std::floor(random::uniform() * poolSize);
        idx = clamp(idx, 0, poolSize - 1);
        nextStep = pool[idx];
      } else {
        bool dummy = false;
        nextStep =
            findNextPlayable(start, steps, relBefore, 1, getStepMode, dummy);
      }
      wrapped = false;
    }
    // Drunk (random walk)
    else {
      drunkDir = (random::uniform() < 0.5f) ? -1 : 1;

      bool allA = false;
      int candA = findNextPlayable(start, steps, relBefore, drunkDir,
                                   getStepMode, allA);

      if (!allA) {
        nextStep = candA;
      } else {
        bool allB = false;
        int candB = findNextPlayable(start, steps, relBefore, -drunkDir,
                                     getStepMode, allB);
        nextStep = candB;
      }

      int relAfter = (nextStep - start + 16) & 15;
      wrapped = (relBefore == 0 && relAfter == (steps - 1)) ||
                (relBefore == (steps - 1) && relAfter == 0);
    }

    return nextStep;
  }
};

// ============================================================================
// Module
// ============================================================================
struct UZZ : Module {
  enum ParamIds {
    ENUMS(PITCH_PARAMS, 16),
    ENUMS(OCT_PARAMS, 16),
    ENUMS(STEP_MODE_PARAMS, 16),
    ENUMS(DUR_PARAMS, 16),
    ENUMS(M1_PARAMS, 16),
    ENUMS(M2_PARAMS, 16),

    STEPS_PARAM,
    START_PARAM,
    DIR_MODE_PARAM,
    GATE_MODE_PARAM,
    RATIO_IDX_PARAM,

    SWING_PARAM,
    SLEW_PARAM,
    ACCUM_AMT_PARAM,

    RND_PITCH_PARAM,
    RND_OCTAVE_PARAM,
    RND_STEP_PARAM,
    RND_DUR_PARAM,
    RND_M1_PARAM,
    RND_M2_PARAM,

    PITCH_SHIFT_DOWN_PARAM,
    PITCH_SHIFT_UP_PARAM,
    OCT_SHIFT_DOWN_PARAM,
    OCT_SHIFT_UP_PARAM,
    DUR_SHIFT_DOWN_PARAM,
    DUR_SHIFT_UP_PARAM,
    M1_SHIFT_DOWN_PARAM,
    M1_SHIFT_UP_PARAM,
    M2_SHIFT_DOWN_PARAM,
    M2_SHIFT_UP_PARAM,

    // New params (appended to preserve patch compatibility)
    ENUMS(PROB_PARAMS, 16),
    PROB_GLOBAL_PARAM,
    RND_PROB_PARAM,
    PROB_SHIFT_DOWN_PARAM,
    PROB_SHIFT_UP_PARAM,
    ACCUM_CLIP_PARAM,

    NUM_PARAMS
  };

  enum InputIds {
    CLK_INPUT,
    RESET_INPUT,
    RND_PITCH_TRIG_INPUT,
    RND_OCT_TRIG_INPUT,
    RND_STEP_TRIG_INPUT,
    RND_DUR_TRIG_INPUT,
    RND_M1_TRIG_INPUT,
    RND_M2_TRIG_INPUT,
    XPOSE_INPUT,
    RND_PROB_TRIG_INPUT,
    NUM_INPUTS
  };

  enum OutputIds {
    PITCH_OUTPUT,
    GATE_OUTPUT,
    STEP_GATES_OUTPUT,
    EOC_OUTPUT,
    M1_OUTPUT,
    M2_OUTPUT,
    NUM_OUTPUTS
  };

  enum LightIds {
    ENUMS(STEP_LIGHTS, 16),
    RND_LIGHT,
    RND_OCT_LIGHT,
    RND_STEP_LIGHT,
    RND_DUR_LIGHT,
    RND_M1_LIGHT,
    RND_M2_LIGHT,
    RND_PROB_LIGHT, // appended
    NUM_LIGHTS
  };

  // State
  int step = 0;
  float pitchOut = 0.f;
  bool pitchInit = false;

  // Cached slew coefficients — recomputed only when slewSec / sampleTime change
  float cachedSlewSec = -1.f;
  float cachedSampleTime = -1.f;
  float cachedSlewAlpha = 1.f;

  ClockProcessor clock;
  StepNavigator navigator;
  dsp::SchmittTrigger rstTrig;
  dsp::PulseGenerator gatePulse, eocPulse, stepGateTrig[16];
  dsp::ClockDivider lightDivider;

  dsp::BooleanTrigger rndBtnTrig[NUM_RND_BANKS];
  dsp::SchmittTrigger rndCvTrig[NUM_RND_BANKS];
  dsp::BooleanTrigger shiftUpTrig[NUM_SHIFT_ROWS],
      shiftDownTrig[NUM_SHIFT_ROWS];
  bool skipNextRandom[NUM_RND_BANKS] = {};

  bool playCurrentOnNextTick = false;
  bool resetPending = false;
  int resetTargetStep = 0;
  bool eocOnReset = false;

  int accumOffset[16] = {};

  int m1Range = UZZRanges::MR_0_10;
  int m2Range = UZZRanges::MR_0_10;
  int pitchRangeSemis = 11; // 11 = 1 octave, 23 = 2 octaves

  float capiFlash = 0.f;
  dsp::SchmittTrigger capiTrig;

  UZZ() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    lightDivider.setDivision(512);

    for (int i = 0; i < 16; ++i) {
      configParam(PITCH_PARAMS + i, 0.f, 11.f, 0.f,
                  string::f("Pitch %d", i + 1), " semitones");
      paramQuantities[PITCH_PARAMS + i]->snapEnabled = true;

      configParam(OCT_PARAMS + i, -2.f, 2.f, 0.f, string::f("Octave %d", i + 1),
                  " oct");
      paramQuantities[OCT_PARAMS + i]->snapEnabled = true;

      configParam(STEP_MODE_PARAMS + i, 0.f, 4.f, 0.f,
                  string::f("Step mode %d", i + 1));
      paramQuantities[STEP_MODE_PARAMS + i]->snapEnabled = true;

      configParam(DUR_PARAMS + i, 0.005f, 2.0f, 0.100f,
                  string::f("Duration %d", i + 1), " s");

      configParam(M1_PARAMS + i, 0.f, 10.f, 0.f, string::f("Mod 1 %d", i + 1),
                  " (0..10)");
      configParam(M2_PARAMS + i, 0.f, 10.f, 0.f, string::f("Mod 2 %d", i + 1),
                  " (0..10)");

      configParam(PROB_PARAMS + i, 0.f, 100.f, 100.f,
                  string::f("Probability %d", i + 1), " %");
      paramQuantities[PROB_PARAMS + i]->snapEnabled = true;
    }

    configParam(PROB_GLOBAL_PARAM, 0.f, 100.f, 100.f, "Global probability",
                " %");
    paramQuantities[PROB_GLOBAL_PARAM]->snapEnabled = true;

    configParam(STEPS_PARAM, 1.f, 16.f, 16.f, "Steps");
    paramQuantities[STEPS_PARAM]->snapEnabled = true;
    configParam(START_PARAM, 1.f, 16.f, 1.f, "Start");
    paramQuantities[START_PARAM]->snapEnabled = true;

    configParam<DirModeQuantity>(DIR_MODE_PARAM, -2.f, 2.f, 0.f,
                                 "Direction mode");
    paramQuantities[DIR_MODE_PARAM]->snapEnabled = true;

    configParam(GATE_MODE_PARAM, 0.f, 1.f, 0.f, "Gate mode (0=Gate,1=Trig)");
    paramQuantities[GATE_MODE_PARAM]->snapEnabled = true;

    configParam<RatioQuantity>(RATIO_IDX_PARAM, 0.f, (float)(NUM_RATIOS - 1),
                               (float)RATIO_DEFAULT_INDEX, "Clock ratio");
    paramQuantities[RATIO_IDX_PARAM]->snapEnabled = true;

    configParam(SWING_PARAM, 0.f, 0.6f, 0.f, "Swing", "%", 0.f, 100.f, 0.f);
    configParam(SLEW_PARAM, 0.f, 2.0f, 0.f, "Glide (slew)", " s");
    configParam(ACCUM_AMT_PARAM, 0.f, 24.f, 1.f, "Accumulator amount", " st");
    paramQuantities[ACCUM_AMT_PARAM]->snapEnabled = true;
    configParam(ACCUM_CLIP_PARAM, 0.f, 12.f, 0.f, "Accumulator wrap (semitones, 0=OFF = full ±12 range)");
    paramQuantities[ACCUM_CLIP_PARAM]->snapEnabled = true;

    configParam(RND_PITCH_PARAM, 0.f, 1.f, 0.f, "Randomize pitch");
    configParam(RND_OCTAVE_PARAM, 0.f, 1.f, 0.f, "Randomize octave");
    configParam(RND_STEP_PARAM, 0.f, 1.f, 0.f, "Randomize step-mode");
    configParam(RND_DUR_PARAM, 0.f, 1.f, 0.f, "Randomize duration");
    configParam(RND_M1_PARAM, 0.f, 1.f, 0.f, "Randomize mod1");
    configParam(RND_M2_PARAM, 0.f, 1.f, 0.f, "Randomize mod2");
    configParam(RND_PROB_PARAM, 0.f, 1.f, 0.f, "Randomize probability");

    configButton(PITCH_SHIFT_DOWN_PARAM, "Shift pitch row down");
    configButton(PITCH_SHIFT_UP_PARAM, "Shift pitch row up");
    configButton(OCT_SHIFT_DOWN_PARAM, "Shift octave row down");
    configButton(OCT_SHIFT_UP_PARAM, "Shift octave row up");
    configButton(DUR_SHIFT_DOWN_PARAM, "Shift duration row down");
    configButton(DUR_SHIFT_UP_PARAM, "Shift duration row up");
    configButton(M1_SHIFT_DOWN_PARAM, "Shift mod1 row down");
    configButton(M1_SHIFT_UP_PARAM, "Shift mod1 row up");
    configButton(M2_SHIFT_DOWN_PARAM, "Shift mod2 row down");
    configButton(M2_SHIFT_UP_PARAM, "Shift mod2 row up");
    configButton(PROB_SHIFT_DOWN_PARAM, "Shift probability row down");
    configButton(PROB_SHIFT_UP_PARAM, "Shift probability row up");

    configInput(CLK_INPUT, "Clock");
    configInput(RESET_INPUT, "Reset");
    configInput(RND_PITCH_TRIG_INPUT, "Randomize pitch (trig)");
    configInput(RND_OCT_TRIG_INPUT, "Randomize octave (trig)");
    configInput(RND_STEP_TRIG_INPUT, "Randomize step-mode (trig)");
    configInput(RND_DUR_TRIG_INPUT, "Randomize duration (trig)");
    configInput(RND_M1_TRIG_INPUT, "Randomize mod1 (trig)");
    configInput(RND_M2_TRIG_INPUT, "Randomize mod2 (trig)");
    configInput(RND_PROB_TRIG_INPUT, "Randomize probability (trig)");
    configInput(XPOSE_INPUT, "Transpose (1V/oct)");

    configOutput(PITCH_OUTPUT, "Pitch (1V/oct)");
    configOutput(GATE_OUTPUT, "Gate/Trig");
    configOutput(STEP_GATES_OUTPUT, "Step gates (poly)");
    configOutput(EOC_OUTPUT, "End of cycle");
    configOutput(M1_OUTPUT, "Mod 1");
    configOutput(M2_OUTPUT, "Mod 2");
  }

  void onReset() override {
    int start =
        clamp((int)std::round(params[START_PARAM].getValue()) - 1, 0, 15);
    step = start;

    gatePulse.reset();
    eocPulse.reset();
    for (int i = 0; i < 16; ++i)
      stepGateTrig[i].reset();
    for (int i = 0; i < 16; ++i)
      lights[STEP_LIGHTS + i].setBrightness(0.f);

    clock.reset();
    navigator.reset();

    pitchOut = 0.f;
    pitchInit = false;

    playCurrentOnNextTick = false;
    resetPending = false;
    resetTargetStep = start;

    for (int i = 0; i < 16; ++i)
      accumOffset[i] = 0;
  }

  void setPitchRange(int maxSemis, bool scaleValues = true) {
    int newMax = clamp(maxSemis, 11, 23);
    int oldMax = pitchRangeSemis;
    pitchRangeSemis = newMax;
    for (int i = 0; i < 16; ++i) {
      paramQuantities[PITCH_PARAMS + i]->maxValue = (float)newMax;
      if (scaleValues && newMax != oldMax && oldMax > 0) {
        float v = params[PITCH_PARAMS + i].getValue();
        float scaled = std::round(v * (float)newMax / (float)oldMax);
        scaled = clamp(scaled, 0.f, (float)newMax);
        params[PITCH_PARAMS + i].setValue(scaled);
      }
    }
  }

  void randomizePitch() {
    for (int i = 0; i < 16; ++i)
      params[PITCH_PARAMS + i].setValue(
          (int)std::floor(random::uniform() * (pitchRangeSemis + 1)));
  }
  void randomizeOctaves() {
    for (int i = 0; i < 16; ++i)
      params[OCT_PARAMS + i].setValue((int)std::floor(random::uniform() * 5.f) -
                                      2);
  }
  void randomizeStepMode() {
    for (int i = 0; i < 16; ++i) {
      float r = random::uniform();
      int m = (r < 0.55f)
                  ? SM_PLAY
                  : ((r < 0.75f)
                         ? SM_MUTE
                         : ((r < 0.88f)
                                ? SM_SKIP
                                : ((r < 0.94f) ? SM_ACCUM_UP : SM_ACCUM_DOWN)));
      params[STEP_MODE_PARAMS + i].setValue((float)m);
    }
  }

  void fillRow(int base, float value) {
    for (int i = 0; i < 16; ++i) params[base + i].setValue(value);
  }

  void resetPitchRow()    { fillRow(PITCH_PARAMS,     0.f);    }
  void resetOctaveRow()   { fillRow(OCT_PARAMS,       0.f);    }
  void resetStepModeRow() { fillRow(STEP_MODE_PARAMS, 0.f);    }
  void resetDurRow()      { fillRow(DUR_PARAMS,       0.100f); }
  void resetM1Row()       { fillRow(M1_PARAMS,        0.f);    }
  void resetM2Row()       { fillRow(M2_PARAMS,        0.f);    }
  void resetProbRow()     { fillRow(PROB_PARAMS,      100.f);  }

  void randomizeDurations() {
    for (int i = 0; i < 16; ++i)
      params[DUR_PARAMS + i].setValue(0.005f + random::uniform() * (2.0f - 0.005f));
  }
  void randomizeM1() {
    for (int i = 0; i < 16; ++i)
      params[M1_PARAMS + i].setValue(random::uniform() * 10.f);
  }
  void randomizeM2() {
    for (int i = 0; i < 16; ++i)
      params[M2_PARAMS + i].setValue(random::uniform() * 10.f);
  }
  void randomizeProb() {
    for (int i = 0; i < 16; ++i)
      params[PROB_PARAMS + i].setValue((int)std::floor(random::uniform() * 101.f));
  }

  static float quantize_to_step(float value, float min_val, float step) {
    if (step <= 0.f)
      return value;
    float steps_from_min = std::round((value - min_val) / step);
    return min_val + steps_from_min * step;
  }

  void get_active_window(int &start_idx, int &count) {
    count = clamp((int)std::round(params[STEPS_PARAM].getValue()), 1, 16);
    start_idx =
        clamp((int)std::round(params[START_PARAM].getValue()) - 1, 0, 15);
  }

  void shift_row_int(int base_param, int dir, int start_idx, int count,
                     int min_val, int max_val) {
    if (count <= 0)
      return;
    int step_dir = (dir >= 0) ? 1 : -1;
    for (int i = 0; i < count; ++i) {
      int idx = wrap16(start_idx + i);
      int current = (int)std::round(params[base_param + idx].getValue());
      int next = clamp(current + step_dir, min_val, max_val);
      params[base_param + idx].setValue((float)next);
    }
  }

  void shift_row_float(int base_param, int dir, int start_idx, int count,
                       float step_amount, float min_val, float max_val,
                       bool quantize) {
    if (count <= 0)
      return;
    int step_dir = (dir >= 0) ? 1 : -1;
    for (int i = 0; i < count; ++i) {
      int idx = wrap16(start_idx + i);
      float current = params[base_param + idx].getValue();
      if (quantize)
        current = quantize_to_step(current, min_val, step_amount);
      float next = current + (float)step_dir * step_amount;
      if (quantize)
        next = quantize_to_step(next, min_val, step_amount);
      next = clamp(next, min_val, max_val);
      params[base_param + idx].setValue(next);
    }
  }

  void shift_pitch_row(int dir) {
    int s, c;
    get_active_window(s, c);
    shift_row_int(PITCH_PARAMS, dir, s, c, 0, pitchRangeSemis);
  }
  void shift_oct_row(int dir) {
    int s, c;
    get_active_window(s, c);
    shift_row_int(OCT_PARAMS, dir, s, c, -2, 2);
  }
  void shift_dur_row(int dir) {
    int s, c;
    get_active_window(s, c);
    shift_row_float(DUR_PARAMS, dir, s, c, 0.1f, 0.005f, 2.0f, true);
  }
  void shift_m1_row(int dir) {
    int s, c;
    get_active_window(s, c);
    shift_row_float(M1_PARAMS, dir, s, c, 1.f, 0.f, 10.f, true);
  }
  void shift_m2_row(int dir) {
    int s, c;
    get_active_window(s, c);
    shift_row_float(M2_PARAMS, dir, s, c, 1.f, 0.f, 10.f, true);
  }
  void shift_prob_row(int dir) {
    int s, c;
    get_active_window(s, c);
    shift_row_float(PROB_PARAMS, dir, s, c, 10.f, 0.f, 100.f, true);
  }

  json_t *dataToJson() override {
    json_t *rootJ = json_object();

    json_object_set_new(rootJ, "m1Range", json_integer(m1Range));
    json_object_set_new(rootJ, "m2Range", json_integer(m2Range));
    json_object_set_new(rootJ, "pitchRangeSemis",
                        json_integer(pitchRangeSemis));
    json_object_set_new(rootJ, "eocOnReset", json_boolean(eocOnReset));
    json_object_set_new(rootJ, "currentStep", json_integer(step));
    json_object_set_new(rootJ, "pingDir", json_integer(navigator.pingDir));
    json_object_set_new(rootJ, "drunkDir", json_integer(navigator.drunkDir));

    json_t *accArr = json_array();
    for (int i = 0; i < 16; ++i)
      json_array_append_new(accArr, json_integer(accumOffset[i]));
    json_object_set_new(rootJ, "accumOffset", accArr);

    return rootJ;
  }

  void dataFromJson(json_t *rootJ) override {
    onReset();

    for (int r = 0; r < NUM_RND_BANKS; ++r)
      skipNextRandom[r] = false;
    eocOnReset = false;

    if (!rootJ)
      return;

    if (json_t *j = json_object_get(rootJ, "m1Range"))
      m1Range = clamp((int)json_integer_value(j), 0, UZZRanges::MR_COUNT - 1);
    else
      m1Range = UZZRanges::MR_0_10;

    if (json_t *j = json_object_get(rootJ, "m2Range"))
      m2Range = clamp((int)json_integer_value(j), 0, UZZRanges::MR_COUNT - 1);
    else
      m2Range = UZZRanges::MR_0_10;

    if (json_t *j = json_object_get(rootJ, "eocOnReset"))
      eocOnReset = json_is_true(j);

    if (json_t *j = json_object_get(rootJ, "pitchRangeSemis"))
      setPitchRange((int)json_integer_value(j), false);
    else
      setPitchRange(11, false);

    int steps = clamp((int)std::round(params[STEPS_PARAM].getValue()), 1, 16);
    int start =
        clamp((int)std::round(params[START_PARAM].getValue()) - 1, 0, 15);

    if (json_t *j = json_object_get(rootJ, "currentStep")) {
      int savedStep = clamp((int)json_integer_value(j), 0, 15);
      int rel = (savedStep - start + 16) & 15;
      step = (rel < steps) ? savedStep : start;
    } else {
      step = start;
    }

    if (json_t *j = json_object_get(rootJ, "pingDir"))
      navigator.pingDir = clamp((int)json_integer_value(j), 0, 1);

    if (json_t *j = json_object_get(rootJ, "drunkDir")) {
      int dir = clamp((int)json_integer_value(j), -1, 1);
      navigator.drunkDir = (dir == 0) ? 1 : dir;
    }

    if (json_t *accArr = json_object_get(rootJ, "accumOffset")) {
      if (json_is_array(accArr)) {
        size_t n = json_array_size(accArr);
        for (int i = 0; i < 16 && (size_t)i < n; ++i) {
          int v = (int)json_integer_value(json_array_get(accArr, i));
          accumOffset[i] = clamp(v, -12, 12);
        }
      }
    }
  }

  void applySlew(float target, float sampleTime) {
    float slewSec = params[SLEW_PARAM].getValue();
    if (slewSec <= 1e-6f) {
      pitchOut = target;
      pitchInit = true;
    } else {
      if (slewSec != cachedSlewSec || sampleTime != cachedSampleTime) {
        cachedSlewSec = slewSec;
        cachedSampleTime = sampleTime;
        float tau = std::max(1e-5f, slewSec);
        cachedSlewAlpha =
            std::sqrt(std::min(1.f - std::exp(-sampleTime / tau), 1.f));
      }
      if (!pitchInit) {
        pitchOut = target;
        pitchInit = true;
      }
      pitchOut += (target - pitchOut) * cachedSlewAlpha;
    }
    outputs[PITCH_OUTPUT].setVoltage(pitchOut);
  }

  void hardStop(int steps) {
    gatePulse.reset();
    eocPulse.reset();
    for (int ch = 0; ch < 16; ++ch)
      stepGateTrig[ch].reset();
    clock.reset();

    outputs[GATE_OUTPUT].setVoltage(0.f);
    outputs[STEP_GATES_OUTPUT].setChannels(steps);
    for (int ch = 0; ch < steps; ++ch)
      outputs[STEP_GATES_OUTPUT].setVoltage(0.f, ch);
    outputs[EOC_OUTPUT].setVoltage(0.f);
  }

  void process(const ProcessArgs &args) override {
    bool clkConnected = inputs[CLK_INPUT].isConnected();
    bool updateLights = lightDivider.process();
    float lightDt = args.sampleTime * lightDivider.getDivision();
    int ratioIdx = clamp((int)std::round(params[RATIO_IDX_PARAM].getValue()), 0,
                         NUM_RATIOS - 1);
    float ratio = RATIO_TABLE[ratioIdx];
    float swing = clamp(params[SWING_PARAM].getValue(), 0.f, 0.6f);

    bool clockNow =
        clock.process(args, inputs[CLK_INPUT], ratio, swing, clkConnected);

    if (capiTrig.process(inputs[CLK_INPUT].getVoltage())) {
      capiFlash = 1.0f;
    }
    // Decay flash over time
    if (capiFlash > 0.f) {
      capiFlash = std::max(0.f, capiFlash - 15.f * args.sampleTime);
    }

    if (!clkConnected && clock.prevClkConnected)
      clock.onDisconnect();

    // Random buttons + lights + CV triggers
    typedef void (UZZ::*RndFn)();
    static const RndFn rndFns[] = {
        &UZZ::randomizePitch,    &UZZ::randomizeOctaves,
        &UZZ::randomizeStepMode, &UZZ::randomizeDurations,
        &UZZ::randomizeM1,       &UZZ::randomizeM2,
        &UZZ::randomizeProb};
    static const int rndParamIds[] = {
        RND_PITCH_PARAM, RND_OCTAVE_PARAM, RND_STEP_PARAM, RND_DUR_PARAM,
        RND_M1_PARAM,    RND_M2_PARAM,     RND_PROB_PARAM};
    static const int rndInputIds[] = {RND_PITCH_TRIG_INPUT, RND_OCT_TRIG_INPUT,
                                      RND_STEP_TRIG_INPUT,  RND_DUR_TRIG_INPUT,
                                      RND_M1_TRIG_INPUT,    RND_M2_TRIG_INPUT,
                                      RND_PROB_TRIG_INPUT};
    for (int r = 0; r < NUM_RND_BANKS; ++r) {
      if (rndBtnTrig[r].process(params[rndParamIds[r]].getValue() > .5f)) {
        if (!skipNextRandom[r])
          (this->*rndFns[r])();
        skipNextRandom[r] = false;
      }
      if (updateLights)
        lights[RND_LIGHT + r].setSmoothBrightness(
            params[rndParamIds[r]].getValue(), lightDt);
      if (rndCvTrig[r].process(inputs[rndInputIds[r]].getVoltage()))
        (this->*rndFns[r])();
    }

    // Shift buttons
    typedef void (UZZ::*ShiftFn)(int);
    static const ShiftFn shiftFns[] = {
        &UZZ::shift_pitch_row, &UZZ::shift_oct_row, &UZZ::shift_dur_row,
        &UZZ::shift_m1_row,    &UZZ::shift_m2_row,  &UZZ::shift_prob_row};
    static const int shiftDownIds[] = {
        PITCH_SHIFT_DOWN_PARAM, OCT_SHIFT_DOWN_PARAM, DUR_SHIFT_DOWN_PARAM,
        M1_SHIFT_DOWN_PARAM,    M2_SHIFT_DOWN_PARAM,  PROB_SHIFT_DOWN_PARAM};
    static const int shiftUpIds[] = {PITCH_SHIFT_UP_PARAM, OCT_SHIFT_UP_PARAM,
                                     DUR_SHIFT_UP_PARAM,   M1_SHIFT_UP_PARAM,
                                     M2_SHIFT_UP_PARAM,    PROB_SHIFT_UP_PARAM};
    for (int r = 0; r < NUM_SHIFT_ROWS; ++r) {
      if (shiftUpTrig[r].process(params[shiftUpIds[r]].getValue() > 0.5f))
        (this->*shiftFns[r])(+1);
      if (shiftDownTrig[r].process(params[shiftDownIds[r]].getValue() > 0.5f))
        (this->*shiftFns[r])(-1);
    }

    // Window
    int steps = clamp((int)std::round(params[STEPS_PARAM].getValue()), 1, 16);
    int start =
        clamp((int)std::round(params[START_PARAM].getValue()) - 1, 0, 15);

    int rel = (step - start + 16) & 15;
    if (rel >= steps) {
      rel = rel % steps;
      step = wrap16(start + rel);
    }

    // Reset
    if (rstTrig.process(inputs[RESET_INPUT].getVoltage())) {
      resetPending = true;
      resetTargetStep = start;
      playCurrentOnNextTick = true;

      for (int i = 0; i < 16; ++i)
        accumOffset[i] = 0;

      gatePulse.reset();
      eocPulse.reset();
      for (int i = 0; i < 16; ++i)
        stepGateTrig[i].reset();

      if (eocOnReset)
        eocPulse.trigger(TRIG_LEN);

      navigator.reset();
      clock.swingPhase = 0;
      clock.queuedBaseTicks = 0;
      clock.tickPending = false;
      clock.pendingDelay = 0.f;
      clock.pendingTimer = 0.f;
      clock.virtTimer = 0.f;
    }

    // Transpose
    int xposeSemis = 0;
    if (inputs[XPOSE_INPUT].isConnected()) {
      float v = inputs[XPOSE_INPUT].getVoltage();
      if (std::isfinite(v))
        xposeSemis = clamp((int)std::round(v * 12.f), -48, 48);
    }

    // Pitch (pre-slew)
    float semis = params[PITCH_PARAMS + step].getValue();
    int octIv = (int)std::round(params[OCT_PARAMS + step].getValue());
    int accum = accumOffset[step];
    float pitchV =
        ((semis + (float)xposeSemis + (float)accum) / 12.f) + (float)octIv;

    // Mod outputs (always active)
    outputs[M1_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(
        params[M1_PARAMS + step].getValue(), m1Range));
    outputs[M2_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(
        params[M2_PARAMS + step].getValue(), m2Range));

    if (!clkConnected) {
      if (clock.prevClkConnected) {
        hardStop(steps);
        clock.prevClkConnected = false;
      }
      applySlew(pitchV, args.sampleTime);
      if (updateLights) {
        for (int i = 0; i < 16; ++i)
          lights[STEP_LIGHTS + i].setSmoothBrightness(i == step ? 1.f : 0.f,
                                                      lightDt);
      }
      return;
    }

    // Clock tick
    if (clockNow) {
      int modeDir =
          clamp((int)std::round(params[DIR_MODE_PARAM].getValue()), -2, 2);

      bool allSkip = false;
      bool wrapped = false;
      int nextStep = navigator.getNextStep(
          step, start, steps, modeDir,
          [this](int idx) { return params[STEP_MODE_PARAMS + idx].getValue(); },
          playCurrentOnNextTick, wrapped, allSkip);

      playCurrentOnNextTick = false;
      int prevStep = step;
      int prevMode =
          (int)std::round(params[STEP_MODE_PARAMS + prevStep].getValue());
      step = nextStep;
      if (wrapped)
        eocPulse.trigger(TRIG_LEN);

      if (prevMode == SM_ACCUM_UP || prevMode == SM_ACCUM_DOWN) {
        int amt  = (int)std::round(params[ACCUM_AMT_PARAM].getValue());
        int wrap = (int)std::round(params[ACCUM_CLIP_PARAM].getValue());
        int signedAmt = (prevMode == SM_ACCUM_UP) ? amt : -amt;
        int v = accumOffset[prevStep] + signedAmt;
        if (wrap > 0) {
          // Módulo sobre el rango: cicla 0..wrap-1 (ACCUM_UP) o 0..-wrap+1 (ACCUM_DOWN)
          v = v % wrap;
        } else {
          // Sin wrap configurado: rango completo ±12
          static constexpr int ACCUM_RANGE = 25; // -12..+12 inclusive
          v = ((v + 12) % ACCUM_RANGE + ACCUM_RANGE) % ACCUM_RANGE - 12;
        }
        accumOffset[prevStep] = v;
      }

      bool muteGlobal = false;
      if (modeDir == DIR_FWD || modeDir == DIR_REV) {
        muteGlobal = allSkip;
      } else {
        bool anyPlayable = false;
        for (int k = 0; k < steps; ++k) {
          int sIdx = wrap16(start + k);
          if ((int)std::round(params[STEP_MODE_PARAMS + sIdx].getValue()) !=
              SM_SKIP) {
            anyPlayable = true;
            break;
          }
        }
        muteGlobal = !anyPlayable;
      }

      int mode = (int)std::round(params[STEP_MODE_PARAMS + step].getValue());
      int k = (step - start + 16) & 15;

      bool resetFiresAfterGate = resetPending;
      bool playing = !muteGlobal && (mode == SM_PLAY || mode == SM_ACCUM_UP ||
                                     mode == SM_ACCUM_DOWN);
      if (playing) {
        float pStep =
            clamp(params[PROB_PARAMS + step].getValue(), 0.f, 100.f) / 100.f;
        float pGlobal =
            clamp(params[PROB_GLOBAL_PARAM].getValue(), 0.f, 100.f) / 100.f;
        float pFinal = pStep * pGlobal;
        if (pFinal < 1.f && random::uniform() >= pFinal)
          playing = false;
      }
      if (playing) {
        int gateMode = (int)std::round(params[GATE_MODE_PARAM].getValue());
        float userDur =
            clamp(params[DUR_PARAMS + step].getValue(), 0.001f, 10.0f);
        float gLen = (gateMode == 0) ? userDur : TRIG_LEN;

        if (gateMode == 0 && ratio > 1.f && clock.getVirtPeriod() > 0.f) {
          float maxDuty = clock.getVirtPeriod() * 0.90f;
          if (gLen > maxDuty)
            gLen = maxDuty;

          float minOff = std::max(0.001f, 2.f * args.sampleTime);
          float maxLen = clock.getVirtPeriod() - minOff;
          if (gLen > maxLen)
            gLen = std::max(TRIG_LEN, maxLen);
        }

        gatePulse.trigger(gLen);
        stepGateTrig[k].trigger(gLen);
      } else {
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

    // Gate output
    outputs[GATE_OUTPUT].setVoltage(gatePulse.process(args.sampleTime) ? 10.f
                                                                       : 0.f);

    // Poly step gates
    outputs[STEP_GATES_OUTPUT].setChannels(steps);
    for (int ch = 0; ch < steps; ++ch)
      outputs[STEP_GATES_OUTPUT].setVoltage(
          stepGateTrig[ch].process(args.sampleTime) ? 10.f : 0.f, ch);

    // EOC
    outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f
                                                                     : 0.f);

    applySlew(pitchV, args.sampleTime);

    if (updateLights) {
      for (int i = 0; i < 16; ++i)
        lights[STEP_LIGHTS + i].setSmoothBrightness(i == step ? 1.f : 0.f,
                                                    lightDt);
    }

    clock.prevClkConnected = true;
  }
};

// ============================================================================
// Widgets
// ============================================================================

// Generic Random Button Template
template <void (UZZ::*ResetFunc)(), int SkipIdx> struct RndButton : TL1105 {
  void draw(const DrawArgs &args) override {
    drawScaled(args.vg, box.size, UI::RAND_BTN_SCALE,
               [&] { SvgSwitch::draw(args); });
  }
  void drawLayer(const DrawArgs &args, int layer) override {
    drawScaled(args.vg, box.size, UI::RAND_BTN_SCALE,
               [&] { SvgSwitch::drawLayer(args, layer); });
  }
  void onDoubleClick(const event::DoubleClick &e) override {
    if (auto q = getParamQuantity()) {
      if (auto m = dynamic_cast<UZZ *>(q->module)) {
        (m->*ResetFunc)();
        m->skipNextRandom[SkipIdx] = true;
      }
    }
    e.consume(this);
  }
};

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
    auto svg = loadPluginSvg(pluginPath);
    if (!svg)
      svg = Svg::load(asset::system(fallbackPath));
    if (svg) {
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

// Step mode button (play/mute/skip)
struct StepModeButton : app::SvgSwitch {
  StepModeButton() {
    momentary = false;
    shadow->visible = false;

    auto loadFrame = [&](const char *pluginPath, const char *fallbackPath) {
      if (auto svg = loadPluginSvg(pluginPath))
        addFrame(svg);
      else
        addFrame(Svg::load(asset::system(fallbackPath)));
    };

    loadFrame("res/step_play.svg", "res/ComponentLibrary/TL1105_0.svg");
    loadFrame("res/step_mute.svg", "res/ComponentLibrary/TL1105_1.svg");
    loadFrame("res/step_skip.svg", "res/ComponentLibrary/TL1105_2.svg");
    loadFrame("res/step_accum.svg", "res/ComponentLibrary/TL1105_0.svg");
    loadFrame("res/step_accum_down.svg", "res/ComponentLibrary/TL1105_0.svg");
  }
};

// Short direction label for the display (fits narrow rect)
static const char *dirShort(int v) {
  switch (clamp(v, -2, 2)) {
  case -2:
    return "P.P";
  case -1:
    return "BWD";
  case 0:
    return "FWD";
  case 1:
    return "RND";
  default:
    return "DRK";
  }
}

// ─── ParamDisplay ──────────────────────────────────────────────────────────
// Styled like OxiCvExp's ExpChannelLabel: dark rounded rect + blue #5DB7FF
// text. Reads and formats the value of one param from the UZZ module.
struct ParamDisplay : TransparentWidget {
  UZZ *module = nullptr;
  int paramId = 0;

  ParamDisplay(Vec pos, Vec size, UZZ *m, int pid) : module(m), paramId(pid) {
    box.pos = pos;
    box.size = size;
  }

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
    case UZZ::DIR_MODE_PARAM:
      return dirShort((int)std::round(v));
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

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1)
      return;
    std::shared_ptr<Font> font = APP->window->uiFont;
    if (!font)
      return;

    // Background rect
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.5f);
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 180));
    nvgFill(args.vg);

    // Value text
    std::string txt = formatValue();
    nvgFontFaceId(args.vg, font->handle);
    nvgFontSize(args.vg, 9.5f);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, nvgRGB(0x5D, 0xB7, 0xFF));
    float cx = box.size.x * 0.5f;
    float cy = box.size.y * 0.5f;
    nvgText(args.vg, cx, cy, txt.c_str(), nullptr);
  }
};

// Muestra en dos líneas: semitones (arriba) y clip count (abajo).
struct AccumDisplay : TransparentWidget {
  UZZ* module;

  AccumDisplay(Vec pos, Vec size, UZZ* m) : module(m) {
    box.pos = pos;
    box.size = size;
  }

  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer != 1) return;
    std::shared_ptr<Font> font = APP->window->uiFont;
    if (!font) return;

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.5f);
    nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 180));
    nvgFill(args.vg);

    std::string stTxt   = "--";
    std::string clipTxt = "OFF";
    if (module) {
      int st   = (int)std::round(module->params[UZZ::ACCUM_AMT_PARAM].getValue());
      int clip = (int)std::round(module->params[UZZ::ACCUM_CLIP_PARAM].getValue());
      stTxt   = std::to_string(st) + "st";
      clipTxt = (clip > 0) ? std::to_string(clip) + "st" : "OFF";
    }

    nvgFontFaceId(args.vg, font->handle);
    nvgFillColor(args.vg, nvgRGB(0x5D, 0xB7, 0xFF));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    float cx = box.size.x * 0.5f;
    nvgFontSize(args.vg, 8.5f);
    nvgText(args.vg, cx, box.size.y * 0.30f, stTxt.c_str(), nullptr);
    nvgText(args.vg, cx, box.size.y * 0.72f, clipTxt.c_str(), nullptr);
  }
};

// Panel-theme-aware text color (white-ish on dark panels, black on light).
static NVGcolor panelTextColor() {
  return settings::preferDarkPanels ? nvgRGB(0xC8, 0xD4, 0xE3)
                                    : nvgRGB(0x14, 0x18, 0x22);
}

// Static text label
struct TextLabel : TransparentWidget {
    std::string text;
    float fontSize = 9.f;
    NVGcolor color = nvgRGBA(0, 0, 0, 0); // alpha==0 → use panelTextColor()

    TextLabel(const char* t, Vec pos, Vec size = Vec(40.f, 12.f)) : text(t) {
        box.pos  = pos;
        box.size = size;
        for (auto& c : text) c = toupper((unsigned char)c);
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        std::shared_ptr<Font> font = APP->window->uiFont;
        if (!font) return;
        nvgFontSize(args.vg, fontSize);
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, color.a > 0.f ? color : panelTextColor());
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        float cx = box.size.x * .5f;
        float by = box.size.y;
        nvgText(args.vg, cx, by, text.c_str(), nullptr);
        nvgText(args.vg, cx + 0.15f, by, text.c_str(), nullptr); // fake bold
    }
};

// Thin horizontal connector line between a label and a port.
struct ConnectorLine : TransparentWidget {
  float x1, y1, x2, y2;
  int alpha;
  ConnectorLine(float ax1, float ay1, float ax2, float ay2, int a = -1)
      : x1(ax1), y1(ay1), x2(ax2), y2(ay2) {
    if (a < 0)
      alpha = settings::preferDarkPanels ? 160 : 180;
    else
      alpha = a;
    box.pos = Vec(std::min(x1, x2) - 2.f, std::min(y1, y2) - 2.f);
    box.size = Vec(std::abs(x2 - x1) + 4.f, std::abs(y2 - y1) + 4.f);
  }
  void draw(const DrawArgs &args) override {
    NVGcolor c = settings::preferDarkPanels ? nvgRGBA(0x9A, 0xA2, 0xB5, alpha)
                                            : nvgRGBA(0x55, 0x5A, 0x6A, alpha);
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, x1 - box.pos.x, y1 - box.pos.y);
    nvgLineTo(args.vg, x2 - box.pos.x, y2 - box.pos.y);
    nvgStrokeColor(args.vg, c);
    nvgStrokeWidth(args.vg, 0.4f);
    nvgLineCap(args.vg, NVG_ROUND);
    nvgStroke(args.vg);
  }
};

// Note label
struct NoteLabel : TransparentWidget {
  UZZ *module = nullptr;
  int stepIndex = 0;
  std::string fontPath;

  NoteLabel(UZZ *m, int i) : module(m), stepIndex(i) {
    box.size = Vec(24.f, 12.f);
    fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
  }

  void draw(const DrawArgs &args) override {
    if (!module)
      return;
    std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
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
  UZZ *module;
  CapybaraWidget(UZZ *module) : module(module) {}

  void draw(const DrawArgs &args) override {
    if (!module)
      return;
    nvgSave(args.vg);
    // Reduced scale by another 15% (X: 0.285->0.242, Y: 0.3135->0.266)
    nvgScale(args.vg, -0.242f, 0.266f);

    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, -33.617f, -32.668f);
    nvgBezierTo(args.vg, -35.069f, -32.305f, -35.7f, -30.087f, -39.453f, -28.211f);
    nvgBezierTo(args.vg, -40.758f, -27.557f, -43.168f, -26.883f, -46.456f, -25.24f);
    nvgBezierTo(args.vg, -48.044f, -24.445f, -56.305f, -22.731f, -58.022f, -19.297f);
    nvgBezierTo(args.vg, -59.027f, -17.287f, -60.858f, -10.142f, -57.704f, -6.988f);
    nvgBezierTo(args.vg, -53.272f, -2.556f, -52.279f, -5.715f, -44.652f, -5.715f);
    nvgBezierTo(args.vg, -40.382f, -5.715f, -40.194f, -7.306f, -36.376f, -5.396f);
    nvgBezierTo(args.vg, -34.067f, -4.242f, -28.591f, 0.621f, -28.204f, 1.395f);
    nvgBezierTo(args.vg, -27.953f, 1.899f, -27.703f, 1.654f, -27.462f, 2.138f);
    nvgBezierTo(args.vg, -27.291f, 2.478f, -27.48f, 2.526f, -26.507f, 4.472f);
    nvgBezierTo(args.vg, -24.656f, 8.173f, -23.758f, 9.549f, -24.703f, 17.1f);
    nvgBezierTo(args.vg, -25.319f, 22.023f, -26.511f, 21.455f, -29.266f, 24.21f);
    nvgBezierTo(args.vg, -30.343f, 25.287f, -32.314f, 26.275f, -32.661f, 26.969f);
    nvgBezierTo(args.vg, -32.956f, 27.557f, -34.662f, 28.141f, -33.085f, 28.666f);
    nvgBezierTo(args.vg, -30.983f, 29.367f, -30.962f, 28.815f, -28.734f, 28.666f);
    nvgBezierTo(args.vg, -28.365f, 28.642f, -25.057f, 28.421f, -24.385f, 27.075f);
    nvgBezierTo(args.vg, -21.899f, 22.105f, -20.24f, 21.86f, -19.715f, 20.283f);
    nvgBezierTo(args.vg, -19.638f, 20.049f, -19.614f, 20.079f, -18.335f, 17.524f);
    nvgBezierTo(args.vg, -18.305f, 17.465f, -17.414f, 14.464f, -15.682f, 13.598f);
    nvgBezierTo(args.vg, -11.796f, 11.655f, -10.243f, 12.145f, -10.058f, 13.068f);
    nvgBezierTo(args.vg, -9.697f, 14.877f, -9.553f, 14.846f, -6.663f, 17.737f);
    nvgBezierTo(args.vg, -5.155f, 19.245f, -3.512f, 26.046f, -5.071f, 27.605f);
    nvgBezierTo(args.vg, -5.394f, 27.928f, -8.468f, 31.001f, -9.316f, 31.425f);
    nvgBezierTo(args.vg, -9.669f, 31.602f, -0.201f, 32.614f, 0.553f, 31.107f);
    nvgBezierTo(args.vg, 0.641f, 30.931f, 1.9f, 29.778f, 1.931f, 28.985f);
    nvgBezierTo(args.vg, 1.976f, 27.891f, 1.931f, 27.896f, 1.931f, 15.296f);
    nvgBezierTo(args.vg, 1.931f, 13.057f, 5.332f, 13.172f, 6.177f, 12.749f);
    nvgBezierTo(args.vg, 7.719f, 11.978f, 11.297f, 11.772f, 13.71f, 11.37f);
    nvgBezierTo(args.vg, 23.072f, 9.81f, 23.126f, 10.272f, 25.065f, 9.884f);
    nvgBezierTo(args.vg, 25.394f, 9.818f, 25.359f, 9.827f, 25.702f, 9.778f);
    nvgBezierTo(args.vg, 27.506f, 9.52f, 28.739f, 9.91f, 27.929f, 8.292f);
    nvgBezierTo(args.vg, 27.409f, 7.249f, 26.34f, 5.425f, 28.779f, 2.987f);
    nvgBezierTo(args.vg, 29.026f, 2.739f, 28.885f, 3.199f, 28.249f, 3.836f);
    nvgBezierTo(args.vg, 27.615f, 4.469f, 26.953f, 12.141f, 30.053f, 13.174f);
    nvgBezierTo(args.vg, 32.483f, 13.984f, 32.909f, 15.809f, 33.236f, 16.463f);
    nvgBezierTo(args.vg, 33.504f, 16.999f, 33.804f, 21.473f, 31.644f, 22.193f);
    nvgBezierTo(args.vg, 31.525f, 22.233f, 29.898f, 24.181f, 29.203f, 24.528f);
    nvgBezierTo(args.vg, 28.566f, 24.846f, 28.019f, 25.393f, 27.929f, 25.483f);
    nvgBezierTo(args.vg, 26.59f, 26.822f, 24.547f, 26.414f, 21.988f, 31.531f);
    nvgBezierTo(args.vg, 21.733f, 32.042f, 22.201f, 32.168f, 29.097f, 32.168f);
    nvgBezierTo(args.vg, 30.536f, 32.168f, 31.93f, 32.667f, 30.901f, 31.638f);
    nvgBezierTo(args.vg, 28.141f, 28.877f, 36.13f, 27.335f, 37.268f, 25.059f);
    nvgBezierTo(args.vg, 37.482f, 24.632f, 38.665f, 23.54f, 38.965f, 22.936f);
    nvgBezierTo(args.vg, 40.346f, 20.177f, 41.542f, 18.981f, 41.831f, 18.692f);
    nvgBezierTo(args.vg, 43.147f, 17.376f, 42.364f, 13.122f, 44.59f, 14.235f);
    nvgBezierTo(args.vg, 45.433f, 14.656f, 44.888f, 15.431f, 45.757f, 15.72f);
    nvgBezierTo(args.vg, 46.614f, 16.006f, 52.694f, 19.067f, 52.761f, 20.071f);
    nvgBezierTo(args.vg, 52.792f, 20.541f, 52.877f, 21.938f, 52.231f, 24.528f);
    nvgBezierTo(args.vg, 51.828f, 26.134f, 52.191f, 26.734f, 48.941f, 27.817f);
    nvgBezierTo(args.vg, 48.489f, 27.968f, 45.441f, 29.514f, 43.74f, 30.364f);
    nvgBezierTo(args.vg, 43.105f, 30.683f, 43.882f, 30.752f, 44.272f, 31.531f);
    nvgBezierTo(args.vg, 44.507f, 32.004f, 52.552f, 32.061f, 52.972f, 31.956f);
    nvgBezierTo(args.vg, 53.161f, 31.909f, 53.066f, 31.803f, 55.096f, 30.789f);
    nvgBezierTo(args.vg, 55.732f, 30.47f, 56.1f, 29.734f, 56.156f, 29.621f);
    nvgBezierTo(args.vg, 57.022f, 27.89f, 58.144f, 28.384f, 59.446f, 21.875f);
    nvgBezierTo(args.vg, 60.731f, 15.454f, 60.858f, 12.675f, 59.233f, 11.051f);
    nvgBezierTo(args.vg, 58.608f, 10.425f, 59.013f, 10.194f, 58.385f, 9.566f);
    nvgBezierTo(args.vg, 56.546f, 7.726f, 55.691f, 3.113f, 50.638f, 5.64f);
    nvgBezierTo(args.vg, 46.861f, 7.528f, 45.007f, 6.184f, 43.635f, 8.929f);
    nvgBezierTo(args.vg, 42.702f, 10.796f, 42.078f, 11.088f, 42.786f, 13.917f);
    nvgBezierTo(args.vg, 43.046f, 14.954f, 43.3f, 14.844f, 43.316f, 14.765f);
    nvgBezierTo(args.vg, 43.784f, 12.429f, 40.367f, 11.454f, 43.74f, 8.08f);
    nvgBezierTo(args.vg, 44.356f, 7.466f, 50.013f, 6.693f, 53.185f, 4.579f);
    nvgBezierTo(args.vg, 53.588f, 4.31f, 55.536f, 3.537f, 56.05f, -2.637f);
    nvgBezierTo(args.vg, 56.41f, -6.946f, 55.843f, -11.761f, 55.202f, -12.081f);
    nvgBezierTo(args.vg, 54.923f, -12.22f, 54.969f, -12.782f, 53.822f, -13.355f);
    nvgBezierTo(args.vg, 53.459f, -13.537f, 46.341f, -20.8f, 43.529f, -21.738f);
    nvgBezierTo(args.vg, 38.082f, -23.554f, 38.125f, -23.646f, 35.571f, -24.285f);
    nvgBezierTo(args.vg, 32.689f, -25.005f, 30.282f, -26.749f, 27.082f, -27.15f);
    nvgBezierTo(args.vg, 26.673f, -27.201f, 19.488f, -28.488f, 18.38f, -28.529f);
    nvgBezierTo(args.vg, 5.239f, -29.016f, 2.13f, -27.461f, 0.87f, -26.831f);
    nvgBezierTo(args.vg, -0.029f, -26.382f, -17.334f, -21.53f, -22.263f, -24.815f);
    nvgBezierTo(args.vg, -22.898f, -25.24f, -25.478f, -26.099f, -25.763f, -26.195f);
    nvgBezierTo(args.vg, -28.091f, -26.97f, -28.893f, -29.829f, -30.538f, -30.651f);
    nvgBezierTo(args.vg, -31.257f, -31.011f, -31.492f, -31.845f, -32.236f, -32.031f);
    nvgBezierTo(args.vg, -33.103f, -32.248f, -33.066f, -32.272f, -33.934f, -32.561f);
    nvgClosePath(args.vg);

    // Theme-aware color interpolation
    NVGcolor base = nvgRGB(0x2C, 0x7F, 0xFF);
    NVGcolor flashTarget;
    if (settings::preferDarkPanels) {
      flashTarget = nvgRGB(0xFF, 0xFF, 0xFF); // Flash to White in Dark Mode
    } else {
      flashTarget = nvgRGB(0x00, 0x33, 0x66); // Flash to Dark Navy in Light Mode
    }
    nvgStrokeColor(args.vg, nvgLerpRGBA(base, flashTarget, module->capiFlash));
    nvgStrokeWidth(args.vg, 2.2f);
    nvgStroke(args.vg);

    nvgRestore(args.vg);
  }
};

// ============================================================================
// Module Widget
// ============================================================================
struct UZZWidget : ModuleWidget {
  UZZWidget(UZZ *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/UZZ-light.svg"),
                         asset::plugin(pluginInstance, "res/UZZ.svg")));

    const int cols = UI::COLS;
    auto Xc = [&](int i) { return UI::colCenter(box.size.x, i); };

    for (int i = 0; i < cols; ++i)
      addChild(createLightCentered<SmallLight<BlueLight>>(
          Vec(Xc(i) - 10, UI::Y_STEP_LED), module, UZZ::STEP_LIGHTS + i));
    for (int i = 0; i < cols; ++i)
      addParam(createParamCentered<StepModeButton>(
          Vec(Xc(i) + 10, UI::Y_STEP_MODE), module, UZZ::STEP_MODE_PARAMS + i));
    for (int i = 0; i < cols; ++i) {
      auto *lbl = new NoteLabel(module, i);
      lbl->box.pos = Vec(Xc(i) - lbl->box.size.x * .2f - 18, UI::Y_NOTE + 2);
      addChild(lbl);
    }

    for (int i = 0; i < cols; ++i)
      addParam(createParamCentered<Trimpot>(
          Vec(Xc(i) + 14.f, UI::Y_PROB - 14.f), module, UZZ::PROB_PARAMS + i));

    for (int i = 0; i < cols; ++i)
      addParam(createParamCentered<UzzArcKnob>(Vec(Xc(i), UI::Y_PITCH), module,
                                               UZZ::PITCH_PARAMS + i));
    for (int i = 0; i < cols; ++i)
      addParam(createParamCentered<UzzArcKnob>(Vec(Xc(i), UI::Y_OCT), module,
                                               UZZ::OCT_PARAMS + i));
    for (int i = 0; i < cols; ++i)
      addParam(createParamCentered<UzzArcKnob>(Vec(Xc(i), UI::Y_DUR), module,
                                               UZZ::DUR_PARAMS + i));
    for (int i = 0; i < cols; ++i)
      addParam(createParamCentered<UzzArcKnob>(Vec(Xc(i), UI::Y_C1), module,
                                               UZZ::M1_PARAMS + i));
    for (int i = 0; i < cols; ++i)
      addParam(createParamCentered<UzzArcKnob>(Vec(Xc(i), UI::Y_C2), module,
                                               UZZ::M2_PARAMS + i));

    const float trigL = UI::trigLeftX();

    addInput(createInputCentered<UzzInputPort>(
        Vec(trigL, UI::Y_STEP_MODE + 18), module, UZZ::RND_STEP_TRIG_INPUT));
    addParam(createParamCentered<RndStepButton>(
        Vec(UI::randButtonX(), UI::Y_STEP_MODE + 18), module,
        UZZ::RND_STEP_PARAM));

    addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_PITCH), module,
                                               UZZ::RND_PITCH_TRIG_INPUT));
    addParam(createParamCentered<RndPitchButton>(
        Vec(UI::randButtonX(), UI::Y_PITCH), module, UZZ::RND_PITCH_PARAM));

    addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_OCT), module,
                                               UZZ::RND_OCT_TRIG_INPUT));
    addParam(createParamCentered<RndOctButton>(
        Vec(UI::randButtonX(), UI::Y_OCT), module, UZZ::RND_OCTAVE_PARAM));

    addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_DUR), module,
                                               UZZ::RND_DUR_TRIG_INPUT));
    addParam(createParamCentered<RndDurButton>(
        Vec(UI::randButtonX(), UI::Y_DUR), module, UZZ::RND_DUR_PARAM));

    addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_C1), module,
                                               UZZ::RND_M1_TRIG_INPUT));
    addParam(createParamCentered<RndM1Button>(Vec(UI::randButtonX(), UI::Y_C1),
                                              module, UZZ::RND_M1_PARAM));

    const float bBase = box.size.y - UI::BOTTOM_MARGIN;
    const float yTop = bBase + UI::BOT_DY_TOP;
    const float yMid = bBase + UI::BOT_DY_MID;
    const float yBot = bBase + UI::BOT_DY_BOT;
    const float ySep1 = (yTop + yMid) * .5f;
    const float ySep2 = (yMid + yBot) * .5f;

    addInput(createInputCentered<UzzInputPort>(Vec(trigL, UI::Y_C2), module,
                                               UZZ::RND_M2_TRIG_INPUT));
    addParam(createParamCentered<RndM2Button>(Vec(UI::randButtonX(), UI::Y_C2),
                                              module, UZZ::RND_M2_PARAM));

    addInput(createInputCentered<UzzInputPort>(Vec(trigL, ySep2), module,
                                               UZZ::RND_PROB_TRIG_INPUT));
    addParam(createParamCentered<RndProbButton>(Vec(UI::randButtonX(), ySep2),
                                                module, UZZ::RND_PROB_PARAM));

    auto addShiftPair = [&](float y, int downParam, int upParam) {
      addParam(createParamCentered<RowShiftUpButton>(
          Vec(UI::rowShiftX(), UI::rowShiftYUp(y)), module, upParam));
      addParam(createParamCentered<RowShiftDownButton>(
          Vec(UI::rowShiftX(), UI::rowShiftYDown(y)), module, downParam));
    };

    addShiftPair(UI::Y_PITCH, UZZ::PITCH_SHIFT_DOWN_PARAM,
                 UZZ::PITCH_SHIFT_UP_PARAM);
    addShiftPair(UI::Y_OCT, UZZ::OCT_SHIFT_DOWN_PARAM, UZZ::OCT_SHIFT_UP_PARAM);
    addShiftPair(UI::Y_DUR, UZZ::DUR_SHIFT_DOWN_PARAM, UZZ::DUR_SHIFT_UP_PARAM);
    addShiftPair(UI::Y_C1, UZZ::M1_SHIFT_DOWN_PARAM, UZZ::M1_SHIFT_UP_PARAM);
    addShiftPair(UI::Y_C2, UZZ::M2_SHIFT_DOWN_PARAM, UZZ::M2_SHIFT_UP_PARAM);
    addShiftPair(ySep2, UZZ::PROB_SHIFT_DOWN_PARAM, UZZ::PROB_SHIFT_UP_PARAM);



    addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_CTRL1, yBot), module,
                                             UZZ::START_PARAM));
    addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_CTRL1, yMid), module,
                                             UZZ::STEPS_PARAM));
    addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_CTRL1, yTop), module,
                                             UZZ::RATIO_IDX_PARAM));

    addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_CTRL2, yTop), module,
                                             UZZ::DIR_MODE_PARAM));
    addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_CTRL2, yMid), module,
                                             UZZ::SWING_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(UI::X_CTRL2 - 10.f, yBot), module,
                                          UZZ::ACCUM_AMT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(UI::X_CTRL2 + 10.f, yBot), module,
                                          UZZ::ACCUM_CLIP_PARAM));

    // ── Param displays (dark rect + blue text, like OxiCvExp channel label) ──
    {
      // Displays centred on step 5 (RATIO group) and step 8 (DIR group).
      const float dW = 38.f, dH = 14.f;
      const float xStep5 = Xc(4); // col_center(4) = step 5
      const float xStep8 = Xc(7); // col_center(7) = step 8
      auto addDispAt = [&](float cx, float ky, int pid) {
        addChild(new ParamDisplay(Vec(cx - dW * .5f, ky - dH * .5f),
                                  Vec(dW, dH), module, pid));
      };
      addDispAt(xStep5, yTop, UZZ::RATIO_IDX_PARAM);
      addDispAt(xStep5, yMid, UZZ::STEPS_PARAM);
      addDispAt(xStep5, yBot, UZZ::START_PARAM);
      addDispAt(xStep8, yTop, UZZ::DIR_MODE_PARAM);
      addDispAt(xStep8, yMid, UZZ::SWING_PARAM);
      addChild(new AccumDisplay(
          Vec(xStep8 - dW * .5f, yBot - dH * .5f), Vec(dW, dH), module));
    }

    // — Bottom section labels —
    // CLK / RESET / XPOSE → label centered on step 1's column,
    // with a thin gray connector line running to the input port.
    auto addInputLabel = [&](float cy, const char *text) {
      const float w = 54.f, h = 12.f;
      const float cx = UI::X_IN;
      auto *lbl =
          new TextLabel(text, Vec(cx - w * .5f, cy - h * .5f), Vec(w, h));
      lbl->fontSize = 11.f;
      addChild(lbl);
      // Connector line: from port at step 1 to label at step 2.
      const float gap = 5.f;
      const float textLeft = cx - 14.f - gap;
      const float dividerX = Xc(0) + 14.f; // port at step 1
      const float lineStart = dividerX + gap;
      if (textLeft > lineStart + 2.f)
        addChild(new ConnectorLine(lineStart, cy, textLeft, cy));
    };
    addInputLabel(yTop, "CLK");
    addInputLabel(yMid, "RESET");
    addInputLabel(yBot, "XPOSE");

    // Generic helper: label centered on a step column, connector line
    // ending just before a target knob (knobX − knob radius − gap).
    // lblHalfW: approx half-width of the text (tweak per label group).
    // portR   : approx widget radius (knob ~13, port ~10).
    auto addStepLabel = [&](int stepCol1Based, float knobX, float cy,
                            const char *text, float lblHalfW = 18.f,
                            float portR = 13.f) {
      const float w = 54.f, h = 12.f;
      const float cx = Xc(stepCol1Based - 1);
      auto *lbl =
          new TextLabel(text, Vec(cx - w * .5f, cy - h * .5f), Vec(w, h));
      lbl->fontSize = 11.f;
      addChild(lbl);
      const float gap = 5.f;
      const float textRight = cx + lblHalfW + gap;
      const float lineEnd = knobX - portR - gap;
      if (lineEnd > textRight + 2.f)
        addChild(new ConnectorLine(textRight, cy, lineEnd, cy));
    };

    // RATIO / STEPS / START → aligned with step 3, connector to X_CTRL1 knobs.
    addStepLabel(3, UI::X_CTRL1, yTop, "RATIO");
    addStepLabel(3, UI::X_CTRL1, yMid, "STEPS");
    addStepLabel(3, UI::X_CTRL1, yBot, "START");
    // DIR / SWING / ACUMM → aligned with step 6, connector to X_CTRL2 knobs.
    addStepLabel(6, UI::X_CTRL2, yTop, "DIR");
    addStepLabel(6, UI::X_CTRL2, yMid, "SWING");
    addStepLabel(6, UI::X_CTRL2, yBot, "ACUMM");

    // — UZZ logo text —
    // Placed at Xc(8) (column index 8 = step 9), midway between the param
    // displays and SLEW label. It resides right above the SVG logo.
    {
      const float w = 54.f, h = 12.f;
      auto *lbl =
          new TextLabel("UZZ", Vec(Xc(8) - w * .5f, yTop - h * .5f), Vec(w, h));
      lbl->fontSize = 16.f;
      lbl->color = nvgRGB(0x2C, 0x7F, 0xFF);
      addChild(lbl);
    }

    // SLEW / MODE → label at step 10, widget at step 11 (X_SWITCH).
    // Double connector: label ── widget ── next label (PITCH/GATE).
    {
      const float gap = 5.f;
      const float wRadius = 13.f;     // approx widget half-width
      const float lblHalfW = 18.f;    // approx text half-width
      const float xLbl10 = Xc(9);     // step 10 centre
      const float xPitchLbl = Xc(11); // step 12 centre (PITCH/GATE labels)

      // Struct: text label at step 10
      auto addSwitchLabel = [&](float cy, const char *text) {
        const float w = 54.f, h = 12.f;
        auto *lbl =
            new TextLabel(text, Vec(xLbl10 - w * .5f, cy - h * .5f), Vec(w, h));
        lbl->fontSize = 11.f;
        addChild(lbl);
      };

      // Connector helper: from x1 to x2 at height cy (with gap on each side)
      auto seg = [&](float x1, float x2, float cy) {
        if (x2 - gap > x1 + gap)
          addChild(new ConnectorLine(x1 + gap, cy, x2 - gap, cy));
      };

      // SLEW: label(step10) ─── Trimpot(step11) ─── PITCH label(step12)
      addSwitchLabel(yTop, "SLEW");
      seg(xLbl10 + lblHalfW, UI::X_SWITCH - wRadius, yTop); // label → trimpot
      seg(UI::X_SWITCH + wRadius, xPitchLbl - lblHalfW,
          yTop); // trimpot → PITCH lbl

      // MODE: label(step10) ─── CKSS(step11) ─── GATE/TRIG Display(step12) ───
      // GATE Output(step13)
      addSwitchLabel(yMid, "MODE");
      seg(xLbl10 + lblHalfW, UI::X_SWITCH - wRadius, yMid);

      const float dW = 38.f, dH = 14.f;
      addChild(new ParamDisplay(Vec(xPitchLbl - dW * .5f, yMid - dH * .5f),
                                Vec(dW, dH), module, UZZ::GATE_MODE_PARAM));
      // Line from switch to display
      seg(UI::X_SWITCH + wRadius, xPitchLbl - (dW * .5f), yMid);
      // Line from display to output port (X_OUT1)
      {
        const float lineStart = xPitchLbl + (dW * .5f) + gap;
        const float lineEnd = UI::X_OUT1 - 10.f - gap;
        if (lineEnd > lineStart + 2.f)
          addChild(new ConnectorLine(lineStart, yMid, lineEnd, yMid));
      }

      // POLY: label(step10) ─── UzzOutputPort(step11)
      addSwitchLabel(yBot, "POLY");
      seg(xLbl10 + lblHalfW, UI::X_SWITCH - wRadius, yBot);
    }
    // PITCH / GATE / EOC → label at step 12, output at step 13.
    addStepLabel(12, UI::X_OUT1, yTop, "V/OCT", 14.f, 10.f);
    addStepLabel(12, UI::X_OUT1, yBot, "EOC", 14.f, 10.f);
    // M1 / M2 / GLB.P → label at step 14, output at step 15.
    addStepLabel(14, UI::X_OUT2, yTop, "MOD1", 14.f, 10.f);
    addStepLabel(14, UI::X_OUT2, yMid, "MOD2", 14.f, 10.f);
    addStepLabel(14, UI::X_OUT2, yBot, "PROB", 14.f, 10.f);

    // — Row labels (centered above each trigger input on the left column) —
    auto addRowLabel = [&](float cy, const char *text) {
      const float w = 50.f, h = 10.f;
      // ALIGN_BOTTOM: text renders at box.pos.y + h.
      // Target baseline: cy - 13  (3 px gap above port top edge ~cy-10).
      // So box.pos.y = cy - 13 - h.
      float cx = UI::trigLeftX();
      auto *lbl = new TextLabel(text, Vec(cx - w * .5f, cy - 13.f - h), Vec(w, h));
      lbl->fontSize = 9.f;
      addChild(lbl);
    };
    addRowLabel(UI::Y_STEP_MODE + 18.f, "MODE");
    addRowLabel(ySep2, "PROB");
    addRowLabel(UI::Y_PITCH, "PITCH");
    addRowLabel(UI::Y_OCT, "OCT");
    addRowLabel(UI::Y_DUR, "DUR");
    addRowLabel(UI::Y_C1, "MOD1");
    addRowLabel(UI::Y_C2, "MOD2");

    addInput(createInputCentered<UzzInputPort>(Vec(Xc(0), yTop), module,
                                               UZZ::CLK_INPUT));
    addInput(createInputCentered<UzzInputPort>(Vec(Xc(0), yMid), module,
                                               UZZ::RESET_INPUT));
    addInput(createInputCentered<UzzInputPort>(Vec(Xc(0), yBot), module,
                                               UZZ::XPOSE_INPUT));

    addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT2, yTop), module,
                                                  UZZ::M1_OUTPUT));
    addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT2, yMid), module,
                                                  UZZ::M2_OUTPUT));
    addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_OUT2, yBot), module,
                                             UZZ::PROB_GLOBAL_PARAM));

    addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT1, yTop), module,
                                                  UZZ::PITCH_OUTPUT));
    addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT1, yMid), module,
                                                  UZZ::GATE_OUTPUT));
    addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT1, yBot), module,
                                                  UZZ::EOC_OUTPUT));

    // Separator lines between bottom rows (Step 0 to 8)
    {
      float x0 = UI::LEFT;
      float x8 = UI::LEFT + 8.f * UI::colW(box.size.x);
      addChild(new ConnectorLine(0.f, ySep1, x8, ySep1, 80));
      addChild(new ConnectorLine(x0, ySep2, x8, ySep2, 80));

      float x10 = UI::LEFT + 9.f * UI::colW(box.size.x);
      float x15 = UI::LEFT + 15.f * UI::colW(box.size.x);
      addChild(new ConnectorLine(x10, ySep1, x15, ySep1, 80));
      addChild(new ConnectorLine(x10, ySep2, x15, ySep2, 80));
    }

    // Vertical separator between POLY output and EOC label
    {
      float xMid = (UI::X_SWITCH + Xc(11)) * .5f;
      addChild(new ConnectorLine(xMid, yBot - 12.f, xMid, yBot + 12.f, 80));
    }

    // Hand-drawn Capybara from backup
    {
      auto *capi = new CapybaraWidget(module);
      // Centered in the sidebar (X=24)
      // Repositioned: Moved back to 308 as requested
      capi->box.pos = Vec(24.f, 308.f);
      addChild(capi);
    }

    addOutput(createOutputCentered<UzzOutputPort>(
        Vec(UI::X_SWITCH, yBot), module, UZZ::STEP_GATES_OUTPUT));
    addParam(createParamCentered<CKSS>(Vec(UI::X_SWITCH, yMid), module,
                                       UZZ::GATE_MODE_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(UI::X_SWITCH, yTop), module,
                                          UZZ::SLEW_PARAM));
  }

  void appendContextMenu(ui::Menu *menu) override {
    ModuleWidget::appendContextMenu(menu);
    auto *m = dynamic_cast<UZZ *>(module);

    menu->addChild(new ui::MenuSeparator());
    menu->addChild(createCheckMenuItem(
        "EOC on reset", "", [m]() { return m && m->eocOnReset; },
        [m]() {
          if (m)
            m->eocOnReset = !m->eocOnReset;
        }));

    menu->addChild(createSubmenuItem("Direction mode", "", [m](ui::Menu *sub) {
      for (int i = -2; i <= 2; ++i) {
        sub->addChild(createCheckMenuItem(
            dirLabel(i), "",
            [m, i]() {
              if (!m)
                return false;
              int cur =
                  (int)std::round(m->params[UZZ::DIR_MODE_PARAM].getValue());
              return cur == i;
            },
            [m, i]() {
              if (!m)
                return;
              m->params[UZZ::DIR_MODE_PARAM].setValue((float)i);
              m->navigator.pingDir = 0;
              m->navigator.drunkDir = 1;
            }));
      }
    }));

    menu->addChild(new ui::MenuSeparator());
    menu->addChild(createSubmenuItem("Pitch range", "", [m](ui::Menu *sub) {
      const int opts[] = {11, 23};
      const char *labels[] = {"1 octave (0..11)", "2 octaves (0..23)"};
      for (int k = 0; k < 2; ++k) {
        int s = opts[k];
        const char *lbl = labels[k];
        sub->addChild(createCheckMenuItem(
            lbl, "", [m, s]() { return m && m->pitchRangeSemis == s; },
            [m, s]() {
              if (m)
                m->setPitchRange(s);
            }));
      }
    }));

    auto addRangeMenu = [&](const char* label, int* rangePtr) {
      menu->addChild(createSubmenuItem(label, "", [m, rangePtr](ui::Menu* sub) {
        for (int r = 0; r < UZZRanges::MR_COUNT; ++r) {
          sub->addChild(createCheckMenuItem(
              UZZRanges::RANGE_DEFS[r].label, "",
              [m, rangePtr, r]() { return m && *rangePtr == r; },
              [m, rangePtr, r]() { if (m) *rangePtr = r; }));
        }
      }));
    };
    addRangeMenu("Range Mod 1", &m->m1Range);
    addRangeMenu("Range Mod 2", &m->m2Range);
  }
};

Model *modelUZZ = createModel<UZZ, UZZWidget>("UZZ");
