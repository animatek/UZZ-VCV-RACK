#include "plugin.hpp"
#include "rosic_Open303.h"
#include "AtekOsc.hpp"
#include "AtekFilter.hpp"
#include "ui/AtekWidgets.hpp"

#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// ATEK303 — voz TB-303.
//
// Motor actual: Open303 (Robin Schmidt, MIT — ver dep/open303/PROVENANCE.md).
// Es el motor de referencia; el motor propio derivado del esquema llegará como
// segunda opción seleccionable, ver ANALISIS.md §9.
// ---------------------------------------------------------------------------

// El Env Mod del 303 lleva un pot logarítmico (VR5, 50 kΩ tipo A): casi toda la
// acción está en el último tercio del recorrido. Aproximamos esa curva con x³
// (12,5 % a mitad de recorrido, que es lo típico de un pot A).
static inline float envModCurve(float x, bool logTaper) {
	return logTaper ? x * x * x : x;
}

// El esquema da el decay como tiempo de 100 % a 10 % (200 ms – 2,5 s), y Open303
// trabaja con la constante de tiempo τ. Son escalas distintas: T = τ·ln(10) = 2,303·τ.
// Mostramos T, que es lo que uno piensa cuando piensa "decay".
static const double DECAY_T_OVER_TAU = 2.302585092994046;

struct DecayQuantity : ParamQuantity {
	float getDisplayValue() override;
	void setDisplayValue(float displayValue) override;
};

struct EnvModQuantity : ParamQuantity {
	float getDisplayValue() override;
	void setDisplayValue(float displayValue) override;
};

struct Atek303 : Module {
	enum ParamId {
		TUNING_PARAM,
		CUTOFF_PARAM,
		RESONANCE_PARAM,
		ENVMOD_PARAM,
		DECAY_PARAM,
		ACCENT_PARAM,
		WAVEFORM_PARAM,
		// Atenuverters de los cinco CV. Van al final para que los siete controles
		// originales del 303 conserven su orden histórico.
		CUTOFF_CV_PARAM,
		RESONANCE_CV_PARAM,
		ENVMOD_CV_PARAM,
		DECAY_CV_PARAM,
		ACCENT_CV_PARAM,
		TUNING_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,
		GATE_INPUT,
		ACCENT_INPUT,
		SLIDE_INPUT,
		CUTOFF_CV_INPUT,
		RESONANCE_CV_INPUT,
		ENVMOD_CV_INPUT,
		DECAY_CV_INPUT,
		ACCENT_CV_INPUT,
		TUNING_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		GATE_LIGHT,
		ACCENT_LIGHT,
		SLIDE_LIGHT,
		LIGHTS_LEN
	};

	rosic::Open303 core;
	AtekOsc atekOsc;
	AtekFilter atekFilter;

	// 0 = oscilador de tablas de Open303, 1 = oscilador ATEK modelado del esquema
	int oscEngine = 1;
	// 0 = TeeBeeFilter de Open303, 1 = ladder de diodos ATEK
	int filterEngine = 1;
	// El pot real VR4 es 50 kΩ tipo B (lineal). Open303 le mete una curva
	// (1−e^(−3r))/(1−e^(−3)) que al 25 % del knob ya da un 55 % de resonancia.
	bool resonanceLinear = true;
	int filterDriveIdx = 1;      // 0 / +6 / +12 dB

	// Rango del decay. El de Open303 sale de su wrapper VST; el del circuito sale de
	// la nota del esquema (T de 100 % a 10 %: 200 ms – 2,5 s) convertida a τ.
	int decayRangeIdx = 1;       // 0 = Open303, 1 = circuito

	// Saturación del OTA BA662A, antes del VCA. Umbrales elegidos sobre el nivel
	// medido antes del VCA (pico 0,36 / rms 0,14 en un patch típico).
	int otaIdx = 1;              // ninguna / suave / fuerte

	// Deriva analógica. Dos cosas distintas que van juntas:
	//  · el DAC R-2R (R74–R90, apareadas al 0,1 %) tiene un error por nota que es
	//    FIJO y repetible: cada 303 desafina de su propia manera, siempre igual.
	//  · el convertidor exponencial deriva despacio con la temperatura, y el
	//    posistor R100 compensa pero no del todo.
	int driftIdx = 1;            // ninguna / sutil / marcada / mucha
	int unitSeed = 0;            // "número de unidad": cambia el patrón del DAC
	float driftState = 0.f;
	// Deriva del cutoff, con su propio paseo aleatorio. El VCO lleva el posistor R100
	// compensando la temperatura; la red de polarización del VCF (Q9/Q10/Q11) NO lleva
	// compensación ninguna, así que el corte deriva más que la afinación. Y en una
	// línea monofónica el oído distingue mucho mejor un cambio de timbre que unos
	// cents de desafinación.
	float driftFilter = 0.f;
	static constexpr float DRIFT_TAU = 25.f;   // s

