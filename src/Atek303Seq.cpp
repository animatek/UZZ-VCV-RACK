#include "plugin.hpp"
#include "ui/AtekWidgets.hpp"
#include "ui/AcidEditor.hpp"
#include "AcidPattern.hpp"
#include "AcidMidiImport.hpp"

#include <osdialog.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// ATEK303 SEQ — generador de patrones acid.
//
// No es un secuenciador de pasos: no hay knobs ni switches por paso. Hay un botón
// GENERATE, unos mandos de carácter y un algoritmo. El modelo dual v4 vive en
// `AcidPattern.hpp`, aparte y sin dependencias de Rack, para poder probarlo en el banco
// offline (`tools/pattern_v4.cpp`); aquí queda el módulo: reloj, gate, salidas y panel.
// ---------------------------------------------------------------------------

static const int MAX_STEPS = ACID_MAX_STEPS;

struct Atek303Seq : Module, AcidEditorHost {
	enum PatternAction { ACTION_NONE, MUTATE_TIME, MUTATE_PITCH, MUTATE_ARTICULATION,
	                     UNDO_MUTATION, ACTION_GENERATE, ACTION_CLEAR };
	enum ParamId {
		GENERATE_PARAM, STEPS_PARAM, GATELEN_PARAM, DENSITY_PARAM, RANGE_PARAM,
		ACCENT_PARAM, SLIDEAMT_PARAM, ROOT_PARAM, SCALE_PARAM,
		// Los IDs nuevos se añaden al final: los patches v3/v4 conservan intactos 0..8.
		SEED_LOCK_PARAM, MUTATE_TIME_PARAM, MUTATE_PITCH_PARAM, MUTATE_ARTICULATION_PARAM,
		VIEW_PARAM,
		PARAMS_LEN
	};
	static_assert(GENERATE_PARAM == 0 && SCALE_PARAM == 8 && SEED_LOCK_PARAM == 9,
	              "No cambiar los IDs históricos de parámetros de ATEK303SEQ");
	enum InputId { CLOCK_INPUT, RESET_INPUT, GEN_INPUT, INPUTS_LEN };
	enum OutputId { VOCT_OUTPUT, GATE_OUTPUT, ACCENT_OUTPUT, SLIDE_OUTPUT, EOC_OUTPUT, OUTPUTS_LEN };
	enum LightId {
		ENUMS(STEP_LIGHT, ACID_PAGE_STEPS * 3),
		GENERATE_LIGHT, SEED_LOCK_LIGHT, MUTATE_TIME_LIGHT, MUTATE_PITCH_LIGHT,
		MUTATE_ARTICULATION_LIGHT, VIEW_LIGHT,
		// Una por página: azul la que se edita, verde la que suena, y las dos a la vez
		// cuando coinciden.
		ENUMS(PAGE_LIGHT, ACID_PAGES * 3),
		LIGHTS_LEN
	};

	AcidPatternV4 pattern;
	AcidDualGenerator generator;
	// Vista temporal derivada. Mantenerla permite que el motor de audio, los LEDs y la
	// comunicación con ATEK303 sigan siendo simples mientras el patrón ya vive en dos capas.
	AcidGen gen;
	uint32_t patternSeed = 1u;
	bool seedLocked = false;
	AcidGenParams generatedWith;
	AcidPatternV4 undoPattern;
	uint32_t mutationCounter = 0;
	uint32_t undoMutationCounter = 0;
	bool hasUndo = false;
	std::atomic<int> pendingPatternAction {ACTION_NONE};

	// Patrones leídos de un fichero MIDI. Se guardan enteros para poder saltar de compás
	// sin volver a abrir el fichero, y el patrón entra en el motor por el mismo camino
	// diferido que las mutaciones: la conversión ocurre en el hilo de la interfaz y el
	// hilo de audio solo copia.
	// Sube cada vez que el hilo de audio instala un patrón nuevo. Es lo que le dice al
	// editor que su copia de trabajo se quedó vieja porque alguien pulsó GENERATE, mutó o
	// importó un fichero.
	std::atomic<uint32_t> patternVersion {1};

	std::vector<AcidPatternV4> midiBars;
	std::string midiPath;
	std::string midiName;
	std::string midiStatus;
	// Motivo del último fallo al abrir. Separado de `midiStatus` para que un fichero que no
	// se puede leer no deje su error colgando bajo el nombre del que sí está cargado.
	std::string midiError;
	int midiBar = 0;
	// Un único hueco de entrega para todo lo que llega desde la interfaz: la importación
	// de MIDI y cada edición del piano roll. Se construye en el hilo gráfico y el de audio
	// lo instala; el patrón son unos 150 bytes, así que copiarlo entero por gesto sale
	// gratis y evita una cola de operaciones que habría que mantener.
	AcidPatternV4 pendingPattern;
	std::atomic<bool> hasPendingPattern {false};

	dsp::SchmittTrigger clockTrig, resetTrig, genTrig;
	dsp::BooleanTrigger genButton, mutateTimeButton, mutatePitchButton, mutateArticulationButton;
	dsp::PulseGenerator eocPulse, genPulse, mutateTimePulse, mutatePitchPulse, mutateArticulationPulse;

	int step = 0;
	// Hasta que no llega el primer flanco no hay paso en curso: si no, el módulo suelta
	// una nota al cargarse y el primer clock se salta el paso 0.
	bool clockStarted = false;
	float stepTime = 0.f;
	float clockPeriod = 0.125f;
	float glideVoct = 0.f;
	bool prevSlide = false;

	// Nombre conservado por compatibilidad con el JSON de los patches existentes. Esta
	// opción solo cambia la convención de los slides; los ties reales siempre sostienen gate.
	bool legatoTies = false;
	// El glide de V/Oct va activo por defecto: sin él, el slide no se oye en ninguna voz
	// que no sea el ATEK303. Cuando el ATEK303 está pegado se desactiva solo, porque el
	// slide lo hace él con su red y hacerlo dos veces lo emborrona.
	bool internalGlide = true;
	// Página que enseña el editor, 0..3. Es estado de interfaz, no de sonido: el
	// secuenciador recorre el patrón entero pase lo que pase. Atómica porque la escribe
	// el hilo gráfico al pulsar una página y la lee el de audio para las luces.
	std::atomic<int> editPage {0};
	// Alcance de MUTATE. Por defecto, la página abierta: con sesenta y cuatro pasos,
	// mutarlo todo de golpe se lleva por delante lo que acabas de ajustar.
	std::atomic<bool> mutateWholePattern {false};
	// Con el seguimiento puesto, el editor va detrás de la cabeza de reproducción y
	// cambia de página solo. Se enciende con un doble clic en el botón de página.
	std::atomic<bool> followPlayhead {false};
	// Página por la que arranca la secuencia, 0..3. Esto sí es sonido: con PASOS en 16 y
	// la página 2 activa, la secuencia recorre los pasos 17 a 32 y no los cuatro primeros
	// compases. Doble clic en un LED de página la mueve.
	std::atomic<int> startPage {0};
	int octaveBase = -2;   // en VCV 0 V es C4; una línea de 303 vive por C2
	bool accentAsCV = false;
	float accentLevel = 8.f;
	float accentBase = 2.f;

