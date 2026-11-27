# Manual de ECLIPSE + ECLIPSE PITCH

**Versión del manual:** 1.0

**Versión del plugin:** Animatek, la siguiente a la 2.5.10

**Módulos:** ECLIPSE y ECLIPSE PITCH para VCV Rack

**Ancho:** 8 HP cada uno

---

## 1. Qué es

**ECLIPSE** es un secuenciador euclidiano doble: dos ritmos euclidianos dibujados como dos anillos de LEDs en un mismo círculo. El anillo **A**, por fuera, avanza con el reloj que entra; el anillo **B**, por dentro, avanza con ese reloj dividido o multiplicado. Cada anillo tiene su salida de gate, y otras tres los combinan con lógica: **AND** (golpean los dos), **OR** (golpea cualquiera) y **XOR** (golpea solo uno). Entran dos ritmos y salen cinco.

**ECLIPSE PITCH** es su expansor de notas. Pegado a la derecha de un ECLIPSE, convierte uno de esos cinco gates en una melodía dentro de una escala, elegida al azar o movida paso a paso por los golpes de cada anillo, y añade un **LOCK** que repite los últimos pasos de todo, gates y notas por igual, como una Turing Machine.

ECLIPSE funciona solo como generador de ritmos. ECLIPSE PITCH no hace nada sin un ECLIPSE a su izquierda.

---

## 2. Para empezar

1. Conecta un reloj a `CLK` de ECLIPSE (las semicorcheas son un buen comienzo).
2. Conecta `A` a un bombo, `B` a un charles y `XOR` a una caja o una percusión.
3. Gira `HITS` y `ROT` de cada anillo hasta que el groove encaje; pon `RATIO` en `×2` para que B vaya al doble.
4. Para una melodía: botón derecho en ECLIPSE y **Add pitch expander**. ECLIPSE PITCH aparece pegado a su derecha y ya enlazado.
5. Conecta `V/OCT` de ECLIPSE PITCH a un oscilador y el gate elegido en `SRC` (OR por defecto) a su envolvente.
6. Cuando oigas algo que te guste, pulsa `LOCK`.

---

## 3. ECLIPSE

### Cómo funcionan los anillos

Cada anillo reparte `HITS` golpes lo más uniformemente posible entre `LEN` pasos (el ritmo euclidiano, de la misma familia que el algoritmo de Bjorklund). El paso 1 está arriba y el anillo gira en el sentido de las agujas del reloj. `ROT` gira el patrón entero en ese mismo sentido.

En la pantalla:

- Los **golpes** se encienden del color de su anillo (A azul por defecto, B naranja por defecto); los silencios son puntos grises pequeños.
- El **paso que suena** va en blanco; si es un golpe, brilla.
- El paso donde **empieza el patrón** se dibuja algo más grande y más brillante, y gira con `ROT`, así que la rotación se lee en el anillo.
- En el centro: `HITS/LEN` de A, `HITS/LEN` de B y la relación del reloj de B.
- Mientras **AND** dispara, el centro se ilumina suavemente: el eclipse.
- Mientras ECLIPSE PITCH tiene un bloqueo puesto, `LOCK` y su longitud salen en la esquina de arriba a la derecha.

### Fila de arriba

| Control | Qué hace |
| --- | --- |
| `CLK` | Entrada de reloj. Un flanco de subida (por encima de 2 V, después de bajar de 0,1 V) hace avanzar el anillo A. |
| `RST` | Entrada de reset. Los dos anillos vuelven al principio, y el reloj siguiente toca el paso 1. |
| `RATIO` | El reloj de B respecto al de A: ÷4, ÷3, ÷2, ×1, ×2, ×3, ×4. Por defecto ×1. |
| `GATE` | Duración del gate, del 5 al 95 % del paso que lo dispara. Por defecto 50 %. |

### Filas de los anillos: A y B

| Control | Rango | Qué hace |
| --- | --- | --- |
| `LEN` | 1–64 | Pasos del anillo. Por defecto 16 en A y 8 en B. |
| `HITS` | 0–64 | Golpes repartidos por el anillo. Por encima de `LEN` cuenta como `LEN`. Por defecto 5 en A y 3 en B. |
| `ROT` | 0–63 | Gira el patrón en el sentido de las agujas del reloj, dando la vuelta en `LEN`. |

**Haz clic en la letra A o B** al lado de una fila para sortear ese anillo: `LEN` de 3 a 32, `HITS` de 1 a uno menos que `LEN` (un anillo lleno de golpes no dice nada) y `ROT` en cualquier punto. La letra se ilumina al pasar el ratón, y un solo deshacer devuelve los tres mandos.

### El reloj de B

