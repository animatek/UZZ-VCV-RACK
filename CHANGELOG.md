# Changelog

Registro de cambios de los módulos Animatek. Formato basado en
[Keep a Changelog](https://keepachangelog.com/); las versiones corresponden a
`plugin.json`.

**Regla del repo: no se commitea nada sin apuntar el cambio aquí.**

## [2.6.0]

### Added
- **ATEK303** (20 HP entre los dos) y **ATEK303 SEQ**: emulación del Roland TB-303 y su
  generador de patrones acid, que llegan desde su propio repo
  ([animatek/ATEK303](https://github.com/animatek/ATEK303)).
  - **ATEK303** (12 HP): VCO de rampa modelado del esquema, ladder de diodos con no
    linealidad por célula, envolvente de decay y acento de dos etapas. Secciones FILTER,
    ENVELOPE y VOICE, con entrada de CV y atenuverter en los seis mandos. Dos modelos de
    sonido conmutables (Circuito / Open303) y ajuste fino por bloques en el menú.
  - **ATEK303 SEQ** (20 HP): generador algorítmico de líneas acid de 16 pasos, con seed
    persistente y bloqueable, y mutación por capas de tiempo, alturas y articulación.
    Funciona como expander de ATEK303 si se pega a su izquierda.
  - Motor **Open303** de Robin Schmidt vendorizado en `dep/open303` (MIT, compatible con
    la GPL-3.0-or-later del plugin), con cinco ganchos documentados en su
    `PROVENANCE.md`. El Makefile lo compila con `-Idep/open303`, y `.gitignore` exceptúa
    ese directorio del `/dep/` ignorado, porque es código fuente y no una dependencia
    descargada.
  - `src/ui/AtekWidgets.hpp` añade `SectionLabel` y `GroupBox` sobre `CommonWidgets.hpp`.
  - **La colección no es la fuente de verdad de estos dos módulos.** El análisis, el banco
    de medida y el generador de paneles se quedan en el repo ATEK303, y de allí llega lo
    esencial con `make sync-apply`. No editar aquí `Atek303*.cpp`, `Atek*.hpp`,
    `Acid*.hpp`, `ui/AtekWidgets.hpp`, `dep/open303` ni `res/ATEK303*.svg`: el script
    avisa si lo has hecho, pero el cambio hay que llevarlo al otro repo.

## [2.5.5]

### Added
- **CAP** (slug `SideChain`): nuevo módulo (6HP) — **VCA de ducking** disparado por trigger,
  para hacer pumping sin compresor. Knobs RECOVERY (40 ms–1 s, exponencial,
  250 ms por defecto), DEPTH y JITTER.
  - **Camino de audio estéreo**: entradas IN L / IN R y salidas OUT L / OUT R.
    **IN R está normalizado a IN L**, así que con un solo cable el módulo hace
    de ducker mono-a-estéreo. Ganancia unidad en reposo.
  - Salida **ENV** con la envolvente como CV (reposo 10 V, cae y vuelve),
    polifónica según los canales de TRIG, para duckear otras cosas en fase.
  - Por defecto **todos los canales de audio comparten una envolvente**, para
    que un par estéreo duckee simétricamente y la imagen no se bambolee. El
    menú "Per-channel envelopes" da a cada canal su propio generador, para
    duckear varias pistas independientes por un cable polifónico.
  - Entradas TRIG (polifónica) y DEPTH CV (10 V = 100 %, sumada y clampeada).
  - **Slider LEVEL + medidor** ocupando la mitad derecha del panel, al estilo
    del VCA-1: el mango fija el techo del VCA (100 % por defecto, o sea audio
    intacto en reposo) y la barra dibuja la ganancia que se está aplicando de
    verdad, así que el duck se ve caer en cada golpe. Los knobs se desplazan
    a la columna izquierda. La barra se dibuja en la capa de luces de Rack
    con un halo suave, así que sigue encendida al bajar el brillo de sala.
    Su **intensidad sigue a la envolvente** (no a la ganancia), de modo que un
    duck se lee dos veces —la barra se acorta y se atenúa— pero bajar el techo
    con el slider solo la acorta, sin apagarla. El número de barras es
    **dinámico**: una en mono, dos en cuanto están conectadas IN L e IN R
    (sea cual sea la polifonía: dos cables mono siguen siendo dos caminos), y
    una por canal —hasta 16— con `Per-channel envelopes`. Cada barra dibuja la ganancia de su
    canal, deliberadamente no el nivel de audio: eso lo convertiría en un VU de
    salida y el ducking dejaría de leerse, que es lo que el medidor tiene que
    contar. La separación entre barras se estrecha al crecer el número, porque
    con 16 una separación fija se comería más de la mitad del ancho.
  - Menú **"Level attenuates ENV"**, apagado por defecto: con él, el slider
    atenúa también la salida ENV y sirve de atenuador de CV. Apagado, ENV sigue
    siendo la envolvente completa de 0 a 10 V. EOC nunca se atenúa, porque un
    trigger a media altura es un trigger que algunos módulos se pierden.
  - Salida **EOC**: trigger de 1 ms cuando la recuperación termina y la
    envolvente vuelve al reposo. Un retrigger que corte la recuperación no
    dispara nada, para que EOC signifique siempre "el duck se ha soltado del
    todo" y no degenere en una copia de TRIG a tempos rápidos.
  - Orden de jacks de arriba abajo agrupado por lo que es cada cosa, no por el
    flujo: primero lo que el módulo fabrica —TRIG · D-CV y, cerrando la mitad
    de arriba con los knobs y el slider, ENV · EOC—, y bajo la raya del panel
    solo el audio, IN L · IN R y después OUT L · OUT R.
  - **Botón de trigger manual** en la cabecera. El nombre del módulo baja a la
    esquina inferior izquierda junto al logo, como en los demás módulos, y ese
    hueco de arriba es el que ocupa el botón; el slider aprovecha para crecer
    de 40 a 51 mm.
  - **Autoparcheo**: con EOC conectado a TRIG, una pulsación del botón lo deja
    oscilando solo. El ciclo es 2 ms de caída + 12 ms de meseta + recuperación,
    o sea de ~1 Hz a ~18.5 Hz, y con jitter ningún ciclo se repite: un
    generador de funciones que un LFO normal no da.
  - **Humanización**: en cada golpe se sortean tiempo de recuperación (±50 %),
    profundidad (±25 %) y exponente de la curva (±30 %) a JITTER 100 %,
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

### Changed
- **UZZ**: el capibara del panel se dibuja en la capa de luces de Rack, con un
  pase tenue constante y el flash encima. Antes estaba en `draw()` y se apagaba
  junto al panel al bajar el brillo de sala; ahora sigue visible y late.

### Fixed
- **UNIT-D**: `polyVoices` y `polyUseVoiceSeeds` (menú contextual) ahora se
  persisten en el patch (`dataToJson`/`dataFromJson`); antes cada recarga
  volvía a 1 voz con seed compartida.
- **UZZ**: la división de clock (RATIO < 1) dejaba de funcionar con clocks más
  lentos de ~60 BPM: el timeout de fase virtual estaba capado a 1 s y mataba
  la fase entre flancos (`ClockProcessor.hpp`). Ahora escala con el período
  medido (×2.5, techo 6 s, alineado con el máximo de período aceptado de 5 s).

### Known issues / Pendiente
- **CAP**: a **audio rate el VCA no modula**. Metiendo un oscilador cuadrado en
  TRIG y subiendo su frecuencia, la salida deja de modularse y se queda en una
  atenuación constante. No es un defecto suelto sino la consecuencia de los
  tiempos fijos de la envolvente: caída de 2 ms + meseta de 12 ms + recuperación
  mínima de 40 ms dan un **ciclo mínimo de 54 ms, o sea un techo de 18.5 Hz**.
  Por encima de eso cada nuevo trigger cae dentro del ataque o la meseta del
  anterior, y como `floorLevel = min(1 - depth, level)` el suelo solo puede
  bajar, el nivel se queda clavado abajo y no vuelve a subir.
  Caminos posibles para la próxima sesión, por orden de menor a mayor cambio:
  (a) escalar ATTACK y HOLD con RECOVERY en vez de dejarlos fijos, de modo que
  con recuperaciones cortas se encojan solos; (b) un modo "fast" de menú que
  reduzca los tres tiempos; (c) asumir que es un ducker y no un VCA de
  modulación, y documentar el techo. La opción (a) es la que menos superficie
  nueva añade, pero hay que comprobar que no reaparecen los clicks que el
  ataque de 2 ms vino a eliminar.
- **CAP**: sin manual de usuario. UNIT-D y Sacromonte tienen el suyo en
  `Manuals/`; el módulo ya tiene tres formas de curva, dos modos de envolvente,
  el atenuador de ENV y el modo LFO por autoparcheo, y el README se queda corto.
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
