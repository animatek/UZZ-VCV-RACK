# ATEK303 - Manual de usuario

**Versión del manual:** 1.0
**Versión del plugin:** Animatek 2.5.5
**Módulo:** ATEK303, voz acid monofónica de 12 HP para VCV Rack

## 1. Concepto

ATEK303 es una voz monofónica completa inspirada en el flujo de señal y la interpretación de la TB-303: oscilador, filtro paso bajo resonante, envolvente de decay, acento, slide, VCA y etapa de salida. Está pensada tanto como compañera directa de ATEK303 SEQ como para funcionar como una voz modular normal controlada mediante cables.

Hay dos presets de modelo sonoro:

- **Circuit (ATEK)** favorece el comportamiento del oscilador y del filtro ladder de diodos modelados a partir del circuito, curvas de control propias del circuito, slide en el dominio del pitch, acumulación de acento, saturación y variación analógica sutil.
- **Open303 original** selecciona el comportamiento de referencia Open303, más limpio, junto con sus rangos y curvas originales.

El preset elige un grupo coherente de ajustes estructurales. Todos los detalles siguen disponibles en **Fine tuning**, por lo que mezclar motores y comportamientos cambia la indicación del modelo a **Custom**. Los menús presentan calibraciones musicales sin exigir conocimientos del DSP interno.

## 2. Inicio rápido

### Con ATEK303 SEQ

1. Coloca ATEK303 SEQ inmediatamente a la izquierda de ATEK303, sin dejar ningún hueco.
2. Envía un reloj externo a la entrada `CLOCK` del secuenciador.
3. Conecta `OUT` de ATEK303 a un mezclador o VCA y empieza con un nivel prudente.
4. Pulsa `GENERATE` en el secuenciador.
5. Ajusta `CUT OFF`, `RESONANCE`, `ENV MOD`, `DECAY` y `ACCENT`.

Pitch, gate, accent y slide viajan por la conexión de expander sin necesidad de cuatro cables visibles. Un cable insertado en cualquiera de las entradas correspondientes de la voz sustituye solo esa señal del expander.

### Con otro secuenciador

1. Conecta pitch a `V/OCT` y gate a `GATE`.
2. Si lo deseas, conecta gates o triggers a `ACC` y `SLIDE`.
3. Selecciona sierra o cuadrada y conecta `OUT` a un mezclador, VCA o efecto.
4. Si la fuente entrega pitch continuo, activa **Quantize pitch to semitones** cuando quieras pasos cromáticos.

## 3. Entradas del panel

### V/OCT

Entrada de pitch de 1 V/oct con la convención de VCV de 0 V = C4. El pitch es continuo salvo que se active la cuantización del menú. **Limit to the 303 range (C1-C4)** limita la nota interpretada; no atenúa el voltaje entrante. Con la cuantización desactivada, la fracción de semitono se reaplica como pitch bend y puede superar los extremos C1/C4 en casi medio semitono.

### GATE

Inicia y libera notas. La detección usa histéresis: se activa a 1 V y permanece activa hasta caer por debajo de 0.1 V. Esto evita el efecto del ruido cerca del umbral.

### ACC

Marca un ataque como acentuado cuando alcanza 1 V o más. El acento se captura al comenzar la nota y afecta al volumen, al movimiento del filtro y a la envolvente de acento. El LED rojo sigue el estado lógico de esta entrada.

### SLIDE

Solicita una transición de pitch en legato. Usa la misma histéresis de activación a 1 V y desactivación a 0.1 V que `GATE`. Si el gate baja mientras `SLIDE` sigue alto, la voz se mantiene activa para la nota siguiente en vez de liberarse. El LED azul muestra el estado lógico de slide.

### Seis entradas de modulación

`CUT OFF CV`, `RESONANCE CV`, `ENV MOD CV`, `DECAY CV`, `ACCENT CV` y `TUNING CV` modulan el control situado justo encima. Cada una dispone de un atenuversor bipolar:

- Centro: sin modulación, aunque haya un cable conectado.
- Sentido horario: modulación positiva.
- Sentido antihorario: modulación invertida.
- Con el atenuversor al máximo, el intervalo de -5 V a +5 V recorre todo el rango del control.
- La suma del mando y el CV se limita al rango válido del control.

## 4. Controles del panel

### CUT OFF

Ajusta el cutoff del filtro aproximadamente entre 314 Hz y 2394 Hz antes de la deriva analógica. La posición inicial es 35%. La respuesta es exponencial para ofrecer una resolución útil en todo el recorrido.

### RESONANCE

Ajusta la resonancia del filtro de 0% a 100%; el valor inicial es 50%. El modelo Circuit usa una respuesta lineal acorde con la ley del control del circuito original. Open303 usa su curva de resonancia de referencia.

