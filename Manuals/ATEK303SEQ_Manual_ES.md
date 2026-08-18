# ATEK303 SEQ - Manual de usuario

**Versión del manual:** 1.0
**Versión del plugin:** Animatek 2.5.5
**Módulo:** ATEK303 SEQ, generador acid determinista de 16 pasos y 20 HP para VCV Rack

## 1. Concepto

ATEK303 SEQ es un generador de patrones monofónico con reloj externo, no una fila convencional de 16 controles editables. Una semilla y un pequeño grupo de controles musicales crean líneas acid repetibles con notas, silencios, ties, acentos y slides. El resultado es determinista para un patrón guardado y su historial de mutaciones, mientras que `GENERATE` puede elegir deliberadamente una identidad nueva.

El patrón tiene dos capas coordinadas:

- La **capa de tiempo** contiene estados Rest, Note o Tie.
- La **capa de pitch** contiene los eventos de nota, su octava, acento y slide saliente, que consumen los estados Note.

Un Rest cierra la voz. Un Note crea un ataque nuevo y consume el siguiente evento de pitch. Un Tie prolonga el pitch y el gate anteriores sin otro ataque. Un slide conecta dos eventos Note adyacentes y distintos; no es lo mismo que un tie.

## 2. Inicio rápido

1. Coloca ATEK303 SEQ inmediatamente a la izquierda de ATEK303 o conecta sus salidas a otra voz monofónica.
2. Conecta un reloj externo a `CLOCK`.
3. Si usas cables, conecta `V/OCT`, `GATE`, `ACCENT` y `SLIDE` a sus destinos.
4. Pulsa `GENERATE` para crear un patrón con una semilla nueva.
5. Empieza con los valores iniciales: 16 pasos, 65% de notas, rango del 45%, raíz C, escala Acid, gate del 85%, acento del 60% y slide del 50%.
6. Pulsa `BLOCK` cuando quieras conservar la identidad y usa los tres botones de mutación para obtener variaciones controladas.

El primer flanco de reloj inicia el paso 1 de 16; el módulo no emite una nota por el simple hecho de añadirlo a un patch.

## 3. Controles principales

### STEPS

Define la longitud del loop entre 1 y 16 pasos; el valor inicial es 16. Cambia inmediatamente el bucle activo. El material generado subyacente conserva 16 pasos, por lo que puede volver a aparecer al aumentar la longitud.

### NOTES

Define la densidad de notas entre 5% y 100%; el valor inicial es 65%. Los valores altos generan más actividad Note/Tie y menos silencios. Es un control de generación: muévelo y usa `GENERATE` sin bloqueo para crear un patrón nuevo con esa densidad.

### RANGE

Define la extensión melódica y de octavas generada entre 0% y 100%; el valor inicial es 45%. Los valores bajos mantienen una línea compacta y los altos permiten movimientos de octava más amplios. Afecta al material que se genere posteriormente; no transpone el patrón actual.

### ROOT

Selecciona una de las 12 raíces cromáticas, de C a B; el valor inicial es C. Root transpone inmediatamente la reproducción sin regenerar el patrón.

### SCALE

Selecciona uno de ocho vocabularios de pitch cuantizado; el valor inicial es Acid:

- Acid pentatónica menor
- Menor natural
- Frigia
- Menor armónica
- Dórica
- Blues
- Mayor
- Cromática

La escala afecta a la interpretación durante la reproducción y a la mutación de pitch/octava. Por ello, cambiarla puede alterar de inmediato la línea audible, y la generación posterior usa la escala seleccionada.

### GATE

Define la longitud del gate normal entre 5% y 100% del período de reloj medido; el valor inicial es 85%. Se conserva un pequeño hueco de seguridad antes del siguiente flanco. Los ties mantienen el gate durante su continuación independientemente de la longitud normal. Los slides solo sostienen el gate a través del flanco si está activado **Gate held through slides (legato)**.

