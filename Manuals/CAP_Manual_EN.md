# CAP User Manual

**Manual version:** 1.1

**Plugin version:** Animatek 2.5.9

**Module:** CAP for VCV Rack

**Width:** 6 HP

---

## 1. Overview

**CAP** is a trigger-driven ducking VCA. Send it the trigger used for a kick or other event and CAP quickly lowers the patched audio, holds it down briefly, then restores it along the selected recovery curve. No detector or external VCA is required.

Each duck has a fixed **2 ms fall** and **12 ms hold**. `RECOVERY` controls the return from **40 ms to 1 s**. `DEPTH` controls how far the gain falls, `JITTER` adds correlated hit-to-hit variation, and `LEVEL` sets the VCA ceiling.

CAP handles stereo and polyphonic audio. By default, one envelope is shared by all audio channels so stereo imaging remains stable. The `ENV` and `EOC` outputs also let CAP act as a modulation envelope or a self-cycling function generator without audio connected.

---

## 2. Quick start

1. Patch a stereo source to `IN L` and `IN R`, or patch a mono source only to `IN L`.
2. Patch `OUT L` and `OUT R` to the next stage of the mix.
3. Send the kick trigger, gate, or rhythm used as the sidechain event to `TRIG`.
4. Start with `RECOVERY` at **250 ms**, `DEPTH` at **80%**, `JITTER` at **25%**, and `LEVEL` at **100%**.
5. Shorten `RECOVERY` for tight rhythmic gaps or lengthen it for audible pumping.
6. Reduce `DEPTH` for subtle movement. Increase `JITTER` when repeated hits should breathe rather than repeat identically.
7. Patch `VCA` for voltage control over gain: the CV multiplies what `LEVEL` sets, so CAP works as an ordinary VCA even with no trigger arriving.

To audition the duck without patching a trigger source, right-click CAP and choose **Fire a trigger**.

---

## 3. Envelope cycle

A trigger starts or retriggers this sequence:

1. **Fall:** gain moves from its current value toward the floor in 2 ms.
2. **Hold:** gain remains at the floor for 12 ms.
3. **Recovery:** gain returns to rest over the current recovery time and curve.
4. **Rest:** gain is 100% of the `LEVEL` ceiling; `ENV` is normally 10 V.

Retriggering during a cycle starts a new fall from the current level and never causes an upward jump. A retriggered recovery is interrupted, so it does not produce `EOC`.

At each hit, CAP samples `DEPTH` plus `D-CV` and chooses the hit's jittered depth, recovery, and curve. Those values remain fixed for that hit instead of moving continuously during the envelope.

---

## 4. Controls

### RECOVERY

Sets the nominal recovery time from **40 ms to 1 s**, with an exponential knob scale for useful fine control at short times. Default: **250 ms**. Jitter can make individual recoveries shorter or longer than this nominal setting.

### DEPTH

Sets the amount of attenuation at the bottom of the duck from **0% to 100%**. Default: **80%**.

- At 0%, triggers do not lower the envelope.
- At 100%, the VCA reaches zero gain at the floor.
- `D-CV` is added before the final range is limited.

### JITTER

Sets correlated variation from **0% to 100%**. Default: **25%**. CAP uses a random walk, so each hit is related to the previous one rather than receiving unrelated white-noise variation. It varies recovery time, depth, and curve shape.

At 0%, every hit uses the nominal settings. Higher values create more organic movement. Stereo remains coherent in the default shared-envelope mode.

### LEVEL

Sets the VCA's maximum gain from **0% to 100%**. Default: **100%**. It scales the audio at rest as well as during a duck. By default it does not scale `ENV`; the context-menu option **Level attenuates ENV** changes that behavior.

### PAN

Places this CAP in the stereo mix of a chain, from hard left to hard right, at the left end of the `TRIG` row; a hairline ties it to its CV jack. It is a balance control, unity in the centre, and only matters when CAPs sit side by side with a **BUS** to their right: this CAP's own outputs never change. See the BUS manual for how the chain works.

The trimmer took the place of the manual trigger button, which moved to the context menu as **Fire a trigger**. No jack went away, so patches made before keep all their cables.

---

## 5. Inputs

### TRIG

Trigger or gate input, at the right end of the `TRIG` row and under its label. It accepts polyphonic signals and uses Schmitt-trigger thresholds: the signal becomes high at **1 V** and must return below **0.1 V** before another rising edge can fire. A sustained gate therefore triggers once.

The number of `TRIG` channels sets the envelope and utility-output polyphony, with a minimum of one channel when no cable is connected.

### D-CV

Polyphonic depth CV. The jack carries no label: a panel hairline runs from it up to the `DEPTH` knob, which is what it modulates. **10 V adds 100% depth** and negative voltage reduces depth. The result of `DEPTH + D-CV / 10 V` is limited to 0-100%, then the hit's jitter variation is applied. CV is sampled when the channel triggers.

