# Animatek VCV Rack Plugin

Eight modules for VCV Rack 2.x by **Javier Melgar (Animatek)**.

- **UZZ** — Ultimate Ztep Zequencer: a 16-step sequencer with per-row shift, probability, accumulator, and flexible clock.
- **UZZ-X** — 6HP CV expander for UZZ: bipolar offsets for steps, start, direction, ratio, swing, probability and accumulator, plus absolute step addressing, rotation triggers and a reverse gate.
- **CAP** — 6HP ducking VCA with sidechain envelope: stereo audio in/out, humanised per hit so the ducking breathes, and self-patchable into a jittered LFO.
- **ONE** — 6HP MIDI-to-CV interface designed for the Oxi One controller.
- **MULTI** — 10HP expander for ONE with 8 configurable multi-track outputs.
- **APC40 CTRL** — 13HP MIDI-to-CV bridge for the Akai APC40, with all knobs, faders and the master fader on a single panel.
- **UNIT-D** — deterministic polyphonic generative sequencer based on unit-distance graph walks. Manuals: [EN](Manuals/UNIT-D_Manual_EN.md) / [ES](Manuals/UNIT-D_Manual_ES.md).
- **BLANK 3** — 3HP blank panel with animated low-opacity Animatek logo pattern and global pause option.

---

## UZZ — Ultimate Ztep Zequencer

UZZ is a step sequencer originally created as a Max for Live device, designed for immediate control, clear visual feedback, and fluid musical flow.

This repository is a port to VCV Rack 2.x, aiming to preserve the original philosophy: precise timing, structured improvisation, and modular flexibility.

### Features

- 16 steps with per-step **Pitch**, **Octave**, tempo-relative **Duration**, **Mod1**, **Mod2**, and bipolar **Prob/Pulse**
- Per-step **step mode**: Play, Mute, Skip, Accum Up, Accum Down, Pulse, Gated, Hold
- **Active window**: configurable Start and Step count with wrap-around
- **Clock ratios**: ÷8 to ×8 and beyond, with swing
- **Direction modes**: Forward, Backward, Pendulum, Ping-Pong, Random, Drunk, Odd/Even, Jump, Converge, Diverge
- **Global glide (slew)** on pitch output
- **Accumulator**: semitone offset that accumulates on ACCUM steps, with wrap control
- **Per-row Randomize** buttons with CV trig inputs (Pitch, Oct, Mode, Dur, Mod1, Mod2, Prob)
- **Per-row Shift** up/down arrows for all rows including Prob/Pulse
- **Per-step Prob/Pulse** knob: probability on the left side, pulse multiplier on the right side
- **Global Probability** knob — multiplicative on gate triggering
- Polyphonic step-gate output (one channel per active step)
- EOC output, transpose input, reset input

---

## ONE

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

## MULTI

A 10HP expander for **ONE** that unlocks its multi-track capabilities.

### Features

- **8 configurable tracks** with independent **V/Oct**, **Gate**, and **Velocity** outputs
- Works alongside ONE in Multitrack and Matriceal modes
- Must be placed immediately to the right of ONE
- Mirrors the current ONE track configuration through the expander link

---

## APC40 CTRL

A 13HP MIDI-to-CV bridge that captures every continuous controller of the **Akai APC40 MkI/MkII** on a single panel. Designed to sit next to **MULTI**: title, header divider and the first knob share the same vertical position so both modules read as a pair.

### Features

- **8 Track Controls** (T1–T8) on **CC 48–55** with per-output attenuverters.
- **8 Device Knobs** (D1–D8) on **CC 16–23** with per-output attenuverters.
- **CUE level** (CC 47), **MASTER fader** (CC 14) and **XFAD crossfader** (CC 11), each with attenuverter.
- **8 Channel Faders** (F1–F8) in a dedicated column on the right: all share **CC 7** but on **MIDI channels 1–8**, so the eight faders of the APC40 reach Rack as eight independent CV outputs without remapping the controller.
- All knob/CC controls listen on **MIDI channel 1**; the fader column accepts channels 1–8 in parallel.
- 0 → 10 V output range. Use the per-control attenuverter to scale or invert as needed; faders pass through directly.

