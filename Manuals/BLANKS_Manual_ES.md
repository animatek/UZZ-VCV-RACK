# Manual de usuario de BLANK 3 y BLANK ACID

**Versión del manual:** 1.0

**Versión del plugin:** Animatek 2.5.5

**Módulos:** BLANK 3 y BLANK ACID para VCV Rack

**Ancho:** 3 HP cada uno

---

## 1. Descripción general

**BLANK 3** y **BLANK ACID** son paneles ciegos animados de 3 HP sin controles ni puertos. Sirven para ocupar espacio, separar áreas funcionales y crear composiciones visuales en evolución dentro de un patch.

Los módulos BLANK 3 y BLANK ACID adyacentes se reconocen como un único grupo contiguo. Las juntas internas desaparecen y los paneles se convierten en secciones de un lienzo compartido. Cada panel aporta dos marcas que derivan lentamente y pueden cruzar los límites entre paneles. BLANK 3 aporta logotipos de Animatek; BLANK ACID aporta caritas acid. Los grupos mixtos conservan ambas formas, por lo que cada variante sigue siendo identificable.

La paleta disponible para las marcas es **Blue, Orange, Green, Purple, Cyan y Rose**.

---

## 2. Inicio rápido

1. Añade cualquiera de los dos paneles para crear un separador visual estrecho.
2. Coloca más módulos BLANK 3 o BLANK ACID justo a su lado, sin ningún otro módulo entre ellos.
3. Mezcla las dos variantes para combinar logotipos y caritas en un mismo lienzo.
4. Haz clic derecho sobre cualquier miembro para ajustar la velocidad de animación y los colores del grupo.
5. Usa **Pause animation** cuando quieras observar o presentar una composición inmóvil.

No hacen falta cables porque ninguno de los dos módulos tiene entradas ni salidas.

---

## 3. Lienzo compartido y agrupación

Un grupo es una secuencia horizontal ininterrumpida formada exclusivamente por módulos BLANK 3 y BLANK ACID. El orden de las variantes no importa.

- Las variantes adyacentes se unen automáticamente.
- Cada panel aporta exactamente dos marcas animadas.
- Las marcas conservan la forma del panel que las aportó.
- Las marcas se mueven por todo el ancho del grupo y pueden cruzar las juntas internas.
- Las juntas entre paneles se ocultan visualmente; solo permanece el borde exterior del grupo.
- Añadir un blank a la izquierda amplía el espacio de coordenadas y mantiene las marcas existentes visualmente cerca de sus posiciones anteriores.
- Un módulo que no sea blank divide la secuencia en grupos separados.
- Alejar un blank divide su grupo anterior; colocarlo junto a otro blank lo une al grupo nuevo.

La detección de grupos se actualiza automáticamente, aunque puede tardar un breve instante después de mover módulos.

---

## 4. Variantes

### BLANK 3

Aporta dos marcas móviles con el **logotipo de Animatek** y muestra un logotipo fijo correspondiente cerca de la parte inferior. Úsalo para obtener una textura geométrica y orientada a la marca.

### BLANK ACID

Aporta dos marcas móviles con **caritas acid** y muestra una carita fija correspondiente cerca de la parte inferior. La opacidad de sus marcas animadas está compensada visualmente para equilibrar la forma más densa de la carita.

### Grupos mixtos

Ambas variantes usan las mismas reglas de agrupación, velocidad, paleta y lienzo. En una secuencia mixta, cada panel conserva la forma de sus marcas mientras todas ellas son visibles en todo el grupo. El símbolo fijo inferior identifica la variante incluso cuando sus marcas móviles han derivado a otro lugar.

---

## 5. Controles y puertos

No hay controles de panel, parámetros automatizables, entradas ni salidas. Todos los ajustes están en el menú contextual.

Los módulos no procesan audio ni CV. Su finalidad es el espaciado y la animación visual.

---

## 6. Menú contextual

Haz clic derecho sobre cualquiera de las variantes para abrir su menú.

### Pause animation

Pausa o reanuda la animación de **todos los módulos BLANK 3 y BLANK ACID de Rack**, no solo la del panel o grupo seleccionado. Es un interruptor global de la sesión.

La pausa no se guarda en los patches. Una sesión nueva de Rack comienza con la animación activa.

### Speed

Selecciona el multiplicador de deriva:

- **0.25x**
- **0.5x**
- **1x** (por defecto)
- **2x**
- **4x**
- **8x**

La selección se aplica a todo el grupo contiguo. Cada panel guarda su velocidad actual, por lo que el ajuste del grupo sobrevive al guardado/carga del patch y a la duplicación.

