#pragma once

#include "UzzTypes.hpp"

struct StepNavigator {
  int pingDir = 0;
  int drunkDir = 1;
  int seqPos = 0;

  void reset() {
    pingDir = 0;
    drunkDir = 1;
    seqPos = 0;
  }

  static int igcd(int a, int b) {
    a = std::abs(a); b = std::abs(b);
    while (b) { int t = b; b = a % b; a = t; }
    return a ? a : 1;
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
                  bool &wrapped, bool &allSkip, int jumpN = 2) {

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
    } else if (dirMode == DIR_PENDULUM) {
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
    } else if (dirMode == DIR_PINGPONG) {
      int cycleLen = 2 * steps;
      int oldPos = seqPos;
      seqPos = (seqPos + 1) % cycleLen;
      int rel = (seqPos < steps) ? seqPos : (2 * steps - 1 - seqPos);
      nextStep = wrap16(start + rel);
      wrapped = (seqPos == 0 && oldPos != 0);
      allSkip = false;
    } else if (dirMode == DIR_ODD_EVEN) {
      int oldPos = seqPos;
      seqPos = (seqPos + 1) % steps;
      int half = (steps + 1) / 2;
      int rel = (seqPos < half) ? seqPos * 2 : (seqPos - half) * 2 + 1;
      nextStep = wrap16(start + rel);
      wrapped = (seqPos == 0 && oldPos != 0);
      allSkip = false;
    } else if (dirMode == DIR_JUMP) {
      int jn = clamp(jumpN, 2, steps > 1 ? steps - 1 : 1);
      int cycleLen = steps / igcd(steps, jn);
      int oldPos = seqPos % cycleLen;
      seqPos = (oldPos + 1) % cycleLen;
      int rel = (seqPos * jn) % steps;
      nextStep = wrap16(start + rel);
      wrapped = (seqPos == 0 && oldPos != 0);
      allSkip = false;
    } else if (dirMode == DIR_CONVERGE) {
      int oldPos = seqPos;
      seqPos = (seqPos + 1) % steps;
      int rel = (seqPos % 2 == 0) ? seqPos / 2 : steps - 1 - seqPos / 2;
      nextStep = wrap16(start + rel);
      wrapped = (seqPos == 0 && oldPos != 0);
      allSkip = false;
    } else if (dirMode == DIR_DIVERGE) {
      int oldPos = seqPos;
      seqPos = (seqPos + 1) % steps;
      int center = (steps - 1) / 2;
      int rel = (seqPos % 2 == 0) ? center - seqPos / 2
                                   : center + (seqPos + 1) / 2;
      nextStep = wrap16(start + rel);
      wrapped = (seqPos == 0 && oldPos != 0);
      allSkip = false;
    } else if (dirMode == DIR_RANDOM) {
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
    } else {
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
