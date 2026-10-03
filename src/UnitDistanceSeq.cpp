#include "plugin.hpp"
#include "ui/CommonWidgets.hpp"

#include <array>
#include <cmath>
#include <cstdint>

using AnimatekUI::TextLabel;

namespace {

static constexpr int MAX_NODES = 64;
static constexpr int MAX_POLY_VOICES = 8;
static constexpr int WALK_HISTORY_SIZE = 8;
static constexpr int LOCK_LOOP_SIZE = 32;
static constexpr int SHORT_LOCK_LOOP_SIZE = 16;
static constexpr float PI = 3.14159265358979323846f;
static constexpr float DEFAULT_CLOCK_PERIOD = 0.125f;

// Scales the X position is quantised into. Natural minor stays first: it was the
// only scale before the selector existed, so it is what older patches load into.
struct Scale {
    const char* name;
    int size;
    std::array<int, 12> degrees;
};

static const Scale SCALES[] = {
    {"Minor", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"Major", 7, {0, 2, 4, 5, 7, 9, 11}},
    {"Dorian", 7, {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygian", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"Lydian", 7, {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydian", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"Harmonic minor", 7, {0, 2, 3, 5, 7, 8, 11}},
    {"Minor pentatonic", 5, {0, 3, 5, 7, 10}},
    {"Major pentatonic", 5, {0, 2, 4, 7, 9}},
    {"Blues", 6, {0, 3, 5, 6, 7, 10}},
    {"Chromatic", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
};
static constexpr int NUM_SCALES = sizeof(SCALES) / sizeof(SCALES[0]);

static const char* ROOT_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct GraphNode {
    float x = 0.f;
    float y = 0.f;
    float nx = 0.5f;
    float ny = 0.5f;
};

struct LockedStep {
    std::array<int, MAX_POLY_VOICES> nodes = {};
    std::array<bool, MAX_POLY_VOICES> gates = {};
    int voiceCount = 1;
};

static float fract(float v) {
    return v - std::floor(v);
}

static uint32_t hashU32(uint32_t v) {
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return v;
}

} // namespace

struct UnitDistanceSeq : Module {
    enum ParamIds {
        SEED_PARAM,
        NODES_PARAM,
        RADIUS_PARAM,
        TOLERANCE_PARAM,
        DENSITY_PARAM,
        WALK_PARAM,
        RANGE_PARAM,
        GATE_LENGTH_PARAM,
        GATE_DENSITY_PARAM,
        LOCK_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        CLOCK_INPUT,
        RESET_INPUT,
        SEED_INPUT,
        DENSITY_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        VOCT_OUTPUT,
        GATE_OUTPUT,
        ACCENT_OUTPUT,
        X_OUTPUT,
        Y_OUTPUT,
        NUM_OUTPUTS
    };

    // Lights are not stored in patches, so dropping the four activity lights
    // (debug readouts that never had a place on the panel) breaks nothing.
    enum LightIds {
        CLOCK_LIGHT,
        GATE_LIGHT,
        NUM_LIGHTS
    };

    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    std::array<dsp::PulseGenerator, MAX_POLY_VOICES> gatePulses;
    dsp::PulseGenerator clockPulse;
    dsp::ClockDivider graphDivider;

    std::array<GraphNode, MAX_NODES> graphNodes;
    std::array<uint64_t, MAX_NODES> neighbors = {};
    std::array<int, MAX_NODES> degrees = {};
    std::array<std::array<GraphNode, MAX_NODES>, MAX_POLY_VOICES> voiceGraphNodes = {};
    std::array<std::array<uint64_t, MAX_NODES>, MAX_POLY_VOICES> voiceNeighbors = {};
    std::array<std::array<int, MAX_NODES>, MAX_POLY_VOICES> voiceDegrees = {};
    std::array<int, MAX_POLY_VOICES> voiceMaxDegrees = {};
    std::array<bool, MAX_POLY_VOICES> voiceGraphHasEdges = {};

    int nodeCount = 16;
    int currentNode = 0;
    int polyVoices = 1;
    bool polyUseVoiceSeeds = false;
    std::array<int, MAX_POLY_VOICES> voiceNodes = {};
    std::array<float, MAX_POLY_VOICES> voicePitchVolts = {};
    std::array<float, MAX_POLY_VOICES> voiceAccentVolts = {};
    std::array<float, MAX_POLY_VOICES> voiceXVolts = {};
    std::array<float, MAX_POLY_VOICES> voiceYVolts = {};
    std::array<uint32_t, MAX_POLY_VOICES> voiceWalkStates = {};
    std::array<int, MAX_POLY_VOICES> voiceNodeOffsets = {};
    std::array<int, WALK_HISTORY_SIZE> walkHistory = {};
    int walkHistoryPos = 0;
    int walkHistoryCount = 0;
    std::array<LockedStep, LOCK_LOOP_SIZE> captureLoop = {};
    std::array<LockedStep, LOCK_LOOP_SIZE> lockedLoop = {};
    int captureWritePos = 0;
    int captureCount = 0;
    int lockedLoopLength = 0;
    int lockedLoopPos = 0;
    int lockDirection = 0;
    bool wasLocking = false;
    int maxDegree = 0;
    int edgeCount = 0;
    uint32_t baseWalkState = 1;
    uint32_t walkState = 1;
    uint32_t gateStep = 0;
    float clockPeriod = DEFAULT_CLOCK_PERIOD;
    float clockTimer = DEFAULT_CLOCK_PERIOD;
    float pitchVolts = 0.f;
    float accentVolts = 0.f;
    bool graphHasEdges = false;
    int scaleIndex = 0;
    int rootSemitone = 0;
    // Set by a reset (and at load): the next clock plays the starting node
    // instead of walking away from it, so the phrase begins where it should.
    bool playCurrentOnNextClock = true;
    // Sample & hold on V/O: the pitch only moves on steps that fire a gate, so
    // the output carries the notes that are heard rather than every node the
    // walk passes through. The node is held, not the voltage, so a change of
    // scale or root still reaches a held note.
    bool sampleHoldPitch = false;
    std::array<int, MAX_POLY_VOICES> heldNodes = {};
    std::array<float, MAX_POLY_VOICES> heldPitchVolts = {};

    int cachedSeed = -999999;
    int cachedNodes = -1;
    int cachedRadius = -1;
    int cachedTolerance = -1;
    int cachedDensity = -1;

    UnitDistanceSeq() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        graphDivider.setDivision(64);
        for (int i = 0; i < MAX_POLY_VOICES; ++i)
            voiceNodes[i] = 0;

        configParam(SEED_PARAM, 0.f, 999.f, 1.f, "Seed");
        paramQuantities[SEED_PARAM]->snapEnabled = true;
        configParam(NODES_PARAM, 8.f, 64.f, 16.f, "Nodes");
        paramQuantities[NODES_PARAM]->snapEnabled = true;
        configParam(RADIUS_PARAM, 0.25f, 2.f, 1.f, "Unit radius");
        configParam(TOLERANCE_PARAM, 0.01f, 0.20f, 0.04f, "Tolerance fine");
        configParam(DENSITY_PARAM, 0.f, 1.f, 0.35f, "Density", "%", 0.f, 100.f);
        configParam(WALK_PARAM, 0.f, 2.f, 0.f, "Walk mode");
        paramQuantities[WALK_PARAM]->snapEnabled = true;
        configParam(RANGE_PARAM, 1.f, 4.f, 2.f, "Pitch range", " oct");
        paramQuantities[RANGE_PARAM]->snapEnabled = true;
        configParam(GATE_LENGTH_PARAM, 0.05f, 0.95f, 0.45f, "Gate length", "%", 0.f, 100.f);
        configParam(GATE_DENSITY_PARAM, 0.15f, 1.f, 0.75f, "Gate density", "%", 0.f, 100.f);
        configParam(LOCK_PARAM, -1.f, 1.f, 0.f, "Lock", "%", 0.f, 100.f);

        configInput(CLOCK_INPUT, "Clock");
        configInput(RESET_INPUT, "Reset");
        configInput(SEED_INPUT, "Seed CV");
        configInput(DENSITY_INPUT, "Density CV");

        configOutput(VOCT_OUTPUT, "V/oct");
        configOutput(GATE_OUTPUT, "Gate");
        configOutput(ACCENT_OUTPUT, "Accent");
        configOutput(X_OUTPUT, "X CV");
        configOutput(Y_OUTPUT, "Y CV");

        rebuildGraph(true);
        resetPolyVoices();
    }

    int effectiveSeed() {
        float seed = params[SEED_PARAM].getValue();
        if (inputs[SEED_INPUT].isConnected())
            seed += inputs[SEED_INPUT].getVoltage() * 10.f;
        return clamp((int)std::round(seed), -9999, 9999);
    }

    float effectiveDensity() {
        float density = params[DENSITY_PARAM].getValue();
        if (inputs[DENSITY_INPUT].isConnected())
            density += inputs[DENSITY_INPUT].getVoltage() * 0.1f;
        return clamp(density, 0.f, 1.f);
    }

    float effectiveTolerance() {
        float fine = params[TOLERANCE_PARAM].getValue();
        float density = effectiveDensity();
        float musicalWidth = 0.01f + density * density * 0.42f;
        return clamp(fine + musicalWidth, 0.01f, 0.5f);
    }

    /** Places the nodes for a seed and joins every pair whose distance lies
    within tolerance of the unit radius. One routine for the main graph and the
    per-voice ones, so the two can never drift apart. */
    void buildGraph(int seed, float radius, float tolerance,
                    std::array<GraphNode, MAX_NODES>& nodes,
                    std::array<uint64_t, MAX_NODES>& adjacency,
                    std::array<int, MAX_NODES>& degree,
                    int& outMaxDegree, int& outEdges) const {
        constexpr float alpha = 0.61803398875f;
        constexpr float beta = 0.41421356237f;
        constexpr float spread = 0.65f;
        float minX = 10.f;
        float minY = 10.f;
        float maxX = -10.f;
        float maxY = -10.f;

        for (int i = 0; i < nodeCount; ++i) {
            float a = fract((float)seed * 0.017f + (float)i * alpha);
            float b = fract((float)seed * 0.031f + (float)i * beta);
            float theta1 = 2.f * PI * a;
            float theta2 = 2.f * PI * b;
            nodes[i].x = std::cos(theta1) + spread * std::cos(theta2);
            nodes[i].y = std::sin(theta1) + spread * std::sin(theta2);
            minX = std::min(minX, nodes[i].x);
            minY = std::min(minY, nodes[i].y);
            maxX = std::max(maxX, nodes[i].x);
            maxY = std::max(maxY, nodes[i].y);
            adjacency[i] = 0u;
            degree[i] = 0;
        }

        float xSpan = std::max(0.0001f, maxX - minX);
        float ySpan = std::max(0.0001f, maxY - minY);
        for (int i = 0; i < nodeCount; ++i) {
            nodes[i].nx = clamp((nodes[i].x - minX) / xSpan, 0.f, 1.f);
            nodes[i].ny = clamp((nodes[i].y - minY) / ySpan, 0.f, 1.f);
        }

        outEdges = 0;
        outMaxDegree = 0;
        for (int i = 0; i < nodeCount; ++i) {
            for (int j = i + 1; j < nodeCount; ++j) {
                float dx = nodes[i].x - nodes[j].x;
                float dy = nodes[i].y - nodes[j].y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (std::abs(d - radius) < tolerance) {
                    adjacency[i] |= (1ull << j);
                    adjacency[j] |= (1ull << i);
                    degree[i]++;
                    degree[j]++;
                    outEdges++;
                }
            }
        }

        for (int i = 0; i < nodeCount; ++i)
            outMaxDegree = std::max(outMaxDegree, degree[i]);
    }

    void generateVoiceGraph(int voice, int seed, float radius, float tolerance) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        int voiceEdgeCount = 0;
        buildGraph(seed, radius, tolerance, voiceGraphNodes[voice], voiceNeighbors[voice],
                   voiceDegrees[voice], voiceMaxDegrees[voice], voiceEdgeCount);
        voiceGraphHasEdges[voice] = voiceEdgeCount > 0;
    }