	Atek303Seq() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(GENERATE_PARAM,
		             "New seed; with BLOCK, mutate time + notes/octaves + slides/accents");
		configParam(STEPS_PARAM, 1.f, (float) MAX_STEPS, (float) ACID_PAGE_STEPS, "Steps");
		getParamQuantity(STEPS_PARAM)->snapEnabled = true;
		configParam(GATELEN_PARAM, 0.05f, 1.f, 0.85f, "Gate length", " %", 0.f, 100.f);
		configParam(DENSITY_PARAM, 0.05f, 1.f, 0.65f, "Note density", " %", 0.f, 100.f);
		configParam(RANGE_PARAM, 0.f, 1.f, 0.45f, "Range", " %", 0.f, 100.f);
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.6f, "Accent density", " %", 0.f, 100.f);
		configParam(SLIDEAMT_PARAM, 0.f, 1.f, 0.5f, "Slide density", " %", 0.f, 100.f);

		std::vector<std::string> roots;
		for (int i = 0; i < 12; i++) roots.push_back(ACID_NOTE_NAMES[i]);
		configSwitch(ROOT_PARAM, 0.f, 11.f, 0.f, "Root", roots);
		std::vector<std::string> scales;
		for (int i = 0; i < ACID_SCALES_LEN; i++) scales.push_back(ACID_SCALES[i].name);
		configSwitch(SCALE_PARAM, 0.f, (float) (ACID_SCALES_LEN - 1), 0.f, "Scale", scales);
		configSwitch(SEED_LOCK_PARAM, 0.f, 1.f, 0.f,
		             "Switch GENERATE between new seed and full mutation",
		             {"GENERATE creates a new seed", "GENERATE mutates all three layers"});
		configButton(MUTATE_TIME_PARAM, "Mutate time (2 operations)");
		configButton(MUTATE_PITCH_PARAM, "Mutate notes and octaves (2 operations)");
		configButton(MUTATE_ARTICULATION_PARAM, "Mutate slides and accents (3 operations)");
		configSwitch(VIEW_PARAM, 0.f, 1.f, 0.f, "Panel view",
		             {"Generation controls", "Pattern editor"});

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(GEN_INPUT, "GENERATE; with BLOCK, mutate all three layers (trigger)");
		configOutput(VOCT_OUTPUT, "1V/oct");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(ACCENT_OUTPUT, "Accent");
		configOutput(SLIDE_OUTPUT, "Slide");
		configOutput(EOC_OUTPUT, "End of cycle");
		generate();
	}

	int length() { return clamp((int) std::round(params[STEPS_PARAM].getValue()), 1, MAX_STEPS); }
	int scaleIdx() { return clamp((int) std::round(params[SCALE_PARAM].getValue()), 0, ACID_SCALES_LEN - 1); }
	int rootSemi() { return clamp((int) std::round(params[ROOT_PARAM].getValue()), 0, 11); }

	AcidGenParams currentGenerationParams() {
		AcidGenParams p;
		p.steps = MAX_STEPS;
		p.scale = clamp((int) std::round(params[SCALE_PARAM].getValue()), 0, ACID_SCALES_LEN - 1);
		p.density = std::max(0.05f, params[DENSITY_PARAM].getValue());
		p.accent = params[ACCENT_PARAM].getValue();
		p.slide = params[SLIDEAMT_PARAM].getValue();
		p.range = params[RANGE_PARAM].getValue();
		p.tie = 0.4f;
		return p;
	}

	void renderPattern() {
		pattern.render(gen);
		// Único punto por el que pasa todo cambio de patrón —generar, mutar, deshacer,
		// importar, editar a mano—, así que es donde el editor se entera de que su copia
		// de trabajo se quedó atrás.
		patternVersion++;
	}

	void generate(bool forceNewSeed = false) {
		if (forceNewSeed || !seedLocked)
			patternSeed = random::u32();
		if (!patternSeed) patternSeed = 1u;
		generatedWith = currentGenerationParams();
		generator.generate(pattern, generatedWith, patternSeed);
		renderPattern();
		mutationCounter = 0;
		hasUndo = false;
	}

	// Vaciar el patrón para empezar a dibujar desde cero. Es destructivo y no tiene
	// deshacer propio, así que se apunta en el mismo hueco que las mutaciones: «Undo last
	// mutation» lo recupera. La semilla se conserva —sigue siendo ese patrón, vaciado— y
	// el paso 1 se queda con una nota, que es la invariante que sostiene `sanitize()`.
	void clearPattern() {
		undoPattern = pattern;
		undoMutationCounter = mutationCounter;
		hasUndo = true;
		pattern.clear();
		pattern.seed = patternSeed;
		pattern.sanitize(scaleIdx());
		renderPattern();
		mutationCounter = 0;
	}

	// Primer paso y final del tramo que muta: la página abierta, o el patrón entero si
	// así se ha pedido en el menú.
	void mutationRange(int& lo, int& hi) {
		if (mutateWholePattern.load()) { lo = 0; hi = MAX_STEPS; return; }
		lo = editorPage() * ACID_PAGE_STEPS;
		hi = lo + ACID_PAGE_STEPS;
	}

	bool mutatePattern(AcidPatternMutator::Layer layer) {
		const AcidPatternV4 before = pattern;
		const uint32_t beforeCounter = mutationCounter;
		const int operations = layer == AcidPatternMutator::Articulation ? 3 : 2;
		uint32_t lastMutationIndex = beforeCounter;
		int lo, hi;
		mutationRange(lo, hi);
		if (AcidPatternMutator::mutateBurst(pattern, layer, beforeCounter + 1,
		                                    operations, scaleIdx(), lastMutationIndex,
		                                    lo, hi)) {
			undoPattern = before;
			undoMutationCounter = beforeCounter;
			mutationCounter = lastMutationIndex;
			hasUndo = true;
			renderPattern();
			return true;
		}
		pattern = before;
		return false;
	}

	bool mutateAllLayers() {
		const AcidPatternV4 before = pattern;
		const uint32_t beforeCounter = mutationCounter;
		uint32_t cursor = beforeCounter;
		const AcidPatternMutator::Layer layers[3] = {
			AcidPatternMutator::Time,
			AcidPatternMutator::Pitch,
			AcidPatternMutator::Articulation
		};
		int lo, hi;
		mutationRange(lo, hi);
		for (int i = 0; i < 3; i++) {
			const int operations = layers[i] == AcidPatternMutator::Articulation ? 3 : 2;
			uint32_t lastMutationIndex = cursor;
			if (!AcidPatternMutator::mutateBurst(pattern, layers[i], cursor + 1,
			                                     operations, scaleIdx(), lastMutationIndex,
			                                     lo, hi)) {
				pattern = before;
				return false;
			}
			cursor = lastMutationIndex;
		}
		undoPattern = before;
		undoMutationCounter = beforeCounter;
		mutationCounter = cursor;
		hasUndo = true;
		renderPattern();
		return true;
	}

	// Lo que hace GENERATE, sea quien sea quien lo pida: el botón, la entrada de trigger o
	// el sticker acid del panel. Con la semilla bloqueada muta en vez de sortear otra.
	void generateOrMutate() {
		if (seedLocked) {
			if (mutateAllLayers()) {
				mutateTimePulse.trigger(0.12f);
				mutatePitchPulse.trigger(0.12f);
				mutateArticulationPulse.trigger(0.12f);
			}
		}
		else {
			generate();
		}
		genPulse.trigger(0.12f);
	}

	bool undoMutation() {
		if (!hasUndo) return false;
		pattern = undoPattern;
		mutationCounter = undoMutationCounter;
		hasUndo = false;
		renderPattern();
		genPulse.trigger(0.12f);
		return true;
	}

	// Abre y convierte el fichero. Corre en el hilo de la interfaz: aquí se puede leer de
	// disco y reservar memoria, cosa que en process() no.
	bool loadMidiFile(const std::string& path) {
		midiError.clear();
		MidiFileReader reader;
		MidiFileData data;
		if (!reader.load(path, data)) {
			midiError = reader.error;
			return false;
		}
		const AcidMidiImportResult result =
			AcidMidiImport::convert(data, scaleIdx(), rootSemi(), octaveBase);
		if (!result.ok()) {
			midiError = result.error.empty() ? "could not convert file" : result.error;
			return false;
		}
		midiBars = result.bars;
		midiPath = path;
		midiName = system::getFilename(path);
		midiBar = 0;

		// La escala solo se toca si el fichero no cabía en la que tiene puesta el panel:
		// Cromática es la única que representa cualquier altura sin reescribir la melodía.
		if (result.forcedChromatic)
			params[SCALE_PARAM].setValue((float) result.scaleIdx);

		midiStatus = string::f("%d bar%s", (int) midiBars.size(),
		                       midiBars.size() == 1 ? "" : "s");
		if (result.forcedChromatic) midiStatus += " · scale set to Chromatic";
		if (!result.hasAccents) midiStatus += " · no velocity accents";
		if (result.transposedOctaves)
			midiStatus += string::f(" · transposed %+d oct", result.transposedOctaves);
		if (result.droppedVoices)
			midiStatus += string::f(" · %d extra voice%s dropped", result.droppedVoices,
			                        result.droppedVoices == 1 ? "" : "s");
		if (result.clippedNotes)
			midiStatus += string::f(" · %d note%s out of range", result.clippedNotes,
			                        result.clippedNotes == 1 ? "" : "s");
		if (result.skippedSteps)
			midiStatus += string::f(" · skipped %d leading empty step%s",
			                        result.skippedSteps,
			                        result.skippedSteps == 1 ? "" : "s");
		applyMidiBar(0);
		return true;
	}

	// El patrón se entrega al hilo de audio, que es el único que toca `pattern`.
	void applyMidiBar(int index) {
		if (index < 0 || index >= (int) midiBars.size()) return;
		midiBar = index;
		pendingPattern = midiBars[(size_t) index];
		params[STEPS_PARAM].setValue((float) pendingPattern.timeLength);
		hasPendingPattern.store(true);
	}

	void clearMidiFile() {
		midiBars.clear();
		midiPath.clear();
		midiName.clear();
		midiStatus.clear();
		midiError.clear();
		midiBar = 0;
	}

	// --- AcidEditorHost: lo que el piano roll necesita saber del módulo ------
	const AcidPatternV4& editorPattern() override { return pattern; }
	uint32_t editorVersion() override { return patternVersion.load(); }
	int editorScale() override {
		const int v = (int) std::round(params[SCALE_PARAM].getValue());
		return (v < 0) ? 0 : (v >= ACID_SCALES_LEN ? ACID_SCALES_LEN - 1 : v);
	}
	int editorRoot() override {
		const int v = (int) std::round(params[ROOT_PARAM].getValue());
		return (v < 0) ? 0 : (v > 11 ? 11 : v);
	}
	int editorLength() override {
		const int v = (int) std::round(params[STEPS_PARAM].getValue());
		return (v < 1) ? 1 : (v > MAX_STEPS ? MAX_STEPS : v);
	}
	// Sin reloj todavía no hay paso en curso, y el editor no debe pintar cabeza.
	int editorStep() override { return clockStarted ? step : -1; }
	int editorStartStep() override { return startStep(); }
	int editorPage() override {
		// Siguiendo a la cabeza, la página la manda el secuenciador. Parado no hay
		// cabeza a la que seguir, así que se queda en la última que se estuvo mirando.
		if (followPlayhead.load() && clockStarted) {
			const int v = step / ACID_PAGE_STEPS;
			return (v < 0) ? 0 : (v >= ACID_PAGES ? ACID_PAGES - 1 : v);
		}
		const int v = editPage.load();
		return (v < 0) ? 0 : (v >= ACID_PAGES ? ACID_PAGES - 1 : v);
	}

	// Primer paso de la ventana que suena, en pasos absolutos.
	int startStep() const {
		const int v = startPage.load();
		return ((v < 0) ? 0 : (v >= ACID_PAGES ? ACID_PAGES - 1 : v)) * ACID_PAGE_STEPS;
	}
	// Posición dentro de la ventana. Si la cabeza quedó detrás del arranque —se acaba de
	// mover la página activa— cuenta dando la vuelta, no en negativo.
	static int relStep(int s, int base) {
		const int rel = s - base;
		return (rel < 0) ? rel + MAX_STEPS : rel;
	}
	// Avanza dentro de la ventana. La ventana da la vuelta dentro de los sesenta y cuatro
	// pasos: con PASOS largo y la página activa alta, el final enlaza con el principio en
	// vez de quedarse sin patrón.
	static int nextStep(int s, int len, int base) {
		const int rel = (relStep(s, base) + 1) % len;
		return (base + rel) % MAX_STEPS;
	}

	// Arrancar por otra página es cosa del sonido, no de la vista: la secuencia empieza
	// ahí y el editor se queda donde estaba.
	void setStartPage(int page) {
		startPage.store((page < 0) ? 0 : (page >= ACID_PAGES ? ACID_PAGES - 1 : page));
	}

	// Ir a una página concreta es dejar de seguir: se mira lo que uno quiere mirar.
	void showPage(int page) {
		followPlayhead.store(false);
		editPage.store((page < 0) ? 0 : (page >= ACID_PAGES ? ACID_PAGES - 1 : page));
	}
	// Para poder rotular las teclas con su octava real y no solo con el nombre de la nota.
	int editorOctaveBase() override { return octaveBase; }

	// La edición entra por el mismo hueco diferido que la importación: el hilo gráfico
	// nunca escribe `pattern`.
	void editorSubmit(const AcidPatternV4& edited) override {
		pendingPattern = edited;
		hasPendingPattern.store(true);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		step = 0;
		seedLocked = params[SEED_LOCK_PARAM].getValue() > 0.5f;
		generate();
	}

	void process(const ProcessArgs& args) override {
		seedLocked = params[SEED_LOCK_PARAM].getValue() > 0.5f;
		const bool genPressed = genButton.process(params[GENERATE_PARAM].getValue() > 0.5f);
		const bool genTrigged = genTrig.process(inputs[GEN_INPUT].getVoltage(), 0.1f, 1.f);
		if (genPressed || genTrigged)
			generateOrMutate();
		if (mutateTimeButton.process(params[MUTATE_TIME_PARAM].getValue() > 0.5f)
		    && mutatePattern(AcidPatternMutator::Time))
			mutateTimePulse.trigger(0.12f);
		if (mutatePitchButton.process(params[MUTATE_PITCH_PARAM].getValue() > 0.5f)
		    && mutatePattern(AcidPatternMutator::Pitch))
			mutatePitchPulse.trigger(0.12f);
		if (mutateArticulationButton.process(params[MUTATE_ARTICULATION_PARAM].getValue() > 0.5f)
		    && mutatePattern(AcidPatternMutator::Articulation))
			mutateArticulationPulse.trigger(0.12f);
		const int action = pendingPatternAction.exchange(ACTION_NONE);
		if (action == MUTATE_TIME && mutatePattern(AcidPatternMutator::Time))
			mutateTimePulse.trigger(0.12f);
		else if (action == MUTATE_PITCH && mutatePattern(AcidPatternMutator::Pitch))
			mutatePitchPulse.trigger(0.12f);
		else if (action == MUTATE_ARTICULATION && mutatePattern(AcidPatternMutator::Articulation))
			mutateArticulationPulse.trigger(0.12f);
		else if (action == UNDO_MUTATION) undoMutation();
		else if (action == ACTION_GENERATE) generateOrMutate();
		else if (action == ACTION_CLEAR) clearPattern();
		if (hasPendingPattern.exchange(false)) {
			// Una edición manual conserva la semilla: sigue siendo ese patrón, retocado.
			// Una importación trae la suya, que es un hash de su contenido.
			pattern = pendingPattern;
			patternSeed = pattern.seed;
			renderPattern();
			mutationCounter = 0;
			hasUndo = false;
		}

		// Todo el recorrido se cuenta desde la página activa: el primer paso de la
		// secuencia es el suyo, no el paso 1 del patrón.
		const int base = startStep();
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			// Reset deja el primer paso preparado, no en curso: lo arranca el siguiente
			// flanco, que es lo que espera cualquier reloj con reset.
			step = base;
			stepTime = 0.f;
			clockStarted = false;
		}
		const int len = length();
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (!clockStarted) {
				// Primer flanco: arranca por la página activa y no mide periodo, porque
				// lo que llevaba contado es el tiempo desde que se cargó el módulo.
				clockStarted = true;
				step = base;
			}
			else {
				if (stepTime > 1e-4f)
					clockPeriod = clamp(stepTime, 0.002f, 4.f);
				step = nextStep(step, len, base);
			}
			stepTime = 0.f;
			if (step == base)
				eocPulse.trigger(1e-3f);
		}
		// Acortar PASOS o mover la página activa puede dejar la cabeza fuera de la
		// ventana; entonces vuelve al principio en vez de sonar donde ya no toca.
		if (step < 0 || step >= MAX_STEPS || relStep(step, base) >= len)
			step = base;
		stepTime += args.sampleTime;

		const int next = nextStep(step, len, base);
		const bool active = clockStarted && gen.gate[step];
		const bool nextActive = clockStarted && gen.gate[next];
		const bool tieIn = active && gen.tie[step];
		const bool tieOut = active && nextActive && gen.tie[next];
		const int stepSemi = AcidGen::semiOf(gen.deg[step], gen.oct[step], scaleIdx());
		// PASOS puede cerrar el bucle antes de tiempo, así que se valida la transición que
		// realmente va a sonar y no solo la que se generó. La altura no entra en la
		// condición: un slide entre dos notas iguales no glissa, pero las toca legato, que
		// es la otra mitad de lo que hace un 303.
		const bool slide = active && gen.slide[step] && nextActive
		                && !tieIn && !gen.tie[next];

		// Un tie real mantiene el gate desde su ataque hasta casi el final del último paso
		// prolongado. Un slide, en cambio, solo cruza el cambio de paso si el usuario eligió
		// la convención legato; en la convención corta cae antes y SLIDE mantiene viva la voz.
		const bool legatoSlide = legatoTies && slide && nextActive && !gen.tie[next];
		const float gap = std::max(0.0015f, 0.03f * clockPeriod);
		const float normalHold = std::min(params[GATELEN_PARAM].getValue() * clockPeriod,
		                                  clockPeriod - gap);
		const float tiedHold = clockPeriod - gap;
		const bool gate = active && ((tieOut || legatoSlide)
		                         || stepTime < (tieIn ? tiedHold : normalHold));

		const bool voiceDoesSlide = rightExpander.module
		                         && rightExpander.module->model == modelAtek303;
		const float target = octaveBase
		                   + (rootSemi() + stepSemi) / 12.f;
		if (internalGlide && !voiceDoesSlide) {
			// La constante va atada al paso, no en milisegundos fijos: así el slide ocupa
			// la misma fracción de la nota a cualquier tempo, que es lo que lo hace sonar
			// a 303 y no a portamento de sintetizador.
			const float tau = clamp(0.45f * clockPeriod, 0.005f, 1.f);
			const float a = 1.f - std::exp(-args.sampleTime / tau);
			glideVoct += (prevSlide ? a : 1.f) * (target - glideVoct);
		}
		else {
			glideVoct = target;
		}
		prevSlide = slide;

		const bool accent = active && gen.accent[step];
		// El acento como CV se mantiene todo el paso y da un nivel también a las notas sin
		// acento: así otra voz lo puede leer de velocity con un S&H o directo a un VCA.
		const float accentV = accentAsCV ? (active ? (accent ? accentLevel : accentBase) : 0.f)
		                                 : (accent ? 10.f : 0.f);
		outputs[VOCT_OUTPUT].setVoltage(glideVoct);
		outputs[GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f);
		outputs[ACCENT_OUTPUT].setVoltage(accentV);
		outputs[SLIDE_OUTPUT].setVoltage(slide ? 10.f : 0.f);
		outputs[EOC_OUTPUT].setVoltage(eocPulse.process(args.sampleTime) ? 10.f : 0.f);

		if (rightExpander.module && rightExpander.module->model == modelAtek303) {
			Module::Expander& dst = rightExpander.module->leftExpander;
			if (dst.producerMessage) {
				Atek303SeqMessage* m = (Atek303SeqMessage*) dst.producerMessage;
				m->voct = glideVoct;
				m->gate = gate;
				m->accent = accent;
				m->slide = slide;
				dst.requestMessageFlip();
			}
		}

		// Una fila de LEDs para ver el patrón, sin controles: apagado = silencio, y los
		// colores del editor (acidNoteColor y compañía): azul = ataque, azul profundo =
		// tie, cian = slide, casi blanco = acento; el paso en curso brilla.
		// Enseña la página que se está editando, la misma que el roll: son dieciséis LEDs
		// para sesenta y cuatro pasos, y seguir a la cabeza saltando de página dejaría la
		// fila diciendo algo distinto de lo que hay debajo.
		const int lightBase = editorPage() * ACID_PAGE_STEPS;
		for (int c = 0; c < ACID_PAGE_STEPS; c++) {
			const int i = lightBase + c;
			float r = 0.f, g = 0.f, b = 0.f;
			if (i < MAX_STEPS && relStep(i, base) < len && gen.gate[i]) {
				// The editor's colours as RGB light levels (see acidNoteColor).
				if (gen.tie[i]) { r = 0.12f; g = 0.31f; b = 0.61f; }
				else if (gen.accent[i]) { r = 0.81f; g = 0.90f; b = 1.f; }
				else if (gen.slide[i]) { r = 0.f; g = 0.76f; b = 1.f; }
				else { r = 0.17f; g = 0.50f; b = 1.f; }
			}
			// La cabeza se pinta en blanco a plena luz, tape lo que tape: en una fila de
			// azules es lo que se localiza sin pensar.
			const bool playing = clockStarted && i == step;
			if (playing) { r = 1.f; g = 1.f; b = 1.f; }
			const float dim = playing ? 1.f : 0.28f;
			lights[STEP_LIGHT + c * 3 + 0].setBrightness(r * dim);
			lights[STEP_LIGHT + c * 3 + 1].setBrightness(g * dim);
			lights[STEP_LIGHT + c * 3 + 2].setBrightness(b * dim);
		}
		// Las cuatro páginas. Apagadas por defecto: encendidas todas, la tira no dice
		// nada. Verde muy leve por donde va el secuenciador, azul del logo a plena luz
		// donde estás tú, que es lo que hay que encontrar de un vistazo. Si la página que
		// miras queda fuera de PASOS, el azul baja: sigue diciendo dónde estás, y además
		// que eso no llega a sonar.
		for (int pg = 0; pg < ACID_PAGES; pg++) {
			const int first = pg * ACID_PAGE_STEPS;
			const bool exists = relStep(first, base) < len;
			const bool editing = pg == editorPage();
			const bool sounding = clockStarted && step >= first
			                   && step < first + ACID_PAGE_STEPS;
			// Mismo código que la fila de pasos: blanco por donde va la cabeza, con un punto
			// más de brillo en el tiempo fuerte. Blanco tenue en la página por la que
			// arranca la secuencia, para saber dónde empieza con el reloj parado.
			const float head = sounding ? ((step % 4) == 0 ? 1.f : 0.60f) : 0.f;
			const float blue = editing ? (exists ? 1.f : 0.45f) : 0.f;
			const float start = (pg == startPage.load()) ? 0.22f : 0.f;
			// El azul del logotipo, componente a componente: #2C7FFF.
			lights[PAGE_LIGHT + pg * 3 + 0].setBrightness(
				std::max(blue * 0.17f, start) + head);
			lights[PAGE_LIGHT + pg * 3 + 1].setBrightnessSmooth(
				std::max(blue * 0.50f, start) + head, args.sampleTime);
			lights[PAGE_LIGHT + pg * 3 + 2].setBrightness(std::max(blue, start) + head);
		}
		lights[GENERATE_LIGHT].setBrightnessSmooth(
			genPulse.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
		lights[SEED_LOCK_LIGHT].setBrightness(seedLocked ? 1.f : 0.f);
		lights[VIEW_LIGHT].setBrightness(params[VIEW_PARAM].getValue() > 0.5f ? 1.f : 0.f);
		lights[MUTATE_TIME_LIGHT].setBrightnessSmooth(
			mutateTimePulse.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
		lights[MUTATE_PITCH_LIGHT].setBrightnessSmooth(
			mutatePitchPulse.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
		lights[MUTATE_ARTICULATION_LIGHT].setBrightnessSmooth(
			mutateArticulationPulse.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// schemaVersion describe el JSON; algorithmVersion identifica el generador. No se
		// vuelven a mezclar: una versión nueva puede leer un esquema viejo sin fingir que
		// aquel patrón fue generado por el algoritmo actual.
		json_object_set_new(rootJ, "schemaVersion", json_integer(AcidPatternV4::SCHEMA_VERSION));
		json_object_set_new(rootJ, "patternVersion", json_integer(AcidPatternV4::SCHEMA_VERSION));
		json_object_set_new(rootJ, "algorithmVersion", json_integer(pattern.algorithmVersion));
		json_object_set_new(rootJ, "seed", json_integer((json_int_t) patternSeed));
		json_object_set_new(rootJ, "seedLocked",
		                    json_boolean(params[SEED_LOCK_PARAM].getValue() > 0.5f));
		json_object_set_new(rootJ, "mutationCounter", json_integer((json_int_t) mutationCounter));
		json_object_set_new(rootJ, "legatoTies", json_boolean(legatoTies));
		json_object_set_new(rootJ, "internalGlide", json_boolean(internalGlide));
		json_object_set_new(rootJ, "octaveBase", json_integer(octaveBase));
		json_object_set_new(rootJ, "accentAsCV", json_boolean(accentAsCV));
		json_object_set_new(rootJ, "accentLevel", json_real(accentLevel));
		json_object_set_new(rootJ, "accentBase", json_real(accentBase));

		json_t* paramsJ = json_object();
		json_object_set_new(paramsJ, "steps", json_integer(generatedWith.steps));
		json_object_set_new(paramsJ, "scale", json_integer(generatedWith.scale));
		json_object_set_new(paramsJ, "density", json_real(generatedWith.density));
		json_object_set_new(paramsJ, "accent", json_real(generatedWith.accent));
		json_object_set_new(paramsJ, "slide", json_real(generatedWith.slide));
		json_object_set_new(paramsJ, "range", json_real(generatedWith.range));
		json_object_set_new(paramsJ, "tie", json_real(generatedWith.tie));
		json_object_set_new(rootJ, "generatedWith", paramsJ);

		json_object_set_new(rootJ, "timeLength", json_integer(pattern.timeLength));
		json_t* timeJ = json_array();
		for (int i = 0; i < pattern.timeLength; i++)
			json_array_append_new(timeJ, json_integer((int) pattern.time[i]));
		json_object_set_new(rootJ, "timeData", timeJ);

		json_object_set_new(rootJ, "pitchLength", json_integer(pattern.pitchLength));
		json_t* pitchJ = json_array();
		for (int i = 0; i < pattern.pitchLength; i++) {
			json_t* p = json_object();
			json_object_set_new(p, "d", json_integer(pattern.pitch[i].degree));
			json_object_set_new(p, "o", json_integer(pattern.pitch[i].octave));
			json_object_set_new(p, "a", json_boolean(pattern.pitch[i].accent));
			json_object_set_new(p, "s", json_boolean(pattern.pitch[i].slideOut));
			json_array_append_new(pitchJ, p);
		}
		json_object_set_new(rootJ, "pitchData", pitchJ);

		// Copia renderizada para downgrade y diagnóstico. v4 siempre carga timeData/pitchData;
		// una versión v3 puede seguir leyendo "pattern" sin saber nada del modelo dual.
		json_t* pat = json_array();
		for (int i = 0; i < MAX_STEPS; i++) {
			json_t* st = json_object();
			json_object_set_new(st, "d", json_integer(gen.deg[i]));
			json_object_set_new(st, "o", json_integer(gen.oct[i]));
			json_object_set_new(st, "g", json_boolean(gen.gate[i]));
			json_object_set_new(st, "a", json_boolean(gen.accent[i]));
			json_object_set_new(st, "s", json_boolean(gen.slide[i]));
			json_object_set_new(st, "t", json_boolean(gen.tie[i]));
			json_array_append_new(pat, st);
		}
		json_object_set_new(rootJ, "pattern", pat);

		json_object_set_new(rootJ, "editPage", json_integer(editorPage()));
		json_object_set_new(rootJ, "mutateWholePattern",
		                    json_boolean(mutateWholePattern.load()));
		json_object_set_new(rootJ, "followPlayhead", json_boolean(followPlayhead.load()));
		json_object_set_new(rootJ, "startPage", json_integer(startPage.load()));

		// La ruta se guarda solo para poder seguir saltando de compás al reabrir el patch.
		// El patrón que suena ya va entero en el JSON, así que un fichero que desaparezca
		// no rompe nada.
		if (!midiPath.empty()) {
			json_object_set_new(rootJ, "midiPath", json_string(midiPath.c_str()));
			json_object_set_new(rootJ, "midiBar", json_integer(midiBar));
		}
		return rootJ;
	}
	// Vuelve a leer el fichero MIDI del patch sin tocar el patrón: lo único que restaura
	// es la lista de compases, para que el submenú siga funcionando tras reabrir.
	void reopenMidiFile(const std::string& path, int bar) {
		clearMidiFile();
		MidiFileReader reader;
		MidiFileData data;
		if (!reader.load(path, data)) return;
		const AcidMidiImportResult result =
			AcidMidiImport::convert(data, scaleIdx(), rootSemi(), octaveBase);
		if (!result.ok()) return;
		midiBars = result.bars;
		midiPath = path;
		midiName = system::getFilename(path);
		midiBar = (bar >= 0 && bar < (int) midiBars.size()) ? bar : 0;
		midiStatus = string::f("%d bar%s", (int) midiBars.size(),
		                       midiBars.size() == 1 ? "" : "s");
	}

	void dataFromJson(json_t* rootJ) override {
		seedLocked = params[SEED_LOCK_PARAM].getValue() > 0.5f;
		if (json_t* j = json_object_get(rootJ, "seed"))
			patternSeed = (uint32_t) json_integer_value(j);
		if (!patternSeed) patternSeed = 1u;
		if (json_t* j = json_object_get(rootJ, "seedLocked")) {
			seedLocked = json_boolean_value(j);
			params[SEED_LOCK_PARAM].setValue(seedLocked ? 1.f : 0.f);
		}
		if (json_t* j = json_object_get(rootJ, "editPage")) {
			const int v = (int) json_integer_value(j);
			editPage.store((v < 0) ? 0 : (v >= ACID_PAGES ? ACID_PAGES - 1 : v));
		}
		if (json_t* j = json_object_get(rootJ, "followPlayhead"))
			followPlayhead.store(json_boolean_value(j));
		if (json_t* j = json_object_get(rootJ, "startPage"))
			setStartPage((int) json_integer_value(j));
		if (json_t* j = json_object_get(rootJ, "mutateWholePattern"))
			mutateWholePattern.store(json_boolean_value(j));
		if (json_t* j = json_object_get(rootJ, "mutationCounter"))
			mutationCounter = (uint32_t) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "midiPath")) {
			const char* path = json_string_value(j);
			json_t* barJ = json_object_get(rootJ, "midiBar");
			if (path && path[0])
				reopenMidiFile(path, barJ ? (int) json_integer_value(barJ) : 0);
		}
		if (json_t* j = json_object_get(rootJ, "legatoTies"))
			legatoTies = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "internalGlide"))
			internalGlide = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "octaveBase"))
			octaveBase = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "accentAsCV"))
			accentAsCV = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "accentLevel"))
			accentLevel = json_number_value(j);
		if (json_t* j = json_object_get(rootJ, "accentBase"))
			accentBase = json_number_value(j);

		generatedWith = currentGenerationParams();
		if (json_t* p = json_object_get(rootJ, "generatedWith")) {
			if (json_t* v = json_object_get(p, "steps")) generatedWith.steps = (int) json_integer_value(v);
			if (json_t* v = json_object_get(p, "scale")) generatedWith.scale = (int) json_integer_value(v);
			if (json_t* v = json_object_get(p, "density")) generatedWith.density = json_number_value(v);
			if (json_t* v = json_object_get(p, "accent")) generatedWith.accent = json_number_value(v);
			if (json_t* v = json_object_get(p, "slide")) generatedWith.slide = json_number_value(v);
			if (json_t* v = json_object_get(p, "range")) generatedWith.range = json_number_value(v);
			if (json_t* v = json_object_get(p, "tie")) generatedWith.tie = json_number_value(v);
		}

		json_t* timeJ = json_object_get(rootJ, "timeData");
		json_t* pitchJ = json_object_get(rootJ, "pitchData");
		if (json_is_array(timeJ) && json_is_array(pitchJ)) {
			pattern.clear();
			pattern.seed = patternSeed;
			if (json_t* j = json_object_get(rootJ, "algorithmVersion"))
				pattern.algorithmVersion = (uint8_t) json_integer_value(j);
			if (json_t* j = json_object_get(rootJ, "timeLength"))
				pattern.timeLength = (uint8_t) json_integer_value(j);
			const int timeCount = std::min(ACID_MAX_STEPS, (int) json_array_size(timeJ));
			for (int i = 0; i < timeCount; i++)
				pattern.time[i] = (AcidTimeState) json_integer_value(json_array_get(timeJ, i));
			if (json_t* j = json_object_get(rootJ, "pitchLength"))
				pattern.pitchLength = (uint8_t) json_integer_value(j);
			const int pitchCount = std::min(ACID_MAX_STEPS, (int) json_array_size(pitchJ));
			for (int i = 0; i < pitchCount; i++) {
				json_t* p = json_array_get(pitchJ, i);
				if (json_t* v = json_object_get(p, "d")) pattern.pitch[i].degree = (int8_t) json_integer_value(v);
				if (json_t* v = json_object_get(p, "o")) pattern.pitch[i].octave = (int8_t) json_integer_value(v);
				if (json_t* v = json_object_get(p, "a")) pattern.pitch[i].accent = json_boolean_value(v);
				if (json_t* v = json_object_get(p, "s")) pattern.pitch[i].slideOut = json_boolean_value(v);
			}
			pattern.sanitize(scaleIdx());
			renderPattern();
			return;
		}

		if (json_t* pat = json_object_get(rootJ, "pattern")) {
			// En la versión anterior no existía "t". Se limpia primero para que un patrón
			// guardado no herede por accidente los ties del patrón generado en el constructor.
			for (int i = 0; i < MAX_STEPS; i++) gen.tie[i] = false;
			for (int i = 0; i < MAX_STEPS && i < (int) json_array_size(pat); i++) {
				json_t* st = json_array_get(pat, i);
				// "d" es el grado dentro de la octava y "o" la octava; los patches
				// anteriores guardaban "n" en semitonos y no se pueden reinterpretar,
				// así que se ignoran y el módulo arranca con el patrón que generó solo.
				if (json_t* v = json_object_get(st, "d")) gen.deg[i] = (int8_t) json_integer_value(v);
				if (json_t* v = json_object_get(st, "o")) gen.oct[i] = (int8_t) json_integer_value(v);
				if (json_t* v = json_object_get(st, "g")) gen.gate[i] = json_boolean_value(v);
				if (json_t* v = json_object_get(st, "a")) gen.accent[i] = json_boolean_value(v);
				if (json_t* v = json_object_get(st, "s")) gen.slide[i] = json_boolean_value(v);
				if (json_t* v = json_object_get(st, "t")) gen.tie[i] = json_boolean_value(v);
			}
			// Sanea JSON editado a mano y mantiene una representación inequívoca: un tie
			// continúa exactamente la altura anterior, sin acento ni slide alrededor.
			gen.tie[0] = false;
			for (int i = 1; i < MAX_STEPS; i++) {
				if (!gen.tie[i]) continue;
				if (!gen.gate[i - 1] || !gen.gate[i]) {
					gen.tie[i] = false;
					continue;
				}
				gen.deg[i] = gen.deg[i - 1];
				gen.oct[i] = gen.oct[i - 1];
				gen.accent[i] = false;
				gen.slide[i - 1] = false;
				gen.slide[i] = false;
			}
			pattern.importRendered(gen, MAX_STEPS, patternSeed, 3);
			pattern.sanitize(scaleIdx());
			renderPattern();
		}
	}
};

