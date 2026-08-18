# UZZ and UZZ-X User Manual
**Manual version:** 1.0

**Modules:** UZZ and UZZ-X for VCV Rack

**Plugin version:** Animatek 2.5.5

**UZZ format:** 16-step, external-clock sequencer

**UZZ-X format:** CV and trigger expander for UZZ
---

## 1. Overview
UZZ is a 16-step sequencer for pitch, gates, and two modulation lanes.
It combines conventional sequencing with per-step probability, ratchets,
accumulation, selectable active windows, and ten direction modes.
Each physical step has seven programmable rows:
- `MODE`
- `PITCH`
- `OCT`
- `DUR`
- `MOD1`
- `MOD2`
- `PROB`
UZZ has no internal tempo generator.
It only advances when a cable is connected to `CLK` and an external clock is
present. Its clock processor can divide or multiply that clock with `RATIO`,
then apply `SWING` to the resulting sequence ticks.
UZZ-X adds voltage control over the active window, direction, clock ratio,
swing, probability, and accumulator amount. It also adds absolute step
addressing, sequence rotation, accumulator reset, and a reverse gate.
---

## 2. Quick start
1. Add UZZ to the rack.
2. Patch a clock to `CLK`.
3. Patch `V/OCT` to an oscillator's pitch input.
4. Patch `GATE` to an envelope or voice gate input.
5. Patch the envelope through a VCA so the sequence controls amplitude.
6. Set `RATIO` to `×1`, `STEPS` to `16`, `START` to `1`, and `DIR` to
   `Forward`.
7. Leave the global `PROB` control at `100%`.
8. Enter notes with the `PITCH` and `OCT` rows.
9. Adjust `DUR` for gate length and use `MOD1` or `MOD2` for timbre changes.
10. Patch a trigger to `RESET` if UZZ must restart with the rest of the patch.
Useful first settings:
```text
MODE: Play on all steps
PITCH: 0..11 semitones
OCT: 0
DUR: 50%
PROB: 100%
RATIO: ×1
STEPS: 16
START: 1
DIR: Forward
SWING: 0%
ACUMM amount: 1 st
ACUMM wrap: OFF
SLEW: 0 s
MODE switch: GATE
```
---

## 3. Active window and step layout
The panel always contains 16 physical steps, numbered 1 through 16.
`START` and `STEPS` define which of those steps form the active window.
### STEPS
- Range: `1` to `16` steps.
- Default: `16`.
- Sets the number of active positions.
### START
- Range: step `1` to `16`.
- Default: step `1`.
- Sets the first physical step in the active window.
The active window wraps around the end of the panel. For example,
`START = 13` and `STEPS = 8` selects physical steps
`13, 14, 15, 16, 1, 2, 3, 4`.
Direction modes, row shifts, UZZ-X rotation, `ADDR`, and the poly output all
operate in relation to this active window.
The blue step light identifies the currently selected physical step.
---

## 4. Complete per-step row reference
### MODE
Press a step's mode button repeatedly to select one of eight behaviors:
```text
Play, Mute, Skip, Accum Up, Accum Down, Pulse, Gated, Hold
```
The modes are described fully in section 5.
The default for every step is `Play`.
### PITCH
- Default range: `0` to `11` semitones.
- Optional context-menu range: `0` to `23` semitones.
- Default value: `0` semitones.
- Values are quantized to whole semitones.
The displayed note name combines `PITCH` with `OCT`.
Pitch output also includes transpose and the step's accumulator offset.
### OCT
- Range: `-2` to `+2` octaves.
- Default: `0` octaves.
- Values are quantized to whole octaves.
### DUR
- Range: `1%` to `95%` of one effective sequence period, subject to the `2 s`
  maximum gate length described in the timing section.
