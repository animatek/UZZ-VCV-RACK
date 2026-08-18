# CAP User Manual

**Manual version:** 1.0

**Plugin version:** Animatek 2.5.5

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

Press the panel trigger button to audition the duck without patching a trigger source.

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

### Manual trigger button

Starts a duck without an external trigger. The button fires all current envelope channels together and is edge-sensitive, so holding it does not repeatedly retrigger CAP.

---

## 5. Inputs

### TRIG

Trigger or gate input. It accepts polyphonic signals and uses Schmitt-trigger thresholds: the signal becomes high at **1 V** and must return below **0.1 V** before another rising edge can fire. A sustained gate therefore triggers once.

The number of `TRIG` channels sets the envelope and utility-output polyphony, with a minimum of one channel when no cable is connected.

### D-CV

Polyphonic depth CV. **10 V adds 100% depth** and negative voltage reduces depth. The result of `DEPTH + D-CV / 10 V` is limited to 0-100%, then the hit's jitter variation is applied. CV is sampled when the channel triggers.

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

---

## 8. Context menu

Right-click CAP to access these settings.

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

### Reset jitter seed

Creates a new random seed and resets all channel random walks. Use it to obtain a different family of correlated variations. This does not trigger an envelope or change panel knob positions.

---

## 9. Persistence and reset

VCV Rack saves the knob values and the following CAP menu state in the patch:

- recovery curve,
- Freeze jitter,
- Per-channel envelopes,
- Level attenuates ENV,
- jitter seed.

Saving and reopening a patch restores the chosen seed and settings. Runtime envelope phases are not saved; a reopened module starts with its envelopes at rest rather than resuming an interrupted cycle.

Resetting the module restores the exponential curve, disables all three menu toggles, restores the factory seed, and returns the envelopes to rest. Rack's normal parameter reset behavior restores the panel defaults.

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

### Self-cycling function generator

Patch `EOC` back to `TRIG`. After one press of the manual trigger button, each completed recovery starts the next cycle. The period is approximately the 2 ms fall, 12 ms hold, and selected recovery combined; jitter makes successive cycles breathe. Break the feedback cable or interrupt the trigger path to stop it.

---

## 11. Caveats and practical notes

- CAP is a trigger-driven VCA, not a compressor: it does not listen to audio level and has no threshold, ratio, or makeup gain.
- `LEVEL` is a ceiling/attenuator only. CAP does not amplify above unity gain.
- Fast retriggers can prevent recovery from completing, so `EOC` may remain silent. This is intentional.
- At full depth, the 2 ms fall reduces clicks but extremely discontinuous or low-frequency material can still reveal rapid gain changes.
- Jitter is correlated and bounded, not a percentage guarantee for every individual hit.
- Rack bypass routes `IN L` to `OUT L` and `IN R` to `OUT R`, but bypass does not reproduce CAP's internal right-input normaling. With only `IN L` patched, do not rely on `OUT R` while CAP is bypassed; patch both inputs or split the source externally if the right bypass path is required.