### VCA

Polyphonic gain CV. Like `D-CV` it carries no label: the panel hairline runs from the jack up to the `LEVEL` fader, which is what this CV scales. It is unipolar and linear: **0 V closes the VCA and 10 V passes the full ceiling**, with negative voltage treated as 0 V. It multiplies what `LEVEL` sets rather than replacing it, so `LEVEL` stays the ceiling and the CV trims down from there. Unpatched, it attenuates nothing, so an older patch sounds exactly as it did.

This input is what makes CAP an ordinary voltage-controlled amplifier: patch an envelope into it and you have a plain VCA, with trigger-fired ducking available on top when you want it. It does not affect `ENV` or `EOC`: the envelope is what the module generates, not what it amplifies. The meter does follow it, since the meter shows the gain actually applied.

### PAN CV

Polyphonic CV for `PAN`, added to the trimmer: **±5 V sweeps the whole range** and the sum is limited to hard left and hard right. Each audio channel takes its own CV channel. It only affects the chain mix.

### IN L

Left or mono audio input. Accepts polyphonic audio.

### IN R

Right audio input. Accepts polyphonic audio. When it is unpatched, it is internally normalled from `IN L`, so one mono cable feeds both audio paths.

---

## 6. Outputs

### ENV

Outputs the ducking gain envelope as CV: **10 V at rest**, falling according to depth, then recovering to 10 V. It has the same channel count as `TRIG`, with at least one channel. With **Level attenuates ENV** enabled, `LEVEL` also scales this output, including its resting voltage.

### EOC

Outputs a **10 V, 1 ms** end-of-cycle trigger on each envelope channel. It fires only when recovery reaches its natural end. Retriggering before completion cancels that cycle's EOC event. `LEVEL` never attenuates EOC.

### OUT L / OUT R

Ducked audio outputs. Their polyphony follows the connected audio inputs. If no audio input is connected, both outputs have zero channels. `OUT R` receives the normalled `IN L` signal when `IN R` is not patched during normal operation.

---

## 7. Gain meter

The illuminated bar behind the `LEVEL` slider displays applied gain, not audio amplitude. It therefore shows the duck even with silence at the input.

- The white slider marker shows the selected gain ceiling.
- Bar height includes both the envelope and `LEVEL`.
- Bar brightness follows the envelope.
- A stereo or polyphonic patch may show multiple narrow bars.
- In shared-envelope mode, repeated bars represent the same coherent gain; in per-channel mode, they show the individual channel envelopes.
- The bar is blue in **VCA** mode and amber in the two filter modes, where its height reads as how open the filter is.

---

## 8. Context menu

Right-click CAP to access these settings.

### Mode

What the control signal (the envelope multiplied by `VCA` CV) acts on. `LEVEL` stays the output fader in every mode and never moves the filter.

- **VCA** (default): the control sets the gain. This is CAP as it always was; older patches load in this mode and sound exactly as before.
- **Lowpass filter:** the control sets the cutoff of a two-pole lowpass, from 20 Hz with the control closed to 20 kHz with it fully open; the level is left alone. A duck becomes a filtered pump: the hit darkens the sound instead of lowering it. The filter follows the envelope without lag, so the 2 ms fall keeps its punch. Bear in mind that a deep duck pulls the cutoff below the fundamental of most sources, so `DEPTH` decides how dark the hit gets.
- **Low-pass gate:** cutoff and level close together, through a vactrol model that opens in about 2 ms and closes in 30 ms or more, slower the further it closes. That lag is the low-pass gate sound: the tail of a note keeps losing highs after its level has fallen. It also softens the fall of a duck to some 50 ms.

The meter turns amber in both filter modes, so the mode shows on the panel.

### Ping envelope (trigger opens)

Flips the envelope. At rest the control is closed; a trigger opens it to `DEPTH` in 2 ms, holds it for 12 ms and `RECOVERY` closes it again. In **Low-pass gate** mode this is what turns CAP into a classic LPG played by triggers: patch a voice into `IN L`, a sequencer gate into `TRIG`, and every step plucks.

`ENV` and the meter follow the flipped envelope, so `ENV` rests at 0 V and rises on each hit. For a percussive pluck choose the **Logarithmic** recovery curve, which drops fast and then tails off; **Exponential** holds open and closes late.

### Recovery curve

- **Exponential** (default): stays low longer, then returns more quickly near the end; useful for pronounced pumping.
- **Linear:** rises at a constant rate.
- **Logarithmic:** rises quickly at first, then settles more slowly.

Jitter can subtly vary the selected curve on each hit.

### Freeze jitter

Stops the correlated random walks from advancing. Subsequent hits reuse the current variation state, making their timing, depth, and curvature repeat while the option remains enabled. The `JITTER` knob still controls how strongly that frozen state offsets the nominal settings.

### Per-channel envelopes