    void copyMainGraphToVoice(int voice) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        for (int i = 0; i < nodeCount; ++i) {
            voiceGraphNodes[voice][i] = graphNodes[i];
            voiceNeighbors[voice][i] = neighbors[i];
            voiceDegrees[voice][i] = degrees[i];
        }
        voiceMaxDegrees[voice] = maxDegree;
        voiceGraphHasEdges[voice] = graphHasEdges;
    }

    /** Rebuilds the graph when the geometry controls have moved.

    A forced rebuild also rewinds the walk to its deterministic start. One that
    merely follows a knob or a CV does not: it reshapes the network under the
    walk and lets it carry on. Rewinding here used to zero the gate counter on
    every change, so a modulated DENS kept replaying the first step's gates. */
    void rebuildGraph(bool force = false) {
        int seed = effectiveSeed();
        int nodes = clamp((int)std::round(params[NODES_PARAM].getValue()), 8, MAX_NODES);
        float radius = clamp(params[RADIUS_PARAM].getValue(), 0.25f, 2.f);
        float tolerance = effectiveTolerance();
        float density = effectiveDensity();
        int radiusKey = (int)std::round(radius * 1000.f);
        int toleranceKey = (int)std::round(tolerance * 1000.f);
        int densityKey = (int)std::round(density * 1000.f);

        if (!force && seed == cachedSeed && nodes == cachedNodes &&
            radiusKey == cachedRadius && toleranceKey == cachedTolerance &&
            densityKey == cachedDensity)
            return;

        cachedSeed = seed;
        cachedNodes = nodes;
        cachedRadius = radiusKey;
        cachedTolerance = toleranceKey;
        cachedDensity = densityKey;
        nodeCount = nodes;
        currentNode = clamp(currentNode, 0, nodeCount - 1);
        for (int v = 0; v < MAX_POLY_VOICES; ++v) {
            voiceNodes[v] = clamp(voiceNodes[v], 0, nodeCount - 1);
            heldNodes[v] = clamp(heldNodes[v], 0, nodeCount - 1);
        }
        for (int& node : walkHistory) {
            if (node >= nodeCount)
                node = -1;
        }

        buildGraph(seed, radius, tolerance, graphNodes, neighbors, degrees, maxDegree, edgeCount);
        graphHasEdges = edgeCount > 0;
        copyMainGraphToVoice(0);
        for (int v = 1; v < MAX_POLY_VOICES; ++v) {
            if (polyUseVoiceSeeds)
                generateVoiceGraph(v, seed + v * 137 + nodeCount * 17, radius, tolerance);
            else
                copyMainGraphToVoice(v);
        }
        baseWalkState = hashU32((uint32_t)(seed * 73856093u) ^ (uint32_t)radiusKey ^ ((uint32_t)toleranceKey << 11) ^ ((uint32_t)densityKey << 3));
        for (int v = 0; v < MAX_POLY_VOICES; ++v) {
            voiceNodeOffsets[v] = (int)(hashU32(baseWalkState ^ (uint32_t)(nodeCount * (v + 1)) ^
                                                (uint32_t)(v * 0x85ebca6bu)) % (uint32_t)std::max(1, nodeCount));
        }
        if (force) {
            walkState = baseWalkState;
            for (int v = 0; v < MAX_POLY_VOICES; ++v)
                voiceWalkStates[v] = hashU32(baseWalkState ^ (uint32_t)(v * 0x9e3779b9u));
            gateStep = 0;
            clearWalkHistory();
        }
        updateAllVoiceOutputs();
    }

    void clearWalkHistory() {
        walkHistory.fill(-1);
        walkHistoryPos = 0;
        walkHistoryCount = 0;
    }

    void pushWalkHistory(int node) {
        walkHistory[walkHistoryPos] = node;
        walkHistoryPos = (walkHistoryPos + 1) % WALK_HISTORY_SIZE;
        walkHistoryCount = std::min(walkHistoryCount + 1, WALK_HISTORY_SIZE);
    }

    bool isInWalkHistory(int node) const {
        for (int i = 0; i < walkHistoryCount; ++i) {
            if (walkHistory[i] == node)
                return true;
        }
        return false;
    }

    void recordStep(const std::array<bool, MAX_POLY_VOICES>& gates) {
        captureLoop[captureWritePos].voiceCount = polyVoices;
        for (int v = 0; v < MAX_POLY_VOICES; ++v) {
            captureLoop[captureWritePos].nodes[v] = voiceNodes[v];
            captureLoop[captureWritePos].gates[v] = gates[v];
        }
        captureWritePos = (captureWritePos + 1) % LOCK_LOOP_SIZE;
        captureCount = std::min(captureCount + 1, LOCK_LOOP_SIZE);
    }

    void freezeLockLoop(int length) {
        lockedLoopLength = clamp(length, 1, LOCK_LOOP_SIZE);
        int oldest = (captureCount < LOCK_LOOP_SIZE) ? 0 : captureWritePos;
        if (captureCount == 0) {
            lockedLoop[0].voiceCount = polyVoices;
            for (int v = 0; v < MAX_POLY_VOICES; ++v) {
                lockedLoop[0].nodes[v] = voiceNodes[v];
                lockedLoop[0].gates[v] = false;
            }
        }
        else {
            int available = std::max(1, captureCount);
            for (int i = 0; i < lockedLoopLength; ++i)
                lockedLoop[i] = captureLoop[(oldest + (i % available)) % LOCK_LOOP_SIZE];
        }
        lockedLoopPos = 0;
    }

    float lockAmount() {
        return std::abs(params[LOCK_PARAM].getValue());
    }

    int requestedLockLength() {
        return params[LOCK_PARAM].getValue() < 0.f ? LOCK_LOOP_SIZE : SHORT_LOCK_LOOP_SIZE;
    }

    int requestedLockDirection() {
        float v = params[LOCK_PARAM].getValue();
        if (std::abs(v) < 0.01f)
            return 0;
        return v < 0.f ? -1 : 1;
    }

    bool shouldUseLockedStep(int pos, float amount) const {
        if (amount <= 0.001f)
            return false;
        if (amount >= 0.999f)
            return true;
        uint32_t h = hashU32(baseWalkState ^ (uint32_t)(pos * 1103515245u) ^ 0x51ed270bu);
        float threshold = (float)(h & 0x00ffffffu) / (float)0x01000000u;
        return threshold < amount;
    }

    int firstConnectedNodeAfter(int start) const {
        for (int offset = 1; offset <= nodeCount; ++offset) {
            int idx = (start + offset) % nodeCount;
            if (degrees[idx] > 0)
                return idx;
        }
        return start;
    }

    int firstConnectedNodeAfterForVoice(int voice, int start) const {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        for (int offset = 1; offset <= nodeCount; ++offset) {
            int idx = (start + offset) % nodeCount;
            if (voiceDegrees[voice][idx] > 0)
                return idx;
        }
        return start;
    }

    int neighborByRank(int node, int rank) const {
        uint64_t mask = neighbors[node];
        int count = 0;
        for (int i = 0; i < nodeCount; ++i) {
            if (mask & (1ull << i)) {
                if (count == rank)
                    return i;
                count++;
            }
        }
        return node;
    }

    int chooseNextNode() {
        if (!graphHasEdges)
            return currentNode;
        if (degrees[currentNode] == 0)
            return firstConnectedNodeAfter(currentNode);

        int mode = clamp((int)std::round(params[WALK_PARAM].getValue()), 0, 2);
        if (mode == 1) {
            walkState = hashU32(walkState + 0x9e3779b9u + (uint32_t)currentNode);
            return neighborByRank(currentNode, (int)(walkState % (uint32_t)degrees[currentNode]));
        }
        if (mode == 2) {
            int best = currentNode;
            int bestDegree = -1;
            int fallback = currentNode;
            int fallbackDegree = -1;
            uint64_t mask = neighbors[currentNode];
            for (int i = 0; i < nodeCount; ++i) {
                if (!(mask & (1ull << i)))
                    continue;
                if (degrees[i] > fallbackDegree) {
                    fallback = i;
                    fallbackDegree = degrees[i];
                }
                if (!isInWalkHistory(i) && degrees[i] > bestDegree) {
                    best = i;
                    bestDegree = degrees[i];
                }
            }
            if (bestDegree < 0)
                return fallback;
            return best;
        }

        uint64_t mask = neighbors[currentNode];
        for (int offset = 1; offset <= nodeCount; ++offset) {
            int idx = (currentNode + offset) % nodeCount;
            if (mask & (1ull << idx))
                return idx;
        }
        return currentNode;
    }

    void resetPolyVoices() {
        polyVoices = clamp(polyVoices, 1, MAX_POLY_VOICES);
        for (int v = 0; v < MAX_POLY_VOICES; ++v) {
            int spread = std::max(1, nodeCount / std::max(1, polyVoices));
            voiceNodeOffsets[v] = (int)(hashU32(baseWalkState ^ (uint32_t)(nodeCount * (v + 1)) ^
                                                (uint32_t)(v * 0x85ebca6bu)) % (uint32_t)std::max(1, nodeCount));
            voiceNodes[v] = (v * spread + voiceNodeOffsets[v]) % std::max(1, nodeCount);
            voiceWalkStates[v] = hashU32(baseWalkState ^ (uint32_t)(v * 0x9e3779b9u) ^
                                         (uint32_t)(voiceNodeOffsets[v] * 0x7feb352du));
        }
        currentNode = voiceNodes[0];
        heldNodes = voiceNodes;
        clearWalkHistory();
        updateAllVoiceOutputs();
    }

    const Scale& currentScale() const {
        // Explicit bounds check rather than clamp(): cppcheck cannot follow
        // clamp's return value into the array index.
        if (scaleIndex < 0 || scaleIndex >= NUM_SCALES)
            return SCALES[0];
        return SCALES[scaleIndex];
    }

    float pitchVoltsForNode(int node, int voice) {
        const Scale& scale = currentScale();
        int step = pitchIndexForNode(node, voice);
        int octave = step / scale.size;
        int degree = scale.degrees[step % scale.size];
        return (float)octave + (float)(degree + rootSemitone) / 12.f;
    }

    void updateVoiceOutputs(int voice) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        int nodeIndex = clamp(voiceNodes[voice], 0, nodeCount - 1);
        const GraphNode& node = voiceGraphNodes[voice][nodeIndex];
        voicePitchVolts[voice] = pitchVoltsForNode(nodeIndex, voice);
        heldPitchVolts[voice] = pitchVoltsForNode(heldNodes[voice], voice);
        int vMaxDegree = voiceMaxDegrees[voice];
        voiceAccentVolts[voice] = (vMaxDegree > 0) ? clamp((float)voiceDegrees[voice][nodeIndex] / (float)vMaxDegree, 0.f, 1.f) * 10.f : 0.f;
        voiceXVolts[voice] = node.nx * 10.f;
        voiceYVolts[voice] = node.ny * 10.f;
        if (voice == 0) {
            currentNode = nodeIndex;
            pitchVolts = voicePitchVolts[voice];
            accentVolts = voiceAccentVolts[voice];
        }
    }

    void updateAllVoiceOutputs() {
        for (int v = 0; v < polyVoices; ++v)
            updateVoiceOutputs(v);
    }

    bool shouldFireGate(int node, int voice = 0) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        node = clamp(node, 0, nodeCount - 1);
        if (!voiceGraphHasEdges[voice] || voiceDegrees[voice][node] <= 0)
            return false;

        float gateDensity = clamp(params[GATE_DENSITY_PARAM].getValue(), 0.f, 1.f);
        if (gateDensity <= 0.f)
            return false;
        if (gateDensity >= 0.999f)
            return true;

        uint32_t h = hashU32(baseWalkState ^ (uint32_t)(node * 2654435761u) ^
                             (gateStep * 2246822519u) ^ (uint32_t)(voice * 374761393u));
        float randomPart = (float)(h & 0x00ffffffu) / (float)0x01000000u;
        float degreeNorm = (voiceMaxDegrees[voice] > 0) ? clamp((float)voiceDegrees[voice][node] / (float)voiceMaxDegrees[voice], 0.f, 1.f) : 0.f;
        float score = randomPart * 0.7f + (1.f - degreeNorm) * 0.3f;
        return score < gateDensity;
    }

    bool nodeUsedByEarlierVoice(int node, int voice) const {
        for (int v = 0; v < voice; ++v) {
            if (voiceNodes[v] == node)
                return true;
        }
        return false;
    }

    int pitchIndexForNode(int node, int voice = 0) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        node = clamp(node, 0, nodeCount - 1);
        int range = clamp((int)std::round(params[RANGE_PARAM].getValue()), 1, 4);
        int scaleSteps = currentScale().size * range;
        return clamp((int)std::floor(voiceGraphNodes[voice][node].nx * (float)scaleSteps), 0, scaleSteps - 1);
    }

    bool pitchUsedByEarlierVoice(int node, int voice) {
        int pitchIndex = pitchIndexForNode(node, voice);
        for (int v = 0; v < voice; ++v) {
            if (pitchIndexForNode(voiceNodes[v], v) == pitchIndex)
                return true;
        }
        return false;
    }

    bool voiceCandidateIsFree(int node, int voice) {
        return !nodeUsedByEarlierVoice(node, voice) && !pitchUsedByEarlierVoice(node, voice);
    }

    int neighborByRankAvoidingVoices(int node, int rank, int voice) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        uint64_t mask = voiceNeighbors[voice][node];
        int count = 0;
        int fallback = node;
        int fallbackDifferentNode = node;
        for (int i = 0; i < nodeCount; ++i) {
            if (!(mask & (1ull << i)))
                continue;
            if (fallback == node)
                fallback = i;
            if (fallbackDifferentNode == node && !nodeUsedByEarlierVoice(i, voice))
                fallbackDifferentNode = i;
            if (!voiceCandidateIsFree(i, voice))
                continue;
            if (count == rank)
                return i;
            count++;
        }
        return fallbackDifferentNode != node ? fallbackDifferentNode : fallback;
    }

    int chooseNextNodeForVoice(int voice) {
        // El rango se comprueba entero aquí, no con un clamp: cppcheck no puede
        // seguir el valor de retorno de clamp() y marcaba los accesos de abajo
        // como fuera de rango. Una voz inválida cae en la principal.
        if (voice <= 0 || voice >= MAX_POLY_VOICES)
            return chooseNextNode();

        int node = clamp(voiceNodes[voice], 0, nodeCount - 1);
        if (!voiceGraphHasEdges[voice])
            return (node + voiceNodeOffsets[voice]) % std::max(1, nodeCount);
        if (voiceDegrees[voice][node] == 0)
            return firstConnectedNodeAfterForVoice(voice, (node + voiceNodeOffsets[voice]) % nodeCount);

        int mode = clamp((int)std::round(params[WALK_PARAM].getValue()), 0, 2);
        if (mode == 1) {
            voiceWalkStates[voice] = hashU32(voiceWalkStates[voice] + 0x9e3779b9u +
                                             (uint32_t)node + (uint32_t)(voice * 97));
            int available = std::max(1, voiceDegrees[voice][node]);
            return neighborByRankAvoidingVoices(node, (int)(voiceWalkStates[voice] % (uint32_t)available), voice);
        }
        if (mode == 2) {
            int best = node;
            int bestDegree = -1;
            int fallback = node;
            int fallbackDegree = -1;
            uint64_t mask = voiceNeighbors[voice][node];
            int start = (node + voice) % nodeCount;
            for (int offset = 0; offset < nodeCount; ++offset) {
                int i = (start + offset) % nodeCount;
                if (!(mask & (1ull << i)))
                    continue;
                if (voiceDegrees[voice][i] > fallbackDegree) {
                    fallback = i;
                    fallbackDegree = voiceDegrees[voice][i];
                }
                if (voiceCandidateIsFree(i, voice) && voiceDegrees[voice][i] > bestDegree) {
                    best = i;
                    bestDegree = voiceDegrees[voice][i];
                }
            }
            if (bestDegree < 0)
                return fallback;
            return best;
        }

        uint64_t mask = voiceNeighbors[voice][node];
        int startOffset = 1 + voice + voiceNodeOffsets[voice];
        for (int offset = startOffset; offset <= nodeCount + startOffset; ++offset) {
            int idx = (node + offset) % nodeCount;
            if ((mask & (1ull << idx)) && voiceCandidateIsFree(idx, voice))
                return idx;
        }
        for (int offset = startOffset; offset <= nodeCount + startOffset; ++offset) {
            int idx = (node + offset) % nodeCount;
            if ((mask & (1ull << idx)) && !nodeUsedByEarlierVoice(idx, voice))
                return idx;
        }
        for (int offset = startOffset; offset <= nodeCount + startOffset; ++offset) {
            int idx = (node + offset) % nodeCount;
            if (mask & (1ull << idx))
                return idx;
        }
        return node;
    }

    void triggerGate(int voice) {
        float gateLen = clamp(params[GATE_LENGTH_PARAM].getValue(), 0.05f, 0.95f) * clockPeriod;
        gatePulses[voice].trigger(clamp(gateLen, 0.001f, 2.f));
        heldNodes[voice] = voiceNodes[voice];
        heldPitchVolts[voice] = voicePitchVolts[voice];
    }

    /** One free step: every voice moves along an edge, and the gates that fire
    are recorded for LOCK to capture. The first clock after a reset leaves the
    voices where the reset put them, so the starting node is heard instead of
    skipped. */
    void walkOneStep() {
        // The starting step runs on gate counter 0 and leaves it there, so from
        // the second step on the gates fall exactly where they did before this
        // step existed: older patches keep their rhythm, with the start in front.
        if (playCurrentOnNextClock) {
            playCurrentOnNextClock = false;
        }
        else {
            int nextNode = chooseNextNode();
            pushWalkHistory(currentNode);
            voiceNodes[0] = nextNode;
            currentNode = voiceNodes[0];
            for (int v = 1; v < polyVoices; ++v)
                voiceNodes[v] = chooseNextNodeForVoice(v);
            gateStep++;
        }
        updateAllVoiceOutputs();
        std::array<bool, MAX_POLY_VOICES> fireGates = {};
        for (int v = 0; v < polyVoices; ++v) {
            fireGates[v] = shouldFireGate(voiceNodes[v], v);
            if (fireGates[v])
                triggerGate(v);
        }
        recordStep(fireGates);
    }

    void onClock() {
        walkOneStep();
        clockPulse.trigger(0.03f);
    }

    void onHybridClock(float amount) {
        if (lockedLoopLength <= 0)
            freezeLockLoop(requestedLockLength());

        int pos = lockedLoopPos;
        lockedLoopPos = (lockedLoopPos + 1) % std::max(1, lockedLoopLength);

        if (shouldUseLockedStep(pos, amount)) {
            // The locked loop already starts on its first step after a reset.
            playCurrentOnNextClock = false;
            const LockedStep& step = lockedLoop[pos];
            int lockedVoices = clamp(step.voiceCount, 1, MAX_POLY_VOICES);
            for (int v = 0; v < polyVoices; ++v)
                voiceNodes[v] = clamp(step.nodes[v % lockedVoices], 0, nodeCount - 1);
            currentNode = voiceNodes[0];
            updateAllVoiceOutputs();
            gateStep++;
            for (int v = 0; v < polyVoices; ++v) {
                if (step.gates[v % lockedVoices])
                    triggerGate(v);
            }
        }
        else {
            walkOneStep();
        }
        clockPulse.trigger(0.03f);
    }

    void onReset(const ResetEvent& e) override {
        Module::onReset(e);
        polyVoices = 1;
        polyUseVoiceSeeds = false;
        scaleIndex = 0;
        rootSemitone = 0;
        sampleHoldPitch = false;
        rebuildGraph(true);
        resetPolyVoices();
        playCurrentOnNextClock = true;
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "polyVoices", json_integer(polyVoices));
        json_object_set_new(root, "polyUseVoiceSeeds",
                            json_boolean(polyUseVoiceSeeds));
        json_object_set_new(root, "scale", json_integer(scaleIndex));
        json_object_set_new(root, "root", json_integer(rootSemitone));
        json_object_set_new(root, "sampleHoldPitch", json_boolean(sampleHoldPitch));
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root)
            return;
        if (json_t* j = json_object_get(root, "polyVoices"))
            polyVoices = clamp((int)json_integer_value(j), 1, MAX_POLY_VOICES);
        if (json_t* j = json_object_get(root, "polyUseVoiceSeeds"))
            polyUseVoiceSeeds = json_is_true(j);
        // Missing in patches saved before the selector: they keep C minor.
        if (json_t* j = json_object_get(root, "scale"))
            scaleIndex = clamp((int)json_integer_value(j), 0, NUM_SCALES - 1);
        if (json_t* j = json_object_get(root, "root"))
            rootSemitone = clamp((int)json_integer_value(j), 0, 11);
        if (json_t* j = json_object_get(root, "sampleHoldPitch"))
            sampleHoldPitch = json_is_true(j);
        rebuildGraph(true);
        resetPolyVoices();
    }

    void process(const ProcessArgs& args) override {
        clockTimer += args.sampleTime;

        float amount = lockAmount();
        bool locking = amount > 0.01f;
        int direction = requestedLockDirection();
        int lockLength = requestedLockLength();
        if (locking && (!wasLocking || direction != lockDirection || lockLength != lockedLoopLength)) {
            freezeLockLoop(lockLength);
        }
        else if (!locking && wasLocking) {
            clearWalkHistory();
            rebuildGraph(true);
        }
        wasLocking = locking;
        lockDirection = direction;

        if ((!locking || amount < 0.999f) && graphDivider.process())
            rebuildGraph();

        if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
            clearWalkHistory();
            walkState = baseWalkState;
            gateStep = 0;
            if (locking && lockedLoopLength > 0) {
                lockedLoopPos = 0;
                int lockedVoices = clamp(lockedLoop[0].voiceCount, 1, MAX_POLY_VOICES);
                for (int v = 0; v < polyVoices; ++v)
                    voiceNodes[v] = clamp(lockedLoop[0].nodes[v % lockedVoices], 0, nodeCount - 1);
                currentNode = voiceNodes[0];
            }
            else {
                resetPolyVoices();
            }
            playCurrentOnNextClock = true;
            updateAllVoiceOutputs();
            for (int v = 0; v < MAX_POLY_VOICES; ++v)
                gatePulses[v].reset();
        }

        if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
            clockPeriod = clamp(clockTimer, 0.005f, 2.f);
            clockTimer = 0.f;
            if (locking)
                onHybridClock(amount);
            else {
                rebuildGraph();
                onClock();
            }
        }

        std::array<bool, MAX_POLY_VOICES> gateHigh = {};
        for (int v = 0; v < MAX_POLY_VOICES; ++v)
            gateHigh[v] = gatePulses[v].process(args.sampleTime);
        bool clockHigh = clockPulse.process(args.sampleTime);
        outputs[VOCT_OUTPUT].setChannels(polyVoices);
        outputs[GATE_OUTPUT].setChannels(polyVoices);
        outputs[ACCENT_OUTPUT].setChannels(polyVoices);
        outputs[X_OUTPUT].setChannels(polyVoices);
        outputs[Y_OUTPUT].setChannels(polyVoices);
        for (int v = 0; v < polyVoices; ++v) {
            outputs[VOCT_OUTPUT].setVoltage(sampleHoldPitch ? heldPitchVolts[v] : voicePitchVolts[v], v);
            outputs[GATE_OUTPUT].setVoltage(gateHigh[v] ? 10.f : 0.f, v);
            outputs[ACCENT_OUTPUT].setVoltage(voiceAccentVolts[v], v);
            outputs[X_OUTPUT].setVoltage(voiceXVolts[v], v);
            outputs[Y_OUTPUT].setVoltage(voiceYVolts[v], v);
        }

        lights[CLOCK_LIGHT].setBrightnessSmooth(clockHigh ? 1.f : 0.f, args.sampleTime);
        lights[GATE_LIGHT].setBrightnessSmooth(gateHigh[0] ? 1.f : 0.f, args.sampleTime);
    }
};

