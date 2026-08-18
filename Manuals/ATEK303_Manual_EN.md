# ATEK303 - User Manual

**Manual version:** 1.0
**Plugin version:** Animatek 2.5.5
**Module:** ATEK303, 12 HP monophonic acid voice for VCV Rack

## 1. Concept

ATEK303 is a complete monophonic voice inspired by the signal flow and playing behavior of the TB-303: oscillator, resonant low-pass filter, decay envelope, accent, slide, VCA, and output stage. It is intended both as the direct partner of ATEK303 SEQ and as a normal modular voice driven through patch cables.

Two sound-model presets are available:

- **Circuit (ATEK)** favors circuit-modeled oscillator and diode-ladder filter behavior, circuit-style control curves, pitch-domain slide, accent accumulation, saturation, and subtle analogue variation.
- **Open303 original** selects the cleaner reference Open303 behavior and its original ranges and curves.

The preset chooses a coherent group of structural settings. Every detail remains available in **Fine tuning**, so mixing engines and behaviors changes the model indication to **Custom**. The menus expose musical calibration rather than requiring knowledge of the underlying DSP.

## 2. Quick start

### With ATEK303 SEQ

1. Place ATEK303 SEQ immediately to the left of ATEK303, with no gap.
2. Send an external clock to the sequencer's `CLOCK` input.
3. Patch ATEK303 `OUT` to a mixer or VCA, beginning at a conservative level.
4. Press `GENERATE` on the sequencer.
5. Adjust `CUT OFF`, `RESONANCE`, `ENV MOD`, `DECAY`, and `ACCENT`.

Pitch, gate, accent, and slide travel through the expander connection without four visible cables. A cable inserted into any corresponding voice input replaces only that expander signal.

### With another sequencer

1. Patch pitch to `V/OCT` and gate to `GATE`.
2. Optionally patch gate or trigger signals to `ACC` and `SLIDE`.
3. Select saw or square, then patch `OUT` to a mixer, VCA, or effect.
4. If the source emits continuous pitch, enable **Quantize pitch to semitones** when chromatic stepping is wanted.

## 3. Panel inputs

### V/OCT

1 V/oct pitch input, using the VCV convention of 0 V = C4. Pitch is continuous unless menu quantization is enabled. **Limit to the 303 range (C1-C4)** clamps the interpreted note range; it does not attenuate the incoming voltage. With quantization disabled, the fractional semitone is reapplied as pitch bend and can extend nearly half a semitone beyond the C1/C4 boundaries.

### GATE

Starts and releases notes. Gate detection uses hysteresis: it rises at 1 V and remains high until it falls below 0.1 V. This rejects noise near the threshold.

### ACC

Marks an attack as accented when at or above 1 V. Accent is sampled as the note begins and affects loudness, filter movement, and accent-envelope behavior. The red LED follows this input's logical state.

### SLIDE

Requests a legato pitch transition. It uses the same 1 V rising and 0.1 V falling hysteresis as `GATE`. When gate falls while `SLIDE` remains high, the voice is kept alive for the following note instead of being released. The blue LED shows the logical slide state.

### Six modulation inputs

`CUT OFF CV`, `RESONANCE CV`, `ENV MOD CV`, `DECAY CV`, `ACCENT CV`, and `TUNING CV` each modulate the control directly above them. Each has a bipolar attenuverter:

- Center: no modulation, even with a cable connected.
- Clockwise: positive modulation.
- Counterclockwise: inverted modulation.
- At full attenuverter depth, -5 V to +5 V spans the full control range.
- The combined knob and CV value is clamped to that control's valid range.

## 4. Panel controls

### CUT OFF

Sets the filter cutoff, approximately 314 Hz to 2394 Hz before analogue drift. The default panel position is 35%. The response is exponential, giving useful resolution across the range.

### RESONANCE

Sets filter resonance from 0% to 100%; default 50%. The Circuit model uses a linear response matching the control law of the original-style circuit. Open303 uses its reference resonance curve.

