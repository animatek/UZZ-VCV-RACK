#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Tablas medidas sobre un corpus de patrones acid.
//
// PROCEDENCIA. Estos números salen de analizar 61 ficheros MIDI de patrones acid
// (140 compases distintos de 16 pasos, 1928 notas). Aquí no vive ni una nota de ese
// material: lo que se guarda son frecuencias agregadas — cada cuánto una clase de nota
// aparece en un compás, cuántas notas llevan acento, con qué probabilidad un salto de
// N semitonos se liga. Son medidas sobre la música, no la música, y el generador las
// usa para calibrar sus propios sorteos. Ningún patrón del corpus se puede reconstruir
// desde este fichero.
//
// Método: las probabilidades con poca muestra se encogen hacia la media global
// (estimador bayesiano con m = 60 observaciones de prior). Sin eso, un salto de cuarta
// justa — 71 casos — entraría con 0.704 y sonaría a regla, no a tendencia.
//
// Lo que el corpus corrige de lo que había:
//   · La séptima menor y la sexta menor pesan más que la quinta a la hora de elegir
//     vocabulario. La quinta estaba sobrevalorada.
//   · El vocabulario real llega a seis o siete clases de nota, no a cuatro.
//   · Los saltos GRANDES se ligan más que los pequeños, no al revés.
//   · El acento no cae preferentemente en la parte fuerte: el sesgo métrico es débil.
// ---------------------------------------------------------------------------

namespace AcidCorpus {

// Tamaño del corpus, para poder citarlo sin números sueltos por el código.
static const int BARS = 140;
static const int NOTES = 1928;

// Cada cuánto una clase de nota aparece en un compás, relativa a la tónica y normalizada
// al tritono, la más rara. Se consulta al elegir el vocabulario, donde la tónica ya entra
// siempre, así que su 5.38 queda de referencia y no se usa.
static const float PITCH_CLASS_WEIGHT[12] = {
	5.38f,  // 0   tónica: en los 140 compases, sin excepción
	1.38f,  // 1   segunda menor, el color frigio
	1.23f,  // 2   segunda mayor
	1.73f,  // 3   tercera menor
	1.12f,  // 4   tercera mayor
	1.81f,  // 5   cuarta justa
	1.00f,  // 6   tritono, la más rara
	1.46f,  // 7   quinta justa
	1.73f,  // 8   sexta menor
	1.19f,  // 9   sexta mayor
	2.19f,  // 10  séptima menor, la segunda más frecuente
	1.08f,  // 11  séptima mayor
};

// Reparto acumulado del número de clases de nota distintas por compás. La mediana está
// en cuatro, y una de cada diez líneas usa siete: el vocabulario acid es más ancho de lo
// que sugiere el tópico de la nota machacada.
static const int VOCAB_MAX = 7;
static const float VOCAB_SIZE_CDF[VOCAB_MAX] = {
	0.143f, 0.264f, 0.457f, 0.636f, 0.772f, 0.851f, 1.000f
};

// Elige un tamaño de vocabulario con el reparto del corpus, acotado a la escala.
inline int vocabSize(float u, int scaleSize) {
	int n = VOCAB_MAX;
	for (int i = 0; i < VOCAB_MAX; i++) {
		if (u < VOCAB_SIZE_CDF[i]) { n = i + 1; break; }
	}
	return n < scaleSize ? n : scaleSize;
}

// Probabilidad de que un ataque se ligue al siguiente, por tamaño del salto en semitonos.
// El índice 12 recoge la octava y todo lo que la supera.
//
// La entrada de salto 0 es 0.074 y viene solo del encogido: en 193 transiciones de la
// misma altura el corpus no liga NUNCA. Confirma desde fuera la regla que el generador ya
// tenía — repetir la nota es un ataque nuevo o un tie, jamás un slide — así que el código
// sigue descartando el salto 0 antes de mirar esta tabla.
static const float SLIDE_BY_LEAP[13] = {
	0.074f,  //  0  (no se usa: sin movimiento no hay slide)
	0.274f,  //  1
	0.348f,  //  2
	0.217f,  //  3
	0.525f,  //  4
	0.307f,  //  5
	0.406f,  //  6
	0.241f,  //  7
	0.307f,  //  8
	0.226f,  //  9
	0.280f,  // 10
	0.320f,  // 11
	0.391f,  // 12 y más: la octava es el salto que más se liga
};

inline float slideBase(int leap) {
	if (leap < 0) leap = -leap;
	return SLIDE_BY_LEAP[leap > 12 ? 12 : leap];
}

// Proporción global de slides sobre transiciones con movimiento. Es el punto en el que
// debe caer el mando de SLIDE a la mitad de su recorrido.
static const float SLIDE_RATE = 0.313f;

// Proporción de notas acentuadas. El reparto por posición métrica va de 0.21 a 0.47 con
// unas 120 muestras por posición, así que la parte fuerte solo destaca dentro del ruido:
// el sesgo por posición se conserva, pero mucho más suave de lo que estaba.
static const float ACCENT_RATE = 0.363f;
static const float ACCENT_CELL_START = 1.05f;   // multiplicador en el primer paso de célula
static const float ACCENT_CELL_REST = 0.95f;    // en el resto

// Proporción de pasos ocupados que prolongan la nota anterior en vez de reatacarla.
static const float TIE_RATE = 0.072f;

// Densidad media de pasos con nota. La probabilidad por posición métrica va de 0.79 a
// 0.94, plana dentro del ruido: estos patrones son semicorcheas continuas, así que el
// corpus no aporta pesos métricos y el generador conserva los suyos.
static const float DENSITY_RATE = 0.85f;

}  // namespace AcidCorpus
