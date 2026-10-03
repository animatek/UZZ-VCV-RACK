# Changelog

Registro de cambios de los módulos Animatek. Formato basado en
[Keep a Changelog](https://keepachangelog.com/); las versiones corresponden a
`plugin.json`.

**Regla del repo: no se commitea nada sin apuntar el cambio aquí.**

## [Unreleased]

### Added
- **CAP: modos lowpass filter y low-pass gate, y envolvente invertida.** En el menú, `Mode`
  elige sobre qué actúa el control (envolvente × CV de `VCA`): la ganancia (**VCA**, como
  siempre), el corte de un paso bajo SVF de dos polos entre 20 Hz y 20 kHz sin tocar el
  nivel (**Lowpass filter**), o corte y nivel a la vez a través de un modelo de vactrol que
  abre en 2 ms y cierra en 30 ms o más (**Low-pass gate**). `LEVEL` sigue siendo el fader en
  todos los modos. `Ping envelope` invierte la envolvente: en reposo cerrado y el trigger
  abre, que es lo que hace de CAP un LPG tocado con triggers. El medidor se pone ámbar en los
  modos con filtro. El vactrol va solo en LPG: en LPF ablandaba la caída de 2 ms de un duck a
  unos 50 ms. Comprobado contra libRack con una sierra de 220 Hz: el modo VCA da las mismas
  muestras, bit a bit, que `main`; en LPF el nivel se mantiene y el brillo cae de 0,22 a 0,03
  durante el duck; en LPG con ping hay silencio en reposo y la cola se oscurece al decaer.
  Etiquetas `Low-pass gate` y `Filter` en `plugin.json`.
- **UNIT-D: escala y raíz en el menú contextual.** Once escalas (menor, mayor, dórica,
  frigia, lidia, mixolidia, menor armónica, las dos pentatónicas, blues y cromática) y las
  doce raíces. Por defecto C menor, que era la única que había: los patches anteriores no
  cambian de nota. Se guardan con el patch y `Initialize` las devuelve a C menor.
- **UNIT-D: los LEDs de reloj y de gate se ven por fin.** Se calculaban en cada muestra pero
  nunca se habían puesto en el panel. Van entre `CLK` y `RST` y entre `GATE` y `ACC`. Las
  cuatro luces de "actividad", que eran lecturas de depuración sin sitio, se quitan.

### Fixed
- **UNIT-D: el reset ya no se salta el primer paso.** El primer reloj tras un reset (y tras
  cargar el patch) toca el nodo de partida en vez de salir ya hacia el siguiente. Ese paso
  no consume contador de gates, así que desde el segundo paso los gates caen exactamente
  donde caían antes.
- **UNIT-D: modular `DENS` (o `SEED`, `RADIUS`, `TOL`) ya no congela el ritmo.** Cualquier
  cambio de geometría reconstruía el grafo y, de paso, rebobinaba el recorrido y ponía el
  contador de gates a cero; con un LFO en `DENS` eso pasaba cada pocos milisegundos y el
  patrón de gates se quedaba repitiendo el del primer paso. Ahora el grafo cambia bajo el
  walker sin reiniciarlo; solo `RST`, `Initialize` y soltar `LOCK` rebobinan. Comprobado
  instanciando el módulo contra libRack: antes, 32 relojes con un LFO en `DENS` dejaban
  `gateStep` en 0; ahora llega a 31 y el patrón evoluciona.

### Changed
- UNIT-D: la generación del grafo y el paso de reloj estaban escritos dos veces cada uno
  (grafo principal y por voz; paso libre y paso híbrido de `LOCK`). Ahora son una función
  cada uno.

## [2.5.8] - 2026-08-25

### Added
- **ATEK303 SEQ: editor de patrón con piano roll.** El botón `EDIT` de la cabecera cambia
  los controles de generación por un editor, en el mismo hueco del panel: todo lo que hay
  entre los LEDs de paso y la raya azul está hecho de widgets y no horneado en el SVG, así
  que conmutar es enseñar una capa y esconder la otra y **el SVG no se toca**.
  El roll enseña **una octava entera**: los doce semitonos, siete blancas y cinco negras. Las
  filas que la escala no contiene salen hundidas y no aceptan clic —están para que la octava
  se lea como una octava, no para tocarlas—, y clicarlas no hace nada en vez de cambiar la
  escala a Cromática por su cuenta, que reinterpretaría el patrón entero. Una octava y no
  más: el desplazamiento va en las filas `UP` y `DOWN`, que es cómo funciona un 303 de
  verdad y cómo `AcidPatternV4` ya guarda el patrón por dentro. También es lo que hace que
  quepa, porque dos octavas cromáticas dejarían las filas en un milímetro.
  Debajo van `UP`, `DOWN`, `GATE` (silencio/nota/tie), `ACC` y `SLIDE`, con los colores de
  los LEDs de paso para que panel y editor digan lo mismo, y los slides dibujados como una
  línea que une las dos notas. Se pinta arrastrando; los pasos fuera de `STEPS` salen
  apagados y no se editan.
  Por el borde izquierdo baja un **teclado vertical**, una tecla por fila, blanca o negra
  según lo sea esa nota en un piano. La tónica lleva marca y **su octava real**, que es lo que
  quita la ambigüedad de un roll de una sola octava.
  Las celdas que **no admiten valor salen oscuras**: `UP`, `DOWN`, `ACC` y `SLIDE` solo
  significan algo sobre un ataque, y el slide necesita además que el paso siguiente ataque a
  otra altura. Es la misma regla que aplica `sanitize()` al reproducir, dibujada por
  adelantado: antes se pulsaba y no pasaba nada sin saber por qué.
- **ATEK303 SEQ: *Clear pattern* en el menú contextual.** Vacía el patrón para dibujar uno
  desde cero en el editor, sin pasar por GENERATE. Conserva la semilla y no toca `STEPS`, y
  el paso 1 se queda con una nota, que es la invariante que sostiene `sanitize()`. Como es
  destructivo ocupa el hueco del deshacer de las mutaciones, así que *Undo last mutation*
  lo recupera.
- **ATEK303 SEQ: el botón derecho borra en el editor.** El trato de cualquier piano roll:
  el izquierdo pone y el derecho quita, y arrastrando con él se barre una tirada entera.
  En el roll y en `GATE` apaga el paso —da igual la fila en la que caiga el ratón, para
  poder barrer en diagonal—; en `UP` y `DOWN` devuelve la octava a cero, y en `ACC` y
  `SLIDE` apaga el atributo. Sólo se consume el clic si cae sobre una celda de verdad: en
  el tecladito de la izquierda y en los huecos entre filas el menú contextual del módulo
  sigue saliendo como siempre.
- **ATEK303 SEQ: tira de páginas bajo el editor.** Sesenta y cuatro pasos no caben en
  dieciséis columnas, así que el patrón se edita de compás en compás: un botón redondo y
  cuatro LEDs, colgados de las columnas de la rejilla del editor para que caigan a plomo
  con ella. Un clic en un LED enseña esa página —sólo mira, no toca el sonido— y un toque
  en el botón avanza una; mantenerlo dos segundos engancha el seguimiento de la cabeza y
  el editor cambia de página solo, con la barra que llena el botón contando esos dos
  segundos.
  **Doble clic en un LED mueve el arranque de la secuencia a esa página**: con `STEPS` en
  16 y la página `2:4` activa, la secuencia recorre los pasos 17 a 32, y la ventana da la
  vuelta al patrón si `STEPS` se pasa del paso 64. Es estado de sonido, se guarda en el
  patch, y el editor lo respeta: el sombreado de lo que queda fuera del bucle, las líneas
  de slide y las celdas que aceptan clic se cuentan desde la página activa y no desde el
  paso 1.
- **ATEK303 SEQ: el sticker acid es un botón de generar.** La cara de la esquina superior
  derecha hace lo mismo que `GENERATE` —semilla nueva, o mutación completa con `BLOCK`
  encendido—, así que se pueden sortear patrones sin salir del editor a buscar el panel de
  generación. Lleva tooltip, porque un sticker no parece un control, y la zona sensible es
  el círculo y no el cuadro que lo envuelve. El comportamiento de `GENERATE` se extrajo a un
  único método que comparten el botón, la entrada de trigger y el sticker.
  Por dentro, `AcidPatternEdit` mantiene alineadas las dos capas del patrón: `time` va por
  paso pero `pitch` va empaquetado por orden de nota, así que crear o borrar un ataque
  desplaza los eventos posteriores **conservando su acento, su octava y su slide**. Dejarlo
  en manos de `sanitize()` los habría reescrito con valores por defecto. La edición entra al
  motor por el mismo camino diferido que la importación de MIDI, con un contador de versión
  que resincroniza la copia del editor cuando GENERATE o una mutación cambian el patrón por
  debajo. Verificado en banco con 160 000 ediciones aleatorias sobre las ocho escalas sin
  romper ni una vez la invariante `pitchLength == número de NOTE`.
- **ATEK303 SEQ: lector de ficheros MIDI.** *Load MIDI file...* en el menú contextual lee
  un Standard MIDI File y lo convierte en patrón. El mapeo no es una convención inventada:
  es el que usan de hecho los clones de 303 al exportar, y por eso un fichero suelto suena
  igual dentro del módulo que fuera. Un note-on es un ataque; una nota que cruza el límite
  del paso sin que empiece otra es un **tie**, y si la siguiente sí ataca y a otra altura
  es un **slide**; la velocidad por encima del punto medio del fichero es el **acento**.
  El tiempo se cuantiza a semicorcheas, así que un fichero de cuatro compases da cuatro
  patrones de 16 pasos y un submenú **Bar** salta entre ellos sin volver a abrir el
  fichero. La escala del panel se respeta si el fichero cabe en ella y pasa a Cromática si
  no, que es la única forma de importar sin reescribir la melodía; `ROOT` no se toca. El
  módulo informa en el menú de todo lo que tuvo que hacer —escala forzada, transporte por
  octavas, voces descartadas de un acorde, ausencia de acentos—, en vez de hacerlo en
  silencio. El patch guarda el patrón entero, así que suena aunque el fichero MIDI
  desaparezca después. El lector (`src/MidiFile.hpp`) no depende de Rack ni de nada más, y
  cubre los formatos 0, 1 y 2 con running status; rechaza la división SMPTE, que no tiene
  rejilla musical que cuantizar.
- **Cada módulo enlaza a su propio manual.** Los once módulos declaran su `manualUrl` en
  `plugin.json`, apuntando al manual en inglés que tienen en `Manuals/`, así que la opción
  *Manual* del menú contextual y el enlace de la web de la librería abren la documentación
  de ese módulo y no una página general. `Model::getManualUrl()` usa el del módulo si
  existe y solo cae al del plugin si no lo hay, de modo que el `manualUrl` general a
  animatek.net se queda como estaba para lo que venga en el futuro.
- **CAP: entrada `VCA`** ([#7](https://github.com/animatek/UZZ-VCV-RACK/issues/7)). CV de
  ganancia polifónico, unipolar y lineal: 0 V cierra la VCA y 10 V deja pasar el tope
  entero. Multiplica lo que fija `LEVEL` en vez de sustituirlo, así que el fader sigue
  siendo el techo y el CV recorta desde ahí, y sin cable no atenúa nada —un patch
  anterior suena igual—. Es lo que convierte a CAP en una VCA controlada por tensión de
  las de siempre: con una envolvente conectada ahí es una VCA normal, y el ducking por
  trigger queda encima para cuando haga falta. No toca `ENV` ni `EOC`; el medidor sí la
  refleja, porque enseña la ganancia realmente aplicada.

### Changed
- **ATEK303 SEQ: el slide entre dos notas de la misma altura ya vale.** Antes se
  descartaba —en `sanitize()`, en el editor y al reproducir— con el argumento de que no
  hay nada que deslizar. Pero un 303 no solo glissa al hacer slide: toca las dos notas
  *legato*, y entre dos alturas iguales eso se sigue oyendo como una nota sostenida en vez
  de dos ataques. Ahora la única condición es que el paso siguiente ataque. El generador
  no cambia: ya se saltaba las transiciones de altura igual por el corpus (193 medidas, y
  ninguna liga), así que una semilla suena exactamente igual que antes; lo que cambia es
  lo que te deja marcar a mano y lo que conserva una importación de MIDI. Para oír el
  legato hace falta el ATEK303 al lado o la opción *Gate held through slides (legato)* del
  menú; sin eso, un slide de altura igual solo levanta la salida `SLIDE`.
- **ATEK303 SEQ: la importación de MIDI descarta el silencio inicial.** Un fichero que no
  arranca en el tick 0 —una sonata que entra al segundo compás, una pista con cuenta
  atrás— se importaba con la primera página en blanco: eso dice dónde empieza la música,
  no qué contiene. Ahora ese silencio se salta, pero en bloques de dieciséis pasos, porque
  recortar hasta la primera nota movería una anacrusa al tiempo fuerte y dejaría el compás
  entero a contratiempo. Un fichero que empieza unos pasos después del principio de su
  primer compás conserva su anacrusa. El menú lo informa como una decisión más del
  importador (`skipped N leading empty steps`). Medido sobre cinco ficheros clásicos: la
  sonata para cello de Debussy se saltaba 16 pasos y *La Mer* 48; el *Étude 12* y la
  *Arabesque*, que entran dentro del primer compás, no se tocan.
- **ATEK303 SEQ: la cabeza de reproducción se pinta en rojo.** El paso en curso llevaba un
  realce azul sobre el color de la nota; ahora se enciende en rojo a plena luz, tape lo que
  tape, en la fila de pasos y en la tira de páginas. Es el código de las cajas de ritmo
  desde siempre y es lo único de la fila que hay que encontrar sin buscarlo. El blanco
  tenue de la tira marca la página por la que arranca la secuencia, para verlo con el
  reloj parado.
- **ATEK303 SEQ: el generador se calibra con un corpus medido** (algoritmo v4 -> v5). Las
  probabilidades del generador estaban puestas a ojo o estimadas sobre una muestra
  pequeña. Ahora salen de medir 61 ficheros MIDI de patrones acid: 140 compases distintos
  de 16 pasos, 1928 notas. Las tablas viven en `src/AcidCorpus.hpp` y son frecuencias
  agregadas, no material: ningún patrón del corpus se puede reconstruir desde ese fichero.
  Lo que el corpus corrige:
  - **Los saltos grandes se ligan más que los pequeños, no al revés.** El código daba 0.40
    a los saltos de hasta cuarta y 0.16 al resto. Medido: la octava y lo que la supera se
    ligan con 0.39 y son lo que más se liga; una tercera menor, con 0.22. La regla estaba
    invertida.
  - **La quinta estaba sobrevalorada al elegir vocabulario**, justo detrás de la tónica. En
    el corpus pesan más que ella la séptima menor, la sexta menor y la cuarta justa.
  - **El vocabulario real llega a seis o siete clases de nota**, no a cuatro como fijaba el
    tope anterior.
  - **El acento global es del 36%, no del 57%** que estimaba el comentario del código, y su
    sesgo hacia la parte fuerte es mucho más flojo del que había escrito.
  - **Un 7,2% de los pasos ocupados prolonga** en vez de reatacar, no el 10% estimado.
  Los mandos `ACCENT` y `SLIDE` quedan calibrados para que su punto medio caiga sobre la
  tasa del corpus, conservando el recorrido entero: medido en banco sobre 20 000 patrones,
  acento 0.366 y slide 0.307 a mitad de recorrido, y 0.000 y 0.883 en los extremos.
  El esquema del JSON no cambia y **ningún patch existente suena distinto**: el patrón va
  entero en el fichero y no se regenera desde la semilla. Lo que cambia es qué patrón
  produce una semilla nueva, y por eso sube `ALGORITHM_VERSION`.
- **CAP: nueva disposición del panel** ([#7](https://github.com/animatek/UZZ-VCV-RACK/issues/7)).
  `DEPTH` pasa a ser el tercer mando para quedar justo encima de `D-CV`, que se muda a la
  columna izquierda: el jack que modula un mando va debajo del mando. `VCA` se le alinea
  al lado, así que los dos CV se leen en la misma fila, y el fader se acorta de 54 a 41 mm
  para dejarles sitio —sigue siendo el recorrido más largo del panel—. Ninguno de los dos
  jacks lleva etiqueta: una línea sube desde cada uno hasta el control al que modula,
  `D-CV` al mando `DEPTH` y `VCA` al fader, que es el mismo recurso con el que el panel
  une el botón de disparo manual con el jack `TRIG`. Debajo va
  esa misma fila: el botón a la izquierda, sin etiqueta como los dos CV, y el jack
  `TRIG` a la derecha bajo la suya, unidos por la línea. `IN L` / `IN R` cierran el bloque,
  así que **todas las entradas quedan por encima de la línea del panel y todas las salidas
  por debajo**: `ENV` / `EOC` y luego `OUT L` / `OUT R`. La línea del panel no se mueve de
  y = 88, así que el SVG no se toca.

### Fixed
- **UZZ: cppcheck avisaba de tres arrays de disparadores sin inicializar.**
  `dsp::BooleanTrigger` se inicializa solo, pero guarda su estado en una unión y el
  análisis estático no ve que el inicializador por defecto de un miembro de la unión cubre
  el objeto entero: avisaba de `rndBtnTrig`, `shiftUpTrig` y `shiftDownTrig`. Un `= {}` en
  la declaración lo calla sin cambiar nada en ejecución. La librería de VCV pasa análisis
  estático al enviar y abre una issue en el repo si sale sucio, así que `src/` vuelve a
  salir limpio.
- **ATEK303 SEQ: el editor volvía atrás un par de cuadros después de cada edición.** La
  copia de trabajo se resincronizaba con `version != seenVersion`, y `submit()` deja
  `seenVersion` un paso por delante a propósito, porque el patrón no se instala hasta que
  el hilo de audio pasa por ahí. En esos cuadros la desigualdad daba cierto y el editor se
  recargaba con el patrón **viejo**: la nota recién puesta parpadeaba, y si el arrastre
  seguía, la edición siguiente salía de la copia atrasada y se perdía la anterior. Ahora
  la comparación es con signo sobre la resta —sólo se resincroniza si el módulo va por
  delante—, así que arrastrar deprisa ya no pierde pasos ni parpadea.
- **ATEK303 SEQ: el arrastre en el roll no seguía al ratón en vertical.** La fila se fijaba
  en la primera pulsación, así que subir o bajar sin cambiar de paso no movía la nota. Ahora
  la fila se recalcula en cada movimiento: cambiar de paso pinta, cambiar de fila mueve la
  nota a esa altura, y si el puntero se sale del roll por arriba o por abajo se conserva la
  última fila buena para no romper la tirada.
- **ATEK303 SEQ: arrastrar por el editor no pintaba salvo con el patch sin desplazar.** El
  arrastre reconstruía la posición del ratón contra `RackWidget::getMousePos()`, que está
  en coordenadas de la rack, y la restaba de `getAbsoluteOffset()`, que está en las de la
  escena: en cuanto el patch estaba desplazado o con zoom —o sea, casi siempre— la
  posición caía fuera del widget y no se pintaba ni un paso. Ahora el pintado va por
  `onDragHover`, que trae la posición ya en coordenadas del widget, así que se dibuja
  arrastrando de verdad: una pulsación y un barrido a derecha o izquierda dejan la tirada
  de notas puesta.
- **Los blanks se comían la gráfica y dejaban módulos transparentes**
  ([#6](https://github.com/animatek/UZZ-VCV-RACK/issues/6)). El lienzo animado de los
  blanks se dibujaba entero en cada cuadro, sin caché: unas nueve figuras translúcidas
  teseladas y enviadas a la GPU por panel y por cuadro, frente al único blit de textura
  que cuesta cualquier otro módulo. Con la pantalla llena de blanks eso agota el
  presupuesto de cuadro de Rack, y al agotarse Rack deja de renderizar los framebuffers
  sucios: por eso un módulo recién añadido o una ficha del navegador, que todavía no
  tienen textura, se dibujaban como nada. Ahora el lienzo vive en su propio framebuffer y
  se repinta doce veces por segundo en vez de sesenta —las marcas derivan 0.0075 mm por
  cuadro, así que se ve idéntico—, y el resto de los cuadros es un blit como el de
  cualquier módulo. De propina, el reparto de presupuesto de Rack pasa a jugar a favor:
  si un cuadro va justo aplaza el repintado del lienzo, que ya tiene textura válida, en
  vez de dejar sin la suya a un módulo que no tiene ninguna. Gracias a Santi por dar con
  el fallo.
- El reparto de colores de un grupo de blanks se ejecutaba en cada comprobación de la
  cadena y no solo al asentarse, así que el primer blank del grupo la recorría entera
  veinte veces por segundo desde el hilo de audio sin nada que repartir.

### Changed
- La caché de SVG teñidos de los blanks pasa de un `std::map` con clave de cadena a una
  tabla indexada por (variante, color), y se devuelve por referencia. Se consulta una vez
  por panel y por vecino en cada cuadro, y con la clave de cadena cada consulta reservaba
  y liberaba memoria.
- El descarte de marcas fuera del panel usa el semieje real de la marca girada
  (`lado x 0.7072`) en vez del lado entero, así que cada panel deja de dibujar marcas que
  quedaban a más de un panel de distancia.

## [2.5.7]

### Fixed
- **Compilación en la librería de VCV**: el motor Open303 vendorizado se mueve de
  `dep/open303/` a `thirdparty/open303/`. `dep/` es el directorio de salida del SDK para
  las dependencias que un plugin compila, y su `make cleandep` hace `rm -rf dep/` sin
  mirar qué hay dentro. El toolchain de la librería encadena
  `clean && cleandep && dep && dist` por cada plataforma, así que borraba los 42 ficheros
  del motor antes de compilarlos y el build moría con `'rosic_Open303.h' file not found`
  ([#5](https://github.com/animatek/UZZ-VCV-RACK/issues/5)). El `.gitignore` se simplifica:
  ya no necesita la excepción `!/dep/open303/`.

### Changed
- El workflow de binarios encadena `clean`, `cleandep`, `dep` y `dist` en vez de llamar a
  `dist` a secas. Es lo que hace el toolchain de la librería, y es la razón de que este
  fallo pasara el CI propio y muriese en el suyo.

## [2.5.6]

### Documentation and release tooling
- Añadido el post bilingüe de Patreon para presentar la colección 2.5.6.
- Añadida una build de GitHub Actions para generar paquetes Linux x64, Windows x64,
  macOS Intel y macOS Apple Silicon, y adjuntarlos automáticamente a cada release.

### Fixed
- **UNIT DISTANCE**: `chooseNextNodeForVoice()` comprueba el rango entero de la voz antes
  de indexar. En la práctica nunca se desbordaba —todas las llamadas vienen de bucles
  limitados por `polyVoices`, que ya está acotado—, pero cppcheck no podía demostrarlo y
  lo marcaba como `containerOutOfBounds` en el análisis estático de la librería de VCV
  ([#4](https://github.com/animatek/UZZ-VCV-RACK/issues/4)). Se comprueba con una guarda
  y no con `clamp()` porque cppcheck no sigue el valor de retorno de esa función.
- **UZZ**: las dieciséis etiquetas de nota y la capibara ya se dibujan en el navegador de
  módulos y en la web de la librería. Ambos widgets salían con `if (!module) return;`, y
  ahí el panel se renderiza sin instancia, así que se veía a medias. Las notas caen ahora
  en los valores por defecto (`C4`) y el contorno de la capibara no depende del módulo;
  el destello sigue en la capa de luz, que sí lo comprueba.
- **UNIT DISTANCE**: el display del grafo pintaba solo el recuadro sin módulo. Ahora
  muestra un anillo de muestra fijo, para que la miniatura no salga hueca.
- **ONE**: `module` y `ccIndex` de los rótulos dinámicos se declaran inicializados. Se
  asignaban siempre nada más crear el widget, así que no llegaba a leerse basura, pero
  cppcheck lo marcaba y no cuesta nada cerrarlo.
- **CAP**: el miembro `module` de `LevelSlider` pasa a llamarse `sideChain`. Sombreaba el
  `module` que `ParamWidget` ya trae, que es de otro tipo; con dos nombres iguales en la
  misma jerarquía es fácil coger el que no es sin que el compilador diga nada.

## [2.5.5]

### Added
- **ATEK303** y **ATEK303 SEQ** (32 HP entre los dos): emulación del Roland TB-303 y su
  generador de patrones acid, que llegan desde su propio repo
  ([animatek/ATEK303](https://github.com/animatek/ATEK303)).
  - **ATEK303** (12 HP): VCO de rampa modelado del esquema, ladder de diodos con no
    linealidad por célula, envolvente de decay y acento de dos etapas. Secciones FILTER,
    ENVELOPE y VOICE, con entrada de CV y atenuverter en los seis mandos. Dos modelos de
    sonido conmutables (Circuito / Open303) y ajuste fino por bloques en el menú.
  - **ATEK303 SEQ** (20 HP): generador algorítmico de líneas acid de 16 pasos, con seed
    persistente y bloqueable, y mutación por capas de tiempo, alturas y articulación.
    Funciona como expander de ATEK303 si se pega a su izquierda.
  - Motor **Open303** de Robin Schmidt vendorizado en `dep/open303` (MIT, compatible con
    la GPL-3.0-or-later del plugin), con cinco ganchos documentados en su
    `PROVENANCE.md`. El Makefile lo compila con `-Idep/open303`, y `.gitignore` exceptúa
    ese directorio del `/dep/` ignorado, porque es código fuente y no una dependencia
    descargada.
  - `src/ui/AtekWidgets.hpp` añade `SectionLabel` y `GroupBox` sobre `CommonWidgets.hpp`.
  - **La colección no es la fuente de verdad de estos dos módulos.** El análisis, el banco
    de medida y el generador de paneles se quedan en el repo ATEK303, y de allí llega lo
    esencial con `make sync-apply`. No editar aquí `Atek303*.cpp`, `Atek*.hpp`,
    `Acid*.hpp`, `ui/AtekWidgets.hpp`, `dep/open303` ni `res/ATEK303*.svg`: el script
    avisa si lo has hecho, pero el cambio hay que llevarlo al otro repo.

### Changed
- Las descripciones del navegador de módulos son más cortas y accesibles: explican el
  propósito de cada módulo sin enumerar todos sus controles ni detalles internos.
- **La colección pasa a ser dark-only.** Se borran los diez paneles claros
  (`res/*-light.svg`) y los diez módulos cargan un único SVG. Desaparece el submenú
  `Panel` (Light / Dark), que ya no significaría nada.
  - **No basta con borrar los ficheros**: `panelTextColor()`, `panelSeparatorColor()`,
    el arco de los knobs de UZZ y la tapa de junta del blank decidían su color según
    `settings::preferDarkPanels`, que es un ajuste **de Rack entero**, no nuestro. Si se
    dejaran así, alguien con Rack en modo claro por otros plugins vería nuestros paneles
    negros con el texto negro. Ahora esos colores son fijos.

### Added (continuación)
- **BLANK ACID** (slug `BlankAcid`): la variante del blank con caritas acid en vez del
  logo Animatek. La marca fija inferior también usa la carita para distinguirla de
  **BLANK 3**. `Blank3.cpp` sirve a las dos variantes.
- **Lienzo compartido entre blanks contiguos.** Las marcas ya no viven en el panel sino
  en el grupo: su posición se mide desde el borde izquierdo del bloque de blanks
  pegados, así que al juntar varios el lienzo se ensancha y una marca cruza de un panel
  al de al lado sin salto. Cada panel dibuja su rodaja y descarta lo que no le roza. El
  grupo se corta en cuanto hay un módulo que no es blank, y meter uno por la izquierda
  desplaza las marcas con su panel para que no den un brinco.
- **Paleta de la guía de estilo** (artifact "Animatek — Paleta de colores"): cada blank
  del grupo coge el siguiente color, empezando por el azul primario `#2C7FFF`, el
  naranja secundario `#FD9A00` —que es justo su complementario— y el verde de acento
  `#24B979`. Los SVG se tiñen en memoria sobre una copia privada, no sobre la caché de
  `Svg::load()`, que la comparte todo el plugin.
- **Dos marcas por panel** en vez de siete, con más opacidad: con siete se solapaban
  tanto que el conjunto se leía como una mancha en vez de como caras.
- Menú de botón derecho: **Speed** (0,25x a 8x) y **Mark colour** (Auto por posición, o
  forzado). Los dos se guardan en el patch y **se aplican a todo el grupo**: si el lienzo
  es común, sus mandos también lo son. Con "Auto" el grupo sale multicolor; forzando un
  color, se pinta entero de ese.
- **Un grupo se ve como un lienzo y no como una reja.** Rack pinta un borde gris
  alrededor de cada panel; con varios blanks pegados eso dejaba líneas por el medio. Se
  apaga el `PanelBorder` de serie (vive dentro del framebuffer del panel, así que no
  puede reaccionar a los vecinos) y se dibuja uno propio que solo pinta los lados que dan
  al exterior del grupo. Encima queda la hilera del corte entre framebuffers, que se tapa
  con una tira del color del fondo por debajo de las marcas — si fuera por encima
  partiría el lienzo justo donde se intenta disimular.
- **La marca fija de la esquina distingue la variante**: logo Animatek en el BLANK 3 y
  carita en el BLANK ACID.
- **Cada blank se queda con su color.** Al entrar en un grupo coge el primer color libre
  y lo fija; a partir de ahí duplicarlo, copiarlo o moverlo de sitio no se lo cambia. El
  reparto lo hace solo el primero del grupo: cuando cada módulo se asignaba el suyo,
  varios lo hacían a la vez leyendo un estado a medias y acababan todos del mismo color.
  "Auto" en el menú los suelta a todos para volver a repartir.
- **El logo fijo de la esquina también sigue el color del grupo.** Estaba dentro del SVG
  del panel, donde no se puede teñir; ahora lo dibuja `BlankBottomLogo` reproduciendo el
  mismo transform que tenía. En `Blank3.svg` solo quedan el fondo y las dos barras.
- Los blanks en bypass siguen actualizando la geometría del grupo; al unir dos grupos,
  la velocidad del panel izquierdo se aplica a todo el lienzo, y los grupos ya no tienen
  un límite interno incoherente de 64 paneles.
- El generador de UZZ deja de recrear `res/UZZ-light.svg`, y el README refleja el
  inventario, los paneles dark-only y el nuevo orden de jacks de CAP.
- Manuales completos en español e inglés para UZZ/UZZ-X, ONE/MULTI, APC40 CTRL,
  CAP, BLANK 3/BLANK ACID, ATEK303 y ATEK303 SEQ. El README añade un índice común
  y una imagen de la colección completa.

- **CAP** (slug `SideChain`): nuevo módulo (6HP) — **VCA de ducking** disparado por trigger,
  para hacer pumping sin compresor. Knobs RECOVERY (40 ms–1 s, exponencial,
  250 ms por defecto), DEPTH y JITTER.
  - **Camino de audio estéreo**: entradas IN L / IN R y salidas OUT L / OUT R.
    **IN R está normalizado a IN L**, así que con un solo cable el módulo hace
    de ducker mono-a-estéreo. Ganancia unidad en reposo.
  - Salida **ENV** con la envolvente como CV (reposo 10 V, cae y vuelve),
    polifónica según los canales de TRIG, para duckear otras cosas en fase.
  - Por defecto **todos los canales de audio comparten una envolvente**, para
    que un par estéreo duckee simétricamente y la imagen no se bambolee. El
    menú "Per-channel envelopes" da a cada canal su propio generador, para
    duckear varias pistas independientes por un cable polifónico.
  - Entradas TRIG (polifónica) y DEPTH CV (10 V = 100 %, sumada y clampeada).
  - **Slider LEVEL + medidor** ocupando la mitad derecha del panel, al estilo
    del VCA-1: el mango fija el techo del VCA (100 % por defecto, o sea audio
    intacto en reposo) y la barra dibuja la ganancia que se está aplicando de
    verdad, así que el duck se ve caer en cada golpe. Los knobs se desplazan
    a la columna izquierda. La barra se dibuja en la capa de luces de Rack
    con un halo suave, así que sigue encendida al bajar el brillo de sala.
    Su **intensidad sigue a la envolvente** (no a la ganancia), de modo que un
    duck se lee dos veces —la barra se acorta y se atenúa— pero bajar el techo
    con el slider solo la acorta, sin apagarla. El número de barras es
    **dinámico**: una en mono, dos en cuanto están conectadas IN L e IN R
    (sea cual sea la polifonía: dos cables mono siguen siendo dos caminos), y
    una por canal —hasta 16— con `Per-channel envelopes`. Cada barra dibuja la ganancia de su
    canal, deliberadamente no el nivel de audio: eso lo convertiría en un VU de
    salida y el ducking dejaría de leerse, que es lo que el medidor tiene que
    contar. La separación entre barras se estrecha al crecer el número, porque
    con 16 una separación fija se comería más de la mitad del ancho.
  - Menú **"Level attenuates ENV"**, apagado por defecto: con él, el slider
    atenúa también la salida ENV y sirve de atenuador de CV. Apagado, ENV sigue
    siendo la envolvente completa de 0 a 10 V. EOC nunca se atenúa, porque un
    trigger a media altura es un trigger que algunos módulos se pierden.
  - Salida **EOC**: trigger de 1 ms cuando la recuperación termina y la
    envolvente vuelve al reposo. Un retrigger que corte la recuperación no
    dispara nada, para que EOC signifique siempre "el duck se ha soltado del
    todo" y no degenere en una copia de TRIG a tempos rápidos.
  - Orden de jacks de arriba abajo agrupado por lo que es cada cosa, no por el
    flujo: primero lo que el módulo fabrica —TRIG · D-CV y, cerrando la mitad
    de arriba con los knobs y el slider, ENV · EOC—, y bajo la raya del panel
    solo el audio, IN L · IN R y después OUT L · OUT R.
  - **Botón de trigger manual** en la cabecera. El nombre del módulo baja a la
    esquina inferior izquierda junto al logo, como en los demás módulos, y ese
    hueco de arriba es el que ocupa el botón; el slider aprovecha para crecer
    de 40 a 51 mm.
  - **Autoparcheo**: con EOC conectado a TRIG, una pulsación del botón lo deja
    oscilando solo. El ciclo es 2 ms de caída + 12 ms de meseta + recuperación,
    o sea de ~1 Hz a ~18.5 Hz, y con jitter ningún ciclo se repite: un
    generador de funciones que un LFO normal no da.
  - **Humanización**: en cada golpe se sortean tiempo de recuperación (±50 %),
    profundidad (±25 %) y exponente de la curva (±30 %) a JITTER 100 %,
    escalados linealmente por el knob. Con JITTER a 0 es determinista.
  - Los valores vienen de un **random walk correlacionado** (a = 0.7), no de
    ruido blanco: el azar puro suena aleatorio, el correlacionado suena humano.
    El paso usa `√(1−a²)` en vez de `(1−a)`; medido sobre 2 M de muestras, con
    `(1−a)` la desviación se queda en 0.24 y el rango nominal nunca se alcanza
    (±4.6 % efectivo en RECOVERY en vez de ±20 %).
  - **Un generador independiente por canal de polifonía**, para que al duckear
    varias pistas cada una respire distinto.
  - Caída fija de 2 ms y meseta de 12 ms: retriggear a mitad de la recuperación
    no produce clicks ni saltos al alza.
  - Menú: forma de la curva (exponencial `p^2.5` / lineal / logarítmica `p^0.4`),
    congelar jitter para comparar A/B, y reset de la semilla. Los tres se
    persisten en el patch.
- **UZZ-X**: nuevo expander CV para UZZ (6HP, se acopla a la izquierda).
  Offsets bipolares alrededor del knob para STEPS, START, DIR, RATIO, SWING,
  PROB y ACCUM (1V/incremento en los steppeados, ±5V/±10V en los continuos);
  entrada ADDR de direccionamiento absoluto de step (0–10V sobre la ventana
  activa, anula la navegación); triggers **ROT+/ROT−** que rotan la secuencia
  completa (todas las lanes por-paso + acumuladores) una posición dentro de la
  ventana activa, con wrap; trigger RST de reset de acumuladores; gate REV de
  inversión momentánea FWD↔REV. LED de enlace.

### Changed
- **UZZ**: el capibara del panel se dibuja en la capa de luces de Rack, con un
  pase tenue constante y el flash encima. Antes estaba en `draw()` y se apagaba
  junto al panel al bajar el brillo de sala; ahora sigue visible y late.

### Fixed
- `plugin.json` usa únicamente etiquetas oficiales y canónicas del Rack SDK; se elimina
  `Monophonic`, que no existe en el catálogo de tags.
- **UNIT-D**: `polyVoices` y `polyUseVoiceSeeds` (menú contextual) ahora se
  persisten en el patch (`dataToJson`/`dataFromJson`); antes cada recarga
  volvía a 1 voz con seed compartida.
- **UZZ**: la división de clock (RATIO < 1) dejaba de funcionar con clocks más
  lentos de ~60 BPM: el timeout de fase virtual estaba capado a 1 s y mataba
  la fase entre flancos (`ClockProcessor.hpp`). Ahora escala con el período
  medido (×2.5, techo 6 s, alineado con el máximo de período aceptado de 5 s).

### Known issues / Pendiente
- **CAP**: a **audio rate el VCA no modula**. Metiendo un oscilador cuadrado en
  TRIG y subiendo su frecuencia, la salida deja de modularse y se queda en una
  atenuación constante. No es un defecto suelto sino la consecuencia de los
  tiempos fijos de la envolvente: caída de 2 ms + meseta de 12 ms + recuperación
  mínima de 40 ms dan un **ciclo mínimo de 54 ms, o sea un techo de 18.5 Hz**.
  Por encima de eso cada nuevo trigger cae dentro del ataque o la meseta del
  anterior, y como `floorLevel = min(1 - depth, level)` el suelo solo puede
  bajar, el nivel se queda clavado abajo y no vuelve a subir.
  Caminos posibles para la próxima sesión, por orden de menor a mayor cambio:
  (a) escalar ATTACK y HOLD con RECOVERY en vez de dejarlos fijos, de modo que
  con recuperaciones cortas se encojan solos; (b) un modo "fast" de menú que
  reduzca los tres tiempos; (c) asumir que es un ducker y no un VCA de
  modulación, y documentar el techo. La opción (a) es la que menos superficie
  nueva añade, pero hay que comprobar que no reaparecen los clicks que el
  ataque de 2 ms vino a eliminar.
- **CAP**: sin manual de usuario. UNIT-D y Sacromonte tienen el suyo en
  `Manuals/`; el módulo ya tiene tres formas de curva, dos modos de envolvente,
  el atenuador de ENV y el modo LFO por autoparcheo, y el README se queda corto.
- **UNIT-D**: sin `onReset` — "Initialize" no limpia lock loop, historial de
  walk ni posiciones de voz.
- **Apc40Ctrl**: fuerza `midiInput.channel = -1` en cada sample de `process()`;
  anula el selector de canal del menú MIDI (necesario para faders ch 1–8,
  pero debería salir del hot path y ocultarse el selector).
- **OxiCv**: modo Mono sin pila de notas (soltar una nota no recupera la
  anterior aún pulsada).

## [2.5.4] y anteriores

Módulos: UZZ, OXI-CV (ONE) + MULTI, APC40 CTRL, UNIT-D, BLANK 3.
Historial anterior en `git log`.