struct UnitDistanceGraphDisplay : TransparentWidget {
    UnitDistanceSeq* module = nullptr;

    explicit UnitDistanceGraphDisplay(UnitDistanceSeq* module) : module(module) {}

    // The flat palette of the other displays (FILTERtek, ADSRtek): grey for the
    // network, the logo blue for what is happening, white for where it is.
    static void edge(NVGcontext* vg, float ax, float ay, float bx, float by, bool current) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, ax, ay);
        nvgLineTo(vg, bx, by);
        nvgStrokeColor(vg, current ? AnimatekUI::logoBlue(200) : nvgRGBA(0x5a, 0x63, 0x75, 90));
        nvgStrokeWidth(vg, current ? 1.3f : 0.6f);
        nvgStroke(vg);
    }

    static void node(NVGcontext* vg, float x, float y, float r, float strength) {
        // strength 0..1: how likely this node is to fire a gate.
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r);
        nvgFillColor(vg, nvgRGBA(0x8a, 0x95, 0xa8, (uint8_t)(70.f + 150.f * strength)));
        nvgFill(vg);
    }

    static void voice(NVGcontext* vg, float x, float y, float r) {
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r);
        nvgFillColor(vg, AnimatekUI::logoBlue(230));
        nvgFill(vg);
    }

    static void current(NVGcontext* vg, float x, float y, float r) {
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r + 4.f);
        nvgFillColor(vg, AnimatekUI::logoBlue(70));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, r);
        nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
        nvgFill(vg);
    }

    /** Grafo de muestra para cuando no hay módulo: doce nodos en anillo, las
    aristas del anillo y sus diagonales cortas, con uno marcado como el actual.
    Mismo dibujo en cada frame. */
    template <typename PxFn, typename PyFn>
    void drawPreviewGraph(NVGcontext* vg, PxFn px, PyFn py) {
        constexpr int PREVIEW_NODES = 12;
        constexpr float RADIUS = 0.38f;
        float nx[PREVIEW_NODES], ny[PREVIEW_NODES];
        for (int i = 0; i < PREVIEW_NODES; ++i) {
            float a = 2.f * (float)M_PI * (float)i / (float)PREVIEW_NODES;
            nx[i] = 0.5f + RADIUS * std::cos(a);
            ny[i] = 0.5f + RADIUS * std::sin(a);
        }
        // Cada arista sale una sola vez: yendo siempre hacia adelante, {i, i+1} y
        // {i, i+2} no se repiten ni chocan entre sí. Nada de saltarse las que dan
        // la vuelta, o el anillo quedaría abierto por un lado.
        const int cur = 0;
        for (int i = 0; i < PREVIEW_NODES; ++i)
            for (int step : {1, 2}) {
                int j = (i + step) % PREVIEW_NODES;
                edge(vg, px(nx[i]), py(ny[i]), px(nx[j]), py(ny[j]), i == cur || j == cur);
            }
        for (int i = 0; i < PREVIEW_NODES; ++i) {
            if (i == cur)
                current(vg, px(nx[i]), py(ny[i]), 2.6f);
            else
                node(vg, px(nx[i]), py(ny[i]), 1.6f, 0.5f);
        }
    }

    void draw(const DrawArgs& args) override {
        const float w = box.size.x, h = box.size.y;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, w, h, 2.5f);
        nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x13));
        nvgFill(args.vg);
        nvgStrokeWidth(args.vg, 1.f);
        nvgStrokeColor(args.vg, nvgRGB(0x33, 0x38, 0x4a));
        nvgStroke(args.vg);
        // A faint grid in quarters, as the filter's display has its decades.
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStrokeColor(args.vg, nvgRGB(0x1d, 0x21, 0x29));
        for (int k = 1; k < 4; k++) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, w * k / 4.f, 1.f);
            nvgLineTo(args.vg, w * k / 4.f, h - 1.f);
            nvgStroke(args.vg);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 1.f, h * k / 4.f);
            nvgLineTo(args.vg, w - 1.f, h * k / 4.f);
            nvgStroke(args.vg);
        }
    }

    /** The network on the light layer, so it stays lit in a dimmed room. */
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) {
            TransparentWidget::drawLayer(args, layer);
            return;
        }
        NVGcontext* vg = args.vg;
        float pad = 4.f;
        auto px = [&](float x) { return pad + x * (box.size.x - 2.f * pad); };
        auto py = [&](float y) { return pad + (1.f - y) * (box.size.y - 2.f * pad); };

        // El navegador de módulos y la web de la librería dibujan con module ==
        // nullptr. Sin esto el display sale como un recuadro vacío, así que se
        // pinta un grafo de muestra: un anillo fijo, sin estado ni aleatoriedad.
        if (!module) {
            drawPreviewGraph(vg, px, py);
            TransparentWidget::drawLayer(args, layer);
            return;
        }

        for (int i = 0; i < module->nodeCount; ++i) {
            uint64_t mask = module->neighbors[i];
            for (int j = i + 1; j < module->nodeCount; ++j) {
                if (!(mask & (1ull << j)))
                    continue;
                edge(vg, px(module->graphNodes[i].nx), py(module->graphNodes[i].ny),
                     px(module->graphNodes[j].nx), py(module->graphNodes[j].ny),
                     i == module->currentNode || j == module->currentNode);
            }
        }

        const float gateDensity = clamp(module->params[UnitDistanceSeq::GATE_DENSITY_PARAM].getValue(), 0.15f, 1.f);
        const float gateLength = clamp(module->params[UnitDistanceSeq::GATE_LENGTH_PARAM].getValue(), 0.05f, 0.95f);
        for (int i = 0; i < module->nodeCount; ++i) {
            float x = px(module->graphNodes[i].nx), y = py(module->graphNodes[i].ny);
            float degreeNorm = module->maxDegree > 0 ? clamp((float)module->degrees[i] / (float)module->maxDegree, 0.f, 1.f) : 0.f;
            float gateChance = clamp(gateDensity * (0.45f + degreeNorm * 0.55f), 0.f, 1.f);
            bool polyActive = false;
            for (int v = 1; v < module->polyVoices; ++v)
                if (module->voiceNodes[v] == i) {
                    polyActive = true;
                    break;
                }
            if (i == module->currentNode)
                current(vg, x, y, 2.0f + gateLength * 1.8f);
            else if (polyActive)
                voice(vg, x, y, 1.8f + gateLength * 1.3f);
            else
                node(vg, x, y, 1.1f + degreeNorm * 1.1f, gateChance);
        }
        TransparentWidget::drawLayer(args, layer);
    }
};

