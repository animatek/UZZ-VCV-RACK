#pragma once

#include "../plugin.hpp"

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
