# Animatek – UZZ (VCV Rack Plugin)

**UZZ** es un *ultimate ztep zequencer* creado originalmente como **dispositivo de Max for Live**.  
Este repositorio es un **intento de port** del UZZ a **VCV Rack 2.x**, manteniendo su filosofía: secuenciación clara, controles directos y modulación de reloj para ritmos dinámicos.

- Web: https://animatek.net  
- Manual (UZZ): https://animatek.net/ultimate-ztep-zequencer/  
- Autor: **Javier Melgar (Animatek)** – *GPL-3.0-or-later*

> Estado: port en progreso. El objetivo es una versión estable compatible con VCV Rack ≥ **2.4.0** y su publicación en la **VCV Library**.

---

## Características (objetivo del port)
- Secuenciador por pasos con controles inmediatos.
- Modulación de reloj (divisiones/multiplicaciones) para *grooves* vivos.
- Controles de duración/gate y utilidades de *reset/sync*.
- GUI limpia y escalable (SVG), pensada para directos.

 Nota: durante el port pueden cambiar nombres y comportamientos hasta alcanzar paridad con la versión Max for Live.

 # Changelog


 ## 2.1.0 - 2025-10-03
- Added per-row shift controls for Pitch, Octave, Duration, Mod1, and Mod2 including wrap-free behaviour and step-size tweaks (Mod rows now move 10% per click).
- Introduced contextual menu refinements: dedicated submenus for Direction mode, Range Mod 1, Range Mod 2, and a toggle to emit EOC on reset.
- Implemented optional EOC trigger on external reset with persistence.
- Refined reset handling so the active step still fires before jumping back to the start window.
- Added custom input/output port widgets with 10% scale reduction and support for optional recoloured SVGs.
- Tuned slew response to a logarithmic curve as the fixed behaviour.
- Cleaned up random button lights logic to retain the original smoothing behaviour.
- Fixed malformed `plugin.json` manifest (missing closing quote/comma on version).


## [2.0.5] - 2025-09-23
### Fixed
- Clock multipliers now trigger the first sub-tick correctly (×2, ×3, ×4… no longer skip).
- Gate/Trigger handling improved for more consistent behavior.
- Removed unused variables and cleaned up compilation warnings.

### Changed
- Limited maximum multiplier to ×48 (removed ×64 and ×96 which caused continuous gates).
- Old patches saved with ×64/×96 automatically clamp to ×48 for compatibility.

### Notes
- Tested with VCV Rack 2.5 SDK.


