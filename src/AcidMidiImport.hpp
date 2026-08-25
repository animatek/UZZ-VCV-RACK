#pragma once

#include "AcidPattern.hpp"
#include "MidiFile.hpp"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Conversión de un Standard MIDI File al formato interno del secuenciador.
//
// El mapeo no es una convención inventada: es el que usan de hecho los editores y los
// clones de 303 cuando exportan un patrón a MIDI, y es lo que hace que un fichero
// suelto suene igual dentro del módulo que fuera.
//
//   note-on en el paso                        → NOTE
//   nota que cruza el límite del paso sin que  → TIE   (prolonga, no reataca)
//     empiece otra
//   nota que cruza el límite y la siguiente    → SLIDE (enlaza dos alturas)
//     tiene otro ataque de distinta altura
//   velocidad por encima del punto medio       → ACENTO
//
// La rejilla es de semicorcheas, la del secuenciador. Un fichero de 4 compases da 4
// patrones de 16 pasos, que es exactamente la forma en la que están escritos los packs
// de patrones acid.
// ---------------------------------------------------------------------------

struct AcidMidiImportResult {
	std::vector<AcidPatternV4> bars;
	int scaleIdx = 0;          // escala con la que se codificaron las alturas
	bool forcedChromatic = false;  // el fichero no cabía en la escala pedida
	int transposedOctaves = 0;     // desplazamiento aplicado para que entrase en el rango
	int droppedVoices = 0;         // notas simultáneas descartadas (el módulo es monofónico)
	int clippedNotes = 0;          // alturas que no cabían ni transponiendo
	int skippedSteps = 0;          // silencio inicial descartado, en pasos
	bool hasAccents = false;
	std::string error;

	bool ok() const { return error.empty() && !bars.empty(); }
};

struct AcidMidiImport {
	// Codifica un desplazamiento en semitonos como grado + octava de la escala dada.
	// Devuelve false si no cabe: el llamante lo cuenta y lo recorta.
	static bool encode(int semi, int scaleIdx, int8_t& degree, int8_t& octave) {
		const AcidScale& sc = ACID_SCALES[scaleIdx];
		const int pc = ((semi % 12) + 12) % 12;
		int i = -1;
		for (int k = 0; k < sc.n; k++)
			if (sc.s[k] == pc) { i = k; break; }
		if (i < 0) return false;
		// semiOf(d, o) = sc.s[i] + 12 * (o + k) con d = i + k * n. El total de octavas se
		// reparte entre la octava del paso, acotada a ±2, y el resto va al grado.
		const int totalOctaves = (semi - sc.s[i]) / 12;
		int o = totalOctaves;
		if (o < -2) o = -2;
		if (o > 2) o = 2;
		const int k = totalOctaves - o;
		const int d = i + k * sc.n;
		if (d < -24 || d > 24) return false;
		degree = (int8_t) d;
		octave = (int8_t) o;
		return true;
	}

	static bool fitsScale(int semi, int scaleIdx) {
		const AcidScale& sc = ACID_SCALES[scaleIdx];
		const int pc = ((semi % 12) + 12) % 12;
		for (int k = 0; k < sc.n; k++)
			if (sc.s[k] == pc) return true;
		return false;
	}

	// Un paso del fichero ya reducido a monofonía.
	struct Slot {
		bool on = false;
		int semi = 0;         // relativo a la raíz del módulo
		bool accent = false;
		double endStep = 0.0; // final de la nota en pasos absolutos
	};

