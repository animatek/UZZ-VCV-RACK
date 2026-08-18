# Manual de usuario de UZZ y UZZ-X
**Versión del manual:** 1.0

**Módulos:** UZZ y UZZ-X para VCV Rack

**Versión del plugin:** Animatek 2.5.5

**Formato de UZZ:** secuenciador de 16 pasos con clock externo

**Formato de UZZ-X:** expansor de CV y triggers para UZZ
---

## 1. Descripción general
UZZ es un secuenciador de 16 pasos para pitch, gates y dos líneas de
modulación. Combina secuenciación convencional con probabilidad por paso,
ratchets, acumulación, ventanas activas seleccionables y diez modos de
dirección.
Cada paso físico tiene siete filas programables:
- `MODE`
- `PITCH`
- `OCT`
- `DUR`
- `MOD1`
- `MOD2`
- `PROB`
UZZ no tiene generador de tempo interno.
Solo avanza cuando hay un cable conectado a `CLK` y existe un clock externo.
Su procesador de clock puede dividir o multiplicar ese reloj con `RATIO` y
después aplicar `SWING` a los ticks resultantes de la secuencia.
UZZ-X añade control por voltaje de la ventana activa, dirección, ratio de
clock, swing, probabilidad y cantidad del acumulador. También añade selección
absoluta de pasos, rotación de la secuencia, reset del acumulador y un gate de
inversión.
---

## 2. Inicio rápido
1. Añade UZZ al rack.
2. Conecta un clock a `CLK`.
3. Conecta `V/OCT` a la entrada de pitch de un oscilador.
4. Conecta `GATE` a una envolvente o entrada de gate de una voz.
5. Conecta la envolvente a un VCA para controlar la amplitud de la secuencia.
6. Ajusta `RATIO` a `×1`, `STEPS` a `16`, `START` a `1` y `DIR` a `Forward`.
7. Deja el control global `PROB` en `100%`.
8. Introduce notas con las filas `PITCH` y `OCT`.
9. Ajusta `DUR` para la longitud de gate y usa `MOD1` o `MOD2` para cambios de
   timbre.
10. Conecta un trigger a `RESET` si UZZ debe reiniciarse junto al resto del
    patch.
Ajustes iniciales útiles:
```text
MODE: Play en todos los pasos
PITCH: 0..11 semitonos
OCT: 0
DUR: 50%
PROB: 100%
RATIO: ×1
STEPS: 16
START: 1
DIR: Forward
SWING: 0%
Cantidad ACUMM: 1 st
Wrap ACUMM: OFF
SLEW: 0 s
Selector MODE: GATE
```
---

## 3. Ventana activa y disposición de pasos
El panel siempre contiene 16 pasos físicos, numerados del 1 al 16.
`START` y `STEPS` definen cuáles forman la ventana activa.
### STEPS
- Rango: `1` a `16` pasos.
- Valor por defecto: `16`.
- Define el número de posiciones activas.
### START
- Rango: paso `1` a `16`.
- Valor por defecto: paso `1`.
- Define el primer paso físico de la ventana activa.
La ventana activa continúa desde el final hasta el principio del panel. Por
ejemplo, `START = 13` y `STEPS = 8` selecciona los pasos físicos
`13, 14, 15, 16, 1, 2, 3, 4`.
Los modos de dirección, desplazamientos de fila, rotación de UZZ-X, `ADDR` y
la salida poly funcionan con relación a esta ventana activa.
La luz azul identifica el paso físico seleccionado actualmente.
---

## 4. Referencia completa de filas por paso
### MODE
Pulsa repetidamente el botón de modo de un paso para elegir uno de ocho
comportamientos:
```text
Play, Mute, Skip, Accum Up, Accum Down, Pulse, Gated, Hold
```
Los modos se describen por completo en la sección 5.
El valor por defecto de todos los pasos es `Play`.
### PITCH
- Rango por defecto: `0` a `11` semitonos.
- Rango opcional en el menú contextual: `0` a `23` semitonos.
- Valor por defecto: `0` semitonos.
- Los valores se cuantizan a semitonos enteros.
El nombre de nota mostrado combina `PITCH` con `OCT`.
La salida de pitch también incluye la transposición y el offset acumulado del
paso.
### OCT
- Rango: `-2` a `+2` octavas.
- Valor por defecto: `0` octavas.
- Los valores se cuantizan a octavas enteras.
### DUR
- Rango: `1%` a `95%` de un periodo efectivo de secuencia, sujeto al límite
  máximo de gate de `2 s` descrito en la sección de timing.
