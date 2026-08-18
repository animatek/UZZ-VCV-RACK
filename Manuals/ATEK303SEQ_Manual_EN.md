# ATEK303 SEQ - User Manual

**Manual version:** 1.0
**Plugin version:** Animatek 2.5.5
**Module:** ATEK303 SEQ, 20 HP deterministic 16-step acid generator for VCV Rack

## 1. Concept

ATEK303 SEQ is an externally clocked monophonic pattern generator, not a conventional row of 16 editable step controls. A seed and a small set of musical controls create repeatable acid lines with notes, rests, ties, accents, and slides. The result is deterministic for a given saved pattern and mutation history, while `GENERATE` can deliberately choose a new identity.

The pattern has two coordinated layers:

- The **time layer** contains Rest, Note, or Tie states.
- The **pitch layer** contains the note events, octave placement, accent, and outgoing slide information consumed by Note states.

A Rest closes the voice. A Note creates a new attack and consumes the next pitch event. A Tie extends the previous pitch and gate without a new attack. A slide connects two adjacent, different Note events; it is not the same as a tie.

## 2. Quick start

1. Place ATEK303 SEQ immediately to the left of ATEK303, or patch its outputs to another monophonic voice.
2. Patch an external clock to `CLOCK`.
3. If using cables, patch `V/OCT`, `GATE`, `ACCENT`, and `SLIDE` to their destinations.
4. Press `GENERATE` to make a pattern from a new seed.
5. Start with the defaults: 16 steps, 65% notes, 45% range, C root, Acid scale, 85% gate, 60% accent, and 50% slide.
6. Press `BLOCK` when the identity is worth preserving, then use the three mutation buttons for controlled variation.

The first clock edge starts step 1 of 16; the module does not emit a note merely because it was added to a patch.

## 3. Main controls

### STEPS

Sets the playback loop length from 1 to 16 steps; default 16. It changes the active loop immediately. The underlying generated material remains 16 steps, so increasing the length can reveal it again.

### NOTES

Sets note density from 5% to 100%; default 65%. Higher values generate more Note/Tie activity and fewer rests. This is a generation control: turn it, then use an unlocked `GENERATE` to build a new pattern with that density.

### RANGE

Sets generated melodic/octave spread from 0% to 100%; default 45%. Low values keep a compact line; high values allow wider octave movement. It affects newly generated material rather than transposing the current pattern.

### ROOT

Selects one of 12 chromatic roots, C through B; default C. Root transposes playback immediately without regenerating the pattern.

### SCALE

Selects one of eight quantized pitch vocabularies; default Acid:

- Acid minor pentatonic
- Natural minor
- Phrygian
- Harmonic minor
- Dorian
- Blues
- Major
- Chromatic

Scale affects playback interpretation and pitch/octave mutation. Changing it can therefore alter the audible current line immediately, and future generation uses the selected scale.

### GATE

Sets ordinary note gate length from 5% to 100% of the measured clock period; default 85%. A small safety gap remains before the next edge. Ties hold the gate through their continuation independently of ordinary gate length. Slides hold gate across the edge only when **Gate held through slides (legato)** is enabled.

### ACCENT

Sets generated accent density from 0% to 100%; default 60%. It affects future unlocked generation. Existing accents can be changed with articulation mutation.

### SLIDE

Sets generated slide density from 0% to 100%; default 50%. Slides are kept only between adjacent active notes of different pitch; invalid or redundant slides are removed. It affects future unlocked generation, while articulation mutation can alter existing slides.

## 4. Generation and mutation controls

### GENERATE

With `BLOCK` off, creates a new random seed and generates all pattern layers from the current generation controls. This intentionally replaces the current pattern and clears mutation undo.

With `BLOCK` on, retains the seed identity and mutates all three families in one gesture: time, pitch/octave, and slide/accent. This is a mutation, so it can be undone once from the context menu.

The `GEN` input performs the same operation as the button.

### BLOCK

Latch that locks the seed. Off means `GENERATE` chooses a new seed. On means `GENERATE` mutates all layers instead. The lit button and **Lock seed** context item represent the same option.

### MUT TIME