---

## CAP

A 6HP trigger-fired ducking VCA. Feed it the same trigger that fires the kick, run the audio through it, and the signal drops and recovers on every hit — no compressor involved. The VCA is built in, so nothing else is needed, but **ENV** still carries the envelope as CV for whatever else you want to duck in step.

What separates it from any inverted envelope patched by hand is that **each hit is slightly different from the last**, so the ducking breathes like an analogue compressor instead of stamping an identical curve forever.

### Features

- **RECOVERY** (40 ms – 1 s, exponential), **DEPTH** (0–100 %) and **JITTER** (0–100 %) down the left, with the **LEVEL slider** filling the right.
- The slider is also the **meter**: the bar draws the gain actually applied, so the duck is visible on every hit, and the handle sets the VCA ceiling. At 100 % (the default) the audio passes untouched at rest.
- **Stereo audio path**: patch **IN L** and **IN R** for true stereo, or just **IN L** — right is normalled to left, so one cable turns it into a mono-to-stereo ducker. Unity gain at rest.
- **Humanisation** across three dimensions per hit: recovery time (±50 %), depth (±25 %) and curve exponent (±30 %) at full jitter, scaled linearly by the knob. At JITTER = 0 the module is perfectly deterministic and repeatable.
- The variation is a **correlated random walk**, not white noise: each hit relates to the previous one, which is what makes it read as human rather than as random.
- By default every audio channel shares one envelope, so a stereo pair ducks symmetrically and the image does not wobble. **Per-channel envelopes** in the menu gives each channel its own generator instead — for ducking several unrelated tracks through one polyphonic cable.
- Fixed 2 ms fall and 12 ms hold at the floor, so retriggering mid-recovery never clicks or steps the level upward.
- **ENV** rests at 10 V and dips, following TRIG's channel count. Handy for ducking a reverb return or driving a filter in step with the audio path.
- **EOC** fires a 1 ms trigger when the recovery completes and the envelope is back at rest. A retrigger that cuts the recovery short fires nothing, so EOC always means "the duck has fully released" rather than turning into a copy of TRIG at fast tempos.
- Layout: knobs down the left with the **TRIG** jack closing the column, the meter-slider alongside them, then **D-CV**, and the audio and CV jacks below — **IN L · IN R**, **OUT L · OUT R**, **ENV · EOC**. Two hairlines tie the trigger group together: one from the TRIG jack across to the slider, one down to the **manual trigger button**, which carries no label because the line already says what it is.
- **Self-patch EOC into TRIG** and one press of the button sets it free-running: the cycle is 2 ms fall + 12 ms hold + recovery, so it oscillates from about 1 Hz to 18.5 Hz — a function generator whose every cycle differs, which a plain LFO cannot do.
- **DEPTH CV** input, summed with the knob (10 V = 100 %) and clamped.
- Context menu: **recovery curve** (exponential / linear / logarithmic), **freeze jitter** for A/B comparison, **per-channel envelopes**, **reset jitter seed**, and the standard **Panel** theme menu.

---

## UNIT-D

A compact deterministic generative sequencer based on unit-distance graph walks. UNIT-D creates a seeded 2D cloud of nodes, connects nodes that sit near a target distance, and turns the graph traversal into quantized pitch, gates, accents and related X/Y modulation. The same seed and settings always produce the same structure, so it can move between generative exploration and repeatable musical phrases. Manuals: [EN](Manuals/UNIT-D_Manual_EN.md) / [ES](Manuals/UNIT-D_Manual_ES.md).

### Features