- Valor por defecto: `50%`.
En el modo global `GATE`, define la fracción activa del gate.
En el modo global `TRIG`, los pulsos de salida duran aproximadamente `10 ms`,
por lo que `DUR` no alarga el trigger.
### MOD1 y MOD2
- Rango del knob: `0` a `10`.
- Valor por defecto del knob: `0`.
- Mapeo de salida por defecto: `0V` a `10V`.
Cada línea tiene un rango de voltaje independiente en el menú contextual de
UZZ. El rango seleccionado mapea el recorrido completo `0..10` del knob al
rango completo de voltaje.
### PROB
Este control bipolar por paso tiene dos funciones.
- Centro y lado izquierdo: probabilidad de gate desde `100%` hasta `0%`.
- Centro/valor por defecto: `100%`.
- Lado derecho: número de pulsos `×2` a `×8`.
- El número de pulsos solo se usa en los modos `Pulse`, `Gated` y `Hold`.
- Un ajuste positivo de número de pulsos tiene `100%` de probabilidad por paso.
- `Play`, `Accum Up` y `Accum Down` siempre usan un gate aunque el control esté
  en el lado de número de pulsos.
La probabilidad efectiva de reproducción es la probabilidad del paso
multiplicada por la probabilidad global.
---

## 5. Modos de paso
### Play
El paso actualiza pitch y modulación y produce un gate o trigger cuando supera
la prueba de probabilidad.
### Mute
La posición sigue formando parte de la secuencia y consume su tiempo normal,
pero no produce gate. Pitch y modulación siguen al paso seleccionado.
### Skip
En `Forward`, `Backward`, `Pendulum`, `Random` y `Drunk`, el navegador evita
las posiciones Skip sin asignarles tiempo de secuencia.
En `Ping-Pong`, `Odd/Even`, `Jump`, `Converge` y `Diverge`, las posiciones Skip
siguen siendo seleccionadas por el orden programado. Son silenciosas, pero
consumen su posición dentro de ese orden.
El `ADDR` conectado de UZZ-X también selecciona directamente posiciones Skip.
Esas posiciones direccionadas son silenciosas.
### Accum Up
Cuando supera la prueba de probabilidad, el paso suma la cantidad `ACUMM`
actual a su propio offset de pitch almacenado y después se reproduce.
### Accum Down
Cuando supera la prueba de probabilidad, el paso resta la cantidad `ACUMM`
actual a su propio offset de pitch almacenado y después se reproduce.
Cada paso físico tiene un acumulador independiente. La acumulación afecta al
pitch, no a las posiciones de los knobs `PITCH`, `OCT`, `MOD1` o `MOD2`.
### Pulse
Con `PROB` entre `×2` y `×8`, Pulse distribuye ese número de gates de forma
uniforme dentro de un periodo efectivo de secuencia. El secuenciador avanza
después de ese periodo.
### Gated
Con `PROB` entre `×2` y `×8`, Gated mantiene un gate durante ese número de
periodos de clock. El paso actual permanece seleccionado mientras se consumen
esos periodos, con un máximo de `8 s`. Queda un intervalo bajo corto antes del paso siguiente para que
pueda volver a dispararse.
### Hold
Con `PROB` entre `×2` y `×8`, Hold mantiene seleccionado el paso durante ese
número de periodos de clock y vuelve a disparar en cada periodo. La longitud de
gate o trigger sigue dependiendo de la selección global `GATE`/`TRIG`.
En `Pulse`, `Gated` y `Hold`, un valor del lado de probabilidad usa un pulso.
---

