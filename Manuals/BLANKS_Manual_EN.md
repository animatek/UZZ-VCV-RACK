# BLANK 3 & BLANK ACID User Manual

**Manual version:** 1.0

**Plugin version:** Animatek 2.5.5

**Modules:** BLANK 3 and BLANK ACID for VCV Rack

**Width:** 3 HP each

---

## 1. Overview

**BLANK 3** and **BLANK ACID** are animated 3 HP blank panels with no controls or ports. They fill space, separate functional areas, and create evolving visual compositions in a patch.

Adjacent BLANK 3 and BLANK ACID modules recognize each other as one contiguous group. The internal seams disappear and the panels become slices of a shared canvas. Every panel contributes two slowly drifting marks, which can move across panel boundaries. BLANK 3 contributes Animatek logo marks; BLANK ACID contributes acid-smiley marks. Mixed groups retain both shapes, so each variant remains identifiable.

The available mark palette is **Blue, Orange, Green, Purple, Cyan, and Rose**.

---

## 2. Quick start

1. Add either blank panel to create a narrow visual spacer.
2. Place more BLANK 3 or BLANK ACID modules directly beside it, with no other module between them.
3. Mix the two variants to combine logo and smiley marks on one canvas.
4. Right-click any member to set the group's animation speed and mark colors.
5. Use **Pause animation** when you want to inspect or present a still composition.

No patch cables are needed because neither module has inputs or outputs.

---

## 3. Shared canvas and grouping

A group is an uninterrupted horizontal run made only of BLANK 3 and BLANK ACID modules. Variant order does not matter.

- Adjacent variants join automatically.
- Each panel contributes exactly two animated marks.
- Marks use the shape of the panel that contributed them.
- Marks move through the full width of the group and can cross internal seams.
- Internal panel seams are visually covered; only the outside group border remains.
- Adding a blank on the left expands the coordinate space while keeping existing marks visually near their previous positions.
- A nonblank module breaks the run into separate groups.
- Moving a blank away splits its old group; placing it beside another blank joins the new group.

Group detection updates automatically but may take a brief moment after moving modules.

---

## 4. Variants

### BLANK 3

Contributes two drifting **Animatek logo** marks and displays a matching fixed logo near the bottom. Use it for a geometric, brand-oriented texture.

### BLANK ACID

Contributes two drifting **acid-smiley** marks and displays a matching fixed smiley near the bottom. Its animated marks are optically balanced against the denser smiley shape.

### Mixed groups

Both variants use the same grouping rules, speed, palette, and canvas. In a mixed run, every panel keeps its own mark shape while all marks are visible across the group. The fixed bottom symbol identifies the variant even when its moving marks have drifted elsewhere.

---

## 5. Controls and ports

There are no panel controls, parameter automation targets, inputs, or outputs. All settings are in the context menu.

The modules do not process audio or CV. Their purpose is spacing and visual animation.

---

## 6. Context menu

Right-click either variant to open its menu.

### Pause animation

Pauses or resumes animation for **all BLANK 3 and BLANK ACID modules in Rack**, not only the selected panel or group. This is a global session switch.

Pause is not saved in patches. A new Rack session starts with animation running.

### Speed

Selects the drift multiplier:

- **0.25x**
- **0.5x**
- **1x** (default)
- **2x**
- **4x**
- **8x**

The selection is applied to the entire contiguous group. Every panel stores its current speed so the group setting survives patch save/load and duplication.

When two groups with different stored speeds are joined, the **leftmost group's speed wins** and is propagated through the merged canvas after group detection stabilizes.

### Mark colour

The selection is also applied group-wide.

- **Auto (pick a free one):** releases the group colors and redistributes available palette colors. Panels take free colors where possible; groups longer than six panels reuse the palette.
- **Blue**
- **Orange**
- **Green**
- **Purple**
- **Cyan**
- **Rose**

Choosing a named color assigns that same color to every member of the current group. Auto allows the group to distribute colors among its members. Each panel subsequently stores its assigned color, so moving or duplicating it preserves that color until Auto or another named color is selected.

---

## 7. Persistence

Saved per panel in the VCV Rack patch:

- animation speed,
- assigned or selected mark color.

Not saved:

- current mark positions, velocities, rotations, sizes, and opacity,
- the global Pause animation state.

Consequently, a reopened patch preserves the speed and palette arrangement but generates a fresh moving composition. Marks will not resume at the exact places seen when the patch was saved.

When saved panels are joined, normal group rules still apply. In particular, the left group resolves speed conflicts when groups merge.

---

## 8. Bypass behavior

Bypassing one blank stops the movement of **that member's own two marks**. It does not remove the module from the shared canvas and does not break group detection.

Other non-bypassed members continue to move their own marks, and those marks may still cross and be drawn over the bypassed panel. The bypassed panel's frozen marks also remain part of the shared group artwork. This makes bypass a per-member motion freeze, not a visibility switch or group pause.

Use **Pause animation** instead when the whole rack should stop together.

---

## 9. Practical visual arrangements

### Minimal spacer

Place one BLANK 3 between a sequencer and a voice to create a clean 3 HP break with understated logo movement.

### Acid divider

Place two or three BLANK ACID panels together between rack sections. Use Orange or Rose and a 0.5x speed for a slow, visible divider.

### Mixed panoramic strip

Alternate BLANK 3 and BLANK ACID across four to eight panels. Select Auto colors so the palette spreads through the group. Marks from every panel roam across the full strip and mix both shapes.

### Monochrome canvas

Join several variants, choose one named color from **Mark colour**, and set speed to 0.25x. The common color unifies the different logo and smiley silhouettes.

### Deliberate group break

Build one cool-colored group, place any functional nonblank module next, then build a warm-colored group. The functional module prevents the canvases, speed settings, and automatic color assignment from merging.

### Selective frozen marks

Bypass one member of a moving group. Its own two marks become stationary while neighboring marks continue to travel across the same canvas, producing a layered still/moving composition.

---

## 10. Caveats and practical notes

- Grouping is based strictly on immediate left/right adjacency. Empty rack space or any nonblank module separates groups.
- Both variants count as blanks for grouping; mixing their shapes never splits the canvas.
- Group menu edits affect all members of the current group, so separate panels first if they need independent speed or color choices.
- Selecting a named color makes the whole current group monochrome. Select Auto to redistribute palette colors.
- Auto assignment happens after the chain is stable, so colors or merged speed can take a brief moment to settle after dragging modules.
- Pause is global and temporary. Do not rely on it being restored with a patch.
- Bypass freezes only the bypassed member's marks; it does not hide them and does not isolate the panel.
- Mark positions are intentionally ephemeral. Use Pause for a temporary still image, but expect a different composition after reloading.
