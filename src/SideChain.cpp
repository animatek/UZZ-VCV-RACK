#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

using AnimatekUI::ConnectorLine;
using AnimatekUI::TekInputPort;
using AnimatekUI::TekOutputPort;
using AnimatekUI::TextLabel;

// ============================================================================
// SideChain - trigger-fired ducking VCA
// ============================================================================
//
// Feed it the same trigger that fires the kick and run the audio through it:
// the signal drops and recovers, no compressor involved. The VCA is built in,
// so nothing else is needed, but ENV still carries the envelope as CV for
// whatever else you want to duck in step.
//
// What separates it from any inverted envelope patched by hand is that every
// hit is slightly different from the last, so the ducking breathes the way an
// analogue compressor does instead of stamping an identical curve forever.
//
// The variation is a random walk, not white noise: each hit is correlated
// with the previous one. Pure randomness sounds random; correlated variation
// sounds human, because a real compressor also drags state between hits.
// Every polyphony channel keeps its own generator, so ducking several tracks
// makes each of them breathe differently - which is exactly what cannot be
// built by patching envelopes by hand.
// ============================================================================

static constexpr float ATTACK_TIME = 0.002f;  // fall time, fixed
static constexpr float HOLD_TIME = 0.012f;    // time spent at the floor

// Jitter ranges at JITTER = 100%, scaled linearly by the knob. Recovery gets
// the widest range because it is the most natural sounding and the least
// distracting; depth gets the narrowest because it moves the perceived level
// of the mix.
//
// These are the extremes, not the typical swing: the walk settles at a
// standard deviation of about 0.54, so what you actually hear most of the
// time is a bit over half of each figure. A nominal ±20% on recovery came out
// as ±11% in practice, which was too polite to notice.
static constexpr float JITTER_RECOVERY = 0.50f;
static constexpr float JITTER_DEPTH = 0.25f;
static constexpr float JITTER_CURVE = 0.30f;

// Random walk: x = A*x + B*u, with u uniform in [-1, 1].
//
// B is sqrt(1 - A^2) rather than the more obvious (1 - A). With (1 - A) the
// walk still has the right correlation but its steady-state deviation
// collapses to about 0.23, so the nominal ranges above would only ever be
// heard at a quarter of their size. This keeps unit variance, so ±20% of
// recovery really means ±20%.
static constexpr float WALK_A = 0.7f;
static constexpr float WALK_B = 0.714143f;  // sqrt(1 - 0.7^2)

enum CurveShape {
    CURVE_EXPONENTIAL,
    CURVE_LINEAR,
    CURVE_LOGARITHMIC,
    NUM_CURVE_SHAPES
};

// Recovery is level = floor + (1 - floor) * phase^k. k > 1 stays down and
// snaps back at the end (the classic pump), k < 1 lifts immediately and
// settles slowly (closer to how an analogue release actually behaves).
static const float CURVE_EXPONENTS[NUM_CURVE_SHAPES] = {2.5f, 1.0f, 0.4f};

static const char* CURVE_NAMES[NUM_CURVE_SHAPES] = {
    "Exponential", "Linear", "Logarithmic"};

// What the control signal (envelope x VCA CV) acts on. VCA is the module as it
// always was; the other two run the audio through a lowpass whose cutoff
// follows the control, LPF leaving the level alone and LPG closing both at
// once, the way a Buchla-style low-pass gate does.
enum GateMode {
    MODE_VCA,
    MODE_LPF,
    MODE_LPG,
    NUM_GATE_MODES
};

static const char* GATE_MODE_NAMES[NUM_GATE_MODES] = {
    "VCA", "Lowpass filter", "Low-pass gate"};

// Cutoff spans 20 Hz to 20 kHz exponentially, so equal steps of control are
// equal musical intervals. At full control the filter sits above hearing and
// LPF mode at rest is as transparent as VCA mode.
static constexpr float CUTOFF_MIN = 20.f;
static constexpr float CUTOFF_OCTAVES = 10.f;
// Damping of the SVF: 1/Q. A touch under the Butterworth 1.414 gives the
// faint bump at the cutoff that makes a closing gate audible as a sweep
// instead of a plain fade, without turning it into a resonant filter.
static constexpr float FILTER_DAMPING = 1.1f;

// A vactrol, the light-dependent resistor in a real LPG, opens fast and
// closes slowly, and closes slower still the darker it gets. That lag is the
// "plonk": the tail of a note keeps losing highs after the level has fallen.
//
// LPG mode only. Closing is the slow direction, so a vactrol turns the 2 ms
// fall of a duck into some 50 ms; LPF mode follows the envelope unsmoothed so
// a filtered pump keeps its punch.
static constexpr float VACTROL_RISE = 0.002f;
static constexpr float VACTROL_FALL = 0.030f;
static constexpr float VACTROL_FALL_DARK = 3.f;  // extra fall time, x, at zero

struct Vactrol {
    float y = 1.f;
    // Unprimed, the first sample jumps straight to the control: a vactrol
    // starting fully lit would leak a tail of sound into a closed ping gate
    // every time the mode is selected or the patch loads.
    bool primed = false;

    float process(float x, float dt) {
        if (!primed) {
            y = x;
            primed = true;
        }
        float tau = (x > y) ? VACTROL_RISE
                            : VACTROL_FALL * (1.f + VACTROL_FALL_DARK * (1.f - y));
        y += (x - y) * (1.f - std::exp(-dt / tau));
        return y;
    }
};

/** Two-pole lowpass, the trapezoidal state-variable form: stable under fast
cutoff modulation, which is all this filter ever gets. */
struct LowpassSvf {
    float ic1 = 0.f;
    float ic2 = 0.f;

