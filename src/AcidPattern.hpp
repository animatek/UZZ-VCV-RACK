#pragma once

#include "AcidGen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Formato interno v4. El tiempo consume una altura solamente en NOTE; TIE sostiene la
// altura anterior y REST corta la voz. AcidGen sigue siendo la vista temporal que usan
// el motor, los LEDs y los jacks, no la fuente de verdad del patrón.
enum class AcidTimeState : uint8_t { Rest = 0, Note = 1, Tie = 2 };

struct AcidPitchEvent {
	int8_t degree = 0;
	int8_t octave = 0;
	bool accent = false;
	bool slideOut = false;
};

struct AcidPatternV4 {
	static const uint8_t SCHEMA_VERSION = 4;
	static const uint8_t ALGORITHM_VERSION = 4;

	uint32_t seed = 1u;
	uint8_t algorithmVersion = ALGORITHM_VERSION;
	uint8_t timeLength = ACID_MAX_STEPS;
	uint8_t pitchLength = 1;
	AcidTimeState time[ACID_MAX_STEPS] = {};
	AcidPitchEvent pitch[ACID_MAX_STEPS] = {};

	void clear() {
		seed = 1u;
		algorithmVersion = ALGORITHM_VERSION;
		timeLength = ACID_MAX_STEPS;
		pitchLength = 1;
		for (int i = 0; i < ACID_MAX_STEPS; i++) {
			time[i] = AcidTimeState::Rest;
			pitch[i] = AcidPitchEvent();
		}
		time[0] = AcidTimeState::Note;
	}

	int noteCount() const {
		int n = 0;
		for (int i = 0; i < timeLength; i++)
			if (time[i] == AcidTimeState::Note) n++;
		return n;
	}

	// Repara JSON editado a mano y mantiene una representación canónica. Por ahora el
	// slide v4 conserva la restricción segura de v3: enlaza NOTE adyacentes, nunca REST/TIE.
	void sanitize(int scaleIdx) {
		timeLength = (uint8_t) std::max(1, std::min((int) timeLength, ACID_MAX_STEPS));
		bool active = false;
		int notes = 0;
		for (int i = 0; i < timeLength; i++) {
			const int raw = (int) time[i];
			if (raw < (int) AcidTimeState::Rest || raw > (int) AcidTimeState::Tie)
				time[i] = AcidTimeState::Rest;
			if (time[i] == AcidTimeState::Tie && !active)
				time[i] = AcidTimeState::Rest;
			if (time[i] == AcidTimeState::Note) {
				active = true;
				notes++;
			}
			else if (time[i] == AcidTimeState::Rest) {
				active = false;
			}
		}
		for (int i = timeLength; i < ACID_MAX_STEPS; i++)
			time[i] = AcidTimeState::Rest;

		if (notes == 0) {
			time[0] = AcidTimeState::Note;
			notes = 1;
		}
		const int oldPitchLength = std::max(0, std::min((int) pitchLength, ACID_MAX_STEPS));
		for (int i = oldPitchLength; i < notes; i++)
			pitch[i] = AcidPitchEvent();
		pitchLength = (uint8_t) notes;

		for (int i = 0; i < pitchLength; i++) {
			pitch[i].degree = (int8_t) std::max(-24, std::min((int) pitch[i].degree, 24));
			pitch[i].octave = (int8_t) std::max(-2, std::min((int) pitch[i].octave, 2));
		}
		for (int i = pitchLength; i < ACID_MAX_STEPS; i++)
			pitch[i] = AcidPitchEvent();

		const int si = std::max(0, std::min(scaleIdx, ACID_SCALES_LEN - 1));
		int eventStep[ACID_MAX_STEPS];
		for (int i = 0; i < ACID_MAX_STEPS; i++) eventStep[i] = -1;
		int e = 0;
		for (int s = 0; s < timeLength && e < pitchLength; s++)
			if (time[s] == AcidTimeState::Note) eventStep[e++] = s;
		for (int i = 0; i < pitchLength; i++) {
			if (!pitch[i].slideOut) continue;
			const int s = eventStep[i];
			const int nextStep = (s + 1) % timeLength;
			const int nextEvent = (i + 1) % pitchLength;
			const int here = AcidGen::semiOf(pitch[i].degree, pitch[i].octave, si);
			const int there = AcidGen::semiOf(pitch[nextEvent].degree, pitch[nextEvent].octave, si);
			if (s < 0 || time[nextStep] != AcidTimeState::Note || here == there)
				pitch[i].slideOut = false;
		}
	}

