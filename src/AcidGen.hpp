#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

// ---------------------------------------------------------------------------
// Generador de patrones acid. Sin dependencias de Rack a propósito: así se puede
// probar en el banco offline (`tools/pattern.cpp`) igual que el resto del proyecto,
// y el RNG propio deja la puerta abierta a la semilla guardable.
//
// Lo que hace que una línea acid suene a línea acid, y cómo lo genera esto:
//
//  1. **Dos capas: grado y octava.** El teclado del 303 es de una octava (C a C) y los
//     botones Up/Down suben o bajan *notas sueltas* una octava. Así que la altura no es
//     un número suelto: es un grado dentro de una octava más un desplazamiento de octava
//     por paso, acotado a -2..+2.
//  2. **Vocabulario y ámbito son ejes distintos.** El patrón elige de uno a cuatro grados,
//     casi siempre dos o tres, sin mirar RANGO. RANGO solo reparte esas pocas clases de
//     nota por octavas. Una línea puede usar únicamente C2 y C3 y tener a la vez un
//     vocabulario mínimo y un ámbito de una octava.
//  3. **Células con contorno, no pasos sueltos.** Sortear cada nota por separado suena a
//     azar cuantizado, que era el fallo de la primera versión. Se elige primero un
//     *gesto* — pedal de tónica, escalera, zigzag — y la célula se rellena siguiéndolo.
//  4. **La altura se repite; lo que cambia es el acento y la octava.** Es la marca del
//     303: la misma nota machacada, viva por los acentos y los saltos de octava.
//  5. **La célula se desplaza dentro de su vocabulario al repetirse.** Así aparece una
//     frase sin introducir grados nuevos ni volver a acoplar vocabulario y ámbito.
//  6. **Células de 3 sobre patrones de 16.** No divide, así que el gesto se va
//     desplazando contra el compás. Variación sin tocar una sola nota.
//  7. **Tie y slide son distintos.** Un tie prolonga la misma nota sin otro ataque; un
//     slide enlaza dos alturas diferentes. Ambos se deciden cuando la frase ya existe.
//  8. **Slides al final**, solo entre dos ataques de distinta altura y con probabilidad
//     según el salto.
//  9. **Acentos con criterio rítmico**, con preferencia por el primer paso de cada
//     célula, no repartidos al azar.
//
// El patrón se guarda en grados de escala, no en semitonos, así ESCALA y RAÍZ se aplican
// a la salida y se pueden cambiar sin perder el patrón.
// ---------------------------------------------------------------------------

static const int ACID_MAX_STEPS = 16;