- Default: `50%`.
In global `GATE` mode, this sets the active gate fraction.
In global `TRIG` mode, output pulses are fixed at approximately `10 ms`, so
`DUR` does not lengthen the trigger.
### MOD1 and MOD2
- Knob range: `0` to `10`.
- Default knob value: `0`.
- Default output mapping: `0V` to `10V`.
Each lane has an independent voltage range in UZZ's context menu.
The selected range maps the knob's full `0..10` travel to that full voltage
range.
### PROB
This bipolar per-step control has two functions.
- Center and left side: gate probability from `100%` down to `0%`.
- Center/default: `100%`.
- Right side: pulse count `×2` through `×8`.
- The pulse count is used only by `Pulse`, `Gated`, and `Hold` step modes.
- A positive pulse-count setting has `100%` per-step probability.
- `Play`, `Accum Up`, and `Accum Down` always use one gate even when this
  control is on its pulse-count side.
The effective chance of playing is the per-step probability multiplied by the
global probability.
---

## 5. Step modes
### Play
The step updates pitch and modulation and produces one gate or trigger when its
probability test succeeds.
### Mute
The position remains part of the sequence and consumes its normal time, but it
does not produce a gate. Pitch and modulation still follow the selected step.
### Skip
In `Forward`, `Backward`, `Pendulum`, `Random`, and `Drunk`, the navigator
bypasses Skip positions without assigning them sequence time.
In `Ping-Pong`, `Odd/Even`, `Jump`, `Converge`, and `Diverge`, Skip positions
are still selected by the programmed order. They are silent, but they consume
their position in that order.
Connected UZZ-X `ADDR` also selects Skip positions directly. Such addressed
positions are silent.
### Accum Up
When the probability test succeeds, the step adds the current `ACUMM` amount
to its own stored pitch offset, then plays.
### Accum Down
When the probability test succeeds, the step subtracts the current `ACUMM`
amount from its own stored pitch offset, then plays.
Each physical step has an independent accumulator. Accumulation affects pitch,
not `PITCH`, `OCT`, `MOD1`, or `MOD2` knob positions.
### Pulse
With `PROB` on `×2` through `×8`, Pulse distributes that many gates evenly
inside one effective sequence period. The sequencer advances after that period.
### Gated
With `PROB` on `×2` through `×8`, Gated sustains one gate across that many
clock periods, up to a maximum of `8 s`. The current step remains selected while those periods are
consumed. A short low interval is left before the following step so it can
retrigger.
### Hold
With `PROB` on `×2` through `×8`, Hold keeps the step selected for that many
clock periods and fires again on each period. Gate or trigger length still
follows the global `GATE`/`TRIG` choice.
For `Pulse`, `Gated`, and `Hold`, a probability-side value uses one pulse.
---

## 6. Row tools
Every row has a random button and trigger input. `PITCH`, `OCT`, `DUR`,
`MOD1`, `MOD2`, and `PROB` also have down/up shift buttons.
### Random buttons and inputs
- Single-click a row's random button to randomize all 16 physical steps.
- Trigger the jack beside a row to randomize that row by CV.
- Double-click a random button to reset that row instead of randomizing it.
Double-click reset values are:
```text
MODE: Play
PITCH: 0 st
OCT: 0 oct
DUR: 50%
MOD1: 0
MOD2: 0
PROB: 100%
```
Pitch randomization respects the selected one- or two-octave pitch range.
Octave randomization uses `-2..+2`. Duration randomization stays approximately
within `10..90%`. Mod lanes use their full `0..10` knob range.
### Row shifts
The down/up buttons change every value inside the current active window:
- `PITCH`: one semitone.
- `OCT`: one octave.
- `DUR`: nominally five percentage points, quantized to a grid anchored at 1%;
  for example, 50% moves to 56% upward or 46% downward.
- `MOD1` and `MOD2`: one knob unit.
- `PROB`: one discrete probability/pulse setting.
Values stop at the row limit; value shifts do not wrap. Steps outside the
active window are unchanged. These buttons change values, unlike UZZ-X
`ROT -` and `ROT +`, which move complete step data between positions.
---

