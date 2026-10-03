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

### EDIT

Interruptor enclavado en la esquina superior izquierda. Cambia los controles de generación
por el editor de patrón, en el mismo hueco del panel. Ver la sección 14.

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
- Azul del logo: ataque Note normal.
- Azul profundo: Tie, continuación de la nota anterior sin otro ataque.
- Cian: Note con slide saliente.
- Casi blanco: Note acentuado.
- Paso actual: blanco a plena luz, tape lo que tape: en una fila de azules es lo único que se encuentra sin buscarlo.

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
- **Clear pattern:** vacía el patrón para dibujar uno desde cero en el editor. Conserva la semilla y no toca `STEPS`; el paso 1 se queda con una nota, que es la invariante de la que vive el secuenciador. Es destructivo, así que ocupa el hueco de undo: **Undo last mutation** lo recupera.
- **Gate held through slides (legato):** mantiene el gate alto en transiciones de slide válidas. Apagado, usa un pequeño hueco de gate mientras `SLIDE` indica a una voz compatible que permanezca activa. Con ATEK303, deja esta opción apagada o activa también **Auto-legato** en la voz; de lo contrario, el gate sostenido no crea el nuevo flanco que ATEK303 necesita por defecto.
- **Own glide on the V/Oct output:** aplica un glide proporcional al tempo para otras voces. Se omite automáticamente al conectar ATEK303.
- **Load MIDI file...:** lee un Standard MIDI File y sustituye con él el patrón actual. Ver la sección 13.
- **Bar:** aparece cuando el fichero cargado pasa de 16 pasos. Salta entre los compases del fichero sin volver a abrirlo.
- **Forget file:** olvida la referencia al fichero. El patrón ya cargado sigue sonando.
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

## 13. Importar ficheros MIDI

**Load MIDI file...** en el menú contextual lee cualquier Standard MIDI File (formato 0, 1
o 2) y lo convierte en un patrón. Está pensado para los packs de patrones acid que se
distribuyen en MIDI, pero acepta cualquier fichero cuyas notas caigan en una rejilla de
semicorcheas.

### En qué se convierte cada cosa

| En el fichero | En el secuenciador |
|---|---|
| Note-on en un paso | Ataque de nota |
| Nota que cruza el límite del paso sin que empiece otra | Tie: la nota se prolonga, no se vuelve a atacar |
| Nota que cruza el límite hacia otro ataque de distinta altura | Slide |
| Velocidad por encima del punto medio del fichero | Acento |
| Nada sonando | Silencio |

El tiempo se cuantiza a semicorcheas, la rejilla del propio secuenciador. Un fichero de
cuatro compases da cuatro patrones de 16 pasos, y el submenú **Bar** se mueve entre ellos.

### Qué informa el módulo

El menú muestra una línea de estado bajo el nombre del fichero. Dice si el fichero necesitó
la escala Cromática, si se transportó y qué hubo que descartar.

- **Scale set to Chromatic:** el fichero traía alturas que la escala del panel no puede
  representar, así que el módulo pasa a Cromática, que representa cualquiera. Así la
  importación no pierde nada: la melodía no se reescribe. `ROOT` no se toca y sigue
  transportando el resultado. Si el fichero sí cabe en la escala puesta, la escala se
  respeta.
- **No velocity accents:** todas las notas traen la misma velocidad, así que no hay
  información de acento que leer y el patrón entra sin acentos.
- **Transposed +/-N oct:** el fichero se salía del rango representable y se movió por
  octavas enteras. Todos los intervalos se conservan; lo único que cambia es el registro.
- **Extra voices dropped:** el secuenciador es monofónico. En un acorde se queda la nota más
  grave, que es lo que convierte un arreglo en una línea de bajo.
