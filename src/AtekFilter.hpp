#pragma once
#include "rosic_Open303.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Filtro ATEK — ladder de diodos del TB-303 (esquema pág. 5, bloque VCF).
//
// El circuito: par diferencial Q12 (2SC1583) abajo, cuatro células con Q13/Q15,
// Q14/Q17, Q16/Q23 y Q22 (2SC2291) arriba, con los condensadores **asimétricos**:
//
//     C18 = 0,018 µF   (primera célula)
//     C19 = C24 = C26 = 0,033 µF
//
// La primera célula tiene por tanto 33/18 = **1,833 veces** la frecuencia de corte
// de las otras tres. Por eso la pendiente del 303 no es un 4 polos limpio.
//
// Qué añade esto sobre el filtro de Open303, que ya es un ladder acoplado:
//
//   1. **No linealidad en las cuatro células.** Open303 aplica una sola, y en el
//      camino de realimentación. El ladder real tiene diodos en cada escalón, y son
//      ellos los que hacen que el filtro responda al *nivel* de la señal: es la
//      diferencia entre un filtro que solo filtra y uno que tiene carácter.
//   2. **Drive de entrada.** En modo TB_303 Open303 ni siquiera usa su `driveFactor`.
//   3. **Relación de condensadores real** (1,833) en vez de 2 exacto.
//
// El paso alto en la realimentación (150 Hz) se mantiene: es lo que hace que la
// resonancia se coma los graves, y eso Robin ya lo tenía bien.
//
// Estabilidad: corre dentro del sobremuestreo 4× de Open303, la no linealidad está
// acotada, y los estados se limitan. Aun así, k tiene un techo por debajo del punto
// de auto-oscilación — el 303 real tampoco llega a oscilar del todo.
// ---------------------------------------------------------------------------

struct AtekFilter : rosic::Open303::ExternalFilter {

	// --- constantes del circuito -----------------------------------------------
	static constexpr double CAP_RATIO = 33.0 / 18.0;   // C19 / C18 = 1,833
	// La afinación (coeficiente por célula, escala de la realimentación y ganancia de
	// compensación) se toma tal cual de `TeeBeeFilter::calculateCoefficientsApprox4()`
	// en modo TB_303: son polinomios que Robin ajustó contra medidas del hardware y no
	// tiene ningún sentido volver a derivarlos. Lo que añadimos encima es la no
	// linealidad por célula, la relación real de condensadores y el drive.
	static constexpr double VT = 1.2;                  // "voltaje térmico" del diodo,
	                                                   // marca dónde empieza a saturar
	static constexpr double FB_HIGHPASS_HZ = 150.0;    // pérdida de graves con resonancia

	// Techo del corte instantáneo. Con env mod y acento al máximo, el barrido pide
	// cortes de decenas de kHz, y ahí la escala de realimentación del polinomio crece
	// de 17 (a 1 kHz) a 96 (a 86 kHz): con los diodos por célula el lazo entra en
	// oscilación caótica. Medido: a resonancia 1,0 está limpio hasta 16 kHz y oscila
	// a 20 kHz. Un paso bajo a 14 kHz ya deja pasar toda la banda audible, así que
	// el techo no quita nada — y el VCF del 303 real no se acerca ni de lejos.
	static constexpr double MAX_CUTOFF_HZ = 14000.0;
	static constexpr double K_MAX = 40.0;              // segunda red, por si acaso
	// ---------------------------------------------------------------------------

	double sampleRate = 0.0;
	double cutoff = 1000.0;
	double resonance = 0.0;    // 0..1 ya mapeado
	double driveFactor = 1.0;

	double b0 = 0.0, k = 0.0, gComp = 1.0;
	double y1 = 0.0, y2 = 0.0, y3 = 0.0, y4 = 0.0;
	double hpState = 0.0, hpPrev = 0.0, hpCoeff = 0.0;

	// Saturación blanda del diodo. tanh acotado: por debajo de VT es casi lineal,
	// por encima comprime, y nunca puede devolver algo que dispare el lazo.
	static inline double diode(double x) {
		return VT * std::tanh(x / VT);
	}

	void updateCoeffs() {
		if (sampleRate <= 0.0)
			return;
		const double fc = std::max(10.0, std::min(std::min(cutoff, MAX_CUTOFF_HZ),
		                                          0.45 * sampleRate));
		const double fx = fc / (sampleRate * 1.4142135623730951);

		b0 = (0.00045522346 + 6.1922189 * fx)
		   / (1.0 + 12.358354 * fx + 4.4156345 * (fx * fx));

		double kScale = fx*(fx*(fx*(fx*(fx*(fx + 7198.6997) - 5837.7917) - 476.47308)
		              + 614.95611) + 213.87126) + 16.998792;

		gComp = kScale * (1.0 / 17.0);
		gComp = (gComp - 1.0) * resonance + 1.0;
		gComp = gComp * (1.0 + resonance);
		k = std::min(kScale * resonance, K_MAX);

		hpCoeff = std::exp(-2.0 * M_PI * FB_HIGHPASS_HZ / sampleRate);
	}

	// --- interfaz que consume Open303 ------------------------------------------
	void setSampleRate(double sr) override {
		sampleRate = sr;
		updateCoeffs();
	}

	void setCutoff(double hz) override {
		if (hz != cutoff) {
			cutoff = hz;
			updateCoeffs();
		}
	}

	void setResonance(double percent) override {
		// Misma curva que TeeBeeFilter, para que el parámetro signifique lo mismo en
		// los dos motores. Si se quiere respuesta lineal (el pot VR4 real es tipo B),
		// el módulo pre-deforma el valor con la inversa antes de llegar aquí.
		const double r = std::max(0.0, std::min(1.0, 0.01 * percent));
		resonance = (1.0 - std::exp(-3.0 * r)) / (1.0 - std::exp(-3.0));
		updateCoeffs();
	}

	void setDrive(double factor) { driveFactor = factor; }

	void reset() override {
		y1 = y2 = y3 = y4 = 0.0;
		hpState = hpPrev = 0.0;
	}

	double getSample(double in) override {
		// Realimentación: se toma de la última célula, pasa por el diodo y por el
		// paso alto que le quita los graves antes de restarse a la entrada.
		const double fb = k * diode(y4);
		hpState = hpCoeff * (hpState + fb - hpPrev);
		hpPrev = fb;

		const double x = driveFactor * in - hpState;

		// Las cuatro células. Cada una ve a sus vecinas — así se acopla el ladder de
		// diodos, a diferencia del de transistores — y cada una satura por su cuenta.
		y1 += CAP_RATIO * b0 * diode(x  - y1 + y2);
		y2 +=             b0 * diode(y1 - 2.0 * y2 + y3);
		y3 +=             b0 * diode(y2 - 2.0 * y3 + y4);
		y4 +=             b0 * diode(y3 - 2.0 * y4);

		// Red de seguridad: si algo se desmadra, se corta aquí y no se propaga.
		if (!std::isfinite(y1) || !std::isfinite(y2) || !std::isfinite(y3) || !std::isfinite(y4))
			reset();

		return 2.0 * gComp * y4;
	}
};
