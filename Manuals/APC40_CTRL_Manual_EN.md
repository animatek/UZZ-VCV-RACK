# APC40 CTRL User Manual

**Manual version:** 1.0
**For Animatek:** 2.5.5
**Module:** APC40 CTRL fixed MIDI CC-to-CV bridge for VCV Rack

---

## 1. Overview

**APC40 CTRL** converts a fixed set of MIDI Control Change messages into CV. Its layout provides eight track controls, eight device controls, Cue, Master, Crossfader, and eight channel-fader outputs.

The module is intentionally direct: there is no MIDI learn, remapping, button handling, or MIDI feedback. It processes only CC messages. Notes, pitch bend, transport, clock, aftertouch, and program changes are ignored.

The main 19 controls each include an attenuverter. The eight fader outputs are direct unipolar CV without attenuverters.

---

## 2. Quick start

1. Add APC40 CTRL to the patch.
2. Right-click the module and select the APC40 MIDI port in Rack's MIDI section.
3. Leave a main attenuverter fully clockwise at its default `+1` setting.
4. Move the matching hardware control.
5. Patch its output to a filter, VCA, effect, mixer, or other CV destination.
6. For the eight channel faders, patch `F1` through `F8` directly. They produce 0-10 V and have no attenuverters.

The module forces MIDI reception to Omni because its faders use the same CC number on eight different MIDI channels.

---

## 3. Fixed MIDI map

### Track controls

All track controls listen on MIDI channel 1.

| Panel | MIDI message | Raw CV | With attenuverter |
| --- | --- | --- | --- |
| `T1` | CC 48, channel 1 | 0-10 V | 0 to ±10 V |
| `T2` | CC 49, channel 1 | 0-10 V | 0 to ±10 V |
| `T3` | CC 50, channel 1 | 0-10 V | 0 to ±10 V |
| `T4` | CC 51, channel 1 | 0-10 V | 0 to ±10 V |
| `T5` | CC 52, channel 1 | 0-10 V | 0 to ±10 V |
| `T6` | CC 53, channel 1 | 0-10 V | 0 to ±10 V |
| `T7` | CC 54, channel 1 | 0-10 V | 0 to ±10 V |
| `T8` | CC 55, channel 1 | 0-10 V | 0 to ±10 V |

### Device controls

All device controls listen on MIDI channel 1.

| Panel | MIDI message | Raw CV | With attenuverter |
| --- | --- | --- | --- |
| `D1` | CC 16, channel 1 | 0-10 V | 0 to ±10 V |
| `D2` | CC 17, channel 1 | 0-10 V | 0 to ±10 V |
| `D3` | CC 18, channel 1 | 0-10 V | 0 to ±10 V |
| `D4` | CC 19, channel 1 | 0-10 V | 0 to ±10 V |
| `D5` | CC 20, channel 1 | 0-10 V | 0 to ±10 V |
| `D6` | CC 21, channel 1 | 0-10 V | 0 to ±10 V |
| `D7` | CC 22, channel 1 | 0-10 V | 0 to ±10 V |
| `D8` | CC 23, channel 1 | 0-10 V | 0 to ±10 V |

### Global controls

All three listen on MIDI channel 1.

| Panel | MIDI message | Raw CV | With attenuverter |
| --- | --- | --- | --- |
| `CUE` | CC 47, channel 1 | 0-10 V | 0 to ±10 V |
| `MASTER` | CC 14, channel 1 | 0-10 V | 0 to ±10 V |
| `XFAD` | CC 11, channel 1 | 0-10 V | 0 to ±10 V |

### Channel faders

`F1` through `F8` all listen to CC 7, distinguished by MIDI channel.

| Panel | MIDI message | Output |
| --- | --- | --- |
| `F1` | CC 7, channel 1 | 0-10 V direct |
| `F2` | CC 7, channel 2 | 0-10 V direct |
| `F3` | CC 7, channel 3 | 0-10 V direct |
| `F4` | CC 7, channel 4 | 0-10 V direct |
| `F5` | CC 7, channel 5 | 0-10 V direct |
| `F6` | CC 7, channel 6 | 0-10 V direct |
| `F7` | CC 7, channel 7 | 0-10 V direct |
| `F8` | CC 7, channel 8 | 0-10 V direct |

MIDI values 0-127 are converted linearly to 0-10 V.

---

## 4. Attenuverters and outputs

Each `T1-T8`, `D1-D8`, `CUE`, `MASTER`, and `XFAD` output has a dedicated attenuverter with a range of -1 to +1 and a default of +1.

- `+1`: full positive response, 0-10 V.
- Between `0` and `+1`: reduced positive response.
- `0`: output remains at 0 V regardless of the MIDI value.
- Between `0` and `-1`: reduced inverted response.
- `-1`: full inverted-polarity response, 0 to -10 V.