	// Produce la rejilla compatible que consume Atek303Seq. En silencios se conserva la
	// última altura: V/Oct deja de saltar a valores invisibles mientras gate está cerrado.
	void render(AcidGen& out) const {
		int event = 0;
		int8_t lastDegree = pitchLength ? pitch[0].degree : 0;
		int8_t lastOctave = pitchLength ? pitch[0].octave : 0;
		bool havePitch = false;
		for (int s = 0; s < ACID_MAX_STEPS; s++) {
			out.gate[s] = out.accent[s] = out.slide[s] = out.tie[s] = false;
			if (s < timeLength && time[s] == AcidTimeState::Note && event < pitchLength) {
				const AcidPitchEvent& p = pitch[event++];
				lastDegree = p.degree;
				lastOctave = p.octave;
				havePitch = true;
				out.gate[s] = true;
				out.accent[s] = p.accent;
				out.slide[s] = p.slideOut;
			}
			else if (s < timeLength && time[s] == AcidTimeState::Tie && havePitch) {
				out.gate[s] = true;
				out.tie[s] = true;
			}
			out.deg[s] = lastDegree;
			out.oct[s] = lastOctave;
		}
	}

	// Importación sin pérdida audible desde la rejilla v3. Las alturas almacenadas bajo
	// REST no tenían ataque ni semántica musical y se descartan deliberadamente.
	void importRendered(const AcidGen& in, int length, uint32_t importedSeed,
	                    uint8_t sourceAlgorithmVersion = 3) {
		clear();
		seed = importedSeed ? importedSeed : 1u;
		algorithmVersion = sourceAlgorithmVersion;
		timeLength = (uint8_t) std::max(1, std::min(length, ACID_MAX_STEPS));
		pitchLength = 0;
		bool active = false;
		for (int s = 0; s < timeLength; s++) {
			if (!in.gate[s]) {
				time[s] = AcidTimeState::Rest;
				active = false;
				continue;
			}
			if (in.tie[s] && active) {
				time[s] = AcidTimeState::Tie;
				continue;
			}
			time[s] = AcidTimeState::Note;
			active = true;
			AcidPitchEvent& p = pitch[pitchLength++];
			p.degree = in.deg[s];
			p.octave = in.oct[s];
			p.accent = in.accent[s];
			p.slideOut = in.slide[s];
		}
	}
};

// PRNG y derivación de streams definidos bit a bit. No se usa std::hash, de modo que una
// misma seed conserva el mismo resultado en todas las plataformas soportadas.
struct AcidLayerRng {
	uint32_t state = 1u;

	explicit AcidLayerRng(uint32_t seed = 1u) : state(seed ? seed : 1u) {}

	uint32_t next() {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}

	float unit() { return (next() >> 8) * (1.f / 16777216.f); }

	static uint32_t mix(uint32_t seed, uint32_t stream) {
		uint32_t z = seed ^ stream ^ 0x9e3779b9u;
		z ^= z >> 16;
		z *= 0x85ebca6bu;
		z ^= z >> 13;
		z *= 0xc2b2ae35u;
		z ^= z >> 16;
		return z ? z : 1u;
	}
};

struct AcidDualGenerator {
	enum Stream : uint32_t {
		STYLE  = 0x5354594cu,
		TIME   = 0x54494d45u,
		PITCH  = 0x50495443u,
		OCTAVE = 0x4f435441u,
		ACCENT = 0x41434345u,
		SLIDE  = 0x534c4944u
	};

	int lastContour = AcidGen::C_PEDAL;