    float process(float x, float g, float k) {
        float a1 = 1.f / (1.f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        return v2;
    }
};


struct SideChain : Module {
    // New entries go at the end of each enum: the indices are what patches
    // store, so appending keeps older patches loading onto the right jacks.
    enum ParamId { RECOVERY_PARAM, DEPTH_PARAM, JITTER_PARAM, LEVEL_PARAM,
                   TRIG_PARAM, PAN_PARAM, PARAMS_LEN };
    enum InputId { TRIG_INPUT, DEPTH_CV_INPUT, IN_L_INPUT, IN_R_INPUT,
                   VCA_CV_INPUT, PAN_CV_INPUT, INPUTS_LEN };
    enum OutputId { ENV_OUTPUT, OUT_L_OUTPUT, OUT_R_OUTPUT, EOC_OUTPUT, OUTPUTS_LEN };
    enum LightId { CHAIN_IN_LIGHT, CHAIN_OUT_LIGHT, LIGHTS_LEN };

    enum Stage { STAGE_IDLE, STAGE_ATTACK, STAGE_HOLD, STAGE_RECOVER };

    struct Voice {
        dsp::SchmittTrigger trigger;
        int stage = STAGE_IDLE;
        float level = 1.f;
        float phase = 0.f;
        float attackStart = 1.f;
        float floorLevel = 1.f;
        // Drawn once per hit, never modulated continuously: a value that
        // drifts between hits sounds like an LFO, not like a compressor.
        float recTime = 0.25f;
        float exponent = 2.5f;
        float walkRecovery = 0.f;
        float walkDepth = 0.f;
        float walkCurve = 0.f;
        dsp::PulseGenerator eocPulse;
        random::Xoroshiro128Plus rng;
    };

    Voice voices[16];
    int curveShape = CURVE_EXPONENTIAL;
    bool freezeJitter = false;
    uint64_t baseSeed = 0x5C1DECA1ULL;
    // Off by default: a stereo pair must duck identically. Giving L and R
    // their own jittered envelopes decorrelates them and the image wobbles
    // sideways on every hit. Worth turning on only to duck several unrelated
    // tracks through one polyphonic cable.
    bool perChannelEnvelopes = false;
    // Live values of channel 0 for the meter to draw. Written from the audio
    // thread and read from the UI thread; a torn float here would just mean
    // one wrong frame of a meter, so no synchronisation.
    //
    // Two of them because they drive different things: the bar's height is
    // the gain including the ceiling, but its brightness follows the envelope
    // alone. Pulling the slider down should shorten the bar, not dim it.
    float meterGain = 1.f;
    float meterEnv = 1.f;
    // One bar per thing worth showing. Deliberately the gain, not the audio
    // level: metering the signal turns this into an output VU and the ducking
    // stops being legible, which is the whole point of the module.
    //
    // The count is dynamic. With the shared envelope there is only one gain,
    // so it draws one bar, or two when a stereo pair is patched. Under
    // per-channel envelopes each channel really does have its own gain and
    // gets its own bar.
    float meterBar[16] = {};
    int meterBars = 1;

    bool levelAffectsEnv = false;
    // The last CAP of a row with no BUS at its end sums the chain into its own
    // OUT. On for new modules; patches saved before it existed load with it
    // off, so CAPs that already sat side by side keep sounding as they did.
    bool chainMix = true;
    // Set from the context menu (UI thread), consumed by the audio thread.
    std::atomic<bool> manualTriggerPending{false};
    int gateMode = MODE_VCA;
    // Off is the ducker: rest is open and a hit closes. On, the envelope is
    // flipped so rest is closed and a hit opens to DEPTH, then RECOVERY closes
    // it again: a trigger pings the gate, which is what an LPG is played with.
    bool pingEnvelope = false;

    // Per audio channel: the vactrol smoothing the control, and one filter for
    // each side. Only used outside VCA mode.
    Vactrol vactrols[16];
    LowpassSvf filtersL[16];
    LowpassSvf filtersR[16];
    int lastGateMode = MODE_VCA;

    // Double buffer for the chain bus arriving from the left (see plugin.hpp).
    CapBusMessage busMessages[2];
    CapChainLed chainInLed, chainOutLed;
    // This CAP's channel number in its row, counted from the left; 0 when it
    // stands alone. Written by the audio thread, drawn by the panel.
    int displayChannel = 0;

    /** Hands the chain on to the right and lights the two chain LEDs: what came
    in from the left, and what goes out to the right. */
    void sendChain(const CapBusMessage& in, const CapBusMessage& out, float sampleTime) {
        lights[CHAIN_IN_LIGHT].setBrightness(
            chainInLed.update(in.linked, in, sampleTime));
        lights[CHAIN_OUT_LIGHT].setBrightness(
            chainOutLed.update(capChainModule(rightExpander.module), out, sampleTime));
        // Channels count from the left: the first CAP of a row is 1, and a BUS
        // that closes its row starts the count again for the next one.
        const int channel = in.linked ? in.channel + 1 : 1;
        displayChannel = (in.linked || capChainModule(rightExpander.module)) ? channel : 0;
        CapBusMessage msg = out;
        msg.linked = true;
        msg.channel = channel;
        capBusSend(*this, msg);
    }