// Un knob de doce posiciones sin nada escrito no dice en qué nota está. Los valores de
// RAIZ y ESCALA se pintan debajo de su knob.
struct SeqReadout : Widget {
	Atek303Seq* module = NULL;
	float size = 8.f;
	int kind = 0;   // 0 raíz, 1 escala

	void draw(const DrawArgs& args) override {
		std::shared_ptr<window::Font> font =
			APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		if (!font)
			return;
		const char* text = (kind == 0) ? ACID_NOTE_NAMES[module ? module->rootSemi() : 0]
		                               : ACID_SCALES[module ? module->scaleIdx() : 0].shortName;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, size);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, AnimatekUI::logoBlue());   // el azul del sticker
		nvgText(args.vg, box.size.x / 2, box.size.y / 2, text, NULL);
	}
};

// El sticker acid del panel es además el botón de generar. En la vista de editor no hay
// GENERATE a la vista, y tener que salir del editor para sortear otro patrón rompía el
// flujo de trabajo. Como no lo parece a simple vista, lleva su propio tooltip.
// El alcance de MUTATE se elige en el menú, pero se decide tocando: tiene que verse en
// el panel, junto a los botones que lo usan.
struct AcidMutationScopeLabel : AnimatekUI::TextLabel {
	Atek303Seq* module = NULL;

