#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Lector mínimo de Standard MIDI File. Sin dependencias, ni siquiera de Rack: se
// puede probar en el banco offline igual que AcidGen. Solo extrae lo que el
// secuenciador necesita — notas con tick absoluto, altura y velocidad — y tira el
// resto: controladores, program change, letra, tempo. Un patrón acid no tiene nada
// que hacer con ellos.
//
// Cubre los formatos 0, 1 y 2. En formato 1 las pistas suenan a la vez, así que se
// vuelcan todas a la misma lista de eventos; en 0 y 2 hay una sola pista relevante y
// el resultado es el mismo. La división SMPTE (los ficheros con vídeo, división
// negativa) se rechaza: no tiene rejilla musical que cuantizar.
// ---------------------------------------------------------------------------

struct MidiNote {
	int32_t startTick = 0;
	int32_t endTick = 0;
	uint8_t note = 0;
	uint8_t velocity = 0;
	uint8_t channel = 0;
};

struct MidiFileData {
	int format = 0;
	int ticksPerQuarter = 96;
	std::vector<MidiNote> notes;   // ordenadas por startTick
	int32_t lengthTicks = 0;

	bool empty() const { return notes.empty(); }
};

struct MidiFileReader {
	std::string error;

	// Lee el fichero entero en memoria. Un SMF de patrones son cientos de bytes; hasta
	// una canción completa cabe de sobra, así que no hay motivo para leer a trozos.
	bool load(const std::string& path, MidiFileData& out) {
		error.clear();
		std::vector<uint8_t> buf;
		if (!readAll(path, buf)) {
			error = "cannot read file";
			return false;
		}
		return parse(buf, out);
	}

