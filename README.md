# Animatek VCV Rack Plugin

Three modules for VCV Rack 2.x by **Javier Melgar (Animatek)**.

- **UZZ** — Ultimate Ztep Zequencer: a 16-step sequencer with per-row shift, probability, accumulator, and flexible clock.
- **OXI-CV** — 6HP MIDI-to-CV interface designed for the Oxi One controller.
- **OXI-CV EXPANSOR** — 10HP expander for OXI-CV with 8 configurable multi-track outputs.

---

## UZZ — Ultimate Ztep Zequencer

UZZ is a step sequencer originally created as a Max for Live device, designed for immediate control, clear visual feedback, and fluid musical flow.

This repository is a port to VCV Rack 2.x, aiming to preserve the original philosophy: precise timing, structured improvisation, and modular flexibility.

### Features

- 16 steps with per-step **Pitch**, **Octave**, **Duration**, **Mod1**, **Mod2**, and **Probability**
- Per-step **step mode**: Play, Mute, Skip, Accum Up, Accum Down
- **Active window**: configurable Start and Step count with wrap-around
- **Clock ratios**: ÷8 to ×8 and beyond, with swing
- **Direction modes**: Forward, Backward, Ping-Pong, Random, Drunk
- **Global glide (slew)** on pitch output
- **Accumulator**: semitone offset that accumulates on ACCUM steps
- **Per-row Randomize** buttons with CV trig inputs (Pitch, Oct, Mode, Dur, Mod1, Mod2, Prob)
- **Per-row Shift** up/down arrows for all rows including Probability
- **Per-step Probability** (0–100%) and **Global Probability** knob — multiplicative, gate only
- Polyphonic step-gate output (one channel per active step)
- EOC output, transpose input, reset input

---

## OXI-CV

A 6HP MIDI-to-CV converter designed for the **Oxi One** sequencer/controller, covering all its output modes.

### Features

- **V/Oct**, **Gate**, **Velocity** outputs
- **8× CC outputs** (configurable CC numbers per slot)
- **Clock output** and **CLK/n divisions** (÷1 to ÷32)
- **Run output**
- Supports **Mono, Poly, Chord, Multitrack and Matriceal** Oxi One modes
- Channel-selectable MIDI input (Omni or specific channel)
- Compact 6HP panel

---

## OXI-CV EXPANSOR

A 10HP expander for **OXI-CV** that unlocks its multi-track capabilities.

### Features

- **8 configurable tracks** with independent **V/Oct**, **Gate**, and **Velocity** outputs
- Works alongside OXI-CV in Multitrack and Matriceal modes
- Must be placed immediately to the right of OXI-CV

---

## Changelog

### 2.4.2 — Accumulator Wrap & Code Quality
*(2026-04)*

#### UZZ: Accumulator Wrap (modulo)
* Added **ACCUM WRAP** Trimpot (0–12 st) alongside the existing ACCUM AMT knob.
* When WRAP > 0, the accumulated pitch offset loops via **modulo**: e.g. WRAP=6 cycles the offset 0→1→2→3→4→5→0→... (50 % of an octave).
* WRAP=12 gives a full-octave cycle; WRAP=0 (default) preserves the original ±12 semitone behaviour.
* Works symmetrically for **ACCUM UP** and **ACCUM DOWN**.
* The **AccumDisplay** panel widget shows both values: semitone amount (top line) and wrap point or "OFF" (bottom line).
* Existing patches are unaffected (WRAP defaults to 0 = OFF).

#### Code quality (refactor, no behaviour change)
* Slug comparisons in `process()` replaced with model-pointer comparisons (avoids `std::string` allocation at audio rate).
* `noteToVoct()` helper added to `plugin.hpp`, removing 3 duplicated inline expressions across OXI-CV and OXI-CV EXPANSOR.
* `silenceAllVoices()` extracted in OXI-CV, replacing two copy-paste blocks.
* `fillRow()` helper added to UZZ, simplifying 7 `reset*Row()` methods.
* Dead code removed: unused lambdas and constants.

