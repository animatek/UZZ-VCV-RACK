# UNIT-D — Expanded User Manual

**Manual version:** 1.0

**Plugin version:** Animatek 2.5.5

**Module:** UNIT-D for VCV Rack

**Concept:** compact generative sequencer inspired by unit-distance graphs

---

# English

## 1. What UNIT-D is

**UNIT-D** is a generative sequencer for VCV Rack that turns a geometric structure into music.

Instead of using a linear list of steps like a traditional sequencer, UNIT-D creates a 2D cloud of points. It then connects some of those points according to a distance rule. Each point becomes a possible musical state, and each connection becomes a possible transition.

The core idea is simple:

- **Points** are musical states.
- **Lines** are possible paths between states.
- The **clock** moves through the graph.
- The point's **X position** generates pitch and modulation.
- The point's **Y position** generates a second modulation source.
- The **number of connections** of a point generates accent and density.

The result is not pure randomness. It is a **deterministic** generative system: with the same parameters, the behaviour repeats in the same way. This makes it possible to find patterns that feel alive, but still remain controllable.

UNIT-D is designed for melodies, modular lines, semi-repetitive patterns, generative counterpoint and polyphonic material with a clear musical structure.

---

## 2. Quick start

Patch UNIT-D like this to get started:

1. Send a clock into `CLK`.
2. Patch `V/O` into the `V/OCT` input of an oscillator.
3. Patch `GATE` into an envelope generator.
4. Use the envelope to open a VCA.
5. Patch `ACC` into amplitude, filter cutoff or any accent destination.
6. Patch `X` and `Y` into timbral parameters: cutoff, wavefolder, FM amount, wavetable position, decay, etc.
7. Adjust `NODES`, `RADIUS`, `DENS` and `WALK` until you find an interesting phrase.
8. Use `LOCK` to partially freeze the phrase when something good appears.

Recommended starting point:

```text
NODES: 24-32
RADIUS: around 1.0
DENS: medium
TOL: low/medium
WALK: 0 or 1
RNG: 2-3 octaves
GLEN: medium
GDEN: 60-80%
LOCK: center
```

---

## 3. Musical concept

UNIT-D works with a graph. A graph is a network made of points and connections.

In UNIT-D:

- Each point is called a **node**.
- Each line between two nodes is called an **edge** or **connection**.
- The internal path moving from node to node is called the **walker**.
- The number of connections of a node is called its **degree**.

The module creates a deterministic cloud of nodes from `SEED`. It then connects nodes whose distance is close to a target value defined by `RADIUS`, adjusted by `TOL` and `DENS`.

On every clock pulse, the walker tries to move from the current node to one of its neighbours. The selected node produces:

- a note at `V/O`,
- a gate at `GATE`,
- an accent at `ACC`,
- two modulation voltages at `X` and `Y`.

This means melody, accents and modulation are related. They are not independent layers: they all come from the same geometric structure.

---

## 4. Design philosophy

UNIT-D is not meant to behave like an uncontrolled random generator. The point is to find areas where the graph produces phrases with a balance of repetition, movement and surprise.

Think of the module as an instrument with three layers:

1. **Shape:** `SEED`, `NODES`, `RADIUS`, `TOL` and `DENS` define the geometry.
2. **Traversal:** `WALK`, `LOCK`, `GDEN` and `GLEN` define how that geometry is played over time.
3. **Musical reading:** `RNG`, `V/O`, `ACC`, `X` and `Y` define how the traversal becomes sound.

The best way to use it is not to move every control at once. First find an interesting geometry. Then adjust the traversal. Finally, decide how to use the outputs inside the patch.

---

## 5. Inputs

### CLK

Clock input.

Each rising edge advances the walker to the next node. The main tempo and rhythm depend on the signal patched here.

Typical uses:

- Regular clock from a clock module.
- Rhythmic triggers for irregular patterns.
- Clock divisions for slower sequences.
- Swing clocks for a more human feel.

If no clock is present, the sequence does not advance.

### RST

Reset input.

When it receives a pulse, UNIT-D returns to the initial traversal state. It also resets the internal deterministic behaviour, so the pattern can return to the same musical point.

Typical uses:

- Reset every 1, 2, 4, 8 or 16 bars.
- Synchronise UNIT-D with other sequencers.
- Force repeatable phrases inside a generative patch.

### SEED

CV input for seed modulation.

This input can change the entire graph geometry, so it is powerful and should be used with care.

