#pragma once

#include "AtekWidgets.hpp"
#include "../AcidPattern.hpp"

#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Editor de patrón de ATEK303 SEQ: un piano roll de una octava con las filas de
// articulación debajo.
//
// Una octava, no más. Es lo que hace que quepa: el roll enseña únicamente los grados de
// la escala activa —cinco filas gordas en ACID, doce en Cromática— y el desplazamiento
// por octavas vive en las filas UP y DOWN, que es exactamente cómo funciona un 303 de
// verdad: teclado de una octava y dos botones que suben o bajan notas sueltas. Y encaja
// con el modelo interno sin conversiones, porque `AcidPatternV4` ya guarda grado dentro
// de la octava más octava por paso.
//
// El widget no toca el patrón del módulo. Mantiene su propia copia de trabajo, aplica la
// edición sobre ella y la entrega por `editorSubmit()`; el hilo de audio la instala
// cuando le toca. La copia se resincroniza cuando el módulo cambia el patrón por debajo
// —GENERATE, una mutación, una importación—, que es lo que detecta el contador de versión.
// ---------------------------------------------------------------------------

// Lo que el editor necesita del módulo, sin conocer el módulo.
struct AcidEditorHost {
	virtual ~AcidEditorHost() {}
	// Sin const: leer un parámetro de Rack no lo es, y fingir que sí obligaba a colar un
	// const_cast en el módulo por nada.
	virtual const AcidPatternV4& editorPattern() = 0;
	virtual uint32_t editorVersion() = 0;
	virtual int editorScale() = 0;
	virtual int editorRoot() = 0;
	virtual int editorLength() = 0;
	virtual int editorStep() = 0;
	// Primer paso de la ventana que suena. Con la página activa en la 1 es 0, y entonces
	// todo se comporta como siempre; movida, el bucle empieza en otro sitio del patrón.
	virtual int editorStartStep() = 0;
	// Página visible, 0..ACID_PAGES-1. El editor enseña dieciséis pasos de los sesenta y
	// cuatro, y todo lo que dibuja o edita se desplaza con ella.
	virtual int editorPage() = 0;
	virtual int editorOctaveBase() = 0;
	virtual void editorSubmit(const AcidPatternV4& pattern) = 0;
};

struct AcidEditor : OpaqueWidget {
	AcidEditorHost* host = NULL;

	// --- geometría, en mm dentro de la propia caja -------------------------
	static constexpr float GUTTER = 7.0f;    // el tecladito vertical, a la izquierda
	static constexpr float ROLL_H = 34.0f;
	static constexpr float ROW_H = 4.1f;
	static constexpr float ROW_GAP = 0.3f;
	static constexpr float ROLL_GAP = 1.5f;

	enum Row { ROW_UP, ROW_DOWN, ROW_GATE, ROW_ACCENT, ROW_SLIDE, ROW_LEN };
	static const char* rowName(int r) {
		static const char* N[ROW_LEN] = {"UP", "DOWN", "GATE", "ACC", "SLIDE"};
		return N[r];
	}

	// --- copia de trabajo ---------------------------------------------------
	AcidPatternV4 work;
	uint32_t seenVersion = 0;
	bool haveWork = false;

	// --- estado de arrastre -------------------------------------------------
	// Un arrastre se queda en la fila donde empezó y no repite sobre el mismo paso: así
	// pintar una tirada de notas es un gesto, y no un parpadeo de encendido y apagado.
	int dragRegion = -1;      // -1 ninguno, 0 el roll, 1..5 las filas
	int dragRow = -1;         // fila del roll donde empezó, para arrastrar en horizontal
	int dragLastStep = -1;
	int dragLastRow = -1;     // última fila del roll aplicada, para arrastrar en vertical
	// Con el botón derecho el gesto quita en vez de poner, y lo sigue haciendo mientras se
	// arrastra: es el trato de cualquier piano roll.
	bool removing = false;
	bool dragErases = false;  // el roll borra si el gesto empezó sobre una nota ya puesta

	// El roll enseña la octava entera: doce semitonos. Las filas que la escala no contiene
	// salen apagadas y no aceptan clic, con el mismo lenguaje que las celdas no disponibles
	// de las filas de abajo. Enseñar solo los grados dejaba un teclado de cinco teclas que
	// parecía roto, y perdía la referencia visual de dónde cae cada nota dentro de la octava.
	static const int ROLL_ROWS = 12;