## 7. Direction modes
`DIR` selects one of ten traversal modes. Default is `Forward`.
### Forward
Moves from the start toward the end of the active window, then wraps.
Skip positions are bypassed.
### Backward
Moves in reverse through the active window, then wraps.
Skip positions are bypassed.
### Pendulum
Moves forward to an end, reverses without wrapping through the window, and
continues back. Skip positions are bypassed. `EOC` fires at each turn.
### Random
Chooses a non-Skip position from the active window on every advance.
Random navigation does not generate `EOC`.
### Drunk
Chooses a forward or backward move for each advance, bypassing Skip positions.
Crossing between the two ends generates `EOC`.
### Ping-Pong
Traverses a fixed forward/backward order with repeated endpoints.
It follows positions directly, so Skip positions are selected silently.
`EOC` marks completion of the full programmed order.
### Odd/Even
Visits alternating positions as one group, then the interleaved positions as
the other group. Skip positions remain in the order and are silent.
### Jump
Advances by a fixed stride, selected as `÷2` through `÷7` in the context menu.
Skip positions remain in the order and are silent.
If the stride and active-window length share a common divisor, Jump visits
only a subset of the active positions before its cycle repeats.
### Converge
Alternates positions from the outer parts of the window toward the center.
Skip positions remain in the order and are silent.
### Diverge
Alternates outward from the center toward the ends of the window.
Skip positions remain in the order and are silent.
---

## 8. Timing controls
### RATIO
`RATIO` divides or multiplies the external clock. Default is `×1`.
Available settings, in index order:
```text
÷48, ÷32, ÷24, ÷16, ÷12, ÷10, ÷8, ÷6, ÷5, ÷4,
÷3, ÷2.5, ÷2, ÷1.5, ×1, ×1.5, ×2, ×2.5, ×3, ×4,
×5, ×6, ×8, ×10, ×12, ×16, ×24, ×32, ×48
```
Division produces fewer sequence ticks than incoming clock edges.
Integer multiplication produces more. UZZ must observe the external clock period before
non-unity timing is fully established, so allow an initial edge or two when
starting or changing a clock source.

In the current implementation, fractional multipliers `×1.5` and `×2.5`
re-anchor their phase on every external edge and produce approximately `×1`
and `×2`, respectively. Use integer multipliers when an exact relationship is
required.
### SWING
- Range: `0%` to `60%`.
- Default: `0%`.
- Delays alternating effective sequence ticks.
- It acts after the selected ratio, so multiplied or divided ticks are swung.
### SLEW
- Range: `0` to `2 s`.
- Default: `0 s`.
- Smooths only the `V/OCT` output between pitch targets.
- It does not delay gates, modulation, step selection, or EOC.
### GATE/TRIG switch
- Default: `GATE`.
- `GATE`: length follows the active step's `DUR`, from `1%` to `95%`.
- `TRIG`: each ordinary pulse is approximately `10 ms`.
- An ordinary gate has a maximum length of `2 s`.
- A multi-period `Gated` event has a maximum length of `8 s`.

These limits apply even with a stable clock and prevent stuck gates with
extreme periods or stopped sources.
---

## 9. Accumulator
The two `ACUMM` controls set increment size and wrap range.
### Amount
- Range: `0` to `24 st`.
- Default: `1 st`.
- Quantized to whole semitones.
- UZZ-X `ACCUM` offsets this value before it is limited to `0..24 st`.
### Wrap
- Range: `OFF`, then `1` to `12 st`.
- Default: `OFF`.
- A value `N` wraps symmetrically through all integer offsets from `-N` to
  `+N`.
- `OFF` uses the full symmetric `-12..+12 st` accumulator range.
For example, with wrap set to `2 st`, upward accumulation cycles through
`-2, -1, 0, +1, +2` rather than clipping at an endpoint.
The accumulator is cleared by UZZ `RESET`, Rack/module reset, or UZZ-X `RST`.
A failed probability test prevents the accumulation event as well as the gate.
Pitch and modulation still update to the selected step on that tick.
---