### ENV MOD

Ajusta la intensidad con la que la envolvente de decay barre el filtro, de 0% a 100%. La posición inicial del mando es 50%; con la curva logarítmica predeterminada se muestra y se comporta como aproximadamente 12.5%. Por ello, el movimiento más intenso se concentra en la zona superior, como en un potenciómetro de audio. La curva puede hacerse lineal desde el menú.

### DECAY

Ajusta el decay de la envolvente en una escala temporal logarítmica. El rango mostrado depende del menú:

- **Circuit:** 200 ms a 2.5 s.
- **Open303:** aproximadamente 460 ms a 4.6 s.

El acento usa el decay mínimo del rango seleccionado, lo que reproduce una respuesta acentuada más cerrada.

### ACCENT

Ajusta la intensidad del acento de 0% a 100%; el valor inicial es 50%. Define cuánto enfatiza una nota una señal activa en `ACC`. Con la acumulación activada, los acentos consecutivos pueden sumar énfasis hasta el límite interno seguro.

### TUNING

Ajusta la afinación de referencia del oscilador entre 400 Hz y 480 Hz; el valor inicial es 440 Hz. Es una afinación global, independiente del pitch de `V/OCT`.

### Selector de onda

Selecciona **sierra** a la izquierda o **cuadrada** a la derecha. El valor inicial es la onda cuadrada. Las opciones detalladas permiten alterar el ancho de pulso, la forma del reset y el droop de las ondas.

## 5. Salida

### OUT

Salida de audio monofónica con nivel completo y fijo. No hay control de nivel en el panel: usa un VCA, mezclador o atenuador externo. La señal final está limitada por seguridad a +/-12 V, pero el acento y la saturación aún pueden producir una señal mucho más fuerte que una nota sin acento. Baja el canal receptor antes de probar ajustes agresivos.

## 6. Expander y prioridad de cables

ATEK303 recibe `V/OCT`, `GATE`, `ACC` y `SLIDE` de un ATEK303 SEQ colocado directamente a su izquierda. No hace falta configurarlo en el navegador de módulos ni seleccionar ninguna opción de menú.

La prioridad se evalúa por separado para las cuatro señales:

1. Un jack cableado en ATEK303 tiene prioridad.
2. Si ese jack no tiene cable, se usa la señal correspondiente del expander.
3. Si ninguna está disponible, el pitch queda en 0 V y las entradas lógicas quedan bajas.

Por ejemplo, conectar solo `V/OCT` permite que otro secuenciador transponga o sustituya el pitch mientras ATEK303 SEQ sigue proporcionando gate, accent y slide. Conectar solo `ACC` sustituye los acentos del expander sin alterar las otras tres señales.

El secuenciador envía el pitch de cada paso sin glide cuando está conectado y desactiva su propio glide de salida, de modo que ATEK303 realiza el slide una sola vez en el dominio de pitch o frecuencia seleccionado.

## 7. Menú contextual

Haz clic derecho sobre el módulo para abrir estos ajustes.

### Sound model

- **Circuit (ATEK):** selecciona oscilador y filtro orientados al circuito, rango de decay Circuit, saturación de salida fuerte, saturación OTA suave, deriva sutil, resonancia lineal, slide en el dominio del pitch, acumulación de acento y curva logarítmica de Env Mod.
- **Open303 original:** selecciona oscilador y filtro Open303, decay y slide Open303, sin saturación ni deriva añadidas, curva de resonancia Open303, sin acumulación de acento y Env Mod lineal.
- **Custom:** es un indicador que aparece cuando los ajustes actuales no coinciden con ninguno de los dos presets completos; no es un preset independiente.

Elegir un modelo sonoro cambia las opciones estructurales agrupadas, pero no reinicia los controles del panel ni las calibraciones independientes, como el tiempo de slide, el drive del filtro o la forma de onda.

### Fine tuning - Oscillator

- **Motor:** `Open303 (wavetables)` o `ATEK (modelled from schematic)`.
- **Pulse width (TM5):** 44%, 47%, 50%, 53% o 56%.
- **Square droop:** sin droop, 8 Hz, 15 Hz, 30 Hz o 60 Hz. Los valores altos dan más inclinación y movimiento a la onda cuadrada.
- **Saw: reset corner:** 1 us sharp, 3 us, 8 us o 20 us round. Cambia la dureza del reset de la sierra.
- **Saw droop:** 0.7 Hz, 3 Hz, 8 Hz o 15 Hz.

### Fine tuning - Filter

- **Motor:** `Open303 (TeeBee)` o `ATEK (diode ladder)`.
- **Linear resonance:** al activarlo, usa la respuesta lineal propia del circuito.
- **Drive:** 0 dB, +6 dB o +12 dB hacia el filtro ATEK. Más drive engrosa y puede comprimir la respuesta resonante.

