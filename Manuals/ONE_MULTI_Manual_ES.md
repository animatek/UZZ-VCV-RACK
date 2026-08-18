# Manual de usuario de ONE / MULTI

**Versión del manual:** 1.0
**Para Animatek:** 2.5.5
**Módulos:** interfaz MIDI a CV ONE y expansor MULTI para VCV Rack

---

## 1. Descripción general

**ONE** es una interfaz MIDI a CV compacta diseñada para convertir notas, controladores, reloj y transporte. Ofrece conversión de voces monofónica y polifónica, funcionamiento multitrack por canales, ocho salidas CC asignables y salidas de sincronización MIDI.

ONE no tiene knobs en el panel ni entradas CV. La fuente MIDI y el comportamiento se configuran desde el menú de click derecho de Rack. El display muestra el modo actual y el canal MIDI seleccionado; su indicador de actividad responde a mensajes MIDI de canal.

**MULTI** es el expansor complementario de ocho tracks. Cada track tiene un selector de canal MIDI independiente y salidas dedicadas `V/OCT`, `GATE` y `VEL`. Coloca MULTI directamente a la derecha de ONE para que la conexión de expansor pueda transportar los datos de voz derivados de MIDI y el pitch bend global.

El flujo de trabajo compatible con ONE + MULTI usa ONE en modo **Multitrack** o **Matriceal**, con la recepción MIDI ajustada a **Omni**.

---

## 2. Inicio rápido

### Una voz monofónica

1. Añade ONE al patch.
2. Haz click derecho en ONE y selecciona driver y dispositivo en la sección MIDI de Rack.
3. En `Play Mode`, selecciona `Mono`.
4. Selecciona el canal MIDI del controlador, o `Omni` si corresponde.
5. Conecta `V/OCT` a un oscilador, `GATE` a una envolvente y `VEL` a un destino sensible a velocity.
6. Toca una nota. La nota MIDI 60 produce 0 V antes del pitch bend, el gate es de 10 V mientras está activo y velocity se convierte a 0-10 V.

### Ocho tracks por canal

1. Coloca MULTI directamente a la derecha de ONE.
2. Selecciona `Multitrack` en ONE y comprueba que el canal MIDI de Rack sea `Omni`.
3. Ajusta los ocho selectores `CH` de MULTI a los canales de origen. Los valores iniciales son los canales 1 a 8.
4. Conecta las salidas `V/OCT`, `GATE` y `VEL` de cada fila de MULTI a una voz distinta.
5. Envía una parte musical por cada canal MIDI seleccionado.

---

## 3. Convenciones de notas MIDI y CV

- El pitch sigue el estándar de 1 V/octava: la nota MIDI 60 es 0 V y cada semitono suma o resta 1/12 V.
- El pitch bend MIDI global actual se añade a todos los canales de pitch de ONE y a todas las salidas de pitch del MULTI conectado.
- Los gates activos son de 10 V; los inactivos son de 0 V.
- MIDI velocity 0-127 se convierte linealmente a 0-10 V.
- Un Note On con velocity 0 se interpreta como Note Off.
- MIDI CC 123 (`All Notes Off`) baja todos los gates de voz.
- MIDI Stop baja todos los gates de voz y pone `RUN` a nivel bajo.
- Los valores de pitch y velocity pueden conservar su último valor después de bajar el gate. Usa el gate para determinar si una voz está activa.

---

## 4. Modos de ONE

### Mono

Produce un canal en `V/OCT`, `GATE` y `VEL`. El Note On más reciente se convierte en la nota actual.

Mono no tiene pila de notas mantenidas ni recuperación por prioridad de última nota. Si se solapan varias notas, al soltar la nota actual baja el gate; ONE no vuelve automáticamente a una nota anterior que siga pulsada. Para líneas monofónicas fiables, evita solapamientos o deja que la fuente gestione la prioridad de notas.

### Poly

Produce cables polifónicos de ocho canales en `V/OCT`, `GATE` y `VEL`. Las notas ocupan la primera voz disponible. Repetir un pitch activo actualiza su velocity sin asignar otra voz. Si las ocho voces están ocupadas, las notas nuevas roban voces por turno circular.

### Chord

Usa el mismo asignador de ocho voces y las mismas salidas polifónicas que Poly. La opción `Chord: unison gate` solo cambia el comportamiento de gate:

- Desactivada: cada voz tiene su propio gate.
- Activada: los ocho canales de gate están altos mientras cualquier voz asignada esté activa.

Pitch y velocity siguen siendo independientes por voz. La opción unison está desactivada por defecto.

### Multitrack

Produce cables polifónicos de ocho canales en `V/OCT`, `GATE` y `VEL`. Los canales MIDI 1-8 se asignan directamente a los canales polifónicos 1-8. Cada canal MIDI conserva una sola nota actual; un Note On nuevo en ese canal sustituye su estado de nota actual.

Usa recepción Omni para que ONE pueda recibir los ocho canales de origen.

### Matriceal