Cuando se unen dos grupos con velocidades guardadas distintas, **manda la velocidad del grupo situado a la izquierda** y se propaga por todo el lienzo combinado cuando se estabiliza la detección del grupo.

### Mark colour

La selección también se aplica a todo el grupo.

- **Auto (pick a free one):** libera los colores del grupo y redistribuye los colores disponibles de la paleta. Los paneles eligen colores libres cuando es posible; los grupos de más de seis paneles reutilizan la paleta.
- **Blue**
- **Orange**
- **Green**
- **Purple**
- **Cyan**
- **Rose**

Elegir un color concreto asigna ese mismo color a todos los miembros del grupo actual. Auto permite que el grupo reparta los colores entre sus miembros. Después, cada panel guarda su color asignado, por lo que al moverlo o duplicarlo conserva ese color hasta que se seleccione Auto u otro color concreto.

---

## 7. Persistencia

Se guarda por panel en el patch de VCV Rack:

- velocidad de animación,
- color asignado o seleccionado para las marcas.

No se guarda:

- posiciones, velocidades, rotaciones, tamaños y opacidades actuales de las marcas,
- estado global de Pause animation.

Por tanto, al volver a abrir un patch se conservan la velocidad y la distribución de la paleta, pero se genera una composición móvil nueva. Las marcas no continúan exactamente en los lugares donde estaban al guardar.

Cuando se unen paneles guardados, siguen aplicándose las reglas normales de grupo. En particular, el grupo izquierdo resuelve los conflictos de velocidad al combinar grupos.

---

## 8. Comportamiento del bypass

Poner un blank en bypass detiene el movimiento de **las dos marcas propias de ese miembro**. No elimina el módulo del lienzo compartido ni interrumpe la detección del grupo.

Los demás miembros que no estén en bypass continúan moviendo sus propias marcas, que todavía pueden cruzar y dibujarse sobre el panel en bypass. Las marcas congeladas del panel en bypass también siguen formando parte de la ilustración compartida. Por eso, el bypass congela el movimiento de un miembro, pero no oculta el panel ni pausa el grupo.

Usa **Pause animation** cuando quieras detener todo el rack a la vez.

---

## 9. Disposiciones visuales prácticas

### Separador mínimo

Coloca un BLANK 3 entre un secuenciador y una voz para crear una separación limpia de 3 HP con un movimiento discreto del logotipo.

### Divisor acid

Coloca juntos dos o tres BLANK ACID entre secciones del rack. Usa Orange o Rose y velocidad 0.5x para crear un divisor lento y visible.

### Franja panorámica mixta

Alterna BLANK 3 y BLANK ACID a lo largo de cuatro a ocho paneles. Selecciona Auto para distribuir la paleta por el grupo. Las marcas de todos los paneles recorren la franja completa y mezclan ambas formas.

### Lienzo monocromo

Une varias variantes, elige un único color en **Mark colour** y ajusta la velocidad a 0.25x. El color común unifica las siluetas diferentes de logotipos y caritas.

### Separación deliberada de grupos

Crea un grupo de colores fríos, coloca después cualquier módulo funcional que no sea blank y construye a continuación un grupo de colores cálidos. El módulo funcional impide que se unan los lienzos, velocidades y asignaciones automáticas de color.

### Marcas congeladas selectivamente

Pon un miembro de un grupo móvil en bypass. Sus dos marcas quedan quietas mientras las de los paneles vecinos siguen recorriendo el mismo lienzo, creando una composición por capas fijas y móviles.

---

## 10. Limitaciones y notas prácticas

- La agrupación depende estrictamente de la adyacencia inmediata a izquierda y derecha. Un espacio vacío del rack o cualquier módulo que no sea blank separa los grupos.
- Ambas variantes cuentan como blanks para la agrupación; mezclar sus formas nunca divide el lienzo.
- Los cambios del menú de grupo afectan a todos los miembros del grupo actual. Separa primero los paneles si necesitan velocidades o colores independientes.
- Elegir un color concreto vuelve monocromo todo el grupo actual. Selecciona Auto para redistribuir los colores de la paleta.
- La asignación Auto se realiza cuando la cadena está estable, por lo que los colores o la velocidad combinada pueden tardar un breve instante en asentarse después de arrastrar módulos.
- Pause es global y temporal. No confíes en que se restaure con el patch.
- El bypass solo congela las marcas del miembro en bypass; no las oculta ni aísla el panel.
- Las posiciones de las marcas son deliberadamente efímeras. Usa Pause para obtener una imagen inmóvil temporal, pero espera una composición diferente después de recargar.