	void step() override {
		if (module) {
			text = module->mutateWholePattern.load()
			     ? string::f("SCOPE  ALL %d", ACID_MAX_STEPS)
			     : string::f("SCOPE  PAGE %d:%d", module->editorPage() + 1, ACID_PAGES);
		}
		AnimatekUI::TextLabel::step();
	}
};

// La tira de páginas: cuatro celdas, una por compás. Sólo cambia lo que enseña el
// editor; el secuenciador sigue recorriendo los sesenta y cuatro pasos pase lo que pase.
// Geometría de la tira de páginas: el botón a la izquierda, los cuatro LEDs a la
// derecha. La comparten el widget que recoge los clics y el panel que coloca las luces.
static constexpr float PANEL_W = 101.6f;     // 20 HP
// La fila de LEDs de paso de la cabecera. Los cuatro LEDs de página cuelgan de sus
// cuatro últimas columnas —los pasos 13 a 16—, así que se derivan de aquí en vez de
// llevar números propios: mover la fila de arriba mueve la tira con ella.
static constexpr float STEP_X0 = 9.5f;
static constexpr float STEP_X1 = PANEL_W - 9.5f;
static constexpr float STEP_PITCH = (STEP_X1 - STEP_X0) / (float) (ACID_PAGE_STEPS - 1);