    SideChain() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Exponential scale: the stored value is log2(seconds), so the knob
        // spends as much travel on 40-200 ms as on 200-1000 ms.
        //
        // 40 ms floor rather than 20: below that the duck is shorter than the
        // 12 ms hold plus the 2 ms fall, so the knob stops doing anything
        // useful. 1 s ceiling rather than 2: past a second the recovery
        // outlasts a bar at most tempos and the pumping stops reading as such.
        configParam(RECOVERY_PARAM, std::log2(0.040f), std::log2(1.0f),
                    std::log2(0.250f), "Recovery", " ms", 2.f, 1000.f);
        configParam(DEPTH_PARAM, 0.f, 1.f, 0.80f, "Depth", "%", 0.f, 100.f);
        configParam(JITTER_PARAM, 0.f, 1.f, 0.25f, "Jitter", "%", 0.f, 100.f);
        // Ceiling of the VCA. At 100% the module behaves exactly as before it
        // had a slider, so old patches sound unchanged.
        configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f);
        // The panel button gave its place to PAN; the parameter stays so older
        // patches load cleanly, and the trigger lives on in the context menu.
        configButton(TRIG_PARAM, "Manual trigger");
        // Where this CAP sits in the chain's stereo mix. Randomising the
        // module must not throw a channel sideways.
        configParam(PAN_PARAM, -1.f, 1.f, 0.f, "Pan in the chain mix", "%", 0.f, 100.f);
        paramQuantities[PAN_PARAM]->randomizeEnabled = false;

        configInput(TRIG_INPUT, "Trigger");
        configInput(DEPTH_CV_INPUT, "Depth CV");
        configInput(VCA_CV_INPUT, "VCA CV");
        configInput(PAN_CV_INPUT, "Pan CV (±5 V sweeps it all, added to PAN)");
        configInput(IN_L_INPUT, "Audio left");
        configInput(IN_R_INPUT, "Audio right (normalled to left)");
        configOutput(ENV_OUTPUT, "Ducked envelope");
        configOutput(OUT_L_OUTPUT, "Ducked audio left");
        configOutput(OUT_R_OUTPUT, "Ducked audio right");
        configOutput(EOC_OUTPUT, "End of cycle");
        configLight(CHAIN_IN_LIGHT, "Chain from the left (dim: linked, bright: audio passing)");
        configLight(CHAIN_OUT_LIGHT, "Chain to the right (dim: linked, bright: audio passing)");
        configBypass(IN_L_INPUT, OUT_L_OUTPUT);
        configBypass(IN_R_INPUT, OUT_R_OUTPUT);

        reseedVoices(baseSeed);