	// Error del DAC en cents para una nota, determinista y repetible.
	float dacErrorCents(int note) const {
		uint32_t h = (uint32_t) note * 2654435761u ^ (uint32_t) unitSeed * 40503u;
		h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
		return ((h & 0xffffu) / 32768.f - 1.f);   // −1 .. +1
	}
	double decayTauMin() const { return decayRangeIdx == 0 ? 200.0 : 200.0 / DECAY_T_OVER_TAU; }
	double decayTauMax() const { return decayRangeIdx == 0 ? 2000.0 : 2500.0 / DECAY_T_OVER_TAU; }
	double decayTau(double knob) const {
		const double lo = decayTauMin(), hi = decayTauMax();
		return lo * std::pow(hi / lo, knob);
	}
	// Valores elegidos a oído el 2026-08-17: 44 % de ancho y droop fuerte.
	int pulseWidthIdx = 0;   // 44 / 47 / 50 / 53 / 56 %
	// Afinados a oído el 2026-08-18: esquina redonda y droop marcado.
	int sawResetIdx = 3;     // 1 / 3 / 8 / 20 µs
	int sawDroopIdx = 3;     // 0,7 / 3 / 8 / 15 Hz
	int droopIdx = 4;        // sin droop / 8 / 15 / 30 / 60 Hz

	double sampleRate = 0.0;
	dsp::ClockDivider paramDivider;

	// Estado de la lógica de gate/slide
	bool gateHigh = false;
	bool slideHigh = false;
	bool noteHeld = false;
	int heldNote = -1;

	// Opciones (menú contextual)
	bool quantizePitch = false;  // opcional: el 303 real recibe semitonos de un DAC R-2R
	bool limitRange = false;     // opcional: el teclado original solo alcanza C1–C4
	bool autoLegato = false;     // slide al cambiar el pitch con el gate alto
	bool accentAccum = true;     // acumulación del acento entre notas seguidas (red C72)
	bool envModLog = true;       // curva del pot VR5 (tipo A) en vez de lineal

	// Saturación de salida. En el hardware, después del VCA (OTA BA662A) vienen el
	// mixer discreto de dos transistores (Q33/Q34) y el amplificador IC14 LA4140,
	// los tres con recorrido limitado. El acento sube mucho el nivel — Open303
	// llega a multiplicar la envolvente de amplitud por (0,45 + 4·acento) — así que
	// las notas acentuadas entran en esa zona y las normales no. Open303 no modela
	// nada de esto: multiplica y ya.
	int satIdx = 1;              // ninguna / suave / fuerte

	// Slide. El hardware desliza la tensión de control (R91 1 MΩ · C35 0,22 µF) y el
	// conversor exponencial la pasa a Hz, así que el glide es constante en
	// semitonos/segundo. Open303 desliza los Hz linealmente, que es otra curva.
	bool slidePitchDomain = true;
	int slideTauIdx = 1;         // 60 / 120 / 220 ms — 120 por defecto: con 60 el
	                             // slide pasa tan rápido que apenas se oye como slide
	float outputLevel = 5.f;     // V de pico nominales
	// El knob de volumen ya no está en el panel: el nivel del motor queda fijo en el
	// valor que tenía por defecto, y el volumen se hace fuera con un VCA.
	static constexpr double FIXED_VOLUME_DB = -6.0;

	// Rango del original: 3 octavas. Con el convenio de VCV (0 V = C4 = MIDI 60),
	// eso es MIDI 24–60, es decir de −3 V a 0 V.
	static const int NOTE_MIN = 24;
	static const int NOTE_MAX = 60;

	// Acumuladores del acento. El circuito tiene DOS etapas con constantes distintas
	// trabajando a la vez — Q36 con C72 (10 µF y resistencias de 10 kΩ) y Q38 con
	// C55 → D37 → R152 → C62 — y por eso al comparar gustaban tanto 100 ms como
	// 450 ms: cada una aportaba la mitad de lo que hace el hardware.
	float accentEnvFast = 0.f;
	float accentEnvSlow = 0.f;
	static constexpr float ACCENT_TAU_FAST = 0.10f;   // s — el empuje inmediato
	static constexpr float ACCENT_TAU_SLOW = 0.45f;   // s — el arrastre entre pasos
	int accentModeIdx = 2;                            // rápida / lenta / las dos
	static constexpr float ACCENT_STEP = 0.55f;   // cuánto carga cada acento
	static constexpr float ACCENT_DEPTH = 0.8f;   // cuánto sube el acento efectivo

	Atek303SeqMessage expMsgA, expMsgB;

	Atek303() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Lado receptor del expander: el SEQ escribe aquí cuando está pegado a la
		// izquierda. Un cable en cualquier jack siempre manda sobre el expander.
		leftExpander.producerMessage = &expMsgA;
		leftExpander.consumerMessage = &expMsgB;