// Botón redondo, el mismo dibujo que los bezels de GENERATE —aro negro, corona y
// casquillo gris— pero a menor escala: en este hueco no cabe un bezel entero.
static constexpr float PAGE_BTN_W = 4.2f;  // diámetro
// Centro del botón dentro de la tira. No va a la altura del LED sino algo por debajo:
// cada página es LED más rótulo, y el centro de ese conjunto cae más abajo que el punto
// de luz. Alineado con el LED a secas, el botón se ve subido; medio milímetro por debajo
// del centro del conjunto se ve caído. Este par de décimas es el punto en el que queda
// repartido entre el borde del editor y los rótulos de entradas y salidas.
static constexpr float PAGE_BTN_CY = 3.2f;
// La tira cuelga del editor, que es la rejilla que tiene justo encima, y no de la fila
// de LEDs de la cabecera: las columnas del editor arrancan después del tecladito y son
// algo más estrechas que los pasos de arriba, así que hay que caer a plomo con ellas.
static constexpr float EDIT_X = 5.f;
static constexpr float EDIT_W = PANEL_W - 10.f;
static constexpr float PAGE_COL_X = EDIT_X + AcidEditor::GUTTER;  // borde de la columna 1
static constexpr float PAGE_CELL = (EDIT_W - AcidEditor::GUTTER)
                                 / (float) ACID_PAGE_STEPS;
