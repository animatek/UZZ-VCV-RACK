# ONE / MULTI User Manual

**Manual version:** 1.0
**For Animatek:** 2.5.5
**Modules:** ONE MIDI-to-CV interface and MULTI expander for VCV Rack

---

## 1. Overview

**ONE** is a compact MIDI-to-CV interface designed for note, controller, clock, and transport data. It provides monophonic and polyphonic voice conversion, channel-based multitrack operation, eight assignable CC outputs, and MIDI synchronization outputs.

ONE has no panel knobs or CV inputs. Choose its MIDI source and configure its behavior from the Rack right-click menu. The display identifies the current mode and selected MIDI channel; its activity indicator responds to MIDI channel messages.

**MULTI** is the companion eight-track expander. Each track has an independent MIDI channel selector and dedicated `V/OCT`, `GATE`, and `VEL` outputs. Place MULTI directly to the right of ONE so the expander connection can carry MIDI-derived voice data and global pitch bend.

The supported ONE + MULTI workflow uses ONE in **Multitrack** or **Matriceal** mode with MIDI reception set to **Omni**.

---

## 2. Quick start

### A monophonic voice

1. Add ONE to the patch.
2. Right-click ONE and use the Rack MIDI section to select the MIDI driver and device.
3. Under `Play Mode`, select `Mono`.
4. Select the MIDI channel used by the controller, or `Omni` if appropriate.
5. Patch `V/OCT` to an oscillator, `GATE` to an envelope, and `VEL` to a velocity-sensitive destination.
6. Play a note. MIDI note 60 produces 0 V before pitch bend, gate is 10 V while active, and velocity is converted to 0-10 V.

### Eight channel-based tracks

1. Place MULTI directly to the right of ONE.
2. Select `Multitrack` on ONE and verify that the Rack MIDI channel is `Omni`.
3. Set MULTI's eight `CH` selectors to the source channels. Defaults are channels 1 through 8.
4. Patch each MULTI row's `V/OCT`, `GATE`, and `VEL` outputs to a separate voice.
5. Send one musical part on each selected MIDI channel.

---

## 3. MIDI note and CV conventions

- Pitch follows the 1 V/octave standard: MIDI note 60 is 0 V, with each semitone adding or subtracting 1/12 V.
- The current global MIDI pitch bend is added to every ONE pitch channel and every connected MULTI pitch output.
- Active gates are 10 V; inactive gates are 0 V.
- MIDI velocity 0-127 is converted linearly to 0-10 V.
- A Note On with velocity 0 is treated as Note Off.
- MIDI CC 123 (`All Notes Off`) lowers all voice gates.
- MIDI Stop lowers all voice gates and sets `RUN` low.
- Pitch and velocity values can remain at their last values after a gate falls. Use the gate to determine whether a voice is active.

---

## 4. ONE play modes

### Mono

Produces one channel at `V/OCT`, `GATE`, and `VEL`. The latest Note On becomes the current note.

Mono has no held-note stack or last-note-priority recovery. If several notes overlap, releasing the current note lowers the gate; ONE does not return automatically to an older held note. For reliable monophonic lines, avoid overlapping notes or let the source handle note priority.

### Poly

Produces eight-channel polyphonic `V/OCT`, `GATE`, and `VEL` cables. Notes occupy the first available voice. Repeating an active pitch updates its velocity without allocating another voice. If all eight voices are occupied, new notes steal voices in round-robin order.

### Chord

Uses the same eight-voice allocator and polyphonic outputs as Poly. The optional `Chord: unison gate` menu setting changes only gate behavior:

- Off: every voice has its own gate.
- On: all eight gate channels are high while any allocated voice is active.

Pitch and velocity remain independent per voice. The unison option is disabled by default.

### Multitrack

Produces eight-channel polyphonic `V/OCT`, `GATE`, and `VEL` cables. MIDI channels 1-8 map directly to polyphonic channels 1-8. Each MIDI channel holds one current note; a new Note On on that channel replaces its current note state.

Use Omni reception so ONE can receive all eight source channels.

### Matriceal