The attenuverter multiplies a unipolar CV by a bipolar scale. It does not offset the signal around a center point. At `-1`, MIDI value 0 still produces 0 V and MIDI value 127 produces -10 V.

`F1-F8` bypass this stage. Their outputs are always direct 0-10 V conversions of CC 7 on channels 1-8.

---

## 5. MIDI behavior

- Reception is forced to `Omni`; a saved channel selection cannot restrict it.
- Only MIDI CC (`Control Change`) messages are processed.
- CC 7 on channels 1-8 updates `F1-F8` respectively.
- The fixed track, device, Cue, Master, and Crossfader CCs update only when received on channel 1.
- Other CC numbers and CCs on unrelated channels are ignored.
- There is no smoothing or pickup mode; each accepted CC value immediately becomes the current target voltage.
- The MIDI activity indicator flashes only for accepted mapped CC messages.
- The module receives data only. It sends no LED, ring, motor-fader, value, or other feedback to hardware.

Because CC 7 on channel 1 is reserved for `F1`, it does not control any attenuverted main output.

---

## 6. Right-click menu

APC40 CTRL adds only Rack's standard MIDI selection section to its context menu. Use it to select the MIDI driver and APC40 input device.

There are no module menus for:

- MIDI learn or CC reassignment.
- Controller presets or hardware variants.
- Button mapping.
- Output range selection.
- Smoothing.
- Feedback configuration.

MIDI channel reception remains Omni even if another channel appears in loaded MIDI configuration.

---

## 7. Persistence and reset behavior

Saved with the Rack patch:

- MIDI driver/device configuration.
- All 19 attenuverter parameter positions.

Not saved:

- Current track-control CC values.
- Current device-control CC values.
- Current Cue, Master, and Crossfader values.
- Current `F1-F8` values.
- MIDI activity-light state.

Runtime values clear to 0 V on module reset and begin at 0 V in a fresh runtime state. Move the hardware controls, or make the controller resend its values, to repopulate the outputs. Parameter and MIDI-device settings are patch configuration and persist when the patch is saved; live controller values are not snapshots.

---

## 8. Practical patch examples

### Eight filter macros

1. Patch `T1-T8` to the filter cutoff CV inputs of eight voices.
2. Leave the attenuverters positive for conventional operation.
3. Reduce each attenuverter to keep the modulation within a musical range.
4. Invert selected tracks when one filter should close as another opens.

### Device controls for an effects bank

1. Patch `D1-D8` to delay time, feedback, reverb mix, distortion, or other effect parameters.
2. Set attenuverters independently to establish depth and polarity.
3. Use the hardware device-control section as a hands-on effects surface.

### Mixer control

1. Patch `F1-F8` to eight VCA or mixer-level CV inputs that accept 0-10 V.
2. Use `MASTER` for a final VCA or macro level.
3. Use `CUE` for headphone/effect send level if the destination offers CV control.
4. Use `XFAD` for a crossfader CV input; adjust its attenuverter to match the destination's expected polarity and range.

### Opposed modulation

1. Patch one attenuverted output to a parameter.
2. Set its attenuverter negative.
3. As the hardware control rises, the output moves from 0 V toward a negative voltage.
4. If the destination needs a positive reversed range such as 10 V down to 0 V, add an offset/inverter utility; the built-in attenuverter alone provides polarity inversion, not a 10 V offset.

---

## 9. Hardware-map caveat

APC40 CTRL follows the exact CC/channel map listed in this manual. APC40 revisions, operating modes, host templates, MIDI translators, custom firmware, and controller configuration can change what the hardware sends. A physical label or control position does not guarantee that its outgoing message matches this module's fixed map.

If a control does not respond:

1. Confirm that Rack is receiving the intended MIDI port.
2. Inspect the controller with a MIDI monitor.
3. Verify the CC number and channel against the tables above.
4. Disable conflicting DAW control-surface ownership or routing if it prevents Rack from receiving data.
5. If the hardware map differs, translate the MIDI externally; APC40 CTRL cannot learn or remap messages.

---

## 10. Caveats and troubleshooting

- No output changes until its mapped CC message is received.
- Controller positions are not restored from a saved patch. Resend or move controls after loading.
- APC40 buttons are ignored, even if they send MIDI.
- MIDI Clock and transport do not produce outputs.
- There is no bidirectional communication or visual feedback to the controller.
- The main outputs can reach negative voltage when their attenuverters are negative. Confirm that the destination accepts bipolar CV.
- `F1-F8` have no attenuation, inversion, smoothing, or remapping. Add utility modules when these functions are needed.
- If only `F1` works, check whether all hardware faders are transmitting CC 7 on channels 1-8 rather than all on channel 1.
- If `T`, `D`, or global controls fail while faders work, verify that those fixed CCs are sent on MIDI channel 1.
