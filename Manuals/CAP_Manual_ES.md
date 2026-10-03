# Manual de usuario de CAP

**Versión del manual:** 1.1

**Versión del plugin:** Animatek 2.5.9

**Módulo:** CAP para VCV Rack

**Ancho:** 6 HP

---

## 1. Descripción general

**CAP** es un VCA de ducking disparado por trigger. Envíale el trigger usado por un bombo u otro evento y CAP bajará rápidamente el audio conectado, lo mantendrá abajo durante un instante y lo restaurará siguiendo la curva de recuperación elegida. No necesita detector ni VCA externo.

Cada duck tiene una **caída fija de 2 ms** y un **hold fijo de 12 ms**. `RECOVERY` controla el retorno entre **40 ms y 1 s**. `DEPTH` determina cuánto baja la ganancia, `JITTER` añade variación correlacionada entre golpes y `LEVEL` fija el techo del VCA.

CAP admite audio estéreo y polifónico. Por defecto, todo el audio comparte una envolvente para mantener estable la imagen estéreo. Las salidas `ENV` y `EOC` también permiten usar CAP como envolvente de modulación o generador de funciones autocíclico sin conectar audio.

---

## 2. Inicio rápido

1. Conecta una fuente estéreo a `IN L` e `IN R`, o una fuente mono solamente a `IN L`.
2. Conecta `OUT L` y `OUT R` a la siguiente etapa de la mezcla.
3. Envía a `TRIG` el trigger del bombo, gate o ritmo que actuará como evento de sidechain.
4. Empieza con `RECOVERY` en **250 ms**, `DEPTH` al **80%**, `JITTER` al **25%** y `LEVEL` al **100%**.
5. Acorta `RECOVERY` para abrir huecos rítmicos ajustados o alárgalo para obtener bombeo audible.
6. Reduce `DEPTH` para un movimiento sutil. Aumenta `JITTER` cuando quieras que los golpes repetidos respiren en vez de ser idénticos.
7. Conecta `VCA` si quieres controlar la ganancia por tensión: el CV multiplica lo que fija `LEVEL`, así que CAP funciona como una VCA normal aunque no le llegue ningún trigger.

Para escuchar el duck sin conectar una fuente de triggers, haz clic derecho sobre CAP y elige **Fire a trigger**.

---

## 3. Ciclo de la envolvente

Un trigger inicia o redispara esta secuencia:

1. **Caída:** la ganancia pasa desde su valor actual hasta el suelo en 2 ms.
2. **Hold:** la ganancia permanece en el suelo durante 12 ms.
3. **Recuperación:** la ganancia vuelve al reposo durante el tiempo y con la curva actuales.
4. **Reposo:** la ganancia está al 100% del techo fijado por `LEVEL`; `ENV` está normalmente a 10 V.

Un redisparo durante el ciclo inicia una nueva caída desde el nivel actual y nunca provoca un salto hacia arriba. Si interrumpe una recuperación, esa recuperación no produce `EOC`.

En cada golpe, CAP muestrea `DEPTH` más `D-CV` y elige la profundidad, recuperación y curva con jitter de ese golpe. Esos valores permanecen fijos durante el golpe en vez de cambiar continuamente durante la envolvente.

---

## 4. Controles

### RECOVERY

Fija el tiempo nominal de recuperación entre **40 ms y 1 s**, con una escala exponencial que facilita el ajuste fino de tiempos cortos. Valor por defecto: **250 ms**. El jitter puede hacer que cada recuperación sea más corta o más larga que este valor nominal.

### DEPTH

Fija la atenuación en el fondo del duck entre **0% y 100%**. Valor por defecto: **80%**.

- Al 0%, los triggers no bajan la envolvente.
- Al 100%, el VCA alcanza ganancia cero en el suelo.
- `D-CV` se suma antes de limitar el resultado al rango final.

### JITTER

Fija la variación correlacionada entre **0% y 100%**. Valor por defecto: **25%**. CAP usa un paseo aleatorio, de modo que cada golpe está relacionado con el anterior en vez de recibir una variación de ruido blanco independiente. Varía el tiempo de recuperación, la profundidad y la forma de la curva.

Al 0%, todos los golpes usan los valores nominales. Los valores altos generan un movimiento más orgánico. El estéreo permanece coherente en el modo de envolvente compartida predeterminado.

### LEVEL

Fija la ganancia máxima del VCA entre **0% y 100%**. Valor por defecto: **100%**. Escala el audio tanto en reposo como durante el duck. Por defecto no escala `ENV`; la opción contextual **Level attenuates ENV** cambia este comportamiento.

### PAN