### ENV MOD

Sets how strongly the decay envelope sweeps the filter, from 0% to 100%. Default knob position is 50%; with the default logarithmic taper this displays and behaves as approximately 12.5%. Most of the strongest movement is therefore concentrated near the upper part of the knob, like an audio-taper control. The taper can be made linear in the menu.

### DECAY

Sets envelope decay on a logarithmic time scale. Its available display range depends on the menu setting:

- **Circuit:** 200 ms to 2.5 s.
- **Open303:** approximately 460 ms to 4.6 s.

Accent uses the shortest decay of the selected range, reproducing the tighter accented response.

### ACCENT

Sets accent intensity from 0% to 100%; default 50%. It defines how strongly an active `ACC` signal emphasizes a note. With accent accumulation enabled, consecutive accents can build additional emphasis, up to the safe internal limit.

### TUNING

Sets the oscillator reference tuning from 400 Hz to 480 Hz; default 440 Hz. This is global fine tuning, separate from `V/OCT` pitch.

### Waveform switch

Selects **sawtooth** on the left or **square** on the right. The default is square. Fine-tuning options can alter pulse width, reset shape, and waveform droop.

## 5. Output

### OUT

Monophonic audio output at a fixed full voice level. There is no panel level control: use an external VCA, mixer, or attenuator. The final signal is safety-clamped to +/-12 V, but accent and saturation can still produce a much hotter signal than an unaccented note. Lower the receiving channel before auditioning aggressive settings.

## 6. Expander operation and cable priority

ATEK303 receives `V/OCT`, `GATE`, `ACC`, and `SLIDE` from an ATEK303 SEQ placed directly to its left. No browser configuration or menu command is needed.

Priority is evaluated independently for all four signals:

1. A patched jack on ATEK303 has priority.
2. If that jack is unpatched, the corresponding expander signal is used.
3. If neither is available, pitch defaults to 0 V and the logical inputs are low.

Examples: patching only `V/OCT` lets another sequencer transpose or replace pitch while ATEK303 SEQ still supplies gate, accent, and slide. Patching only `ACC` replaces expander accents without disturbing the other three signals.

The sequencer sends raw step pitch when attached and disables its own output glide, leaving ATEK303 to perform the slide once in the selected pitch/frequency domain.

## 7. Context menu

Right-click the module to open these settings.

### Sound model

- **Circuit (ATEK):** selects the circuit-oriented oscillator, filter, decay range, hard output saturation, soft OTA saturation, subtle drift, linear resonance, pitch-domain slide, accent accumulation, and logarithmic Env Mod taper.
- **Open303 original:** selects Open303 oscillator and filter, Open303 decay and slide behavior, no added saturation or drift, the Open303 resonance curve, no accent accumulation, and linear Env Mod.
- **Custom:** an indicator shown when the current detailed settings match neither complete preset; it is not a separate preset.

Selecting a sound model changes the grouped structural options, but does not reset panel controls or unrelated calibration choices such as slide time, filter drive, or waveform shape.

### Fine tuning - Oscillator

- **Motor:** `Open303 (wavetables)` or `ATEK (modelled from schematic)`.
- **Pulse width (TM5):** 44%, 47%, 50%, 53%, or 56%.
- **Square droop:** no droop, 8 Hz, 15 Hz, 30 Hz, or 60 Hz. Higher settings give the square wave more tilt and movement.
- **Saw: reset corner:** 1 us sharp, 3 us, 8 us, or 20 us round. This changes the hardness of the saw reset.
- **Saw droop:** 0.7 Hz, 3 Hz, 8 Hz, or 15 Hz.

### Fine tuning - Filter

- **Motor:** `Open303 (TeeBee)` or `ATEK (diode ladder)`.
- **Linear resonance:** uses the circuit-style linear control response when enabled.
- **Drive:** 0 dB, +6 dB, or +12 dB into the ATEK filter. More drive thickens and can compress the resonant response.