	float rollRowHeight() { return ROLL_H / (float) ROLL_ROWS; }

	// Índice del grado que suena en ese semitono, o -1 si la escala no lo contiene.
	// Grado de la escala cuyo semitono cae más cerca de `semi`, para que un clic en una
	// fila que la escala no tiene coloque la nota vecina en vez de no hacer nada. En
	// empate gana la grave, y no se busca al otro lado de la octava: cuantizar no debe
	// mover la nota de octava sin que nadie lo haya pedido.
	static int nearestDegree(int semi, int scaleIdx) {
		const AcidScale& sc = ACID_SCALES[scaleIdx];
		int best = -1, bestDist = 0;
		for (int i = 0; i < sc.n; i++) {
			const int d = std::abs(sc.s[i] - semi);
			if (best < 0 || d < bestDist) { best = i; bestDist = d; }
		}
		return best;
	}

	static int degreeOfSemi(int semi, int scaleIdx) {
		const AcidScale& sc = ACID_SCALES[scaleIdx];
		for (int i = 0; i < sc.n; i++)
			if (sc.s[i] == semi) return i;
		return -1;
	}

	int scaleSize() {
		const int si = host ? host->editorScale() : 0;
		return ACID_SCALES[si < 0 ? 0 : (si >= ACID_SCALES_LEN ? 0 : si)].n;
	}

	int scaleIdx() {
		const int si = host ? host->editorScale() : 0;
		return (si < 0 || si >= ACID_SCALES_LEN) ? 0 : si;
	}

	float gridWidth() const { return box.size.x - mm2px(GUTTER); }
	float gridX() const { return mm2px(GUTTER); }

	// Primer paso de la página visible. Todo el widget trabaja en columnas 0..15 y sólo
	// convierte a paso absoluto al tocar el patrón.
	int pageBase() {
		const int page = host ? host->editorPage() : 0;
		const int clamped = (page < 0) ? 0 : (page >= ACID_PAGES ? ACID_PAGES - 1 : page);
		return clamped * ACID_PAGE_STEPS;
	}

	float rowTop(int r) const {
		return mm2px(ROLL_H + ROLL_GAP + (float) r * (ROW_H + ROW_GAP));
	}

	// Posición de un paso dentro de la ventana que suena, dando la vuelta al patrón.
	static int loopPos(int step, int start) {
		const int d = step - start;
		return (d < 0) ? d + ACID_MAX_STEPS : d;
	}
	static bool inLoop(int step, int start, int length) {
		return step >= 0 && step < ACID_MAX_STEPS && loopPos(step, start) < length;
	}
	// El paso que suena después de éste, dentro de la ventana.
	static int nextInLoop(int step, int start, int length) {
		if (length <= 0) return step;
		return (start + (loopPos(step, start) + 1) % length) % ACID_MAX_STEPS;
	}

	// La regla de slide que aplica `sanitize()`, replicada para poder dibujarla antes de
	// que el usuario pulse: el paso siguiente tiene que atacar. La altura da igual —dos
	// notas iguales unidas por un slide suenan legato, que es lo que hace un 303—.
	static bool slideAllowed(const AcidPatternV4& p, int step, int start, int length) {
		if (length <= 0 || !inLoop(step, start, length)) return false;
		if (p.time[step] != AcidTimeState::Note) return false;
		return p.time[nextInLoop(step, start, length)] == AcidTimeState::Note;
	}

	// --- sincronía con el módulo -------------------------------------------
	// Se llama al dibujar y antes de cada edición: si el patrón cambió por debajo, la
	// copia de trabajo se tira y se vuelve a coger la buena.
	void syncWork() {
		if (!host) return;
		const uint32_t version = host->editorVersion();
		// Comparación con signo sobre la resta: sólo se resincroniza si el módulo va por
		// delante. Entre `submit()` y el momento en que el hilo de audio instala el patrón
		// hay uno o dos cuadros en los que el módulo todavía enseña el anterior; con una
		// comparación de desigualdad, el editor volvía atrás en esos cuadros —la edición
		// parpadeaba— y una edición encadenada se hacía sobre la copia vieja y se perdía.
		if (!haveWork || (int32_t) (version - seenVersion) > 0) {
			work = host->editorPattern();
			seenVersion = version;
			haveWork = true;
		}
	}

