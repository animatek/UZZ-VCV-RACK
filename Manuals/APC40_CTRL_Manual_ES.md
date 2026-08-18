# Manual de usuario de APC40 CTRL

**Versión del manual:** 1.0
**Para Animatek:** 2.5.5
**Módulo:** puente MIDI CC a CV fijo APC40 CTRL para VCV Rack

---

## 1. Descripción general

**APC40 CTRL** convierte un conjunto fijo de mensajes MIDI Control Change en CV. Su panel ofrece ocho controles de track, ocho controles de device, Cue, Master, Crossfader y ocho salidas para channel faders.

El módulo es deliberadamente directo: no incluye MIDI learn, remapeo, gestión de botones ni feedback MIDI. Solo procesa mensajes CC. Ignora notas, pitch bend, transporte, clock, aftertouch y program changes.

Los 19 controles principales incluyen un atenuverter cada uno. Las ocho salidas de fader entregan CV unipolar directo, sin atenuverters.

---

## 2. Inicio rápido

1. Añade APC40 CTRL al patch.
2. Haz click derecho en el módulo y selecciona el puerto MIDI del APC40 en la sección MIDI de Rack.
3. Deja un atenuverter principal totalmente a la derecha, en su valor por defecto `+1`.
4. Mueve el control correspondiente del hardware.
5. Conecta su salida a un filtro, VCA, efecto, mixer u otro destino CV.
6. Para los ocho channel faders, conecta directamente `F1` a `F8`. Producen 0-10 V y no tienen atenuverters.

El módulo fuerza la recepción MIDI a Omni porque sus faders usan el mismo número CC en ocho canales MIDI distintos.

---

## 3. Mapa MIDI fijo

### Controles de track

Todos los controles de track escuchan en el canal MIDI 1.

| Panel | Mensaje MIDI | CV original | Con atenuverter |
| --- | --- | --- | --- |
| `T1` | CC 48, canal 1 | 0-10 V | 0 a ±10 V |
| `T2` | CC 49, canal 1 | 0-10 V | 0 a ±10 V |
| `T3` | CC 50, canal 1 | 0-10 V | 0 a ±10 V |
| `T4` | CC 51, canal 1 | 0-10 V | 0 a ±10 V |
| `T5` | CC 52, canal 1 | 0-10 V | 0 a ±10 V |
| `T6` | CC 53, canal 1 | 0-10 V | 0 a ±10 V |
| `T7` | CC 54, canal 1 | 0-10 V | 0 a ±10 V |
| `T8` | CC 55, canal 1 | 0-10 V | 0 a ±10 V |

### Controles de device

Todos los controles de device escuchan en el canal MIDI 1.

| Panel | Mensaje MIDI | CV original | Con atenuverter |
| --- | --- | --- | --- |
| `D1` | CC 16, canal 1 | 0-10 V | 0 a ±10 V |
| `D2` | CC 17, canal 1 | 0-10 V | 0 a ±10 V |
| `D3` | CC 18, canal 1 | 0-10 V | 0 a ±10 V |
| `D4` | CC 19, canal 1 | 0-10 V | 0 a ±10 V |
| `D5` | CC 20, canal 1 | 0-10 V | 0 a ±10 V |
| `D6` | CC 21, canal 1 | 0-10 V | 0 a ±10 V |
| `D7` | CC 22, canal 1 | 0-10 V | 0 a ±10 V |
| `D8` | CC 23, canal 1 | 0-10 V | 0 a ±10 V |

### Controles globales

Los tres escuchan en el canal MIDI 1.

| Panel | Mensaje MIDI | CV original | Con atenuverter |
| --- | --- | --- | --- |
| `CUE` | CC 47, canal 1 | 0-10 V | 0 a ±10 V |
| `MASTER` | CC 14, canal 1 | 0-10 V | 0 a ±10 V |
| `XFAD` | CC 11, canal 1 | 0-10 V | 0 a ±10 V |

### Channel faders

`F1` a `F8` escuchan todos el CC 7 y se distinguen por el canal MIDI.

| Panel | Mensaje MIDI | Salida |
| --- | --- | --- |
| `F1` | CC 7, canal 1 | 0-10 V directo |
| `F2` | CC 7, canal 2 | 0-10 V directo |
| `F3` | CC 7, canal 3 | 0-10 V directo |
| `F4` | CC 7, canal 4 | 0-10 V directo |
| `F5` | CC 7, canal 5 | 0-10 V directo |
| `F6` | CC 7, canal 6 | 0-10 V directo |
| `F7` | CC 7, canal 7 | 0-10 V directo |
| `F8` | CC 7, canal 8 | 0-10 V directo |

Los valores MIDI 0-127 se convierten linealmente a 0-10 V.

---

## 4. Atenuverters y salidas

Cada salida `T1-T8`, `D1-D8`, `CUE`, `MASTER` y `XFAD` tiene un atenuverter dedicado con rango de -1 a +1 y valor por defecto +1.

- `+1`: respuesta positiva completa, 0-10 V.
- Entre `0` y `+1`: respuesta positiva reducida.
- `0`: la salida permanece en 0 V independientemente del valor MIDI.
- Entre `0` y `-1`: respuesta invertida reducida.
- `-1`: respuesta con polaridad invertida completa, 0 a -10 V.

El atenuverter multiplica un CV unipolar por una escala bipolar. No desplaza la señal alrededor de un punto central. En `-1`, el valor MIDI 0 sigue produciendo 0 V y el valor MIDI 127 produce -10 V.

`F1-F8` omiten esta etapa. Sus salidas siempre son conversiones directas a 0-10 V de CC 7 en los canales 1-8.

---

## 5. Comportamiento MIDI