### ACCENT

Define la densidad de acentos generada entre 0% y 100%; el valor inicial es 60%. Afecta a futuras generaciones sin bloqueo. Los acentos existentes pueden cambiarse con la mutación de articulación.

### SLIDE

Define la densidad de slides generada entre 0% y 100%; el valor inicial es 50%. Los slides solo se conservan entre notas activas adyacentes de pitch distinto; los que no sean válidos o resulten redundantes se eliminan. Afecta a futuras generaciones sin bloqueo, mientras que la mutación de articulación puede alterar los slides existentes.

## 4. Controles de generación y mutación

### GENERATE

Con `BLOCK` apagado, crea una semilla aleatoria nueva y genera todas las capas usando los controles de generación actuales. Esto sustituye intencionadamente el patrón actual y borra el undo de mutación.

Con `BLOCK` encendido, conserva la identidad de la semilla y muta las tres familias en un solo gesto: tiempo, pitch/octava y slide/acento. Como es una mutación, puede deshacerse una vez desde el menú contextual.

La entrada `GEN` realiza la misma operación que el botón.

### BLOCK

Interruptor enclavado que bloquea la semilla. Apagado significa que `GENERATE` elige una semilla nueva. Encendido significa que `GENERATE` muta todas las capas. El botón iluminado y la opción contextual **Lock seed** representan el mismo ajuste.

### MUT TIME

Solicita dos operaciones de mutación deterministas a la capa temporal Rest/Note/Tie. Puede mover ataques o cambiar relaciones entre nota y tie, y mantiene un patrón válido. Si las operaciones se cancelan entre sí y dejan el patrón igual, el generador puede aplicar una operación adicional para producir un cambio visible.

### MUT NOTE/OCT

Solicita dos operaciones de la familia de pitch. En la versión actual, esta mutación cambia la colocación de octavas; el nombre del botón reserva el ámbito nota/octava, pero por ahora debe usarse como control de variación de octava. Puede añadirse una operación si las dos primeras dejan el patrón sin cambios.

### MUT SLD/ACC

Solicita tres operaciones de mutación deterministas a acentos y slides. Puede añadirse una operación si el resultado inicial coincide con el patrón de partida.

Cada mutación correcta guarda un nivel de undo. La siguiente mutación sustituye esa instantánea.

## 5. Entradas

### CLOCK

Entrada de reloj externo. Los flancos de subida avanzan la secuencia. No hay reloj interno. El período medido controla la duración del gate y, si está activo, el glide propio del secuenciador, de modo que los cambios de tempo mantienen proporciones musicales. La detección de reloj y triggers usa un comportamiento Schmitt alrededor de 0.1 V/1 V.

### RESET

Prepara el paso 1 y detiene el estado de transporte actual. El siguiente flanco de reloj inicia el paso 1. Reset no genera otro patrón, no cambia la semilla ni emite EOC por sí solo.

### GEN

Entrada de trigger para `GENERATE`. Con `BLOCK` apagado, crea una semilla y un patrón nuevos; con `BLOCK` encendido, muta las tres capas. Esto permite disparar cambios controlados desde otro módulo.

## 6. Salidas

### EOC

Pulso de fin de ciclo de 10 V y aproximadamente 1 ms. Se dispara en el primer flanco de reloj que inicia el paso 1 y siempre que la reproducción vuelve al paso 1. Ten en cuenta ese pulso inicial al contar ciclos completos o encadenar secuenciadores.

### V/OCT

Pitch monofónico de 1 V/oct. La octava base y la raíz se suman al pitch cuantizado del patrón. Durante los silencios, conserva el último pitch en vez de saltar a un valor sin utilidad.

Con **Own glide** activo y sin ATEK303 conectado, esta salida física aplica glide tras un slide. Si ATEK303 está inmediatamente a la derecha, el glide propio se desactiva: el expander recibe pitch sin glide y la salida física `V/OCT` también queda sin glide, lo que evita dos etapas de deslizamiento.