// La tira se lee de izquierda a derecha: el botón bajo la columna 11, una raya del ancho
// de la 12, y las cuatro páginas bajo las columnas 13 a 16, contra el borde derecho de
// la rejilla. El botón manda sobre esos cuatro LEDs y sobre ningún otro sitio del panel:
// dibujado el cable, se ve de dónde a dónde va sin tener que leer el manual.
static constexpr float PAGE_BTN_CX = PAGE_COL_X + 10.5f * PAGE_CELL;
static constexpr float PAGE_LINK_X = PAGE_COL_X + 11.f * PAGE_CELL;
// La raya no llena la columna de lado a lado: a tamaño completo se comía el hueco y el
// botón y los LEDs se leían pegados. Media columna, centrada, deja aire a los dos lados.
static constexpr float PAGE_LINK_W = PAGE_CELL * 0.5f;
static constexpr float PAGE_LED_X = PAGE_COL_X + 12.f * PAGE_CELL;
static constexpr float PAGE_STRIP_X = PAGE_BTN_CX - PAGE_BTN_W * 0.5f;
static constexpr float PAGE_STRIP_W = PAGE_LED_X + PAGE_CELL * ACID_PAGES - PAGE_STRIP_X;

struct AcidPageButtons : OpaqueWidget {
	Atek303Seq* module = NULL;
	ui::Tooltip* tooltip = NULL;
	// Un toque avanza de página; mantener pulsado engancha o suelta el seguimiento. Por
	// eso el avance se hace al soltar y no al pulsar: hasta que no se levanta el dedo no
	// se sabe cuál de los dos gestos era.
	static constexpr double LONG_PRESS = 2.0;
	double pressedAt = -1.0;
	bool held = false;
	bool consumedByHold = false;
	// El doble clic no trae posición, así que se apunta en el clic simple sobre qué LED
	// cayó. -1 es "en ninguno": el botón, o el aire entre celdas.
	int lastPage = -1;

	// El botón cierra la tira por la derecha; los cuatro LEDs la abren. La caja del botón
	// es cuadrada y el dibujo, un círculo inscrito: para pulsar se acepta la caja entera,
	// que el blanco de las esquinas es un pelo de milímetro y el objetivo ya es pequeño.
	Rect buttonRect() const {
		const float d = mm2px(PAGE_BTN_W);
		const float cx = mm2px(PAGE_BTN_CX - PAGE_STRIP_X);
		return Rect(Vec(cx - d * 0.5f, mm2px(PAGE_BTN_CY) - d * 0.5f), Vec(d, d));
	}

	int pageAt(Vec p) const {
		const float cell = mm2px(PAGE_CELL);
		const float x0 = mm2px(PAGE_LED_X - PAGE_STRIP_X);
		if (cell <= 0.f || p.x < x0) return -1;
		const int i = (int) std::floor((p.x - x0) / cell);
		return (i < 0 || i >= ACID_PAGES) ? -1 : i;
	}

	// 0 sin pulsar, 1 cuando la pulsación ya ha cumplido el tiempo del seguimiento.
	float holdProgress() const {
		if (!held || pressedAt < 0.0) return 0.f;
		const double t = (system::getTime() - pressedAt) / LONG_PRESS;
		return (float) (t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t));
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (module && buttonRect().contains(e.pos)) {
				pressedAt = system::getTime();
				held = true;
				consumedByHold = false;
				lastPage = -1;
				e.consume(this);
				return;
			}
			const int page = pageAt(e.pos);
			if (module && page >= 0) {
				lastPage = page;
				module->showPage(page);
				e.consume(this);
				return;
			}
		}
		// El clic derecho no se consume: tiene que llegar al menú contextual del módulo.
		OpaqueWidget::onButton(e);
	}

	// Un clic enseña la página; dos la ponen a sonar. Son dos cosas distintas —mirar y
	// arrancar—, y así se puede repasar el patrón sin cambiar lo que suena.
	void onDoubleClick(const event::DoubleClick& e) override {
		if (module && lastPage >= 0) {
			module->setStartPage(lastPage);
			e.consume(this);
			return;
		}
		OpaqueWidget::onDoubleClick(e);
	}

	void onDragEnd(const event::DragEnd& e) override {
		// Soltar antes del umbral es un toque: avanza de página. Si el seguimiento ya se
		// enganchó durante la pulsación, soltar no hace nada más.
		if (held && module && !consumedByHold)
			module->showPage((module->editorPage() + 1) % ACID_PAGES);
		held = false;
		pressedAt = -1.0;
		consumedByHold = false;
		OpaqueWidget::onDragEnd(e);
	}

	// Centro y radio del casquillo, dentro del aro. Los dos dibujos —el apagado y el azul
	// de la capa de luces— tienen que caer en el mismo sitio, así que sale de aquí.
	void capGeometry(Vec& c, float& radius) const {
		const Rect r = buttonRect();
		c = r.pos.plus(r.size.div(2.f));
		radius = r.size.x * 0.5f * 0.8f;  // la cara, como en los botones planos (FlatLightButton)
	}

	// El botón es de panel, no de pantalla: va en la capa normal, como los jacks.
	void draw(const DrawArgs& args) override {
		// La raya que une el botón con los cuatro LEDs: centrada en la columna que queda
		// entre ambos, a la altura del centro del botón.
		{
			const float y = mm2px(PAGE_BTN_CY);
			const float x1 = mm2px(PAGE_LINK_X - PAGE_STRIP_X
			                       + (PAGE_CELL - PAGE_LINK_W) * 0.5f);
			const float x2 = x1 + mm2px(PAGE_LINK_W);
			{
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, x1, y);
				nvgLineTo(args.vg, x2, y);
				nvgStrokeColor(args.vg, AnimatekUI::panelSeparatorColor(160));
				nvgStrokeWidth(args.vg, 0.8f);
				nvgLineCap(args.vg, NVG_ROUND);
				nvgStroke(args.vg);
			}
		}

		const Rect r = buttonRect();
		Vec c;
		float radius;
		capGeometry(c, radius);
		// El estilo plano de los botones y mandos del plugin: aro gris fino y cara gris
		// lisa. Pulsado, la cara baja un escalón; el azul de seguir al cabezal la llena
		// en la capa de luz (drawLayer).
		const float d = r.size.x;
		const float rimW = std::max(1.2f, d * 0.045f);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, d * 0.5f - rimW * 0.5f);
		nvgFillColor(args.vg, nvgRGB(0x1b, 0x1b, 0x1b));
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, rimW);
		nvgStrokeColor(args.vg, nvgRGB(0x5a, 0x5a, 0x5a));
		nvgStroke(args.vg);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, radius);
		nvgFillColor(args.vg, held ? nvgRGB(0x22, 0x22, 0x22) : nvgRGB(0x2c, 0x2c, 0x2c));
		nvgFill(args.vg);
		OpaqueWidget::draw(args);
	}

	// Siguiendo a la cabeza, el botón se enciende: es un estado, no un gesto, y hay que
	// poder verlo sin acordarse de qué se pulsó. Mientras se mantiene, el azul entra por
	// la izquierda: dos segundos a ciegas son eternos, con la barra se sabe cuándo suelta.
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			Vec c;
			float radius;
			capGeometry(c, radius);
			const bool following = module->followPlayhead.load();
			const float progress = holdProgress();
			// Al alternar el estado, el relleno pinta lo que va a quedar: si estaba
			// siguiendo, la barra vacía el azul en vez de llenarlo.
			const float fill = (progress > 0.f) ? (following ? 1.f - progress : progress)
			                                    : (following ? 1.f : 0.f);
			if (fill > 0.001f) {
				nvgSave(args.vg);
				nvgScissor(args.vg, c.x - radius, c.y - radius, 2.f * radius * fill, 2.f * radius);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, c.x, c.y, radius);
				nvgFillColor(args.vg, AnimatekUI::logoBlue());
				nvgFill(args.vg);
				nvgRestore(args.vg);
			}
		}
		OpaqueWidget::drawLayer(args, layer);
	}

	void onEnter(const EnterEvent& e) override {
		if (tooltip) return;
		tooltip = new ui::Tooltip;
		tooltip->text = "Pages: click to show, double click to start there\n"
		               "Button: tap to advance, hold to follow the playhead";
		APP->scene->addChild(tooltip);
	}

	void onLeave(const LeaveEvent& e) override {
		if (!tooltip) return;
		APP->scene->removeChild(tooltip);
		delete tooltip;
		tooltip = NULL;
	}

	void step() override {
		// El seguimiento cae solo al cumplirse el tiempo: se nota bajo el dedo, sin tener
		// que soltar para saber si ha cogido.
		if (held && !consumedByHold && module && holdProgress() >= 1.f) {
			module->followPlayhead.store(!module->followPlayhead.load());
			consumedByHold = true;
		}
		if (tooltip)
			tooltip->box.pos = getAbsoluteOffset(Vec(0, box.size.y)).round();
		OpaqueWidget::step();
	}
};

struct AcidStickerButton : OpaqueWidget {
	Atek303Seq* module = NULL;
	ui::Tooltip* tooltip = NULL;

	// El sticker es redondo: el cuadro que lo envuelve no debe tragarse los clics de las
	// esquinas, que caen sobre el panel.
	bool insideSticker(Vec p) const {
		const Vec c = box.size.div(2.f);
		const float r = std::min(c.x, c.y);
		return p.minus(c).norm() <= r;
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT
		    && insideSticker(e.pos)) {
			if (module)
				module->pendingPatternAction.store(Atek303Seq::ACTION_GENERATE);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}

	void onEnter(const EnterEvent& e) override {
		if (tooltip) return;
		tooltip = new ui::Tooltip;
		tooltip->text = "New pattern; with BLOCK, mutate all three layers";
		APP->scene->addChild(tooltip);
	}

	void onLeave(const LeaveEvent& e) override {
		if (!tooltip) return;
		APP->scene->removeChild(tooltip);
		delete tooltip;
		tooltip = NULL;
	}

	~AcidStickerButton() override {
		if (tooltip) {
			APP->scene->removeChild(tooltip);
			delete tooltip;
		}
	}
};