## 6. Herramientas de fila
Cada fila tiene un botón de random y una entrada de trigger. `PITCH`, `OCT`,
`DUR`, `MOD1`, `MOD2` y `PROB` también tienen botones de desplazamiento abajo y
arriba.
### Botones y entradas de random
- Un clic en el botón random de una fila aleatoriza los 16 pasos físicos.
- Un trigger en el jack de una fila aleatoriza esa fila por CV.
- Un doble clic en un botón random resetea esa fila en vez de aleatorizarla.
Valores del reset con doble clic:
```text
MODE: Play
PITCH: 0 st
OCT: 0 oct
DUR: 50%
MOD1: 0
MOD2: 0
PROB: 100%
```
La aleatorización de pitch respeta el rango de una o dos octavas seleccionado.
La de octava usa `-2..+2`. La de duración permanece aproximadamente dentro de
`10..90%`. Las líneas mod usan todo el rango `0..10` de sus knobs.
### Desplazamientos de fila
Los botones abajo/arriba cambian todos los valores dentro de la ventana activa:
- `PITCH`: un semitono.
- `OCT`: una octava.
- `DUR`: nominalmente cinco puntos porcentuales, cuantizados sobre una rejilla
  anclada en 1%; por ejemplo, 50% pasa a 56% al subir o a 46% al bajar.
- `MOD1` y `MOD2`: una unidad del knob.
- `PROB`: un ajuste discreto de probabilidad/pulso.
Los valores se detienen en el límite de la fila; los desplazamientos de valor
no hacen wrap. Los pasos fuera de la ventana activa no cambian. Estos botones
cambian valores, a diferencia de `ROT -` y `ROT +` de UZZ-X, que mueven datos
completos de paso entre posiciones.
---

## 7. Modos de dirección
`DIR` selecciona uno de diez modos de recorrido. El valor por defecto es
`Forward`.
### Forward
Avanza desde el principio hacia el final de la ventana activa y después vuelve
al principio. Evita las posiciones Skip.
### Backward
Avanza en sentido inverso por la ventana activa y después vuelve al final.
Evita las posiciones Skip.
### Pendulum
Avanza hasta un extremo, invierte el sentido sin atravesar el límite y vuelve.
Evita las posiciones Skip. `EOC` se dispara en cada giro.
### Random
Elige una posición que no sea Skip dentro de la ventana activa en cada avance.
La navegación Random no genera `EOC`.
### Drunk
Elige un movimiento hacia delante o atrás en cada avance y evita posiciones
Skip. Al cruzar entre los dos extremos genera `EOC`.
### Ping-Pong
Recorre un orden fijo de ida y vuelta con los extremos repetidos.
Sigue posiciones directamente, por lo que selecciona los Skip en silencio.
`EOC` marca la finalización del orden programado completo.
### Odd/Even
Visita las posiciones alternas como un grupo y después las posiciones
intercaladas como otro. Los Skip permanecen en el orden y son silenciosos.
### Jump
Avanza con un salto fijo, seleccionado como `÷2` a `÷7` en el menú contextual.
Los Skip permanecen en el orden y son silenciosos.
Si el salto y la longitud de la ventana activa comparten un divisor común,
Jump solo visita un subconjunto de las posiciones antes de repetir su ciclo.
### Converge
Alterna posiciones desde las zonas exteriores de la ventana hacia el centro.
Los Skip permanecen en el orden y son silenciosos.
### Diverge
Alterna hacia fuera desde el centro hasta los extremos de la ventana.
Los Skip permanecen en el orden y son silenciosos.
---

## 8. Controles de timing
### RATIO
`RATIO` divide o multiplica el clock externo. El valor por defecto es `×1`.
Ajustes disponibles, en orden de índice:
```text
÷48, ÷32, ÷24, ÷16, ÷12, ÷10, ÷8, ÷6, ÷5, ÷4,
÷3, ÷2.5, ÷2, ÷1.5, ×1, ×1.5, ×2, ×2.5, ×3, ×4,
×5, ×6, ×8, ×10, ×12, ×16, ×24, ×32, ×48
```
La división produce menos ticks de secuencia que flancos de clock entrantes.
Los multiplicadores enteros producen más. UZZ debe observar el periodo del clock externo
antes de establecer por completo un timing distinto de uno; deja pasar uno o
dos flancos iniciales al arrancar o cambiar de fuente de clock.