	static float activationThreshold(float u, bool cellStart) {
		const float floor = cellStart ? 0.30f : 0.05f;
		const float slope = cellStart ? 0.70f : 0.80f;
		return u <= floor ? 0.f : (u - floor) / slope;
	}

	static int chooseVocabSize(float u, int scaleSize) {
		const int n = u < 0.10f ? 1 : u < 0.55f ? 2 : u < 0.90f ? 3 : 4;
		return std::min(n, scaleSize);
	}

	static int buildVocab(int (&voc)[12], int scaleIdx, int want, AcidLayerRng& rng) {
		const AcidScale& sc = ACID_SCALES[scaleIdx];
		bool taken[12] = {};
		voc[0] = 0;
		taken[0] = true;
		int nv = 1;
		while (nv < want && nv < 12) {
			float total = 0.f;
			for (int d = 1; d < sc.n; d++)
				if (!taken[d]) total += AcidGen::semiWeight(sc.s[d]);
			float r = rng.unit() * total;
			for (int d = 1; d < sc.n; d++) {
				if (taken[d]) continue;
				r -= AcidGen::semiWeight(sc.s[d]);
				if (r <= 0.f) {
					taken[d] = true;
					voc[nv++] = d;
					break;
				}
			}
		}
		std::sort(voc, voc + nv);
		return nv;
	}

	static int pickContour(AcidLayerRng& rng) {
		static const float weight[AcidGen::CONTOUR_LEN] = {0.22f, 0.18f, 0.15f, 0.26f, 0.19f};
		float r = rng.unit();
		for (int i = 0; i < AcidGen::CONTOUR_LEN; i++) {
			r -= weight[i];
			if (r <= 0.f) return i;
		}
		return AcidGen::C_PEDAL;
	}

	static void foldPitchRange(AcidPatternV4& pattern, int scaleIdx, int limitSemi) {
		for (int pass = 0; pass < 6; pass++) {
			int lo = 999, hi = -999;
			for (int i = 0; i < pattern.pitchLength; i++) {
				const int v = AcidGen::semiOf(pattern.pitch[i].degree, pattern.pitch[i].octave, scaleIdx);
				lo = std::min(lo, v);
				hi = std::max(hi, v);
			}
			if (hi - lo <= limitSemi) return;
			const int mid = (hi + lo) / 2;
			const bool moveHigh = (hi - mid) >= (mid - lo);
			const int from = moveHigh ? hi : lo;
			const int delta = moveHigh ? -1 : 1;
			bool moved = false;
			for (int i = 0; i < pattern.pitchLength; i++) {
				AcidPitchEvent& p = pattern.pitch[i];
				if (AcidGen::semiOf(p.degree, p.octave, scaleIdx) != from) continue;
				const int octave = p.octave + delta;
				if (octave < -2 || octave > 2) continue;
				p.octave = (int8_t) octave;
				moved = true;
			}
			if (!moved) return;
		}
	}

