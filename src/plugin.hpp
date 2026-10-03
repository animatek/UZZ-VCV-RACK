#pragma once
#include <rack.hpp>


using namespace rack;

// Shared data structures for OXI-CV and its expander
struct VoiceState {
    int8_t  note = -1;
    uint8_t vel  = 0;
    bool    gate = false;
};

struct OxiCvExpMsg {
    VoiceState channels[16];
    float pitchBend = 0.f;
};

// Converts a MIDI note (int8_t, -1 = inactive) to V/Oct relative to C4 (60)
static inline float noteToVoct(int8_t note) {
    return (note >= 0) ? (note - 60) / 12.f : 0.f;
}

// Shared data for UZZ and its CV expander (UZZ-X, attaches to UZZ's left).
// CVs are bipolar offsets added to the corresponding UZZ knob value.
enum UzzXCvIds {
    UZZX_CV_STEPS,   // 1 V = 1 step
    UZZX_CV_START,   // 1 V = 1 step
    UZZX_CV_DIR,     // 1 V = 1 direction mode
    UZZX_CV_ADDR,    // absolute step address: 0-10 V spans the active window
    UZZX_CV_RATIO,   // 1 V = 1 ratio-table index
    UZZX_CV_SWING,   // ±5 V = full swing range
    UZZX_CV_PROB,    // ±10 V = ±100% global probability
    UZZX_CV_ACCUM,   // 1 V = 1 semitone of accumulator amount
    UZZX_NUM_CVS
};

struct UzzExpMsg {
    float cv[UZZX_NUM_CVS] = {};
    bool connected[UZZX_NUM_CVS] = {};
    bool revGate = false;          // momentary direction reverse while high
    uint32_t accumRstCount = 0;    // event counters: UZZ acts on increments
    uint32_t rotFwdCount = 0;      // rotate whole sequence +1 step (wraps)
    uint32_t rotBackCount = 0;     // rotate whole sequence -1 step (wraps)
};

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Mensaje del expander ATEK303: el SEQ lo escribe a su derecha y la voz lo lee a su
// izquierda. Solo se usa para los jacks que la voz tenga sin cablear, así que un cable
// siempre manda sobre el expander.
struct Atek303SeqMessage {
    float voct = 0.f;
    bool gate = false;
    bool accent = false;
    bool slide = false;
};

// Bus de la cadena de CAP. Cada CAP suma su estéreo a lo que le llega por la
// izquierda y lo pasa a la derecha; un BUS lo recoge, lo saca por MIX y tiene el
// envío/retorno. Viaja empujado: cada módulo escribe en el producerMessage de su
// vecino derecho, así que cada salto añade una muestra de latencia.
struct CapBusMessage {
    float left = 0.f;
    float right = 0.f;
};

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* modelUZZ;    // UZZ step sequencer
extern Model* modelUzzX;   // UZZ-X CV expander for UZZ
extern Model* modelOxiCv;  // OXI-CV (6HP MIDI-to-CV, Oxi One)
extern Model* modelOxiCvExp; // OXI-CV EXPANSOR
extern Model* modelApc40Ctrl; // APC40 controller CV bridge
extern Model* modelSideChain; // SIDECHAIN trigger-fired ducking envelope
extern Model* modelCapBus; // BUS: mix, insert send/return and master for a CAP chain
extern Model* modelAdsrTek; // ADSRtek envelope modelled on a classic 90s modular
extern Model* modelFilterTek; // FILTERtek multimode filter modelled on a classic 90s modular
extern Model* modelUnitDistanceSeq; // UNIT-D unit-distance graph sequencer
extern Model* modelBlank3; // 3HP blank panel
extern Model* modelBlankAcid; // 3HP blank panel, acid smiley marks
extern Model* modelAtek303;    // ATEK303 TB-303 voice
extern Model* modelAtek303Seq; // ATEK303 SEQ acid pattern generator




// Lo que llega por la izquierda de la cadena: cero si ahí no hay una CAP o un BUS.
inline CapBusMessage capBusReceive(Module& self) {
    Module* left = self.leftExpander.module;
    if (!left || (left->model != modelSideChain && left->model != modelCapBus))
        return CapBusMessage();
    return *static_cast<const CapBusMessage*>(self.leftExpander.consumerMessage);
}

// Pasa el bus al vecino derecho, si es parte de la cadena.
inline void capBusSend(Module& self, const CapBusMessage& msg) {
    Module* right = self.rightExpander.module;
    if (!right || (right->model != modelSideChain && right->model != modelCapBus))
        return;
    *static_cast<CapBusMessage*>(right->leftExpander.producerMessage) = msg;
    right->leftExpander.requestMessageFlip();
}

inline bool capChainModule(Module* m) {
    return m && (m->model == modelSideChain || m->model == modelCapBus);
}

// Brillo de un LED de la cadena: apagado sin vecino enlazado; tenue con el enlace
// hecho y en silencio; y late con el audio que pasa (pico con unos 150 ms de caída,
// a pleno brillo con 5 V). Es lo que permite ver de un vistazo que dos módulos
// pegados están de verdad unidos y que el audio pasa del uno al otro.
struct CapChainLed {
    float env = 0.f;

    float update(bool linked, const CapBusMessage& msg, float sampleTime) {
        float level = std::max(std::fabs(msg.left), std::fabs(msg.right));
        env = std::max(level, env * std::exp(-sampleTime / 0.15f));
        if (!linked)
            return 0.f;
        return 0.2f + 0.8f * clamp(env / 5.f, 0.f, 1.f);
    }
};