En la implementación actual, los multiplicadores fraccionarios `×1.5` y
`×2.5` resincronizan la fase en cada flanco externo y producen aproximadamente
`×1` y `×2`, respectivamente. Usa multiplicadores enteros cuando necesites una
relación exacta.
### SWING
- Rango: `0%` a `60%`.
- Valor por defecto: `0%`.
- Retrasa ticks efectivos alternos de la secuencia.
- Actúa después del ratio seleccionado, por lo que afecta a los ticks
  multiplicados o divididos.
### SLEW
- Rango: `0` a `2 s`.
- Valor por defecto: `0 s`.
- Suaviza solo la salida `V/OCT` entre objetivos de pitch.
- No retrasa gates, modulación, selección de pasos ni EOC.
### Selector GATE/TRIG
- Valor por defecto: `GATE`.
- `GATE`: la longitud depende del `DUR` del paso, de `1%` a `95%`.
- `TRIG`: cada pulso normal dura aproximadamente `10 ms`.
- Un gate normal tiene un límite máximo de `2 s`.
- Un evento `Gated` de varios periodos tiene un límite máximo de `8 s`.

Estos límites se aplican incluso con un clock estable y evitan gates atascados
con periodos extremos o fuentes detenidas.
---

## 9. Acumulador
Los dos controles `ACUMM` definen el tamaño del incremento y el rango de wrap.
### Cantidad
- Rango: `0` a `24 st`.
- Valor por defecto: `1 st`.
- Cuantizada a semitonos enteros.
- `ACCUM` de UZZ-X aplica un offset antes de limitar el resultado a `0..24 st`.
### Wrap
- Rango: `OFF` y después `1` a `12 st`.
- Valor por defecto: `OFF`.
- Un valor `N` hace wrap simétrico por todos los offsets enteros entre `-N` y
  `+N`.
- `OFF` usa el rango simétrico completo del acumulador, `-12..+12 st`.
Por ejemplo, con wrap en `2 st`, la acumulación ascendente recorre
`-2, -1, 0, +1, +2` en vez de detenerse en un extremo.
El acumulador se borra con `RESET` de UZZ, con el reset de Rack/módulo o con
`RST` de UZZ-X. Una prueba de probabilidad fallida evita tanto el evento de
acumulación como el gate. Pitch y modulación sí se actualizan al paso
seleccionado en ese tick.
---

## 10. Entradas de UZZ
### CLK
Entrada de clock externo. Reconoce los flancos de subida con el comportamiento
normal de trigger de Rack. Sin cable, UZZ no funciona libremente y se detienen
las salidas gate, poly-gate y EOC. Pitch y modulación permanecen disponibles en
el paso seleccionado.
### RESET
Devuelve el recorrido al `START` efectivo, borra todos los acumuladores por
paso, reinicia el timing de swing/recorrido e interrumpe gates mantenidos o
ratchets. El paso inicial queda seleccionado para el siguiente evento.
`EOC` al hacer reset está desactivado por defecto y puede activarse en el menú
contextual.
### XPOSE
- Entrada de transposición a `1 V/oct`.
- Cuantizada a semitonos.
- Rango efectivo: `-48` a `+48 st` (`-4` a `+4 V`).
- Se suma a `PITCH`, `OCT` y al pitch acumulado antes de generar `V/OCT`.
### Entradas trigger de random por fila
Existe una entrada de trigger para cada fila:
```text
MODE, PITCH, OCT, DUR, MOD1, MOD2, PROB
```
Cada trigger ascendente aleatoriza los 16 valores de esa fila,
independientemente de la ventana activa.
---