Coloca esta CAP en la mezcla estéreo de una cadena, de todo a la izquierda a todo a la derecha, en el extremo izquierdo de la fila `TRIG`; una línea lo une a su jack de CV. Es un control de balance, a ganancia unidad en el centro, y solo cuenta cuando hay varias CAP juntas con un **BUS** a su derecha: las salidas propias de esta CAP no cambian nunca. Cómo funciona la cadena está en el manual de BUS.

El trimmer ocupa el sitio del botón de trigger manual, que ha pasado al menú contextual como **Fire a trigger**. No ha desaparecido ningún jack, así que los patches anteriores conservan todos sus cables.

---

## 5. Entradas

### TRIG

Entrada de trigger o gate, en el extremo derecho de la fila `TRIG` y bajo su etiqueta. Admite señales polifónicas y usa umbrales Schmitt: la señal pasa a estado alto al alcanzar **1 V** y debe volver por debajo de **0,1 V** antes de que otro flanco ascendente pueda disparar. Por tanto, un gate sostenido dispara una sola vez.

El número de canales de `TRIG` determina la polifonía de la envolvente y de las salidas auxiliares, con un mínimo de un canal cuando no hay cable.

### D-CV

CV polifónico de profundidad. El jack no lleva etiqueta: una línea del panel sube desde él hasta el mando `DEPTH`, que es al que modula. **10 V suman un 100% de profundidad** y un voltaje negativo la reduce. El resultado de `DEPTH + D-CV / 10 V` se limita a 0-100% y después se aplica la variación con jitter del golpe. El CV se muestrea cuando se dispara cada canal.

### VCA

CV polifónico de ganancia. Como `D-CV`, no lleva etiqueta: la línea del panel sube desde el jack hasta el fader `LEVEL`, que es lo que este CV escala. Es unipolar y lineal: **0 V cierra la VCA y 10 V deja pasar el tope entero**, y los voltajes negativos se tratan como 0 V. Multiplica lo que fija `LEVEL` en vez de sustituirlo, así que `LEVEL` sigue siendo el techo y el CV recorta desde ahí. Sin cable no atenúa nada, de modo que un patch anterior suena exactamente igual.

Esta entrada es lo que convierte a CAP en una VCA controlada por tensión de las de siempre: conéctale una envolvente y tienes una VCA normal, con el ducking por trigger disponible encima si lo necesitas. No afecta a `ENV` ni a `EOC`: la envolvente es lo que el módulo genera, no lo que amplifica. El medidor sí la tiene en cuenta, porque muestra la ganancia realmente aplicada.

### PAN CV

CV polifónico para `PAN`, sumado al trimmer: **±5 V recorren todo el rango** y la suma se limita a los extremos. Cada canal de audio toma su propio canal de CV. Solo afecta a la mezcla de la cadena.

### IN L

Entrada de audio izquierda o mono. Admite audio polifónico.

### IN R

Entrada de audio derecha. Admite audio polifónico. Cuando no está conectada, recibe internamente la señal de `IN L`, de modo que un único cable mono alimenta ambas rutas de audio.

---

## 6. Salidas

### ENV

Entrega la envolvente de ganancia del duck como CV: **10 V en reposo**, baja según la profundidad y después recupera hasta 10 V. Tiene el mismo número de canales que `TRIG`, con un mínimo de uno. Si activas **Level attenuates ENV**, `LEVEL` también escala esta salida, incluido su voltaje de reposo.

### EOC

Entrega un trigger de fin de ciclo de **10 V y 1 ms** en cada canal de envolvente. Solo se dispara cuando la recuperación llega a su final natural. Un redisparo antes de terminar cancela el evento EOC de ese ciclo. `LEVEL` nunca atenúa EOC.

### OUT L / OUT R

Salidas de audio con ducking. Su polifonía sigue a las entradas de audio conectadas. Si no hay ninguna entrada de audio conectada, ambas salidas tienen cero canales. Durante el funcionamiento normal, `OUT R` recibe la señal normalizada desde `IN L` cuando `IN R` no está conectado.

---

## 7. Medidor de ganancia

La barra iluminada tras el deslizador `LEVEL` muestra la ganancia aplicada, no la amplitud del audio. Por eso permite ver el duck incluso si la entrada está en silencio.

- La marca blanca del deslizador indica el techo de ganancia elegido.
- La altura de la barra incluye la envolvente y `LEVEL`.
- El brillo de la barra sigue a la envolvente.
- Un patch estéreo o polifónico puede mostrar varias barras estrechas.
- En modo compartido, las barras repetidas representan la misma ganancia coherente; en modo por canal muestran las envolventes individuales.
- La barra es azul en modo **VCA** y ámbar en los dos modos con filtro, donde su altura se lee como lo abierto que está el filtro.

---

## 8. Menú contextual

Haz clic derecho sobre CAP para acceder a estos ajustes.

