# UNIT-D — Manual ampliado

**Versión del manual:** borrador ampliado 0.2  
**Módulo:** UNIT-D para VCV Rack  
**Concepto:** secuenciador generativo compacto inspirado en grafos de distancia unidad

---

# Español

## 1. Qué es UNIT-D

**UNIT-D** es un secuenciador generativo para VCV Rack que convierte una estructura geométrica en música.

En vez de usar una lista lineal de pasos como un secuenciador clásico, UNIT-D crea una nube de puntos en 2D. Después conecta algunos de esos puntos siguiendo una regla de distancia. Cada punto se convierte en un posible estado musical y cada conexión se convierte en una transición posible.

La idea importante es esta:

- Los **puntos** son estados musicales.
- Las **líneas** son caminos posibles entre estados.
- El **clock** mueve el recorrido por el grafo.
- La **posición X** del punto genera pitch y modulación.
- La **posición Y** genera otra modulación independiente.
- La **cantidad de conexiones** de un punto genera acento y sensación de densidad.

El resultado no es azar puro. Es un sistema generativo **determinista**: con los mismos parámetros, el comportamiento se repite igual. Esto permite buscar patrones vivos, pero controlables.

UNIT-D está pensado para crear melodías, líneas modulares, patrones semi-repetitivos, contrapunto generativo y material polifónico sin perder una estructura musical clara.

---

## 2. Resumen rápido

Conecta UNIT-D de esta forma para empezar:

1. Envía un clock a `CLK`.
2. Conecta `V/O` al `V/OCT` de un oscilador.
3. Conecta `GATE` a una envolvente.
4. Usa la envolvente para abrir un VCA.
5. Conecta `ACC` a la amplitud, al filtro o a cualquier entrada de acento.
6. Conecta `X` e `Y` a parámetros de timbre: cutoff, wavefolder, FM, posición de wavetable, decay, etc.
7. Ajusta `NODES`, `RADIUS`, `DENS` y `WALK` hasta encontrar una frase interesante.
8. Usa `LOCK` para congelar parcialmente la frase cuando aparezca algo bueno.

Punto de partida recomendado:

```text
NODES: 24-32
RADIUS: cerca de 1.0
DENS: medio
TOL: medio/bajo
WALK: 0 o 1
RNG: 2-3 octavas
GLEN: medio
GDEN: 60-80%
LOCK: centro
```

---

## 3. Concepto musical

UNIT-D trabaja con un grafo. Un grafo es una red compuesta por puntos y conexiones.

En UNIT-D:

- Cada punto del grafo se llama **nodo**.
- Cada línea entre dos nodos se llama **conexión** o **arista**.
- El recorrido que se mueve de nodo en nodo se llama **walker**.
- El número de conexiones de un nodo se llama **grado**.

El módulo crea una nube de nodos de forma determinista a partir de `SEED`. Después conecta los nodos cuya distancia está cerca de un valor objetivo definido por `RADIUS`, ajustado por `TOL` y `DENS`.

En cada pulso de clock, el walker intenta avanzar desde el nodo actual hacia uno de sus vecinos. El nodo seleccionado produce:

- una nota por `V/O`,
- un gate por `GATE`,
- un acento por `ACC`,
- dos voltajes de modulación por `X` e `Y`.

Esto hace que la melodía, los acentos y la modulación estén relacionados entre sí. No son capas independientes: todas salen de la misma estructura geométrica.

---

## 4. Filosofía de uso

UNIT-D no pretende comportarse como un generador aleatorio sin control. La gracia está en encontrar zonas donde el grafo produce frases con una mezcla de repetición, movimiento y sorpresa.

Piensa en el módulo como un instrumento con tres niveles:

1. **Forma:** `SEED`, `NODES`, `RADIUS`, `TOL` y `DENS` definen la geometría.
2. **Recorrido:** `WALK`, `LOCK`, `GDEN` y `GLEN` definen cómo se interpreta esa geometría en el tiempo.
3. **Lectura musical:** `RNG`, `V/O`, `ACC`, `X` e `Y` definen cómo se convierte el recorrido en sonido.

La mejor forma de usarlo no es mover todos los controles a la vez. Primero busca una geometría interesante. Después ajusta el recorrido. Por último, decide cómo usar las salidas dentro del patch.

---

## 5. Entradas

### CLK

Entrada de reloj.

Cada flanco de subida avanza el walker al siguiente nodo. El tempo y el ritmo principal dependen de la señal que conectes aquí.

