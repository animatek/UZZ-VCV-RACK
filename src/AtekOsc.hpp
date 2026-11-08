#pragma once
#include "rosic_Open303.h"

#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// Oscilador ATEK — modelo estructural del VCO del TB-303 (esquema pág. 5).
//
// El VCO real es un núcleo de rampa: un condensador cargado por la corriente del
// convertidor exponencial (par apareado Q76 2SC1583 + posistor R100) y descargado
// por un FET de reset (Q8/Q28 2SK30). El cuadrado no es una onda independiente:
// sale de un comparador (Q24/Q27/D25) sobre esa misma rampa.
//
// Lo que diferencia esto de una onda ideal, y lo que modelamos aquí:
//
//   1. El reset del condensador no es instantáneo (unos µs) → la esquina de la
//      rampa está redondeada. Se modela con un paso bajo de un polo.
//   2. Los flancos del comparador tienen slew finito → mismo tratamiento, con
//      una constante distinta.
//   3. El acoplo AC de la salida (condensadores en serie) hace que cada semiciclo
//      del cuadrado se incline hacia la línea de base. Ese *droop* es la firma
//      del cuadrado del 303 y es justo lo que no tiene una tabla de onda.
//
// Método: flancos ideales anti-aliased con polyBLEP (corriendo en el 4× de
// sobremuestreo que Open303 ya usa) y encima el filtrado lineal del circuito.
//
// Calibración: el ancho de pulso (44 %) y el droop (60 Hz) se eligieron a oído el
// 2026-08-17 y son los valores por defecto; siguen siendo ajustables desde el menú
// contextual del módulo. Las constantes de reset y slew siguen siendo estimaciones
// sin medir — se calibran con tools/render_test.cpp contra hardware real.
// ---------------------------------------------------------------------------

struct AtekOsc : rosic::Open303::ExternalOscillator {

	// --- constantes a calibrar -------------------------------------------------
	static constexpr double SQUARE_SLEW_US = 5.0;    // slew de los flancos del comparador

	// La sierra y el cuadrado salen de ramas distintas del circuito: la rampa viene
	// directa del buffer del condensador, y el cuadrado del comparador Q24/Q27/D25.
	// Por eso tienen sus propias constantes en vez de compartirlas.
	//
	// El acoplo hacia el filtro es C17 1 µF con R62 220 kΩ ≈ 0,7 Hz, o sea un simple
	// bloqueador de continua sin droop audible. La inclinación marcada que se ve en las
	// fotos de osciloscopio del cuadrado no viene de ahí, sino de su propia rama, así
	// que **la sierra debe llevar mucho menos droop que el cuadrado**.
	double sawResetUs = 3.0;      // redondeo de la esquina de reset
	double sawDroopHz = 0.7;      // acoplo hacia el filtro (C17 · R62)
	// Estos dos son ajustables desde el menú contextual: el ancho de pulso lo fija
	// el trimmer TM5 del original ("WIDTH") y no es exactamente el 50 %, y el droop
	// depende del acoplo de la etapa siguiente. Se calibran a oído.
	double pulseWidth = 0.44;
	double squareDroopHz = 60.0;
	// Igualación de nivel con las tablas de Open303, para que el A/B del menú
	// compare timbre y no volumen (medido con tools/render_test.cpp).
	static constexpr double OUTPUT_GAIN    = 0.484;
	// ---------------------------------------------------------------------------

	double phase = 0.0;
	double inc = 0.0;
	double sampleRate = 0.0;
	double blend = 1.0;      // 0 = sierra, 1 = cuadrado

	// Coeficientes de las cuatro celdas de un polo
	double aLpSaw = 1.0, aLpSq = 1.0, aHpSaw = 0.0, aHpSq = 0.0;

	// --- capa extra ------------------------------------------------------------
	// Fuentes que el TB-3 suma al 303 antes del filtro (ver «Sonidos de fábrica» en
	// Roland TB3 Mod/NOTES.md): un seno afinado aparte, la otra onda del TB con su
	// propia desafinación, ruido, ring-mod del TB por el seno y FM del seno sobre el
	// TB. Es una idea de arquitectura, no código suyo. Con todo a 0 la salida es
	// exactamente la de antes.
	double sinLevel = 0.0;      // 0..1, nivel del seno
	double sinRatio = 0.5;      // relación de frecuencia con el TB (−12 st)
	double otherLevel = 0.0;    // 0..1, la onda que no elige el conmutador
	double otherRatio = 1.0;    // afinación de la otra onda: salto + desafinado
	double noiseLevel = 0.0;    // 0..1
	bool noisePink = false;
	double ringLevel = 0.0;     // 0..1, TB × seno
	double xmodDepth = 0.0;     // 0..1, FM lineal del seno sobre el TB