## 11. Salidas de UZZ
### V/OCT
CV de pitch monofónico a `1 V/oct`. Combina:
```text
Semitonos PITCH + semitonos XPOSE + acumulador por paso
+ octavas enteras OCT
```
`SLEW` se aplica a esta salida.
### GATE
Salida monofónica de gate/trigger de `10V`. Su timing depende del modo de paso,
la probabilidad, `DUR` y el selector global `GATE`/`TRIG`.
### POLY
Salida polifónica de gates de paso de `10V`, con un canal por posición de la
ventana activa. Los canales son relativos a la ventana activa, no están fijados
a los pasos físicos:
```text
Canal 1 = START efectivo
Canal 2 = siguiente posición activa
...
Canal N = posición final de la ventana STEPS efectiva
```
La salida tiene exactamente el número de canales de `STEPS` efectivo. Si la
ventana continúa desde el paso 16 al 1, el mapeo de canales continúa con ella.
El CV `STEPS` y `START` de UZZ-X también cambia el número de canales o el mapeo
a pasos físicos.
### EOC
Pulso de fin de ciclo de `10V` y aproximadamente `10 ms`.
- Forward/Backward: se dispara cuando la navegación cruza el límite.
- Pendulum: se dispara en cada giro.
- Random: no se dispara por navegación.
- Drunk: se dispara al cruzar entre extremos de la ventana.
- Modos de orden programado: se dispara al completar su orden.
- `ADDR` de UZZ-X: se dispara cuando la posición relativa direccionada cae por
  debajo de la posición anterior.
- RESET: se dispara solo si `EOC on reset` está activado.
### MOD1 y MOD2
CV monofónico del paso seleccionado. Cada salida usa el rango elegido de forma
independiente en el menú contextual. El valor por defecto es `0V..10V`.
---

## 12. Colocación y enlace de UZZ-X
Coloca UZZ-X directamente a la izquierda de UZZ, sin ningún módulo entre ellos.
UZZ-X solo se comunica con el UZZ inmediatamente situado a su derecha.
La luz verde de enlace se enciende cuando la colocación es correcta. Si está
apagada, ninguna entrada de UZZ-X afecta a UZZ. UZZ-X no tiene salidas y no
necesita un cable para establecer la conexión de expansor.
Cuando una entrada de offset no está conectada, su contribución es cero. Los
offsets discretos se redondean al índice o paso más próximo y después el valor
combinado se limita al rango válido del destino.
---

## 13. Entradas CV de UZZ-X
### STEPS
- Escala: `1 V` por paso de offset.
- Se suma al knob `STEPS` de UZZ.
- El resultado efectivo se limita a `1..16`.
### START
- Escala: `1 V` por paso físico de offset.
- Se suma al knob `START` de UZZ.
- El resultado efectivo se limita a los pasos `1..16`; el propio offset no
  hace wrap dentro del rango del knob.
La ventana activa resultante sí puede continuar físicamente desde el paso 16
al paso 1.
### DIR
- Escala: `1 V` por índice de modo de dirección.
- Se suma al ajuste `DIR` de UZZ.
- El resultado efectivo se limita a los diez modos desde `Pendulum` hasta
  `Diverge`.
El orden de índices es:
```text
Pendulum, Backward, Forward, Random, Drunk,
Ping-Pong, Odd/Even, Jump, Converge, Diverge
```
### ADDR
- Rango: `0V` a `10V`, limitado en ambos extremos.
- Se mapea sobre la ventana activa efectiva actual.
- `0V` selecciona su primera posición; `10V` selecciona la última.
- Los voltajes intermedios seleccionan la posición más cercana.
Cuando hay un cable conectado, `ADDR` evita el navegador normal de dirección.
No se usan el ajuste de dirección ni el bypass normal de Skip. Por tanto, un
paso Skip seleccionado por `ADDR` es silencioso. `EOC` se dispara cuando el
valor de `ADDR` baja desde una posición relativa superior a otra inferior.
### RATIO
- Escala: `1 V` por entrada de la lista de ratios.
- Se suma al índice `RATIO` de UZZ.
- El índice efectivo se limita a `÷48..×48`.
### SWING
- `+5V` recorre todo el rango de `+60%` desde un knob a cero.
- `-5V` recorre todo el rango en la dirección de offset negativo.
- Se suma de forma continua al knob `SWING` de UZZ.
- El swing efectivo se limita a `0..60%`.
### PROB
- `+10V` suma `100` puntos porcentuales.
- `-10V` resta `100` puntos porcentuales.
- Se suma al control global `PROB` de UZZ.
- La probabilidad global efectiva se limita a `0..100%`.
### ACCUM
- Escala: `1 V` por semitono de offset.
- Se suma a la cantidad del acumulador de UZZ.
- Se redondea a semitonos enteros.
- La cantidad efectiva se limita a `0..24 st`.
---