### Fine tuning - Envelope and accent

- **Decay range:** Open303 (aprox. 460 ms-4.6 s) o Circuit (200 ms-2.5 s).
- **Accent accumulation:** permite que los acentos cercanos se acumulen y arrastren énfasis hacia las notas posteriores.
- **Accent stages:** solo Fast (100 ms), solo Slow (450 ms) o Both. Fast aporta pegada inmediata, Slow aporta arrastre y Both es la respuesta tipo circuito.
- **Log Env Mod taper:** activado proporciona la respuesta de audio predeterminada; desactivado hace lineal el mapeo del mando y el CV.

### Fine tuning - Pitch and output

- **Slide in semitones/s:** activado desliza en el espacio de pitch a una velocidad musical uniforme; desactivado usa la curva en el dominio de la frecuencia de Open303.
- **Slide tau:** 60 ms, 120 ms o 220 ms. Es una constante de tiempo, no el tiempo exacto para alcanzar el destino; los valores mayores producen un deslizamiento más lento.
- **Quantize pitch to semitones:** redondea el pitch entrante a semitonos cromáticos.
- **Limit to the 303 range (C1-C4):** limita las notas al rango original de tres octavas. Para un límite cromático estricto, úsalo junto con **Quantize pitch to semitones**; sin cuantización, el pitch bend fraccionario puede rebasar ligeramente los extremos.
- **Auto-legato:** si el gate sigue alto, un cambio de pitch inicia un slide sin requerir `SLIDE`. Está apagado por defecto porque un CV en movimiento o un cuantizador que se estabiliza podría crear slides no deseados.
- **OTA saturation:** None, Soft o Hard antes del VCA.
- **Output saturation:** None, Soft o Hard en la ruta final modelada.
- **Analogue drift:** None, Subtle, Marked o Heavy. Combina un carácter de afinación repetible según la nota con un movimiento lento de pitch y cutoff. El menú indica la cantidad aproximada de pitch y cutoff de cada nivel.
- **Unit:** 1 a 4. Selecciona el patrón repetible de error del DAC nota a nota; no se aleatoriza con cada nota.

## 8. Persistencia y reset

Los patches de VCV Rack guardan todos los controles del panel y las opciones del menú contextual, incluidos los motores, la calibración, el slide, la saturación, el nivel de deriva y Unit. Al recargar un patch, se restauran estos ajustes. Los estados instantáneos de nota, gate, envolvente y deriva no son datos de secuencia y se reinician. El reset del módulo en Rack libera la nota actual y limpia las envolventes de acento.

## 9. Ejemplos de patch

### Línea acid secuenciada clásica

1. Conecta ATEK303 SEQ a la izquierda y selecciona **Circuit (ATEK)**.
2. Empieza con onda cuadrada, cutoff en torno al 30%, resonancia al 60%, Env Mod al 50-70% y decay corto.
3. Genera hasta que los acentos y slides formen una frase útil.
4. Sube el drive del filtro o la saturación de salida para añadir mordiente.

### Voz con modulación externa

1. Conecta un teclado o secuenciador a `V/OCT` y `GATE`.
2. Envía un LFO bipolar lento a `CUT OFF CV` y ajusta su atenuversor cerca del 25%.
3. Envía velocity o un patrón de triggers a `ACC`.
4. Usa otra envolvente o un CV aleatorio en `DECAY CV` para variar la frase.

### Control híbrido por expander

1. Conecta ATEK303 SEQ y deja que proporcione gate, accent y slide.
2. Conecta otra fuente cuantizada al jack `V/OCT` de la voz.
3. El pitch externo tiene prioridad mientras la articulación generada sigue activa.

## 10. Consideraciones importantes

- ATEK303 es monofónico; los canales de una entrada polifónica no se convierten en voces separadas.
- No hay mando de nivel de salida. Usa un control de ganancia posterior y recuerda que el techo de seguridad de +/-12 V no es un nivel operativo objetivo.
- Los atenuversores de CV empiezan centrados, por lo que conectar una modulación puede parecer que no hace nada hasta mover el trimpot.
- Los slides necesitan una relación legato válida. Con gates cortos convencionales, `SLIDE` mantiene activa la voz durante el hueco; con gates permanentemente altos, usa Auto-legato o asegúrate de que la fuente entregue las transiciones deseadas.
- La cuantización, el límite C1-C4, drift y Unit alteran la interpretación del pitch. Desactívalos si necesitas un seguimiento continuo exacto o realizar mediciones de calibración.
- Circuit y Open303 son puntos de partida, no restricciones. Cualquier edición detallada puede convertir legítimamente el modelo en Custom.
