#pragma once

#include "../plugin.hpp"

namespace UI {
static constexpr int COLS = 16;

static constexpr float LEFT = 48.f;
static constexpr float RIGHT = 1.f;
static constexpr float BOTTOM_MARGIN = 28.f;

static constexpr float Y_STEP_LED = 10.f;
static constexpr float Y_STEP_MODE = 10.f;
static constexpr float Y_NOTE = 30.f;
static constexpr float Y_PROB = 52.f;

static constexpr float ROW_START = 72.f;
static constexpr float ROW_SPACE = 48.f;
static constexpr float Y_PITCH = ROW_START;
static constexpr float Y_OCT = ROW_START + ROW_SPACE;
static constexpr float Y_DUR = ROW_START + ROW_SPACE * 2;
static constexpr float Y_C1 = ROW_START + ROW_SPACE * 3;
static constexpr float Y_C2 = ROW_START + ROW_SPACE * 4;

static constexpr float RAND_X = LEFT - 10.f;
static constexpr float RAND_BTN_SCALE = 0.90f;
static constexpr float RAND_BTN_X_OFFSET = 1.f;

static constexpr float SHIFT_X = RAND_X - 18.f;
static constexpr float SHIFT_X_OFFSET = 19.f;
static constexpr float SHIFT_Y_DELTA = 14.f;
static constexpr float SHIFT_Y_FINE = -1.f;
static constexpr float ROW_SHIFT_SCALE = 0.85f;

static constexpr float TRIG_LEFT_GAP = 23.f;
static constexpr float TRIG_RIGHT_PAD = 14.f;
static constexpr float PORT_SCALE = 0.90f;

static constexpr float BOT_DY_TOP = -54.f;
static constexpr float BOT_DY_MID = -24.f;
static constexpr float BOT_DY_BOT = 6.f;
static constexpr float X_IN = 124.96875f;
static constexpr float X_CTRL1 = 227.59375f;
static constexpr float X_CTRL2 = 381.53125f;
static constexpr float X_SWITCH = 586.78125f;
static constexpr float X_OUT1 = 689.40625f;
static constexpr float X_OUT2 = 792.03125f;

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
