# Animatek 2.5.6: nuevas máquinas para VCV Rack

> **Imagen de portada sugerida:** `Manuals/animatekModules_all.png`

La colección de módulos Animatek sigue creciendo. Después de varios meses de desarrollo, pruebas y rediseño, ya tenemos **11 módulos** reunidos bajo una misma identidad visual.

Esta actualización incorpora nuevas herramientas para crear secuencias acid, controlar patches mediante CV, generar modulación y añadir movimiento a nuestras sesiones.

## ATEK303 + ATEK303 SEQ

La gran incorporación es **ATEK303**, una voz monofónica inspirada en la arquitectura de la clásica TB-303.

Incluye oscilador, filtro ladder de diodos, envolvente, acento, slide y saturación. Todos sus controles principales pueden modularse por CV.

ATEK303 ofrece dos modelos sonoros:

- **Circuit**, basado en el comportamiento del circuito y con un carácter más analógico.
- **Open303**, más limpio y cercano al motor original de referencia.

A su lado llega **ATEK303 SEQ**, un generador algorítmico de patrones acid de 16 pasos.

No es un secuenciador tradicional: una semilla genera notas, silencios, ties, acentos y slides. Después podemos bloquear su identidad y crear mutaciones controladas del ritmo, las notas, las octavas y la articulación.

Cuando ambos módulos están juntos, pitch, gate, accent y slide viajan directamente mediante la conexión interna de expander, sin necesidad de cuatro cables adicionales.

## CAP: ducking con vida propia

**CAP** es un VCA estéreo de ducking disparado por trigger, diseñado para conseguir pumping sin necesidad de utilizar un compresor.

Su control **JITTER** introduce pequeñas variaciones correlacionadas en cada golpe. La recuperación, la profundidad y la curva cambian ligeramente, haciendo que el movimiento respire de una forma menos mecánica.

También incluye:

- Ruta de audio estéreo.
- Funcionamiento polifónico.
- Salida de envolvente.
- Salida de fin de ciclo.
- Trigger manual.
- Medidor dinámico integrado.
- Distintas curvas de recuperación.

Además, conectando **EOC a TRIG**, CAP puede funcionar como un generador de funciones autónomo cuyos ciclos nunca son exactamente iguales.

## UZZ-X: control CV para UZZ

**UZZ-X** amplía UZZ con control por voltaje sobre los principales parámetros del secuenciador:

- Número de pasos.
- Inicio de la secuencia.
- Dirección.
- Ratio de reloj.
- Swing.
- Probabilidad.
- Acumulador.
- Direccionamiento absoluto de pasos.
- Rotación de la secuencia.
- Inversión momentánea.

Es una forma de convertir UZZ en un instrumento todavía más modulable y performativo.

## BLANK ACID y lienzos compartidos

La colección incorpora **BLANK ACID**, un panel animado de 3 HP con smileys acid.

Los paneles BLANK 3 y BLANK ACID contiguos se comportan ahora como un único lienzo. Las figuras pueden desplazarse entre paneles, compartir velocidad y utilizar automáticamente la paleta de colores Animatek.

No generan sonido, pero definitivamente generan ambiente.

## Más novedades

También hemos aprovechado esta actualización para:

- Unificar toda la colección con paneles oscuros.
- Mejorar el comportamiento visual en el navegador de módulos.
- Corregir el funcionamiento de UZZ con clocks lentos.
- Guardar correctamente las opciones polifónicas de UNIT-D.
- Añadir manuales completos en español e inglés.
- Revisar paneles, etiquetas, puertos y descripciones.
- Resolver los avisos del análisis estático de la librería de VCV Rack.

La versión **Animatek 2.5.6** ya está publicada en GitHub y preparada para su incorporación a la VCV Library.

Gracias por apoyar el desarrollo de estos módulos. Todavía quedan ideas extrañas por convertir en voltajes.

**Javier Melgar / Animatek**

---

# Animatek 2.5.6: new machines for VCV Rack

> **Suggested cover image:** `Manuals/animatekModules_all.png`

The Animatek module collection keeps growing. After several months of development, testing and redesign, it now brings together **11 modules** under one visual identity.

This update introduces new tools for creating acid sequences, controlling patches with CV, generating modulation and bringing more movement into our sessions.

## ATEK303 + ATEK303 SEQ

The headline addition is **ATEK303**, a monophonic voice inspired by the architecture of the classic TB-303.

It includes an oscillator, diode ladder filter, envelope, accent, slide and saturation. Every main sound control can be modulated with CV.

ATEK303 offers two sound models:

- **Circuit**, based on circuit behaviour and designed for a more analogue character.
- **Open303**, a cleaner option closer to the original reference engine.

Joining it is **ATEK303 SEQ**, an algorithmic 16-step acid pattern generator.

This is not a conventional step sequencer. A seed generates notes, rests, ties, accents and slides. You can then lock its identity and create controlled mutations of the rhythm, notes, octaves and articulation.

When both modules are placed together, pitch, gate, accent and slide travel through the internal expander connection, with no need for four additional cables.

## CAP: ducking with a life of its own

**CAP** is a trigger-driven stereo ducking VCA designed to create pumping without a compressor.

Its **JITTER** control introduces small, correlated variations on every hit. Recovery time, depth and curve change slightly, allowing the movement to breathe instead of repeating the exact same mechanical shape.

It also includes:

- A stereo audio path.
- Polyphonic operation.
- Envelope output.
- End-of-cycle output.
- Manual trigger.
- Integrated dynamic meter.
- Multiple recovery curves.

By patching **EOC back into TRIG**, CAP can also become a self-running function generator whose cycles are never exactly the same.

## UZZ-X: CV control for UZZ

**UZZ-X** expands UZZ with voltage control over the sequencer's main parameters:

- Sequence length.
- Start position.
- Direction.
- Clock ratio.
- Swing.
- Probability.
- Accumulator.
- Absolute step addressing.
- Sequence rotation.
- Momentary reverse.

It turns UZZ into an even more modulatable and performance-oriented instrument.

## BLANK ACID and shared canvases

The collection now includes **BLANK ACID**, an animated 3 HP blank panel filled with acid smileys.

Adjacent BLANK 3 and BLANK ACID modules now behave as a single shared canvas. The graphics can travel between panels, share their animation speed and automatically use the Animatek colour palette.

They do not generate sound, but they definitely generate atmosphere.

## More improvements

This update also gave us the opportunity to:

- Unify the collection with dark-only panels.
- Improve how modules appear in the module browser.
- Fix UZZ operation with slow clocks.
- Correctly save UNIT-D polyphonic settings.
- Add complete manuals in English and Spanish.
- Refine panels, labels, ports and module descriptions.
- Resolve the static-analysis findings reported by the VCV Rack library checks.

**Animatek 2.5.6** is now available on GitHub and ready for inclusion in the VCV Library.

Thank you for supporting the development of these modules. There are still plenty of strange ideas waiting to be turned into voltages.

**Javier Melgar / Animatek**