		// Rangos tomados del wrapper VST original de Open303, para que el módulo
		// se comporte exactamente igual que la referencia.
		configParam(TUNING_PARAM, 0.f, 1.f, 0.5f, "Tuning", " Hz", 0.f, 80.f, 400.f);
		configParam(CUTOFF_PARAM, 0.f, 1.f, 0.35f, "Cutoff", " Hz", 2394.f / 314.f, 314.f);
		configParam(RESONANCE_PARAM, 0.f, 1.f, 0.5f, "Resonance", " %", 0.f, 100.f);
		configParam<EnvModQuantity>(ENVMOD_PARAM, 0.f, 1.f, 0.5f, "Env mod", " %");
		configParam<DecayQuantity>(DECAY_PARAM, 0.f, 1.f, 0.4f, "Decay", " ms");
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.5f, "Accent", " %", 0.f, 100.f);
		configSwitch(WAVEFORM_PARAM, 0.f, 1.f, 1.f, "Waveform", {"Sawtooth", "Square"});

		// El 303 no tiene VCA de salida: el pot de volumen del original es el del
		// mezclador. En VCV eso es un módulo aparte, así que el motor queda fijo en
		// el −6 dB que era el valor por defecto del knob y la voz sale a nivel pleno.
		// Atenuverter de cada CV, bajo su mando. Arranca en cero, como manda la
		// costumbre: se enchufa el cable y se sube el trimpot.
		configParam(CUTOFF_CV_PARAM,    -1.f, 1.f, 0.f, "Cut off CV", " %", 0.f, 100.f);
		configParam(RESONANCE_CV_PARAM, -1.f, 1.f, 0.f, "Resonance CV", " %", 0.f, 100.f);
		configParam(ENVMOD_CV_PARAM,    -1.f, 1.f, 0.f, "Env mod CV", " %", 0.f, 100.f);
		configParam(DECAY_CV_PARAM,     -1.f, 1.f, 0.f, "Decay CV", " %", 0.f, 100.f);
		configParam(ACCENT_CV_PARAM,    -1.f, 1.f, 0.f, "Accent CV", " %", 0.f, 100.f);
		configParam(TUNING_CV_PARAM,    -1.f, 1.f, 0.f, "Tuning CV", " %", 0.f, 100.f);

		configInput(VOCT_INPUT, "1V/oct pitch");
		configInput(GATE_INPUT, "Gate");
		configInput(ACCENT_INPUT, "Accent");
		configInput(SLIDE_INPUT, "Slide");
		configInput(CUTOFF_CV_INPUT, "Cut off CV");
		configInput(RESONANCE_CV_INPUT, "Resonance CV");
		configInput(ENVMOD_CV_INPUT, "Env mod CV");
		configInput(DECAY_CV_INPUT, "Decay CV");
		configInput(ACCENT_CV_INPUT, "Accent CV");
		configInput(TUNING_CV_INPUT, "Tuning CV");
		configOutput(AUDIO_OUTPUT, "Audio");

		paramDivider.setDivision(16);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		core.allNotesOff();
		gateHigh = slideHigh = noteHeld = false;
		heldNote = -1;
		accentEnvFast = accentEnvSlow = 0.f;
	}

	// Acento efectivo: el knob, más lo que haya cargado el acumulador de acentos
	// consecutivos. Se llama justo antes de disparar la nota, porque Open303
	// congela el valor del acento en el momento del trigger.
	float accentAmount() const {
		static const float WF[3] = {1.f, 0.f, 0.5f};
		static const float WS[3] = {0.f, 1.f, 0.5f};
		const int i = clamp(accentModeIdx, 0, 2);
		return WF[i] * accentEnvFast + WS[i] * accentEnvSlow;
	}

	// Un knob más su CV. Convenio: ±5 V con el atenuverter al máximo recorren el
	// mando entero, que es lo habitual en modular. El jack sin cable no suma nada,
	// así que un patch sin CV se comporta exactamente igual que antes.
	float modKnob(int knobParam, int cvParam, int cvInput) {
		float v = params[knobParam].getValue();
		if (inputs[cvInput].isConnected())
			v += params[cvParam].getValue() * inputs[cvInput].getVoltage() * 0.2f;
		return clamp(v, 0.f, 1.f);
	}

	double effectiveAccent() {
		const double base = 100.0 * modKnob(ACCENT_PARAM, ACCENT_CV_PARAM, ACCENT_CV_INPUT);
		if (!accentAccum)
			return base;
		return std::min(100.0, base * (1.0 + ACCENT_DEPTH * accentAmount()));
	}

	void updateParams(float voct) {
		core.setTuning(400.0 + 80.0 * modKnob(TUNING_PARAM, TUNING_CV_PARAM, TUNING_CV_INPUT));
		double cutoffHz = 314.0 * std::pow(2394.0 / 314.0,
		                                   modKnob(CUTOFF_PARAM, CUTOFF_CV_PARAM, CUTOFF_CV_INPUT));
		{
			static const float CUT_DRIFT[4] = {0.f, 0.015f, 0.045f, 0.10f};
			cutoffHz *= 1.0 + driftFilter * CUT_DRIFT[clamp(driftIdx, 0, 3)];
		}
		core.setCutoff(cutoffHz);
		{
			double res = modKnob(RESONANCE_PARAM, RESONANCE_CV_PARAM, RESONANCE_CV_INPUT);
			if (resonanceLinear) {
				// Pre-deformamos con la inversa de la curva de Open303, de modo que
				// la resonancia efectiva acabe siendo lineal con el knob, como el pot.
				const double s3 = 1.0 - std::exp(-3.0);
				res = -std::log(1.0 - res * s3) / 3.0;
			}
			core.setResonance(100.0 * res);
		}
		core.setEnvMod(100.0 * envModCurve(
			modKnob(ENVMOD_PARAM, ENVMOD_CV_PARAM, ENVMOD_CV_INPUT), envModLog));
		core.setDecay(decayTau(modKnob(DECAY_PARAM, DECAY_CV_PARAM, DECAY_CV_INPUT)));
		// El acento cortocircuita el pot vía IC12, o sea que fuerza el decay MÍNIMO.
		core.setAccentDecay(decayTauMin());
		core.setAccent(effectiveAccent());
		core.setVolume(FIXED_VOLUME_DB);
		core.setWaveform(params[WAVEFORM_PARAM].getValue());
		static const double SLIDE_TAU[3] = {60.0, 120.0, 220.0};
		core.setSlideInPitchDomain(slidePitchDomain);
		core.setSlideTimeConstant(SLIDE_TAU[clamp(slideTauIdx, 0, 2)]);
		atekOsc.setWaveform(params[WAVEFORM_PARAM].getValue());
		static const double PW[5] = {0.44, 0.47, 0.50, 0.53, 0.56};
		static const double DROOP[5] = {0.5, 8.0, 15.0, 30.0, 60.0};
		atekOsc.setShape(PW[clamp(pulseWidthIdx, 0, 4)], DROOP[clamp(droopIdx, 0, 4)]);
		static const double SAW_RESET[4] = {1.0, 3.0, 8.0, 20.0};
		static const double SAW_DROOP[4] = {0.7, 3.0, 8.0, 15.0};
		atekOsc.setSawShape(SAW_RESET[clamp(sawResetIdx, 0, 3)],
		                    SAW_DROOP[clamp(sawDroopIdx, 0, 3)]);
		core.externalOscillator = (oscEngine == 1) ? &atekOsc : NULL;
		static const double DRIVE[3] = {1.0, 2.0, 4.0};   // 0 / +6 / +12 dB
		atekFilter.setDrive(DRIVE[clamp(filterDriveIdx, 0, 2)]);
		if ((filterEngine == 1) != (core.externalFilter != NULL)) {
			core.externalFilter = (filterEngine == 1) ? &atekFilter : NULL;
			if (core.externalFilter) {
				atekFilter.setSampleRate(4.0 * sampleRate);   // el 4x interno de Open303
				atekFilter.reset();
			}
		}

		// En modo continuo, la parte fraccionaria del V/oct va por el pitch bend,
		// que se aplica después del limitador de slew (no interfiere con el slide).
		static const double OTA[3] = {0.0, 0.6, 0.3};
		core.otaHeadroom = OTA[clamp(otaIdx, 0, 2)];

		// Deriva lenta: paseo aleatorio filtrado, se actualiza al ritmo del divisor
		// El error del DAC (fijo por nota) se escala más que la deriva térmica: es lo
		// que da la sensación de "esta máquina no afina", mientras que una deriva lenta
		// muy grande suena a sinte estropeado.
		static const float DAC_CENTS[4]   = {0.f, 1.5f, 5.f, 12.f};
		static const float DRIFT_CENTS[4] = {0.f, 1.5f, 3.5f, 6.f};
		const float dt = 16.f / (float) std::max(1.0, sampleRate);
		driftState += -driftState * (dt / DRIFT_TAU) + random::normal() * std::sqrt(dt) * 0.25f;
		driftState = clamp(driftState, -3.f, 3.f);
		driftFilter += -driftFilter * (dt / DRIFT_TAU) + random::normal() * std::sqrt(dt) * 0.25f;
		driftFilter = clamp(driftFilter, -3.f, 3.f);

		const int i = clamp(driftIdx, 0, 3);
		double bendSemis = 0.0;
		if (!quantizePitch)
			bendSemis += voct * 12.f - std::round(voct * 12.f);
		bendSemis += (dacErrorCents(voltsToNote(voct)) * DAC_CENTS[i]
		              + driftState * DRIFT_CENTS[i]) / 100.0;
		core.setPitchBend(bendSemis);
	}

	int voltsToNote(float voct) const {
		const int note = (int) std::round(voct * 12.f) + 60;
		if (limitRange)
			return clamp(note, (int) NOTE_MIN, (int) NOTE_MAX);
		return clamp(note, 0, 127);
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != sampleRate) {
			sampleRate = args.sampleRate;
			core.setSampleRate(sampleRate);
		}

		const Atek303SeqMessage* exp = NULL;
		if (leftExpander.module && leftExpander.module->model == modelAtek303Seq)
			exp = (const Atek303SeqMessage*) leftExpander.consumerMessage;

		const float voct = inputs[VOCT_INPUT].isConnected()
		                 ? inputs[VOCT_INPUT].getVoltage()
		                 : (exp ? exp->voct : 0.f);

		if (paramDivider.process())
			updateParams(voct);

		// Schmitt manual para gate y slide (0,1 V / 1,0 V)
		const float gateV = inputs[GATE_INPUT].isConnected()
		                  ? inputs[GATE_INPUT].getVoltage() : (exp && exp->gate ? 10.f : 0.f);
		const float slideV = inputs[SLIDE_INPUT].isConnected()
		                   ? inputs[SLIDE_INPUT].getVoltage() : (exp && exp->slide ? 10.f : 0.f);
		const float accentV = inputs[ACCENT_INPUT].isConnected()
		                    ? inputs[ACCENT_INPUT].getVoltage() : (exp && exp->accent ? 10.f : 0.f);

		const bool gate = gateV >= (gateHigh ? 0.1f : 1.f);
		slideHigh = slideV >= (slideHigh ? 0.1f : 1.f);
		const bool accent = accentV >= 1.f;

		const int note = voltsToNote(voct);

		// Descarga de las dos etapas del acento
		accentEnvFast -= accentEnvFast * args.sampleTime / ACCENT_TAU_FAST;
		accentEnvSlow -= accentEnvSlow * args.sampleTime / ACCENT_TAU_SLOW;

		if (gate && !gateHigh) {
			// Flanco de subida. Si ya había una nota sonando (por slide o legato),
			// Open303 desliza en vez de re-disparar la envolvente: es justo lo que
			// hace el hardware.
			if (accent) {
				core.setAccent(effectiveAccent());
				accentEnvFast = std::min(1.f, accentEnvFast + ACCENT_STEP);
				accentEnvSlow = std::min(1.f, accentEnvSlow + ACCENT_STEP);
			}
			core.noteOn(note, accent ? 127 : 64);
			heldNote = note;
			noteHeld = true;
		}
		else if (!gate && gateHigh) {
			// Flanco de bajada. Con SLIDE alto mantenemos la nota abierta para
			// deslizar hacia la siguiente (legato).
			if (!slideHigh) {
				core.allNotesOff();
				noteHeld = false;
				heldNote = -1;
			}
		}
		else if (autoLegato && gate && noteHeld && note != heldNote) {
			// Auto-legato: el gate sigue alto y ha cambiado la nota → slide.
			// Desactivado por defecto: un cuantizador o un S&H delante mueven el
			// pitch con el gate abierto y provocarían slides fantasma.
			core.noteOn(note, accent ? 127 : 64);
			heldNote = note;
		}
		gateHigh = gate;

		float out = (float) core.getSample() * outputLevel;
		static const float SAT[3] = {0.f, 7.f, 4.5f};
		const float sat = SAT[clamp(satIdx, 0, 2)];
		if (sat > 0.f)
			out = sat * std::tanh(out / sat);
		outputs[AUDIO_OUTPUT].setVoltage(clamp(out, -12.f, 12.f));

		lights[GATE_LIGHT].setBrightness(noteHeld ? 1.f : 0.f);
		lights[ACCENT_LIGHT].setBrightness(accent ? 1.f : 0.f);
		lights[SLIDE_LIGHT].setBrightness(slideHigh ? 1.f : 0.f);
	}

	// -----------------------------------------------------------------------
	// Los dos modelos de sonido. Solo agrupan las decisiones *estructurales*:
	// qué motor corre cada bloque y qué comportamientos del circuito están
	// activos. Los ajustes de calibración (ancho de pulso, droop, drive, τ del
	// slide, etapas del acento) quedan fuera a propósito: son gusto dentro de un
	// modelo, no parte de él. Cuantización y límite de rango también quedan fuera:
	// son opciones de interpretación del V/Oct, no del motor de sonido.
	// -----------------------------------------------------------------------
	struct SoundModel {
		int oscEngine, filterEngine, decayRangeIdx, satIdx, otaIdx, driftIdx;
		bool resonanceLinear, slidePitchDomain, accentAccum, envModLog;
	};

	static SoundModel modelCircuit() { return {1, 1, 1, 2, 1, 1, true,  true,  true,  true }; }
	static SoundModel modelOpen303() { return {0, 0, 0, 0, 0, 0, false, false, false, false}; }

	bool matchesModel(const SoundModel& m) const {
		return oscEngine == m.oscEngine && filterEngine == m.filterEngine
		    && decayRangeIdx == m.decayRangeIdx && satIdx == m.satIdx
		    && otaIdx == m.otaIdx && driftIdx == m.driftIdx
		    && resonanceLinear == m.resonanceLinear && slidePitchDomain == m.slidePitchDomain
		    && accentAccum == m.accentAccum && envModLog == m.envModLog;
	}

	/** 0 = circuito, 1 = Open303, 2 = personalizado */
	int detectModel() const {
		if (matchesModel(modelCircuit()))
			return 0;
		if (matchesModel(modelOpen303()))
			return 1;
		return 2;
	}

	void applyModel(int which) {
		const SoundModel m = (which == 0) ? modelCircuit() : modelOpen303();
		oscEngine = m.oscEngine;
		filterEngine = m.filterEngine;
		decayRangeIdx = m.decayRangeIdx;
		satIdx = m.satIdx;
		otaIdx = m.otaIdx;
		driftIdx = m.driftIdx;
		resonanceLinear = m.resonanceLinear;
		slidePitchDomain = m.slidePitchDomain;
		accentAccum = m.accentAccum;
		envModLog = m.envModLog;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "quantizePitch", json_boolean(quantizePitch));
		json_object_set_new(rootJ, "limitRange", json_boolean(limitRange));
		json_object_set_new(rootJ, "autoLegato", json_boolean(autoLegato));
		json_object_set_new(rootJ, "accentAccum", json_boolean(accentAccum));
		json_object_set_new(rootJ, "oscEngine", json_integer(oscEngine));
		json_object_set_new(rootJ, "filterEngine", json_integer(filterEngine));
		json_object_set_new(rootJ, "resonanceLinear", json_boolean(resonanceLinear));
		json_object_set_new(rootJ, "filterDriveIdx", json_integer(filterDriveIdx));
		json_object_set_new(rootJ, "decayRangeIdx", json_integer(decayRangeIdx));
		json_object_set_new(rootJ, "otaIdx", json_integer(otaIdx));
		json_object_set_new(rootJ, "driftIdx", json_integer(driftIdx));
		json_object_set_new(rootJ, "unitSeed", json_integer(unitSeed));
		json_object_set_new(rootJ, "pulseWidthIdx", json_integer(pulseWidthIdx));
		json_object_set_new(rootJ, "droopIdx", json_integer(droopIdx));
		json_object_set_new(rootJ, "sawResetIdx", json_integer(sawResetIdx));
		json_object_set_new(rootJ, "sawDroopIdx", json_integer(sawDroopIdx));
		json_object_set_new(rootJ, "accentModeIdx", json_integer(accentModeIdx));
		json_object_set_new(rootJ, "envModLog", json_boolean(envModLog));
		json_object_set_new(rootJ, "satIdx", json_integer(satIdx));
		json_object_set_new(rootJ, "slidePitchDomain", json_boolean(slidePitchDomain));
		json_object_set_new(rootJ, "slideTauIdx", json_integer(slideTauIdx));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		if (json_t* j = json_object_get(rootJ, "quantizePitch"))
			quantizePitch = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "limitRange"))
			limitRange = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "autoLegato"))
			autoLegato = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "accentAccum"))
			accentAccum = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "oscEngine"))
			oscEngine = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "filterEngine"))
			filterEngine = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "resonanceLinear"))
			resonanceLinear = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "filterDriveIdx"))
			filterDriveIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "decayRangeIdx"))
			decayRangeIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "otaIdx"))
			otaIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "driftIdx"))
			driftIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "unitSeed"))
			unitSeed = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "pulseWidthIdx"))
			pulseWidthIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "droopIdx"))
			droopIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "sawResetIdx"))
			sawResetIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "sawDroopIdx"))
			sawDroopIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "accentModeIdx"))
			accentModeIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "envModLog"))
			envModLog = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "satIdx"))
			satIdx = (int) json_integer_value(j);
		if (json_t* j = json_object_get(rootJ, "slidePitchDomain"))
			slidePitchDomain = json_boolean_value(j);
		if (json_t* j = json_object_get(rootJ, "slideTauIdx"))
			slideTauIdx = (int) json_integer_value(j);
	}
};

