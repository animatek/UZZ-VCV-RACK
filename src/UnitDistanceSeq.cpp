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
static constexpr std::array<int, 7> MINOR_SCALE = {0, 2, 3, 5, 7, 8, 10};

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

    enum LightIds {
        CLOCK_LIGHT,
        GATE_LIGHT,
        ENUMS(ACTIVITY_LIGHTS, 4),
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

    void generateVoiceGraph(int voice, int seed, float radius, float tolerance) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);

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
            voiceGraphNodes[voice][i].x = std::cos(theta1) + spread * std::cos(theta2);
            voiceGraphNodes[voice][i].y = std::sin(theta1) + spread * std::sin(theta2);
            minX = std::min(minX, voiceGraphNodes[voice][i].x);
            minY = std::min(minY, voiceGraphNodes[voice][i].y);
            maxX = std::max(maxX, voiceGraphNodes[voice][i].x);
            maxY = std::max(maxY, voiceGraphNodes[voice][i].y);
            voiceNeighbors[voice][i] = 0u;
            voiceDegrees[voice][i] = 0;
        }

        float xSpan = std::max(0.0001f, maxX - minX);
        float ySpan = std::max(0.0001f, maxY - minY);
        for (int i = 0; i < nodeCount; ++i) {
            voiceGraphNodes[voice][i].nx = clamp((voiceGraphNodes[voice][i].x - minX) / xSpan, 0.f, 1.f);
            voiceGraphNodes[voice][i].ny = clamp((voiceGraphNodes[voice][i].y - minY) / ySpan, 0.f, 1.f);
        }

        int voiceEdgeCount = 0;
        voiceMaxDegrees[voice] = 0;
        for (int i = 0; i < nodeCount; ++i) {
            for (int j = i + 1; j < nodeCount; ++j) {
                float dx = voiceGraphNodes[voice][i].x - voiceGraphNodes[voice][j].x;
                float dy = voiceGraphNodes[voice][i].y - voiceGraphNodes[voice][j].y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (std::abs(d - radius) < tolerance) {
                    voiceNeighbors[voice][i] |= (1ull << j);
                    voiceNeighbors[voice][j] |= (1ull << i);
                    voiceDegrees[voice][i]++;
                    voiceDegrees[voice][j]++;
                    voiceEdgeCount++;
                }
            }
        }

        for (int i = 0; i < nodeCount; ++i)
            voiceMaxDegrees[voice] = std::max(voiceMaxDegrees[voice], voiceDegrees[voice][i]);
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
        for (int v = 0; v < MAX_POLY_VOICES; ++v)
            voiceNodes[v] = clamp(voiceNodes[v], 0, nodeCount - 1);
        for (int& node : walkHistory) {
            if (node >= nodeCount)
                node = -1;
        }

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
            graphNodes[i].x = std::cos(theta1) + spread * std::cos(theta2);
            graphNodes[i].y = std::sin(theta1) + spread * std::sin(theta2);
            minX = std::min(minX, graphNodes[i].x);
            minY = std::min(minY, graphNodes[i].y);
            maxX = std::max(maxX, graphNodes[i].x);
            maxY = std::max(maxY, graphNodes[i].y);
            neighbors[i] = 0u;
            degrees[i] = 0;
        }

        float xSpan = std::max(0.0001f, maxX - minX);
        float ySpan = std::max(0.0001f, maxY - minY);
        for (int i = 0; i < nodeCount; ++i) {
            graphNodes[i].nx = clamp((graphNodes[i].x - minX) / xSpan, 0.f, 1.f);
            graphNodes[i].ny = clamp((graphNodes[i].y - minY) / ySpan, 0.f, 1.f);
        }

        edgeCount = 0;
        maxDegree = 0;
        for (int i = 0; i < nodeCount; ++i) {
            for (int j = i + 1; j < nodeCount; ++j) {
                float dx = graphNodes[i].x - graphNodes[j].x;
                float dy = graphNodes[i].y - graphNodes[j].y;
                float d = std::sqrt(dx * dx + dy * dy);
                if (std::abs(d - radius) < tolerance) {
                    neighbors[i] |= (1ull << j);
                    neighbors[j] |= (1ull << i);
                    degrees[i]++;
                    degrees[j]++;
                    edgeCount++;
                }
            }
        }

        for (int i = 0; i < nodeCount; ++i)
            maxDegree = std::max(maxDegree, degrees[i]);
        graphHasEdges = edgeCount > 0;
        copyMainGraphToVoice(0);
        for (int v = 1; v < MAX_POLY_VOICES; ++v) {
            if (polyUseVoiceSeeds)
                generateVoiceGraph(v, seed + v * 137 + nodeCount * 17, radius, tolerance);
            else
                copyMainGraphToVoice(v);
        }
        baseWalkState = hashU32((uint32_t)(seed * 73856093u) ^ (uint32_t)radiusKey ^ ((uint32_t)toleranceKey << 11) ^ ((uint32_t)densityKey << 3));
        walkState = baseWalkState;
        for (int v = 0; v < MAX_POLY_VOICES; ++v) {
            voiceWalkStates[v] = hashU32(baseWalkState ^ (uint32_t)(v * 0x9e3779b9u));
            voiceNodeOffsets[v] = (int)(hashU32(baseWalkState ^ (uint32_t)(nodeCount * (v + 1)) ^
                                                (uint32_t)(v * 0x85ebca6bu)) % (uint32_t)std::max(1, nodeCount));
        }
        gateStep = 0;
        clearWalkHistory();
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
        clearWalkHistory();
        updateAllVoiceOutputs();
    }

    void updateVoiceOutputs(int voice) {
        voice = clamp(voice, 0, MAX_POLY_VOICES - 1);
        int nodeIndex = clamp(voiceNodes[voice], 0, nodeCount - 1);
        const GraphNode& node = voiceGraphNodes[voice][nodeIndex];
        int range = clamp((int)std::round(params[RANGE_PARAM].getValue()), 1, 4);
        int scaleSteps = (int)MINOR_SCALE.size() * range;
        int scaleIndex = clamp((int)std::floor(node.nx * (float)scaleSteps), 0, scaleSteps - 1);
        int octave = scaleIndex / (int)MINOR_SCALE.size();
        int degree = MINOR_SCALE[scaleIndex % (int)MINOR_SCALE.size()];
        voicePitchVolts[voice] = (float)octave + (float)degree / 12.f;
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
        int scaleSteps = (int)MINOR_SCALE.size() * range;
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
        if (voice <= 0)
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

    void onClock() {
        std::array<bool, MAX_POLY_VOICES> fireGates = {};
        int nextNode = chooseNextNode();
        pushWalkHistory(currentNode);
        voiceNodes[0] = nextNode;
        currentNode = voiceNodes[0];
        for (int v = 1; v < polyVoices; ++v)
            voiceNodes[v] = chooseNextNodeForVoice(v);
        updateAllVoiceOutputs();
        gateStep++;
        for (int v = 0; v < polyVoices; ++v) {
            fireGates[v] = shouldFireGate(voiceNodes[v], v);
            if (fireGates[v]) {
                float gateLen = clamp(params[GATE_LENGTH_PARAM].getValue(), 0.05f, 0.95f) * clockPeriod;
                gatePulses[v].trigger(clamp(gateLen, 0.001f, 2.f));
            }
        }
        recordStep(fireGates);
        clockPulse.trigger(0.03f);
    }

    void onHybridClock(float amount) {
        if (lockedLoopLength <= 0)
            freezeLockLoop(requestedLockLength());

        int pos = lockedLoopPos;
        lockedLoopPos = (lockedLoopPos + 1) % std::max(1, lockedLoopLength);

        if (shouldUseLockedStep(pos, amount)) {
            LockedStep step = lockedLoop[pos];
            int lockedVoices = clamp(step.voiceCount, 1, MAX_POLY_VOICES);
            for (int v = 0; v < polyVoices; ++v)
                voiceNodes[v] = clamp(step.nodes[v % lockedVoices], 0, nodeCount - 1);
            currentNode = voiceNodes[0];
            updateAllVoiceOutputs();
            gateStep++;
            for (int v = 0; v < polyVoices; ++v) {
                if (step.gates[v % lockedVoices]) {
                    float gateLen = clamp(params[GATE_LENGTH_PARAM].getValue(), 0.05f, 0.95f) * clockPeriod;
                    gatePulses[v].trigger(clamp(gateLen, 0.001f, 2.f));
                }
            }
        }
        else {
            std::array<bool, MAX_POLY_VOICES> fireGates = {};
            int nextNode = chooseNextNode();
            pushWalkHistory(currentNode);
            voiceNodes[0] = nextNode;
            currentNode = voiceNodes[0];
            for (int v = 1; v < polyVoices; ++v)
                voiceNodes[v] = chooseNextNodeForVoice(v);
            updateAllVoiceOutputs();
            gateStep++;
            for (int v = 0; v < polyVoices; ++v) {
                fireGates[v] = shouldFireGate(voiceNodes[v], v);
                if (fireGates[v]) {
                    float gateLen = clamp(params[GATE_LENGTH_PARAM].getValue(), 0.05f, 0.95f) * clockPeriod;
                    gatePulses[v].trigger(clamp(gateLen, 0.001f, 2.f));
                }
            }
            recordStep(fireGates);
        }
        clockPulse.trigger(0.03f);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "polyVoices", json_integer(polyVoices));
        json_object_set_new(root, "polyUseVoiceSeeds",
                            json_boolean(polyUseVoiceSeeds));
        return root;
    }

    void dataFromJson(json_t* root) override {
        if (!root)
            return;
        if (json_t* j = json_object_get(root, "polyVoices"))
            polyVoices = clamp((int)json_integer_value(j), 1, MAX_POLY_VOICES);
        if (json_t* j = json_object_get(root, "polyUseVoiceSeeds"))
            polyUseVoiceSeeds = json_is_true(j);
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
        const GraphNode& node = graphNodes[currentNode];

        outputs[VOCT_OUTPUT].setChannels(polyVoices);
        outputs[GATE_OUTPUT].setChannels(polyVoices);
        outputs[ACCENT_OUTPUT].setChannels(polyVoices);
        outputs[X_OUTPUT].setChannels(polyVoices);
        outputs[Y_OUTPUT].setChannels(polyVoices);
        for (int v = 0; v < polyVoices; ++v) {
            outputs[VOCT_OUTPUT].setVoltage(voicePitchVolts[v], v);
            outputs[GATE_OUTPUT].setVoltage(gateHigh[v] ? 10.f : 0.f, v);
            outputs[ACCENT_OUTPUT].setVoltage(voiceAccentVolts[v], v);
            outputs[X_OUTPUT].setVoltage(voiceXVolts[v], v);
            outputs[Y_OUTPUT].setVoltage(voiceYVolts[v], v);
        }

        lights[CLOCK_LIGHT].setBrightnessSmooth(clockHigh ? 1.f : 0.f, args.sampleTime);
        lights[GATE_LIGHT].setBrightnessSmooth(gateHigh[0] ? 1.f : 0.f, args.sampleTime);
        lights[ACTIVITY_LIGHTS + 0].setBrightnessSmooth(node.nx > 0.50f ? 1.f : 0.05f, args.sampleTime);
        lights[ACTIVITY_LIGHTS + 1].setBrightnessSmooth(node.ny > 0.50f ? 1.f : 0.05f, args.sampleTime);
        lights[ACTIVITY_LIGHTS + 2].setBrightnessSmooth(graphHasEdges ? 1.f : 0.05f, args.sampleTime);
        lights[ACTIVITY_LIGHTS + 3].setBrightnessSmooth(edgeCount > nodeCount ? 1.f : 0.05f, args.sampleTime);
    }
};