- Seeded graph generation with **8–64 nodes**, **unit radius**, tolerance fine-tuning and musical density control.
- Deterministic walker with selectable **Walk mode**, resettable state and repeatable behaviour for saved patches.
- **Bipolar LOCK** control for gradual 16 / 32-step phrase capture, including forward and reverse lock directions.
- Quantized minor-scale **V/oct** output with selectable **1–4 octave** range.
- Tempo-following **Gate** output with gate length and gate-density controls.
- Related **Accent**, **X CV** and **Y CV** outputs derived from graph degree and node position.
- Polyphonic operation from the context menu: **1–8 voices**, with shared seed or per-voice seed graphs.
- Inputs: **Clock**, **Reset**, **Seed CV** and **Density CV**.
- Outputs: **V/oct**, **Gate**, **Accent**, **X CV** and **Y CV**.
- **Panel theme** menu (Dark / Light).

---

## BLANK 3

A 3HP blank panel for filling small gaps in a Rack patch while keeping the Animatek visual language.

### Features

- 3HP utility blank with **dark / light** panel artwork.
- Subtle animated background pattern using the Animatek logo.
- Context-menu **Pause animation** option.
- **Panel theme** menu (Dark / Light).

---

## Changelog

### 2.5.3 — UNIT-D & Blank Panel
*(2026-05)*

#### New module: UNIT-D
* Added **UNIT-D**, a compact deterministic polyphonic generative sequencer based on unit-distance graph walks.
* Seeded 2D nodes create graph transitions with controls for node count, radius, density, tolerance, walk mode, range, gate length, gate density and lock.
* Outputs include quantized **V/O**, **GATE**, **ACC**, **X CV** and **Y CV** for linked melody, gates, accents and modulation.
* Added expanded manuals in English and Spanish: [`Manuals/UNIT-D_Manual_EN.md`](Manuals/UNIT-D_Manual_EN.md) and [`Manuals/UNIT-D_Manual_ES.md`](Manuals/UNIT-D_Manual_ES.md).

#### New module: BLANK 3
* Added **BLANK 3**, a 3HP blank panel with Animatek branding.
* Background logos are generated per instance with randomized position, size, rotation and opacity.
* Logos drift with a very slow random walk by default.
* Added right-click **Pause animation** option, global across all BLANK 3 instances, to freeze animation and reduce idle work.

#### Branding and panel polish
* Public display names changed from **OXI-CV** to **ONE** and from **OXI-CV EXPANSOR** to **MULTI**.
* Added bottom-panel module labels using the same blue label style as UZZ.
* Updated custom Animatek input/output port assets across the public modules.

---

### 2.5.2 — APC40 CTRL & UZZ cleanup
*(2026-05)*

#### New module: APC40 CTRL
* **13HP MIDI-to-CV** bridge for the **Akai APC40** with the full continuous-controller surface on one panel: 8 track controls, 8 device knobs, Cue, Master and Crossfader (with attenuverters), plus a separate column of **8 channel faders** that listen to **CC 7 on MIDI channels 1–8** directly.
* Layout aligned with **OXI-CV EXPANSOR** so the two modules share title height, header divider position and first-knob row when placed side by side.

#### UZZ: Old overlay panel removed
* Deleted `res/UZZ-overlay.svg` / `res/UZZ-overlay-light.svg` and the unused `UzzStaticOverlay` widget. Older VCV versions could render the obsolete overlay on top of the current panel; the dead code path is now gone.

#### Code quality (refactor, no behaviour change)
* New `BasicDisplay` base widget shared by `ParamDisplay` and `AccumDisplay` — drops ~25 lines of duplicated background/font setup.
* New `loadPluginSvgOr(plugin, fallback)` helper used by `RowShiftButton` and `StepModeButton` to replace the hand-rolled load-with-fallback pattern.
* The five identical 16-step `UzzArcKnob` loops in `UZZWidget` collapsed into a single `addArcKnobRow(y, paramBase)` lambda.
* Hardcoded display blue replaced with the existing `displayBlue()` helper from `CommonWidgets`.
* Dead `UZZ_USE_CODE_LABELS` macro and its always-true branch removed.

---

### 2.5.1 — UZZ Probability & Clock Robustness
*(2026-04)*

#### All modules: Panel theme menu
* Added **Panel → Light / Dark** submenu to the right-click context menu of **UZZ**, **OXI-CV** and **OXI-CV EXPANSOR**.
* Toggles VCV Rack's global `preferDarkPanels` setting directly from the module, so the theme can be switched without opening Rack's global preferences.