### GATE

10 V mientras la nota actual está activa. La duración sigue `GATE`, salvo que los ties mantienen la nota durante los pasos de continuación. La opción de legato de slide también puede sostener el gate al cruzar slides.

### ACCENT

En modo normal, entrega 10 V en las notas acentuadas y 0 V en las demás. En modo **Accent as velocity CV**, mantiene un nivel de acento configurable para las notas acentuadas y un nivel base configurable para las notas activas sin acento; los silencios entregan 0 V.

### SLIDE

10 V en una nota activa válida cuyo pitch debe deslizarse hacia la siguiente nota activa adyacente. Queda baja en silencios, ties, transiciones con el mismo pitch o transiciones que no pueden formar un slide válido.

## 7. LEDs de paso

Los 16 LEDs RGB muestran el patrón renderizado y destacan el paso actual:

- Apagado: Rest.
- Verde: ataque Note normal.
- Azul: Tie, continuación de la nota anterior sin otro ataque.
- Ámbar/amarillo: Note con slide saliente.
- Rojo: Note acentuado.
- Paso actual: más brillante y con un realce azul adicional.

Cuando coinciden varios atributos, la visualización usa una prioridad clara: tie, después acento, después slide y, finalmente, nota normal. El patrón de audio conserva su articulación interna válida.

## 8. Expander ATEK303

Coloca ATEK303 inmediatamente a la derecha de ATEK303 SEQ. El secuenciador envía internamente cuatro señales: `V/OCT` sin glide, gate, estado de acento y estado de slide. EOC no se envía por el expander.

ATEK303 resuelve la prioridad por jack. Cualquier cable conectado a `V/OCT`, `GATE`, `ACC` o `SLIDE` de la voz sustituye solo la señal equivalente del expander. Esto permite crear patches híbridos, por ejemplo, con el timing y la articulación del expander y un pitch externo.

Al estar conectados, se omite el glide del secuenciador y ATEK303 realiza el slide. La salida física `V/OCT` del secuenciador también queda sin glide en esta configuración, por lo que un destino conectado en paralelo no recibe inadvertidamente el glide que realiza la voz adjunta. Mantén **Gate held through slides (legato)** apagado, que es el valor inicial, para que ATEK303 reciba un flanco de gate nuevo; si activas legato en SEQ, activa también **Auto-legato** en ATEK303 para que los cambios de pitch con gate alto disparen el slide.

## 9. Menú contextual

Haz clic derecho sobre el módulo para acceder a:

- **Versión de patrón y seed:** identificación de solo lectura de la versión del generador y de la semilla hexadecimal actuales.
- **Lock seed:** mismo estado que `BLOCK`.
- **Mutate time (2 operations):** misma acción que `MUT TIME`.
- **Mutate pitches / octaves (2 operations):** misma familia que `MUT NOTE/OCT`; actualmente produce una mutación de octava.
- **Mutate accents / slides (3 operations):** misma acción que `MUT SLD/ACC`.
- **Undo last mutation:** restaura la instantánea anterior a la última mutación correcta. Se desactiva si no hay undo. No deshace una generación con semilla nueva.
- **Gate held through slides (legato):** mantiene el gate alto en transiciones de slide válidas. Apagado, usa un pequeño hueco de gate mientras `SLIDE` indica a una voz compatible que permanezca activa. Con ATEK303, deja esta opción apagada o activa también **Auto-legato** en la voz; de lo contrario, el gate sostenido no crea el nuevo flanco que ATEK303 necesita por defecto.
- **Own glide on the V/Oct output:** aplica un glide proporcional al tempo para otras voces. Se omite automáticamente al conectar ATEK303.
- **Base octave:** C1 (-3 V), C2 (-2 V), C3 (-1 V), C4 (0 V) o C5 (+1 V); el valor inicial es C2.
- **Accent as velocity CV:** cambia `ACCENT` de gate binario a niveles sostenidos tipo velocity.
- **Accent level:** 10 V, 8 V o 5 V; el valor inicial es 8 V. Se usa para las notas acentuadas en modo velocity.
- **Base level (unaccented note):** 0 V, 1 V, 2 V o 3 V; el valor inicial es 2 V. Se usa para las notas activas sin acento en modo velocity.