        leftExpander.producerMessage = &busMessages[0];
        leftExpander.consumerMessage = &busMessages[1];
    }

    /** Balance, not equal-power pan: the centre stays at unity, so a chain of
    CAPs left in the middle mixes at exactly the level each one outputs, and a
    stereo source keeps its own image until it is pushed to one side. */
    void panGains(int c, float& gainL, float& gainR) {
        float pan = params[PAN_PARAM].getValue() + inputs[PAN_CV_INPUT].getPolyVoltage(c) / 5.f;
        pan = clamp(pan, -1.f, 1.f);
        gainL = (pan > 0.f) ? 1.f - pan : 1.f;
        gainR = (pan < 0.f) ? 1.f + pan : 1.f;
    }

    /** A bypassed CAP still has to keep the chain alive, or everything to its
    left would freeze on the last sample it sent. Its audio joins the mix
    untouched, as it does at its own outputs. */
    void processBypass(const ProcessArgs& args) override {
        Module::processBypass(args);
        CapBusMessage bus = capBusReceive(*this);
        const CapBusMessage busIn = bus;
        bool rightPatched = inputs[IN_R_INPUT].isConnected();
        int n = std::max(inputs[IN_L_INPUT].getChannels(), inputs[IN_R_INPUT].getChannels());
        for (int c = 0; c < n; c++) {
            float panL, panR;
            panGains(c, panL, panR);
            float left = inputs[IN_L_INPUT].getPolyVoltage(c);
            float right = rightPatched ? inputs[IN_R_INPUT].getPolyVoltage(c) : left;
            bus.left += left * panL;
            bus.right += right * panR;
        }
        sendChain(busIn, bus, args.sampleTime);
    }

    /** Each channel gets a distinct stream. Sharing one would make every
    ducked track breathe in lockstep, which defeats the point. */
    void reseedVoices(uint64_t seed) {
        for (int c = 0; c < 16; c++) {
            // Odd multiplier of the golden ratio: cheap way to scatter
            // neighbouring channel indices into unrelated seeds.
            uint64_t k = (uint64_t)(c + 1);
            voices[c].rng.seed(seed + 0x9E3779B97F4A7C15ULL * k, (seed ^ k) + 1);
            voices[c].walkRecovery = 0.f;
            voices[c].walkDepth = 0.f;
            voices[c].walkCurve = 0.f;
        }
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        curveShape = CURVE_EXPONENTIAL;
        freezeJitter = false;
        perChannelEnvelopes = false;
        levelAffectsEnv = false;
        chainMix = true;
        gateMode = MODE_VCA;
        pingEnvelope = false;
        baseSeed = 0x5C1DECA1ULL;
        for (int c = 0; c < 16; c++) {
            voices[c].trigger.reset();
            voices[c].stage = STAGE_IDLE;
            voices[c].level = 1.f;
            voices[c].phase = 0.f;
        }
        reseedVoices(baseSeed);
    }

    /** The envelope as it leaves the module: the ducking level, or its mirror
    image when the envelope is set to ping. */
    float shapedEnv(const Voice& v) const {
        return pingEnvelope ? 1.f - v.level : v.level;
    }

    static float walkStep(random::Xoroshiro128Plus& rng, float x) {
        // Xoroshiro128Plus has weak low bits, so take the top 32.
        float u = (float)(uint32_t)(rng() >> 32) * 2.32830629e-10f;  // [0, 1)
        return clamp(WALK_A * x + WALK_B * (2.f * u - 1.f), -1.f, 1.f);
    }

    void process(const ProcessArgs& args) override {
        // A disconnected TRIG reports zero channels; forcing one keeps OUT
        // sitting at 10 V instead of leaving the VCA downstream silent.
        int channels = std::max(1, inputs[TRIG_INPUT].getChannels());

        float recovery = std::pow(2.f, params[RECOVERY_PARAM].getValue());
        float depthKnob = params[DEPTH_PARAM].getValue();
        float jitter = params[JITTER_PARAM].getValue();
        float baseExponent = CURVE_EXPONENTS[curveShape];
        float baseLevel = params[LEVEL_PARAM].getValue();
        float envScale = levelAffectsEnv ? baseLevel : 1.f;
        // A manual trigger (the context menu now, the panel button once) fires
        // every channel at once. It is summed into the trigger voltage for one
        // sample: the Schmitt trigger sees an edge and re-arms right after.
        float manual = params[TRIG_PARAM].getValue() * 10.f;
        if (manualTriggerPending.exchange(false))
            manual = 10.f;

        // El atenuador de la VCA. Multiplica la ganancia en vez de sustituir al
        // fader, así que el fader sigue siendo el tope y el CV recorta desde ahí:
        // eso es lo que hace de CAP una VCA controlada por tensión normal y
        // corriente. Unipolar y lineal, 0 V cierra y 10 V deja pasar el tope
        // entero. Sin cable no atenúa, que es lo que mantiene los patches
        // anteriores sonando igual. No toca ENV: la envolvente es lo que el módulo
        // genera, no lo que amplifica.
        const bool vcaPatched = inputs[VCA_CV_INPUT].isConnected();
        auto vcaCv = [&](int c) {
            if (!vcaPatched)
                return 1.f;
            return clamp(inputs[VCA_CV_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
        };

        for (int c = 0; c < channels; c++) {
            Voice& v = voices[c];

            if (v.trigger.process(inputs[TRIG_INPUT].getPolyVoltage(c) + manual, 0.1f, 1.0f)) {
                if (!freezeJitter) {
                    v.walkRecovery = walkStep(v.rng, v.walkRecovery);
                    v.walkDepth = walkStep(v.rng, v.walkDepth);
                    v.walkCurve = walkStep(v.rng, v.walkCurve);
                }

                float depth = depthKnob + inputs[DEPTH_CV_INPUT].getPolyVoltage(c) / 10.f;
                depth = clamp(depth, 0.f, 1.f);
                depth = clamp(depth * (1.f + JITTER_DEPTH * jitter * v.walkDepth), 0.f, 1.f);

                v.recTime = std::max(0.001f,
                                     recovery * (1.f + JITTER_RECOVERY * jitter * v.walkRecovery));
                v.exponent = std::max(0.05f,
                                      baseExponent * (1.f + JITTER_CURVE * jitter * v.walkCurve));

                // Retriggering mid-recovery must never step the level up, so
                // the floor can only ever be at or below where we already are.
                v.floorLevel = std::min(1.f - depth, v.level);
                v.attackStart = v.level;
                v.phase = 0.f;
                v.stage = STAGE_ATTACK;
            }

            switch (v.stage) {
                case STAGE_ATTACK:
                    v.phase += args.sampleTime / ATTACK_TIME;
                    if (v.phase >= 1.f) {
                        v.level = v.floorLevel;
                        v.phase = 0.f;
                        v.stage = STAGE_HOLD;
                    }
                    else {
                        v.level = v.attackStart + (v.floorLevel - v.attackStart) * v.phase;
                    }
                    break;

                case STAGE_HOLD:
                    v.level = v.floorLevel;
                    v.phase += args.sampleTime / HOLD_TIME;
                    if (v.phase >= 1.f) {
                        v.phase = 0.f;
                        v.stage = STAGE_RECOVER;
                    }
                    break;

                case STAGE_RECOVER:
                    v.phase += args.sampleTime / v.recTime;
                    if (v.phase >= 1.f) {
                        v.level = 1.f;
                        v.phase = 0.f;
                        v.stage = STAGE_IDLE;
                        // Only a recovery that ran to completion counts as a
                        // cycle. A retrigger cuts it short and fires nothing,
                        // otherwise EOC would degrade into a copy of TRIG at
                        // fast tempos.
                        v.eocPulse.trigger(1e-3f);
                    }
                    else {
                        v.level = v.floorLevel +
                                  (1.f - v.floorLevel) * std::pow(v.phase, v.exponent);
                    }
                    break;

                default:
                    v.level = 1.f;
                    break;
            }

            outputs[ENV_OUTPUT].setVoltage(10.f * shapedEnv(v) * envScale, c);
            // EOC is a trigger, never attenuated: a half-height trigger is
            // just a trigger some modules miss.
            outputs[EOC_OUTPUT].setVoltage(v.eocPulse.process(args.sampleTime) ? 10.f : 0.f, c);
        }

        outputs[ENV_OUTPUT].setChannels(channels);
        outputs[EOC_OUTPUT].setChannels(channels);

        meterEnv = shapedEnv(voices[0]);
        meterGain = meterEnv * baseLevel * vcaCv(0);

        // -- Meter --------------------------------------------------------
        bool leftPatched = inputs[IN_L_INPUT].isConnected();
        bool rightPatched = inputs[IN_R_INPUT].isConnected();
        int audioChannels = 0;
        if (leftPatched || rightPatched) {
            audioChannels = std::max(1, std::max(inputs[IN_L_INPUT].getChannels(),
                                                 inputs[IN_R_INPUT].getChannels()));
        }

        // Two separate paths are being ducked the moment both jacks are in,
        // whatever the polyphony says: two mono cables are one channel each,
        // so counting channels alone would collapse a stereo pair into a
        // single bar.
        int minBars = rightPatched ? 2 : 1;

        if (perChannelEnvelopes) {
            // Follow whatever is actually being ducked; without audio, fall
            // back to the envelopes so the meter still says something.
            int n = audioChannels > 0 ? audioChannels : channels;
            meterBars = clamp(std::max(n, minBars), 1, 16);
        }
        else {
            // A single shared gain, so more than a side per bar would just be
            // the same reading repeated.
            meterBars = std::max(minBars, audioChannels >= 2 ? 2 : 1);
        }
        for (int i = 0; i < meterBars; i++) {
            int e = perChannelEnvelopes ? std::min(i, channels - 1) : 0;
            meterBar[i] = shapedEnv(voices[e]) * baseLevel * vcaCv(i);
        }

        // -- VCA ------------------------------------------------------------
        //
        // Whatever arrives from a CAP to the left. A CAP in the middle of a row
        // keeps OUT as a direct out; the last one of a row that ends without a
        // BUS sums the chain there instead (see chainMix), and a BUS at the end
        // takes the sum itself.
        CapBusMessage bus = capBusReceive(*this);
        const CapBusMessage busIn = bus;
        const bool endOfRow = chainMix && busIn.linked
                              && !capChainModule(rightExpander.module);

        // Nothing patched in means nothing to attenuate: leave both audio
        // outputs at zero channels so downstream sees an unconnected jack
        // rather than silence.
        if (audioChannels == 0) {
            if (endOfRow) {
                outputs[OUT_L_OUTPUT].setChannels(1);
                outputs[OUT_R_OUTPUT].setChannels(1);
                outputs[OUT_L_OUTPUT].setVoltage(bus.left);
                outputs[OUT_R_OUTPUT].setVoltage(bus.right);
            }
            else {
                outputs[OUT_L_OUTPUT].setChannels(0);
                outputs[OUT_R_OUTPUT].setChannels(0);
            }
            sendChain(busIn, bus, args.sampleTime);
            return;
        }

        // Highest cutoff the filter is allowed: tan() blows up at Nyquist.
        const float maxCutoff = 0.45f * args.sampleRate;
        if (gateMode != lastGateMode) {
            for (int c = 0; c < 16; c++)
                vactrols[c].primed = false;
            lastGateMode = gateMode;
        }

        for (int c = 0; c < audioChannels; c++) {
            // One envelope for everything unless the user asked otherwise, so
            // a stereo pair ducks symmetrically.
            int e = perChannelEnvelopes ? std::min(c, channels - 1) : 0;
            // LEVEL stays outside the control on purpose: it is the channel
            // fader in every mode, and must not move the filter.
            float control = shapedEnv(voices[e]) * vcaCv(c);

            float left = inputs[IN_L_INPUT].getPolyVoltage(c);
            // Right is normalled to left: one cable feeds both outputs, which
            // turns the module into a mono-to-stereo ducker for free.
            float right = rightPatched ? inputs[IN_R_INPUT].getPolyVoltage(c) : left;
            float panL, panR;
            panGains(c, panL, panR);

            float gain = control * baseLevel;
            if (gateMode != MODE_VCA) {
                float lit = (gateMode == MODE_LPG)
                                ? vactrols[c].process(control, args.sampleTime)
                                : control;
                float cutoff = std::min(maxCutoff,
                                        CUTOFF_MIN * std::pow(2.f, CUTOFF_OCTAVES * lit));
                float g = std::tan(M_PI * cutoff * args.sampleTime);
                left = filtersL[c].process(left, g, FILTER_DAMPING);
                right = filtersR[c].process(right, g, FILTER_DAMPING);
                gain = (gateMode == MODE_LPG ? lit : 1.f) * baseLevel;
            }

            outputs[OUT_L_OUTPUT].setVoltage(left * gain, c);
            outputs[OUT_R_OUTPUT].setVoltage(right * gain, c);
            // Into the chain post-fader and summed across polyphony, the way
            // a mixer channel feeds its bus. OUT stays a direct out: patching
            // it takes nothing away from the mix.
            bus.left += left * gain * panL;
            bus.right += right * gain * panR;
        }

        if (endOfRow) {
            // The row's mix, this CAP included and panned: a stereo pair.
            outputs[OUT_L_OUTPUT].setChannels(1);
            outputs[OUT_R_OUTPUT].setChannels(1);
            outputs[OUT_L_OUTPUT].setVoltage(bus.left);
            outputs[OUT_R_OUTPUT].setVoltage(bus.right);
        }
        else {
            outputs[OUT_L_OUTPUT].setChannels(audioChannels);
            outputs[OUT_R_OUTPUT].setChannels(audioChannels);
        }
        sendChain(busIn, bus, args.sampleTime);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "curveShape", json_integer(curveShape));
        json_object_set_new(root, "freezeJitter", json_boolean(freezeJitter));
        json_object_set_new(root, "perChannelEnvelopes", json_boolean(perChannelEnvelopes));
        json_object_set_new(root, "levelAffectsEnv", json_boolean(levelAffectsEnv));
        json_object_set_new(root, "chainMix", json_boolean(chainMix));
        json_object_set_new(root, "gateMode", json_integer(gateMode));
        json_object_set_new(root, "pingEnvelope", json_boolean(pingEnvelope));
        // Stored as a string: a 64-bit seed does not survive JSON's double.
        json_object_set_new(root, "baseSeed",
                            json_string(string::f("%" PRIu64, baseSeed).c_str()));
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root)
            return;
        if (json_t* j = json_object_get(root, "curveShape"))
            curveShape = clamp((int) json_integer_value(j), 0, NUM_CURVE_SHAPES - 1);
        if (json_t* j = json_object_get(root, "freezeJitter"))
            freezeJitter = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "perChannelEnvelopes"))
            perChannelEnvelopes = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "levelAffectsEnv"))
            levelAffectsEnv = json_boolean_value(j);
        // Missing in patches from before the option: off, as those CAPs were.
        json_t* cm = json_object_get(root, "chainMix");
        chainMix = cm ? json_boolean_value(cm) : false;
        // Absent from patches saved before the modes existed: they stay VCA
        // with a ducking envelope, exactly as they were.
        if (json_t* j = json_object_get(root, "gateMode"))
            gateMode = clamp((int) json_integer_value(j), 0, NUM_GATE_MODES - 1);
        if (json_t* j = json_object_get(root, "pingEnvelope"))
            pingEnvelope = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "baseSeed")) {
            if (json_is_string(j))
                baseSeed = strtoull(json_string_value(j), NULL, 10);
            reseedVoices(baseSeed);
        }
    }
};