	int generate(AcidPatternV4& pattern, const AcidGenParams& params, uint32_t seed) {
		const int length = std::max(1, std::min(params.steps, ACID_MAX_STEPS));
		const int scaleIdx = std::max(0, std::min(params.scale, ACID_SCALES_LEN - 1));
		const float density = std::max(0.f, std::min(1.f, params.density));
		const float range = std::max(0.f, std::min(1.f, params.range));
		const float accentAmount = std::max(0.f, std::min(1.f, params.accent));
		const float slideAmount = std::max(0.f, std::min(1.f, params.slide));
		const float tieAmount = std::max(0.f, std::min(1.f, params.tie));

		AcidLayerRng style(AcidLayerRng::mix(seed, STYLE));
		AcidLayerRng timeRng(AcidLayerRng::mix(seed, TIME));
		AcidLayerRng pitchRng(AcidLayerRng::mix(seed, PITCH));
		AcidLayerRng octaveRng(AcidLayerRng::mix(seed, OCTAVE));
		AcidLayerRng accentRng(AcidLayerRng::mix(seed, ACCENT));
		AcidLayerRng slideRng(AcidLayerRng::mix(seed, SLIDE));

		pattern.clear();
		pattern.seed = seed ? seed : 1u;
		pattern.algorithmVersion = AcidPatternV4::ALGORITHM_VERSION;
		pattern.timeLength = (uint8_t) length;

		const int cell = style.unit() < 0.25f ? 3 : 4;
		lastContour = pickContour(style);
		const int wantVocab = chooseVocabSize(style.unit(), ACID_SCALES[scaleIdx].n);

		// Cada paso recibe el nivel de DENSIDAD en el que aparecerá. La condición usada
		// para los ties garantiza que su nota anterior ya existe al activarse: con seed
		// bloqueada, subir DENSIDAD solo añade estados y nunca borra los anteriores.
		float activation[ACID_MAX_STEPS];
		bool tiePlan[ACID_MAX_STEPS] = {};
		int forcedStep = 0;
		for (int s = 0; s < length; s++) {
			activation[s] = activationThreshold(timeRng.unit(), s % cell == 0);
			if (activation[s] < activation[forcedStep]) forcedStep = s;
		}
		// Solo aproximadamente la mitad de las parejas cumple el orden de activación que
		// hace el tie estable al mover DENSIDAD; se compensa aquí para conservar el carácter
		// de v3 (alrededor de un 10 % de los pasos ocupados con tie=0.40).
		const float tieChance = std::min(0.90f, 0.16f + 0.64f * tieAmount);
		for (int s = 1; s < length; s++) {
			const float chain = tiePlan[s - 1] ? 0.35f : 1.f;
			tiePlan[s] = activation[s - 1] <= activation[s]
			           && timeRng.unit() < tieChance * chain;
		}
		for (int s = 0; s < length; s++) {
			const bool on = activation[s] <= density || s == forcedStep;
			if (!on) pattern.time[s] = AcidTimeState::Rest;
			else if (s > 0 && tiePlan[s]) pattern.time[s] = AcidTimeState::Tie;
			else {
				pattern.time[s] = AcidTimeState::Note;
			}
		}
		// El paso forzado nunca puede depender de una nota anterior.
		if (pattern.time[forcedStep] == AcidTimeState::Tie) {
			pattern.time[forcedStep] = AcidTimeState::Note;
		}
		// La lista potencial se ordena una sola vez para la seed completa. Al aumentar
		// DENSIDAD se revelan candidatos que antes estaban ocultos; los ataques que ya
		// existían conservan grado, octava y acento aunque aparezca otro antes que ellos.
		int candidateAtStep[ACID_MAX_STEPS];
		for (int i = 0; i < ACID_MAX_STEPS; i++) candidateAtStep[i] = -1;
		int candidateCount = 0;
		for (int s = 0; s < length; s++) {
			const bool canAppear = activation[s] <= 1.f || s == forcedStep;
			const bool plannedTie = s > 0 && tiePlan[s] && s != forcedStep;
			if (canAppear && !plannedTie) candidateAtStep[s] = candidateCount++;
		}
		pattern.pitchLength = (uint8_t) candidateCount;

		int vocab[12];
		const int vocabLength = buildVocab(vocab, scaleIdx, wantVocab, pitchRng);
		int cellVocab[4] = {};
		switch (lastContour) {
			case AcidGen::C_PEDAL:
				for (int i = 0; i < cell; i++)
					cellVocab[i] = (i == 0 || pitchRng.unit() < 0.45f)
					             ? 0 : (int) (pitchRng.unit() * vocabLength) % vocabLength;
				break;
			case AcidGen::C_RISE:
			case AcidGen::C_FALL:
				for (int i = 0; i < cell; i++) cellVocab[i] = (int) (pitchRng.unit() * vocabLength) % vocabLength;
				for (int a = 0; a < cell; a++)
					for (int b = a + 1; b < cell; b++)
						if ((lastContour == AcidGen::C_RISE)
						      ? (vocab[cellVocab[b]] < vocab[cellVocab[a]])
						      : (vocab[cellVocab[b]] > vocab[cellVocab[a]]))
							std::swap(cellVocab[a], cellVocab[b]);
				if (pitchRng.unit() < 0.6f) cellVocab[0] = 0;
				break;
			case AcidGen::C_ZIGZAG: {
				const int top = vocabLength > 1
				              ? 1 + (int) (pitchRng.unit() * (vocabLength - 1)) % (vocabLength - 1) : 0;
				for (int i = 0; i < cell; i++) cellVocab[i] = i % 2 ? top : 0;
				break;
			}
			default:
				cellVocab[0] = (int) (pitchRng.unit() * vocabLength) % vocabLength;
				for (int i = 1; i < cell; i++)
					cellVocab[i] = pitchRng.unit() < 0.40f
					             ? cellVocab[i - 1]
					             : (int) (pitchRng.unit() * vocabLength) % vocabLength;
				break;
		}

		int vocabShift[ACID_MAX_STEPS] = {};
		for (int rep = 1; rep * cell < ACID_MAX_STEPS; rep++) {
			if (vocabLength > 1 && pitchRng.unit() < 0.45f) {
				static const int SHIFT[6] = {1, -1, 1, -1, 2, -2};
				vocabShift[rep] = SHIFT[(int) (pitchRng.unit() * 6.f) % 6];
			}
		}

		for (int i = 0; i < ACID_MAX_STEPS; i++) {
			const int cellPos = i % cell;
			const int rep = i / cell;
			int vi = (cellVocab[cellPos] + vocabShift[rep]) % vocabLength;
			if (vi < 0) vi += vocabLength;
			int degree = vocab[vi];
			if (rep > 0 && pitchRng.unit() < 0.18f)
				degree = vocab[(int) (pitchRng.unit() * vocabLength) % vocabLength];

			AcidPitchEvent& p = pattern.pitch[i];
			p.degree = (int8_t) degree;
			const float jumpChance = range * (cellPos == 0 ? 0.55f : 0.38f);
			if (octaveRng.unit() < jumpChance) {
				const float twoChance = std::max(0.f, std::min(1.f, (range - 0.35f) / 0.65f)) * 0.45f;
				const int amount = octaveRng.unit() < twoChance ? 2 : 1;
				p.octave = (int8_t) (octaveRng.unit() < 0.55f ? amount : -amount);
			}
			const float accentChance = cellPos == 0 ? std::min(0.95f, 1.25f * accentAmount)
			                                              : std::min(0.85f, 0.85f * accentAmount);
			p.accent = accentRng.unit() < accentChance;
			p.slideOut = false;
		}

		foldPitchRange(pattern, scaleIdx, 12 + (int) std::lround(range * 24.f));

		AcidPitchEvent candidates[ACID_MAX_STEPS];
		for (int i = 0; i < ACID_MAX_STEPS; i++) candidates[i] = pattern.pitch[i];
		pattern.pitchLength = 0;
		for (int s = 0; s < length; s++) {
			if (pattern.time[s] != AcidTimeState::Note) continue;
			const int candidate = candidateAtStep[s];
			if (candidate >= 0 && candidate < candidateCount)
				pattern.pitch[pattern.pitchLength++] = candidates[candidate];
		}
		for (int i = pattern.pitchLength; i < ACID_MAX_STEPS; i++)
			pattern.pitch[i] = AcidPitchEvent();

		// Slides se asignan en el dominio de alturas, pero solo a transiciones temporales
		// realmente adyacentes. Su stream independiente evita que SLIDE altere las notas.
		int eventAtStep[ACID_MAX_STEPS];
		for (int i = 0; i < ACID_MAX_STEPS; i++) eventAtStep[i] = -1;
		int event = 0;
		for (int s = 0; s < length; s++)
			if (pattern.time[s] == AcidTimeState::Note) eventAtStep[s] = event++;
		for (int s = 0; s < length; s++) {
			const int hereEvent = eventAtStep[s];
			const int nextStep = (s + 1) % length;
			if (hereEvent < 0 || eventAtStep[nextStep] < 0) continue;
			AcidPitchEvent& here = pattern.pitch[hereEvent];
			const AcidPitchEvent& next = pattern.pitch[eventAtStep[nextStep]];
			const int leap = std::abs(AcidGen::semiOf(next.degree, next.octave, scaleIdx)
			                        - AcidGen::semiOf(here.degree, here.octave, scaleIdx));
			if (leap == 0) continue;
			const float base = leap <= 5 ? 0.40f : leap == 12 ? 0.30f : 0.16f;
			here.slideOut = slideRng.unit() < std::min(0.95f, base * 4.5f * slideAmount);
		}

		pattern.sanitize(scaleIdx);
		return lastContour;
	}
};