## 10. Persistencia y transporte

Los patches de VCV Rack guardan el patrón de dos capas, la semilla, el contador de mutación, los ajustes de generación asociados al patrón, todos los parámetros del panel, el estado BLOCK, el comportamiento de gate/slide, el ajuste de glide propio, la octava base y las opciones de CV de acento.

No se guardan la posición actual de transporte, si ya se ha recibido el primer clock, el período medido ni la instantánea de undo de un nivel. Tras cargar, el siguiente clock empieza en el paso 1. El patrón guardado permanece intacto, pero **Undo last mutation** no está disponible hasta realizar otra mutación válida.

## 11. Ejemplos de patch

### Sistema acid directo con ATEK303

1. Conecta ATEK303 a la derecha y envía un reloj de semicorcheas al secuenciador.
2. Usa 16 pasos, C, escala Acid, 60-75% de notas y las densidades de articulación iniciales.
3. Genera varias identidades y activa `BLOCK` cuando encuentres la mejor.
4. Alterna `MUT TIME` y `MUT SLD/ACC` durante la interpretación.
5. Usa EOC para disparar otro evento, recordando que también pulsa en el primer flanco.

### Control de una voz de otro fabricante

1. Conecta `V/OCT` y `GATE` a una voz monofónica.
2. Deja **Own glide** activado y conecta `SLIDE` solo si el destino tiene una entrada de slide específica.
3. Activa **Accent as velocity CV** y conecta `ACCENT` a velocity, nivel de VCA o cutoff.
4. Ajusta los niveles de acento y base al rango de CV del destino.

### Variaciones deterministas para un arreglo musical

1. Encuentra un patrón usando `GENERATE` sin bloqueo.
2. Activa `BLOCK` y guarda el patch.
3. Usa una familia de mutación cada vez y escucha la variación.
4. Usa **Undo last mutation** inmediatamente si el cambio no resulta útil.
5. Envía reset en los límites de frase para alinear el paso 1; reset no cambia la identidad guardada.

### Loop polirrítmico

1. Ajusta `STEPS` a 13 o 15 mientras el ritmo maestro sigue agrupado en 16.
2. Conecta `EOC` para disparar una modulación lenta o resetear otro secuenciador.
3. Recuerda que cambiar `STEPS` altera inmediatamente el momento del wrap y, por tanto, el timing de EOC.

## 12. Consideraciones importantes

- Siempre hace falta un reloj externo; `GENERATE` cambia los datos del patrón, pero no lo hace avanzar.
- `NOTES`, `RANGE`, `ACCENT` y `SLIDE` dan forma a futuras generaciones sin bloqueo. No reescriben continuamente el patrón actual. `STEPS`, `ROOT`, `SCALE` y `GATE` sí tienen efecto inmediato en la reproducción.
- La generación con una semilla nueva no es repetible hasta guardar el patch; las mutaciones con BLOCK conservan la identidad de la semilla y son deterministas.
- Ties y slides son distintos. Los ties prolongan la misma nota y gate; los slides se mueven entre dos notas atacadas de distinto pitch.
- El primer flanco de reloj emite EOC porque entra en el paso 1. Usa un gate delay o lógica de conteo posterior si solo deben contar los wraps completados.
- Undo tiene un nivel y no es persistente. Generar con una semilla nueva lo borra.
- Conectar ATEK303 desactiva el glide propio tanto para el expander como para la salida física de pitch. Es deliberado para que la voz realice exactamente un slide.
