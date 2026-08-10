# Changelog

Registro de cambios de los módulos Animatek. Formato basado en
[Keep a Changelog](https://keepachangelog.com/); las versiones corresponden a
`plugin.json`.

**Regla del repo: no se commitea nada sin apuntar el cambio aquí.**

## [2.5.5]

### Added
- **UZZ-X**: nuevo expander CV para UZZ (6HP, se acopla a la izquierda).
  Offsets bipolares alrededor del knob para STEPS, START, DIR, RATIO, SWING,
  PROB y ACCUM (1V/incremento en los steppeados, ±5V/±10V en los continuos);
  entrada ADDR de direccionamiento absoluto de step (0–10V sobre la ventana
  activa, anula la navegación); triggers **ROT+/ROT−** que rotan la secuencia
  completa (todas las lanes por-paso + acumuladores) una posición dentro de la
  ventana activa, con wrap; trigger RST de reset de acumuladores; gate REV de
  inversión momentánea FWD↔REV. LED de enlace.

### Fixed
- **UNIT-D**: `polyVoices` y `polyUseVoiceSeeds` (menú contextual) ahora se
  persisten en el patch (`dataToJson`/`dataFromJson`); antes cada recarga
  volvía a 1 voz con seed compartida.
- **UZZ**: la división de clock (RATIO < 1) dejaba de funcionar con clocks más
  lentos de ~60 BPM: el timeout de fase virtual estaba capado a 1 s y mataba
  la fase entre flancos (`ClockProcessor.hpp`). Ahora escala con el período
  medido (×2.5, techo 6 s, alineado con el máximo de período aceptado de 5 s).

### Known issues / Pendiente
- **UNIT-D**: sin `onReset` — "Initialize" no limpia lock loop, historial de
  walk ni posiciones de voz.
- **Apc40Ctrl**: fuerza `midiInput.channel = -1` en cada sample de `process()`;
  anula el selector de canal del menú MIDI (necesario para faders ch 1–8,
  pero debería salir del hot path y ocultarse el selector).
- **OxiCv**: modo Mono sin pila de notas (soltar una nota no recupera la
  anterior aún pulsada).

## [2.5.4] y anteriores

Módulos: UZZ, OXI-CV (ONE) + MULTI, APC40 CTRL, UNIT-D, BLANK 3.
Historial anterior en `git log`.
