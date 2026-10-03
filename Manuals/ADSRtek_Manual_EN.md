# ADSRtek User Manual

**Manual version:** 1.0

**Plugin version:** Animatek 2.5.9

**Module:** ADSRtek for VCV Rack

**Width:** 8 HP

---

## 1. Overview

**ADSRtek** is an ADSR and AD envelope modelled on measurements of the envelopes of a classic 90s virtual-analogue modular, the kind that made fast attacks and snappy decays its signature. It is not a generic envelope with a vintage label: its times, curves and behaviour come from recording the original step by step and fitting what came out.

What it does that most envelopes do not:

- **128-step times, as measured.** The knobs move through the original's 128 steps, from 0.5 ms to about 50 s, with the times it really takes.
- **Three attack shapes.** Log rushes up and eases in, Lin is a straight ramp, Exp creeps and then shoots up. Log and Exp bend differently at every step, as on the original.
- **Snappy exponential decays.** Decay and release are the same exponential, timed to reach 1% of the way.
- **Retriggers carry on.** A new gate never restarts from zero: the attack picks up from the current level, as fast as its curve is from there.
- **24 kHz control rate.** It moves in 24 kHz steps like the original, which can be switched off.

It is polyphonic (up to 16 channels, following `GATE`), has CV over every stage and a built-in VCA.

---

## 2. Quick start

1. Patch a gate to `GATE` and `ENV` to whatever you want to shape: a filter cutoff, a VCA.
2. Or patch the audio itself through `IN` and take it from `OUT`: the VCA is built in.
3. Start with a Lin attack at its lowest, `DECAY` near the middle, `SUSTAIN` at 0 and a short `RELEASE` for a pluck; raise `SUSTAIN` and slow the attack for a pad.
4. Flip the mode switch to **AD** for percussive sounds that play their whole shape from a short trigger.

---

## 3. Controls

### Mode: ADSR / AD

- **ADSR:** attack while the gate is high, decay to `SUSTAIN`, hold, and release when the gate falls. Releasing the gate during the attack or decay goes straight to release, from where the envelope is.
- **AD:** a trigger plays the attack to the top and the decay to zero, whatever the gate does afterwards. `SUSTAIN` and `RELEASE` do nothing in this mode. The context menu option **AD: gate release cuts the attack** makes it behave like the original's AD envelope with its gate option on: letting go of the gate during the attack goes straight to the decay.

### Attack shape: LOG / LIN / EXP

- **LOG:** fast at first, then eases into the top. The punchiest.
- **LIN:** a straight ramp.
- **EXP:** slow start, steep finish: swells.

The shape changes the curve and, slightly, the time: at the shortest settings an Exp attack never takes less than about 0.9 ms. The `ATTACK` knob's tooltip shows the real time for the selected shape.

### ATTACK, DECAY, RELEASE

Times in the original's 128 steps. The tooltip shows the time each one really takes: start to full scale for the attack, down to 1% for decay and release. They follow the original's published table to within half a percent in the median, except at the very top, where the measured times run long (an attack can take up to 50 s) and are used as they are. Between steps the knob interpolates, so it is smooth.

### SUSTAIN

The level held while the gate stays high, 0 to 100%. It follows the knob and its CV while held.

### Activity LED

Between the two switches. Its brightness is the level of the first channel's envelope.

---

## 4. Inputs and outputs

### CV: A, D, S, R

Polyphonic CV over each stage, added to its knob. For the times, **1 V moves 12.7 steps**, so 10 V sweeps all 128; for `SUSTAIN`, **1 V adds 10%**. Times are read continuously, so modulating a time during a stage changes its speed at once.

### GATE

Gate input. A rising edge (above 1 V, after going below 0.1 V) starts the attack; while it stays high in ADSR mode, the envelope sustains. Its channel count sets the module's polyphony.

### RETRIG

A rising edge restarts the attack from the current level. In ADSR mode it only acts while `GATE` is high; in AD mode it fires on its own.

### AMP

Scales the envelope, 0 to 10 V for 0 to 100%; unpatched, it is full scale. Patch a velocity CV here for velocity-sensitive envelopes. It applies to `ENV` and to the VCA.

### IN / OUT

The built-in VCA: `OUT` is `IN` times the envelope (after `AMP` and the inversion, if on).

### ENV

The envelope, 0 to 10 V.

---

## 5. Context menu

- **24 kHz control-rate steps** (on by default): the envelope moves on a 24 kHz clock and holds each value between ticks, as the original does. Off, it moves every sample. The difference is only audible on the very fastest attacks.
- **Invert envelope:** `ENV` and the VCA follow 1 minus the envelope, as on the original's invert switch: 10 V at rest, falling on a gate.
- **AD: gate release cuts the attack:** see the AD mode above.

All three are saved with the patch.

---

## 6. How it was made

The envelopes of the original were recorded at 96 kHz, every knob step of attack (in each shape), decay and release, plus retriggers, short gates and the AD envelope: some 650 recordings. From them:

- the attack times were taken per step and per shape, and the decay time constants per step;
- the Log and Exp attacks were found to be level-driven: an exponential approach to a target above full scale, cut on reaching it, and an exponential growth from zero. One number per step, fitted to the recordings, reproduces them to within 0.2% (Log) and 0.6% (Exp) of full scale, except for Exp attacks of a few hundred microseconds, too short to show a shape;
- the module was then run against the original on the same settings and gates: plucks, pads, swells, short gates, retriggers in the release, AD triggers and gates. The worst error was between 0.06% and 1.1% of full scale in all but one case, a 0.46 ms attack, which is off by a fraction of one 24 kHz tick.

The recordings could not resolve anything below about −76 dB. There the envelope settles on its target; whatever the original does that far down, it is inaudible.

---

## 7. Patch examples

### Plucked bass

Mode ADSR, attack LOG at 0, `DECAY` around 0.2 s, `SUSTAIN` 0, `RELEASE` short. `ENV` to the filter cutoff, `IN`/`OUT` around the oscillator.

### Swelling pad

Attack EXP at around 1 s, `DECAY` long, `SUSTAIN` high, `RELEASE` 2-3 s. Retriggering it in the release swells back from where it is, without a dip.

### Percussion

Mode AD, attack LIN at 0, `DECAY` 50-150 ms, triggered by a sequencer. Patch velocity into `AMP`.