- **Skipped N leading empty steps:** el fichero no empezaba en el tick 0. Una pieza que entra
  en el segundo compás se importaría con la primera página en blanco, y eso dice dónde
  empieza la música, no qué contiene, así que el silencio se descarta. Se descarta en
  compases enteros de dieciséis pasos: recortar hasta la primera nota movería una anacrusa al
  tiempo fuerte y dejaría el compás entero a contratiempo, así que un fichero que arranca
  unos pasos después del principio de su primer compás conserva esa anacrusa.

### Límites

- La división SMPTE (ficheros hechos contra vídeo) se rechaza: no hay rejilla musical que
  cuantizar.
- El tempo, los controladores, los cambios de programa y todo lo demás se ignoran. Solo
  cuentan las notas.
- La semilla de un patrón importado ya no identifica un patrón generado. Es un hash del
  contenido importado, estable entre recargas, y se muestra para poder distinguir dos
  importaciones.
- `GENERATE` sobrescribe un patrón importado, igual que cualquier otro. Con `BLOCK` y los
  botones de mutación se desarrolla una importación sin perderla.

El patch guarda el patrón importado entero, así que suena aunque después se mueva o se
borre el fichero MIDI. La ruta se guarda también, solo para que el submenú **Bar** siga
funcionando al reabrir el patch.

## 14. Editor de patrón

El botón `EDIT` de la esquina superior izquierda cambia los controles de generación por un
editor de patrón, en el mismo hueco del panel. Las dos vistas comparten esa zona, así que
no se mueve nada más: los LEDs de paso, las entradas y las salidas se quedan donde están.
El botón es un enclavamiento y se ilumina mientras el editor está a la vista.

### Una octava, y a propósito

El piano roll enseña **una octava entera**: los doce semitonos, siete teclas blancas y
cinco negras. Las filas que la escala activa no contiene salen hundidas y no aceptan clic:
están ahí para que la octava se lea como una octava y para ver dónde cae cada nota, no para
tocarlas. Clicar una no hace nada, en vez de cambiar el módulo a Cromática por su cuenta, que
reinterpretaría el patrón entero. El desplazamiento por octavas vive en las filas `UP` y
`DOWN` de debajo.

Es como funciona un 303 de verdad —teclado de una octava y dos botones que suben o bajan
notas sueltas— y es también como el módulo guarda el patrón por dentro: grado dentro de la
octava más una octava por paso. Así que el roll es una vista directa del dato, sin
conversión por medio. Y es lo que hace que quepa: un roll cromático de dos octavas dejaría
las filas en un milímetro, imposibles de acertar.

### Las filas

Por el borde izquierdo baja un teclado, una octava de verdad. Las teclas fuera de la escala
salen apagadas, a juego con su carril hundido. La tónica lleva una marca azul y su octava
real, de modo que un `C` del roll te dice si es un C2 o un C3 en vez de dejarte adivinando.

| Fila | Qué hace un clic |
|---|---|
| Piano roll | Pone ese paso en una nota de ese grado. Clicar la nota donde ya está la quita. Arrastrando se pinta a lo largo de varios pasos. |
| `UP` | Sube la nota una octava: 0 a +1 a +2 y vuelta a 0. |
| `DOWN` | La baja igual. Clicar en la fila contraria salta directo a ese lado. |
| `GATE` | Recorre silencio, nota, tie. |
| `ACC` | Conmuta el acento. |
| `SLIDE` | Conmuta el slide saliente. |

Se pinta arrastrando: manteniendo pulsado y barriendo a derecha o izquierda el gesto se
sigue aplicando paso a paso, así que una tirada de notas sale de un arrastre y no de un clic
por nota. En el roll el arrastre sigue al ratón también en vertical: subir o bajar sin
cambiar de paso mueve la nota a esa altura. El **botón derecho borra** —en el roll y en `GATE` apaga el paso, en `UP` y `DOWN`
devuelve la octava a cero, y en `ACC` y `SLIDE` apaga el atributo—, y también funciona
arrastrando, que es como se limpia un pasaje de un barrido. El menú contextual del módulo
sigue saliendo con el botón derecho sobre el tecladito de la izquierda o sobre los huecos
entre filas.