/** Vertical slider that sets the VCA ceiling and doubles as the gain meter,
the way Fundamental's VCA-1 does. The bar is the gain actually applied, so the
duck is visible on every hit; the handle is where the ceiling sits. */
struct LevelSlider : app::SliderKnob {
    // No se llama 'module': ParamWidget ya trae uno (engine::Module*) y
    // sombrearlo confunde tanto al lector como al análisis estático.
    SideChain* sideChain = NULL;

    LevelSlider() {
        // 41 mm y no los 54 de antes: el fader llega hasta el pie de la columna de
        // mandos y para ahí, porque debajo empieza la fila de D-CV y VCA. Sigue
        // siendo el recorrido más largo del panel y el medidor no pierde resolución
        // apreciable: 41 mm son 155 px, de sobra para dieciséis barras.
        box.size = mm2px(Vec(8.f, 41.f));
    }

    void draw(const DrawArgs& args) override {
        const float w = box.size.x;
        const float h = box.size.y;
        const float inset = 1.5f;

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 2.f);
        nvgFillColor(args.vg, nvgRGB(0x14, 0x16, 0x1c));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(0x33, 0x38, 0x4a));
        nvgStroke(args.vg);

        // The bar is not drawn here: it lives in the light layer so that
        // dimming the room leaves it lit, like Rack's own LEDs.
        const float track = h - inset * 2.f;

        float value = 1.f;
        if (engine::ParamQuantity* pq = getParamQuantity())
            value = pq->getScaledValue();
        float y = h - inset - track * value;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0.5f, y - 1.f, w - 1.f, 2.f);
        nvgFillColor(args.vg, nvgRGB(0xe8, 0xe8, 0xe8));
        nvgFill(args.vg);
    }

    /** Layer 1 is Rack's light layer: it is composited over the panel after
    the room dimming is applied, which is why LEDs stay bright in a dark room.
    Drawing the gain bar here rather than in draw() means the meter reads as
    something lit instead of something painted. */
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            const float w = box.size.x;
            const float h = box.size.y;
            const float inset = 1.5f;
            const float track = h - inset * 2.f;

            float gain = sideChain ? clamp(sideChain->meterGain, 0.f, 1.f) : 1.f;
            float env = sideChain ? clamp(sideChain->meterEnv, 0.f, 1.f) : 1.f;
            // Brightness follows the envelope, so a duck reads twice: the bars
            // get shorter and dim at the same time. The floor keeps them from
            // vanishing outright at full depth.
            float lit = 0.30f + 0.70f * env;

            int bars = sideChain ? clamp(sideChain->meterBars, 1, 16) : 1;
            // Blue is the VCA; amber means a filter is in the path. The mode
            // lives in the context menu, so the meter is what shows it.
            const bool filtering = sideChain && sideChain->gateMode != MODE_VCA;
            auto barColor = [&](uint8_t alpha) {
                return filtering ? nvgRGBA(0xFF, 0xA0, 0x28, alpha)
                                 : AnimatekUI::logoBlue(alpha);
            };
            // The gap has to shrink as bars multiply or there is nothing left
            // to draw: at sixteen, a fixed 0.8 px would eat more than half the
            // 20.6 px of usable width.
            const float gap = (bars <= 2) ? 0.8f : (bars <= 8 ? 0.5f : 0.25f);
            const float bw = (w - inset * 2.f - gap * (bars - 1)) / bars;
            const float rounding = (bw < 2.f) ? 0.f : 1.f;

            for (int i = 0; i < bars; i++) {
                float value = sideChain ? clamp(sideChain->meterBar[i], 0.f, 1.f) : gain;
                float barH = track * value;
                if (barH <= 0.5f)
                    continue;
                float x = inset + i * (bw + gap);
                float y = h - inset - barH;

                nvgBeginPath(args.vg);
                nvgRect(args.vg, x - 7.f, y - 7.f, bw + 14.f, barH + 14.f);
                nvgFillPaint(args.vg, nvgBoxGradient(args.vg, x, y, bw, barH,
                                                     2.f, 8.f,
                                                     barColor((uint8_t)(95.f * lit)),
                                                     nvgRGBA(0, 0, 0, 0)));
                nvgFill(args.vg);

                nvgBeginPath(args.vg);
                nvgRoundedRect(args.vg, x, y, bw, barH, rounding);
                nvgFillColor(args.vg, barColor((uint8_t)(255.f * lit)));
                nvgFill(args.vg);
            }
        }
        app::SliderKnob::drawLayer(args, layer);
    }
};