Requests two deterministic mutation operations on the Rest/Note/Tie time layer. It can move attacks or change a note and tie relationship while preserving a valid pattern. If the operations cancel each other and leave the pattern unchanged, the generator can apply one additional operation to produce a visible change.

### MUT NOTE/OCT

Requests two pitch-family operations. In the current version this mutation changes octave placement; the button name reserves note/octave scope, but it should presently be used as an octave variation control. One additional operation can be applied if the first two leave the pattern unchanged.

### MUT SLD/ACC

Requests three deterministic articulation operations on accents and slides. One additional operation can be applied if the initial result is identical to the starting pattern.

Each successful mutation stores one level of undo. A subsequent mutation replaces that undo snapshot.

## 5. Inputs

### CLOCK

External clock input. Rising edges advance the sequence. There is no internal clock. The measured period controls gate duration and, when enabled, the sequencer's own glide, so tempo changes remain musically proportional. Clock and trigger detection use Schmitt behavior around 0.1 V/1 V.

### RESET

Prepares step 1 and stops the current transport state. The next clock edge starts step 1. Reset does not generate a new pattern, change the seed, or emit EOC by itself.

### GEN

Trigger input for `GENERATE`. With `BLOCK` off it creates a new seed and pattern; with `BLOCK` on it mutates all three layers. This makes controlled pattern changes clockable from another module.

## 6. Outputs

### EOC

10 V, approximately 1 ms end-of-cycle pulse. It fires on the very first clock edge that starts step 1 and whenever playback wraps to step 1. Account for the first-edge pulse when using EOC to count completed cycles or cascade sequencers.

### V/OCT

Monophonic 1 V/oct pitch. Base octave and root are added to scale-quantized pattern pitch. During rests it keeps the previous pitch rather than jumping to an irrelevant value.

With **Own glide** enabled and no attached ATEK303, this physical output glides after a slide. When ATEK303 is attached immediately to the right, own glide disables itself: the expander receives raw pitch and the physical `V/OCT` output is also unglided, preventing two glide stages.

### GATE

10 V while the current note is active. Gate duration follows `GATE`, except that ties maintain the note through continuation steps. The slide legato menu option can also hold gate across slide boundaries.

### ACCENT

In normal mode, outputs 10 V for accented notes and 0 V otherwise. In **Accent as velocity CV** mode, it holds a configurable accent level on accented notes and a configurable base level on active unaccented notes; rests output 0 V.

### SLIDE

10 V on a valid active note whose pitch should slide into the next adjacent active note. It is low for rests, ties, equal-pitch transitions, or transitions that cannot form a valid slide.

## 7. Step LEDs

The 16 RGB LEDs display the rendered pattern and highlight the current step:

- Off: Rest.
- Green: ordinary Note attack.
- Blue: Tie, continuing the preceding note without a new attack.
- Amber/yellow: Note with outgoing slide.
- Red: accented Note.
- Current step: brighter, with an additional blue highlight.

When attributes overlap, the display uses a clear priority: tie, then accent, then slide, then ordinary note. The audio pattern still retains its valid underlying articulation.

## 8. ATEK303 expander

Place ATEK303 immediately to the right of ATEK303 SEQ. The sequencer sends four signals internally: raw `V/OCT`, gate, accent state, and slide state. EOC is not sent through the expander.

ATEK303 resolves priority per input jack. Any cable patched into the voice's `V/OCT`, `GATE`, `ACC`, or `SLIDE` jack overrides only the matching expander signal. This permits hybrid patches, such as expander timing and articulation with external pitch.

When attached, sequencer glide is bypassed and ATEK303 performs slide itself. The sequencer's physical `V/OCT` also remains raw in this arrangement, so a multed destination does not silently receive the glide that the attached voice does. Leave **Gate held through slides (legato)** off, its default, so ATEK303 receives a new gate edge; if legato is enabled in SEQ, also enable **Auto-legato** in ATEK303 so pitch changes under a high gate initiate the slide.

## 9. Context menu

Right-click the module to access:

- **Pattern version and seed:** read-only identification of the current generator version and hexadecimal seed.
- **Lock seed:** same state as `BLOCK`.
- **Mutate time (2 operations):** same action as `MUT TIME`.
- **Mutate pitches / octaves (2 operations):** same family as `MUT NOTE/OCT`; currently produces octave mutation.
- **Mutate accents / slides (3 operations):** same action as `MUT SLD/ACC`.
- **Undo last mutation:** restores the snapshot before the latest successful mutation. Disabled when no undo is available. It does not undo a new-seed generation.
- **Gate held through slides (legato):** keeps gate high across valid slide transitions. Off uses a short gate gap while `SLIDE` tells a compatible voice to remain alive. With ATEK303, leave this off or also enable **Auto-legato** in the voice; otherwise the sustained gate does not create the new edge ATEK303 expects by default.
- **Own glide on the V/Oct output:** applies tempo-relative glide for other voices. It automatically bypasses when ATEK303 is attached.
- **Base octave:** C1 (-3 V), C2 (-2 V), C3 (-1 V), C4 (0 V), or C5 (+1 V); default C2.
- **Accent as velocity CV:** switches `ACCENT` from a binary accent gate to held velocity-style levels.
- **Accent level:** 10 V, 8 V, or 5 V; default 8 V. Used for accented notes in velocity mode.
- **Base level (unaccented note):** 0 V, 1 V, 2 V, or 3 V; default 2 V. Used for active unaccented notes in velocity mode.

## 10. Persistence and transport

VCV Rack patches save the generated dual-layer pattern, seed, mutation counter, generation settings associated with the pattern, all panel parameters, BLOCK state, gate/slide behavior, own-glide setting, base octave, and accent CV options.

The current transport position, whether the first clock has arrived, measured clock period, and one-level undo snapshot are not saved. After loading, the next clock starts from step 1. The saved pattern remains intact, but **Undo last mutation** is unavailable until a new successful mutation is made.

## 11. Patch examples

### Direct ATEK303 acid system

1. Attach ATEK303 on the right and clock the sequencer with sixteenth notes.
2. Use 16 steps, C, Acid scale, 60-75% notes, and the default articulation densities.
3. Generate several identities, then enable `BLOCK` on the best one.
4. Alternate `MUT TIME` and `MUT SLD/ACC` during performance.
5. Use EOC to trigger another event, remembering it also pulses at the first edge.

### Drive a third-party voice

1. Patch `V/OCT` and `GATE` to a mono synth voice.
2. Leave **Own glide** enabled and patch `SLIDE` only if the destination has a dedicated slide input.
3. Enable **Accent as velocity CV**, then patch `ACCENT` to velocity, VCA level, or filter cutoff.
4. Adjust accent/base levels to suit the destination's CV range.

### Deterministic variations for arrangement

1. Find a pattern with unlocked `GENERATE`.
2. Turn on `BLOCK` and save the patch.
3. Use one mutation family at a time and listen for a useful variation.
4. Use **Undo last mutation** immediately if the change is not useful.
5. Reset at phrase boundaries to align step 1; reset does not alter the saved identity.

### Polyrhythmic loop

1. Set `STEPS` to 13 or 15 while the master rhythm remains in groups of 16.
2. Patch `EOC` to trigger a slow modulation or another sequencer reset.
3. Keep in mind that changing `STEPS` changes wrap timing immediately and therefore EOC timing.

## 12. User-relevant caveats

- An external clock is always required; `GENERATE` changes pattern data but does not advance it.
- `NOTES`, `RANGE`, `ACCENT`, and `SLIDE` shape future unlocked generation. They do not continuously rewrite the current pattern. `STEPS`, `ROOT`, `SCALE`, and `GATE` have immediate playback effects.
- New-seed generation is intentionally non-repeatable until saved; BLOCK mutations preserve the current seed identity and are deterministic.
- Ties and slides are different. Ties extend the same note and gate; slides move between two attacked notes of different pitch.
- The first clock edge emits EOC because it enters step 1. Use a gate delay or downstream counter logic if only completed wraps should count.
- Undo has one level and is not persistent. Generate with a new seed clears it.
- ATEK303 attachment disables own glide for both the expander route and physical pitch output. This is intentional so the voice performs exactly one slide.