	void submit() {
		if (!host) return;
		host->editorSubmit(work);
		// La versión del módulo subirá al instalarlo; adelantarla aquí evita que el
		// siguiente movimiento del ratón resincronice contra el patrón todavía viejo y
		// pierda la edición que acabamos de hacer.
		seenVersion = host->editorVersion() + 1;
	}

	// --- geometría inversa: de píxeles a celda ------------------------------
	int stepAt(float x) {
		const float w = gridWidth() / (float) ACID_PAGE_STEPS;
		if (w <= 0.f) return -1;
		const int c = (int) std::floor((x - gridX()) / w);
		if (c < 0 || c >= ACID_PAGE_STEPS) return -1;
		const int s = pageBase() + c;
		return (s < 0 || s >= ACID_MAX_STEPS) ? -1 : s;
	}

	// Devuelve el semitono de la fila bajo el ratón, 0..11, o -1 fuera del roll.
	int rollRowAt(float y) {
		const float h = mm2px(ROLL_H) / (float) ROLL_ROWS;
		if (h <= 0.f) return -1;
		// La fila 0 es la tónica y va abajo, como en cualquier piano roll.
		const int fromTop = (int) std::floor(y / h);
		if (fromTop < 0 || fromTop >= ROLL_ROWS) return -1;
		return ROLL_ROWS - 1 - fromTop;
	}

	int regionAt(Vec p) const {
		if (p.x < gridX()) return -1;
		if (p.y < mm2px(ROLL_H)) return 0;
		for (int r = 0; r < ROW_LEN; r++) {
			const float top = rowTop(r);
			if (p.y >= top && p.y < top + mm2px(ROW_H)) return 1 + r;
		}
		return -1;
	}