## 10. UZZ inputs
### CLK
External clock input. Rising edges are recognized with normal Rack trigger
behavior. With no cable, UZZ does not free-run and all gate, poly-gate, and EOC
outputs stop. Pitch and modulation remain available at the selected step.
### RESET
Returns traversal to the effective `START`, clears all step accumulators,
resets swing/traversal timing, and interrupts held or ratcheted gates.
The start step is selected for the next sequence event.
`EOC` on reset is disabled by default and can be enabled in the context menu.
### XPOSE
- Transpose input at `1 V/oct`.
- Quantized to semitones.
- Effective range: `-48` to `+48 st` (`-4` to `+4 V`).
- Added to `PITCH`, `OCT`, and accumulator pitch before `V/OCT` is generated.
### Row random trigger inputs
There is one trigger input for each row:
```text
MODE, PITCH, OCT, DUR, MOD1, MOD2, PROB
```
Each rising trigger randomizes all 16 values in that row, independently of the
active window.
---

## 11. UZZ outputs
### V/OCT
Monophonic pitch CV using `1 V/oct`. It combines:
```text
PITCH semitones + XPOSE semitones + per-step accumulator
+ OCT whole octaves
```
`SLEW` is applied to this output.
### GATE
Monophonic `10V` gate/trigger output. Its timing follows step mode,
probability, `DUR`, and the global `GATE`/`TRIG` switch.
### POLY
Polyphonic `10V` step-gate output with one channel per active-window position.
Channel numbering is relative to the active window, not fixed to physical
steps:
```text
Channel 1 = effective START
Channel 2 = next active position
...
Channel N = final position of the effective STEPS window
```
The output has exactly the effective `STEPS` channel count. If the window wraps,
the channel mapping wraps with it. UZZ-X `STEPS` and `START` CV therefore also
change the poly channel count or physical-step mapping.
### EOC
`10V`, approximately `10 ms` end-of-cycle pulse.
- Forward/Backward: fires when navigation wraps.
- Pendulum: fires at each turn.
- Random: does not fire from navigation.
- Drunk: fires when crossing between window ends.
- Programmed-order modes: fires when their complete order cycles.
- UZZ-X `ADDR`: fires when the addressed relative position falls below its
  previous position.
- RESET: fires only when `EOC on reset` is enabled.
### MOD1 and MOD2
Monophonic CV for the selected step. Each output uses its independently chosen
context-menu range. Default is `0V..10V`.
---

## 12. UZZ-X placement and linking
Place UZZ-X directly to the left of UZZ, with no module between them.
UZZ-X communicates only with the UZZ immediately on its right.
The green link light turns on when placement is correct. If it is off, none of
the UZZ-X inputs affect UZZ. UZZ-X has no outputs and needs no patch cable to
establish the expander connection.
When an offset input is unpatched, its contribution is zero. Discrete offsets
are rounded to the nearest index or step, then the combined value is limited to
the destination's valid range.
---

## 13. UZZ-X CV inputs
### STEPS
- Scale: `1 V` per step offset.
- Adds to the UZZ `STEPS` knob.
- Effective result is limited to `1..16`.
### START
- Scale: `1 V` per physical-step offset.
- Adds to the UZZ `START` knob.
- Effective result is limited to steps `1..16`; the offset itself does not
  wrap around the knob range.
The resulting active window can still wrap physically past step 16.
### DIR
- Scale: `1 V` per direction-mode index.
- Adds to the UZZ `DIR` setting.
- Effective result is limited to the ten modes from `Pendulum` through
  `Diverge`.