Usos típicos:

- Clock regular desde un módulo de reloj.
- Triggers rítmicos para hacer patrones irregulares.
- Divisiones de clock para secuencias más lentas.
- Clock con swing para añadir sensación humana.

Si no hay clock, la secuencia no avanza.

### RST

Entrada de reinicio.

Cuando recibe un pulso, UNIT-D vuelve al estado inicial del recorrido. También reinicia el comportamiento determinista interno, de forma que el patrón puede volver al mismo punto musical.

Usos típicos:

- Reiniciar cada 1, 2, 4, 8 o 16 compases.
- Sincronizar UNIT-D con otros secuenciadores.
- Forzar frases repetibles dentro de un patch generativo.

### SEED

Entrada CV para modular el seed.

Esta entrada puede cambiar la geometría completa del grafo. Por eso es una entrada potente y conviene usarla con cuidado.

Funciona especialmente bien con:

- voltajes lentos,
- sample & hold,
- cambios manuales,
- automatizaciones puntuales,
- secuencias muy lentas.

No es recomendable usar audio-rate CV en esta entrada si lo que buscas es estabilidad musical. Cambiar el seed demasiado rápido puede hacer que la frase cambie de identidad constantemente.

### DENS

Entrada CV para modular la densidad de conexiones.

Es una de las entradas más musicales del módulo. Permite hacer que el patrón respire: con menos densidad hay más huecos y rutas frágiles; con más densidad hay más movimiento y más actividad.

Usos típicos:

- Abrir la densidad en un estribillo.
- Reducir conexiones en una intro.
- Modular lentamente con un LFO.
- Controlar la complejidad desde otro secuenciador.
- Crear evolución sin cambiar completamente el seed.

---

## 6. Controles principales

### SEED

Genera una nube de puntos determinista.

Mismo seed = mismo patrón, siempre que los demás parámetros sean iguales.

Usa `SEED` para buscar otra familia de melodías. Es el control más directo para cambiar de idea musical sin tocar el resto del patch.

Consejo práctico: cuando encuentres una frase interesante, no sigas moviendo `SEED` sin guardar el patch o anotar el valor. Es el parámetro que más cambia la identidad del módulo.

### NODES

Define la cantidad de puntos internos del grafo.

Rango conceptual: de pocos nodos a muchos nodos.

- Pocos nodos: patrones más simples, cerrados y repetitivos.
- Muchos nodos: más rutas, más variación, más material para polifonía y contrapunto.

Guía rápida:

```text
8-16 nodos: frases muy simples, loops pequeños, material percusivo.
16-32 nodos: melodías controlables y patrones principales.
32-64 nodos: más exploración, acordes generativos y polifonía.
```

### RADIUS

Define la distancia objetivo usada para conectar puntos.

Es el control más geométrico. Cambiarlo modifica qué nodos se consideran vecinos. Pequeños cambios pueden transformar mucho el grafo.

- Si `RADIUS` es demasiado bajo, puede haber pocas conexiones.
- Si `RADIUS` es demasiado alto, el grafo puede volverse demasiado conectado.
- En la zona media suelen aparecer patrones más musicales.

Consejo: ajusta `RADIUS` mirando el display. Busca una red con suficientes conexiones para moverse, pero no tan densa como para perder dirección.

### DENS

Control principal de densidad musical.

Afecta a cuántas conexiones quedan activas o útiles dentro del grafo.

- Bajo: pocas conexiones, más silencios, patrones frágiles y minimalistas.
- Medio: equilibrio entre repetición y variación.
- Alto: más movimiento, más rutas y más actividad.

`DENS` es ideal para interpretación en directo porque cambia la energía del patrón sin destruir necesariamente toda la identidad del seed.

---

## 7. Controles secundarios

### LOCK

Control bipolar de bloqueo inspirado en el comportamiento de una Turing Machine.

- Centro: secuencia libre.
- Hacia la derecha: bloqueo gradual sobre un bucle de 16 pasos.
- Hacia la izquierda: bloqueo gradual sobre un bucle de 32 pasos.

Cuanto más se aleja del centro, más pasos quedan congelados dentro de la secuencia. En los extremos, la frase queda completamente bloqueada y se repite.

`LOCK` captura notas y gates. En modo polifónico también captura las voces.

Uso práctico:

1. Deja correr la secuencia libremente.
2. Espera a que aparezca una frase interesante.
3. Gira `LOCK` lentamente hacia un lado.
4. Mantén una zona semibloqueada para que haya repetición, pero todavía algo de variación.
5. Vuelve al centro para liberar la secuencia.

### TOL

Control de tolerancia fina de la regla geométrica.

`TOL` ajusta cuánto margen se permite alrededor de `RADIUS` para considerar que dos puntos están conectados.

- Tolerancia baja: regla más estricta, menos conexiones.
- Tolerancia alta: regla más flexible, más conexiones.

Sirve para afinar un patrón que está casi bien pero se siente demasiado vacío o demasiado lleno.

Piensa en `RADIUS` como el gesto grande y en `TOL` como el ajuste fino.

### WALK

Selecciona el modo de recorrido por el grafo.

```text
0 — Vecino hacia adelante
1 — Vecino pseudoaleatorio determinista
2 — Vecino con mayor grado
```

#### WALK 0 — Vecino hacia adelante

Modo más estable y repetitivo. Tiende a producir frases más previsibles.

Útil para:

- bajos,
- secuencias principales,
- patrones que deben mantenerse reconocibles,
- frases que después se pueden bloquear con `LOCK`.

#### WALK 1 — Pseudoaleatorio determinista

Modo más variado, pero repetible. No es azar puro: con los mismos parámetros, el comportamiento puede repetirse.

Útil para:

- melodías generativas,
- texturas,
- leads cambiantes,
- patches donde quieres movimiento sin perder control.

#### WALK 2 — Mayor grado

El walker tiende hacia nodos con más conexiones. Esto empuja el recorrido hacia zonas densas del grafo.

Útil para:

- patrones activos,
- acumulación de energía,
- secciones con más movimiento,
- acentos más frecuentes.

### RNG

Define el rango de pitch en octavas.

No cambia el grafo. Solo cambia cuánta altura melódica se extrae de la posición X de los nodos.

- Bajo: melodías más contenidas.
- Alto: saltos más amplios y frases más abiertas.

Consejo: para bajos usa rangos bajos. Para leads o arpegios generativos, usa rangos medios o altos.

### GLEN

Define la duración del gate.

También afecta al display: el nodo activo se muestra más pequeño con gates cortos y más grande con gates largos.

- Gate corto: frases secas, percusivas, precisas.
- Gate medio: secuencias melódicas naturales.
- Gate largo: notas sostenidas, drones rítmicos o legato modular.

### GDEN

Define la densidad de gates.

El walker puede seguir moviéndose aunque algunos pasos no disparen gate. Esto permite crear silencios sin detener la modulación ni el recorrido interno.

- GDEN bajo: más rests, menos notas disparadas.
- GDEN medio: fraseo con huecos musicales.
- GDEN alto: más actividad y mayor continuidad.

`GDEN` también afecta al display:

- GDEN bajo: nodos más fríos o apagados.
- GDEN alto: nodos más brillantes o cálidos.

---

## 8. Salidas

### V/O

Salida de pitch cuantizado.

El módulo convierte la posición X del nodo actual en una nota dentro de una escala menor fija en C.

Escala usada actualmente:

```text
C minor: 0, 2, 3, 5, 7, 8, 10
```

Conecta esta salida al `V/OCT` de un oscilador, sampler o voz modular.

### GATE

Salida de gate.

Depende de:

- que exista una transición válida,
- la duración definida por `GLEN`,
- la densidad definida por `GDEN`,
- la estructura local del nodo.

Conecta `GATE` a una envolvente, a un generador de percusión, a un sample player o a cualquier entrada de disparo.

### ACC

Salida de acento de 0 a 10V.

Depende del número de conexiones del nodo actual. Un nodo muy conectado produce más acento.

Usos recomendados:

- abrir ligeramente un filtro,
- aumentar la amplitud de un VCA,
- modular decay de una envolvente,
- activar velocity en una voz,
- controlar distorsión o wavefolding,
- añadir énfasis a pasos más importantes.

### X

CV de 0 a 10V basado en la posición X del nodo actual.

Aunque la posición X también se usa para generar pitch, la salida `X` permite usar esa misma información como modulación continua.

Usos recomendados:

- cutoff,
- posición de wavetable,
- índice FM,
- cantidad de reverb o delay,
- mezcla entre dos fuentes,
- panorámica.

### Y

CV de 0 a 10V basado en la posición Y del nodo actual.

