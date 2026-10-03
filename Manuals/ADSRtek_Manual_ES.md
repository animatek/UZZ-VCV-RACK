# Manual de usuario de ADSRtek

**Versión del manual:** 1.0

**Versión del plugin:** Animatek 2.5.9

**Módulo:** ADSRtek para VCV Rack

**Anchura:** 8 HP

---

## 1. Descripción general

**ADSRtek** es una envolvente ADSR y AD modelada sobre medidas de las envolventes de un modular virtual analógico clásico de los 90, de los que hicieron de los ataques rápidos y los decays secos su seña de identidad. No es una envolvente genérica con etiqueta vintage: sus tiempos, sus curvas y su comportamiento salen de grabar el original paso a paso y ajustar lo que salía.

Lo que hace y casi ninguna envolvente hace:

- **Tiempos en 128 pasos, tal como se midieron.** Los mandos recorren los 128 pasos del original, de 0,5 ms a unos 50 s, con los tiempos que tarda de verdad.
- **Tres formas de ataque.** Log sube de golpe y se suaviza al llegar, Lin es una rampa recta, Exp arranca despacio y se dispara al final. Log y Exp se curvan distinto en cada paso, como en el original.
- **Decays exponenciales y secos.** Decay y release son la misma exponencial, medida hasta llegar al 1 % del recorrido.
- **Los redisparos continúan.** Un gate nuevo nunca vuelve a cero: el ataque sigue desde el nivel actual, tan rápido como su curva lo es desde ahí.
- **Control a 24 kHz.** Se mueve en escalones de 24 kHz como el original, y se puede desactivar.

Es polifónica (hasta 16 canales, según `GATE`), con CV en cada etapa y un VCA incorporado.

---

## 2. Inicio rápido

1. Conecta un gate a `GATE` y `ENV` a lo que quieras modelar: el corte de un filtro, un VCA.
2. O pasa el propio audio por `IN` y sácalo por `OUT`: el VCA va incorporado.
3. Para un pulsado, empieza con un ataque Lin al mínimo, `DECAY` hacia la mitad, `SUSTAIN` a 0 y un `RELEASE` corto; sube `SUSTAIN` y alarga el ataque para un pad.
4. Pon el selector de modo en **AD** para sonidos percusivos que tocan su forma entera con un trigger corto.

---

## 3. Controles

### Modo: ADSR / AD

- **ADSR:** ataque mientras el gate está alto, decay hasta `SUSTAIN`, se mantiene, y release cuando el gate baja. Soltar el gate durante el ataque o el decay pasa directamente al release, desde donde esté la envolvente.
- **AD:** un trigger toca el ataque hasta arriba y el decay hasta cero, haga lo que haga el gate después. `SUSTAIN` y `RELEASE` no hacen nada en este modo. La opción del menú **AD: gate release cuts the attack** lo hace comportarse como la envolvente AD del original con su opción de gate activada: soltar el gate durante el ataque pasa directamente al decay.

### Forma del ataque: LOG / LIN / EXP

- **LOG:** rápido al principio, se suaviza al llegar arriba. La más pegada.
- **LIN:** una rampa recta.
- **EXP:** arranque lento y final empinado: crece.

La forma cambia la curva y, un poco, el tiempo: en los ajustes más cortos, un ataque Exp nunca baja de unos 0,9 ms. La ayuda del mando `ATTACK` muestra el tiempo real para la forma elegida.

### ATTACK, DECAY, RELEASE

Tiempos en los 128 pasos del original. La ayuda muestra el tiempo que tarda de verdad cada uno: del inicio al máximo en el ataque, hasta el 1 % en decay y release. Siguen la tabla publicada del original con un error mediano de medio punto, salvo arriba del todo, donde los tiempos medidos se alargan (un ataque puede tardar hasta 50 s) y se usan tal cual. Entre pasos el mando interpola, así que es continuo.

### SUSTAIN

