#include "plugin.hpp"
#include "ui/AtekWidgets.hpp"
#include "AcidPattern.hpp"

#include <atomic>
#include <cmath>

// ---------------------------------------------------------------------------
// ATEK303 SEQ — generador de patrones acid.
//
// No es un secuenciador de pasos: no hay knobs ni switches por paso. Hay un botón
// GENERATE, unos mandos de carácter y un algoritmo. El modelo dual v4 vive en
// `AcidPattern.hpp`, aparte y sin dependencias de Rack, para poder probarlo en el banco
// offline (`tools/pattern_v4.cpp`); aquí queda el módulo: reloj, gate, salidas y panel.
// ---------------------------------------------------------------------------

static const int MAX_STEPS = ACID_MAX_STEPS;

struct Atek303Seq : Module {
	enum PatternAction { ACTION_NONE, MUTATE_TIME, MUTATE_PITCH, MUTATE_ARTICULATION, UNDO_MUTATION };
	enum ParamId {
		GENERATE_PARAM, STEPS_PARAM, GATELEN_PARAM, DENSITY_PARAM, RANGE_PARAM,
		ACCENT_PARAM, SLIDEAMT_PARAM, ROOT_PARAM, SCALE_PARAM,
		// Los IDs nuevos se añaden al final: los patches v3/v4 conservan intactos 0..8.
		SEED_LOCK_PARAM, MUTATE_TIME_PARAM, MUTATE_PITCH_PARAM, MUTATE_ARTICULATION_PARAM,
		PARAMS_LEN
	};
	static_assert(GENERATE_PARAM == 0 && SCALE_PARAM == 8 && SEED_LOCK_PARAM == 9,
	              "No cambiar los IDs históricos de parámetros de ATEK303SEQ");
	enum InputId { CLOCK_INPUT, RESET_INPUT, GEN_INPUT, INPUTS_LEN };
	enum OutputId { VOCT_OUTPUT, GATE_OUTPUT, ACCENT_OUTPUT, SLIDE_OUTPUT, EOC_OUTPUT, OUTPUTS_LEN };
	enum LightId {
		ENUMS(STEP_LIGHT, MAX_STEPS * 3),
		GENERATE_LIGHT, SEED_LOCK_LIGHT, MUTATE_TIME_LIGHT, MUTATE_PITCH_LIGHT,
		MUTATE_ARTICULATION_LIGHT, LIGHTS_LEN
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
	int octaveBase = -2;   // en VCV 0 V es C4; una línea de 303 vive por C2
	bool accentAsCV = false;
	float accentLevel = 8.f;
	float accentBase = 2.f;

	Atek303Seq() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(GENERATE_PARAM,
		             "New seed; with BLOCK, mutate time + notes/octaves + slides/accents");
		configParam(STEPS_PARAM, 1.f, 16.f, 16.f, "Steps");
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