#### UZZ: Probability in Pulse/Gated/Hold
* The per-step **Prob/Pulse** knob now applies probability on its **left side** (negative values) for **Pulse**, **Gated** and **Hold** steps too — previously these modes only honoured the global probability.
* Right side of the knob keeps its meaning: pulse count, always at 100 % probability per repetition.

#### UZZ: Clock — fix stuck-gate at start
* Fixed a case where the **Gate output** could stay high for several seconds (or indefinitely) when the upstream clock was held at 10 V or had been connected long before play started.
* Internal clock now ignores the first edge's accumulated time and rejects implausibly long periods (>5 s ≈ 12 BPM) instead of feeding them into the gate-length calculation.
* Schmitt trigger uses explicit `0.1 V / 2 V` thresholds (matching Fundamental SEQ3) for cleaner edge detection on signals that idle slightly above 0 V.
* Added hard upper bounds: gate length capped at 2 s, Gated sustain capped at 8 s — even with pathological state the gate cannot stay open indefinitely.

---

### 2.5.0 — UZZ Pulse Modes & Advanced Directions
*(2026-04)*

> Kept as a Rack 2.x-compatible `2.x` release so VCV Rack 2 loads the plugin.

#### UZZ: New Step Modes
* Added three per-step modes: **Pulse**, **Gated**, and **Hold**.
* **Pulse** generates ratcheted sub-gates inside the clock period.
* **Gated** can stretch one step across multiple clock periods.
* **Hold** can re-fire the current step across multiple incoming clocks before advancing.
* Added dedicated panel icons for the new step modes.

#### UZZ: Bipolar Prob/Pulse Row
* The former **Probability** row is now **Prob/Pulse**.
* Knob center (`0`) means normal playback: **100 % probability / ×1 pulse**.
* Turning left sets per-step probability from **100 % down to 0 %**.
* Turning right sets pulse multiplication from **×2 to ×8**.
* Randomize, Shift and Reset for the row were updated to the new bipolar behavior.
* For **Play** and **Accum** steps, the row acts as probability.
* For **Pulse**, **Gated**, and **Hold** steps, the **left side** still acts as per-step probability and the **right side** as pulse count (always 100 %).

#### UZZ: New Direction Modes
* Added **Pendulum**, **Odd/Even**, **Jump**, **Converge**, and **Diverge** direction modes.
* **Ping-Pong** now exists as its own mode, distinct from **Pendulum**.
* **Jump** uses a configurable stride from the right-click context menu.
* Sequencer state now stores the advanced direction position so patches resume consistently.

#### UZZ: Gate/Timing Behavior
* The **Duration** row is now tempo-relative duty, displayed as percent of the clock step instead of independent seconds.
* Gate length calculation was centralized to keep pulses and gated playback consistent across ratios.
* In **Gate** mode, ratcheted repeats now use the repeat interval as their timing window so each repeat has a clear low gap before the next gate.
* Ratchet sub-pulses are scheduled inside the audio process loop rather than treated as separate steps.
* Hold/gated states are cleared cleanly on hard stop and reset.

#### UZZ: Panel Polish
* Group guide lines now sit between steps **4/5**, **8/9**, and **12/13**.
* Group guide lines use the Animatek logo blue with reduced opacity.
* The row label for the bipolar probability/pulse controls is shown as **PROB** to fit the panel.

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

## Build

Build against the VCV Rack 2 SDK:

```bash
RACK_DIR=/path/to/Rack-SDK make
```

Install the plugin into your local Rack setup:

```bash
RACK_DIR=/path/to/Rack-SDK make install
```

Create a distributable package:

```bash
RACK_DIR=/path/to/Rack-SDK make dist
```

---

## Links

- Website: <https://animatek.net>
- Manual: <https://animatek.net/ultimate-ztep-zequencer-vcvrack/>
- YouTube: <https://www.youtube.com/@animatek>
- Author: **Javier Melgar (Animatek)**
- License: **GPL-3.0-or-later**
