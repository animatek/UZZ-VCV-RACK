#pragma once
#include "rosic_Open303.h"

#include <cmath>

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
		phase = 0.0;
		lpSaw = lpSq = 0.0;
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
		phase += inc;
		if (phase >= 1.0)
			phase -= 1.0;

		// Rampa del condensador
		double saw = 2.0 * phase - 1.0;
		saw -= polyBlep(phase, inc);

		// Comparador sobre la rampa
		double sq = (phase < pulseWidth) ? 1.0 : -1.0;
		sq += polyBlep(phase, inc);
		double tFall = phase + (1.0 - pulseWidth);
		if (tFall >= 1.0)
			tFall -= 1.0;
		sq -= polyBlep(tFall, inc);

		// 1 y 2: esquina de reset redondeada / flancos con slew finito
		lpSaw += aLpSaw * (saw - lpSaw);
		lpSq  += aLpSq  * (sq  - lpSq);

		// 3: acoplo AC (el droop)
		hpSawY = aHpSaw * (hpSawY + lpSaw - hpSawX);
		hpSawX = lpSaw;
		hpSqY  = aHpSq  * (hpSqY + lpSq - hpSqX);
		hpSqX  = lpSq;

		return OUTPUT_GAIN * (hpSawY + blend * (hpSqY - hpSawY));
	}
};
