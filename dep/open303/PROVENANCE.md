# Origen de este código

Motor DSP **Open303** de Robin Schmidt (www.rs-met.com) y la comunidad de KVR Audio.

- Repositorio original: https://github.com/RobinSchmidt/Open303
- Fork usado (arreglos de includes / definiciones duplicadas para compiladores modernos):
  https://github.com/maddanio/open303
- Commit vendorizado: `52f8614966cde2c1fd9d76369cf9d07e766262ed`
- Fecha de copia: 2026-08-17
- Contenido: únicamente `Source/DSPCode/` (el wrapper VST original no se usa).

## Licencia

MIT — ver `LICENSE-open303.txt`. Compatible con la GPL-3.0-or-later del plugin ATEK303.
El aviso de copyright de Robin Schmidt debe conservarse en cualquier distribución.

## Política de modificaciones

**No editar estos ficheros salvo que sea imprescindible para compilar.** Si hay que tocarlos,
anotar el cambio aquí abajo, para poder volver a sincronizar con el upstream sin perder nada.

### Cambios aplicados sobre el commit vendorizado

**2026-08-17 — gancho de oscilador externo** (3 puntos, todos marcados con `// ATEK303`):

- `rosic_Open303.h`, sección pública de la clase: se añade la interfaz abstracta
  `Open303::ExternalOscillator` y el puntero `externalOscillator` (NULL por defecto).
- `rosic_Open303.h`, `getSample()`: si `externalOscillator` no es NULL se le pide
  la frecuencia y la muestra en lugar de al `BlendOscillator` de tablas.
- `rosic_Open303.cpp`, `triggerNote()`: el reset de fase va al oscilador externo
  cuando está instalado.

**2026-08-17 — slide en el dominio de tensión** (2 puntos, marcados con `// ATEK303`):

- `rosic_Open303.h`: se añade el flag `slideInPitchDomain`, el setter
  `setSlideInPitchDomain()` (que convierte el estado del limitador al cambiar de
  dominio — sin él, el estado guardado se reinterpreta y el oscilador recibe una
  frecuencia absurda) y el método `setSlideTimeConstant()` (setSlideTime aplica un factor 0,2 que el propio autor deja
  marcado como pendiente de ajustar, y el constructor no lo usa).
- `rosic_Open303.h`, `getSample()` y `rosic_Open303.cpp`, `triggerNote()`: con el flag
  activo el limitador de slew trabaja sobre `log(frecuencia)` en vez de sobre la
  frecuencia, que es lo que hace la red R91·C35 antes del conversor exponencial.

**2026-08-17 — gancho de filtro externo** (5 puntos, marcados con `// ATEK303`):

- `rosic_Open303.h`: interfaz `Open303::ExternalFilter` y puntero `externalFilter`.
- `rosic_Open303.h`, `setResonance()` y `getSample()`: se enrutan cutoff, resonancia y
  proceso al filtro externo cuando está instalado.
- `rosic_Open303.cpp`, `setSampleRate()` y `triggerNote()`: sample rate (ya sobremuestreado)
  y reset.

**2026-08-17 — saturación del OTA** (2 puntos, marcados con `// ATEK303`):

- `rosic_Open303.h`: miembro `otaHeadroom` y un `tanh` aplicado a la señal justo antes
  de multiplicarla por la envolvente de amplitud, o sea en la entrada del OTA BA662A.
  Con `otaHeadroom = 0` no se ejecuta.

**2026-08-18 — sample rate del oscilador externo** (1 punto, marcado con `// ATEK303`):

- `rosic_Open303.cpp`, `setSampleRate()`: se actualiza también el miembro `sampleRate`.
  Upstream configuraba los bloques con el argumento nuevo, pero dejaba ese miembro en
  44,1 kHz. El motor interno no lo usa en su incremento; nuestro gancho externo sí, y por
  eso a 48 kHz corría `192 / 176,4 = 1,0884` veces rápido, unos +146 cents.

Con los punteros a NULL, el flag a false y `otaHeadroom` a 0 el comportamiento es idéntico al upstream, así que el
selector del menú contextual compara motores sin ninguna otra diferencia.