	bool mutatePattern(AcidPatternMutator::Layer layer) {
		const AcidPatternV4 before = pattern;
		const uint32_t beforeCounter = mutationCounter;
		const int operations = layer == AcidPatternMutator::Articulation ? 3 : 2;
		uint32_t lastMutationIndex = beforeCounter;
		if (AcidPatternMutator::mutateBurst(pattern, layer, beforeCounter + 1,
		                                    operations, scaleIdx(), lastMutationIndex)) {
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
		for (int i = 0; i < 3; i++) {
			const int operations = layers[i] == AcidPatternMutator::Articulation ? 3 : 2;
			uint32_t lastMutationIndex = cursor;
			if (!AcidPatternMutator::mutateBurst(pattern, layers[i], cursor + 1,
			                                     operations, scaleIdx(), lastMutationIndex)) {
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

	bool undoMutation() {
		if (!hasUndo) return false;
		pattern = undoPattern;
		mutationCounter = undoMutationCounter;
		hasUndo = false;
		renderPattern();
		genPulse.trigger(0.12f);
		return true;
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
		if (genPressed || genTrigged) {
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

		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			// Reset deja el paso 0 preparado, no en curso: lo arranca el siguiente flanco,
			// que es lo que espera cualquier reloj con reset.
			step = 0;
			stepTime = 0.f;
			clockStarted = false;
		}
		const int len = length();
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (!clockStarted) {
				// Primer flanco: arranca en el paso 0 y no mide periodo, porque lo que
				// llevaba contado es el tiempo desde que se cargó el módulo.
				clockStarted = true;
				step = 0;
			}
			else {
				if (stepTime > 1e-4f)
					clockPeriod = clamp(stepTime, 0.002f, 4.f);
				step = (step + 1) % len;
			}
			stepTime = 0.f;
			if (step == 0)
				eocPulse.trigger(1e-3f);
		}
		if (step >= len)
			step = 0;
		stepTime += args.sampleTime;

		const int next = (step + 1) % len;
		const bool active = clockStarted && gen.gate[step];
		const bool nextActive = clockStarted && gen.gate[next];
		const bool tieIn = active && gen.tie[step];
		const bool tieOut = active && nextActive && gen.tie[next];
		const int stepSemi = AcidGen::semiOf(gen.deg[step], gen.oct[step], scaleIdx());
		const int nextSemi = AcidGen::semiOf(gen.deg[next], gen.oct[next], scaleIdx());
		// PASOS puede cerrar el bucle antes del paso 16 y ESCALA puede colapsar dos grados.
		// Se valida la transición que realmente va a sonar, no solo la que se generó.
		const bool slide = active && gen.slide[step] && nextActive
		                && !tieIn && !gen.tie[next] && stepSemi != nextSemi;

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

		// Una fila de LEDs para ver el patrón, sin controles: apagado = silencio,
		// verde = ataque, azul = tie, ámbar = slide, rojo = acento; el paso en curso brilla.
		for (int i = 0; i < MAX_STEPS; i++) {
			float r = 0.f, g = 0.f, b = 0.f;
			if (i < len && gen.gate[i]) {
				if (gen.tie[i]) { r = 0.1f; g = 0.45f; b = 1.f; }
				else if (gen.accent[i]) { r = 1.f; g = 0.15f; }
				else if (gen.slide[i]) { r = 0.8f; g = 0.7f; }
				else { g = 0.8f; }
			}
			const float dim = (i == step) ? 1.f : 0.28f;
			lights[STEP_LIGHT + i * 3 + 0].setBrightness(r * dim);
			lights[STEP_LIGHT + i * 3 + 1].setBrightness(g * dim);
			lights[STEP_LIGHT + i * 3 + 2].setBrightness(b * dim + (i == step ? 0.25f : 0.f));
		}
		lights[GENERATE_LIGHT].setBrightnessSmooth(
			genPulse.process(args.sampleTime) ? 1.f : 0.f, args.sampleTime);
		lights[SEED_LOCK_LIGHT].setBrightness(seedLocked ? 1.f : 0.f);
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
		return rootJ;
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
		if (json_t* j = json_object_get(rootJ, "mutationCounter"))
			mutationCounter = (uint32_t) json_integer_value(j);
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
		nvgFillColor(args.vg, nvgRGB(0xfb, 0xd5, 0x18));   // el amarillo del sticker
		nvgText(args.vg, box.size.x / 2, box.size.y / 2, text, NULL);
	}
};

struct Atek303SeqWidget : ModuleWidget {
	// Rejilla del panel, en mm. tools/panel.py usa las mismas para la raya azul, el
	// logo y el sticker acid, así que si aquí se mueve algo hay que moverlo allí.
	static constexpr float W = 101.6f;         // 20 HP
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
	static constexpr float IO_SEC_Y = 96.5f;   // rótulos ENTRADAS / SALIDAS
	static constexpr float IO_LABEL_Y = 100.5f;
	static constexpr float IO_JACK_Y = 110.0f;
	static constexpr float IO_SPLIT_X = 42.5f; // separador vertical entre los dos bloques

	Atek303SeqWidget(Atek303Seq* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ATEK303SEQ-light.svg"),
		                     asset::plugin(pluginInstance, "res/ATEK303SEQ.svg")));

		auto label = [&](const char* text, float x, float y, float w, float size) {
			auto* l = new AnimatekUI::TextLabel(text, mm2px(Vec(x - w * 0.5f, y)),
			                                    mm2px(Vec(w, 4.f)));
			l->fontSize = size;
			addChild(l);
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
			const float x0 = 9.5f, x1 = W - 9.5f;
			for (int i = 0; i < MAX_STEPS; i++) {
				const float x = x0 + (x1 - x0) * i / (float)(MAX_STEPS - 1);
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
			addChild(new GroupBox(g.name, mm2px(Vec(g.x, GRP_Y)), mm2px(Vec(g.w, GRP_H))));
			const float step = g.w / g.n;
			for (int i = 0; i < g.n; i++) {
				const float x = slotX(g, i);
				label(g.k[i].name, x, KNOB_LABEL_Y, step - 0.8f, 6.8f);
				addParam(createParamCentered<RoundBlackKnob>(
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
				addChild(ro);
			}
		}

		// --- MUTACIÓN -----------------------------------------------------------
		// Fila de performance: generar una identidad, bloquearla y derivar tres familias
		// de variaciones sin entrar en el menú. BLOCK es latch; los otros son trigger.
		addChild(new GroupBox("MUTATION", mm2px(Vec(5.f, MUT_Y)), mm2px(Vec(W - 10.f, MUT_H))));
		{
			static const float bx[5] = {15.5f, 33.3f, 50.8f, 68.3f, 86.1f};
			static const char* names[5] = {"GENERATE", "BLOCK", "MUT TIME",
			                               "MUT NOTE/OCT", "MUT SLD/ACC"};
			addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(bx[0], BTN_Y)), module,
			         Atek303Seq::GENERATE_PARAM, Atek303Seq::GENERATE_LIGHT));
			addParam(createLightParamCentered<VCVLightBezelLatch<>>(mm2px(Vec(bx[1], BTN_Y)), module,
			         Atek303Seq::SEED_LOCK_PARAM, Atek303Seq::SEED_LOCK_LIGHT));
			addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(bx[2], BTN_Y)), module,
			         Atek303Seq::MUTATE_TIME_PARAM, Atek303Seq::MUTATE_TIME_LIGHT));
			addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(bx[3], BTN_Y)), module,
			         Atek303Seq::MUTATE_PITCH_PARAM, Atek303Seq::MUTATE_PITCH_LIGHT));
			addParam(createLightParamCentered<VCVLightBezel<>>(mm2px(Vec(bx[4], BTN_Y)), module,
			         Atek303Seq::MUTATE_ARTICULATION_PARAM, Atek303Seq::MUTATE_ARTICULATION_LIGHT));
			for (int i = 0; i < 5; i++)
				label(names[i], bx[i], BTN_LABEL_Y, 17.f, i < 2 ? 6.8f : 6.2f);
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

	void appendContextMenu(Menu* menu) override {
		Atek303Seq* module = getModule<Atek303Seq>();
		appendPanelThemeMenu(menu);
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