struct AcidPatternMutator {
	enum Layer { Time = 1, Pitch = 2, Articulation = 3 };

	static bool same(const AcidPatternV4& a, const AcidPatternV4& b) {
		if (a.seed != b.seed || a.algorithmVersion != b.algorithmVersion
		    || a.timeLength != b.timeLength || a.pitchLength != b.pitchLength)
			return false;
		for (int i = 0; i < a.timeLength; i++)
			if (a.time[i] != b.time[i]) return false;
		for (int i = 0; i < a.pitchLength; i++) {
			if (a.pitch[i].degree != b.pitch[i].degree
			    || a.pitch[i].octave != b.pitch[i].octave
			    || a.pitch[i].accent != b.pitch[i].accent
			    || a.pitch[i].slideOut != b.pitch[i].slideOut)
				return false;
		}
		return true;
	}

	static int eventIndexAtStep(const AcidPatternV4& pattern, int wantedStep) {
		int event = 0;
		for (int s = 0; s < pattern.timeLength; s++) {
			if (pattern.time[s] != AcidTimeState::Note) continue;
			if (s == wantedStep) return event;
			event++;
		}
		return -1;
	}

	static void removePitch(AcidPatternV4& pattern, int event) {
		if (event < 0 || event >= pattern.pitchLength) return;
		for (int i = event; i + 1 < pattern.pitchLength; i++)
			pattern.pitch[i] = pattern.pitch[i + 1];
		pattern.pitchLength--;
		pattern.pitch[pattern.pitchLength] = AcidPitchEvent();
	}

