# Animatek – UZZ (VCV Rack Plugin)

UZZ (Ultimate Ztep Zequencer) is a step sequencer originally created as a Max for Live device, designed for immediate control, clear visual feedback, and fluid musical flow.

This repository is a port to VCV Rack 2.x, aiming to preserve the original philosophy: precise timing, structured improvisation, and modular flexibility.

## Changelog
### 2.3.0 — Stability & Host Reload Fixes
*(2025-11)*

#### Core Fixes
* Fixed critical crash when reloading the Rack plugin inside Bitwig or other hosts.
* `dataToJson()` no longer assumes a valid base object from `Module::dataToJson()`.
    * It now creates its own `json_object()` when the base is `nullptr`, preventing null writes that previously killed the plugin sandbox (“plugin host died / end of stream”).
* `dataFromJson()` now calls the base method before applying stored values for a clean restore.
* UI asset safety during headless scans.
    * Constructors that previously called `APP->window->loadSvg()` unconditionally now guard against a null `APP->window`.
    * When the host runs Rack without a window (typical during VST scans), custom graphics are skipped or replaced with system fallbacks instead of crashing.

#### Timing & Clock Behavior
* Default clock ratio set to ×1.
* `RATIO_DEFAULT_INDEX` now points to the ×1 entry so new instances and resets start at unity speed. Existing projects keep their saved ratio.
* Virtual clock rewrite for long gates.
    * Added `needsVirtualClock` so the internal oscillator only runs when the ratio differs from ×1.
    * At ×1, the module waits for a new rising edge to advance, eliminating runaway ticks caused by held gates.
* `havePhase` is forced off at ×1 to prevent the scheduler from enqueuing extra steps while an external gate is high.

#### UI & Font Rendering
* Improved note LED readability.
* Font loading now uses `asset::system("res/fonts/ShareTechMono-Regular.ttf")` with `APP && APP->window` guards to avoid headless crashes.
* Text rendering tuned for clarity: centered alignment, pixel-rounded coordinates, and an optimal 9 px size.
* Optional monospace fallback retained for hosts running without system fonts.

#### SVG Loading Refactor
* Unified safe SVG loader (`loadSvgFile()`).
* All plugin and system assets now route through this helper, which performs a single `system::exists()` check and prefers `APP->window->loadSvg()` when available, falling back to a non-window loader in headless mode.
* Step-mode icons and row-shift arrows now render correctly after VST project reloads.

#### Internal Changes
* Hardened JSON read/write path with null-checks and index clamping.
* Moved sample-rate dependent initialization to `onSampleRateChange()`.
* Improved fallbacks for missing UI assets and fonts.
* Minor cleanups in button constructors and naming.

### 2.1.0 — Per-Row Shift & Menu Enhancements
*(2025-10-03)*

* Added per-row shift controls for Pitch, Octave, Duration, Mod1, and Mod2 with wrap-free behavior and step-size tweaks (Mod rows now move 10% per click).
* Introduced contextual menu refinements: dedicated submenus for Direction mode, Range Mod1, Range Mod2, and an EOC-on-reset toggle.
* Implemented optional EOC trigger on external reset with persistence.
* Refined reset handling so the active step still fires before jumping back to the start window.
* Added custom input/output port widgets with 10% scale reduction and optional recolored SVGs.
* Tuned slew response to a logarithmic curve (fixed).
* Cleaned up random button light logic while retaining smoothing.
* Fixed malformed plugin.json version field.

### 2.0.5 — Clock Multipliers & Stability Fixes
*(2025-09-23)*

#### Fixed
* Clock multipliers now trigger the first sub-tick correctly (×2, ×3, ×4… no longer skip).
* Gate/Trigger handling improved for more consistent behavior.
* Removed unused variables and resolved compilation warnings.

#### Changed
* Limited maximum multiplier to ×48 (removed ×64 and ×96, which produced continuous gates).
* Old patches saved with ×64/×96 automatically clamp to ×48 for compatibility.

#### Notes
* Tested with VCV Rack 2.5 SDK.

#### Info and website

- Website: <https://animatek.net>  
- Manual: <https://animatek.net/uzz-en-vcvrack/>  
- Youtube: <https://www.youtube.com/@animatek>  
- Author: **Javier Melgar (Animatek)**  
- License: **GPL-3.0-or-later**