Gives polyphonic channels separate envelope states and jitter streams. Use this when one polyphonic cable carries unrelated tracks that should duck independently. Leave it disabled for stereo material: the default shared envelope prevents left/right image movement.

If audio has more channels than `TRIG`, extra audio channels use the last available trigger-envelope channel.

### Level attenuates ENV

Makes `LEVEL` scale `ENV` as well as audio. Disabled by default, so `ENV` remains a full 10 V at rest regardless of the VCA ceiling.

### Chain LEDs

Two tiny LEDs in the top corners show the chain. The left one lights when a CAP or a BUS sits against the left edge and the chain reaches this CAP; the right one when this CAP hands the chain on to a CAP or BUS on its right. Dim means linked; brighter means audio passing, following its level. ### Last in a row: OUT is the chain mix

CAPs placed side by side form a mixer. If the row ends in a **BUS**, the mix comes out of the BUS. If it ends in a CAP, that last CAP's `OUT L` / `OUT R` carry the mix instead: its own signal plus everything arriving from the left, each CAP panned and after its fader, as one stereo pair. CAPs in the middle of a row keep `OUT` as a direct out.

On by default for new CAPs. In patches saved before this option existed it loads off, so CAPs that already sat side by side there sound exactly as they did; turn it on in this menu to mix them.

### Fire a trigger

Starts a duck on every envelope channel at once, as a trigger at `TRIG` would. It is what the panel button used to do: audition the duck, or start a self-cycling patch.

### Reset jitter seed

Creates a new random seed and resets all channel random walks. Use it to obtain a different family of correlated variations. This does not trigger an envelope or change panel knob positions.

---

## 9. Persistence and reset

VCV Rack saves the knob values and the following CAP menu state in the patch:

- mode,
- Ping envelope,
- recovery curve,
- Freeze jitter,
- Per-channel envelopes,
- Level attenuates ENV,
- jitter seed.

Saving and reopening a patch restores the chosen seed and settings. Runtime envelope phases are not saved; a reopened module starts with its envelopes at rest rather than resuming an interrupted cycle.

Resetting the module restores **VCA** mode and the exponential curve, disables all four menu toggles, restores the factory seed, and returns the envelopes to rest. Rack's normal parameter reset behavior restores the panel defaults.

---

## 10. Patch examples

### Classic kick ducking

Send the kick trigger to both the kick voice and CAP `TRIG`. Run a bass, pad, or full music bus through CAP. Start near 80% depth and 250 ms recovery, then tune recovery to the groove.

### Stereo bus ducking

Patch both stereo inputs and leave **Per-channel envelopes** off. Both sides receive exactly the same gain movement, preserving the stereo image while the meter displays the active paths.

### Independent polyphonic ducking

Patch matching polyphonic triggers and audio, then enable **Per-channel envelopes**. Each trigger channel gets its own cycle and correlated jitter history. Patch polyphonic `D-CV` for different duck depths per voice.

### External modulation envelope

Leave audio unpatched and send triggers to `TRIG`. Patch `ENV` to a filter, wavefolder, reverb send, or another VCA. The signal rests at 10 V and dips on each trigger, making it naturally suited to inverted or ducking modulation.

### Low-pass gate voice

Set **Mode** to **Low-pass gate** and turn on **Ping envelope**. Patch an oscillator to `IN L` and a sequencer gate or trigger to `TRIG`. `DEPTH` sets how far each hit opens, `RECOVERY` how long the note rings, and the **Logarithmic** curve gives the classic pluck. Raise `JITTER` and no two notes ring quite alike.

### Filtered pump

Set **Mode** to **Lowpass filter** and leave the envelope ducking. Run a pad or a bass through CAP with the kick trigger in `TRIG`: each kick darkens the sound and it opens back up over `RECOVERY`, keeping its level. Lower `DEPTH` if the hit gets too dark.

### Self-cycling function generator

Patch `EOC` back to `TRIG`. After **Fire a trigger** in the context menu, each completed recovery starts the next cycle. The period is approximately the 2 ms fall, 12 ms hold, and selected recovery combined; jitter makes successive cycles breathe. Break the feedback cable or interrupt the trigger path to stop it.

---

## 11. Caveats and practical notes

- CAP is a trigger-driven VCA, not a compressor: it does not listen to audio level and has no threshold, ratio, or makeup gain.
- `LEVEL` is a ceiling/attenuator only. CAP does not amplify above unity gain.
- Fast retriggers can prevent recovery from completing, so `EOC` may remain silent. This is intentional.
- At full depth, the 2 ms fall reduces clicks but extremely discontinuous or low-frequency material can still reveal rapid gain changes.
- Jitter is correlated and bounded, not a percentage guarantee for every individual hit.
- Rack bypass routes `IN L` to `OUT L` and `IN R` to `OUT R`, but bypass does not reproduce CAP's internal right-input normaling. With only `IN L` patched, do not rely on `OUT R` while CAP is bypassed; patch both inputs or split the source externally if the right bypass path is required.
