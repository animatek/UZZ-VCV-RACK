# FILTERtek User Manual

**Manual version:** 1.0

**Plugin version:** Animatek 2.5.9

**Module:** FILTERtek for VCV Rack

**Width:** 8 HP

---

## 1. Overview

**FILTERtek** is a multimode filter modelled on measurements of the multimode filter of a classic 90s virtual-analogue modular. Like **ADSRtek**, it comes from recording the original and fitting what came out, not from its code.

- **Four types:** lowpass, bandpass, highpass and band reject, at **12 or 24 dB/octave**.
- **Its resonance**, from a gentle bump to a ringing peak with a Q of several thousand, with the original's **gain control**, which pulls the level down as the resonance rises.
- **Its saturation:** the original computes in fixed point, and its internal values hit a hard ceiling. Driven into resonance it clips in a way that is its own, and FILTERtek reproduces it.
- **Stereo**: two filters with the same settings, `IN R` normalled to `IN L`. Polyphonic on each side (up to 16 channels).
- A **DRIVE** to push it into that saturation, and CV with its own trimmer over cutoff, resonance and drive, plus V/oct.

---

## 2. Quick start

1. Patch an oscillator into `IN L` and take `OUT L` to a VCA or the mix; for a stereo source, use both sides.
2. Turn `CUTOFF` to open or close the filter, `RES` to add resonance, `DRIVE` to make it bite.
3. Patch an envelope (ADSRtek, for instance) into the `CUT` CV jack and turn its trimmer up for a filter sweep.
4. Patch the keyboard's pitch into `V/OCT` so the filter follows the notes.

---

## 3. Controls

### CUTOFF

The cutoff, in the original's 128 steps of a semitone each: **330 Hz at step 60**, from about 10 Hz at 0 to 15.8 kHz at 127. The tooltip shows it in Hz. (The filter really sits about 2 cents below that figure, as the original does.)

### RES

The resonance, in 128 steps. The Q starts at 0.5 (no peak at all) and grows faster and faster: about 2 at 64, 7 at 96, 27 at 112, 100 at 120, and several thousand at 127, where the filter rings for most of a second without quite oscillating by itself. Its damping also follows the cutoff, as on the original: at the same `RES` the peak gets sharper higher up.

### Type button and LEDs

The button steps through **LP**, **BP**, **HP** and **BR**; the LED shows which one is on. The type is also in the context menu.

- **LP, BP, HP** are the three outputs of the same filter. The bandpass is not normalised: its peak rises with the resonance.
- **BR** (band reject) is a notch with a damping of its own, much gentler than the resonance: `RES` narrows it only a little.

### SLOPE: 12 / 24

12 dB/octave is one filter section; 24 dB/octave is two identical sections in cascade, each with its own, lower resonance, so a 24 dB peak is softer than a 12 dB one at the same `RES`.

### GAIN: ON / OFF

The original's gain control. **On** (the default), the level falls as the resonance rises, down to about −40 dB at full resonance, so the peak stays in check. **Off**, the passband stays at full level and the peak towers over it, which drives the filter into its saturation much sooner.

At 12 dB the gain control acts on the input; at 24 dB it acts between the two sections, so the first one always takes the full signal. That is why a loud signal through a resonant 24 dB filter breaks up even with gain control on, as it does on the original.

### DRIVE

Level into the filter, from 0 to +24 dB, to push it into its saturation. At 0 dB it is the original exactly. Half of it is made up for at the output: a quiet signal gets somewhat louder as you turn it up (+12 dB at the top), and a filter driven all the way into clipping comes out at about ±5 V, Rack's usual level, rather than at its internal ceiling of ±20 V.

### CV trimmers: CUT, RES, DRIVE

Each CV jack has its trimmer above it, joined by a hairline: how much and in which direction the CV moves the cutoff, the resonance or the drive, from −100% to +100%. At 0 the jack does nothing.

---

## 4. Inputs and outputs

### IN L / IN R, OUT L / OUT R

Audio in and out, two filters with the same settings. `IN R` is normalled to `IN L`, so a mono source comes out on both sides. The level matters: the filter saturates at about **±20 V** inside, where the original's full scale is, and a normal ±5 V oscillator sits where one sits on the original. Hotter signals, `DRIVE`, or `GAIN` off push it into its saturation. With `GAIN` off and a loud signal right on a sharp resonance, the output really does reach that ±20 V, as the original's does: turn the level down after it, or leave gain control on.

### V/OCT

Moves the cutoff 1 V per octave, with no trimmer: patch the pitch of a voice here so the filter tracks the keyboard.

### CUT

Moves the cutoff exponentially, scaled by its trimmer: at 50% it is 1 V/octave, at 100% 2 octaves per volt (the original's amount at full).

### RES

Moves the resonance, scaled by its trimmer: at 100%, 1 V is 24 steps.

### DRIVE

Moves the drive, scaled by its trimmer: at 100%, 1 V is 2.4 dB, within −12 to +36 dB in all.

---

## 5. How it was made

The original's filter was measured with noise, sines, impulses and saws, every cutoff step and every resonance step, both slopes, all four types, with and without gain control and at rising levels. What it is, as far as the measurements go:

- a **Chamberlin state-variable filter**, which matches the recordings to 0.02 dB and a degree of phase across the whole range, with its cutoff following the editor's table and its damping following the cutoff;
- **24 dB as two identical sections**, not a ladder;
- **saturating arithmetic**: each internal value is limited to full scale when stored. With that, the module matches the gain and the harmonics of the original driven into clipping to within a few hundredths of a dB.

The module was then run against the original on the same signals: ten cases covering all types, both slopes, high resonance and heavy saturation. The difference stayed between 33 and 55 dB below the signal in nine of them. In the tenth, a 24 dB filter at resonance 124 driven hard into clipping, the waveforms drift apart, as a filter that close to oscillation does with the slightest difference, but its spectrum matches to half a dB in the median.

Below about 100 Hz with very high resonance the peak was too narrow to measure precisely; the tables there come from the same rules, measured where they could be.