---

### 2.4.1 — Probability, New Modules & Label Polish
*(2026-04)*

#### UZZ: Per-Step and Global Probability
* Added **PROB row**: 16 Trimpot knobs (0–100 %, default 100 %) placed above the Pitch row, one per step.
* Added **Global PROB knob** in the bottom panel (bottom-right position, replaces old EOC slot).
* Probability is **multiplicative**: `p_final = p_step × p_global`. Only the gate is silenced on failure; pitch, Mod1/Mod2 and accumulator continue unaffected.
* Probability is evaluated **on every step arrival** using `random::uniform()`.
* Full row support: **Randomize PROB** button + CV trig input, **Shift PROB up/down** arrows (10 % per click), right-click **Reset PROB** to 100 %.
* New params appended at end of enum — existing patches default PROB to 100 % and play back unmodified.

#### UZZ: Bottom Panel Reorganisation
* STEP_GATES (poly) output moved to the **step 11 column** (bottom row).
* EOC output moved to the **step 13 column** (bottom row).
* Global PROB knob placed at **step 15 column** (bottom row) with PROB label at step 14.

#### UZZ: Label Rendering
* All panel labels now use `APP->window->uiFont` (Rack's system UI proportional font) rendered in `drawLayer(1)`, matching the look of OXI-CV.
* Fake-bold via 0.15 px horizontal pass, uppercase enforced at construction.
* Row labels (MODE, PITCH, OCT, DUR, MOD1, MOD2, PROB) repositioned above their trig input ports with consistent 3 px gap.

#### New Modules
* **OXI-CV**: 6HP MIDI-to-CV for Oxi One — V/Oct, Gate, Velocity, 8× CC, Clock, CLK divisions, Run. Mono/Poly/Chord/Multitrack/Matriceal modes.
* **OXI-CV EXPANSOR**: 10HP expander — 8 configurable tracks with V/Oct, Gate, Velocity outputs.

---

### 2.3.0 — Stability & Host Reload Fixes
*(2025-11)*

#### Core Fixes
* Fixed critical crash when reloading the Rack plugin inside Bitwig or other hosts.
* `dataToJson()` now creates its own `json_object()` when the base is `nullptr`.
* `dataFromJson()` calls the base method before applying stored values.
* UI asset safety during headless scans — constructors guard against null `APP->window`.

#### Timing & Clock Behavior
* Default clock ratio set to ×1.
* Virtual clock rewrite: internal oscillator only runs when ratio ≠ ×1.
* `havePhase` forced off at ×1 to prevent extra steps during held gates.

#### UI & Font Rendering
* Font loading uses `asset::system("res/fonts/ShareTechMono-Regular.ttf")` with `APP && APP->window` guards.
* Text rendering tuned: centered alignment, pixel-rounded coordinates, 9 px size.

#### SVG Loading Refactor
* Unified safe SVG loader routing all assets through a single helper.
* Step-mode icons and row-shift arrows render correctly after VST project reloads.

---

### 2.1.0 — Per-Row Shift & Menu Enhancements
*(2025-10-03)*

* Added per-row shift controls for Pitch, Octave, Duration, Mod1, and Mod2.
* Contextual menu: submenus for Direction mode, Range Mod1/Mod2, EOC-on-reset toggle.
* Implemented optional EOC trigger on external reset with persistence.
* Refined reset handling: active step fires before jumping to start window.
* Custom input/output port widgets (10 % scale reduction).
* Tuned slew to logarithmic curve.

---

### 2.0.5 — Clock Multipliers & Stability Fixes
*(2025-09-23)*

* Clock multipliers now trigger the first sub-tick correctly.
* Limited maximum multiplier to ×48.
* Old patches with ×64/×96 clamp to ×48 automatically.

---

## Links

- Website: <https://animatek.net>
- Manual: <https://animatek.net/ultimate-ztep-zequencer-vcvrack/>
- YouTube: <https://www.youtube.com/@animatek>
- Author: **Javier Melgar (Animatek)**
- License: **GPL-3.0-or-later**