Produce cables polifónicos de cuatro canales en `V/OCT`, `GATE` y `VEL`. Los canales MIDI 1-4 se asignan directamente a los canales polifónicos 1-4, con una nota actual por canal MIDI.

Usa recepción Omni para que ONE pueda recibir los cuatro canales de origen.

---

## 5. Salidas de ONE

### Salidas de voz

| Salida | Comportamiento |
| --- | --- |
| `V/OCT` | Pitch a 1 V/oct, nota MIDI 60 = 0 V, más pitch bend global. Uno, ocho o cuatro canales según el modo. |
| `GATE` | 0 o 10 V. Uno, ocho o cuatro canales según el modo. |
| `VEL` | MIDI velocity convertida linealmente a 0-10 V. Uno, ocho o cuatro canales según el modo. |

### Salidas CC asignables

`CC1` a `CC8` convierten individualmente el valor 0-127 del CC MIDI asignado a 0-10 V. Las asignaciones son globales, no por canal MIDI: cualquier mensaje CC recibido que supere el filtro de entrada/canal MIDI de Rack puede actualizar una salida coincidente.

| Salida | Asignación por defecto |
| --- | --- |
| `CC1` | CC 1, Mod Wheel |
| `CC2` | CC 7, Volume |
| `CC3` | CC 10, Pan |
| `CC4` | CC 74, Filter Cutoff |
| `CC5` | CC 71, Resonance |
| `CC6` | CC 91, Reverb Send |
| `CC7` | CC 93, Chorus Send |
| `CC8` | CC 11, Expression |

Se puede asignar el mismo CC a varias salidas; todas las salidas coincidentes seguirán ese valor.
La excepción es CC 123: ONE lo consume siempre como `All Notes Off`, por lo que
no actualiza una salida aunque el menú permita seleccionarlo.

### Salidas de sincronización

| Salida | Comportamiento |
| --- | --- |
| `CLOCK` | Emite un pulso de 10 V y 1 ms por cada tick de MIDI Clock recibido (`F8`). |
| `CLK÷n` | Emite un pulso de 10 V y 1 ms según la división seleccionada: ÷1, ÷2, ÷4, ÷8, ÷16 o ÷24. Por defecto: ÷24. |
| `RUN` | 10 V después de MIDI Start o Continue; 0 V después de MIDI Stop. |

La división cuenta ticks MIDI entrantes, no tiempos musicales por sí sola. Con el MIDI Clock estándar de 24 PPQN, ÷24 produce un pulso por negra. MIDI Start reinicia el contador de división; cambiar la división también reinicia su cuenta.

---

## 6. Menú de click derecho de ONE

### Sección MIDI de Rack

Selecciona driver, dispositivo y canal de entrada con el menú MIDI estándar de Rack. El filtrado por canal afecta a mensajes de canal como notas, CC y pitch bend. Los mensajes de sistema en tiempo real Clock, Start, Continue y Stop no tienen canal MIDI.

Al seleccionar `Multitrack` o `Matriceal`, el modo cambia la entrada a Omni. Comprueba `Omni` después de cargar o reconfigurar un patch para asegurar que todos los canales necesarios llegan a ONE.

### Play Mode

Selecciona `Mono`, `Poly`, `Chord`, `Multitrack` o `Matriceal`. Cambiar de modo limpia los gates activos del asignador y de los tracks para evitar que asignaciones antiguas pasen al modo nuevo.

### Chord: unison gate

Cuando está activado en modo Chord, los ocho canales de gate siguen el OR lógico de todas las voces activas. No tiene un efecto útil en los demás modos.

### Pitch Bend Range

Ajusta el rango global de bend entre ±1 y ±12 semitonos. Valor por defecto: ±2 semitonos. Configura el mismo rango en el controlador o secuenciador emisor para mantener una afinación predecible.

### CLK/n Division

Selecciona ÷1, ÷2, ÷4, ÷8, ÷16 o ÷24. Valor por defecto: ÷24.

### CC Assignments

Cada una de las ocho salidas CC se puede asignar de forma independiente a cualquier número MIDI CC entre 0 y 127. El menú agrupa los números en bancos de 16 y muestra nombres estándar cuando están disponibles. CC 123 permanece reservado para `All Notes Off` y no genera CV.

---

## 7. Controles y salidas de MULTI

MULTI tiene ocho filas idénticas y no tiene entradas ni configuración propia de click derecho aparte de los comandos estándar de módulo de Rack.

| Elemento | Comportamiento |
| --- | --- |
| Selector `CH` | Selecciona el canal MIDI 1-16 de esa fila. Los selectores saltan entre números enteros. Por defecto, las filas 1-8 usan los canales 1-8. |
| `V/OCT` | Nota actual del canal MIDI seleccionado, a 1 V/oct y con el pitch bend global de ONE. |
| `GATE` | 10 V mientras la nota actual del canal seleccionado esté activa; en otro caso, 0 V. |
| `VEL` | Velocity actual del canal seleccionado, convertida a 0-10 V. |

