# Changelog

Registro de cambios de los módulos Animatek. Formato basado en
[Keep a Changelog](https://keepachangelog.com/); las versiones corresponden a
`plugin.json`.

**Regla del repo: no se commitea nada sin apuntar el cambio aquí.**

## [2.5.5]

### Added
- **SIDECHAIN**: nuevo módulo (6HP) — envolvente invertida disparada por
  trigger para hacer ducking y pumping sin compresor. Knobs RECOVERY (20 ms–2 s,
  exponencial), DEPTH y JITTER; entradas TRIG (polifónica, define los canales
  del módulo) y DEPTH CV (10 V = 100 %, sumada y clampeada); salidas OUT
  (reposo 10 V, cae y vuelve) y UP (la misma sin invertir, reposo 0 V), ambas
  polifónicas.
  - **Humanización**: en cada golpe se sortean tiempo de recuperación (±20 %),
    profundidad (±12 %) y exponente de la curva (±15 %) a JITTER 100 %,
    escalados linealmente por el knob. Con JITTER a 0 es determinista.
  - Los valores vienen de un **random walk correlacionado** (a = 0.7), no de
    ruido blanco: el azar puro suena aleatorio, el correlacionado suena humano.
    El paso usa `√(1−a²)` en vez de `(1−a)`; medido sobre 2 M de muestras, con
    `(1−a)` la desviación se queda en 0.24 y el rango nominal nunca se alcanza
    (±4.6 % efectivo en RECOVERY en vez de ±20 %).
  - **Un generador independiente por canal de polifonía**, para que al duckear
    varias pistas cada una respire distinto.
  - Caída fija de 2 ms y meseta de 12 ms: retriggear a mitad de la recuperación
    no produce clicks ni saltos al alza.
  - Menú: forma de la curva (exponencial `p^2.5` / lineal / logarítmica `p^0.4`),
    congelar jitter para comparar A/B, y reset de la semilla. Los tres se
    persisten en el patch.
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
