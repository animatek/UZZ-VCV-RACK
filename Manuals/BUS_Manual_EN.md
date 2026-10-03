# BUS User Manual

**Manual version:** 1.0

**Plugin version:** Animatek 2.5.9

**Module:** BUS for VCV Rack

**Width:** 4 HP

---

## 1. Overview

**BUS** turns a row of **CAP** modules into a mixer. Place CAPs side by side with a BUS to their right: each CAP passes its stereo signal, after its `LEVEL` fader and its pan, to the module on its right, and the BUS outputs the sum. No cables are needed between them.

The BUS also carries a stereo **send and return**, so an effect can be inserted on everything to its left, a **WET** control to blend the return with the dry bus, a **LEVEL** master and a peak meter.

---

## 2. How the chain works

- The chain is made by touching modules: a CAP passes what arrives from its left plus its own signal to its right, as long as the module on its right is a CAP or a BUS. A gap of even one HP breaks it.
- **Each CAP's own `OUT L` / `OUT R` never change.** They stay direct outputs, and patching them takes nothing away from the mix. Two CAPs side by side with no BUS behave exactly as they always did.
- What each CAP adds is **post-fader** (after `LEVEL`, the `VCA` CV, the envelope and the mode) and **summed across polyphony**.
- Each CAP's **pan** is in its context menu, under **Chain mix**. It is a balance control: in the centre both sides pass at unity, and turning it to one side fades the other. It only affects the mix, not the CAP's own outputs. It is saved with the patch but, being a menu slider, it leaves no undo step.
- A **bypassed CAP** keeps the chain alive and adds its input untouched, as its own outputs do when bypassed. A bypassed BUS passes the chain straight through.
- Every module the signal crosses adds one sample of delay (about 21 µs at 48 kHz), so the CAPs at the far left arrive a few samples later than those near the BUS. That is inaudible unless the same source feeds several CAPs at once.

---

## 3. Controls

### LINK LED

Lit when a CAP or another BUS sits against the left edge of the BUS, which means the chain is reaching it. If it is off, nothing is arriving.

### WET

Crossfade between the bus and what comes back into `RETURN`, from 0 to 100%. Default: **100%**: what returns replaces the bus, which is how an insert works and what an effect with its own dry/wet control wants. Turn it down to blend a fully wet effect, such as a reverb set to 100% wet, with the dry signal. With nothing patched into `RETURN`, `WET` does nothing and the bus passes through.

### LEVEL

Master level, 0 to 100%, applied after the return. Default: **100%**.

### Meter

Peak level of `MIX`, left and right, on a dB scale from −36 to +6 dB relative to 5 V, Rack's nominal audio level. The white tick marks 5 V; the part of a bar above it turns amber, which is where whatever comes next may start to clip.

---

## 4. Inputs and outputs

### RETURN L / RETURN R

The effect's output comes back here. `RETURN R` is normalled to `RETURN L`, so a mono effect needs one cable. Polyphonic cables are summed.

### SEND L / SEND R

The bus as it arrives from the left, before the return and before `LEVEL`. Patch it into the effect.

### MIX L / MIX R

The result: the bus, crossfaded with the return by `WET`, times `LEVEL`. This is also what the BUS passes on to its right.

---

## 5. Patch examples

### A simple mixer

Place four CAPs and a BUS to their right. Patch a sound into each CAP and `MIX L` / `MIX R` to the audio interface. The CAP faders are the channel faders, the BUS `LEVEL` is the master, and each CAP can still duck its own channel from a kick trigger.

### An effect on a group

`CAP CAP BUS CAP CAP BUS`: the first BUS has a delay between `SEND` and `RETURN` and affects only the first two CAPs. Its `MIX` carries on to the right, where the last BUS adds the other two CAPs and acts as master.

### Reverb blended with the dry signal

Set the reverb to 100% wet, patch `SEND` to its input and its output to `RETURN`, and lower `WET` until the blend sounds right.