Cada canal MIDI conserva una nota actual. Un Note On más reciente en el mismo canal sustituye a la nota anterior. Un Note Off baja el gate solo si coincide con la nota actual de ese canal.

Varias filas pueden seleccionar el mismo canal MIDI para crear copias CV. Si MULTI no está presente, se separa de ONE o no está inmediatamente a su derecha, todas sus salidas son 0 V.

Aunque MULTI recibe el estado de canales seguido por ONE, la disposición de funcionamiento compatible es ONE en modo Multitrack o Matriceal y con recepción Omni. MULTI puede seleccionar cualquiera de los 16 canales MIDI, independientemente de los cuatro u ocho canales polifónicos disponibles en las propias salidas de voz de ONE.

---

## 8. Persistencia y reinicio

Se guarda con el patch de Rack:

- Configuración de driver/dispositivo/canal MIDI de ONE.
- Modo de ONE.
- Ajuste de unison gate para Chord.
- Rango de pitch bend.
- División de clock.
- Las ocho asignaciones CC.
- Los ocho parámetros de selección de canal MIDI de MULTI.

No se guarda como estado musical de ejecución:

- Notas mantenidas y gates.
- Velocities y voltajes CC actuales.
- Posición actual de pitch bend.
- Estado de transporte/run MIDI.
- Fase del divisor de clock y pulsos de clock pendientes.

Después de un reset o de empezar con un estado de ejecución nuevo, vuelve a enviar los datos MIDI necesarios. Guardar un patch no convierte las notas mantenidas ni las posiciones de controladores en una instantánea recuperable.

---

## 9. Ejemplos prácticos

### Lead mono expresivo

1. Usa Mono y selecciona el canal del controlador.
2. Conecta `V/OCT`, `GATE` y `VEL` a una voz de sintetizador mono.
3. Conecta `CC1` (Mod Wheel) a la profundidad de vibrato.
4. Conecta `CC4` (Filter Cutoff) al cutoff del filtro mediante un atenuador.
5. Iguala el rango de bend del controlador con el ajuste de ONE.

### Instrumento polifónico de ocho voces

1. Selecciona Poly.
2. Conecta las tres salidas de voz de ONE a entradas polifónicas de oscilador, envolvente y velocity.
3. Mantén desactivado el unison gate para articular cada voz de forma independiente.
4. Limita la fuente a ocho notas simultáneas si necesitas una asignación de voces predecible.

### Stabs de acordes con articulación compartida

1. Selecciona Chord y activa `Chord: unison gate`.
2. Envía notas de acorde a ONE.
3. Usa los pitches polifónicos con una respuesta de gate común en los ocho canales.
4. Recuerda que los canales sin usar también reciben el unison gate; comprueba que el destino gestione como esperas los pitches inactivos o antiguos.

### Sistema multitrack para directo

1. Coloca MULTI directamente a la derecha de ONE.
2. Selecciona Multitrack y comprueba Omni.
3. Asigna tracks del secuenciador a los canales MIDI 1-8.
4. Conserva los valores por defecto de los selectores de MULTI y conecta cada fila a una voz modular distinta.
5. Usa las salidas CC de ONE como macros compartidas y `CLOCK`, `CLK÷n` y `RUN` para sincronizar.

### MIDI clock como master

1. Activa la salida MIDI Clock del secuenciador externo.
2. Conecta `CLOCK` a dispositivos que necesiten cada tick de 24 PPQN.
3. Ajusta `CLK÷n` a ÷24 para pulsos de negra o usa otra división para pulsos más densos.
4. Conecta `RUN` a una entrada run o destino lógico que acepte un gate de transporte de 10 V.

---

## 10. Limitaciones y solución de problemas

- ONE recibe MIDI; no genera MIDI ni envía feedback al controlador.
- No hay entradas, knobs, gestos MIDI learn ni control CV de los ajustes de menú.
- Mono no recuerda notas antiguas mantenidas. Evita solaparlas si necesitas volver a la última nota anterior.
- Poly y Chord están limitados a ocho voces asignadas. Las notas adicionales roban voces.
- Multitrack y Matriceal se basan en canales y son monofónicos dentro de cada canal MIDI; no ofrecen polifonía interna por track.
- El pitch bend global afecta a todas las voces de ONE y filas de MULTI; no hay bend independiente por canal.
- Las asignaciones CC responden por número de CC después del filtrado MIDI de Rack; no son lanes CC independientes por canal.
- Las salidas de clock necesitan MIDI Clock entrante. El tráfico de notas por sí solo no genera pulsos.
- `RUN` solo sigue el transporte MIDI; no se deduce de notas ni ticks de clock.
- Si MULTI permanece a 0 V, confirma que está inmediatamente a la derecha de ONE, que ONE recibe MIDI, que los selectores coinciden con los canales de origen y que Multitrack/Matriceal usa Omni.
- Si faltan notas en Multitrack o Matriceal, revisa primero el canal MIDI de Rack. Un filtro de canal único impide que los demás canales de track lleguen a ONE.