	static void insertPitch(AcidPatternV4& pattern, int event, const AcidPitchEvent& value) {
		if (pattern.pitchLength >= ACID_MAX_STEPS) return;
		event = std::max(0, std::min(event, (int) pattern.pitchLength));
		for (int i = pattern.pitchLength; i > event; i--)
			pattern.pitch[i] = pattern.pitch[i - 1];
		pattern.pitch[event] = value;
		pattern.pitchLength++;
	}

	static bool mutateTime(AcidPatternV4& pattern, AcidLayerRng& rng) {
		const int length = pattern.timeLength;
		// Operación preferida: mover un ataque aislado a un silencio. Conserva el número y
		// el orden de PitchEvents, pero desplaza la síncopa.
		for (int attempt = 0; attempt < 48; attempt++) {
			const int from = (int) (rng.next() % length);
			const int to = (int) (rng.next() % length);
			const int after = (from + 1) % length;
			if (from == to || pattern.time[from] != AcidTimeState::Note
			    || pattern.time[to] != AcidTimeState::Rest
			    || pattern.time[after] == AcidTimeState::Tie)
				continue;
			pattern.time[from] = AcidTimeState::Rest;
			pattern.time[to] = AcidTimeState::Note;
			return true;
		}

		// Patrón sin huecos útiles: abrir un nuevo ataque dentro de una cadena de ties.
		for (int s = 1; s < length; s++) {
			if (pattern.time[s] != AcidTimeState::Tie) continue;
			int event = 0;
			for (int i = 0; i < s; i++)
				if (pattern.time[i] == AcidTimeState::Note) event++;
			const int previous = std::max(0, event - 1);
			insertPitch(pattern, event, pattern.pitch[previous]);
			pattern.time[s] = AcidTimeState::Note;
			return true;
		}

		// Último fallback: unir dos ataques consecutivos y retirar el evento consumido.
		for (int s = 1; s < length; s++) {
			if (pattern.time[s] != AcidTimeState::Note
			    || pattern.time[s - 1] == AcidTimeState::Rest)
				continue;
			const int event = eventIndexAtStep(pattern, s);
			pattern.time[s] = AcidTimeState::Tie;
			removePitch(pattern, event);
			return true;
		}
		return false;
	}