Es una segunda dimensión de modulación. Normalmente funciona muy bien para parámetros que no deben moverse igual que el pitch.

Usos recomendados:

- resonancia,
- decay,
- folding,
- tamaño de grano,
- cantidad de modulación,
- color tímbrico.

---

## 9. Display

El display muestra el grafo en tiempo real.

Elementos principales:

- Puntos: nodos.
- Líneas: conexiones.
- Nodo fucsia principal: voz activa principal.
- Otros nodos fucsia pequeños: voces polifónicas adicionales.
- Brillo/color: relación con `GDEN` y la probabilidad de gate.
- Tamaño del nodo activo: relación con `GLEN`.

El display es una herramienta de lectura musical.

Qué puedes diagnosticar mirando el display:

```text
Muy pocas líneas: grafo demasiado vacío.
Demasiadas líneas: grafo demasiado denso.
Movimiento encerrado en una zona: el walker está atrapado en una región.
Movimiento por muchas zonas: frase más variada.
Nodo activo muy pequeño: gates cortos.
Nodo activo grande: gates largos.
Poca intensidad visual: baja densidad de gate.
Alta intensidad visual: mayor actividad de gate.
```

Consejo: no ajustes solo de oído. Mira el grafo. Muchas veces verás claramente por qué un patrón está demasiado parado, demasiado caótico o demasiado lleno.

---

## 10. Polifonía

UNIT-D puede producir salidas polifónicas.

Click derecho sobre el módulo para acceder a las opciones de polifonía.

### Poly voices

Define el número de voces.

Opciones:

```text
1, 2, 3, 4, 6, 8
```

Las salidas `V/O`, `GATE`, `ACC`, `X` e `Y` usan el mismo número de canales.

### Poly seed mode

Define cómo se separan las voces.

#### Shared seed

Todas las voces usan la misma geometría base, pero con offsets internos distintos.

Resultado:

- más coherente,
- más compacto,
- más relacionado entre voces,
- ideal para patrones polifónicos que deben sonar como una sola entidad.

#### Per-voice seed

Cada voz genera su propia geometría interna a partir del seed principal.

Resultado:

- más separación entre voces,
- acordes más abiertos,
- contrapunto más independiente,
- mayor sensación generativa.

Para acordes o contrapunto generativo, prueba:

```text
Poly voices: 3 o 4
Poly seed mode: Per-voice seed
NODES: 48-64
DENS: medio/alto
RNG: 3-4
WALK: 1
GDEN: 60-90%
```

---

## 11. Recetas de patch

### 11.1 Patrón monofónico estable

Objetivo: una frase principal reconocible.

```text
Poly voices: 1
NODES: 16-32
RADIUS: cerca de 1.0
DENS: 30-50%
WALK: 0
RNG: 2
GDEN: 60-80%
GLEN: medio
LOCK: centro o ligeramente a la derecha
```

Patch recomendado:

- `V/O` → oscilador `V/OCT`
- `GATE` → envolvente
- envolvente → VCA
- `ACC` → cutoff o nivel del VCA
- `X` → timbre suave
- `Y` → decay o resonancia

### 11.2 Melodía generativa con huecos

Objetivo: una línea viva, con silencios y movimiento.

```text
Poly voices: 1
NODES: 24-48
WALK: 1
DENS: medio
GDEN: 25-60%
GLEN: corto o medio
RNG: 3
LOCK: centro
```

Consejo: usa `GDEN` para controlar los huecos, no pares el clock. Así la modulación de `X` e `Y` sigue evolucionando aunque no suenen todas las notas.

### 11.3 Bajo modular

Objetivo: patrón controlado, estable y con groove.

```text
NODES: 12-24
RNG: 1-2
WALK: 0
GDEN: 70-100%
GLEN: corto/medio
DENS: bajo/medio
```

Patch recomendado:

- `V/O` → oscilador grave
- `GATE` → envolvente rápida
- `ACC` → filtro o saturación
- `Y` → decay o cutoff secundario

### 11.4 Acordes generativos

Objetivo: acordes o clusters controlados.

```text
Poly voices: 3 o 4
Poly seed mode: Per-voice seed
NODES: 48-64
RNG: 3-4
DENS: medio/alto
WALK: 1
GDEN: 60-90%
GLEN: medio/largo
```

Patch recomendado:

- `V/O` polifónico → oscilador o voz polifónica
- `GATE` polifónico → envolventes polifónicas
- `ACC` → brillo o amplitud
- `X` → movimiento tímbrico común
- `Y` → variación secundaria