	bool parse(const std::vector<uint8_t>& b, MidiFileData& out) {
		out = MidiFileData();
		size_t i = 0;
		if (b.size() < 14 || !tag(b, 0, "MThd")) {
			error = "not a Standard MIDI File";
			return false;
		}
		const uint32_t headerLen = be32(b, 4);
		if (headerLen < 6 || 8 + (size_t) headerLen > b.size()) {
			error = "damaged header";
			return false;
		}
		out.format = (int) be16(b, 8);
		const int trackCount = (int) be16(b, 10);
		const int16_t division = (int16_t) be16(b, 12);
		if (division <= 0) {
			error = "SMPTE time division is not supported";
			return false;
		}
		out.ticksPerQuarter = division;
		i = 8 + headerLen;

		// Notas abiertas por canal y altura. El MIDI permite repetir un note-on sobre una
		// nota que ya suena; la convención habitual es que el primer note-off cierra el
		// primer note-on, así que la pila se consume por el frente.
		std::vector<MidiNote> open;
		int tracksRead = 0;
		while (i + 8 <= b.size() && tracksRead < trackCount) {
			if (!tag(b, i, "MTrk")) {
				// Un trozo desconocido se salta por longitud, como manda la especificación.
				const uint32_t skip = be32(b, i + 4);
				i += 8 + skip;
				continue;
			}
			const uint32_t trackLen = be32(b, i + 4);
			size_t j = i + 8;
			const size_t end = (j + trackLen <= b.size()) ? j + trackLen : b.size();
			int32_t tick = 0;
			uint8_t running = 0;
			while (j < end) {
				uint32_t delta = 0;
				if (!vlq(b, j, end, delta)) break;
				tick += (int32_t) delta;
				if (j >= end) break;
				uint8_t status = b[j];
				if (status & 0x80) {
					j++;
					// El running status no sobrevive a un mensaje de sistema.
					running = (status < 0xf0) ? status : 0;
				}
				else {
					if (!running) break;   // dato sin estado previo: pista corrupta
					status = running;
				}
				if (status == 0xff) {
					if (j >= end) break;
					j++;                    // tipo de meta evento
					uint32_t len = 0;
					if (!vlq(b, j, end, len)) break;
					j += len;
				}
				else if (status == 0xf0 || status == 0xf7) {
					uint32_t len = 0;
					if (!vlq(b, j, end, len)) break;
					j += len;
				}
				else {
					const uint8_t kind = status & 0xf0;
					const int dataBytes = (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
					if (j + (size_t) dataBytes > end) break;
					const uint8_t d1 = b[j];
					const uint8_t d2 = (dataBytes == 2) ? b[j + 1] : 0;
					j += dataBytes;
					const uint8_t channel = status & 0x0f;
					// Un note-on de velocidad 0 es un note-off: lo usa casi todo secuenciador
					// que aproveche el running status.
					if (kind == 0x90 && d2 > 0) {
						MidiNote n;
						n.startTick = tick;
						n.endTick = tick;
						n.note = d1;
						n.velocity = d2;
						n.channel = channel;
						open.push_back(n);
						out.notes.push_back(n);
					}
					else if (kind == 0x80 || (kind == 0x90 && d2 == 0)) {
						closeNote(open, out.notes, channel, d1, tick);
					}
				}
			}
			i = i + 8 + trackLen;
			tracksRead++;
		}

		// Una nota sin note-off dura hasta el final: mejor eso que perderla.
		int32_t last = 0;
		for (size_t n = 0; n < out.notes.size(); n++)
			if (out.notes[n].endTick > last) last = out.notes[n].endTick;
		for (size_t n = 0; n < out.notes.size(); n++)
			if (out.notes[n].endTick <= out.notes[n].startTick) out.notes[n].endTick = last;
		for (size_t n = 0; n < out.notes.size(); n++)
			if (out.notes[n].endTick > out.lengthTicks) out.lengthTicks = out.notes[n].endTick;

		if (out.notes.empty()) {
			error = "no notes in file";
			return false;
		}
		// Orden estable por tick y después por altura: dos notas en el mismo paso entran
		// siempre en el mismo orden, y el patrón importado es reproducible.
		insertionSort(out.notes);
		return true;
	}

private:
	static bool tag(const std::vector<uint8_t>& b, size_t i, const char* t) {
		if (i + 4 > b.size()) return false;
		for (int k = 0; k < 4; k++)
			if (b[i + k] != (uint8_t) t[k]) return false;
		return true;
	}

	static uint32_t be32(const std::vector<uint8_t>& b, size_t i) {
		if (i + 4 > b.size()) return 0;
		return ((uint32_t) b[i] << 24) | ((uint32_t) b[i + 1] << 16)
		     | ((uint32_t) b[i + 2] << 8) | (uint32_t) b[i + 3];
	}

	static uint16_t be16(const std::vector<uint8_t>& b, size_t i) {
		if (i + 2 > b.size()) return 0;
		return (uint16_t) (((uint16_t) b[i] << 8) | (uint16_t) b[i + 1]);
	}

	// Cantidad de longitud variable, hasta 4 bytes como manda la especificación.
	static bool vlq(const std::vector<uint8_t>& b, size_t& i, size_t end, uint32_t& out) {
		out = 0;
		for (int k = 0; k < 4; k++) {
			if (i >= end) return false;
			const uint8_t c = b[i++];
			out = (out << 7) | (uint32_t) (c & 0x7f);
			if (!(c & 0x80)) return true;
		}
		return false;
	}

	static void closeNote(std::vector<MidiNote>& open, std::vector<MidiNote>& notes,
	                      uint8_t channel, uint8_t note, int32_t tick) {
		for (size_t k = 0; k < open.size(); k++) {
			if (open[k].channel != channel || open[k].note != note) continue;
			for (size_t n = notes.size(); n-- > 0;) {
				if (notes[n].startTick == open[k].startTick && notes[n].note == note
				    && notes[n].channel == channel && notes[n].endTick <= notes[n].startTick) {
					notes[n].endTick = tick;
					break;
				}
			}
			open.erase(open.begin() + (long) k);
			return;
		}
	}

	// El corpus típico son decenas de notas; la inserción evita arrastrar <algorithm>
	// y mantiene el orden de las notas simultáneas.
	static void insertionSort(std::vector<MidiNote>& v) {
		for (size_t a = 1; a < v.size(); a++) {
			MidiNote key = v[a];
			size_t b = a;
			while (b > 0 && (v[b - 1].startTick > key.startTick
			                 || (v[b - 1].startTick == key.startTick && v[b - 1].note > key.note))) {
				v[b] = v[b - 1];
				b--;
			}
			v[b] = key;
		}
	}

	static bool readAll(const std::string& path, std::vector<uint8_t>& out) {
		FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) return false;
		std::fseek(f, 0, SEEK_END);
		const long size = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		// Un SMF sano no llega a 8 MB; más que eso es un fichero equivocado, no un patrón.
		if (size <= 0 || size > 8 * 1024 * 1024) {
			std::fclose(f);
			return false;
		}
		out.resize((size_t) size);
		const size_t got = std::fread(&out[0], 1, (size_t) size, f);
		std::fclose(f);
		out.resize(got);
		return got > 0;
	}
};