Los colores son los de los LEDs de paso, así que panel y editor dicen siempre lo mismo:
azul del logo ataque normal, casi blanco acento, cian slide, azul profundo tie, y pizarra
las filas de octava. El slide se dibuja además como una
línea que une las dos notas, que es donde el gesto melódico se ve. La cabeza de
reproducción cruza todas las filas.

Los pasos que quedan más allá de `STEPS` salen apagados. Siguen ahí y conservan su
contenido, pero están fuera del bucle y no se pueden editar hasta que `STEPS` los alcance.

### Lo que el editor no te deja hacer

- **Las celdas que no admiten valor salen oscuras y no aceptan clic.** `UP`, `DOWN`, `ACC` y
  `SLIDE` solo significan algo sobre un ataque, y el slide necesita además que el paso
  *siguiente* ataque: un slide une dos notas adyacentes, así que un silencio o un tie detrás
  no le dejan a dónde ir. Las dos notas pueden ser de la misma altura: no hay glissando que
  hacer, pero un 303 toca además el slide *legato*, y esa mitad se sigue oyendo como una
  nota sostenida en vez de dos ataques. Es el comportamiento real de un 303, y dibujarlo
  permite ver qué celdas están vivas en vez de pulsar y no entender por qué no pasa nada.
  Ojo: el slide pertenece a la nota de la que *sale*, no a la que llega.
- **Un slide que deja de ser posible se cae solo.** Si después pones un silencio detrás de
  una nota con slide, el slide se apaga, porque el secuenciador lo habría descartado
  igualmente al reproducir. La fila enseña la verdad, no la petición.
- **El patrón conserva siempre al menos una nota.** Borrar la última devuelve una nota al
  paso 1.
- **Editar no crea una identidad nueva.** La semilla se conserva: sigue siendo ese patrón,
  retocado. `GENERATE` con `BLOCK` apagado lo sustituye como a cualquier otro, así que
  bloquea la semilla antes si quieres conservar lo editado a mano.
- Las notas cuya altura se sale de la octava —solo posible en un patrón importado de MIDI—
  se recolocan dentro de ella, con el desplazamiento movido a `UP` / `DOWN`, la primera vez
  que tocas ese paso. La nota que suena no cambia mientras quepa en dos octavas.

### Las cuatro páginas

Sesenta y cuatro pasos no caben en dieciséis columnas, así que el patrón se edita de compás
en compás. La tira que hay bajo el editor es eso: un botón redondo y cuatro LEDs bajo las
cuatro últimas columnas de la rejilla.

- **Un clic en un LED** enseña esa página en el editor. El sonido no cambia.
- **Doble clic en un LED** hace que la secuencia arranque ahí. Con `STEPS` en 16 y la
  página `2:4` activa, la secuencia recorre los pasos 17 a 32. Si `STEPS` se pasa del paso
  64, la ventana da la vuelta al patrón.
- **Un toque en el botón** avanza una página. **Mantenerlo** dos segundos engancha el
  seguimiento de la cabeza y el editor cambia de página solo; la barra que llena el botón
  son esos dos segundos, y queda encendido en azul mientras sigue. Otra pulsación larga lo
  suelta.

Los LEDs se leen como una caja de ritmos: blanco a plena luz por donde va el secuenciador,
con un punto más de brillo en el tiempo fuerte; el azul del logotipo en la página que estás
editando; y un blanco tenue en la página por la que arranca la secuencia, para saber dónde
empieza con el reloj parado. Una página que cae fuera de `STEPS` enseña el azul más bajo:
sigue diciendo dónde estás, y además que eso no llega a sonar.

### El sticker acid genera

La cara acid de la esquina superior derecha es un botón. Al clicarla hace exactamente lo
mismo que `GENERATE` —semilla nueva, o mutación completa si `BLOCK` está encendido—, así que
puedes seguir sorteando patrones sin salir del editor a buscar el panel de generación. Lleva
tooltip, porque un sticker no parece un control.
