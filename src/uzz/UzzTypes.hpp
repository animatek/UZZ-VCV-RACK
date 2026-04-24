#pragma once

#include "../plugin.hpp"

static inline int wrap16(int x) { return x & 15; }

enum StepMode {
  SM_PLAY = 0,
  SM_MUTE = 1,
  SM_SKIP = 2,
  SM_ACCUM_UP = 3,
  SM_ACCUM_DOWN = 4,
  SM_PULSE = 5,
  SM_GATED = 6,
  SM_HOLD = 7
};

static constexpr int NUM_RND_BANKS = 7;
static constexpr int NUM_SHIFT_ROWS = 6;
static constexpr float TRIG_LEN = 0.010f;

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

enum DirectionMode {
  DIR_PENDULUM = -2,
  DIR_REV = -1,
  DIR_FWD = 0,
  DIR_RANDOM = 1,
  DIR_DRUNK = 2,
  DIR_PINGPONG = 3,
  DIR_ODD_EVEN = 4,
  DIR_JUMP = 5,
  DIR_CONVERGE = 6,
  DIR_DIVERGE = 7
};
static constexpr int DIR_MODE_MIN = -2;
static constexpr int DIR_MODE_MAX = 7;

static inline const char *dirLabel(int v) {
  switch (clamp(v, DIR_MODE_MIN, DIR_MODE_MAX)) {
  case -2: return "Pendulum";
  case -1: return "Backward";
  case  0: return "Forward";
  case  1: return "Random";
  case  2: return "Drunk";
  case  3: return "Ping-Pong";
  case  4: return "Odd/Even";
  case  5: return "Jump";
  case  6: return "Converge";
  default: return "Diverge";
  }
}

static inline const char *dirShort(int v) {
  switch (clamp(v, DIR_MODE_MIN, DIR_MODE_MAX)) {
  case -2: return "PND";
  case -1: return "BWD";
  case  0: return "FWD";
  case  1: return "RND";
  case  2: return "DRK";
  case  3: return "P.P";
  case  4: return "O/E";
  case  5: return "JMP";
  case  6: return "CVG";
  default: return "DVG";
  }
}

struct DirModeQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    return dirLabel(clamp((int)std::round(getValue()), DIR_MODE_MIN, DIR_MODE_MAX));
  }
  std::string getUnit() override { return ""; }
};

struct ProbPulseQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    float v = getValue();
    if (v > 0.f)
      return string::f("×%d", clamp(1 + (int)std::round(v), 2, 8));
    return string::f("%d", clamp(100 + (int)std::round(v), 0, 100));
  }
  std::string getUnit() override {
    return (getValue() > 0.f) ? "" : "%";
  }
};

struct DurationQuantity : ParamQuantity {
  std::string getDisplayValueString() override {
    int pct = clamp((int)std::round(getValue() * 100.f), 1, 95);
    return string::f("%d", pct);
  }
  std::string getUnit() override { return "%"; }
};

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