struct Atek303SeqWidget : ModuleWidget {
	// Las dos vistas que comparten el mismo hueco del panel. Todo lo que hay entre la fila
	// de LEDs y la raya azul está hecho de widgets, no horneado en el SVG, así que conmutar
	// es enseñar una capa y esconder la otra: el SVG no se toca.
	Widget* genLayer = NULL;
	AcidEditor* editor = NULL;

	// Rejilla del panel, en mm. tools/panel.py usa las mismas para la raya azul, el
	// logo y el sticker acid, así que si aquí se mueve algo hay que moverlo allí.
	static constexpr float W = PANEL_W;        // 20 HP
	static constexpr float STEPS_Y = 23.0f;    // fila de LEDs de paso
	static constexpr float GRP_Y = 31.0f;      // borde superior de las tres cajas
	static constexpr float GRP_H = 30.0f;
	static constexpr float KNOB_LABEL_Y = 34.5f;
	static constexpr float KNOB_Y = 46.0f;
	static constexpr float READOUT_Y = 56.5f;
	static constexpr float MUT_Y = 65.0f;      // borde superior de la caja MUTACIÓN
	static constexpr float MUT_H = 24.0f;
	static constexpr float BTN_LABEL_Y = 68.5f;
	static constexpr float BTN_Y = 81.0f;
	static constexpr float PAGE_X = PAGE_STRIP_X;  // borde izquierdo de la tira
	static constexpr float PAGE_Y = 91.2f;     // centro de los LEDs, bajo el editor
	static constexpr float IO_SEC_Y = 96.5f;   // rótulos ENTRADAS / SALIDAS
	static constexpr float IO_LABEL_Y = 100.5f;
	static constexpr float IO_JACK_Y = 110.0f;
	static constexpr float IO_SPLIT_X = 42.5f; // separador vertical entre los dos bloques

	Atek303SeqWidget(Atek303Seq* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ATEK303SEQ.svg")));

		// Las dos capas conmutables ocupan el panel entero; cada widget conserva sus
		// coordenadas de siempre, así que la disposición no se mueve ni un milímetro.
		genLayer = new Widget;
		genLayer->box.size = box.size;
		addChild(genLayer);

		auto labelIn = [&](Widget* parent, const char* text, float x, float y,
		                   float w, float size) {
			auto* l = new AnimatekUI::TextLabel(text, mm2px(Vec(x - w * 0.5f, y)),
			                                    mm2px(Vec(w, 4.f)));
			l->fontSize = size;
			parent->addChild(l);
		};
		auto label = [&](const char* text, float x, float y, float w, float size) {
			labelIn(this, text, x, y, w, size);
		};

		// --- cabecera ----------------------------------------------------------
		auto* title = new AnimatekUI::TextLabel("ATEK303", mm2px(Vec(25.8f, 6.0f)),
		                                        mm2px(Vec(50.f, 6.f)));
		title->fontSize = 20.f;
		title->color = AnimatekUI::logoBlue();
		addChild(title);
		label("SEQ", W * 0.5f, 12.5f, 20.f, 10.f);

		// --- fila de pasos, dentro de su caja ----------------------------------
		addChild(new GroupBox("", mm2px(Vec(5.f, 19.f)), mm2px(Vec(W - 10.f, 8.f))));
		{
			for (int i = 0; i < ACID_PAGE_STEPS; i++) {
				const float x = STEP_X0 + i * STEP_PITCH;
				addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(
					mm2px(Vec(x, STEPS_Y)), module, Atek303Seq::STEP_LIGHT + i * 3));
			}
		}

		// --- las tres cajas de mandos ------------------------------------------
		// El agrupado es el del prototipo: qué genera la secuencia, en qué tonalidad,
		// y cómo se articula. GATE se va con ACENTO y SLIDE, que es donde pertenece.
		struct Knob { const char* name; int param; };
		struct Group { const char* name; float x, w; int n; Knob k[3]; };
		static const Group GROUPS[3] = {
			{"SEQUENCE", 5.0f, 33.0f, 3, {{"STEPS",  Atek303Seq::STEPS_PARAM},
			                               {"NOTES",  Atek303Seq::DENSITY_PARAM},
			                               {"RANGE",  Atek303Seq::RANGE_PARAM}}},
			{"KEY", 40.0f, 22.6f, 2, {{"ROOT",   Atek303Seq::ROOT_PARAM},
			                                {"SCALE",  Atek303Seq::SCALE_PARAM},
			                                {NULL, 0}}},
			{"ARTICULATION", 64.6f, 32.0f, 3, {{"GATE",   Atek303Seq::GATELEN_PARAM},
			                                   {"ACCENT", Atek303Seq::ACCENT_PARAM},
			                                   {"SLIDE",  Atek303Seq::SLIDEAMT_PARAM}}},
		};
		// El paso es w/n con los mandos centrados en su tramo: con w/(n+1) los knobs
		// de tres en tres se tocaban y las etiquetas se pisaban.
		auto slotX = [](const Group& g, int i) { return g.x + g.w * (i + 0.5f) / g.n; };
		for (const Group& g : GROUPS) {
			genLayer->addChild(new GroupBox(g.name, mm2px(Vec(g.x, GRP_Y)),
			                                mm2px(Vec(g.w, GRP_H))));
			const float step = g.w / g.n;
			for (int i = 0; i < g.n; i++) {
				const float x = slotX(g, i);
				labelIn(genLayer, g.k[i].name, x, KNOB_LABEL_Y, step - 0.8f, 6.8f);
				genLayer->addChild(createParamCentered<AnimatekUI::FlatKnob>(
					mm2px(Vec(x, KNOB_Y)), module, g.k[i].param));
			}
		}

		// Los dos lectores de TONALIDAD, bajo sus mandos.
		{
			const Group& g = GROUPS[1];
			for (int i = 0; i < 2; i++) {
				SeqReadout* ro = new SeqReadout;
				ro->module = module;
				ro->kind = i;
				ro->box.size = mm2px(Vec(11.f, 4.f));
				ro->box.pos = mm2px(Vec(slotX(g, i) - 5.5f, READOUT_Y - 2.f));
				genLayer->addChild(ro);
			}
		}

		// --- MUTACIÓN -----------------------------------------------------------
		// Fila de performance: generar una identidad, bloquearla y derivar tres familias
		// de variaciones sin entrar en el menú. BLOCK es latch; los otros son trigger.
		genLayer->addChild(new GroupBox("MUTATION", mm2px(Vec(5.f, MUT_Y)),
		                                mm2px(Vec(W - 10.f, MUT_H))));
		{
			// Sin módulo —el navegador, la web de la biblioteca— vale el valor por defecto.
			AcidMutationScopeLabel* scope = new AcidMutationScopeLabel;
			scope->module = module;
			scope->text = string::f("SCOPE  PAGE 1:%d", ACID_PAGES);
			scope->fontSize = 5.8f;
			scope->box.pos = mm2px(Vec(W * 0.5f - 14.f, MUT_Y + MUT_H - 4.6f));
			scope->box.size = mm2px(Vec(28.f, 4.f));
			genLayer->addChild(scope);
		}
		{
			static const float bx[5] = {15.5f, 33.3f, 50.8f, 68.3f, 86.1f};
			static const char* names[5] = {"GENERATE", "BLOCK", "MUT TIME",
			                               "MUT NOTE/OCT", "MUT SLD/ACC"};
			genLayer->addChild(createLightParamCentered<AnimatekUI::FlatLightButton>(
				mm2px(Vec(bx[0], BTN_Y)), module,
				Atek303Seq::GENERATE_PARAM, Atek303Seq::GENERATE_LIGHT));
			genLayer->addChild(createLightParamCentered<AnimatekUI::FlatLightLatch>(
				mm2px(Vec(bx[1], BTN_Y)), module,
				Atek303Seq::SEED_LOCK_PARAM, Atek303Seq::SEED_LOCK_LIGHT));
			genLayer->addChild(createLightParamCentered<AnimatekUI::FlatLightButton>(
				mm2px(Vec(bx[2], BTN_Y)), module,
				Atek303Seq::MUTATE_TIME_PARAM, Atek303Seq::MUTATE_TIME_LIGHT));
			genLayer->addChild(createLightParamCentered<AnimatekUI::FlatLightButton>(
				mm2px(Vec(bx[3], BTN_Y)), module,
				Atek303Seq::MUTATE_PITCH_PARAM, Atek303Seq::MUTATE_PITCH_LIGHT));
			genLayer->addChild(createLightParamCentered<AnimatekUI::FlatLightButton>(
				mm2px(Vec(bx[4], BTN_Y)), module,
				Atek303Seq::MUTATE_ARTICULATION_PARAM, Atek303Seq::MUTATE_ARTICULATION_LIGHT));
			for (int i = 0; i < 5; i++)
				labelIn(genLayer, names[i], bx[i], BTN_LABEL_Y, 17.f, i < 2 ? 6.8f : 6.2f);
		}

		// --- el editor, en el mismo hueco que el bloque de generación ----------
		editor = new AcidEditor;
		editor->host = module;
		editor->box.pos = mm2px(Vec(EDIT_X, GRP_Y));
		editor->box.size = mm2px(Vec(EDIT_W, MUT_Y + MUT_H - GRP_Y));
		addChild(editor);