- La recepción se fuerza a `Omni`; una selección de canal guardada no puede restringirla.
- Solo se procesan mensajes MIDI CC (`Control Change`).
- CC 7 en los canales 1-8 actualiza respectivamente `F1-F8`.
- Los CC fijos de track, device, Cue, Master y Crossfader solo se actualizan si se reciben en el canal 1.
- Se ignoran los demás números CC y los CC recibidos en canales no relacionados.
- No hay smoothing ni modo pickup; cada valor CC aceptado se convierte inmediatamente en el voltaje objetivo actual.
- El indicador de actividad MIDI solo parpadea con mensajes CC mapeados y aceptados.
- El módulo solo recibe datos. No envía feedback de LEDs, anillos, motor faders, valores ni ningún otro tipo al hardware.

Como CC 7 en el canal 1 está reservado para `F1`, no controla ninguna salida principal con atenuverter.

---

## 6. Menú de click derecho

APC40 CTRL solo añade la sección de selección MIDI estándar de Rack a su menú contextual. Úsala para seleccionar el driver MIDI y el dispositivo de entrada APC40.

No hay menús del módulo para:

- MIDI learn o reasignación de CC.
- Presets de controlador o variantes de hardware.
- Mapeo de botones.
- Selección de rango de salida.
- Smoothing.
- Configuración de feedback.

La recepción MIDI permanece en Omni aunque aparezca otro canal en una configuración MIDI cargada.

---

## 7. Persistencia y reinicio

Se guarda con el patch de Rack:

- Configuración de driver/dispositivo MIDI.
- Posiciones de los 19 parámetros de atenuverter.

No se guarda:

- Valores CC actuales de los controles de track.
- Valores CC actuales de los controles de device.
- Valores actuales de Cue, Master y Crossfader.
- Valores actuales de `F1-F8`.
- Estado del indicador de actividad MIDI.

Los valores de ejecución se ponen a 0 V al hacer reset del módulo y empiezan en 0 V en un estado de ejecución nuevo. Mueve los controles de hardware o haz que el controlador reenvíe sus valores para volver a llenar las salidas. Los parámetros y el dispositivo MIDI son configuración del patch y persisten al guardarlo; los valores vivos del controlador no son instantáneas.

---

## 8. Ejemplos prácticos

### Ocho macros de filtro

1. Conecta `T1-T8` a las entradas CV de cutoff de ocho voces.
2. Deja los atenuverters en positivo para un funcionamiento convencional.
3. Reduce cada atenuverter para mantener la modulación dentro de un rango musical.
4. Invierte algunos tracks si un filtro debe cerrarse mientras otro se abre.

### Controles de device para un banco de efectos

1. Conecta `D1-D8` a tiempo de delay, feedback, mezcla de reverb, distorsión u otros parámetros de efectos.
2. Ajusta los atenuverters de forma independiente para definir profundidad y polaridad.
3. Usa la sección device control del hardware como superficie física de efectos.

### Control de mixer

1. Conecta `F1-F8` a ocho entradas CV de nivel de VCAs o mixer que acepten 0-10 V.
2. Usa `MASTER` para un VCA final o nivel macro.
3. Usa `CUE` para el nivel de auriculares o envío de efecto si el destino ofrece control CV.
4. Usa `XFAD` para una entrada CV de crossfader; ajusta su atenuverter al rango y polaridad esperados por el destino.

### Modulación opuesta

1. Conecta una salida con atenuverter a un parámetro.
2. Ajusta su atenuverter a un valor negativo.
3. Al subir el control de hardware, la salida se moverá desde 0 V hacia un voltaje negativo.
4. Si el destino necesita un rango positivo invertido, por ejemplo de 10 V a 0 V, añade una utilidad de offset/inversión; el atenuverter integrado invierte la polaridad, pero no añade un offset de 10 V.

---

## 9. Advertencia sobre el mapa de hardware

APC40 CTRL sigue exactamente el mapa de CC/canales indicado en este manual. Las revisiones del APC40, modos de funcionamiento, plantillas del host, traductores MIDI, firmware personalizado y configuración del controlador pueden cambiar los mensajes enviados por el hardware. La etiqueta física o posición de un control no garantiza que su mensaje de salida coincida con el mapa fijo del módulo.

Si un control no responde:

1. Confirma que Rack recibe el puerto MIDI correcto.
2. Inspecciona el controlador con un monitor MIDI.
3. Comprueba el número CC y canal con las tablas anteriores.
4. Desactiva el control exclusivo o routing de superficies de control del DAW si impide que Rack reciba los datos.
5. Si el mapa del hardware es distinto, traduce el MIDI externamente; APC40 CTRL no puede aprender ni remapear mensajes.

---

## 10. Limitaciones y solución de problemas

- Ninguna salida cambia hasta recibir su mensaje CC mapeado.
- Las posiciones del controlador no se recuperan desde un patch guardado. Reenvía o mueve los controles después de cargarlo.
- Los botones del APC40 se ignoran aunque envíen MIDI.
- MIDI Clock y transporte no producen salidas.
- No hay comunicación bidireccional ni feedback visual hacia el controlador.
- Las salidas principales pueden producir voltaje negativo cuando sus atenuverters están en negativo. Confirma que el destino admite CV bipolar.
- `F1-F8` no tienen atenuación, inversión, smoothing ni remapeo. Añade módulos de utilidad cuando necesites estas funciones.
- Si solo funciona `F1`, comprueba que los faders del hardware transmitan CC 7 en los canales 1-8 y no todos en el canal 1.
- Si los controles `T`, `D` o globales fallan pero los faders funcionan, comprueba que esos CC fijos se envían en el canal MIDI 1.
