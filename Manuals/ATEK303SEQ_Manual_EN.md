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

### EDIT

Latch switch in the top-left corner. Swaps the generation controls for the pattern editor
in the same panel space. See section 14.

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
- Logo blue: ordinary Note attack.
- Deep blue: Tie, continuing the preceding note without a new attack.
- Cyan: Note with outgoing slide.
- Near white: accented Note.
- Current step: bright white, whatever the step holds: in a row of blues it is the one thing you find without looking for it.

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
- **Clear pattern:** empties the pattern so you can draw one from scratch in the editor. The seed is kept and `STEPS` is left alone; step 1 keeps a note, which is the invariant the sequencer relies on. It is destructive, so it takes the undo slot: **Undo last mutation** brings the pattern back.
- **Gate held through slides (legato):** keeps gate high across valid slide transitions. Off uses a short gate gap while `SLIDE` tells a compatible voice to remain alive. With ATEK303, leave this off or also enable **Auto-legato** in the voice; otherwise the sustained gate does not create the new edge ATEK303 expects by default.
- **Own glide on the V/Oct output:** applies tempo-relative glide for other voices. It automatically bypasses when ATEK303 is attached.
- **Load MIDI file...:** reads a Standard MIDI File and replaces the current pattern with it. See section 13.
- **Bar:** appears when the loaded file is longer than 16 steps. Switches between the file's bars without reopening it.
- **Forget file:** drops the reference to the file. The pattern already loaded keeps playing.
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

## 13. MIDI file import

**Load MIDI file...** in the context menu reads any Standard MIDI File (format 0, 1, or 2)
and turns it into a pattern. It is meant for the acid pattern packs distributed as MIDI,
but it accepts any file whose notes sit on a sixteenth-note grid.

### What each MIDI feature becomes

| In the file | In the sequencer |
|---|---|
| Note-on at a step | Note attack |
| Note crossing the step boundary with nothing attacking after it | Tie: the note is held, not struck again |
| Note crossing the boundary into another attack at a different pitch | Slide |
| Velocity above the file's midpoint | Accent |
| Nothing sounding | Rest |

Timing is quantised to sixteenths, the sequencer's own grid. A four-bar file therefore
yields four 16-step patterns, and the **Bar** submenu moves between them.

### What the module reports

The menu shows a status line under the file name. It tells you whether the file needed the
Chromatic scale, whether it was transposed, and what had to be discarded.

- **Scale set to Chromatic:** the file contained pitches the panel's scale cannot represent,
  so the module switched to Chromatic, which represents any pitch. This keeps the import
  lossless: the melody is not rewritten. `ROOT` is left alone and still transposes the result.
  If the file does fit the current scale, the scale is not touched.
- **No velocity accents:** every note has the same velocity, so there is no accent
  information to read and the pattern arrives unaccented.
- **Transposed +/-N oct:** the file sat outside the representable range and was moved by whole
  octaves. Every interval is preserved; only the register moves.
- **Extra voices dropped:** the sequencer is monophonic. In a chord the lowest note is kept,
  which is what turns an arrangement into a bassline.
- **Skipped N leading empty steps:** the file did not start at tick 0. A piece that comes in
  on the second bar would otherwise import with its first page blank, which says where the
  music starts rather than what it contains, so the silence is dropped. It is dropped in
  whole bars of sixteen steps: trimming right up to the first note would move a pickup onto
  the downbeat and leave the whole bar off the beat, so a file that starts a few steps into
  its first bar keeps that pickup.

### Limits

- SMPTE time division (files authored against video) is rejected; there is no musical grid
  to quantise to.
- Tempo, controllers, program changes, and every other message are ignored. Only notes matter.
- An imported pattern's seed no longer identifies a generated pattern. It is a hash of the
  imported content, stable across reloads, shown so you can tell two imports apart.
- `GENERATE` overwrites an imported pattern, as it does any other. Use `BLOCK` and the
  mutation buttons to develop an import without losing it.

The patch stores the imported pattern in full, so it plays back even if the MIDI file is
later moved or deleted. The file path is saved as well, purely so the **Bar** submenu still
works when the patch is reopened.

## 14. Pattern editor

The `EDIT` button in the top-left corner swaps the generation controls for a pattern
editor in the same panel space. Both views share that area, so nothing else moves and the
step LEDs, inputs, and outputs stay where they are. The button is a latch: it lights while
the editor is showing.

### One octave, on purpose

The piano roll shows **one full octave** - all twelve semitones, seven white keys and five
black ones. The rows the active scale does not contain are drawn recessed and ignore clicks:
they are there so the octave reads as an octave and you can see where each note falls, not
so you can play them. Clicking one does nothing rather than silently switching the module to
Chromatic, which would reinterpret the whole pattern. Octave displacement lives in the `UP`
and `DOWN` rows underneath.