		// --- las cuatro páginas, bajo el editor -------------------------------
		// Sesenta y cuatro pasos no caben en el roll, así que se editan de compás en
		// compás. La tira va pegada al borde derecho, en el hueco que queda entre el
		// editor y la raya de entradas y salidas.
		{
			const float ledX = PAGE_LED_X;
			for (int i = 0; i < ACID_PAGES; i++) {
				const float cx = ledX + (i + 0.5f) * PAGE_CELL;
				addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(
					mm2px(Vec(cx, PAGE_Y)), module, Atek303Seq::PAGE_LIGHT + i * 3));
				char name[8];
				std::snprintf(name, sizeof(name), "%d:%d", i + 1, ACID_PAGES);
				// El rótulo se alinea por abajo: la base cae en y + 4 mm, justo encima de
				// la fila de rótulos de entradas y salidas. La caja se deja más ancha que
				// la celda a propósito: el texto va centrado y no debe recortarse.
				label(name, cx, PAGE_Y - 0.6f, PAGE_CELL + 1.6f, 5.0f);
			}
			AcidPageButtons* pages = new AcidPageButtons;
			pages->module = module;
			pages->box.pos = mm2px(Vec(PAGE_X, PAGE_Y - 2.0f));
			pages->box.size = mm2px(Vec(PAGE_STRIP_W, 5.8f));
			addChild(pages);
		}

		// El conmutador va arriba, junto a la fila de pasos: se ve de un vistazo en qué
		// vista estás y se cambia de un clic sin abrir menús.
		// A la izquierda, en el espejo del sticker acid: es el único hueco de la cabecera
		// que no pisa ni el sticker ni el logotipo.
		addParam(createLightParamCentered<AnimatekUI::FlatLightLatch>(
			mm2px(Vec(13.6f, 10.0f)), module, Atek303Seq::VIEW_PARAM, Atek303Seq::VIEW_LIGHT));
		label("EDIT", 13.6f, 14.5f, 14.f, 5.8f);

		// El sticker vive en el SVG en cx=88 cy=11 r=6.435; aquí solo va la zona sensible.
		{
			AcidStickerButton* sticker = new AcidStickerButton;
			sticker->module = module;
			sticker->box.size = mm2px(Vec(12.9f, 12.9f));
			sticker->box.pos = mm2px(Vec(88.f - 6.45f, 11.f - 6.45f));
			addChild(sticker);
		}

		// --- entradas y salidas, bajo la raya azul ------------------------------
		addChild(new SectionLabel("INPUTS", mm2px(Vec(5.f, IO_SEC_Y - 2.f)),
		                          mm2px(Vec(IO_SPLIT_X - 8.f, 4.f))));
		addChild(new SectionLabel("OUTPUTS", mm2px(Vec(IO_SPLIT_X + 3.f, IO_SEC_Y - 2.f)),
		                          mm2px(Vec(W - IO_SPLIT_X - 8.f, 4.f))));
		{
			auto* sep = new AnimatekUI::ConnectorLine(mm2px(IO_SPLIT_X), mm2px(94.5f),
			                                          mm2px(IO_SPLIT_X), mm2px(115.f));
			sep->strokeWidth = 0.8f;
			addChild(sep);
		}

		struct Jack { const char* name; int id; float x; bool output; };
		static const Jack JACKS[8] = {
			{"CLOCK",  Atek303Seq::CLOCK_INPUT,   11.5f, false},
			{"RESET",  Atek303Seq::RESET_INPUT,   22.5f, false},
			{"GEN",    Atek303Seq::GEN_INPUT,     33.5f, false},
			{"EOC",    Atek303Seq::EOC_OUTPUT,    51.0f, true},
			{"V/OCT",  Atek303Seq::VOCT_OUTPUT,   62.0f, true},
			{"GATE",   Atek303Seq::GATE_OUTPUT,   73.0f, true},
			{"ACCENT", Atek303Seq::ACCENT_OUTPUT, 84.0f, true},
			{"SLIDE",  Atek303Seq::SLIDE_OUTPUT,  95.0f, true},
		};
		for (const Jack& j : JACKS) {
			label(j.name, j.x, IO_LABEL_Y, 11.f, 6.5f);
			if (j.output)
				addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(
					mm2px(Vec(j.x, IO_JACK_Y)), module, j.id));
			else
				addInput(createInputCentered<AnimatekUI::TekInputPort>(
					mm2px(Vec(j.x, IO_JACK_Y)), module, j.id));
		}
	}

	// La conmutación se resuelve cada frame en vez de con un callback del botón: así la
	// vista sigue siendo correcta cuando el parámetro cambia por otra vía —cargar un patch,
	// deshacer, o una automatización externa—, no solo cuando se pulsa.
	void step() override {
		Atek303Seq* m = getModule<Atek303Seq>();
		// Sin instancia —el navegador de módulos y la web de la librería— se enseña la vista
		// de generación, que es la identidad del módulo.
		// Sin instancia —el navegador de módulos y la web de la librería— se enseña la vista
		// de generación, que es la identidad del módulo.
		// Sin instancia —el navegador de módulos y la web de la librería— se enseña la vista
		// de generación, que es la identidad del módulo.
		const bool edit = m && m->params[Atek303Seq::VIEW_PARAM].getValue() > 0.5f;
		if (genLayer) genLayer->visible = !edit;
		if (editor) editor->visible = edit;
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Atek303Seq* module = getModule<Atek303Seq>();
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel(string::f("Pattern v%d · seed %08X",
		                                     module->pattern.algorithmVersion,
		                                     module->patternSeed)));
		menu->addChild(createCheckMenuItem("Lock seed", "",
			[=]() { return module->params[Atek303Seq::SEED_LOCK_PARAM].getValue() > 0.5f; },
			[=]() {
				Param& p = module->params[Atek303Seq::SEED_LOCK_PARAM];
				p.setValue(p.getValue() > 0.5f ? 0.f : 1.f);
			}));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Mutation scope"));
		menu->addChild(createCheckMenuItem("Current page (16 steps)", "",
			[=]() { return !module->mutateWholePattern.load(); },
			[=]() { module->mutateWholePattern.store(false); }));
		menu->addChild(createCheckMenuItem("Whole pattern (64 steps)", "",
			[=]() { return module->mutateWholePattern.load(); },
			[=]() { module->mutateWholePattern.store(true); }));
		menu->addChild(createMenuLabel("GENERATE: new seed · with BLOCK: mutate all three layers"));
		menu->addChild(createMenuItem("Mutate time (2 operations)", "", [=]() {
			module->pendingPatternAction.store(Atek303Seq::MUTATE_TIME);
		}));
		menu->addChild(createMenuItem("Mutate pitches / octaves (2 operations)", "", [=]() {
			module->pendingPatternAction.store(Atek303Seq::MUTATE_PITCH);
		}));
		menu->addChild(createMenuItem("Mutate accents / slides (3 operations)", "", [=]() {
			module->pendingPatternAction.store(Atek303Seq::MUTATE_ARTICULATION);
		}));
		menu->addChild(createMenuItem("Undo last mutation", "", [=]() {
			module->pendingPatternAction.store(Atek303Seq::UNDO_MUTATION);
		}, !module->hasUndo));
		menu->addChild(createMenuItem("Clear pattern", "", [=]() {
			module->pendingPatternAction.store(Atek303Seq::ACTION_CLEAR);
		}));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("MIDI file"));
		menu->addChild(createMenuItem("Load MIDI file\u2026", "", [=]() {
			// El diálogo arranca donde estaba el último fichero cargado: quien trabaja con
			// un pack de patrones abre varios seguidos de la misma carpeta.
			std::string dir = module->midiPath.empty()
			                ? asset::user("") : system::getDirectory(module->midiPath);
			osdialog_filters* filters = osdialog_filters_parse("MIDI:mid,midi,MID,MIDI");
			char* path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
			osdialog_filters_free(filters);
			if (!path) return;
			const bool ok = module->loadMidiFile(path);
			std::free(path);
			if (!ok)
				osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK,
				                 module->midiError.c_str());
		}));
		if (!module->midiBars.empty()) {
			menu->addChild(createMenuLabel(module->midiName));
			menu->addChild(createMenuLabel(module->midiStatus));
			if (module->midiBars.size() > 1) {
				std::vector<std::string> bars;
				for (size_t i = 0; i < module->midiBars.size(); i++)
					bars.push_back(string::f("Bar %d", (int) i + 1));
				menu->addChild(createIndexSubmenuItem("Bar", bars,
					[=]() { return module->midiBar; },
					[=](int i) { module->applyMidiBar(i); }));
			}
			menu->addChild(createMenuItem("Forget file", "", [=]() { module->clearMidiFile(); }));
		}

		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Gate held through slides (legato)", "",
		                                     &module->legatoTies));
		menu->addChild(createBoolPtrMenuItem(
			"Own glide on the V/Oct output (disables itself when ATEK303 is attached)", "",
			&module->internalGlide));
		menu->addChild(createIndexSubmenuItem("Base octave",
			{"C1 (-3)", "C2 (-2)", "C3 (-1)", "C4 (0)", "C5 (+1)"},
			[=]() { return clamp(module->octaveBase + 3, 0, 4); },
			[=](int i) { module->octaveBase = i - 3; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Accent as velocity CV", "", &module->accentAsCV));
		menu->addChild(createIndexSubmenuItem("Accent level",
			{"10 V", "8 V", "5 V"},
			[=]() {
				const float L[3] = {10.f, 8.f, 5.f};
				for (int i = 0; i < 3; i++)
					if (module->accentLevel == L[i]) return i;
				return 1;
			},
			[=](int i) { const float L[3] = {10.f, 8.f, 5.f}; module->accentLevel = L[i]; }));
		menu->addChild(createIndexSubmenuItem("Base level (unaccented note)",
			{"0 V", "1 V", "2 V", "3 V"},
			[=]() { return clamp((int) std::round(module->accentBase), 0, 3); },
			[=](int i) { module->accentBase = (float) i; }));
	}
};

Model* modelAtek303Seq = createModel<Atek303Seq, Atek303SeqWidget>("ATEK303SEQ");