// Escalas sin la octava: el grado que la cierra es el 0 de la octava siguiente, y así
// el mismo índice de grado sirve para subir y bajar sin casos especiales.
struct AcidScale {
	const char* name;
	const char* shortName;   // para el panel, donde no cabe el largo
	int n;
	int8_t s[12];
};
static const AcidScale ACID_SCALES[] = {
	{"Acid (pent. menor)", "ACID",  5,  {0, 3, 5, 7, 10}},
	{"Menor natural",      "MEN",   7,  {0, 2, 3, 5, 7, 8, 10}},
	{"Frigia",             "FRIG",  7,  {0, 1, 3, 5, 7, 8, 10}},
	{"Menor armónica",     "ARM",   7,  {0, 2, 3, 5, 7, 8, 11}},
	{"Dórica",             "DOR",   7,  {0, 2, 3, 5, 7, 9, 10}},
	{"Blues",              "BLUE",  6,  {0, 3, 5, 6, 7, 10}},
	{"Mayor",              "MAY",   7,  {0, 2, 4, 5, 7, 9, 11}},
	{"Cromática",          "CROM", 12,  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
};
static const int ACID_SCALES_LEN = 8;

static const char* ACID_NOTE_NAMES[12] =
	{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Los mandos de carácter, todos 0..1 salvo los que se dicen.
struct AcidGenParams {
	int steps = 16;        // 1..16
	int scale = 0;         // índice en ACID_SCALES
	float density = 0.6f;  // cuántos pasos suenan
	float accent = 0.55f;  // cuántos llevan acento
	float slide = 0.45f;   // cuántos ligan
	float range = 0.5f;    // reparto por octavas y ámbito final
	float tie = 0.4f;      // fuerza interna de notas prolongadas (sin mando propio todavía)
};

struct AcidGen {
	// El patrón vive en grados de escala más una octava por paso: la escala y la raíz se
	// aplican al salir.
	int8_t deg[ACID_MAX_STEPS] = {};
	int8_t oct[ACID_MAX_STEPS] = {};
	bool gate[ACID_MAX_STEPS] = {};
	bool accent[ACID_MAX_STEPS] = {};
	bool slide[ACID_MAX_STEPS] = {};
	// tie[s] significa que el paso s continúa la nota del paso anterior: ocupa tiempo,
	// pero no dispara otra nota ni cambia la altura.
	bool tie[ACID_MAX_STEPS] = {};

	uint32_t rngState = 0x1234567u;

	void seed(uint32_t s) { rngState = s ? s : 1u; }

	// xorshift32: barato, reproducible y sin estado global, que es lo que hace falta
	// para que un patrón bueno se pueda recuperar por su semilla.
	float uniform() {
		rngState ^= rngState << 13;
		rngState ^= rngState >> 17;
		rngState ^= rngState << 5;
		return (rngState >> 8) * (1.f / 16777216.f);
	}

	// Grado (0..n-1) y octava → semitonos.
	static int semiOf(int d, int o, int scaleIdx) {
		const AcidScale& sc = ACID_SCALES[scaleIdx];
		const int i = (d % sc.n + sc.n) % sc.n;
		return sc.s[i] + 12 * (o + (d - i) / sc.n);
	}

	// El peso va por semitono, no por índice, para que valga en cualquier escala:
	// la tónica y la quinta anclan, la tercera menor y la séptima menor son el color.
	static float semiWeight(int semi) {
		switch (((semi % 12) + 12) % 12) {
			case 0:  return 2.4f;
			case 7:  return 2.2f;
			case 3:  return 1.7f;
			case 10: return 1.6f;
			case 5:  return 1.3f;
			default: return 1.0f;
		}
	}

	// Contorno de la célula: el gesto que sigue, elegido antes que las notas.
	enum Contour { C_PEDAL, C_RISE, C_FALL, C_ZIGZAG, C_SCATTER, CONTOUR_LEN };
	static const char* contourName(int c) {
		static const char* N[CONTOUR_LEN] = {"pedal", "escalera", "caída", "zigzag", "disperso"};
		return N[c];
	}

	Contour pickContour() {
		static const float W[CONTOUR_LEN] = {0.22f, 0.18f, 0.15f, 0.26f, 0.19f};
		float r = uniform();
		for (int i = 0; i < CONTOUR_LEN; i++) {
			r -= W[i];
			if (r <= 0.f) return (Contour) i;
		}
		return C_PEDAL;
	}

	// Vocabulario del patrón, independiente del ámbito: uno a cuatro grados, favoreciendo
	// dos o tres. El caso de un solo grado es deliberado: C2/C3 ya puede formar una línea
	// con movimiento sin introducir otra clase de nota.
	int buildVocab(int (&voc)[12], int scaleIdx) {
		const AcidScale& sc = ACID_SCALES[scaleIdx];
		const float amount = uniform();
		const int chosen = amount < 0.10f ? 1 : amount < 0.55f ? 2
		                 : amount < 0.90f ? 3 : 4;
		const int want = std::min(sc.n, chosen);
		bool taken[12] = {};
		voc[0] = 0;
		taken[0] = true;
		int nv = 1;
		while (nv < want && nv < 12) {
			float total = 0.f;
			for (int d = 1; d < sc.n; d++)
				if (!taken[d]) total += semiWeight(sc.s[d]);
			if (total <= 0.f) break;
			float r = uniform() * total;
			for (int d = 1; d < sc.n; d++) {
				if (taken[d]) continue;
				r -= semiWeight(sc.s[d]);
				if (r <= 0.f) { taken[d] = true; voc[nv++] = d; break; }
			}
		}
		std::sort(voc, voc + nv);
		return nv;
	}

	// Pliega el patrón por octavas hasta que quepa en el ámbito pedido. Es lo que hace un
	// músico cuando una frase se le va del registro: baja la nota que se ha ido, una
	// octava, y no toca el resto. Se mueven a la vez todos los pasos que están en esa
	// altura, para no romper la coherencia del riff.
	void foldRange(int len, int si, int limitSemi) {
		for (int pass = 0; pass < 6; pass++) {
			int lo = 999, hi = -999, loS = -1, hiS = -1;
			for (int s = 0; s < len; s++) {
				if (!gate[s]) continue;
				const int v = semiOf(deg[s], oct[s], si);
				if (v < lo) { lo = v; loS = s; }
				if (v > hi) { hi = v; hiS = s; }
			}
			if (loS < 0 || hi - lo <= limitSemi)
				return;
			// Se mueve el extremo que más sobra, y solo si el movimiento no lo lleva
			// al otro lado del patrón.
			const int mid = (hi + lo) / 2;
			const bool moveHigh = (hi - mid) >= (mid - lo);
			const int from = moveHigh ? hi : lo;
			const int delta = moveHigh ? -1 : +1;
			bool moved = false;
			for (int s = 0; s < len; s++) {
				if (!gate[s] || semiOf(deg[s], oct[s], si) != from) continue;
				const int no = oct[s] + delta;
				if (no < -2 || no > 2) continue;
				oct[s] = (int8_t) no;
				moved = true;
			}
			if (!moved)
				return;
			(void) hiS;
		}
	}

	// Devuelve el contorno elegido, que es lo que el banco de pruebas quiere ver.
	Contour generate(const AcidGenParams& p) {
		const int len = std::max(1, std::min(p.steps, ACID_MAX_STEPS));
		const int si = std::max(0, std::min(p.scale, ACID_SCALES_LEN - 1));
		const float range = std::max(0.f, std::min(1.f, p.range));

		int voc[12];
		const int nv = buildVocab(voc, si);

		// Célula de 3 sobre patrón de 16: no divide, y el gesto se va desplazando contra
		// el compás. Variación gratis, sin tocar ninguna nota.
		const int CELL = (uniform() < 0.25f) ? 3 : 4;
		// La célula guarda índices del vocabulario, no grados crudos. Al desplazar esos
		// índices en las repeticiones nunca aparecen clases de nota que no estén en `voc`.
		int cVoc[4] = {};

		const Contour contour = pickContour();
		switch (contour) {
			case C_PEDAL:
				// Pedal de tónica con notas que asoman: el gesto acid por excelencia.
				for (int i = 0; i < CELL; i++)
					cVoc[i] = (i == 0 || uniform() < 0.45f) ? 0 : (int) (uniform() * nv) % nv;
				break;
			case C_RISE:
			case C_FALL: {
				for (int i = 0; i < CELL; i++) cVoc[i] = (int) (uniform() * nv) % nv;
				for (int a = 0; a < CELL; a++)
					for (int b = a + 1; b < CELL; b++)
						if ((contour == C_RISE) ? (voc[cVoc[b]] < voc[cVoc[a]])
						                              : (voc[cVoc[b]] > voc[cVoc[a]]))
							std::swap(cVoc[a], cVoc[b]);
				if (uniform() < 0.6f) cVoc[0] = 0;
				break;
			}
			case C_ZIGZAG: {
				// Alterna un ancla grave con una nota alta: el vaivén de las líneas de
				// Hardfloor, y lo que más despega un patrón de la línea recta.
				const int top = nv > 1 ? 1 + (int) (uniform() * (nv - 1)) % (nv - 1) : 0;
				for (int i = 0; i < CELL; i++)
					cVoc[i] = (i % 2 == 0) ? 0 : top;
				break;
			}
			default:
				// Disperso, pero pegajoso: repetir la altura anterior es la marca del 303.
				cVoc[0] = (int) (uniform() * nv) % nv;
				for (int i = 1; i < CELL; i++)
					cVoc[i] = (uniform() < 0.40f) ? cVoc[i - 1] : (int) (uniform() * nv) % nv;
				break;
		}
		// El primer paso de la célula pesa más: es donde cae el golpe.
		const float pGate0 = 0.30f + 0.70f * p.density, pGateN = 0.05f + 0.80f * p.density;
		// El corpus pequeño disponible da un 57 % global de acentos (con una dispersión
		// enorme: 27..90 %). La curva llega alto sin inventar acentos cuando el mando está a 0.
		const float pAcc0 = std::min(0.95f, 1.25f * p.accent);
		const float pAccN = std::min(0.85f, 0.85f * p.accent);
		bool cGate[4], cAcc[4];
		for (int i = 0; i < CELL; i++) {
			cGate[i] = uniform() < (i == 0 ? pGate0 : pGateN);
			cAcc[i] = uniform() < (i == 0 ? pAcc0 : pAccN);
		}

		// Desplazamiento por repetición dentro del vocabulario. La probabilidad ya no
		// depende de RANGO: ese mando queda reservado al ámbito en octavas.
		int vocShift[ACID_MAX_STEPS] = {};
		for (int rep = 1; rep * CELL < ACID_MAX_STEPS; rep++) {
			if (nv > 1 && uniform() < 0.45f) {
				static const int SHIFT[6] = {1, -1, 1, -1, 2, -2};
				vocShift[rep] = SHIFT[(int) (uniform() * 6.f) % 6];
			}
		}

		for (int s = 0; s < len; s++) {
			const int i = s % CELL;
			const int rep = s / CELL;
			int vi = (cVoc[i] + vocShift[rep]) % nv;
			if (vi < 0) vi += nv;
			int d = voc[vi], o = 0;
			gate[s] = cGate[i];
			accent[s] = cAcc[i];

			// Mutación por repetición: se vuelve a sortear, no se invierte. Invertir
			// empujaba todo hacia la media y dejaba acentos y silencios con los mandos
			// a cero.
			if (rep > 0) {
				if (uniform() < 0.18f) d = voc[(int) (uniform() * nv) % nv];
				if (uniform() < 0.15f) gate[s] = uniform() < (i == 0 ? pGate0 : pGateN);
				if (uniform() < 0.15f) accent[s] = uniform() < (i == 0 ? pAcc0 : pAccN);
			}
			// Salto de octava suelto, con querencia por los acentos: es donde el 303 los
			// luce, y es exactamente lo que hacen los botones Up/Down del teclado. RANGO
			// controla únicamente esta dispersión y el límite de ámbito final.
			const float jumpChance = range * (accent[s] ? 0.55f : 0.38f);
			if (uniform() < jumpChance) {
				const float twoChance = std::max(0.f, std::min(1.f, (range - 0.35f) / 0.65f)) * 0.45f;
				const int amount = uniform() < twoChance ? 2 : 1;
				o = (uniform() < 0.55f) ? amount : -amount;
			}

			deg[s] = (int8_t) d;
			oct[s] = (int8_t) std::max(-2, std::min(o, 2));
			accent[s] = accent[s] && gate[s];
			slide[s] = false;
			tie[s] = false;
		}

		// Los ties sustituyen ataques consecutivos por la prolongación de la nota anterior.
		// No se genera tie en el paso 0: tras un reset no existe una nota previa que sostener.
		const float tieChance = 0.08f + 0.32f * std::max(0.f, std::min(1.f, p.tie));
		for (int s = 1; s < len; s++) {
			if (!gate[s - 1] || !gate[s]) continue;
			const float chainScale = tie[s - 1] ? 0.35f : 1.f;
			if (uniform() >= tieChance * chainScale) continue;
			tie[s] = true;
			deg[s] = deg[s - 1];
			oct[s] = oct[s - 1];
			accent[s] = false;
		}

		// RANGO fija solo el ámbito absoluto: de una octava a tres octavas.
		foldRange(len, si, 12 + (int) std::lround(range * 24.f));

		for (int s = 0; s < len; s++) {
			const int n = (s + 1) % len;
			if (!gate[s] || !gate[n] || tie[s] || tie[n])
				continue;
			const int leap = std::abs(semiOf(deg[n], oct[n], si) - semiOf(deg[s], oct[s], si));
			// La misma altura repetida es un nuevo ataque o un tie, nunca un slide sin
			// movimiento de pitch.
			if (leap == 0) continue;
			const float base = (leap <= 5) ? 0.40f : (leap == 12) ? 0.30f : 0.16f;
			slide[s] = uniform() < std::min(0.95f, base * 4.5f * p.slide);
		}

		for (int s = len; s < ACID_MAX_STEPS; s++) {
			gate[s] = accent[s] = slide[s] = tie[s] = false;
			deg[s] = oct[s] = 0;
		}
		return contour;
	}
};