### 11.5 Secuencia bloqueada tipo Turing Machine

Objetivo: capturar una frase generativa y convertirla en loop.

1. Deja correr el módulo con `LOCK` en el centro.
2. Busca una frase interesante usando `SEED`, `RADIUS`, `DENS` y `WALK`.
3. Gira `LOCK` lentamente hacia la derecha para capturar un bucle de 16 pasos.
4. Gira `LOCK` hacia la izquierda para capturar una frase más larga de 32 pasos.
5. No lo lleves siempre al máximo: las zonas intermedias suelen ser más musicales.
6. Vuelve al centro para liberar la secuencia.

### 11.6 Modulación generativa sin melodía

Objetivo: usar UNIT-D como generador de CV, no necesariamente como secuenciador de notas.

Patch recomendado:

- No conectes `V/O` si no necesitas pitch.
- Usa `X` para cutoff.
- Usa `Y` para resonancia, decay o mezcla.
- Usa `ACC` para golpes de intensidad.
- Usa `GATE` para disparar eventos ocasionales.

Este enfoque es muy útil para drones, texturas, patches ambientales y sistemas generativos largos.

---

## 12. Relaciones importantes entre controles

### RADIUS + TOL + DENS

Estos tres controles definen cuántas conexiones aparecen y cómo se estructura el grafo.

- `RADIUS` decide la distancia objetivo.
- `TOL` decide cuánto margen se permite alrededor de esa distancia.
- `DENS` decide la densidad musical o práctica de conexiones.

Si el patrón está demasiado vacío:

1. Sube un poco `DENS`.
2. Ajusta `TOL`.
3. Mueve `RADIUS` lentamente.
4. Aumenta `NODES` si necesitas más material.

Si el patrón está demasiado caótico:

1. Baja `DENS`.
2. Baja `TOL`.
3. Prueba `WALK 0`.
4. Reduce `RNG` si los saltos melódicos son demasiado grandes.

### NODES + WALK

`NODES` define cuánta materia prima hay. `WALK` define cómo se recorre.

- Pocos nodos + WALK 0: loops simples.
- Muchos nodos + WALK 1: material generativo más variado.
- Muchos nodos + WALK 2: tendencia a zonas densas y activas.

### GDEN + GLEN

`GDEN` decide cuántas notas se disparan. `GLEN` decide cuánto duran.

- GDEN bajo + GLEN corto: patrón fragmentado y percusivo.
- GDEN bajo + GLEN largo: notas ocasionales, más espaciales.
- GDEN alto + GLEN corto: secuencia rítmica activa.
- GDEN alto + GLEN largo: frase más continua o legato.

### LOCK + RESET

`LOCK` captura comportamiento. `RST` permite volver al inicio.

Si quieres una frase generativa pero repetible en estructura, usa resets periódicos. Si quieres que la frase se convierta en loop, usa `LOCK`.

---

## 13. Flujo de trabajo recomendado

### Método 1 — Encontrar una melodía

1. Empieza con una sola voz.
2. Usa `NODES` entre 16 y 32.
3. Pon `WALK` en 0.
4. Ajusta `RADIUS` hasta que el grafo tenga conexiones suficientes.
5. Sube o baja `DENS` hasta que la frase respire.
6. Ajusta `RNG` para controlar el rango melódico.
7. Cuando aparezca una frase buena, usa `LOCK`.

### Método 2 — Crear evolución lenta

1. Elige un seed que te guste.
2. No modulees `SEED` constantemente.
3. Modula `DENS` lentamente.
4. Usa `GDEN` para abrir y cerrar la cantidad de notas.
5. Usa `X` e `Y` para cambiar timbre.
6. Usa `RST` cada varios compases si quieres estructura.

### Método 3 — Crear material polifónico

1. Activa 3 o 4 voces.
2. Usa `Per-voice seed` si quieres separación clara.
3. Usa `Shared seed` si quieres coherencia.
4. Sube `NODES` a 48-64.
5. Usa `WALK 1`.
6. Mantén `RNG` entre 3 y 4.
7. Controla la densidad con `GDEN` y `DENS`.

---

## 14. Problemas comunes

### No suenan notas

Posibles causas:

- No entra clock en `CLK`.
- `GDEN` está demasiado bajo.
- El grafo tiene pocas o ninguna conexión.
- `DENS`, `RADIUS` o `TOL` están en una zona demasiado restrictiva.
- `GATE` no está conectado a una envolvente o destino adecuado.

