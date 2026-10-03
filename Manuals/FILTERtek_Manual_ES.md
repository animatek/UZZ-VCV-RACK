# Manual de usuario de FILTERtek

**Versión del manual:** 1.0

**Versión del plugin:** Animatek 2.5.9

**Módulo:** FILTERtek para VCV Rack

**Anchura:** 8 HP

---

## 1. Descripción general

**FILTERtek** es un filtro multimodo modelado sobre medidas del filtro multimodo de un modular virtual analógico clásico de los 90. Como **ADSRtek**, sale de grabar el original y ajustar lo que salía, no de su código.

- **Cuatro tipos:** paso bajo, paso banda, paso alto y banda eliminada, a **12 o 24 dB por octava**.
- **Su resonancia**, de un leve realce a un pico que resuena con una Q de varios miles, con el **gain control** del original, que baja el nivel a medida que sube la resonancia.
- **Su saturación:** el original calcula en punto fijo, y sus valores internos chocan con un techo duro. Llevado a la resonancia recorta de una forma muy suya, y FILTERtek la reproduce.
- Polifónico (hasta 16 canales, según `IN`), con V/oct, FM y CV de resonancia.

---

## 2. Inicio rápido

1. Conecta un oscilador a `IN` y saca `OUT` a un VCA o a la mezcla.
2. Gira `FREQ` para abrir o cerrar el filtro, y `RES` para añadir resonancia.
3. Conecta una envolvente (ADSRtek, por ejemplo) a `FM` y sube el trimmer `FM` para un barrido de filtro.
4. Conecta el pitch del teclado a `V/OCT` para que el filtro siga las notas.

---

## 3. Controles

### FREQ

El corte, en los 128 pasos de un semitono del original: **330 Hz en el paso 60**, de unos 10 Hz en el 0 a 15,8 kHz en el 127. La ayuda lo muestra en Hz. (El filtro queda en realidad unos 2 cents por debajo de esa cifra, como el original.)

### RES

La resonancia, en 128 pasos. La Q empieza en 0,5 (sin pico) y crece cada vez más deprisa: unos 2 en el 64, 7 en el 96, 27 en el 112, 100 en el 120, y varios miles en el 127, donde el filtro resuena casi un segundo sin llegar a oscilar por sí solo. Su amortiguación sigue también al corte, como en el original: con el mismo `RES`, el pico se afila al subir.

### Botón de tipo y LEDs

El botón recorre **LP**, **BP**, **HP** y **BR**; el LED dice cuál está activo. El tipo está también en el menú contextual.

- **LP, BP, HP** son las tres salidas del mismo filtro. El paso banda no está normalizado: su pico sube con la resonancia.
- **BR** (banda eliminada) es una muesca con su propia amortiguación, mucho más suave que la resonancia: `RES` la estrecha solo un poco.

### SLOPE: 12 / 24

12 dB por octava es una sección de filtro; 24 dB por octava son dos secciones iguales en cascada, cada una con su resonancia, más baja, así que un pico de 24 dB es más suave que uno de 12 con el mismo `RES`.

### GAIN: ON / OFF

El gain control del original. **Activado** (por defecto), el nivel baja al subir la resonancia, hasta unos −40 dB con la resonancia al máximo, y el pico no se dispara. **Desactivado**, la banda de paso queda a nivel completo y el pico se eleva sobre ella, lo que lleva el filtro a su saturación mucho antes.

En 12 dB el gain control actúa en la entrada; en 24 dB, entre las dos secciones, así que la primera recibe siempre la señal entera. Por eso una señal fuerte por un filtro de 24 dB con resonancia se rompe aunque el gain control esté activado, como en el original.

### Trimmers FM y RES CV

Cuánto mueven `FM` y `RES CV` el corte y la resonancia, de −100 % a +100 %. En 0 los jacks no hacen nada.

---

## 4. Entradas y salidas

### IN / OUT

Entrada y salida de audio. El nivel importa: el filtro satura por dentro hacia **±20 V**, donde está el fondo de escala del original, y un oscilador normal de ±5 V queda donde queda uno en el original. Señales más calientes, o `GAIN` desactivado, lo llevan a su saturación.

### V/OCT

Mueve el corte a 1 V por octava, sin trimmer: conecta aquí el pitch de una voz para que el filtro siga al teclado.

### FM

Mueve el corte de forma exponencial, escalado por el trimmer `FM`: al 50 % es 1 V por octava, al 100 % dos octavas por voltio (la cantidad máxima del original).

### RES CV

Mueve la resonancia, escalado por su trimmer: al 100 %, 1 V son 24 pasos.

---

## 5. Cómo se hizo

Se midió el filtro del original con ruido, senos, impulsos y sierras, en cada paso de corte y de resonancia, con las dos pendientes, los cuatro tipos, con y sin gain control y a niveles crecientes. Lo que es, según las medidas:

- un **filtro de variables de estado de Chamberlin**, que encaja con las grabaciones a 0,02 dB y un grado de fase en todo el recorrido, con su corte siguiendo la tabla del editor y su amortiguación siguiendo al corte;
- **24 dB como dos secciones iguales**, no una escalera;
- **aritmética saturante**: cada valor interno se limita al fondo de escala al guardarse. Con eso, el módulo reproduce la ganancia y los armónicos del original saturado con unas centésimas de dB de diferencia.

Después se enfrentó el módulo al original con las mismas señales: diez casos que cubren todos los tipos, las dos pendientes, resonancia alta y saturación fuerte. La diferencia se quedó entre 33 y 55 dB por debajo de la señal en nueve de ellos. En el décimo, un filtro de 24 dB con resonancia 124 llevado a fondo a la saturación, las formas de onda se separan, como hace un filtro tan cerca de oscilar con la mínima diferencia, pero su espectro coincide con medio dB de mediana.

Por debajo de unos 100 Hz y con resonancia muy alta el pico era demasiado estrecho para medirlo con precisión; ahí las tablas salen de las mismas reglas, medidas donde se pudo.