### Mode

Sobre qué actúa la señal de control (la envolvente multiplicada por el CV de `VCA`). `LEVEL` sigue siendo el fader de salida en todos los modos y nunca mueve el filtro.

- **VCA** (por defecto): el control fija la ganancia. Es CAP como siempre ha sido; los patches anteriores cargan en este modo y suenan exactamente igual.
- **Lowpass filter:** el control fija el corte de un paso bajo de dos polos, de 20 Hz con el control cerrado a 20 kHz del todo abierto; el nivel no se toca. El duck se convierte en un bombeo filtrado: el golpe oscurece el sonido en vez de bajarlo. El filtro sigue a la envolvente sin retraso, así que la caída de 2 ms conserva su pegada. Ten en cuenta que un duck profundo lleva el corte por debajo de la fundamental de casi cualquier fuente, así que `DEPTH` decide lo oscuro que queda el golpe.
- **Low-pass gate:** corte y nivel cierran juntos, a través de un modelo de vactrol que abre en unos 2 ms y cierra en 30 ms o más, más despacio cuanto más cerrado está. Ese retraso es el sonido de un low-pass gate: la cola de la nota sigue perdiendo agudos después de que el nivel haya caído. También suaviza la caída de un duck a unos 50 ms.

El medidor se vuelve ámbar en los dos modos con filtro, así que el modo se ve en el panel.

### Ping envelope (trigger opens)

Invierte la envolvente. En reposo el control está cerrado; un trigger lo abre hasta `DEPTH` en 2 ms, lo mantiene 12 ms y `RECOVERY` lo vuelve a cerrar. En modo **Low-pass gate** es lo que convierte CAP en un LPG clásico tocado con triggers: conecta una voz a `IN L`, un gate de secuenciador a `TRIG`, y cada paso suena pulsado.

`ENV` y el medidor siguen a la envolvente invertida, así que `ENV` reposa a 0 V y sube con cada golpe. Para un pulsado percusivo elige la curva **Logarithmic**, que cae deprisa y luego se alarga; **Exponential** se queda abierta y cierra tarde.

### Recovery curve

- **Exponential** (por defecto): permanece abajo más tiempo y vuelve más deprisa cerca del final; resulta útil para un bombeo marcado.
- **Linear:** sube a velocidad constante.
- **Logarithmic:** sube deprisa al principio y se asienta más lentamente.

El jitter puede variar sutilmente la curva seleccionada en cada golpe.

### Freeze jitter

Detiene el avance de los paseos aleatorios correlacionados. Los golpes siguientes reutilizan el estado actual de variación, por lo que su tiempo, profundidad y curvatura se repiten mientras la opción siga activa. El mando `JITTER` aún determina con qué intensidad desplaza ese estado congelado a los valores nominales.

### Per-channel envelopes

Proporciona a los canales polifónicos estados de envolvente y flujos de jitter separados. Úsalo cuando un cable polifónico transporte pistas independientes que deban duckear por separado. Déjalo desactivado para material estéreo: la envolvente compartida predeterminada evita que la imagen se mueva a izquierda y derecha.

Si el audio tiene más canales que `TRIG`, los canales de audio adicionales usan el último canal de envolvente disponible.

### Level attenuates ENV

Hace que `LEVEL` escale `ENV` además del audio. Está desactivado por defecto, por lo que `ENV` permanece a 10 V en reposo independientemente del techo del VCA.

### LEDs de la cadena

Dos LEDs diminutos en las esquinas de arriba muestran la cadena. El izquierdo se enciende cuando hay una CAP o un BUS pegado al borde izquierdo y la cadena llega a esta CAP; el derecho, cuando esta CAP pasa la cadena a una CAP o un BUS a su derecha. Tenue quiere decir enlazado; más brillante, que pasa audio, según su nivel. ### Número de canal

En una fila, cada CAP muestra su número de canal arriba a la derecha de su nombre, contado desde la izquierda: la primera CAP de una fila es la 1. Un BUS que cierra su fila empieza la cuenta de nuevo para la siguiente; uno que pasa su mezcla deja que siga. Una CAP suelta no muestra número.

### Last in a row: OUT is the chain mix

Varias CAP juntas forman un mezclador. Si la fila termina en un **BUS**, la mezcla sale por el BUS. Si termina en una CAP, son `OUT L` / `OUT R` de esa última CAP los que llevan la mezcla: su propia señal más todo lo que llega por la izquierda, cada CAP con su panorama y después de su fader, como un par estéreo. Las CAP de en medio mantienen `OUT` como salida directa.

Activado por defecto en las CAP nuevas. En patches guardados antes de que existiera esta opción se carga desactivado, así que las CAP que ya estaban juntas suenan exactamente como antes; actívalo en este menú para mezclarlas.

