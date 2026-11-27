# ECLIPSE + ECLIPSE PITCH User Manual

**Manual version:** 1.0

**Plugin version:** Animatek, the release after 2.5.10

**Modules:** ECLIPSE and ECLIPSE PITCH for VCV Rack

**Width:** 8 HP each

---

## 1. Overview

**ECLIPSE** is a double Euclidean sequencer: two Euclidean rhythms drawn as two rings of LEDs in one circle. Ring **A**, outside, steps on the incoming clock; ring **B**, inside, steps on that clock divided or multiplied. Each ring has its own gate output, and three more outputs combine them with logic: **AND** (both hit), **OR** (either hits) and **XOR** (only one hits). Two rhythms in, five rhythms out.

**ECLIPSE PITCH** is its note expander. Glued to the right of an ECLIPSE, it turns one of those five gates into a melody in a scale, chosen at random or moved step by step by each ring's hits, and it adds a **LOCK** that loops the last steps of everything, gates and notes alike, as a Turing Machine does.

ECLIPSE works on its own as a rhythm generator. ECLIPSE PITCH does nothing without an ECLIPSE on its left.

---

## 2. Quick start

1. Patch a clock into ECLIPSE's `CLK` (sixteenth notes are a good start).
2. Patch `A` into a kick, `B` into a hi-hat and `XOR` into a snare or a percussion voice.
3. Turn `HITS` and `ROT` on each ring until the groove sits; set `RATIO` to `×2` to make B run twice as fast.
4. For a melody: right-click ECLIPSE and choose **Add pitch expander**. ECLIPSE PITCH appears glued to its right, already linked.
5. Patch ECLIPSE PITCH's `V/OCT` into an oscillator and the gate chosen in `SRC` (OR by default) into its envelope.
6. When you hear something you like, press `LOCK`.

---

## 3. ECLIPSE

### How the rings work