	// --- edición ------------------------------------------------------------
	void applyAt(Vec p, bool firstTouch) {
		if (!host) return;
		const int step = stepAt(p.x);
		if (step < 0) return;
		const int length = host->editorLength();
		// Fuera del bucle activo no se edita.
		if (!inLoop(step, host->editorStartStep(), length)) return;
		const int region = firstTouch ? regionAt(p) : dragRegion;
		if (region < 0) return;
		// En el roll, el gesto sigue al ratón también en vertical: cambiar de fila sin
		// cambiar de paso mueve la nota arriba o abajo. Fuera del roll —y borrando, donde
		// la fila da igual— sólo cuenta el paso.
		const bool followsRow = region == 0 && !removing && !dragErases;
		const int row = followsRow ? rollRowAt(p.y) : -1;
		if (!firstTouch && step == dragLastStep && row == dragLastRow) return;

		syncWork();
		const int si = scaleIdx();

		if (removing) {
			// Quitar es lo mismo en todo el ancho del roll: da igual la fila en la que caiga
			// el ratón, el paso se apaga. Así se barre una tirada de notas de un gesto.
			if (region == 0 || region - 1 == ROW_GATE)
				AcidPatternEdit::setTime(work, step, AcidTimeState::Rest, si);
			else switch (region - 1) {
				case ROW_UP:
				case ROW_DOWN:   AcidPatternEdit::setOctave(work, step, 0, si); break;
				case ROW_ACCENT: AcidPatternEdit::setAccent(work, step, false, si); break;
				default:         AcidPatternEdit::setSlide(work, step, false, si); break;
			}
			if (firstTouch) dragRegion = region;
			dragLastStep = step;
			dragLastRow = row;
			submit();
			return;
		}

		if (region == 0) {
			// Si el puntero se sale del roll por arriba o por abajo se conserva la última
			// fila buena, para que salirse un píxel no rompa la tirada.
			const int semi = (row >= 0) ? row : (firstTouch ? rollRowAt(p.y) : dragRow);
			if (semi < 0) return;
			// Fila que la escala no contiene: el clic vale igual y la nota cae en el grado
			// vecino. Se pinta en cualquier parte del roll, pero lo que suena sigue siendo
			// de la escala; cambiarla porque el ratón cayó ahí reinterpretaría el patrón
			// entero sin avisar.
			int degree = degreeOfSemi(semi, si);
			if (degree < 0) degree = nearestDegree(semi, si);
			if (degree < 0) return;
			// La fila viva se guarda ya cuantizada: si el ratón se sale del roll, la
			// tirada sigue por la última que valía en vez de saltar.
			dragRow = ACID_SCALES[si].s[degree];
			if (firstTouch) {
				// Clic sobre la nota que ya estaba ahí: el gesto borra en vez de poner.
				const int event = AcidPatternEdit::eventAt(work, step);
				dragErases = event >= 0
				          && AcidPatternEdit::rowOfDegree(work.pitch[event].degree, si) == degree;
			}
			if (dragErases)
				AcidPatternEdit::setTime(work, step, AcidTimeState::Rest, si);
			else
				AcidPatternEdit::setRow(work, step, degree, si);
		}
		else {
			switch (region - 1) {
				case ROW_UP:     AcidPatternEdit::bumpOctave(work, step, +1, si); break;
				case ROW_DOWN:   AcidPatternEdit::bumpOctave(work, step, -1, si); break;
				case ROW_GATE:   AcidPatternEdit::cycleTime(work, step, si); break;
				case ROW_ACCENT: AcidPatternEdit::toggleAccent(work, step, si); break;
				default:         AcidPatternEdit::toggleSlide(work, step, si); break;
			}
		}
		if (firstTouch) dragRegion = region;
		dragLastStep = step;
		dragLastRow = row;
		submit();
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			removing = false;
			applyAt(e.pos, true);
			e.consume(this);
			return;
		}
		// El derecho borra, pero sólo si cae sobre una celda de verdad: en el tecladito y
		// en los huecos entre filas no se consume, y ahí el menú contextual sigue saliendo.
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT
		    && stepAt(e.pos.x) >= 0 && regionAt(e.pos) >= 0) {
			removing = true;
			applyAt(e.pos, true);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}

	// El pintado va por DragHover y no por DragMove: DragMove sólo trae el desplazamiento,
	// y reconstruirlo contra el ratón de la rack sale mal en cuanto el patch está
	// desplazado o con zoom, que es por lo que arrastrar no pintaba. DragHover trae la
	// posición ya en coordenadas del widget.
	void onDragHover(const event::DragHover& e) override {
		if (e.origin == this && dragRegion >= 0)
			applyAt(e.pos, false);
		OpaqueWidget::onDragHover(e);
	}

	void onDragEnd(const event::DragEnd& e) override {
		dragRegion = -1;
		dragRow = -1;
		dragLastStep = -1;
		dragLastRow = -1;
		dragErases = false;
		removing = false;
		OpaqueWidget::onDragEnd(e);
	}

	// --- dibujo -------------------------------------------------------------
	void draw(const DrawArgs& args) override {
		// Fondo del editor: algo más oscuro que el panel, como una pantalla empotrada.
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, mm2px(1.2f));
		nvgFillColor(args.vg, nvgRGB(0x16, 0x18, 0x1C));
		nvgFill(args.vg);
		OpaqueWidget::draw(args);
	}

	// Todo lo que brilla va en la capa 1, que es la que Rack ilumina de noche.
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			OpaqueWidget::drawLayer(args, layer);
			return;
		}
		syncWork();
		const int si = scaleIdx();
		const int length = host ? host->editorLength() : ACID_MAX_STEPS;
		const int start = host ? host->editorStartStep() : 0;
		const int playing = host ? host->editorStep() : -1;
		const AcidPatternV4& p = haveWork ? work : dummy();

		const float gx = gridX();
		const float sw = gridWidth() / (float) ACID_PAGE_STEPS;
		const int base = pageBase();
		// Cuántas columnas de esta página caen dentro del bucle: el resto son las que se
		// enseñan apagadas porque existen pero no suenan. Se cuenta desde la página activa,
		// que es por donde arranca la secuencia y no tiene por qué ser la primera.
		const int fromStart = length - loopPos(base, start);
		const int live = (fromStart < 0) ? 0
		               : (fromStart > ACID_PAGE_STEPS ? ACID_PAGE_STEPS : fromStart);
		const float rh = mm2px(ROLL_H) / (float) ROLL_ROWS;

		std::shared_ptr<window::Font> font = APP->window->uiFont;

		// --- fondo del roll: la octava entera, con lo que no es de la escala hundido
		for (int r = 0; r < ROLL_ROWS; r++) {
			const bool inScale = degreeOfSemi(r, si) >= 0;
			const float y = (float) (ROLL_ROWS - 1 - r) * rh;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, gx, y, gridWidth(), rh - 0.4f);
			// La tónica se marca: es la referencia que se busca de un vistazo.
			// El carril que la escala no tiene se hunde, pero no se apaga del todo: se
			// puede pintar en él, y lo que caiga ahí se va al grado vecino.
			NVGcolor bg;
			if (!inScale)     bg = nvgRGB(0x11, 0x13, 0x18);   // fuera de la escala: hundida
			else if (r == 0)  bg = nvgRGB(0x35, 0x3C, 0x49);   // tónica
			else              bg = nvgRGB(0x28, 0x2D, 0x37);
			nvgFillColor(args.vg, bg);
			nvgFill(args.vg);
		}
		// Las columnas fuera de PASOS se apagan: enseñan que existen pero no suenan.
		if (live < ACID_PAGE_STEPS) {
			nvgBeginPath(args.vg);
			nvgRect(args.vg, gx + live * sw, 0.f, (ACID_PAGE_STEPS - live) * sw,
			        mm2px(ROLL_H));
			nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 150));
			nvgFill(args.vg);
		}
		// Rejilla vertical, con el tiempo fuerte marcado cada cuatro pasos.
		for (int c = 0; c <= ACID_PAGE_STEPS; c++) {
			const bool beat = (c % 4) == 0;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, gx + c * sw, 0.f);
			nvgLineTo(args.vg, gx + c * sw, mm2px(ROLL_H));
			nvgStrokeColor(args.vg, beat ? nvgRGBA(0x9A, 0xA2, 0xB5, 90)
			                             : nvgRGBA(0x9A, 0xA2, 0xB5, 35));
			nvgStrokeWidth(args.vg, beat ? 1.0f : 0.6f);
			nvgStroke(args.vg);
		}

		// --- el tecladito vertical --------------------------------------------
		// Una tecla por fila, no doce: las filas son grados de la escala, así que un
		// teclado cromático de verdad no cuadraría con ninguna. Cada tecla se pinta blanca
		// o negra según lo sea esa nota en un piano, que es lo que hace que se lea como un
		// teclado aunque falten las que la escala no tiene.
		{
			const int root = host ? host->editorRoot() : 0;
			const int base = host ? host->editorOctaveBase() : -2;
			const float kbW = mm2px(GUTTER) - mm2px(0.6f);
			// Primero la franja blanca entera y después las negras encima, como se monta un
			// teclado de verdad: así el blanco es blanco de lado a lado y las negras son
			// bloques macizos, sin el gris de fondo asomando entre tecla y tecla.
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0.f, 0.f, kbW, mm2px(ROLL_H));
			nvgFillColor(args.vg, nvgRGB(0xFF, 0xFF, 0xFF));
			nvgFill(args.vg);
			for (int r = 0; r < ROLL_ROWS; r++) {
				const bool inScale = degreeOfSemi(r, si) >= 0;
				const int semi = r + root;
				const int pc = ((semi % 12) + 12) % 12;
				// Las cinco teclas negras del piano.
				const bool black = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
				const float y = (float) (ROLL_ROWS - 1 - r) * rh;
				// La negra es más corta, como en un teclado de verdad.
				const float kw = black ? kbW * 0.58f : kbW;
				// Blanca blanca y negra negra: el teclado tiene que leerse como un teclado de
				// un vistazo. La que la escala no contiene sólo se atenúa un punto —sigue
				// estando, es una octava de piano de verdad— sin dejar de ser blanca o negra.
				// Atenuada dice "esta no es de la escala", no "aquí no se puede pintar".
				if (black || !inScale) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, 0.f, y, kw, rh);
					nvgFillColor(args.vg, black ? (inScale ? nvgRGB(0x00, 0x00, 0x00)
					                                       : nvgRGB(0x22, 0x25, 0x2B))
					                            : nvgRGB(0xBA, 0xC0, 0xC9));
					nvgFill(args.vg);
				}
				// Separación entre teclas: una línea fina, no un hueco. El hueco convertía
				// el teclado en una hilera de pastillas. Sólo se raya el tramo de blanco que
				// se ve: donde la tecla de abajo es negra, la negra ya separa por sí sola.
				if (!black) {
					const int pcBelow = ((semi - 1) % 12 + 12) % 12;
					const bool blackBelow = (pcBelow == 1 || pcBelow == 3 || pcBelow == 6
					                         || pcBelow == 8 || pcBelow == 10);
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, blackBelow ? kbW * 0.58f : 0.f, y + rh);
					nvgLineTo(args.vg, kbW, y + rh);
					nvgStrokeColor(args.vg, nvgRGBA(0x60, 0x66, 0x70, 200));
					nvgStrokeWidth(args.vg, 0.7f);
					nvgStroke(args.vg);
				}
				// Canto derecho azul en las filas que la escala contiene, pegado a la rejilla,
				// que es donde está el ratón: dice de un vistazo dónde cae la nota tal cual y
				// dónde se cuantizará al grado vecino.
				if (inScale) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, kbW - mm2px(0.45f), y, mm2px(0.45f), rh);
					nvgFillColor(args.vg, AnimatekUI::logoBlue());
					nvgFill(args.vg);
				}
				// La tónica lleva su marca: es la referencia que se busca de un vistazo.
				if (r == 0 && inScale) {
					nvgBeginPath(args.vg);
					nvgRect(args.vg, 0.f, y, mm2px(0.5f), rh);
					nvgFillColor(args.vg, AnimatekUI::logoBlue());
					nvgFill(args.vg);
				}
				if (!font) continue;
				// Con doce teclas no cabe el nombre en todas. Se rotulan las blancas, que son
				// las que dan la referencia, y la tónica lleva además su octava real: sin ella
				// el roll no dice si ese C es un C2 o un C3.
				if (black) continue;
				char text[8];
				const int octave = base + 4 + (semi < 0 ? -1 : 0) + (semi / 12);
				if (r == 0)
					std::snprintf(text, sizeof(text), "%s%d", ACID_NOTE_NAMES[pc], octave);
				else
					std::snprintf(text, sizeof(text), "%s", ACID_NOTE_NAMES[pc]);
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, 6.f);
				nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
				nvgFillColor(args.vg, inScale ? nvgRGB(0x18, 0x1A, 0x1E) : nvgRGB(0x7A, 0x80, 0x8A));
				nvgText(args.vg, kbW - mm2px(0.9f), y + rh * 0.5f, text, NULL);
			}
			// Canto derecho del teclado, donde acaba y empieza la rejilla.
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, kbW, 0.f);
			nvgLineTo(args.vg, kbW, mm2px(ROLL_H));
			nvgStrokeColor(args.vg, nvgRGBA(0x00, 0x00, 0x00, 200));
			nvgStrokeWidth(args.vg, 0.8f);
			nvgStroke(args.vg);
		}

		// --- notas ------------------------------------------------------------
		// Una pasada previa saca la fila y la octava de cada paso: la usan tanto los
		// bloques como las líneas de slide y las filas de abajo.
		int noteRow[ACID_MAX_STEPS];
		int noteOct[ACID_MAX_STEPS];
		bool noteAcc[ACID_MAX_STEPS], noteSld[ACID_MAX_STEPS];
		for (int s = 0; s < ACID_MAX_STEPS; s++) {
			noteRow[s] = -1; noteOct[s] = 0; noteAcc[s] = false; noteSld[s] = false;
		}
		{
			int event = 0;
			int lastRow = -1, lastOct = 0;
			for (int s = 0; s < p.timeLength && s < ACID_MAX_STEPS; s++) {
				if (p.time[s] == AcidTimeState::Note && event < p.pitchLength) {
					const AcidPitchEvent& e = p.pitch[event++];
					// La fila es el semitono dentro de la octava, no el índice del grado:
					// el roll ya es una octava de piano completa.
					lastRow = ACID_SCALES[si].s[AcidPatternEdit::rowOfDegree(e.degree, si)];
					lastOct = AcidPatternEdit::totalOctave(e, si);
					noteRow[s] = lastRow;
					noteOct[s] = lastOct;
					noteAcc[s] = e.accent;
					noteSld[s] = e.slideOut;
				}
				else if (p.time[s] == AcidTimeState::Tie && lastRow >= 0) {
					// El tie ocupa la misma altura: se pinta como continuación, sin ataque.
					noteRow[s] = lastRow;
					noteOct[s] = lastOct;
				}
			}
		}

		for (int col = 0; col < ACID_PAGE_STEPS; col++) {
			const int s = base + col;
			if (s >= ACID_MAX_STEPS) break;
			if (noteRow[s] < 0 || noteRow[s] >= ROLL_ROWS) continue;
			const bool tie = p.time[s] == AcidTimeState::Tie;
			const float y = (float) (ROLL_ROWS - 1 - noteRow[s]) * rh;
			const float x = gx + col * sw;
			// Mismo código de color que los LEDs de paso, para que panel y editor digan
			// lo mismo: verde ataque, rojo acento, ámbar slide, azul tie.
			NVGcolor c = tie            ? nvgRGB(0x1A, 0x73, 0xFF)
			           : noteAcc[s]     ? nvgRGB(0xFF, 0x26, 0x26)
			           : noteSld[s]     ? nvgRGB(0xCC, 0xB3, 0x00)
			                            : nvgRGB(0x2E, 0xCC, 0x40);
			if (!inLoop(s, start, length)) c = nvgTransRGBA(c, 70);
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, x + 1.f, y + 1.f, sw - 2.f, rh - 2.5f, 1.5f);
			nvgFillColor(args.vg, c);
			nvgFill(args.vg);
			// Con doce filas el bloque mide poco más de dos milímetros y medio: el número de
			// octava ya no cabe dentro. Lo dicen las filas UP y DOWN, que para eso están.
		}

		// --- líneas de slide: el gesto melódico, como en un piano roll -------
		for (int c = 0; c < ACID_PAGE_STEPS; c++) {
			const int s = base + c;
			if (s >= ACID_MAX_STEPS) break;
			if (!inLoop(s, start, length)) continue;
			if (!noteSld[s] || noteRow[s] < 0) continue;
			const int next = nextInLoop(s, start, length);
			if (noteRow[next] < 0) continue;
			// El slide que salta a la página siguiente no se dibuja aquí: su destino no
			// está en pantalla y la línea acabaría en un borde que no significa nada.
			if (next - base >= ACID_PAGE_STEPS) continue;
			const float y0 = (float) (ROLL_ROWS - 1 - noteRow[s]) * rh + rh * 0.5f;
			const float y1 = (float) (ROLL_ROWS - 1 - noteRow[next]) * rh + rh * 0.5f;
			// Un slide al paso 0 daría una línea que cruza el editor entero; se dibuja
			// solo el que avanza, que es el que se ve como movimiento.
			if (next < s) continue;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, gx + (c + 0.85f) * sw, y0);
			nvgLineTo(args.vg, gx + (next - base + 0.15f) * sw, y1);
			nvgStrokeColor(args.vg, nvgRGBA(0xCC, 0xB3, 0x00, 220));
			nvgStrokeWidth(args.vg, 1.4f);
			nvgStroke(args.vg);
		}

		// --- cabeza de reproducción ------------------------------------------
		if (playing >= base && playing < base + ACID_PAGE_STEPS) {
			nvgBeginPath(args.vg);
			nvgRect(args.vg, gx + (playing - base) * sw, 0.f, sw, mm2px(ROLL_H));
			nvgFillColor(args.vg, AnimatekUI::logoBlue(45));
			nvgFill(args.vg);
		}

		// --- filas de abajo ---------------------------------------------------
		for (int r = 0; r < ROW_LEN; r++) {
			const float top = rowTop(r);
			const float h = mm2px(ROW_H);
			if (font) {
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, 6.2f);
				nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
				nvgFillColor(args.vg, nvgRGBA(0x9A, 0xA2, 0xB5, 170));
				nvgText(args.vg, gx - mm2px(0.8f), top + h * 0.5f, rowName(r), NULL);
			}
			for (int col = 0; col < ACID_PAGE_STEPS; col++) {
				const int s = base + col;
				if (s >= ACID_MAX_STEPS) break;
				const float x = gx + col * sw;
				const bool sounds = inLoop(s, start, length);
				const bool hasNote = p.time[s] == AcidTimeState::Note;

				// Qué celdas aceptan un clic de verdad. Enseñarlo evita el peor defecto de
				// una rejilla: pulsar y que no pase nada sin saber por qué. UP, DOWN, ACC y
				// SLIDE solo tienen sentido sobre un ataque; el slide además necesita que el
				// paso siguiente ataque a otra altura, que es la condición que el
				// secuenciador impone al reproducir.
				bool usable = sounds;
				if (r != ROW_GATE) usable = usable && hasNote;
				if (r == ROW_SLIDE && usable) usable = slideAllowed(p, s, start, length);

				// Cada celda es un botón: primero el hueco, después el estado encendido.
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, x + 1.f, top + 0.8f, sw - 2.f, h - 1.6f, 1.5f);
				nvgFillColor(args.vg, usable ? nvgRGB(0x22, 0x25, 0x2B)
				                             : nvgRGB(0x18, 0x19, 0x1D));
				nvgFill(args.vg);

				float level = 0.f;          // 0 apagado, 0.55 medio, 1 lleno
				NVGcolor c = nvgRGB(0x2E, 0xCC, 0x40);
				switch (r) {
					case ROW_UP:
						if (hasNote && noteOct[s] > 0) { level = noteOct[s] > 1 ? 1.f : 0.55f; }
						c = AnimatekUI::displayBlue();
						break;
					case ROW_DOWN:
						if (hasNote && noteOct[s] < 0) { level = noteOct[s] < -1 ? 1.f : 0.55f; }
						c = AnimatekUI::displayBlue();
						break;
					case ROW_GATE:
						if (p.time[s] == AcidTimeState::Note) { level = 1.f; c = nvgRGB(0x2E, 0xCC, 0x40); }
						else if (p.time[s] == AcidTimeState::Tie) { level = 1.f; c = nvgRGB(0x1A, 0x73, 0xFF); }
						break;
					case ROW_ACCENT:
						if (hasNote && noteAcc[s]) level = 1.f;
						c = nvgRGB(0xFF, 0x26, 0x26);
						break;
					default:
						if (hasNote && noteSld[s]) level = 1.f;
						c = nvgRGB(0xCC, 0xB3, 0x00);
						break;
				}
				if (level > 0.f) {
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, x + 1.f, top + 0.8f, sw - 2.f, h - 1.6f, 1.5f);
					nvgFillColor(args.vg, nvgTransRGBA(c, (int) (level * (sounds ? 255 : 90))));
					nvgFill(args.vg);
				}
			}
			// La cabeza de reproducción cruza también las filas de articulación.
			if (playing >= base && playing < base + ACID_PAGE_STEPS) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, gx + (playing - base) * sw, top, sw, h);
				nvgFillColor(args.vg, AnimatekUI::logoBlue(40));
				nvgFill(args.vg);
			}
		}

		OpaqueWidget::drawLayer(args, layer);
	}

	// Patrón vacío para el navegador de módulos, donde no hay instancia. Sin esto el
	// editor saldría en blanco en la web de la librería, que es justo lo que revisa el
	// control automático de VCV.
	static const AcidPatternV4& dummy() {
		static AcidPatternV4 d;
		static bool built = false;
		if (!built) {
			d.clear();
			d.timeLength = ACID_MAX_STEPS;
			// Una línea acid corta con todo lo que el editor sabe dibujar: acentos, un tie,
			// slides y saltos de octava, para que el navegador enseñe el módulo vivo.
			static const int demo[8] = {0, 2, 4, 5, 8, 10, 12, 14};
			for (int i = 0; i < 8; i++) d.time[demo[i]] = AcidTimeState::Note;
			d.time[6] = AcidTimeState::Tie;
			d.pitchLength = 8;
			static const int8_t deg[8] = {0, 0, 2, 1, 0, 3, 0, 2};
			static const int8_t oct[8] = {0, 1, 0, 0, -1, 0, 1, 0};
			for (int i = 0; i < 8; i++) {
				d.pitch[i].degree = deg[i];
				d.pitch[i].octave = oct[i];
				d.pitch[i].accent = (i == 0 || i == 4 || i == 5);
				d.pitch[i].slideOut = (i == 1 || i == 4);
			}
			// Saneado como cualquier patrón real: si no, el navegador enseñaría slides que
			// la reproducción descarta, y el editor estaría mintiendo justo en la vista que
			// más gente ve.
			d.sanitize(0);
			built = true;
		}
		return d;
	}
};