### Fire a trigger

Inicia un duck en todos los canales de envolvente a la vez, como lo haría un trigger en `TRIG`. Es lo que hacía el botón del panel: escuchar el duck o arrancar un patch autocíclico.

### Reset jitter seed

Crea una semilla aleatoria nueva y reinicia los paseos aleatorios de todos los canales. Úsalo para obtener otra familia de variaciones correlacionadas. No dispara una envolvente ni cambia las posiciones de los controles del panel.

---

## 9. Persistencia y reset

VCV Rack guarda en el patch los valores de los controles y el siguiente estado del menú de CAP:

- modo,
- Ping envelope,
- curva de recuperación,
- Freeze jitter,
- Per-channel envelopes,
- Level attenuates ENV,
- semilla del jitter.

Al guardar y volver a abrir un patch se restauran la semilla y los ajustes elegidos. Las fases de ejecución de las envolventes no se guardan; al reabrir, el módulo empieza con las envolventes en reposo en vez de continuar un ciclo interrumpido.

Al resetear el módulo se restauran el modo **VCA** y la curva exponencial, se desactivan las cuatro opciones, vuelve la semilla de fábrica y las envolventes regresan al reposo. El reset normal de parámetros de Rack restaura los valores predeterminados del panel.

---

## 10. Ejemplos de patch

### Ducking clásico con bombo

Envía el trigger del bombo tanto a la voz de bombo como a `TRIG` de CAP. Pasa un bajo, pad o bus musical completo por CAP. Empieza cerca del 80% de profundidad y 250 ms de recuperación y después ajusta la recuperación al groove.

### Ducking de un bus estéreo

Conecta ambas entradas estéreo y deja **Per-channel envelopes** desactivado. Ambos lados reciben exactamente el mismo movimiento de ganancia, conservando la imagen estéreo mientras el medidor muestra las rutas activas.

### Ducking polifónico independiente

Conecta triggers y audio polifónicos correspondientes y activa **Per-channel envelopes**. Cada canal de trigger obtiene su propio ciclo e historial de jitter correlacionado. Conecta `D-CV` polifónico para dar una profundidad distinta a cada voz.

### Envolvente de modulación externa

No conectes audio, envía triggers a `TRIG` y conecta `ENV` a un filtro, wavefolder, envío de reverb u otro VCA. La señal reposa a 10 V y baja con cada trigger, por lo que resulta apropiada para modulaciones invertidas o de ducking.

### Voz con low-pass gate

Pon **Mode** en **Low-pass gate** y activa **Ping envelope**. Conecta un oscilador a `IN L` y un gate o trigger de secuenciador a `TRIG`. `DEPTH` fija cuánto abre cada golpe, `RECOVERY` cuánto suena la nota, y la curva **Logarithmic** da el pulsado clásico. Sube `JITTER` y no habrá dos notas que suenen igual.

### Bombeo filtrado

Pon **Mode** en **Lowpass filter** y deja la envolvente en ducking. Pasa un pad o un bajo por CAP con el trigger del bombo en `TRIG`: cada bombo oscurece el sonido y se vuelve a abrir durante `RECOVERY`, sin perder nivel. Baja `DEPTH` si el golpe queda demasiado oscuro.

### Generador de funciones autocíclico

Conecta `EOC` de vuelta a `TRIG`. Tras **Fire a trigger** en el menú contextual, cada recuperación completada inicia el siguiente ciclo. El periodo es aproximadamente la suma de la caída de 2 ms, el hold de 12 ms y la recuperación elegida; el jitter hace respirar los ciclos sucesivos. Desconecta el cable de realimentación o interrumpe la ruta de trigger para detenerlo.

---

## 11. Limitaciones y notas prácticas

- CAP es un VCA disparado por trigger, no un compresor: no escucha el nivel de audio y no tiene threshold, ratio ni makeup gain.
- `LEVEL` solo funciona como techo/atenuador. CAP no amplifica por encima de la ganancia unidad.
- Los redisparos rápidos pueden impedir que termine la recuperación, por lo que `EOC` puede permanecer inactivo. Es intencionado.
- A profundidad máxima, la caída de 2 ms reduce los clics, pero el material extremadamente discontinuo o de muy baja frecuencia todavía puede revelar cambios rápidos de ganancia.
- El jitter es correlacionado y está limitado; no garantiza un porcentaje concreto en cada golpe individual.
- El bypass de Rack conecta `IN L` con `OUT L` e `IN R` con `OUT R`, pero no reproduce la normalización interna de la entrada derecha de CAP. Si solo conectas `IN L`, no dependas de `OUT R` mientras CAP esté en bypass; conecta ambas entradas o divide la fuente externamente si necesitas la ruta derecha durante el bypass.