It works especially well with:

- slow voltages,
- sample & hold,
- manual changes,
- occasional automation,
- very slow sequences.

Audio-rate modulation is not recommended here if you want musical stability. Changing the seed too quickly can make the phrase constantly change identity.

### DENS

CV input for connection density modulation.

This is one of the most musical inputs on the module. It lets the pattern breathe: lower density creates more gaps and fragile routes; higher density creates more movement and activity.

Typical uses:

- Open the density in a chorus section.
- Reduce connections in an intro.
- Modulate slowly with an LFO.
- Control complexity from another sequencer.
- Create evolution without completely changing the seed.

---

## 6. Main controls

### SEED

Generates a deterministic cloud of points.

Same seed = same pattern, as long as the other parameters remain the same.

Use `SEED` to search for a different family of melodies. It is the most direct way to change the musical identity without rebuilding the whole patch.

Practical tip: when you find an interesting phrase, save the patch or write down the value before moving `SEED` again. It is the parameter that changes the module's identity the most.

### NODES

Defines the number of internal graph points.

Conceptual range: from few nodes to many nodes.

- Few nodes: simpler, tighter and more repetitive patterns.
- Many nodes: more routes, more variation, more material for polyphony and counterpoint.

Quick guide:

```text
8-16 nodes: very simple phrases, small loops, percussive material.
16-32 nodes: controllable melodies and main patterns.
32-64 nodes: more exploration, generative chords and polyphony.
```

### RADIUS

Defines the target distance used to connect points.

This is the most geometric control. Changing it modifies which nodes are considered neighbours. Small changes can transform the graph significantly.

- If `RADIUS` is too low, there may be too few connections.
- If `RADIUS` is too high, the graph may become too connected.
- The middle area usually produces the most musical results.

Tip: adjust `RADIUS` while watching the display. Look for a network with enough connections to move, but not so dense that it loses direction.

### DENS

Main musical density control.

It affects how many connections remain active or useful inside the graph.

- Low: fewer connections, more silence, fragile and minimal patterns.
- Medium: a balance between repetition and variation.
- High: more motion, more routes and more activity.

`DENS` is ideal for live performance because it changes the energy of the pattern without necessarily destroying the identity of the seed.

---

## 7. Secondary controls

### LOCK

Bipolar locking control inspired by Turing Machine-style behaviour.

- Center: free sequence.
- To the right: gradual locking into a 16-step loop.
- To the left: gradual locking into a 32-step loop.

The further the knob moves away from the center, the more steps are frozen inside the sequence. At the extremes, the phrase becomes fully locked and repeated.

`LOCK` captures notes and gates. In polyphonic mode, it also captures the voices.

Practical use:

1. Let the sequence run freely.
2. Wait until an interesting phrase appears.
3. Turn `LOCK` slowly to either side.
4. Keep it in a semi-locked zone to retain repetition while preserving some variation.
5. Return to the center to release the sequence.

### TOL

Fine tolerance control for the geometric rule.

`TOL` adjusts how much margin is allowed around `RADIUS` when deciding whether two points are connected.

- Low tolerance: stricter rule, fewer connections.
- High tolerance: looser rule, more connections.

Use it to fine-tune a pattern that is almost right but feels too empty or too dense.

Think of `RADIUS` as the broad gesture and `TOL` as the fine adjustment.

### WALK

Selects the graph traversal mode.

```text
0 — Forward neighbour
1 — Deterministic pseudo-random neighbour
2 — Highest-degree neighbour
```

#### WALK 0 — Forward neighbour

The most stable and repetitive mode. It tends to produce more predictable phrases.

Useful for:

- basslines,
- main sequences,
- patterns that must remain recognisable,
- phrases that will later be captured with `LOCK`.

#### WALK 1 — Deterministic pseudo-random

More varied, but still repeatable. It is not pure randomness: with the same parameters, the behaviour can repeat.

Useful for:

- generative melodies,
- textures,
- changing leads,
- patches where you want movement without losing control.

#### WALK 2 — Highest degree

The walker tends to move towards nodes with more connections. This pulls the traversal into denser areas of the graph.

Useful for:

- active patterns,
- energy build-ups,
- sections with more motion,
- more frequent accents.

### RNG

Defines the pitch range in octaves.

It does not change the graph. It only changes how much melodic height is extracted from the X position of the nodes.

- Low values: more contained melodies.
- High values: wider jumps and more open phrases.

