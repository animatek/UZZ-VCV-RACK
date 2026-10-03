# Manual de usuario de BUS

**Versión del manual:** 1.0

**Versión del plugin:** Animatek 2.5.9

**Módulo:** BUS para VCV Rack

**Anchura:** 4 HP

---

## 1. Descripción general

**BUS** convierte una fila de módulos **CAP** en un mezclador. Pon varias CAP una al lado de otra con un BUS a su derecha: cada CAP pasa su señal estéreo, después de su fader `LEVEL` y de su panorama, al módulo de su derecha, y el BUS saca la suma. No hace falta ningún cable entre ellos.

El BUS lleva además un **envío y retorno** estéreo, para insertar un efecto sobre todo lo que tiene a su izquierda, un control **WET** para mezclar el retorno con el bus seco, un **PAN** con CV, un fader **LEVEL** que es también el medidor estéreo, y un nombre arriba que se rellena solo con el efecto que tiene conectado.

---

## 2. Cómo funciona la cadena

- La cadena se hace juntando módulos: una CAP pasa a su derecha lo que le llega por la izquierda más su propia señal, siempre que a su derecha haya una CAP o un BUS. Un hueco, aunque sea de un HP, la corta.
- **Una fila no necesita un BUS para sonar.** Sin él, la última CAP de la fila saca la mezcla por su propio `OUT` (ver el manual de CAP, *Last in a row*). Los `OUT L` / `OUT R` de las demás CAP siguen siendo salidas directas, y conectarlos no quita nada de la mezcla. Un BUS al final añade el envío y retorno, el master y el medidor.
- Lo que aporta cada CAP es **post-fader** (después de `LEVEL`, del CV de `VCA`, de la envolvente y del modo) y **sumado entre canales polifónicos**.
- Cada CAP tiene su propio trimmer **PAN** y su CV. Es un control de balance: en el centro los dos lados pasan a ganancia unidad, y girarlo hacia un lado atenúa el otro. Solo afecta a la mezcla, no a las salidas propias de la CAP.
- Una **CAP en bypass** mantiene viva la cadena y aporta su entrada sin procesar, como hacen sus salidas en bypass. Un BUS en bypass deja pasar la cadena tal cual.
- Cada módulo que cruza la señal añade una muestra de retardo (unos 21 µs a 48 kHz), así que las CAP del extremo izquierdo llegan unas muestras más tarde que las cercanas al BUS. No se oye, salvo que la misma fuente alimente varias CAP a la vez.

---

## 3. Controles

### Nombre

El campo de arriba. Vacío, muestra el nombre del módulo conectado a `RETURN L` (o, si ahí no hay nada, el que alimenta `SEND L`), así que un BUS con una reverb se lee como esa reverb; sin nada conectado muestra **MIX** en gris, porque un BUS sin efecto es un master. Haz clic para escribir tu propio nombre, y pulsa Intro o haz clic fuera para dejarlo; lo que escribas manda sobre el nombre detectado y se guarda con el patch. Borra el campo para volver al nombre detectado.

### LED LINK

Encendido cuando hay una CAP u otro BUS pegado al borde izquierdo del BUS, es decir, cuando la cadena llega hasta él: tenue con el enlace hecho, más brillante con el audio que llega, según su nivel. Si está apagado, no llega nada. Cada CAP lleva además LEDs de cadena en sus esquinas de arriba, así que un corte en la fila se ve dónde está.

### WET

Mezcla entre el bus y lo que vuelve por `RETURN`, de 0 a 100 %. Por defecto: **100 %**: lo que vuelve sustituye al bus, que es como funciona una inserción y lo que quiere un efecto con su propio control dry/wet. Bájalo para mezclar un efecto totalmente húmedo, como una reverb al 100 % wet, con la señal seca. Sin nada conectado a `RETURN`, `WET` no hace nada y el bus pasa tal cual.

### PAN

Balance de toda la mezcla, aplicado después de `LEVEL`: ganancia unidad en el centro, y el lado contrario se atenúa al girarlo. Por defecto: centro.

### CV de PAN y su trimmer

El jack bajo `PAN`, con el trimmer de encima fijando cuánto y en qué sentido mueve el CV el panorama: al máximo, **±5 V recorren todo el rango**. El trimmer empieza en 0, así que un CV no hace nada hasta que se gira.

### Fader LEVEL y medidor

El fader de la derecha es el nivel master, de 0 a 100 %, aplicado después del retorno; la marca blanca dice dónde está. Por defecto: **100 %**. Las barras de dentro son el nivel de pico de `MIX`, izquierda y derecha, en una escala de −36 a +6 dB respecto a 5 V, el nivel nominal de audio en Rack. La marca blanca fina señala 5 V; la parte de la barra por encima se vuelve ámbar, que es donde lo que venga después puede empezar a saturar.

---

## 4. Entradas y salidas

### RETURN L / RETURN R

Aquí vuelve la salida del efecto. `RETURN R` está normalizada a `RETURN L`, así que un efecto mono necesita un solo cable. Los cables polifónicos se suman.

### SEND L / SEND R

El bus tal como llega por la izquierda, antes del retorno y antes de `LEVEL`. Conéctalo a la entrada del efecto.

### MIX L / MIX R

El resultado: el bus, mezclado con el retorno según `WET`, por `LEVEL` y colocado por `PAN`. Es también lo que el BUS pasa a su derecha.

---

## 5. Ejemplos de patch

### Un mezclador sencillo

Pon cuatro CAP y un BUS a su derecha. Conecta un sonido a cada CAP y `MIX L` / `MIX R` a la interfaz de audio. Los faders de las CAP son los faders de canal, el `LEVEL` del BUS es el master, y cada CAP sigue pudiendo hacer ducking de su canal con el trigger del bombo.

### Un efecto sobre un grupo

`CAP CAP BUS CAP CAP BUS`: el primer BUS tiene un delay entre `SEND` y `RETURN` y solo afecta a las dos primeras CAP. Su `MIX` sigue hacia la derecha, donde el último BUS suma las otras dos CAP y hace de master.

### Reverb mezclada con la señal seca

Pon la reverb al 100 % wet, conecta `SEND` a su entrada y su salida a `RETURN`, y baja `WET` hasta que la mezcla suene bien.