struct UnitDistanceSeqWidget : ModuleWidget {
    UnitDistanceSeqWidget(UnitDistanceSeq* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/UnitDistanceSeq.svg")));

        auto label = [&](const char* text, float x, float y, float w = 20.f) {
            auto* l = new TextLabel(text, mm2px(Vec(x - w * 0.5f, y)), mm2px(Vec(w, 4.f)));
            l->fontSize = 7.5f;
            addChild(l);
        };

        auto* moduleName = new TextLabel("UNIT-D", mm2px(Vec(1.8f, 119.9f)), mm2px(Vec(25.4f, 5.8f)));
        moduleName->fontSize = 16.f;
        moduleName->color = nvgRGB(0x2C, 0x7F, 0xFF);
        addChild(moduleName);

        label("CLK", 9.f, 2.2f, 10.f);
        label("RST", 23.f, 2.2f, 10.f);
        label("SEED", 37.f, 2.2f, 12.f);
        label("DENS", 51.f, 2.2f, 13.f);
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(9.f, 11.f)), module, UnitDistanceSeq::CLOCK_INPUT));
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(23.f, 11.f)), module, UnitDistanceSeq::RESET_INPUT));
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(37.f, 11.f)), module, UnitDistanceSeq::SEED_INPUT));
        addInput(createInputCentered<AnimatekUI::TekInputPort>(mm2px(Vec(51.f, 11.f)), module, UnitDistanceSeq::DENSITY_INPUT));
        // Clock LED in the gap between CLK and RST, gate LED between GATE and
        // ACC: each sits beside the jack whose activity it shows.
        addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(16.f, 11.f)), module,
                                                            UnitDistanceSeq::CLOCK_LIGHT));

        auto* display = new UnitDistanceGraphDisplay(module);
        display->box.pos = mm2px(Vec(2.8f, 17.f));
        display->box.size = mm2px(Vec(55.3f, 27.f));
        addChild(display);

        label("SEED", 15.f, 44.8f);
        label("NODES", 45.f, 44.8f);
        addParam(createParamCentered<AnimatekUI::FlatHugeKnob>(mm2px(Vec(15.f, 59.f)), module, UnitDistanceSeq::SEED_PARAM));
        addParam(createParamCentered<AnimatekUI::FlatHugeKnob>(mm2px(Vec(45.f, 59.f)), module, UnitDistanceSeq::NODES_PARAM));

        label("LOCK", 30.f, 61.2f, 12.f);
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(30.f, 69.5f)), module, UnitDistanceSeq::LOCK_PARAM));

        label("RADIUS", 15.f, 69.f);
        label("DENS", 45.f, 69.f);
        addParam(createParamCentered<AnimatekUI::FlatHugeKnob>(mm2px(Vec(15.f, 84.f)), module, UnitDistanceSeq::RADIUS_PARAM));
        addParam(createParamCentered<AnimatekUI::FlatHugeKnob>(mm2px(Vec(45.f, 84.f)), module, UnitDistanceSeq::DENSITY_PARAM));

        label("TOL", 7.f, 93.5f, 9.f);
        label("WALK", 18.5f, 93.5f, 12.f);
        label("RNG", 30.5f, 93.5f, 9.f);
        label("GLEN", 42.5f, 93.5f, 12.f);
        label("GDEN", 54.f, 93.5f, 12.f);
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(7.f, 101.5f)), module, UnitDistanceSeq::TOLERANCE_PARAM));
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(18.5f, 101.5f)), module, UnitDistanceSeq::WALK_PARAM));
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(30.5f, 101.5f)), module, UnitDistanceSeq::RANGE_PARAM));
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(42.5f, 101.5f)), module, UnitDistanceSeq::GATE_LENGTH_PARAM));
        addParam(createParamCentered<AnimatekUI::FlatTrimpot>(mm2px(Vec(54.f, 101.5f)), module, UnitDistanceSeq::GATE_DENSITY_PARAM));

        label("V/O", 7.f, 104.8f, 10.f);
        label("GATE", 19.f, 104.8f, 12.f);
        label("ACC", 31.f, 104.8f, 10.f);
        label("X", 43.f, 104.8f, 8.f);
        label("Y", 55.f, 104.8f, 8.f);
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(7.f, 113.f)), module, UnitDistanceSeq::VOCT_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(19.f, 113.f)), module, UnitDistanceSeq::GATE_OUTPUT));
        addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(25.f, 113.f)), module,
                                                            UnitDistanceSeq::GATE_LIGHT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(31.f, 113.f)), module, UnitDistanceSeq::ACCENT_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(43.f, 113.f)), module, UnitDistanceSeq::X_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(55.f, 113.f)), module, UnitDistanceSeq::Y_OUTPUT));
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* m = dynamic_cast<UnitDistanceSeq*>(module);

        menu->addChild(new ui::MenuSeparator());
        menu->addChild(createSubmenuItem(
            "Scale", m ? SCALES[m->scaleIndex].name : "",
            [m](ui::Menu* sub) {
                for (int i = 0; i < NUM_SCALES; ++i) {
                    sub->addChild(createCheckMenuItem(
                        SCALES[i].name, "",
                        [m, i]() { return m && m->scaleIndex == i; },
                        [m, i]() {
                            if (!m)
                                return;
                            m->scaleIndex = i;
                            m->updateAllVoiceOutputs();
                        }));
                }
            }));
        menu->addChild(createSubmenuItem(
            "Root", m ? ROOT_NAMES[m->rootSemitone] : "",
            [m](ui::Menu* sub) {
                for (int i = 0; i < 12; ++i) {
                    sub->addChild(createCheckMenuItem(
                        ROOT_NAMES[i], "",
                        [m, i]() { return m && m->rootSemitone == i; },
                        [m, i]() {
                            if (!m)
                                return;
                            m->rootSemitone = i;
                            m->updateAllVoiceOutputs();
                        }));
                }
            }));

        menu->addChild(createCheckMenuItem(
            "Sample & hold V/O on gates", "",
            [m]() { return m && m->sampleHoldPitch; },
            [m]() {
                if (m)
                    m->sampleHoldPitch ^= true;
            }));

        menu->addChild(createSubmenuItem(
            "Poly seed mode", m && m->polyUseVoiceSeeds ? "Per-voice seed" : "Shared seed",
            [m](ui::Menu* sub) {
                sub->addChild(createCheckMenuItem(
                    "Shared seed", "",
                    [m]() { return m && !m->polyUseVoiceSeeds; },
                    [m]() {
                        if (!m)
                            return;
                        m->polyUseVoiceSeeds = false;
                        m->rebuildGraph(true);
                        m->resetPolyVoices();
                    }));
                sub->addChild(createCheckMenuItem(
                    "Per-voice seed", "",
                    [m]() { return m && m->polyUseVoiceSeeds; },
                    [m]() {
                        if (!m)
                            return;
                        m->polyUseVoiceSeeds = true;
                        m->rebuildGraph(true);
                        m->resetPolyVoices();
                    }));
            }));

        menu->addChild(createSubmenuItem(
            "Poly voices", m ? string::f("%d", m->polyVoices) : "",
            [m](ui::Menu* sub) {
                const int options[] = {1, 2, 3, 4, 6, 8};
                for (int voices : options) {
                    sub->addChild(createCheckMenuItem(
                        string::f("%d voices", voices).c_str(), "",
                        [m, voices]() { return m && m->polyVoices == voices; },
                        [m, voices]() {
                            if (!m)
                                return;
                            m->polyVoices = voices;
                            m->resetPolyVoices();
                        }));
                }
            }));
    }
};

Model* modelUnitDistanceSeq = createModel<UnitDistanceSeq, UnitDistanceSeqWidget>("UnitDistanceSeq");