Tip: use lower ranges for basslines. Use medium or high ranges for leads and generative arpeggios.

### GLEN

Defines the gate length.

It also affects the display: the active node appears smaller with short gates and larger with long gates.

- Short gate: dry, percussive and precise phrases.
- Medium gate: natural melodic sequences.
- Long gate: sustained notes, rhythmic drones or modular legato.

### GDEN

Defines the gate density.

The walker can keep moving even when some steps do not fire a gate. This creates rests without stopping modulation or the internal traversal.

- Low GDEN: more rests, fewer triggered notes.
- Medium GDEN: musical phrasing with gaps.
- High GDEN: more activity and continuity.

`GDEN` also affects the display:

- Low GDEN: colder or dimmer nodes.
- High GDEN: brighter or warmer nodes.

---

## 8. Outputs

### V/O

Quantised pitch output.

The module turns the X position of the current node into a note inside a fixed C minor scale.

Current scale:

```text
C minor: 0, 2, 3, 5, 7, 8, 10
```

Patch this output into the `V/OCT` input of an oscillator, sampler or modular voice.

### GATE

Gate output.

It depends on:

- whether a valid transition exists,
- the duration set by `GLEN`,
- the density set by `GDEN`,
- the local structure of the node.

Patch `GATE` into an envelope, percussion generator, sample player or any trigger input.

### ACC

0 to 10V accent output.

It depends on the number of connections of the current node. A highly connected node produces a stronger accent.

Recommended uses:

- slightly open a filter,
- increase VCA amplitude,
- modulate envelope decay,
- control velocity on a voice,
- drive distortion or wavefolding,
- emphasise musically important steps.

### X

0 to 10V CV based on the X position of the current node.

Although X position is also used to generate pitch, the `X` output lets you use the same information as continuous modulation.

Recommended uses:

- cutoff,
- wavetable position,
- FM index,
- reverb or delay amount,
- crossfade between sources,
- panning.

### Y

0 to 10V CV based on the Y position of the current node.

This is a second modulation dimension. It works especially well for parameters that should not move exactly like pitch.

Recommended uses:

- resonance,
- decay,
- folding,
- grain size,
- modulation amount,
- timbral colour.

---

## 9. Display

The display shows the graph in real time.

Main elements:

- Points: nodes.
- Lines: connections.
- Main fuchsia node: active main voice.
- Smaller fuchsia nodes: additional polyphonic voices.
- Brightness/colour: related to `GDEN` and gate probability.
- Active node size: related to `GLEN`.

The display is a musical reading tool.

What you can diagnose by watching it:

```text
Very few lines: the graph is too empty.
Too many lines: the graph is too dense.
Movement trapped in one area: the walker is stuck in a region.
Movement across many areas: the phrase is more varied.
Very small active node: short gates.
Large active node: long gates.
Low visual intensity: low gate density.
High visual intensity: higher gate activity.
```

Tip: do not adjust only by ear. Watch the graph. Often you will immediately see why a pattern is too static, too chaotic or too dense.

---

## 10. Polyphony

UNIT-D can produce polyphonic outputs.

Right-click the module to access the polyphony options.

### Poly voices

Defines the number of voices.

Options:

```text
1, 2, 3, 4, 6, 8
```

The `V/O`, `GATE`, `ACC`, `X` and `Y` outputs use the same number of channels.

### Poly seed mode

Defines how voices are separated.

#### Shared seed

All voices use the same base geometry, but with different internal offsets.

Result:

- more coherent,
- more compact,
- more related between voices,
- ideal for polyphonic patterns that should sound like a single entity.

#### Per-voice seed

Each voice generates its own internal geometry from the main seed.

Result:

- more separation between voices,
- more open chords,
- more independent counterpoint,
- stronger generative feeling.

For generative chords or counterpoint, try:

```text
Poly voices: 3 or 4
Poly seed mode: Per-voice seed
NODES: 48-64
DENS: medium/high
RNG: 3-4
WALK: 1
GDEN: 60-90%
```

---

## 11. Patch recipes

### 11.1 Stable monophonic pattern

Goal: a recognisable main phrase.

```text
Poly voices: 1
NODES: 16-32
RADIUS: around 1.0
DENS: 30-50%
WALK: 0
RNG: 2
GDEN: 60-80%
GLEN: medium
LOCK: center or slightly to the right
```

Recommended patch:

- `V/O` → oscillator `V/OCT`
- `GATE` → envelope
- envelope → VCA
- `ACC` → cutoff or VCA level
- `X` → gentle timbre modulation
- `Y` → decay or resonance

### 11.2 Generative melody with rests

Goal: a living line with silence and motion.

```text
Poly voices: 1
NODES: 24-48
WALK: 1
DENS: medium
GDEN: 25-60%
GLEN: short or medium
RNG: 3
LOCK: center
```

Tip: use `GDEN` to control the gaps; do not stop the clock. This way `X` and `Y` modulation keeps evolving even when not every note is triggered.

### 11.3 Modular bassline

Goal: controlled, stable and groovy pattern.

```text
NODES: 12-24
RNG: 1-2
WALK: 0
GDEN: 70-100%
GLEN: short/medium
DENS: low/medium
```

Recommended patch:

- `V/O` → low oscillator
- `GATE` → fast envelope
- `ACC` → filter or saturation
- `Y` → decay or secondary cutoff

### 11.4 Generative chords

Goal: controlled chords or clusters.

```text
Poly voices: 3 or 4
Poly seed mode: Per-voice seed
NODES: 48-64
RNG: 3-4
DENS: medium/high
WALK: 1
GDEN: 60-90%
GLEN: medium/long
```

Recommended patch:

- polyphonic `V/O` → polyphonic oscillator or voice
- polyphonic `GATE` → polyphonic envelopes
- `ACC` → brightness or amplitude
- `X` → shared timbral motion
- `Y` → secondary variation

### 11.5 Turing Machine-style locked sequence

Goal: capture a generative phrase and turn it into a loop.

1. Let the module run with `LOCK` in the center.
2. Search for an interesting phrase using `SEED`, `RADIUS`, `DENS` and `WALK`.
3. Turn `LOCK` slowly to the right to capture a 16-step loop.
4. Turn `LOCK` to the left to capture a longer 32-step phrase.
5. Do not always push it to the maximum: intermediate positions are often more musical.
6. Return to the center to release the sequence.

### 11.6 Generative modulation without melody

Goal: use UNIT-D as a CV generator, not necessarily as a note sequencer.

Recommended patch:

- Do not patch `V/O` if you do not need pitch.
- Use `X` for cutoff.
- Use `Y` for resonance, decay or mix.
- Use `ACC` for intensity hits.
- Use `GATE` to trigger occasional events.

This approach is very useful for drones, textures, ambient patches and long generative systems.

---

## 12. Important control relationships

### RADIUS + TOL + DENS

These three controls define how many connections appear and how the graph is structured.

- `RADIUS` chooses the target distance.
- `TOL` chooses how much margin is allowed around that distance.
- `DENS` chooses the musical or practical connection density.

If the pattern is too empty:

1. Raise `DENS` slightly.
2. Adjust `TOL`.
3. Move `RADIUS` slowly.
4. Increase `NODES` if you need more material.

If the pattern is too chaotic:

1. Lower `DENS`.
2. Lower `TOL`.
3. Try `WALK 0`.
4. Reduce `RNG` if the melodic jumps are too wide.

### NODES + WALK

`NODES` defines how much raw material exists. `WALK` defines how it is traversed.

- Few nodes + WALK 0: simple loops.
- Many nodes + WALK 1: more varied generative material.
- Many nodes + WALK 2: tendency toward dense and active areas.

### GDEN + GLEN

`GDEN` decides how many notes are triggered. `GLEN` decides how long they last.

- Low GDEN + short GLEN: fragmented and percussive pattern.
- Low GDEN + long GLEN: occasional, spacious notes.
- High GDEN + short GLEN: active rhythmic sequence.
- High GDEN + long GLEN: more continuous or legato phrase.

### LOCK + RESET

`LOCK` captures behaviour. `RST` lets the sequence return to the start.

If you want a generative phrase with repeatable structure, use periodic resets. If you want the phrase to become a loop, use `LOCK`.

---

## 13. Recommended workflows

### Method 1 — Find a melody

1. Start with one voice.
2. Set `NODES` between 16 and 32.
3. Set `WALK` to 0.
4. Adjust `RADIUS` until the graph has enough connections.
5. Raise or lower `DENS` until the phrase breathes.
6. Adjust `RNG` to control the melodic range.
7. When a good phrase appears, use `LOCK`.

### Method 2 — Create slow evolution