The index order is:
```text
Pendulum, Backward, Forward, Random, Drunk,
Ping-Pong, Odd/Even, Jump, Converge, Diverge
```
### ADDR
- Range: `0V` to `10V`, limited at both ends.
- Maps across the current effective active window.
- `0V` selects its first position; `10V` selects its last position.
- Intermediate voltages select the nearest position.
When a cable is connected, `ADDR` bypasses the normal direction navigator.
The direction setting and normal Skip bypass are not used. A Skip step selected
by `ADDR` is therefore silent. `EOC` fires when the address moves backward from
a higher relative position to a lower one.
### RATIO
- Scale: `1 V` per entry in the ratio list.
- Adds to the UZZ `RATIO` index.
- The effective index is limited to `÷48..×48`.
### SWING
- `+5V` spans the full `+60%` swing range from a zero knob setting.
- `-5V` spans the full range in the negative offset direction.
- Adds continuously to the UZZ `SWING` knob.
- Effective swing is limited to `0..60%`.
### PROB
- `+10V` adds `100` percentage points.
- `-10V` subtracts `100` percentage points.
- Adds to the global UZZ `PROB` control.
- Effective global probability is limited to `0..100%`.
### ACCUM
- Scale: `1 V` per semitone offset.
- Adds to the UZZ accumulator amount.
- Rounded to whole semitones.
- Effective amount is limited to `0..24 st`.
---

## 14. UZZ-X trigger and gate inputs
### ROT -
A rising trigger rotates complete step data one position backward inside the
effective active window, with wrapping.
### ROT +
A rising trigger rotates complete step data one position forward inside the
effective active window, with wrapping.
Both rotation inputs move `MODE`, `PITCH`, `OCT`, `DUR`, `MOD1`, `MOD2`,
`PROB`, and the stored per-step accumulator offsets together. Steps outside the
active window are unchanged.
### RST
A rising trigger clears all 16 stored accumulator offsets. It does not reset
the sequence position or panel values.
### REV
- Active at `1V` or higher.
- While high, swaps only `Forward` and `Backward`.
- It has no effect on Pendulum, Random, Drunk, Ping-Pong, Odd/Even, Jump,
  Converge, or Diverge.
- It does not reverse `ADDR` mapping.
---

## 15. Context menus
### UZZ context menu
Right-click UZZ to access:
- `EOC on reset`: off by default; enables an EOC pulse on RESET.
- `Direction mode`: selects any of the ten direction modes.
- `Jump stride`: `÷2`, `÷3`, `÷4`, `÷5`, `÷6`, or `÷7`; default `÷2`.
- `Pitch range`: `1 octave (0..11)` or `2 octaves (0..23)`; default one
  octave.
- `Range Mod 1`: selects the MOD1 output range.
- `Range Mod 2`: selects the MOD2 output range.
Changing pitch range rescales existing pitch-row values proportionally to the
new range. The two modulation range menus offer:
```text
±10V, ±5V, ±3V, ±2V, ±1V,
0V..10V, 0V..5V, 0V..3V, 0V..2V, 0V..1V
```
The default for both modulation outputs is `0V..10V`.
### UZZ-X context menu
UZZ-X has no module-specific context-menu settings. Standard Rack module menu
commands remain available.
---

## 16. Persistence and reset behavior
VCV Rack patch saving preserves all panel parameters and cable connections.
UZZ also preserves:
- MOD1 and MOD2 voltage-range selections.
- Pitch-range selection.
- `EOC on reset` state.
- Jump stride.
- Current sequence position and relevant direction progress.
- All 16 per-step accumulator offsets.
On patch reload, a saved current step outside the restored active window is
moved into that window. Random choices after reload are not guaranteed to
continue as an identical random stream.
Rack's module reset restores panel parameters to defaults and clears runtime
accumulation and traversal state. The `RESET` input is a performance reset: it
returns to effective START and clears accumulators, but does not reset knob or
row values.
UZZ-X stores no additional musical state; its effect comes from current input
voltages and trigger events.
---