float DecayQuantity::getDisplayValue() {
	Atek303* m = dynamic_cast<Atek303*>(module);
	if (!m)
		return 0.f;
	return (float) (m->decayTau(getValue()) * DECAY_T_OVER_TAU);
}

void DecayQuantity::setDisplayValue(float displayValue) {
	Atek303* m = dynamic_cast<Atek303*>(module);
	if (!m)
		return;
	const double tau = displayValue / DECAY_T_OVER_TAU;
	const double lo = m->decayTauMin(), hi = m->decayTauMax();
	setValue(clamp((float)(std::log(tau / lo) / std::log(hi / lo)), 0.f, 1.f));
}

float EnvModQuantity::getDisplayValue() {
	Atek303* m = dynamic_cast<Atek303*>(module);
	const float v = getValue();
	return 100.f * envModCurve(v, m ? m->envModLog : true);
}

void EnvModQuantity::setDisplayValue(float displayValue) {
	Atek303* m = dynamic_cast<Atek303*>(module);
	const float f = clamp(displayValue / 100.f, 0.f, 1.f);
	setValue((m && m->envModLog) ? std::cbrt(f) : f);
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

// Rack solo ofrece CKSS horizontal de tres posiciones. Para el selector de onda usamos
// sus frames extremos como un switch de dos posiciones: izquierda = sierra, derecha = pulso.
struct CKSSHorizontal : app::SvgSwitch {
	CKSSHorizontal() {
		shadow->opacity = 0.0;
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThreeHorizontal_0.svg")));
		addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThreeHorizontal_2.svg")));
	}
};

struct Atek303Widget : ModuleWidget {
	// Rejilla del panel, en mm. tools/panel.py usa las mismas para los símbolos de
	// onda y para el logo, así que si aquí se mueve algo hay que moverlo allí.
	//
	// Tres secciones rotuladas, dos celdas cada una. Celda = etiqueta, knob, y debajo
	// el atenuverter unido por una línea a su jack de CV: la línea es la que dice que
	// ese jack y ese trimpot mandan sobre ese mando y no sobre el de al lado.
	static constexpr float COL_L = 15.5f;
	static constexpr float COL_R = 45.5f;
	static constexpr float SUB_DX = 7.0f;      // atenuverter y jack, a cada lado del eje
	static constexpr float IN_LABEL_Y = 1.8f;
	static constexpr float IN_JACK_Y  = 10.5f;
	static constexpr float LIGHT_Y    = 16.3f;
	static constexpr float SEC_Y      = 19.8f; // centro del primer rótulo de sección
	static constexpr float SEC_H      = 30.5f; // paso entre secciones
	static constexpr float CELL_DY    = 2.0f;  // del rótulo al borde de la celda
	static constexpr float KNOB_DY    = 11.1f; // centro del knob dentro de la celda
	static constexpr float SUB_DY     = 22.0f; // centro de la fila atenuverter + CV
	static constexpr float OUT_LABEL_Y = 109.0f;
	static constexpr float OUT_JACK_Y  = 118.5f;
	// El selector de onda va sobre el eje de la columna izquierda: sus dos símbolos
	// (dibujados en el SVG) caen justo bajo el atenuverter y el jack de ACCENT.
	static constexpr float WAVE_Y      = 113.5f;
	static constexpr float OUT_X       = 52.5f;

	Atek303Widget(Atek303* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ATEK303.svg")));

		auto label = [&](const char* text, float x, float y, float w, float size) {
			auto* l = new AnimatekUI::TextLabel(text, mm2px(Vec(x - w * 0.5f, y)),
			                                    mm2px(Vec(w, 4.f)));
			l->fontSize = size;
			addChild(l);
		};

		// --- entradas de nota, arriba -----------------------------------------
		// Caen sobre las mismas cuatro columnas que los atenuverters y los jacks de
		// CV de abajo, así que el panel entero se lee sobre la misma rejilla.
		struct In { const char* name; int id; float x; float w; int light; };
		static const In INS[4] = {
			{"V/OCT", Atek303::VOCT_INPUT,   COL_L - SUB_DX, 13.f, -1},
			{"GATE",  Atek303::GATE_INPUT,   COL_L + SUB_DX, 12.f, Atek303::GATE_LIGHT},
			{"ACC",   Atek303::ACCENT_INPUT, COL_R - SUB_DX, 11.f, Atek303::ACCENT_LIGHT},
			{"SLIDE", Atek303::SLIDE_INPUT,  COL_R + SUB_DX, 13.f, Atek303::SLIDE_LIGHT},
		};
		for (const In& j : INS) {
			label(j.name, j.x, IN_LABEL_Y, j.w, 7.5f);
			addInput(createInputCentered<AnimatekUI::TekInputPort>(
				mm2px(Vec(j.x, IN_JACK_Y)), module, j.id));
			if (j.light < 0)
				continue;
			if (j.light == Atek303::ACCENT_LIGHT)
				addChild(createLightCentered<SmallLight<RedLight>>(
					mm2px(Vec(j.x, LIGHT_Y)), module, j.light));
			else
				addChild(createLightCentered<SmallLight<BlueLight>>(
					mm2px(Vec(j.x, LIGHT_Y)), module, j.light));
		}

		// --- las tres secciones -----------------------------------------------
		struct Cell { const char* name; int knob, atten, cv; };
		struct Section { const char* name; Cell left, right; };
		static const Section SECTIONS[3] = {
			{"FILTER",
			 {"CUT OFF",   Atek303::CUTOFF_PARAM,    Atek303::CUTOFF_CV_PARAM,
			                                         Atek303::CUTOFF_CV_INPUT},
			 {"RESONANCE", Atek303::RESONANCE_PARAM, Atek303::RESONANCE_CV_PARAM,
			                                         Atek303::RESONANCE_CV_INPUT}},
			{"ENVELOPE",
			 {"ENV MOD",   Atek303::ENVMOD_PARAM,    Atek303::ENVMOD_CV_PARAM,
			                                         Atek303::ENVMOD_CV_INPUT},
			 {"DECAY",     Atek303::DECAY_PARAM,     Atek303::DECAY_CV_PARAM,
			                                         Atek303::DECAY_CV_INPUT}},
			{"VOICE",
			 {"ACCENT",    Atek303::ACCENT_PARAM,    Atek303::ACCENT_CV_PARAM,
			                                         Atek303::ACCENT_CV_INPUT},
			 {"TUNING",    Atek303::TUNING_PARAM,    Atek303::TUNING_CV_PARAM,
			                                         Atek303::TUNING_CV_INPUT}},
		};
		for (int i = 0; i < 3; i++) {
			const Section& sec = SECTIONS[i];
			const float secY = SEC_Y + SEC_H * i;
			addChild(new SectionLabel(sec.name, mm2px(Vec(3.f, secY - 2.f)),
			                          mm2px(Vec(60.96f - 6.f, 4.f))));

			const float top = secY + CELL_DY;
			const Cell* cells[2] = {&sec.left, &sec.right};
			for (int c = 0; c < 2; c++) {
				const Cell& cell = *cells[c];
				const float x = c ? COL_R : COL_L;
				label(cell.name, x, top, 26.f, 7.5f);
				addParam(createParamCentered<RoundLargeBlackKnob>(
					mm2px(Vec(x, top + KNOB_DY)), module, cell.knob));
				if (cell.cv < 0)
					continue;
				addParam(createParamCentered<Trimpot>(
					mm2px(Vec(x - SUB_DX, top + SUB_DY)), module, cell.atten));
				addChild(new AnimatekUI::ConnectorLine(
					mm2px(x - SUB_DX + 3.4f), mm2px(top + SUB_DY),
					mm2px(x + SUB_DX - 4.4f), mm2px(top + SUB_DY)));
				addInput(createInputCentered<AnimatekUI::TekInputPort>(
					mm2px(Vec(x + SUB_DX, top + SUB_DY)), module, cell.cv));
			}
		}

		// --- banda de abajo: onda, salida y firma ------------------------------
		addParam(createParamCentered<CKSSHorizontal>(
			mm2px(Vec(COL_L, WAVE_Y)), module, Atek303::WAVEFORM_PARAM));

		label("OUT", OUT_X, OUT_LABEL_Y, 11.f, 7.5f);
		addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(
			mm2px(Vec(OUT_X, OUT_JACK_Y)), module, Atek303::AUDIO_OUTPUT));

		auto* moduleName = new AnimatekUI::TextLabel("ATEK303", mm2px(Vec(1.5f, 120.6f)),
		                                             mm2px(Vec(26.f, 5.8f)));
		moduleName->fontSize = 13.f;
		moduleName->color = AnimatekUI::logoBlue();
		addChild(moduleName);
	}

	void appendContextMenu(Menu* menu) override {
		Atek303* module = getModule<Atek303>();

		static const char* MODEL_NAME[3] = {"Circuit", "Open303", "Custom"};

		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Sound model", MODEL_NAME[module->detectModel()],
			[=](Menu* sub) {
				sub->addChild(createCheckMenuItem("Circuit (ATEK)", "",
					[=]() { return module->detectModel() == 0; },
					[=]() { module->applyModel(0); }));
				sub->addChild(createCheckMenuItem("Open303 original", "",
					[=]() { return module->detectModel() == 1; },
					[=]() { module->applyModel(1); }));
				sub->addChild(new MenuSeparator);
				sub->addChild(createMenuLabel("\"Custom\" shows up only when the"));
				sub->addChild(createMenuLabel("settings match neither model"));
			}));

		menu->addChild(createSubmenuItem("Fine tuning", "", [=](Menu* sub) {
			sub->addChild(createMenuLabel("Oscillator"));
			sub->addChild(createIndexPtrSubmenuItem("Motor",
			                                        {"Open303 (wavetables)",
			                                         "ATEK (modelled from schematic)"},
			                                        &module->oscEngine));
			sub->addChild(createIndexPtrSubmenuItem("Pulse width (TM5)",
			                                        {"44 %", "47 %", "50 %", "53 %", "56 %"},
			                                        &module->pulseWidthIdx));
			sub->addChild(createIndexPtrSubmenuItem("Square droop",
			                                        {"no droop", "8 Hz", "15 Hz", "30 Hz", "60 Hz"},
			                                        &module->droopIdx));
			sub->addChild(createIndexPtrSubmenuItem("Saw: reset corner",
			                                        {"1 µs (sharp)", "3 µs", "8 µs",
			                                         "20 µs (round)"},
			                                        &module->sawResetIdx));
			sub->addChild(createIndexPtrSubmenuItem("Saw droop",
			                                        {"0.7 Hz (C17·R62)", "3 Hz", "8 Hz", "15 Hz"},
			                                        &module->sawDroopIdx));

			sub->addChild(new MenuSeparator);
			sub->addChild(createMenuLabel("Filter"));
			sub->addChild(createIndexPtrSubmenuItem("Motor",
			                                        {"Open303 (TeeBee)",
			                                         "ATEK (diode ladder)"},
			                                        &module->filterEngine));
			sub->addChild(createBoolPtrMenuItem("Linear resonance (VR4 linear pot)", "",
			                                    &module->resonanceLinear));
			sub->addChild(createIndexPtrSubmenuItem("Drive",
			                                        {"0 dB", "+6 dB", "+12 dB"},
			                                        &module->filterDriveIdx));

			sub->addChild(new MenuSeparator);
			sub->addChild(createMenuLabel("Envelope and accent"));
			sub->addChild(createIndexPtrSubmenuItem("Decay range",
			                                        {"Open303 (T 460 ms – 4.6 s)",
			                                         "Circuit (T 200 ms – 2.5 s)"},
			                                        &module->decayRangeIdx));
			sub->addChild(createBoolPtrMenuItem("Accent accumulation (C72 network)", "",
			                                    &module->accentAccum));
			sub->addChild(createIndexPtrSubmenuItem("Accent stages",
			                                        {"Fast only (100 ms, Q36/C72)",
			                                         "Slow only (450 ms, Q38/C62)",
			                                         "Both (as the circuit)"},
			                                        &module->accentModeIdx));
			sub->addChild(createBoolPtrMenuItem("Log Env Mod taper (VR5 audio pot)", "",
			                                    &module->envModLog));

			sub->addChild(new MenuSeparator);
			sub->addChild(createMenuLabel("Pitch and output"));
			sub->addChild(createBoolPtrMenuItem("Slide in semitones/s (as the circuit)", "",
			                                    &module->slidePitchDomain));
			sub->addChild(createIndexPtrSubmenuItem("Slide τ",
			                                        {"60 ms (Open303)", "120 ms",
			                                         "220 ms (circuit R91·C35)"},
			                                        &module->slideTauIdx));
			sub->addChild(createBoolPtrMenuItem("Quantize pitch to semitones", "",
			                                    &module->quantizePitch));
			sub->addChild(createBoolPtrMenuItem("Limit to the 303 range (C1–C4)", "",
			                                    &module->limitRange));
			sub->addChild(createBoolPtrMenuItem("Auto-legato (slide without SLIDE input)", "",
			                                    &module->autoLegato));
			sub->addChild(createIndexPtrSubmenuItem("OTA saturation (BA662A, before the VCA)",
			                                        {"None", "Soft", "Hard"},
			                                        &module->otaIdx));
			sub->addChild(createIndexPtrSubmenuItem("Output saturation (mixer + LA4140)",
			                                        {"None", "Soft", "Hard"},
			                                        &module->satIdx));
			sub->addChild(createIndexPtrSubmenuItem("Analogue drift (R-2R DAC + posistor)",
			                                        {"None",
			                                         "Subtle (±1.5 cents / 1.3 % cutoff)",
			                                         "Marked (±5 cents / 4 % cutoff)",
			                                         "Heavy (±12 cents / 9 % cutoff)"},
			                                        &module->driftIdx));
			sub->addChild(createIndexPtrSubmenuItem("Unit (DAC error pattern)",
			                                        {"1", "2", "3", "4"},
			                                        &module->unitSeed));
		}));
	}
};

Model* modelAtek303 = createModel<Atek303, Atek303Widget>("ATEK303");