### Fine tuning - Envelope and accent

- **Decay range:** Open303 (about 460 ms-4.6 s) or Circuit (200 ms-2.5 s).
- **Accent accumulation:** lets closely spaced accents charge and carry emphasis into following notes.
- **Accent stages:** Fast only (100 ms), Slow only (450 ms), or Both. Fast adds immediate punch; slow adds carry; Both is the circuit-style response.
- **Log Env Mod taper:** enabled gives the default audio-taper response; disabled makes the knob and CV mapping linear.

### Fine tuning - Pitch and output

- **Slide in semitones/s:** enabled glides in pitch space, producing an even musical rate; disabled uses Open303's frequency-domain curve.
- **Slide tau:** 60 ms, 120 ms, or 220 ms. This is a time constant, not the exact time required to reach the target; longer values glide more slowly.
- **Quantize pitch to semitones:** rounds incoming pitch to chromatic semitones.
- **Limit to the 303 range (C1-C4):** clamps notes to the original-style three-octave range. For strict chromatic limits, use it together with **Quantize pitch to semitones**; without quantization, fractional pitch bend can extend slightly beyond the boundaries.
- **Auto-legato:** when gate stays high, a pitch change starts a slide without requiring `SLIDE`. It is off by default because continuously moving CV or a settling quantizer can otherwise cause unintended slides.
- **OTA saturation:** None, Soft, or Hard before the VCA.
- **Output saturation:** None, Soft, or Hard in the modeled final output path.
- **Analogue drift:** None, Subtle, Marked, or Heavy. It combines a repeatable note-dependent tuning character with slow pitch and cutoff movement. The menu describes the approximate pitch and cutoff amount for each level.
- **Unit:** 1 to 4. Selects the repeatable note-by-note DAC error pattern; it does not randomize on every note.

## 8. Persistence and reset

VCV Rack patches save all panel controls and context-menu options, including sound engines, calibration, slide behavior, saturation, drift level, and Unit. Reloading a patch restores these settings. Runtime note, gate, envelope, and drift positions are not musical sequence data and restart from a fresh runtime state. Rack's module reset releases the current note and clears the accent envelopes.

## 9. Patch examples

### Classic sequenced acid line

1. Attach ATEK303 SEQ on the left and select **Circuit (ATEK)**.
2. Start with square, cutoff around 30%, resonance 60%, Env Mod 50-70%, and a short decay.
3. Generate until accents and slides produce a useful phrase.
4. Raise filter drive or output saturation for more bite.

### External modulation voice

1. Patch a keyboard or sequencer to `V/OCT` and `GATE`.
2. Send a slow bipolar LFO to `CUT OFF CV` and set its attenuverter near 25%.
3. Send velocity or a trigger pattern to `ACC`.
4. Use a separate envelope or random CV at `DECAY CV` for phrase variation.

### Hybrid expander control

1. Attach ATEK303 SEQ and let it provide gate, accent, and slide.
2. Patch a different quantized source into the voice's `V/OCT` jack.
3. The external pitch wins while the generated articulation remains active.

## 10. User-relevant caveats

- ATEK303 is monophonic; polyphonic input channels are not separate voices.
- There is no output level knob. Use downstream gain control and remember the +/-12 V safety ceiling is not a target operating level.
- A CV attenuverter defaults to center, so inserting a modulation cable may appear to do nothing until the trimpot is moved.
- Slides require a valid legato relationship. For conventional short gates, `SLIDE` keeps the voice alive across the gap; with permanently high gates, use Auto-legato or ensure the source supplies the intended transitions.
- Quantization, C1-C4 limiting, drift, and Unit all alter pitch interpretation. Disable them when exact continuous tracking or calibration measurements are required.
- Circuit and Open303 are starting points, not restrictions. Any detailed edit legitimately makes the model Custom.