This is how a real 303 works - a one-octave keyboard with two buttons that raise or lower
individual notes - and it is also how the module stores a pattern internally, as a degree
within the octave plus an octave offset per step. So the roll is a direct view of the data,
with no conversion in between. It is also what makes the editor fit: a full chromatic roll
spanning two octaves would give rows a millimetre tall, too small to hit.

### The rows

A keyboard runs down the left edge, a real octave of it. Keys outside the scale are dimmed,
matching their recessed lane. The tonic carries a blue marker and its real octave, so a `C`
on the roll tells you whether it is C2 or C3 rather than leaving you to guess.

| Row | What a click does |
|---|---|
| Piano roll | Sets that step to a note at that degree. Clicking a note where it already sits removes it. Drag to paint across steps. |
| `UP` | Raises the note an octave: 0 to +1 to +2 and back to 0. |
| `DOWN` | Lowers it the same way. Clicking the opposite row jumps straight to that side. |
| `GATE` | Cycles rest, note, tie. |
| `ACC` | Toggles the accent. |
| `SLIDE` | Toggles the outgoing slide. |

Hold and sweep to paint: the gesture keeps applying across steps as you move left or right,
so a run of notes takes one drag rather than one click each. On the roll the drag follows
the mouse vertically too, so moving up or down without leaving the step moves the note to
that pitch. The **right button erases** -
the roll and `GATE` turn the step off, `UP` and `DOWN` return the octave to zero, `ACC` and
`SLIDE` switch the attribute off - and it also works dragged, which is how you clear a
passage in one sweep. Right-clicking the keyboard on the left edge, or the gaps between
rows, still opens the module's context menu.

Colours match the step LEDs, so the panel and the editor always say the same thing: the logo
blue for a plain attack, near white for an accent, cyan for a slide, a deeper blue for a tie, and
slate for the octave rows. A slide is also
drawn as a line joining the two notes, which is where the melodic gesture becomes visible.
The playhead crosses every row.

Steps beyond `STEPS` are dimmed. They are still there and still hold their content, but
they are outside the loop and cannot be edited until `STEPS` reaches them.

### What the editor will not let you do

- **Cells that cannot take a value are drawn dark and ignore clicks.** `UP`, `DOWN`, `ACC`
  and `SLIDE` only mean something on an attack, and a slide additionally needs the *next*
  step to attack - a slide joins two adjacent notes, so a rest or a tie after it leaves
  nothing to slide into. The two notes may share a pitch: there is no glide to make, but a
  303 also plays a slide *legato*, and that half of it is still audible as one sustained
  note instead of two attacks. That is real 303 behaviour, and drawing it means
  you can see which cells are live instead of clicking and wondering why nothing happened.
  Note that the slide belongs to the note it leaves *from*, not the one it arrives at.
- **A slide that stops being possible is dropped.** If you later put a rest after a sliding
  note, the slide turns itself off, because the sequencer would have discarded it at
  playback anyway. The row shows the truth, not the request.
- **The pattern always keeps at least one note.** Erasing the last one puts a note back on
  step 1.
- **Editing does not create a new identity.** The seed is kept: it is still that pattern,
  retouched. `GENERATE` with `BLOCK` off replaces it like any other, so lock the seed first
  if you want to keep hand edits.
- Notes whose pitch sits outside the octave - only possible in a pattern imported from MIDI
  - are folded back into the octave, with the displacement moved to `UP` / `DOWN`, the
  first time you touch that step. The note that sounds does not change as long as it fits
  within two octaves.

### The four pages

Sixty-four steps do not fit in sixteen columns, so the pattern is edited a bar at a time.
The strip under the editor is that: a round button, and four LEDs under the last four
columns of the grid.

- **Click an LED** to show that page in the editor. Nothing about the sound changes.
- **Double-click an LED** to start the sequence there. With `STEPS` at 16 and page `2:4`
  active, the sequence runs steps 17 to 32. The window wraps around the end of the pattern
  if `STEPS` reaches past step 64.
- **Tap the button** to advance one page. **Hold it** for two seconds to follow the
  playhead, so the editor changes page on its own; the bar filling the button is those two
  seconds, and it lights blue while following. Holding again lets go.

The LEDs read like a drum machine: bright white where the sequencer is, brighter still on the
downbeat; the logo blue on the page you are editing; a dim white on the page the sequence
starts from, so you can see where it begins with the clock stopped. A page that falls
outside `STEPS` shows a dimmer blue - still where you are, and it does not sound.

### The acid sticker generates

The acid smiley in the top-right corner is a button. Clicking it does exactly what
`GENERATE` does - a new seed, or a full mutation when `BLOCK` is on - so you can keep
sorting through patterns without leaving the editor to reach the generation panel. It has a
tooltip, because a sticker does not look like a control.