struct UnitDistanceGraphDisplay : TransparentWidget {
    UnitDistanceSeq* module = nullptr;

    explicit UnitDistanceGraphDisplay(UnitDistanceSeq* module) : module(module) {}

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
        nvgFillColor(args.vg, nvgRGBA(5, 8, 12, 210));
        nvgFill(args.vg);

        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 3.f);
        nvgStrokeColor(args.vg, nvgRGBA(93, 183, 255, 90));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);

        if (!module)
            return;

        float pad = 4.f;
        auto px = [&](float x) { return pad + x * (box.size.x - 2.f * pad); };
        auto py = [&](float y) { return pad + (1.f - y) * (box.size.y - 2.f * pad); };

        for (int i = 0; i < module->nodeCount; ++i) {
            uint64_t mask = module->neighbors[i];
            for (int j = i + 1; j < module->nodeCount; ++j) {
                if (!(mask & (1ull << j)))
                    continue;
                bool currentEdge = i == module->currentNode || j == module->currentNode;
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, px(module->graphNodes[i].nx), py(module->graphNodes[i].ny));
                nvgLineTo(args.vg, px(module->graphNodes[j].nx), py(module->graphNodes[j].ny));
                nvgStrokeColor(args.vg, currentEdge ? nvgRGBA(93, 183, 255, 135)
                                                    : nvgRGBA(130, 150, 170, 45));
                nvgStrokeWidth(args.vg, currentEdge ? 1.1f : 0.45f);
                nvgStroke(args.vg);
            }
        }

        for (int i = 0; i < module->nodeCount; ++i) {
            bool current = i == module->currentNode;
            bool polyActive = false;
            for (int v = 1; v < module->polyVoices; ++v) {
                if (module->voiceNodes[v] == i) {
                    polyActive = true;
                    break;
                }
            }
            float degreeNorm = module->maxDegree > 0 ? clamp((float)module->degrees[i] / (float)module->maxDegree, 0.f, 1.f) : 0.f;
            float gateDensity = clamp(module->params[UnitDistanceSeq::GATE_DENSITY_PARAM].getValue(), 0.15f, 1.f);
            float gateLength = clamp(module->params[UnitDistanceSeq::GATE_LENGTH_PARAM].getValue(), 0.05f, 0.95f);
            float gateChance = clamp(gateDensity * (0.45f + degreeNorm * 0.55f), 0.f, 1.f);
            float r = current ? 2.0f + gateLength * 2.4f
                              : (polyActive ? 1.8f + gateLength * 1.3f : 1.1f + degreeNorm * 1.1f);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, px(module->graphNodes[i].nx), py(module->graphNodes[i].ny), r);
            if (current) {
                uint8_t red = (uint8_t)(205 + gateChance * 50.f);
                uint8_t green = (uint8_t)(55 + gateChance * 45.f);
                uint8_t blue = (uint8_t)(190 + gateChance * 55.f);
                nvgFillColor(args.vg, nvgRGBA(red, green, blue, 245));
            }
            else if (polyActive) {
                nvgFillColor(args.vg, nvgRGBA(190, 70, 235, 210));
            }
            else {
                uint8_t red = (uint8_t)(70 + gateChance * 120.f);
                uint8_t green = (uint8_t)(135 + gateChance * 70.f);
                uint8_t blue = (uint8_t)(175 + (1.f - gateChance) * 60.f);
                uint8_t alpha = (uint8_t)(65 + gateChance * 165.f);
                nvgFillColor(args.vg, nvgRGBA(red, green, blue, alpha));
            }
            nvgFill(args.vg);
        }
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

        auto* display = new UnitDistanceGraphDisplay(module);
        display->box.pos = mm2px(Vec(2.8f, 17.f));
        display->box.size = mm2px(Vec(55.3f, 27.f));
        addChild(display);

        label("SEED", 15.f, 44.8f);
        label("NODES", 45.f, 44.8f);
        addParam(createParamCentered<Davies1900hLargeBlackKnob>(mm2px(Vec(15.f, 59.f)), module, UnitDistanceSeq::SEED_PARAM));
        addParam(createParamCentered<Davies1900hLargeBlackKnob>(mm2px(Vec(45.f, 59.f)), module, UnitDistanceSeq::NODES_PARAM));

        label("LOCK", 30.f, 61.2f, 12.f);
        addParam(createParamCentered<Trimpot>(mm2px(Vec(30.f, 69.5f)), module, UnitDistanceSeq::LOCK_PARAM));

        label("RADIUS", 15.f, 69.f);
        label("DENS", 45.f, 69.f);
        addParam(createParamCentered<Davies1900hLargeBlackKnob>(mm2px(Vec(15.f, 84.f)), module, UnitDistanceSeq::RADIUS_PARAM));
        addParam(createParamCentered<Davies1900hLargeBlackKnob>(mm2px(Vec(45.f, 84.f)), module, UnitDistanceSeq::DENSITY_PARAM));

        label("TOL", 7.f, 93.5f, 9.f);
        label("WALK", 18.5f, 93.5f, 12.f);
        label("RNG", 30.5f, 93.5f, 9.f);
        label("GLEN", 42.5f, 93.5f, 12.f);
        label("GDEN", 54.f, 93.5f, 12.f);
        addParam(createParamCentered<Trimpot>(mm2px(Vec(7.f, 101.5f)), module, UnitDistanceSeq::TOLERANCE_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(18.5f, 101.5f)), module, UnitDistanceSeq::WALK_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(30.5f, 101.5f)), module, UnitDistanceSeq::RANGE_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(42.5f, 101.5f)), module, UnitDistanceSeq::GATE_LENGTH_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(54.f, 101.5f)), module, UnitDistanceSeq::GATE_DENSITY_PARAM));

        label("V/O", 7.f, 104.8f, 10.f);
        label("GATE", 19.f, 104.8f, 12.f);
        label("ACC", 31.f, 104.8f, 10.f);
        label("X", 43.f, 104.8f, 8.f);
        label("Y", 55.f, 104.8f, 8.f);
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(7.f, 113.f)), module, UnitDistanceSeq::VOCT_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(19.f, 113.f)), module, UnitDistanceSeq::GATE_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(31.f, 113.f)), module, UnitDistanceSeq::ACCENT_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(43.f, 113.f)), module, UnitDistanceSeq::X_OUTPUT));
        addOutput(createOutputCentered<AnimatekUI::TekOutputPort>(mm2px(Vec(55.f, 113.f)), module, UnitDistanceSeq::Y_OUTPUT));
    }

    void appendContextMenu(ui::Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* m = dynamic_cast<UnitDistanceSeq*>(module);

        menu->addChild(new ui::MenuSeparator());
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