/** The channel number beside the module name, when the CAP is part of a row. */
struct CapChannelNumber : TransparentWidget {
    SideChain* cap = NULL;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1 && cap && cap->displayChannel > 0) {
            std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
            if (font && font->handle >= 0) {
                nvgFontSize(args.vg, 11.f);
                nvgFontFaceId(args.vg, font->handle);
                nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                nvgFillColor(args.vg, nvgRGB(0xe8, 0xe8, 0xe8));
                nvgText(args.vg, 0.f, 0.f, string::f("%d", cap->displayChannel).c_str(), NULL);
            }
        }
        TransparentWidget::drawLayer(args, layer);
    }
};


struct SideChainWidget : ModuleWidget {
    SideChainWidget(SideChain* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/SideChain.svg")));

        constexpr float W = 30.48f;  // 6 HP
        constexpr float X1 = 8.6f;
        constexpr float X2 = W - 8.6f;

        // Name goes bottom left next to the logo, as in the other modules,
        // which frees the whole header strip for the trigger button and lets
        // the slider run the full height of the control section.
        // The chain LEDs, in the top corners next to the neighbours they talk to:
        // left, what arrives from a CAP or BUS on the left; right, what goes on.
        addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(1.8f, 2.2f)), module,
                                                           SideChain::CHAIN_IN_LIGHT));
        addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(W - 1.8f, 2.2f)), module,
                                                           SideChain::CHAIN_OUT_LIGHT));

        // The channel number, up and to the right of the name.
        auto* number = createWidget<CapChannelNumber>(mm2px(Vec(14.6f, 118.6f)));
        number->box.size = mm2px(Vec(8.f, 5.f));
        number->cap = module;
        addChild(number);

        auto* moduleName = new TextLabel("CAP", mm2px(Vec(1.8f, 119.9f)),
                                         mm2px(Vec(14.f, 5.8f)));
        moduleName->fontSize = 16.f;
        moduleName->color = nvgRGB(0x2C, 0x7F, 0xFF);
        addChild(moduleName);

        auto addLabel = [&](const char* text, float cx, float y, float w) {
            auto label = createWidget<TextLabel>(mm2px(Vec(cx - w * 0.5f, y)));
            label->box.size = mm2px(Vec(w, 3.f));
            label->text = text;
            label->fontSize = 7.f;
            addChild(label);
        };

        // Knobs stacked down the left, meter-slider filling the right.
        //
        // RoundSmallBlackKnob is 9.48 mm across, so its top edge sits 4.74 mm
        // below the centre. The label box is 3 mm tall, so the centre has to
        // be at least 7.74 mm below the label to clear it; 8.5 leaves 1.8 mm
        // of air. The full-size RoundBlackKnob (12.87 mm) does not fit: three
        // of them with labels need 50.6 mm and there are only 44 between the
        // header and the jacks.
        // Same x as the jack column, so knobs, TRIG and the button all sit on
        // one axis.
        auto addKnob = [&](const char* text, float y, int paramId) {
            addLabel(text, X1, y, 16.f);
            addParam(createParamCentered<AnimatekUI::FlatSmallKnob>(
                mm2px(Vec(X1, y + 8.5f)), module, paramId));
        };

        // DEPTH va el último de los tres para quedar justo encima de su jack de CV,
        // que es el orden en que se leen: el mando y lo que lo modula, juntos.
        addKnob("RECOVERY", 4.0f, SideChain::RECOVERY_PARAM);
        addKnob("JITTER", 18.0f, SideChain::JITTER_PARAM);
        addKnob("DEPTH", 32.0f, SideChain::DEPTH_PARAM);

        auto* slider = createParam<LevelSlider>(mm2px(Vec(18.0f, 4.0f)), module,
                                                SideChain::LEVEL_PARAM);
        slider->sideChain = module;
        addParam(slider);

        auto addIn = [&](const char* text, float cx, float y, int inputId) {
            addLabel(text, cx, y, 14.f);
            addInput(createInputCentered<TekInputPort>(
                mm2px(Vec(cx, y + 7.5f)), module, inputId));
        };
        auto addOut = [&](const char* text, float cx, float y, int outputId) {
            addLabel(text, cx, y, 14.f);
            addOutput(createOutputCentered<TekOutputPort>(
                mm2px(Vec(cx, y + 7.5f)), module, outputId));
        };

        // Un jack sin etiqueta, colocado por su centro y no por la etiqueta que no
        // tiene. Lo nombra la línea que sube hasta el control que modula.
        auto addBareIn = [&](float cx, float cy, int inputId) {
            addInput(createInputCentered<TekInputPort>(
                mm2px(Vec(cx, cy)), module, inputId));
        };
        auto line = [&](float ax, float ay, float bx, float by) {
            addChild(new ConnectorLine(mm2px(ax), mm2px(ay), mm2px(bx), mm2px(by)));
        };

        // La fila de los dos CV, al pie de la columna de mandos y del fader. Ninguno
        // lleva etiqueta: cada uno sube por una línea hasta el control al que modula,
        // que dice más que un texto de cuatro letras —D-CV al mando DEPTH que tiene
        // justo encima, VCA al fader—. Es el mismo recurso que une el jack TRIG con
        // su botón, y la razón de que el fader se acorte hasta y = 45.
        //
        // Las dos líneas arrancan a la misma altura aunque lo que hay encima no acabe
        // a la misma: el borde del mando está en 45.24 y el pie del fader en 45.0. A
        // ojo pesa más que arranquen parejas que el milímetro de aire que se lleva
        // cada una.
        addBareIn(X1, 54.5f, SideChain::DEPTH_CV_INPUT);
        addBareIn(X2, 54.5f, SideChain::VCA_CV_INPUT);
        line(X1, 45.9f, X1, 49.9f);
        line(X2, 45.9f, X2, 49.9f);

        // La fila del disparo, que ahora es también la del panorama: el mando PAN y su
        // jack de CV unidos por la línea, y el TRIG a la derecha. Tres cosas donde
        // antes había dos: el botón manual cedió su sitio (el disparo sigue en el menú)
        // y el TRIG se corre hasta 25,4 mm, fuera de la columna X2 pero con el mismo
        // milímetro de aire al borde que el resto. Ningún jack desaparece, así que los
        // patches anteriores no pierden cables.
        constexpr float PAN_X = 5.0f;
        constexpr float PAN_CV_X = 15.24f;
        constexpr float TRIG_X = 25.4f;
        addLabel("PAN", PAN_X, 60.0f, 10.f);
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(PAN_X, 67.5f)), module,
                                              SideChain::PAN_PARAM));
        addBareIn(PAN_CV_X, 67.5f, SideChain::PAN_CV_INPUT);
        line(PAN_X + 3.3f, 67.5f, PAN_CV_X - 4.1f, 67.5f);
        addLabel("TRIG", TRIG_X, 60.0f, 10.f);
        addBareIn(TRIG_X, 67.5f, SideChain::TRIG_INPUT);

        // Todo lo que entra, arriba de la línea; todo lo que sale, debajo. La línea
        // del panel (y = 88 en el SVG) separa los dos bloques sin moverse de donde
        // estaba: cada fila es una etiqueta y un jack, con el centro del jack 7.5 mm
        // por debajo de la etiqueta y su borde 4.25 mm más allá, así que 88.0 deja
        // 2.25 mm de aire por arriba y 2.5 mm por abajo.
        //
        // Los índices de los enums se quedan como estaban, que es lo que guardan los
        // patches: lo que cambia es dónde se dibuja cada jack, no qué número tiene.
        addIn("IN L", X1, 74.0f, SideChain::IN_L_INPUT);
        addIn("IN R", X2, 74.0f, SideChain::IN_R_INPUT);
        addOut("ENV", X1, 90.5f, SideChain::ENV_OUTPUT);
        addOut("EOC", X2, 90.5f, SideChain::EOC_OUTPUT);
        addOut("OUT L", X1, 104.5f, SideChain::OUT_L_OUTPUT);
        addOut("OUT R", X2, 104.5f, SideChain::OUT_R_OUTPUT);
    }

    void appendContextMenu(ui::Menu* menu) override {
        SideChain* module = dynamic_cast<SideChain*>(this->module);
        if (!module)
            return;

        menu->addChild(new ui::MenuSeparator);
        // What the panel button used to do: fire every channel once. It is
        // what starts a self-cycling patch (EOC into TRIG).
        menu->addChild(createMenuItem("Fire a trigger", "", [=]() {
            module->manualTriggerPending = true;
        }));
        menu->addChild(createSubmenuItem("Mode", GATE_MODE_NAMES[module->gateMode],
                                         [=](ui::Menu* sub) {
            for (int i = 0; i < NUM_GATE_MODES; i++) {
                sub->addChild(createCheckMenuItem(
                    GATE_MODE_NAMES[i], "",
                    [=]() { return module->gateMode == i; },
                    [=]() { module->gateMode = i; }));
            }
        }));

        menu->addChild(createCheckMenuItem(
            "Ping envelope (trigger opens)", "",
            [=]() { return module->pingEnvelope; },
            [=]() { module->pingEnvelope ^= true; }));

        menu->addChild(createSubmenuItem("Recovery curve", CURVE_NAMES[module->curveShape],
                                         [=](ui::Menu* sub) {
            for (int i = 0; i < NUM_CURVE_SHAPES; i++) {
                sub->addChild(createCheckMenuItem(
                    CURVE_NAMES[i], "",
                    [=]() { return module->curveShape == i; },
                    [=]() { module->curveShape = i; }));
            }
        }));

        menu->addChild(createCheckMenuItem(
            "Freeze jitter", "",
            [=]() { return module->freezeJitter; },
            [=]() { module->freezeJitter ^= true; }));

        menu->addChild(createCheckMenuItem(
            "Per-channel envelopes", "",
            [=]() { return module->perChannelEnvelopes; },
            [=]() { module->perChannelEnvelopes ^= true; }));

        menu->addChild(createCheckMenuItem(
            "Level attenuates ENV", "",
            [=]() { return module->levelAffectsEnv; },
            [=]() { module->levelAffectsEnv ^= true; }));

        menu->addChild(createCheckMenuItem(
            "Last in a row: OUT is the chain mix", "",
            [=]() { return module->chainMix; },
            [=]() { module->chainMix ^= true; }));

        menu->addChild(createMenuItem("Reset jitter seed", "", [=]() {
            module->baseSeed = random::u64();
            module->reseedVoices(module->baseSeed);
        }));
    }
};


Model* modelSideChain = createModel<SideChain, SideChainWidget>("SideChain");