	// Iguala el rms del rosa con el del blanco (0,43 frente a 0,58, medido). Tras el
	// filtro del 303 el rosa se oye mucho más: el blanco es casi todo agudos.
	static constexpr double PINK_GAIN = 1.35;

	double phase2 = 0.0;        // fase propia de la otra onda cuando va afinada aparte
	double phaseSin = 0.0;
	uint32_t noiseState = 0x9e3779b9u;
	double pink0 = 0.0, pink1 = 0.0, pink2 = 0.0;

	void setLayer(double newSinLevel, double sinSemitones, double newOtherLevel,
	              double otherSemitones, double detuneCents, double newNoiseLevel, bool pink,
	              double newRingLevel, double newXmodDepth) {
		sinLevel = newSinLevel;
		sinRatio = std::pow(2.0, sinSemitones / 12.0);
		otherLevel = newOtherLevel;
		otherRatio = std::pow(2.0, (100.0 * otherSemitones + detuneCents) / 1200.0);
		noiseLevel = newNoiseLevel;
		noisePink = pink;
		ringLevel = newRingLevel;
		xmodDepth = newXmodDepth;
	}

	double whiteNoise() {
		noiseState ^= noiseState << 13;
		noiseState ^= noiseState >> 17;
		noiseState ^= noiseState << 5;
		return noiseState * (2.0 / 4294967295.0) - 1.0;
	}
	// Ruido rosa de Paul Kellet (versión económica): −3 dB/octava aproximado.
	double pinkNoise() {
		const double w = whiteNoise();
		pink0 = 0.99765 * pink0 + w * 0.0990460;
		pink1 = 0.96300 * pink1 + w * 0.2965164;
		pink2 = 0.57000 * pink2 + w * 1.0526913;
		return 0.25 * (pink0 + pink1 + pink2 + w * 0.1848);
	}
	// ---------------------------------------------------------------------------

	// Estados
	double lpSaw = 0.0, lpSq = 0.0;
	double hpSawY = 0.0, hpSawX = 0.0;
	double hpSqY = 0.0, hpSqX = 0.0;

	static double onePoleLowpassCoeff(double fc, double sr) {
		return 1.0 - std::exp(-2.0 * M_PI * fc / sr);
	}
	static double onePoleHighpassCoeff(double fc, double sr) {
		return std::exp(-2.0 * M_PI * fc / sr);
	}

	void updateCoeffs() {
		if (sampleRate <= 0.0)
			return;
		const double fcSaw = 1.0 / (2.0 * M_PI * sawResetUs * 1e-6);
		const double fcSq  = 1.0 / (2.0 * M_PI * SQUARE_SLEW_US * 1e-6);
		// Sin pasarse de Nyquist: por encima, la celda deja de filtrar
		aLpSaw = onePoleLowpassCoeff(std::min(fcSaw, 0.45 * sampleRate), sampleRate);
		aLpSq  = onePoleLowpassCoeff(std::min(fcSq,  0.45 * sampleRate), sampleRate);
		aHpSaw = onePoleHighpassCoeff(sawDroopHz, sampleRate);
		aHpSq  = onePoleHighpassCoeff(squareDroopHz, sampleRate);
	}

	void setWaveform(double newBlend) { blend = newBlend; }

	void setShape(double newPulseWidth, double newDroopHz) {
		pulseWidth = newPulseWidth;
		if (newDroopHz != squareDroopHz) {
			squareDroopHz = newDroopHz;
			updateCoeffs();
		}
	}

	void setSawShape(double newResetUs, double newDroopHz) {
		if (newResetUs != sawResetUs || newDroopHz != sawDroopHz) {
			sawResetUs = newResetUs;
			sawDroopHz = newDroopHz;
			updateCoeffs();
		}
	}

	// --- interfaz que consume Open303 -----------------------------------------
	void setFrequency(double hz, double sr) override {
		if (sr != sampleRate) {
			sampleRate = sr;
			updateCoeffs();
		}
		// Red de seguridad: un incremento de fase mayor que Nyquist convierte el
		// oscilador en ruido de banda ancha. Pase lo que pase aguas arriba, aquí
		// no entra una frecuencia imposible.
		if (!std::isfinite(hz))
			hz = 20.0;
		hz = std::max(0.01, std::min(hz, 0.45 * sampleRate));
		inc = (sampleRate > 0.0) ? hz / sampleRate : 0.0;
	}