El nivel que se mantiene mientras el gate sigue alto, de 0 a 100 %. Sigue al mando y a su CV mientras se mantiene.

### LED de actividad

Entre los dos selectores. Su brillo es el nivel de la envolvente del primer canal.

---

## 4. Entradas y salidas

### CV: A, D, S, R

CV polifónico sobre cada etapa, sumado a su mando. En los tiempos, **1 V mueve 12,7 pasos**, así que 10 V recorren los 128; en `SUSTAIN`, **1 V suma un 10 %**. Los tiempos se leen continuamente, así que modular un tiempo durante una etapa cambia su velocidad al momento.

### GATE

Entrada de gate. Un flanco de subida (por encima de 1 V, tras bajar de 0,1 V) inicia el ataque; mientras sigue alto en modo ADSR, la envolvente se mantiene. Su número de canales fija la polifonía del módulo.

### RETRIG

Un flanco de subida reinicia el ataque desde el nivel actual. En modo ADSR solo actúa mientras `GATE` está alto; en modo AD dispara por sí solo.

### AMP

Escala la envolvente, de 0 a 10 V para 0 a 100 %; sin cable, es la escala completa. Conecta aquí un CV de velocidad para envolventes sensibles a la velocidad. Afecta a `ENV` y al VCA.

### IN / OUT

El VCA incorporado: `OUT` es `IN` por la envolvente (después de `AMP` y de la inversión, si está activa).

### ENV

La envolvente, de 0 a 10 V.

---

## 5. Menú contextual

- **24 kHz control-rate steps** (activado por defecto): la envolvente se mueve con un reloj de 24 kHz y mantiene cada valor entre ticks, como el original. Desactivado, se mueve en cada muestra. La diferencia solo se oye en los ataques más rápidos.
- **Invert envelope:** `ENV` y el VCA siguen a 1 menos la envolvente, como el interruptor de inversión del original: 10 V en reposo, y baja con el gate.
- **AD: gate release cuts the attack:** ver el modo AD más arriba.

Las tres se guardan con el patch.

---

## 6. Cómo se hizo

Se grabaron a 96 kHz las envolventes del original, cada paso del ataque (en cada forma), del decay y del release, además de redisparos, gates cortos y la envolvente AD: unas 650 grabaciones. A partir de ellas:

- se tomaron los tiempos de ataque por paso y por forma, y las constantes de tiempo del decay por paso;
- se vio que los ataques Log y Exp dependen del nivel: una aproximación exponencial a un objetivo por encima del máximo, cortada al llegar, y un crecimiento exponencial desde cero. Un número por paso, ajustado a las grabaciones, las reproduce con un error del 0,2 % (Log) y del 0,6 % (Exp) de la escala, salvo los ataques Exp de unos cientos de microsegundos, demasiado cortos para tener forma;
- después se enfrentó el módulo al original con los mismos ajustes y gates: pulsados, pads, crecidas, gates cortos, redisparos en el release, triggers y gates en AD. El peor error estuvo entre el 0,06 % y el 1,1 % de la escala en todos los casos salvo uno, un ataque de 0,46 ms, desplazado una fracción de un tick de 24 kHz.

Las grabaciones no podían resolver nada por debajo de unos −76 dB. Ahí la envolvente se asienta en su objetivo; haga lo que haga el original tan abajo, no se oye.

---

## 7. Ejemplos de patch

### Bajo pulsado

Modo ADSR, ataque LOG a 0, `DECAY` sobre 0,2 s, `SUSTAIN` a 0, `RELEASE` corto. `ENV` al corte del filtro, `IN`/`OUT` alrededor del oscilador.

### Pad que crece

Ataque EXP sobre 1 s, `DECAY` largo, `SUSTAIN` alto, `RELEASE` de 2-3 s. Redispararlo en el release lo hace crecer desde donde esté, sin bache.

### Percusión

Modo AD, ataque LIN a 0, `DECAY` de 50-150 ms, disparado por un secuenciador. Conecta la velocidad a `AMP`.