Produces four-channel polyphonic `V/OCT`, `GATE`, and `VEL` cables. MIDI channels 1-4 map directly to polyphonic channels 1-4, with one current note per MIDI channel.

Use Omni reception so ONE can receive all four source channels.

---

## 5. ONE outputs

### Voice outputs

| Output | Behavior |
| --- | --- |
| `V/OCT` | 1 V/oct pitch, MIDI note 60 = 0 V, plus global pitch bend. One, eight, or four channels according to mode. |
| `GATE` | 0 or 10 V. One, eight, or four channels according to mode. |
| `VEL` | MIDI velocity mapped linearly to 0-10 V. One, eight, or four channels according to mode. |

### Assignable CC outputs

`CC1` through `CC8` each convert their assigned MIDI CC value from 0-127 to 0-10 V. Assignments are global rather than per MIDI channel: any received CC message that passes the selected Rack MIDI input/channel filter can update a matching output.

| Output | Default assignment |
| --- | --- |
| `CC1` | CC 1, Mod Wheel |
| `CC2` | CC 7, Volume |
| `CC3` | CC 10, Pan |
| `CC4` | CC 74, Filter Cutoff |
| `CC5` | CC 71, Resonance |
| `CC6` | CC 91, Reverb Send |
| `CC7` | CC 93, Chorus Send |
| `CC8` | CC 11, Expression |

More than one output may be assigned to the same CC; all matching outputs then follow that value.
CC 123 is the exception: ONE always consumes it as `All Notes Off`, so it does
not update an output even though the assignment menu allows it to be selected.

### Synchronization outputs

| Output | Behavior |
| --- | --- |
| `CLOCK` | Emits a 10 V pulse lasting 1 ms for every incoming MIDI Clock tick (`F8`). |
| `CLK÷n` | Emits a 10 V, 1 ms pulse at the selected tick division: ÷1, ÷2, ÷4, ÷8, ÷16, or ÷24. Default: ÷24. |
| `RUN` | 10 V after MIDI Start or Continue; 0 V after MIDI Stop. |

The division counts incoming MIDI ticks, not musical beats by itself. With standard 24 PPQN MIDI Clock, ÷24 produces one pulse per quarter note. MIDI Start resets the divider count; changing the division also restarts its count.

---

## 6. ONE right-click menu

### Rack MIDI section

Select the MIDI driver, device, and input channel using Rack's standard MIDI menu. Channel filtering affects channel messages such as notes, CC, and pitch bend. System real-time messages such as Clock, Start, Continue, and Stop do not carry a MIDI channel.

For `Multitrack` and `Matriceal`, selecting the mode switches the input to Omni. Verify `Omni` after loading or reconfiguring a patch so all required channels reach ONE.

### Play Mode

Selects `Mono`, `Poly`, `Chord`, `Multitrack`, or `Matriceal`. Changing mode clears active allocator/track gates to prevent old assignments from carrying into the new mode.

### Chord: unison gate

When enabled in Chord mode, all eight gate channels follow the logical OR of all active voices. It has no useful effect in the other modes.

### Pitch Bend Range

Sets the global bend range from ±1 to ±12 semitones. Default: ±2 semitones. Set the same range in the sending controller or sequencer for predictable tuning.

### CLK/n Division

Selects ÷1, ÷2, ÷4, ÷8, ÷16, or ÷24. Default: ÷24.

### CC Assignments

Each of the eight CC outputs can be assigned independently to any MIDI CC number from 0 through 127. The menu groups numbers into banks of 16 for navigation and shows standard names where available. CC 123 remains reserved for `All Notes Off` and does not generate CV.

---

## 7. MULTI controls and outputs

MULTI has eight identical rows and no inputs or right-click configuration beyond Rack's standard module commands.

| Item | Behavior |
| --- | --- |
| `CH` selector | Chooses MIDI channel 1-16 for that row. Selectors are stepped to whole channel numbers. Defaults: rows 1-8 use channels 1-8. |
| `V/OCT` | Current note on the selected MIDI channel, using 1 V/oct and ONE's global pitch bend. |
| `GATE` | 10 V while the selected channel's current note is active; otherwise 0 V. |
| `VEL` | Current velocity on the selected channel, mapped to 0-10 V. |