## 14. Entradas de trigger y gate de UZZ-X
### ROT -
Un trigger ascendente rota todos los datos completos de paso una posición hacia
atrás dentro de la ventana activa efectiva, con wrap.
### ROT +
Un trigger ascendente rota todos los datos completos de paso una posición hacia
delante dentro de la ventana activa efectiva, con wrap.
Ambas entradas de rotación mueven juntos `MODE`, `PITCH`, `OCT`, `DUR`, `MOD1`,
`MOD2`, `PROB` y los offsets almacenados de acumulador por paso. Los pasos fuera
de la ventana activa no cambian.
### RST
Un trigger ascendente borra los 16 offsets de acumulador almacenados. No
reinicia la posición de secuencia ni los valores del panel.
### REV
- Se activa a `1V` o más.
- Mientras está alto, solo intercambia `Forward` y `Backward`.
- No afecta a Pendulum, Random, Drunk, Ping-Pong, Odd/Even, Jump, Converge ni
  Diverge.
- No invierte el mapeo de `ADDR`.
---

## 15. Menús contextuales
### Menú contextual de UZZ
Haz clic derecho en UZZ para acceder a:
- `EOC on reset`: desactivado por defecto; activa un pulso EOC con RESET.
- `Direction mode`: selecciona cualquiera de los diez modos de dirección.
- `Jump stride`: `÷2`, `÷3`, `÷4`, `÷5`, `÷6` o `÷7`; por defecto `÷2`.
- `Pitch range`: `1 octave (0..11)` o `2 octaves (0..23)`; una octava por
  defecto.
- `Range Mod 1`: selecciona el rango de salida de MOD1.
- `Range Mod 2`: selecciona el rango de salida de MOD2.
Al cambiar el rango de pitch, los valores existentes de la fila se reescalan
proporcionalmente al rango nuevo. Los dos menús de rango de modulación ofrecen:
```text
±10V, ±5V, ±3V, ±2V, ±1V,
0V..10V, 0V..5V, 0V..3V, 0V..2V, 0V..1V
```
El valor por defecto de ambas salidas de modulación es `0V..10V`.
### Menú contextual de UZZ-X
UZZ-X no tiene ajustes propios en el menú contextual. Los comandos estándar de
módulo de Rack siguen disponibles.
---

## 16. Persistencia y comportamiento de reset
Al guardar un patch, VCV Rack conserva todos los parámetros del panel y las
conexiones. UZZ también conserva:
- Las selecciones de rango de voltaje de MOD1 y MOD2.
- La selección de rango de pitch.
- El estado de `EOC on reset`.
- El salto de Jump.
- La posición actual y el progreso relevante de los modos de dirección.
- Los 16 offsets de acumulador por paso.
Al recargar el patch, un paso actual guardado fuera de la ventana activa
restaurada se mueve dentro de ella. No se garantiza que las elecciones random
continúen como un flujo aleatorio idéntico después de recargar.
El reset de módulo de Rack devuelve los parámetros del panel a sus valores por
defecto y borra la acumulación y el estado de recorrido. La entrada `RESET` es
un reset de interpretación: vuelve al START efectivo y borra acumuladores, pero
no resetea knobs ni valores de las filas.
UZZ-X no guarda estado musical adicional; su efecto procede de los voltajes de
entrada actuales y de los eventos de trigger.
---