Solución rápida:

```text
Sube GDEN.
Sube DENS.
Mueve RADIUS lentamente.
Aumenta TOL.
Prueba más NODES.
Comprueba que CLK recibe pulsos.
```

### El patrón es demasiado caótico

Posibles causas:

- Demasiados nodos.
- `DENS` demasiado alto.
- `TOL` demasiado alto.
- `WALK` en modo demasiado exploratorio.
- `RNG` demasiado amplio.

Solución rápida:

```text
Baja DENS.
Baja TOL.
Prueba WALK 0.
Reduce RNG.
Baja NODES a 16-32.
Usa LOCK parcialmente.
```

### El patrón es demasiado repetitivo

Solución rápida:

```text
Sube NODES.
Prueba WALK 1.
Sube ligeramente DENS.
Modula DENS con un LFO lento.
Reduce LOCK o llévalo al centro.
Usa más RNG.
```

### El display parece vacío

El grafo tiene muy pocas conexiones.

Prueba:

```text
Aumentar DENS.
Aumentar TOL.
Mover RADIUS.
Aumentar NODES.
Cambiar SEED.
```

### El display parece demasiado lleno

El grafo tiene demasiadas conexiones.

Prueba:

```text
Bajar DENS.
Bajar TOL.
Mover RADIUS a otra zona.
Reducir NODES.
Usar WALK 0 para más estabilidad.
```

### La polifonía suena borrosa

Prueba:

```text
Reducir voces.
Usar Per-voice seed para separar voces.
Bajar RNG.
Bajar DENS.
Usar envelopes más cortas.
Filtrar o panoramizar voces.
```

---

## 15. Notas técnicas

- UNIT-D no recalcula el grafo en cada muestra de audio.
- El grafo se recalcula cuando cambian suficientemente `SEED`, `NODES`, `RADIUS`, `TOL` o `DENS`.
- La generación es determinista.
- Si el grafo queda sin conexiones, el módulo evita comportamientos inválidos: mantiene pitch estable y no dispara gates imposibles.
- La escala y la raíz están fijas por ahora: C menor.
- Las salidas `V/O`, `GATE`, `ACC`, `X` e `Y` pueden trabajar en modo polifónico según el ajuste de voces.

---

## 16. Limitaciones actuales

Estas limitaciones no son fallos; simplemente definen el estado actual del módulo:

- La escala está fija en C menor.
- No hay selector de raíz o escala en el panel.
- La generación está basada en una geometría interna determinista.
- El módulo está diseñado para control musical, no para demostrar matemáticas.
- Cambios extremos de seed o geometría pueden producir saltos musicales drásticos.

Posibles mejoras futuras:

- selector de escala,
- selector de raíz,
- más modos de recorrido,
- salida de índice de nodo,
- control CV adicional para `LOCK`,
- cuantización configurable,
- presets de comportamiento geométrico.

---

## 17. Glosario

### Nodo

Punto del grafo. En UNIT-D, cada nodo representa un estado musical.

### Conexión / arista

Línea entre dos nodos. Define una transición posible.

### Walker

Recorrido interno que se mueve de nodo en nodo con cada clock.

### Grado

Número de conexiones que tiene un nodo. En UNIT-D influye en el acento.

### Grafo de distancia unidad

Tipo de estructura geométrica donde los puntos se conectan según una regla de distancia. UNIT-D toma inspiración de esta idea, pero la usa como motor musical, no como demostración matemática.

### Determinista

Significa que, con los mismos parámetros, el resultado se repite igual.

### Gate density

Cantidad de pasos que producen gate. En UNIT-D se controla con `GDEN`.

### Gate length

Duración del gate. En UNIT-D se controla con `GLEN`.

---

## 18. Idea central

UNIT-D convierte geometría en música.

No es un secuenciador clásico de 16 pasos ni un generador aleatorio sin memoria. Es una red de posibilidades donde cada clock toma una decisión basada en la forma del grafo.

Cuando cambias la forma, cambia la frase. Cuando bloqueas el recorrido, aparece el loop. Cuando abres la densidad, el sistema respira. Cuando usas polifonía, la misma geometría puede convertirse en acordes, contrapunto o texturas.

La clave es no intentar controlarlo todo. Deja que el grafo proponga material, pero usa los controles para llevarlo hacia una dirección musical.