	// `scaleIdx` es la escala que tiene puesta el panel; si el fichero no cabe en ella se
	// pasa a Cromática, que representa cualquier altura sin perder ninguna nota.
	static AcidMidiImportResult convert(const MidiFileData& midi, int scaleIdx,
	                                    int rootSemi, int octaveBase) {
		AcidMidiImportResult r;
		if (midi.notes.empty()) {
			r.error = "no notes in file";
			return r;
		}
		const double ticksPerStep = midi.ticksPerQuarter / 4.0;
		if (ticksPerStep <= 0.0) {
			r.error = "invalid time division";
			return r;
		}

		// Umbral de acento en el punto medio del recorrido de velocidad del fichero. Los
		// patrones acid se exportan con dos velocidades — normal y acentuada — así que el
		// punto medio las separa limpiamente. Con velocidad plana no hay acentos que leer.
		int minVel = 127, maxVel = 0;
		for (size_t n = 0; n < midi.notes.size(); n++) {
			const int v = midi.notes[n].velocity;
			if (v < minVel) minVel = v;
			if (v > maxVel) maxVel = v;
		}
		const bool velocityIsAccent = (maxVel - minVel) >= 8;
		const int accentThreshold = (minVel + maxVel) / 2;
		r.hasAccents = velocityIsAccent;

		// V/Oct del módulo = octaveBase + (rootSemi + semi) / 12, y en VCV 0 V es C4, la
		// nota MIDI 60. De ahí sale el desplazamiento que deja la altura importada exacta.
		const int origin = 60 + 12 * octaveBase + rootSemi;

		// Monofonía: en un acorde se queda la nota más grave. Es lo que hace falta de un
		// secuenciador de líneas de bajo, y deja los arreglos con acompañamiento usables.
		std::vector<Slot> slots;
		int totalSteps = 0;
		for (size_t n = 0; n < midi.notes.size(); n++) {
			const MidiNote& m = midi.notes[n];
			const int step = (int) (m.startTick / ticksPerStep + 0.5);
			if (step < 0) continue;
			if (step >= totalSteps) {
				totalSteps = step + 1;
				slots.resize((size_t) totalSteps);
			}
			Slot& s = slots[(size_t) step];
			const int semi = (int) m.note - origin;
			const double endStep = m.endTick / ticksPerStep;
			if (s.on) {
				r.droppedVoices++;
				if (semi >= s.semi) continue;   // se conserva la más grave
			}
			s.on = true;
			s.semi = semi;
			s.accent = velocityIsAccent && (int) m.velocity > accentThreshold;
			s.endStep = endStep;
		}
		if (totalSteps <= 0) {
			r.error = "no notes on the grid";
			return r;
		}

		// Silencio inicial. Un fichero que no arranca en el tick 0 —una sonata que entra
		// al segundo compás, una pista con cuenta atrás— dejaría la primera página en
		// blanco, y eso no es lo que hay en el fichero sino dónde empieza. Se descarta,
		// pero en bloques de dieciséis pasos: recortar hasta la primera nota movería una
		// anacrusa al tiempo fuerte y dejaría el compás entero a contratiempo.
		int firstOn = -1;
		for (size_t i = 0; i < slots.size(); i++)
			if (slots[i].on) { firstOn = (int) i; break; }
		if (firstOn >= ACID_PAGE_STEPS) {
			const int skip = (firstOn / ACID_PAGE_STEPS) * ACID_PAGE_STEPS;
			slots.erase(slots.begin(), slots.begin() + skip);
			for (size_t i = 0; i < slots.size(); i++)
				slots[i].endStep -= (double) skip;
			totalSteps -= skip;
			r.skippedSteps = skip;
		}

		// Si el fichero se sale del rango representable se transporta entero por octavas:
		// mueve la línea de sitio, pero conserva todos los intervalos.
		int lo = 9999, hi = -9999;
		for (size_t i = 0; i < slots.size(); i++) {
			if (!slots[i].on) continue;
			if (slots[i].semi < lo) lo = slots[i].semi;
			if (slots[i].semi > hi) hi = slots[i].semi;
		}
		int shift = 0;
		while (hi + shift > 47 && lo + shift - 12 >= -48) shift -= 12;
		while (lo + shift < -48 && hi + shift + 12 <= 47) shift += 12;
		r.transposedOctaves = shift / 12;

		// La escala del panel se conserva solo si representa todas las alturas del fichero.
		int useScale = (scaleIdx >= 0 && scaleIdx < ACID_SCALES_LEN) ? scaleIdx : 0;
		for (size_t i = 0; i < slots.size(); i++) {
			if (slots[i].on && !fitsScale(slots[i].semi + shift, useScale)) {
				useScale = ACID_SCALES_LEN - 1;   // Cromática: representa cualquier altura
				r.forcedChromatic = true;
				break;
			}
		}
		r.scaleIdx = useScale;

		const int barCount = (totalSteps + ACID_MAX_STEPS - 1) / ACID_MAX_STEPS;
		for (int bar = 0; bar < barCount; bar++) {
			AcidPatternV4 p;
			p.clear();
			const int first = bar * ACID_MAX_STEPS;
			const int steps = (totalSteps - first < ACID_MAX_STEPS)
			                ? (totalSteps - first) : ACID_MAX_STEPS;
			p.timeLength = (uint8_t) (steps > 0 ? steps : 1);

			int event = 0;
			for (int i = 0; i < steps; i++) {
				const int s = first + i;
				const Slot& here = slots[(size_t) s];
				if (here.on) {
					p.time[i] = AcidTimeState::Note;
					AcidPitchEvent& e = p.pitch[event];
					int8_t degree = 0, octave = 0;
					if (encode(here.semi + shift, useScale, degree, octave)) {
						e.degree = degree;
						e.octave = octave;
					}
					else {
						r.clippedNotes++;
					}
					e.accent = here.accent;
					// La nota cruza el límite del paso y la siguiente tiene ataque propio:
					// eso es un slide, no un tie. La comprobación de altura distinta la
					// repite sanitize(), que además cubre el cierre del bucle.
					const int nextStep = s + 1;
					const bool crosses = here.endStep > (double) nextStep + 0.02;
					const bool nextAttacks = nextStep < totalSteps && slots[(size_t) nextStep].on;
					e.slideOut = crosses && nextAttacks;
					event++;
				}
				else {
					// Sin ataque propio: es TIE mientras la nota anterior siga sonando.
					bool sustained = false;
					for (int back = s - 1; back >= first && back >= s - ACID_MAX_STEPS; back--) {
						if (!slots[(size_t) back].on) continue;
						sustained = slots[(size_t) back].endStep > (double) s + 0.02;
						break;
					}
					p.time[i] = sustained ? AcidTimeState::Tie : AcidTimeState::Rest;
				}
			}
			p.pitchLength = (uint8_t) event;
			// Semilla estable derivada del contenido: identifica el patrón importado en el
			// menú sin fingir que salió del generador.
			uint32_t h = 2166136261u;
			for (int i = 0; i < steps; i++) {
				h = (h ^ (uint32_t) p.time[i]) * 16777619u;
				h = (h ^ (uint32_t) (uint8_t) p.pitch[i].degree) * 16777619u;
				h = (h ^ (uint32_t) (uint8_t) p.pitch[i].octave) * 16777619u;
			}
			p.seed = h ? h : 1u;
			p.sanitize(useScale);
			if (p.noteCount() > 0)
				r.bars.push_back(p);
		}
		if (r.bars.empty())
			r.error = "no usable bars in file";
		return r;
	}
};