## 17. Patches prácticos
### 17.1 Secuencia melódica básica
```text
Clock -> UZZ CLK
UZZ V/OCT -> V/OCT del oscilador
UZZ GATE -> GATE de la envolvente
Oscilador -> entrada de audio del VCA
Envolvente -> CV del VCA
```
Usa `PITCH`, `OCT` y `DUR` para escribir la frase. Conecta `MOD1` al cutoff del
filtro y usa su rango contextual `0V..5V` para un movimiento moderado.
### 17.2 Ritmo probabilístico con CV estable
Ajusta algunos pasos a `25%`, `50%` o `75%` en el lado izquierdo de `PROB`.
Conecta `MOD1` al timbre y `MOD2` al decay. Las pruebas fallidas crean silencios,
pero pitch y ambas salidas de modulación siguen los pasos seleccionados.
### 17.3 Ratchets y notas sostenidas
Ajusta un paso a `Pulse` y su `PROB` a `×4` para obtener cuatro subpulsos en un
periodo. Ajusta otro a `Gated` y `×3` para un evento sostenido durante tres
periodos. Usa `Hold` y `×3` cuando quieras tres ataques separados por clock
manteniendo el mismo pitch.
### 17.4 Melodía acumulativa
Ajusta algunos pasos a `Accum Up` y uno a `Accum Down`.
Empieza con cantidad `1 st` y wrap `5 st`. Los triggers periódicos a `RST` de
UZZ-X devuelven el movimiento armónico a su estado original sin reiniciar la
posición.
### 17.5 Variación rotatoria con UZZ-X
Usa una ventana activa de 8 pasos y envía un trigger lento a `ROT +` de UZZ-X.
Todas las líneas se mueven juntas y conservan la identidad musical completa de
cada paso. Usa `ROT -` para una frase de respuesta ocasional.
### 17.6 Frase direccionada por CV
Conecta un secuenciador, una fuente aleatoria escalonada o un sample-and-hold a
`ADDR`. Usa `0..10V` para recorrer la ventana activa. Recuerda que se evita el
modo de dirección, los pasos Skip se convierten en posiciones direccionadas
silenciosas y el movimiento descendente de `ADDR` puede producir EOC.
### 17.7 Distribución de percusión por ventana activa
Conecta `POLY` a una utilidad de triggers polifónica o a un separador. Cada
canal representa una posición relativa de la ventana activa. Cambiar START rota
qué programación física aparece en cada canal; cambiar STEPS cambia el número
de canales.
---

## 18. Advertencias importantes y solución de problemas
### La secuencia no se mueve
- UZZ solo funciona con clock externo. Comprueba que `CLK` está conectado y
  recibe pulsos ascendentes.
- Los ratios distintos de `×1` necesitan medir el periodo del clock fuente;
  deja pasar al menos dos flancos después de conectar o reiniciar.
### Un paso Skip parece seguir siendo visitado
- El bypass de Skip se limita a Forward, Backward, Pendulum, Random y Drunk.
- Ping-Pong, Odd/Even, Jump, Converge y Diverge seleccionan los Skip en silencio.
- `ADDR` conectado también selecciona los Skip en silencio.
### Falta EOC o aparece con más frecuencia de la esperada
- La navegación Random no produce EOC intencionadamente.
- Pendulum produce EOC en ambos giros.
- El EOC de Jump describe su ciclo programado, que puede visitar solo un
  subconjunto si el salto y la longitud comparten un divisor común.
- El modo Address produce EOC cuando cae la posición relativa.
### A veces no se produce la acumulación
La prueba de probabilidad controla tanto el gate como el evento Accum Up/Down.
Una prueba fallida no cambia el acumulador de ese paso, aunque las salidas de
pitch y mod sí se actualizan a la posición seleccionada.
### Los canales POLY no coinciden con los pasos físicos
Es el comportamiento esperado. Los canales POLY son relativos a START y STEPS
efectivos. La modulación de cualquiera de ellos desde UZZ-X cambia el mapeo en
tiempo real.
### REV no hace nada
`REV` de UZZ-X solo intercambia Forward y Backward, y solo mientras su entrada
está al menos a `1V`. No es un control general de inversión para todos los
modos de dirección.
### Todos los pasos están en silencio
Comprueba `PROB` global, la probabilidad de cada paso, los modos y la ruta de
gate. Si todos los pasos activos son Skip, la navegación no puede encontrar un
paso reproducible. Restaura la fila MODE con doble clic en su botón random.
---

## 19. Referencia de valores por defecto
```text
MODE por paso: Play
PITCH por paso: 0 st
OCT por paso: 0 oct
DUR por paso: 50%
MOD1 / MOD2 por paso: 0
PROB por paso: 100%
PROB global: 100%
RATIO: ×1
STEPS: 16
START: 1
DIR: Forward
SWING: 0%
Cantidad del acumulador: 1 st
Wrap del acumulador: OFF (wrap completo -12..+12 st)
SLEW: 0 s
Modo de gate: GATE
Rango de pitch: 0..11 st
Rango de MOD1 / MOD2: 0V..10V
Salto de Jump: ÷2
EOC on reset: desactivado
```