- **Dividido** (÷N): B avanza en uno de cada N ticks de A, contados desde el último reset, así que un reset vuelve a alinear los dos.
- **Multiplicado** (×N): B avanza en cada tick de A y en N−1 ticks intermedios, repartidos con el tiempo medido entre los dos últimos flancos de reloj. El primer reloj después de arrancar todavía no tiene nada que medir, así que B no hace intermedios hasta el segundo. Si el tempo sube y llega un flanco antes de acabar los intermedios, B se salta los que faltan sin tocarlos, así que nunca se desfasa de A.

### Las salidas de lógica

Cada anillo tiene un estado: si su paso actual es un golpe. La lógica se calcula en cada tick de cualquiera de los dos relojes, es decir, al ritmo del más rápido:

- **AND**: golpean A y B.
- **OR**: golpea A, B o los dos.
- **XOR**: golpea uno de los dos, pero no ambos.

Cuánto dura el golpe de un anillo se elige en el menú contextual, en **Logic**:

- **Hold** (por defecto): un golpe dura todo su paso. Con B más rápido que A, cada tick de B dentro de un golpe de A se compara con él, así que AND hace ratchets dentro de los golpes de A. Con B más lento, un golpe de B abarca varios pasos de A.
- **Trigger**: un anillo solo cuenta en el tick en que avanza. La lógica es entonces la de los golpes que caen en el mismo instante.

Con `RATIO` en ×1 los dos dan lo mismo.

### Salidas

`A`, `B`, `AND`, `OR` y `XOR`: gates de 10 V, cada uno con una luz junto a su nombre.

- `A` y `B` duran `GATE` del paso de su propio anillo; las de lógica, `GATE` del paso del reloj más rápido.
- Un gate que vuelve a dispararse mientras sigue alto baja 1 ms antes, para que el módulo de después vea un flanco nuevo.

### Menú contextual

- **Add pitch expander**: crea ECLIPSE PITCH pegado a la derecha, apartando lo que hubiera, y lo enlaza. Pone *Pitch expander attached* si ya hay uno.
- **Ring A colour**: el color del módulo, de la paleta de Animatek. Cambia el anillo A y todo lo azul del panel. Por defecto azul.
- **Ring B colour**: el color del anillo B y de todo lo suyo (`RATIO`, la letra B, la salida B y su luz). En **Auto** (por defecto) cada ECLIPSE que se añade o se duplica coge un color que no tenga otro del patch, para distinguir varios. El azul no se ofrece porque es el del anillo A.
- **Logic**: Hold o Trigger (ver arriba).

Todo esto se guarda con el patch.

---

## 4. ECLIPSE PITCH

### Cómo se engancha

Tiene que tocar el borde derecho de un ECLIPSE. Lo más fácil es **Add pitch expander** en el menú de ECLIPSE, que lo coloca ahí. Suelto, su pantalla pide que lo pegues a la derecha de un ECLIPSE y sus salidas se quedan a 0 V.

Toda la secuencia sigue en ECLIPSE, porque el bloqueo tiene que rebobinar también los anillos: ECLIPSE PITCH le manda sus ajustes y ECLIPSE le devuelve la nota, con una muestra de retraso en cada sentido. ECLIPSE PITCH toma el color de su ECLIPSE (Ring A colour), así que los dos se ven como un solo instrumento.

### Cómo se elige la nota

Cada vez que dispara el gate elegido en `SRC`, ECLIPSE elige una nota nueva y la mantiene hasta que ese gate vuelve a disparar. La nota es un grado de la escala, dentro de `RANGE` octavas por encima de la tónica.

- **RND**: un grado cualquiera del rango, al azar.
- **ACUM** (por defecto): un acumulador. Cada golpe de A lo mueve `+A` grados, cada golpe de B lo mueve `+B` grados, y da la vuelta al llegar arriba del rango. La nota es el valor del acumulador cuando dispara `SRC`. Como los dos anillos se repiten, la melodía también, pero puede tardar mucho en volver a donde empezó. Con `+A` en +1 y `+B` en 0 sube la escala un grado por cada golpe de A.

### Controles

| Control | Rango | Qué hace |
| --- | --- | --- |
| `MODE` | RND / ACUM | Cómo se elige la nota. Por defecto ACUM. |
| `SRC` | A, B, AND, OR, XOR | Qué gate de ECLIPSE elige nota nueva. Por defecto OR. |
| `RANGE` | 1–4 octavas | Hasta dónde se mueve la nota. Por defecto 2. |
| `SCALE` | 11 escalas | Menor (por defecto), mayor, dórica, frigia, lidia, mixolidia, menor armónica, pentatónica menor, pentatónica mayor, blues y cromática. La misma lista que UNIT-D. |
| `KEY` | C–B | La tónica. Por defecto C. |
| `OCT` | −2 a +2 | Octava de las dos salidas. |
| `+A` | −7 a +7 | ACUM: grados que mueve la nota cada golpe de A. Por defecto +2. |
| `+B` | −7 a +7 | ACUM: grados que mueve la nota cada golpe de B. Por defecto −1. |
| `LOOP` | 1–64 | Pasos que repite el bloqueo. Por defecto 16. |
| `LOCK` | botón | Bloquea y desbloquea (encendido mientras está bloqueado). |