	static bool mutatePitch(AcidPatternV4& pattern, AcidLayerRng& rng) {
		if (!pattern.pitchLength) return false;
		const int event = (int) (rng.next() % pattern.pitchLength);
		AcidPitchEvent& p = pattern.pitch[event];
		// Una octava conserva el vocabulario y produce una variación inequívoca incluso en
		// patrones de una sola clase. Alterna dirección en los extremos.
		if (p.octave >= 2) p.octave--;
		else if (p.octave <= -2) p.octave++;
		else p.octave += (rng.next() & 1u) ? 1 : -1;
		return true;
	}

	static bool mutateArticulation(AcidPatternV4& pattern, AcidLayerRng& rng, int scaleIdx) {
		if (!pattern.pitchLength) return false;
		// La mitad de las veces se intenta slide, siempre sobre una transición válida.
		if (rng.next() & 1u) {
			int eventAtStep[ACID_MAX_STEPS];
			for (int i = 0; i < ACID_MAX_STEPS; i++) eventAtStep[i] = -1;
			int event = 0;
			for (int s = 0; s < pattern.timeLength; s++)
				if (pattern.time[s] == AcidTimeState::Note) eventAtStep[s] = event++;
			for (int attempt = 0; attempt < pattern.timeLength; attempt++) {
				const int s = (int) (rng.next() % pattern.timeLength);
				const int next = (s + 1) % pattern.timeLength;
				const int a = eventAtStep[s], b = eventAtStep[next];
				if (a < 0 || b < 0) continue;
				const int here = AcidGen::semiOf(pattern.pitch[a].degree, pattern.pitch[a].octave, scaleIdx);
				const int there = AcidGen::semiOf(pattern.pitch[b].degree, pattern.pitch[b].octave, scaleIdx);
				if (here == there) continue;
				pattern.pitch[a].slideOut = !pattern.pitch[a].slideOut;
				return true;
			}
		}
		const int event = (int) (rng.next() % pattern.pitchLength);
		pattern.pitch[event].accent = !pattern.pitch[event].accent;
		return true;
	}

	static bool mutate(AcidPatternV4& pattern, Layer layer, uint32_t mutationIndex, int scaleIdx) {
		const AcidPatternV4 before = pattern;
		const uint32_t tag = layer == Time ? 0x4d54494du
		                   : layer == Pitch ? 0x4d504954u : 0x4d415254u;
		AcidLayerRng rng(AcidLayerRng::mix(pattern.seed ^ (0x9e3779b9u * mutationIndex), tag));
		bool changed = false;
		if (layer == Time) changed = mutateTime(pattern, rng);
		else if (layer == Pitch) changed = mutatePitch(pattern, rng);
		else changed = mutateArticulation(pattern, rng, scaleIdx);
		pattern.sanitize(scaleIdx);
		return changed && !same(before, pattern);
	}

	// Agrupa varias operaciones deterministas en un solo gesto de interfaz. Los índices
	// consumidos se devuelven para que la siguiente pulsación continúe la secuencia y para
	// que todo el grupo pueda deshacerse como una única mutación.
	static bool mutateBurst(AcidPatternV4& pattern, Layer layer, uint32_t firstMutationIndex,
	                        int operationCount, int scaleIdx, uint32_t& lastMutationIndex) {
		const AcidPatternV4 before = pattern;
		const int wanted = std::max(1, operationCount);
		int applied = 0;
		lastMutationIndex = firstMutationIndex;
		for (int attempt = 0; attempt < 32; attempt++) {
			const uint32_t candidate = firstMutationIndex + (uint32_t) attempt;
			lastMutationIndex = candidate;
			if (mutate(pattern, layer, candidate, scaleIdx))
				applied++;
			// Dos operaciones pueden cancelarse entre sí. En ese caso consumimos una más
			// para que una pulsación nunca termine visualmente en el patrón de partida.
			if (applied >= wanted && !same(before, pattern))
				return true;
		}
		pattern = before;
		return false;
	}
};