1. Choose a seed you like.
2. Do not modulate `SEED` constantly.
3. Modulate `DENS` slowly.
4. Use `GDEN` to open and close the amount of notes.
5. Use `X` and `Y` to change timbre.
6. Use `RST` every few bars if you want structure.

### Method 3 — Create polyphonic material

1. Enable 3 or 4 voices.
2. Use `Per-voice seed` if you want clear separation.
3. Use `Shared seed` if you want coherence.
4. Raise `NODES` to 48-64.
5. Use `WALK 1`.
6. Keep `RNG` between 3 and 4.
7. Control density with `GDEN` and `DENS`.

---

## 14. Common problems

### No notes are playing

Possible causes:

- No clock is patched into `CLK`.
- `GDEN` is too low.
- The graph has very few or no connections.
- `DENS`, `RADIUS` or `TOL` are in a too restrictive range.
- `GATE` is not patched to an envelope or suitable destination.

Quick fix:

```text
Raise GDEN.
Raise DENS.
Move RADIUS slowly.
Increase TOL.
Try more NODES.
Check that CLK receives pulses.
```

### The pattern is too chaotic

Possible causes:

- Too many nodes.
- `DENS` is too high.
- `TOL` is too high.
- `WALK` is in a more exploratory mode.
- `RNG` is too wide.

Quick fix:

```text
Lower DENS.
Lower TOL.
Try WALK 0.
Reduce RNG.
Lower NODES to 16-32.
Use LOCK partially.
```

### The pattern is too repetitive

Quick fix:

```text
Raise NODES.
Try WALK 1.
Raise DENS slightly.
Modulate DENS with a slow LFO.
Reduce LOCK or return it to the center.
Use more RNG.
```

### The display looks empty

The graph has too few connections.

Try:

```text
Increase DENS.
Increase TOL.
Move RADIUS.
Increase NODES.
Change SEED.
```

### The display looks too crowded

The graph has too many connections.

Try:

```text
Lower DENS.
Lower TOL.
Move RADIUS to another area.
Reduce NODES.
Use WALK 0 for more stability.
```

### Polyphony sounds blurry

Try:

```text
Reduce the number of voices.
Use Per-voice seed to separate voices.
Lower RNG.
Lower DENS.
Use shorter envelopes.
Filter or pan voices.
```

---

## 15. Technical notes

- UNIT-D does not recalculate the graph on every audio sample.
- The graph is recalculated when `SEED`, `NODES`, `RADIUS`, `TOL` or `DENS` change enough.
- Generation is deterministic.
- If the graph has no connections, the module avoids invalid behaviour: it keeps the pitch stable and does not fire impossible gates.
- The scale and root are currently fixed: C minor.
- `V/O`, `GATE`, `ACC`, `X` and `Y` can work polyphonically according to the voice setting.

---

## 16. Current limitations

These limitations are not bugs; they simply define the current state of the module:

- The scale is fixed to C minor.
- There is no root or scale selector on the panel.
- Generation is based on an internal deterministic geometry.
- The module is designed for musical control, not for proving mathematics.
- Extreme seed or geometry changes can produce drastic musical jumps.

Possible future improvements:

- scale selector,
- root selector,
- more traversal modes,
- node index output,
- additional CV control for `LOCK`,
- configurable quantisation,
- geometric behaviour presets.

---

## 17. Glossary

### Node

A point in the graph. In UNIT-D, each node represents a musical state.

### Connection / edge

A line between two nodes. It defines a possible transition.

### Walker

The internal path that moves from node to node on each clock pulse.

### Degree

The number of connections a node has. In UNIT-D, it influences accent.

### Unit-distance graph

A type of geometric structure where points are connected according to a distance rule. UNIT-D is inspired by this idea, but uses it as a musical engine rather than as a mathematical proof.

### Deterministic

Means that with the same parameters, the result repeats in the same way.

### Gate density

The amount of steps that produce gates. In UNIT-D, this is controlled by `GDEN`.

### Gate length

The duration of the gate. In UNIT-D, this is controlled by `GLEN`.

---

## 18. Core idea

UNIT-D turns geometry into music.

It is not a classic 16-step sequencer and it is not a memoryless random generator. It is a network of possibilities where every clock pulse makes a decision based on the shape of the graph.

When you change the shape, the phrase changes. When you lock the traversal, a loop appears. When you open the density, the system breathes. When you use polyphony, the same geometry can become chords, counterpoint or textures.

The key is not to control everything. Let the graph suggest material, then use the controls to push it in a musical direction.