`SCALE`, `KEY`, `OCT` y `ROOT IN` actúan al momento, también sobre la nota que ya suena.

### Entradas y salidas

| Jack | Qué hace |
| --- | --- |
| `ROOT` (entrada) | Transpone la tónica, 1 V por octava, redondeado al semitono. Conecta aquí el V/OCT de otro secuenciador para que la melodía siga sus notas. |
| `LOCK` (entrada) | Bloqueado mientras está a 1 V o más, igual que con el botón. |
| `V/OCT` | La nota. 0 V es C4 (tónica C, octava 0, primer grado). |
| `ROOT` (salida) | La tónica en la misma octava: la nota sobre la que se construye la melodía. |

### LOCK

En cada flanco de reloj ECLIPSE guarda una copia de todo lo que decide lo que viene: dónde están los dos anillos, el acumulador y el generador aleatorio. Al activar el bloqueo, en el siguiente flanco vuelve a la copia de hace `LOOP` pasos y sigue desde ahí; cada `LOOP` pasos vuelve a la misma copia. Así se repite lo que acaba de sonar, **gates y notas por igual**.

- `LOOP` cuenta pasos del reloj de A. Justo después de un reset hay menos pasos guardados, y el bloqueo repite los que haya.
- Si `LOOP` no coincide con `LEN`, el bloqueo recorta el polirritmo y sale un patrón nuevo.
- Los mandos siguen funcionando con el bloqueo puesto: cambiar `HITS`, `ROT`, `SCALE` o los pasos cambia el patrón que se repite.
- Al desbloquear, la secuencia sigue desde donde esté el bucle.
- Un reset con el bloqueo puesto vuelve a empezar el bucle desde ese punto: entonces repite los `LOOP` pasos siguientes.
- Sin ECLIPSE PITCH no hay bloqueo: sus controles están en él.

### La rueda de notas

La pantalla se dibuja como los anillos de ECLIPSE:

- Un anillo de LEDs por octava de `RANGE`, la grave dentro; el de fuera tiene el tamaño del anillo A de ECLIPSE, así que las dos pantallas se leen juntas.
- Un LED por grado de la escala, con la tónica arriba, más grande y del color del módulo.
- Las últimas 12 notas encendidas y unidas por una línea en el orden en que sonaron, más tenues cuanto más viejas; la que suena va en blanco con halo. Con el bloqueo puesto se dibuja la misma figura una y otra vez.
- En el centro, la nota que suena (C4 = 0 V).
- En las esquinas: tónica y escala, el bloqueo y su longitud, el modo con su fuente y, en ACUM, los pasos `+A` y `+B`, cada uno del color de su anillo.

Las 32 últimas notas se guardan en el módulo y no en la pantalla, para no perder ninguna aunque lleguen más rápido de lo que se refresca.

---

## 5. Ejemplos de patch

### Tres voces con dos anillos

`A` (16 pasos, 4 golpes) a un bombo, `B` (`RATIO` ×2, 8 pasos, 3 golpes) a un charles cerrado, `XOR` a una palmada. Gira `ROT` de B y escucha cómo la palmada se mueve entre los dos.

### Ratchets dentro del bombo

Logic **Hold**, `RATIO` ×3, B con muchos golpes. `AND` a una caja: redobla solo dentro de los golpes de A.

### Un bajo que contesta a la batería

ECLIPSE PITCH en ACUM, `SRC` en XOR, `RANGE` 1, `+A` +3, `+B` −2, pentatónica menor. `V/OCT` y `XOR` a una voz de bajo. La línea sube con el bombo y baja con los charles.

### Una frase bloqueada

MODE RND, `SRC` en OR, `LOOP` 8. Déjalo sonar hasta que salga una frase que te guste y pulsa `LOCK`. Gira `KEY` o conecta una progresión a `ROOT` para mover la frase bloqueada.

### Varios ECLIPSE en la misma tonalidad

Conecta la salida `ROOT` de un ECLIPSE PITCH a la entrada `ROOT` de los demás, deja su `KEY` en C (se suma a lo que llega) y da a cada uno su `OCT`: uno una octava abajo para el bajo, otro una arriba para una melodía, todos en la misma tonalidad. Cada ECLIPSE coge su propio color de anillo B en Auto, así que se distinguen bien.

---

## 6. Notas

- Los dos módulos son monofónicos.
- El periodo del reloj solo se acepta entre 1 ms y 5 s; una pausa más larga se toma como reloj parado, no como tempo.
- Los ajustes de ECLIPSE PITCH llegan a ECLIPSE con una muestra de retraso, así que un bloqueo pulsado justo en un flanco de reloj entra en el siguiente.