## 17. Practical patches
### 17.1 Basic melodic sequence
```text
Clock -> UZZ CLK
UZZ V/OCT -> oscillator V/OCT
UZZ GATE -> envelope GATE
Oscillator -> VCA audio input
Envelope -> VCA CV
```
Use `PITCH`, `OCT`, and `DUR` to write the phrase. Patch `MOD1` to filter cutoff
and keep its context range at `0V..5V` for moderate movement.
### 17.2 Probabilistic rhythm with stable CV
Set selected steps to `25%`, `50%`, or `75%` on the left side of `PROB`.
Patch `MOD1` to timbre and `MOD2` to decay. Failed gate tests create rests, but
pitch and both modulation outputs still move through selected steps.
### 17.3 Ratchets and sustained notes
Set one step to `Pulse` and its `PROB` to `×4` for four sub-pulses in one
period. Set another to `Gated` and `×3` for one sustained three-period event.
Use `Hold` and `×3` when you want three separate clocked attacks while staying
on the same pitch.
### 17.4 Accumulating melody
Set a few steps to `Accum Up` and one to `Accum Down`.
Start with amount `1 st` and wrap `5 st`. Periodic UZZ-X `RST` triggers can
return the harmonic motion to its original state without resetting position.
### 17.5 Rotating variation with UZZ-X
Use an 8-step active window and send a slow trigger to UZZ-X `ROT +`.
All lanes move together, preserving each step's complete musical identity.
Use `ROT -` for an occasional answer phrase.
### 17.6 CV-addressed phrase
Patch a sequencer, stepped random source, or sample-and-hold into `ADDR`.
Use `0..10V` to scan the active window. Remember that direction is bypassed,
Skip steps become silent addressed positions, and descending address movement
can produce EOC.
### 17.7 Active-window drum breakout
Patch `POLY` to a polyphonic trigger utility or splitter. Each channel
represents one relative active-window position. Changing START rotates which
physical programming appears on each channel; changing STEPS changes the
channel count.
---

## 18. Important caveats and troubleshooting
### The sequence does not move
- UZZ is external-clock-only. Confirm that `CLK` is patched and receiving
  rising pulses.
- Ratios other than `×1` need a measured source-clock period; allow at least
  two clock edges after connection or restart.
### A Skip step still appears to be visited
- Skip bypass is limited to Forward, Backward, Pendulum, Random, and Drunk.
- Ping-Pong, Odd/Even, Jump, Converge, and Diverge select Skip positions
  silently.
- Connected `ADDR` also selects Skip positions silently.
### EOC is missing or more frequent than expected
- Random navigation intentionally produces no navigation EOC.
- Pendulum produces EOC at both turns.
- Jump EOC describes its programmed cycle, which may visit only a subset when
  stride and window length have a common divisor.
- Address mode produces EOC when the relative address falls.
### Accumulation sometimes does not occur
The probability test controls both the gate and the Accum Up/Down event. A
failed test leaves that step's accumulator unchanged, although pitch and mod
outputs still update to the selected position.
### POLY channels do not match physical step numbers
This is expected. POLY channels are relative to effective START and STEPS.
UZZ-X modulation of either control changes that mapping in real time.
### REV does nothing
UZZ-X `REV` only swaps Forward and Backward, and only while its input is at
least `1V`. It is not a general inversion control for all direction modes.
### All steps are silent
Check the global `PROB`, per-step probability, step modes, and gate routing.
If every active step is Skip, navigation cannot find a playable step. Restore
the MODE row by double-clicking its random button.
---

## 19. Default reference
```text
Per-step MODE: Play
Per-step PITCH: 0 st
Per-step OCT: 0 oct
Per-step DUR: 50%
Per-step MOD1 / MOD2: 0
Per-step PROB: 100%
Global PROB: 100%
RATIO: ×1
STEPS: 16
START: 1
DIR: Forward
SWING: 0%
Accumulator amount: 1 st
Accumulator wrap: OFF (full -12..+12 st wrap)
SLEW: 0 s
Gate mode: GATE
Pitch range: 0..11 st
MOD1 / MOD2 range: 0V..10V
Jump stride: ÷2
EOC on reset: Off
```