	void resetPhase() override {
		phase = phase2 = phaseSin = 0.0;
		lpSaw = lpSq = 0.0;
		pink0 = pink1 = pink2 = 0.0;
		hpSawY = hpSawX = hpSqY = hpSqX = 0.0;
	}

	// Corrección polyBLEP de un flanco: cancela el salto ideal por la banda que
	// se saldría de Nyquist.
	static inline double polyBlep(double t, double dt) {
		if (dt <= 0.0)
			return 0.0;
		if (t < dt) {
			t /= dt;
			return t + t - t * t - 1.0;
		}
		if (t > 1.0 - dt) {
			t = (t - 1.0) / dt;
			return t * t + t + t + 1.0;
		}
		return 0.0;
	}

	double getSample() override {
		// Seno de la capa extra: va primero porque modula la fase del TB.
		const bool layer = sinLevel > 0.0 || ringLevel > 0.0 || xmodDepth > 0.0;
		double sine = 0.0;
		if (layer) {
			phaseSin += inc * sinRatio;
			phaseSin -= std::floor(phaseSin);
			sine = std::sin(2.0 * M_PI * phaseSin);
		}
		// FM lineal: el incremento nunca baja de 0 (la rampa del 303 no va hacia atrás).
		double dt = inc;
		if (xmodDepth > 0.0)
			// Con la cross-mod extrema el incremento llega a 5 veces el normal: se
			// limita como en setFrequency para no pasar de Nyquist en notas agudas.
			dt = std::min(0.45, std::max(0.0, inc * (1.0 + xmodDepth * 4.0 * sine)));

		phase += dt;
		if (phase >= 1.0)
			phase -= std::floor(phase);

		// La otra onda (la que no elige el conmutador) comparte la rampa salvo que
		// esté transportada o desafinada; entonces lleva su propia fase. La onda
		// principal siempre va por la rampa de siempre.
		const bool shifted = otherRatio != 1.0;
		const bool squareMain = blend >= 0.5;
		double dt2 = dt, phase2v = phase;
		if (shifted) {
			dt2 = std::min(dt * otherRatio, 0.45);
			phase2 += dt2;
			if (phase2 >= 1.0)
				phase2 -= std::floor(phase2);
			phase2v = phase2;
		}
		const double pSaw = (shifted && squareMain) ? phase2v : phase;
		const double dtSaw = (shifted && squareMain) ? dt2 : dt;
		const double pSq = (shifted && !squareMain) ? phase2v : phase;
		const double dtSq = (shifted && !squareMain) ? dt2 : dt;

		// Rampa del condensador
		double saw = 2.0 * pSaw - 1.0;
		saw -= polyBlep(pSaw, dtSaw);

		// Comparador sobre la rampa
		double sq = (pSq < pulseWidth) ? 1.0 : -1.0;
		sq += polyBlep(pSq, dtSq);
		double tFall = pSq + (1.0 - pulseWidth);
		if (tFall >= 1.0)
			tFall -= 1.0;
		sq -= polyBlep(tFall, dtSq);

		// 1 y 2: esquina de reset redondeada / flancos con slew finito
		lpSaw += aLpSaw * (saw - lpSaw);
		lpSq  += aLpSq  * (sq  - lpSq);

		// 3: acoplo AC (el droop)
		hpSawY = aHpSaw * (hpSawY + lpSaw - hpSawX);
		hpSawX = lpSaw;
		hpSqY  = aHpSq  * (hpSqY + lpSq - hpSqX);
		hpSqX  = lpSq;

		double tb = hpSawY + blend * (hpSqY - hpSawY);
		if (otherLevel > 0.0) {
			// Las dos ondas comparten fase: sumadas, el pico casi se dobla. Se
			// normaliza como si fueran incorreladas para que no salte el volumen.
			tb += otherLevel * (hpSqY + blend * (hpSawY - hpSqY));
			tb /= std::sqrt(1.0 + otherLevel * otherLevel);
		}

		// El ring es un fundido: al 100 % solo queda el producto, sin el 303 seco.
		// Sumado sin más, el producto queda enterrado bajo el 303 y no se oye.
		double out = tb;
		if (layer)
			out = tb + ringLevel * (tb * sine - tb) + sinLevel * sine;
		if (noiseLevel > 0.0)
			out += noiseLevel * (noisePink ? PINK_GAIN * pinkNoise() : whiteNoise());
		return OUTPUT_GAIN * out;
	}
};