Each ring spreads `HITS` hits as evenly as it can over `LEN` steps (the Euclidean rhythm, the same family as Bjorklund's algorithm). Step 1 is at the top and the ring runs clockwise. `ROT` turns the whole pattern clockwise.

On the display:

- **Hits** are lit in the ring's colour (A blue by default, B orange by default); rests are small grey dots.
- The **step playing now** is white; when it is a hit it glows.
- The step where the **pattern starts** is drawn a little larger and brighter, and it turns with `ROT`, so the rotation can be read off the ring.
- In the middle: `HITS/LEN` of A, `HITS/LEN` of B, and B's clock ratio.
- While **AND** fires, the centre lights up softly: the eclipse.
- While ECLIPSE PITCH holds a lock, `LOCK` and its length show in the top right corner.

### Top row

| Control | What it does |
| --- | --- |
| `CLK` | Clock input. A rising edge (above 2 V, after going below 0.1 V) steps ring A. |
| `RST` | Reset input. Both rings go back to the start, and the next clock plays step 1. |
| `RATIO` | B's clock against A's: ÷4, ÷3, ÷2, ×1, ×2, ×3, ×4. Default ×1. |
| `GATE` | Gate length, 5 to 95% of the step that fires it. Default 50%. |

### Ring rows: A and B

| Control | Range | What it does |
| --- | --- | --- |
| `LEN` | 1–64 | Steps in the ring. Default 16 for A, 8 for B. |
| `HITS` | 0–64 | Hits spread over the ring. Above `LEN` it counts as `LEN`. Default 5 for A, 3 for B. |
| `ROT` | 0–63 | Turns the pattern clockwise, wrapping at `LEN`. |

**Click the A or B letter** beside a row to roll that ring: `LEN` from 3 to 32, `HITS` from 1 to one less than `LEN` (a ring full of hits says nothing) and `ROT` anywhere in it. The letter lights up when the mouse is over it, and one undo puts the three knobs back.

### B's clock

- **Divided** (÷N): B steps on one A tick in N, counted from the last reset, so a reset brings both back in line.
- **Multiplied** (×N): B steps on every A tick and on N−1 ticks in between, spaced by the time measured between the last two clock edges. The first clock after starting has nothing to measure yet, so B plays no in-between ticks until the second one. If the tempo rises and an edge arrives before the in-between ticks are done, B skips the ones left without playing them, so it never drifts away from A.

### The logic outputs

Each ring has a state: whether its current step is a hit. The logic is worked out on every tick of either clock, that is, at the rate of the faster one:

- **AND**: A and B both hit.
- **OR**: A, B or both hit.
- **XOR**: one of them hits, not both.

How long a ring's hit counts is chosen in the context menu, under **Logic**:

- **Hold** (default): a hit lasts its whole step. With B running faster than A, every B tick inside an A hit is checked against it, so AND plays ratchets inside A's hits. With B slower, one B hit spans several A steps.
- **Trigger**: a ring only counts on the tick it steps. The logic is then about hits that land at the same moment.

With `RATIO` at ×1 both give the same result.

### Outputs

`A`, `B`, `AND`, `OR` and `XOR`: 10 V gates, each with a light beside its label.

- `A` and `B` last `GATE` of their own ring's step; the logic outputs last `GATE` of the faster clock's step.
- A gate fired again while still high drops for 1 ms first, so whatever is downstream sees a new edge.

### Context menu

- **Add pitch expander**: creates ECLIPSE PITCH glued to the right, pushing aside whatever was there, and links it. It reads *Pitch expander attached* when one is already there.
- **Ring A colour**: the module's colour, from the Animatek palette. It recolours ring A and everything blue on the panel. Default blue.
- **Ring B colour**: ring B's colour and everything that belongs to it (`RATIO`, the B letter, the B output and its light). In **Auto** (default) each ECLIPSE added or duplicated takes a colour no other ECLIPSE in the patch has, so several of them can be told apart. Blue is not offered, as it is ring A's.
- **Logic**: Hold or Trigger (see above).

All of these are saved with the patch.

---

## 4. ECLIPSE PITCH

### Attaching it

It has to touch the right edge of an ECLIPSE. The easiest way is ECLIPSE's **Add pitch expander**, which places it there. On its own its display asks to be glued to the right of an ECLIPSE and its outputs stay at 0 V.

All the sequencing stays in ECLIPSE, because the lock has to rewind the rings too: ECLIPSE PITCH sends its settings to ECLIPSE and ECLIPSE sends the note back, one sample late each way. ECLIPSE PITCH takes its ECLIPSE's colour (Ring A colour), so the pair reads as one instrument.

### How the note is chosen

Each time the gate chosen in `SRC` fires, ECLIPSE picks a new note and holds it until that gate fires again. The note is a degree of the scale, within `RANGE` octaves above the key.

- **RND**: a degree anywhere in the range, at random.
- **ACUM** (default): an accumulator. Every hit of A moves it `+A` degrees, every hit of B moves it `+B` degrees, and it wraps around at the top of the range. The note is the accumulator's value when `SRC` fires. Because both rings repeat, so does the melody, but it can take a long time to come back to where it started. With `+A` at +1 and `+B` at 0 it climbs the scale one degree per A hit.

### Controls

| Control | Range | What it does |
| --- | --- | --- |
| `MODE` | RND / ACUM | How the note is chosen. Default ACUM. |
| `SRC` | A, B, AND, OR, XOR | Which of ECLIPSE's gates picks a new note. Default OR. |
| `RANGE` | 1–4 octaves | How far the note can move. Default 2. |
| `SCALE` | 11 scales | Minor (default), Major, Dorian, Phrygian, Lydian, Mixolydian, Harmonic minor, Minor pentatonic, Major pentatonic, Blues, Chromatic. The same list as UNIT-D. |
| `KEY` | C–B | The key. Default C. |
| `OCT` | −2 to +2 | Octave of both outputs. |
| `+A` | −7 to +7 | ACUM: degrees each A hit moves the note. Default +2. |
| `+B` | −7 to +7 | ACUM: degrees each B hit moves the note. Default −1. |
| `LOOP` | 1–64 | Steps the lock repeats. Default 16. |
| `LOCK` | button | Locks and unlocks (lit while locked). |

`SCALE`, `KEY`, `OCT` and `ROOT IN` act at once, on the note already playing too.

### Inputs and outputs

| Jack | What it does |
| --- | --- |
| `ROOT` (input) | Transposes the key, 1 V per octave, rounded to the semitone. Patch another sequencer's V/OCT here to make the melody follow its notes. |
| `LOCK` (input) | Locked while it is at 1 V or more, as well as with the button. |
| `V/OCT` | The note. 0 V is C4 (key C, octave 0, first degree). |
| `ROOT` (output) | The key at the same octave: the tonic the melody is built on. |

### LOCK

At every clock edge ECLIPSE keeps a copy of everything that decides what comes next: where both rings are, the accumulator and the random generator. When the lock goes on, at the next clock edge it goes back to the copy from `LOOP` steps ago and plays on from there; every `LOOP` steps it goes back to the same copy. So what was just heard repeats, **gates and notes alike**.

- `LOOP` counts A's clock steps. Right after a reset there are fewer steps recorded, and the lock loops as many as there are.
- If `LOOP` does not match `LEN`, the lock cuts the polyrhythm short and a new pattern comes out of it.
- The knobs keep working while locked: changing `HITS`, `ROT`, `SCALE` or the steps changes the pattern being looped.
- Unlocking carries on from wherever the loop is.
- A reset while locked starts the loop again from that point: it then loops the next `LOOP` steps.
- Without ECLIPSE PITCH there is no lock: its controls live on it.

### The note wheel

The display is drawn like ECLIPSE's rings:

- One ring of LEDs per octave of `RANGE`, the lowest inside; the outermost is the size of ECLIPSE's ring A, so the two displays line up side by side.
- One LED per degree of the scale, with the key at the top, larger and in the module's colour.
- The last 12 notes lit and joined by a line in the order they played, dimmer the older they are; the note playing now is white with a halo. With the lock on, the same figure is drawn again and again.
- In the middle, the note playing now (C4 = 0 V).
- In the corners: key and scale, the lock and its length, the mode with its source, and in ACUM the `+A` and `+B` steps, each in its ring's colour.

The last 32 notes are kept in the module, not in the display, so none are missed even when they come faster than the screen refreshes.

---

## 5. Patch examples

### Three voices from two rings

`A` (16 steps, 4 hits) into a kick, `B` (`RATIO` ×2, 8 steps, 3 hits) into a closed hat, `XOR` into a clap. Turn B's `ROT` and listen to the clap move between the two.

### Ratchets inside the kick

Logic **Hold**, `RATIO` ×3, B with many hits. `AND` into a snare: it rolls inside A's hits only.

### A bassline that answers the drums

ECLIPSE PITCH in ACUM, `SRC` on XOR, `RANGE` 1, `+A` +3, `+B` −2, minor pentatonic. `V/OCT` and `XOR` into a bass voice. The line rises with the kick and falls with the hats.

### A locked phrase

MODE RND, `SRC` on OR, `LOOP` 8. Let it play until a phrase sounds right, then press `LOCK`. Turn `KEY` or patch a progression into `ROOT` to move the locked phrase around.

### Several ECLIPSEs in one key

Patch the `ROOT` output of one ECLIPSE PITCH into the `ROOT` input of the others, leave their `KEY` at C (it adds to what arrives) and give each its own `OCT`: one an octave down for the bass, one an octave up for a lead, all in the same key. Each ECLIPSE takes its own ring B colour in Auto, so they stay easy to tell apart.

---

## 6. Notes

- Both modules are monophonic.
- The clock period is only accepted between 1 ms and 5 s; a longer gap is taken as a stopped clock, not a tempo.
- ECLIPSE PITCH's settings reach ECLIPSE one sample late, so a lock pressed exactly on a clock edge takes effect on the next one.