Each MIDI channel stores one current note. A newer Note On on the same channel replaces the prior note. A Note Off lowers the gate only when it matches that channel's current note.

Rows may select the same MIDI channel if duplicate CV copies are useful. When MULTI is absent, separated from ONE, or not immediately to its right, all MULTI outputs are 0 V.

Although MULTI receives ONE's tracked channel state, the supported operating arrangement is ONE in Multitrack or Matriceal mode with Omni reception. MULTI can select any of the 16 MIDI channels, independent of the four/eight polyphonic channels available on ONE's own voice outputs.

---

## 8. Persistence and reset behavior

Saved with the Rack patch:

- ONE MIDI driver/device/channel configuration.
- ONE play mode.
- Chord unison-gate setting.
- Pitch-bend range.
- Clock division.
- All eight CC assignments.
- MULTI's eight MIDI channel selector parameters.

Not saved as musical runtime state:

- Held notes and gates.
- Current velocities and CC voltages.
- Current pitch-bend position.
- MIDI transport/run state.
- Clock divider phase and pending clock pulses.

After a reset or fresh runtime state, send the relevant MIDI data again. Saving a patch does not turn currently held notes or controller positions into a snapshot for later recall.

---

## 9. Practical patch examples

### Expressive mono lead

1. Use Mono mode and select the controller's channel.
2. Patch `V/OCT`, `GATE`, and `VEL` to a mono synth voice.
3. Patch `CC1` (Mod Wheel) to vibrato depth.
4. Patch `CC4` (Filter Cutoff) through an attenuator to filter cutoff.
5. Match the controller's bend range to ONE's menu setting.

### Eight-voice polyphonic instrument

1. Choose Poly mode.
2. Patch ONE's three voice outputs to polyphonic oscillator, envelope, and velocity inputs.
3. Keep individual chord gates off so each voice articulates independently.
4. Limit the source to eight simultaneous notes if predictable voice allocation matters.

### Chord stabs with shared articulation

1. Choose Chord mode and enable `Chord: unison gate`.
2. Send chord notes to ONE.
3. Use the polyphonic pitches with a common gate response across all eight channels.
4. Remember that unused channels also receive the unison gate; make sure the destination handles inactive/stale pitches as intended.

### Multitrack performance rig

1. Place MULTI directly to the right of ONE.
2. Select Multitrack and verify Omni.
3. Assign sequencer tracks to MIDI channels 1-8.
4. Leave MULTI selectors at their defaults and patch each row to a separate modular voice.
5. Use ONE's CC outputs for shared macro controls and `CLOCK`, `CLK÷n`, and `RUN` for synchronization.

### MIDI clock master

1. Enable MIDI Clock output on the external sequencer.
2. Patch `CLOCK` to devices that need every 24 PPQN tick.
3. Set `CLK÷n` to ÷24 for quarter-note pulses, or another division for denser pulses.
4. Patch `RUN` to a run input or logic destination that accepts a 10 V transport gate.

---

## 10. Caveats and troubleshooting

- ONE receives MIDI; it does not generate MIDI or send feedback to the controller.
- There are no panel inputs, knobs, MIDI-learn gestures, or CV control over menu settings.
- Mono does not remember older held notes. Avoid overlapping notes when last-note fallback is required.
- Poly and Chord are limited to eight allocated voices. Additional notes steal voices.
- Multitrack and Matriceal are channel-based and monophonic per MIDI channel, not polyphonic within each track.
- Global pitch bend affects all ONE voices and all MULTI rows; there is no per-channel bend state.
- CC assignments respond by CC number after Rack's MIDI filtering; they are not independent per-channel CC lanes.
- Clock outputs require incoming MIDI Clock. Note traffic alone does not create clock pulses.
- `RUN` follows MIDI transport only; it is not inferred from notes or clock ticks.
- If MULTI outputs remain at 0 V, confirm that it is immediately to ONE's right, ONE is receiving MIDI, the selectors match the source channels, and Multitrack/Matriceal is using Omni.
- If notes are missing in Multitrack or Matriceal, check the Rack MIDI channel first. A single-channel filter prevents the remaining track channels from reaching ONE.
