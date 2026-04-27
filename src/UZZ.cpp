#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"
#include "uzz/ClockProcessor.hpp"
#include "uzz/StepNavigator.hpp"
#include "uzz/UzzLayout.hpp"
#include "uzz/UzzTypes.hpp"

using AnimatekUI::ConnectorLine;
using AnimatekUI::DisplayBox;
using AnimatekUI::drawScaled;
using AnimatekUI::loadPluginSvg;
using AnimatekUI::panelTextColor;
using AnimatekUI::TextLabel;

#ifndef UZZ_USE_CODE_LABELS
#define UZZ_USE_CODE_LABELS 1
#endif

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

  // Pulse mode
  enum PulseMode { PM_PULSE = 0, PM_GATED = 1, PM_HOLD = 2 };
  int pulseMode = PM_PULSE;

  // Ratchet sub-pulse state (PM_RATCHET)
  int pulsesRemaining = 0;
  float pulseTimer = 0.f;
  float pulseInterval = 0.f;
  float pulseGLen = 0.f;
  int pulseStepK = 0;

  // Hold state (PM_HOLD)
  int holdPulsesLeft = 0;
  bool holdPlaying = false;

  int m1Range = UZZRanges::MR_0_10;
  int m2Range = UZZRanges::MR_0_10;
  int pitchRangeSemis = 11; // 11 = 1 octave, 23 = 2 octaves
  int jumpN = 2;

  float capiFlash = 0.f;
  dsp::SchmittTrigger capiTrig;

  float getGateLength(int gateMode, float duty, float period, float sampleTime,
                      float maxWindow = 0.f) const {
    if (gateMode != 0)
      return TRIG_LEN;

    float window = (maxWindow > 0.f) ? maxWindow : period;
    if (window <= 0.f)
      return TRIG_LEN;

    float minOff = std::max(0.001f, 2.f * sampleTime);
    float gLen = window * clamp(duty, 0.01f, 0.95f);
    float maxLen = std::max(TRIG_LEN, window - minOff);
    if (gLen > maxLen)
      gLen = maxLen;
    // Hard upper bound: a single step gate should never exceed 2 s, regardless
    // of what the clock period reports. Guards against pathological state
    // (stale period, paused upstream clock) producing a stuck gate.
    if (gLen > 2.f)
      gLen = 2.f;
    return gLen;
  }

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

      configParam(STEP_MODE_PARAMS + i, 0.f, 7.f, 0.f,
                  string::f("Step mode %d", i + 1));
      paramQuantities[STEP_MODE_PARAMS + i]->snapEnabled = true;

      configParam<DurationQuantity>(DUR_PARAMS + i, 0.01f, 0.95f, 0.50f,
                                    string::f("Duration %d", i + 1));

      configParam(M1_PARAMS + i, 0.f, 10.f, 0.f, string::f("Mod 1 %d", i + 1),
                  " (0..10)");
      configParam(M2_PARAMS + i, 0.f, 10.f, 0.f, string::f("Mod 2 %d", i + 1),
                  " (0..10)");

      configParam<ProbPulseQuantity>(PROB_PARAMS + i, -100.f, 7.f, 0.f,
                                     string::f("Prob/Pulse %d", i + 1));
      paramQuantities[PROB_PARAMS + i]->snapEnabled = true;
    }

    configParam(PROB_GLOBAL_PARAM, 0.f, 100.f, 100.f, "Global probability",
                " %");
    paramQuantities[PROB_GLOBAL_PARAM]->snapEnabled = true;

    configParam(STEPS_PARAM, 1.f, 16.f, 16.f, "Steps");
    paramQuantities[STEPS_PARAM]->snapEnabled = true;
    configParam(START_PARAM, 1.f, 16.f, 1.f, "Start");
    paramQuantities[START_PARAM]->snapEnabled = true;

    configParam<DirModeQuantity>(DIR_MODE_PARAM, (float)DIR_MODE_MIN,
                                 (float)DIR_MODE_MAX, 0.f, "Direction mode");
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
    configParam(ACCUM_CLIP_PARAM, 0.f, 12.f, 0.f,
                "Accumulator wrap (semitones, 0=OFF = full ±12 range)");
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

    pulsesRemaining = 0;
    holdPulsesLeft = 0;
    holdPlaying = false;
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
      int m;
      if (r < 0.55f)
        m = SM_PLAY;
      else if (r < 0.70f)
        m = SM_MUTE;
      else if (r < 0.82f)
        m = SM_SKIP;
      else if (r < 0.88f)
        m = SM_ACCUM_UP;
      else if (r < 0.93f)
        m = SM_ACCUM_DOWN;
      else if (r < 0.96f)
        m = SM_PULSE;
      else if (r < 0.98f)
        m = SM_GATED;
      else
        m = SM_HOLD;
      params[STEP_MODE_PARAMS + i].setValue((float)m);
    }
  }

  void fillRow(int base, float value) {
    for (int i = 0; i < 16; ++i)
      params[base + i].setValue(value);
  }

  void resetPitchRow() { fillRow(PITCH_PARAMS, 0.f); }
  void resetOctaveRow() { fillRow(OCT_PARAMS, 0.f); }
  void resetStepModeRow() { fillRow(STEP_MODE_PARAMS, 0.f); }
  void resetDurRow() { fillRow(DUR_PARAMS, 0.50f); }
  void resetM1Row() { fillRow(M1_PARAMS, 0.f); }
  void resetM2Row() { fillRow(M2_PARAMS, 0.f); }
  void resetProbRow() { fillRow(PROB_PARAMS, 0.f); }

  void randomizeDurations() {
    for (int i = 0; i < 16; ++i)
      params[DUR_PARAMS + i].setValue(0.10f + random::uniform() * 0.80f);
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
    for (int i = 0; i < 16; ++i) {
      float r = random::uniform();
      int v;
      if (r < 0.65f)
        v = -(int)std::floor(random::uniform() * 101.f); // prob 0–100%
      else if (r < 0.75f)
        v = 0; // default (100%/×1)
      else
        v = 1 + (int)std::floor(random::uniform() * 7.f); // pulse ×2–×8
      params[PROB_PARAMS + i].setValue((float)v);
    }
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
    shift_row_float(DUR_PARAMS, dir, s, c, 0.05f, 0.01f, 0.95f, true);
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
    shift_row_float(PROB_PARAMS, dir, s, c, 1.f, -100.f, 7.f, true);
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
    json_object_set_new(rootJ, "seqPos", json_integer(navigator.seqPos));
    json_object_set_new(rootJ, "jumpN", json_integer(jumpN));

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
    if (json_t *j = json_object_get(rootJ, "seqPos"))
      navigator.seqPos = clamp((int)json_integer_value(j), 0, 31);
    if (json_t *j = json_object_get(rootJ, "jumpN"))
      jumpN = clamp((int)json_integer_value(j), 2, 7);

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
        // sqrt() compresses the 1-pole alpha: snappier attack, shorter tail —
        // intentionally more musical than a pure exponential glide.
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
    pulsesRemaining = 0;
    holdPulsesLeft = 0;
    holdPlaying = false;
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

    bool wasClkConnected = clock.prevClkConnected;
    bool clockNow =
        clock.process(args, inputs[CLK_INPUT], ratio, swing, clkConnected);

    if (capiTrig.process(inputs[CLK_INPUT].getVoltage())) {
      capiFlash = 1.0f;
    }
    // Decay flash over time
    if (capiFlash > 0.f) {
      capiFlash = std::max(0.f, capiFlash - 15.f * args.sampleTime);
    }

    if (!clkConnected && wasClkConnected)
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

    if (!clkConnected) {
      float semis = params[PITCH_PARAMS + step].getValue();
      int octIv = (int)std::round(params[OCT_PARAMS + step].getValue());
      int accum = accumOffset[step];
      float pitchV =
          ((semis + (float)xposeSemis + (float)accum) / 12.f) + (float)octIv;

      outputs[M1_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(
          params[M1_PARAMS + step].getValue(), m1Range));
      outputs[M2_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(
          params[M2_PARAMS + step].getValue(), m2Range));

      if (wasClkConnected) {
        hardStop(steps);
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
      pulsesRemaining = 0; // cancel ratchet sub-pulses

      // Determine effective pulse mode for the current step
      // (per-step modes SM_PULSE/SM_GATED/SM_HOLD override the global
      // pulseMode)
      auto stepEffMode = [&](int smode) -> int {
        if (smode == SM_PULSE)
          return PM_PULSE;
        if (smode == SM_GATED)
          return PM_GATED;
        if (smode == SM_HOLD)
          return PM_HOLD;
        return pulseMode;
      };
      int curStepMode =
          (int)std::round(params[STEP_MODE_PARAMS + step].getValue());
      int effMode = stepEffMode(curStepMode);

      // PM_HOLD / PM_GATED: consume held ticks before advancing to the next
      // step
      bool holdFired = false;
      if ((effMode == PM_HOLD || effMode == PM_GATED) && holdPulsesLeft > 0) {
        if (resetPending) {
          holdPulsesLeft = 0; // reset interrupts hold
        } else {
          --holdPulsesLeft;
          if (effMode == PM_HOLD && holdPlaying) {
            // PM_HOLD: re-fire a gate on each tick
            int hk = (step - start + 16) & 15;
            int gateMode = (int)std::round(params[GATE_MODE_PARAM].getValue());
            float duty =
                clamp(params[DUR_PARAMS + step].getValue(), 0.01f, 0.95f);
            float period = clock.getVirtPeriod();
            float gLen = getGateLength(gateMode, duty, period, args.sampleTime);
            gatePulse.trigger(gLen);
            stepGateTrig[hk].trigger(gLen);
          }
          // PM_GATED: gate is already running long — nothing to do here
          holdFired = true;
        }
      }

      if (!holdFired) {
        int modeDir = clamp((int)std::round(params[DIR_MODE_PARAM].getValue()),
                            DIR_MODE_MIN, DIR_MODE_MAX);

        bool allSkip = false;
        bool wrapped = false;
        int nextStep = navigator.getNextStep(
            step, start, steps, modeDir,
            [this](int idx) {
              return params[STEP_MODE_PARAMS + idx].getValue();
            },
            playCurrentOnNextTick, wrapped, allSkip, jumpN);

        playCurrentOnNextTick = false;
        step = nextStep;
        if (wrapped)
          eocPulse.trigger(TRIG_LEN);

        bool muteGlobal = false;
        if (modeDir == DIR_FWD || modeDir == DIR_REV) {
          muteGlobal = allSkip;
        } else {
          bool anyPlayable = false;
          for (int ki = 0; ki < steps; ++ki) {
            int sIdx = wrap16(start + ki);
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
        bool playing =
            !muteGlobal &&
            (mode == SM_PLAY || mode == SM_ACCUM_UP || mode == SM_ACCUM_DOWN ||
             mode == SM_PULSE || mode == SM_GATED || mode == SM_HOLD);
        if (playing) {
          // Bipolar prob/pulse knob: >=0 = probability, <0 = pulse count
          float ppVal = params[PROB_PARAMS + step].getValue();
          float pGlobal =
              clamp(params[PROB_GLOBAL_PARAM].getValue(), 0.f, 100.f) / 100.f;
          // Left side (<=0): probability 0–100%; right side (>0): 100% prob
          // (right side = pulse count for SM_PULSE/GATED/HOLD).
          float pStep =
              (ppVal <= 0.f) ? clamp((100.f + ppVal) / 100.f, 0.f, 1.f) : 1.f;
          if (pStep * pGlobal < 1.f && random::uniform() >= pStep * pGlobal)
            playing = false;
        }
        if (playing) {
          if (mode == SM_ACCUM_UP || mode == SM_ACCUM_DOWN) {
            int amt = (int)std::round(params[ACCUM_AMT_PARAM].getValue());
            int wrap = (int)std::round(params[ACCUM_CLIP_PARAM].getValue());
            int signedAmt = (mode == SM_ACCUM_UP) ? amt : -amt;
            int v = accumOffset[step] + signedAmt;
            if (wrap > 0) {
              // Módulo sobre el rango configurado alrededor de 0.
              int span = wrap * 2 + 1;
              v = ((v + wrap) % span + span) % span - wrap;
            } else {
              // Sin wrap configurado: rango completo ±12.
              static constexpr int ACCUM_RANGE = 25; // -12..+12 inclusive
              v = ((v + 12) % ACCUM_RANGE + ACCUM_RANGE) % ACCUM_RANGE - 12;
            }
            accumOffset[step] = v;
          }

          int gateMode = (int)std::round(params[GATE_MODE_PARAM].getValue());
          float duty =
              clamp(params[DUR_PARAMS + step].getValue(), 0.01f, 0.95f);
          float period = clock.getVirtPeriod();
          float gLen = getGateLength(gateMode, duty, period, args.sampleTime);

          // Pulse count: right side (<0) of bipolar knob, only for pulse modes
          // PLAY/ACCUM always use a single gate regardless of knob position
          float ppVal = params[PROB_PARAMS + step].getValue();
          int pulseCount = 1;
          if (mode == SM_PULSE || mode == SM_GATED || mode == SM_HOLD) {
            pulseCount =
                (ppVal > 0.f) ? clamp(1 + (int)std::round(ppVal), 2, 8) : 1;
          }
          int newEffMode =
              stepEffMode(mode); // effective mode for this new step

          if (newEffMode == PM_HOLD) {
            holdPulsesLeft = pulseCount - 1;
            holdPlaying = true;
            gatePulse.trigger(gLen);
            stepGateTrig[k].trigger(gLen);
          } else if (newEffMode == PM_GATED && period > 0.f && pulseCount > 1) {
            // Gate sustained for N clock periods; ends TRIG_LEN before next
            // tick so the gate goes LOW briefly, allowing retrigger on the next
            // step
            float sustainLen =
                std::max(TRIG_LEN, (float)pulseCount * period - TRIG_LEN);
            // Same hard upper bound as getGateLength: prevent stuck gates.
            if (sustainLen > 8.f)
              sustainLen = 8.f;
            holdPulsesLeft = pulseCount - 1;
            holdPlaying = false;
            gatePulse.trigger(sustainLen);
            stepGateTrig[k].trigger(sustainLen);
          } else {
            // PM_PULSE (or PM_GATED with pulseCount==1): sub-gates within one
            // period
            holdPulsesLeft = 0;
            holdPlaying = false;
            if (pulseCount > 1 && period > 0.f) {
              float interval = period / (float)pulseCount;
              float pGLen = getGateLength(gateMode, duty, period,
                                          args.sampleTime, interval);
              pulseInterval = interval;
              pulseGLen = pGLen;
              pulseStepK = k;
              pulsesRemaining = pulseCount - 1;
              pulseTimer = interval;
              gatePulse.trigger(pGLen);
              stepGateTrig[k].trigger(pGLen);
            } else {
              gatePulse.trigger(gLen);
              stepGateTrig[k].trigger(gLen);
            }
          }
        } else {
          holdPulsesLeft = 0;
          holdPlaying = false;
          pulsesRemaining = 0;
          gatePulse.reset();
        }

        if (resetFiresAfterGate) {
          step = resetTargetStep;
          playCurrentOnNextTick = true;
          resetPending = false;
        }
      }
    }

    if (!clockNow && resetPending) {
      step = resetTargetStep;
      playCurrentOnNextTick = true;
      resetPending = false;
    }

    // Ratchet sub-pulses
    if (pulsesRemaining > 0) {
      pulseTimer -= args.sampleTime;
      if (pulseTimer <= 0.f) {
        gatePulse.trigger(pulseGLen);
        stepGateTrig[pulseStepK].trigger(pulseGLen);
        --pulsesRemaining;
        pulseTimer += pulseInterval;
      }
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

    float semis = params[PITCH_PARAMS + step].getValue();
    int octIv = (int)std::round(params[OCT_PARAMS + step].getValue());
    int accum = accumOffset[step];
    float pitchV =
        ((semis + (float)xposeSemis + (float)accum) / 12.f) + (float)octIv;

    outputs[M1_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(
        params[M1_PARAMS + step].getValue(), m1Range));
    outputs[M2_OUTPUT].setVoltage(UZZRanges::mapMod0_10ToRange(
        params[M2_PARAMS + step].getValue(), m2Range));

    applySlew(pitchV, args.sampleTime);

    if (updateLights) {
      for (int i = 0; i < 16; ++i)
        lights[STEP_LIGHTS + i].setSmoothBrightness(i == step ? 1.f : 0.f,
                                                    lightDt);
    }
  }
};

#include "uzz/UzzWidgets.hpp"

// ============================================================================
// Module Widget
// ============================================================================
struct UZZWidget : ModuleWidget {
  UZZWidget(UZZ *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/UZZ-light.svg"),
                         asset::plugin(pluginInstance, "res/UZZ.svg")));

    if (!UZZ_USE_CODE_LABELS)
      addChild(new UzzStaticOverlay());

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
      addParam(createParamCentered<ProbPulseKnob>(
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
      addChild(new AccumDisplay(Vec(xStep8 - dW * .5f, yBot - dH * .5f),
                                Vec(dW, dH), module));
    }

    if (UZZ_USE_CODE_LABELS) {
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

      // RATIO / STEPS / START → aligned with step 3, connector to X_CTRL1
      // knobs.
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
        auto *lbl = new TextLabel("UZZ", Vec(Xc(8) - w * .5f, yTop - h * .5f),
                                  Vec(w, h));
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
          auto *lbl = new TextLabel(text, Vec(xLbl10 - w * .5f, cy - h * .5f),
                                    Vec(w, h));
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

        // MODE: label(step10) ─── CKSS(step11) ─── GATE/TRIG Display(step12)
        // ─── GATE Output(step13)
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
        auto *lbl =
            new TextLabel(text, Vec(cx - w * .5f, cy - 13.f - h), Vec(w, h));
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

      addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT2, yTop),
                                                    module, UZZ::M1_OUTPUT));
      addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT2, yMid),
                                                    module, UZZ::M2_OUTPUT));
      addParam(createParamCentered<UzzArcKnob>(Vec(UI::X_OUT2, yBot), module,
                                               UZZ::PROB_GLOBAL_PARAM));

      addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT1, yTop),
                                                    module, UZZ::PITCH_OUTPUT));
      addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT1, yMid),
                                                    module, UZZ::GATE_OUTPUT));
      addOutput(createOutputCentered<UzzOutputPort>(Vec(UI::X_OUT1, yBot),
                                                    module, UZZ::EOC_OUTPUT));

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
    }

    // Hand-drawn Capybara from backup
    {
      auto *capi = new CapybaraWidget(module);
      // Centered in the sidebar (X=24)
      // Repositioned: Moved back to 308 as requested
      capi->box.pos = Vec(1.f, 280.f);

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

    appendPanelThemeMenu(menu);

    menu->addChild(new ui::MenuSeparator());
    menu->addChild(createCheckMenuItem(
        "EOC on reset", "", [m]() { return m && m->eocOnReset; },
        [m]() {
          if (m)
            m->eocOnReset = !m->eocOnReset;
        }));

    menu->addChild(createSubmenuItem("Direction mode", "", [m](ui::Menu *sub) {
      for (int i = DIR_MODE_MIN; i <= DIR_MODE_MAX; ++i) {
        std::string lbl = (i == DIR_JUMP && m)
                              ? string::f("Jump \xc3\xb7%d", m->jumpN)
                              : dirLabel(i);
        sub->addChild(createCheckMenuItem(
            lbl.c_str(), "",
            [m, i]() {
              if (!m)
                return false;
              return (int)std::round(
                         m->params[UZZ::DIR_MODE_PARAM].getValue()) == i;
            },
            [m, i]() {
              if (!m)
                return;
              m->params[UZZ::DIR_MODE_PARAM].setValue((float)i);
              m->navigator.pingDir = 0;
              m->navigator.drunkDir = 1;
              m->navigator.seqPos = 0;
            }));
      }
    }));

    menu->addChild(createSubmenuItem(
        "Jump stride", m ? string::f("\xc3\xb7%d", m->jumpN) : "",
        [m](ui::Menu *sub) {
          for (int n = 2; n <= 7; ++n) {
            sub->addChild(createCheckMenuItem(
                string::f("\xc3\xb7%d", n).c_str(), "",
                [m, n]() { return m && m->jumpN == n; },
                [m, n]() {
                  if (m) {
                    m->jumpN = n;
                    m->navigator.seqPos = 0;
                  }
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

    auto addRangeMenu = [&](const char *label, int *rangePtr) {
      menu->addChild(createSubmenuItem(label, "", [m, rangePtr](ui::Menu *sub) {
        for (int r = 0; r < UZZRanges::MR_COUNT; ++r) {
          sub->addChild(createCheckMenuItem(
              UZZRanges::RANGE_DEFS[r].label, "",
              [m, rangePtr, r]() { return m && *rangePtr == r; },
              [m, rangePtr, r]() {
                if (m)
                  *rangePtr = r;
              }));
        }
      }));
    };
    addRangeMenu("Range Mod 1", &m->m1Range);
    addRangeMenu("Range Mod 2", &m->m2Range);
  }
};

Model *modelUZZ = createModel<UZZ, UZZWidget>("UZZ");
