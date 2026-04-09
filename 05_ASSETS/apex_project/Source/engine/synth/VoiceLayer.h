#pragma once

#include "SynthDSP.h"

#include <array>
#include <atomic>

namespace wolfsden
{

struct ParamPointers
{
    std::atomic<float>* masterPitch = nullptr;
    std::atomic<float>* masterVol = nullptr;
    std::atomic<float>* filterAdsrA = nullptr;
    std::atomic<float>* filterAdsrD = nullptr;
    std::atomic<float>* filterAdsrS = nullptr;
    std::atomic<float>* filterAdsrR = nullptr;
    std::atomic<float>* lfoRate = nullptr;
    std::atomic<float>* lfoDepth = nullptr;
    std::atomic<float>* lfoShape = nullptr;

    std::atomic<float>* layerVol[4] {};
    std::atomic<float>* layerPan[4] {};
    std::atomic<float>* layerOsc[4] {};
    std::atomic<float>* layerCoarse[4] {};
    std::atomic<float>* layerFine[4] {};
    std::atomic<float>* layerCutoff[4] {};
    std::atomic<float>* layerRes[4] {};
    std::atomic<float>* layerFtype[4] {};
    std::atomic<float>* layerAA[4] {};
    std::atomic<float>* layerAD[4] {};
    std::atomic<float>* layerAS[4] {};
    std::atomic<float>* layerAR[4] {};
};

class VoiceLayer
{
public:
    void prepare(double sampleRate, const double* wavetable, int wavetableSize, const double* granularSource, int granularSize) noexcept;
    void reset() noexcept;
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff() noexcept;

    /** Sum stereo into L/R (accumulate). */
    void renderAdd(double& outL,
                   double& outR,
                   int layerIndex,
                   int midiNote,
                   float velocity,
                   double globalLfoValue,
                   const ParamPointers& p,
                   float modMatrixCutSemi = 0.f,
                   float modMatrixResAdd = 0.f) noexcept;

    bool isActive() const noexcept { return ampAdsr.stage != wolfsden::dsp::Adsr::Stage::idle; }

    float getLastAmpEnv() const noexcept { return lastAmpEnvOut; }
    float getLastFiltEnv() const noexcept { return lastFiltEnvOut; }

private:
    double sr = 44100;
    const double* wtData = nullptr;
    int wtLen = 2048;
    const double* granSource = nullptr;
    int granLen = 8192;

    int currentNote = 60;
    float currentVel = 0.8f;

    double phaseSin = 0, phaseSaw = 0, phaseSq = 0, triIntegrator = 0;
    double phaseWt = 0, phaseSmpl = 0;
    double phaseFmCar = 0, phaseFmMod = 0;

    struct Grain
    {
        bool active = false;
        double pos = 0;
        double speed = 1;
        double winPos = 0;
        double winInc = 0;
        double amp = 1;
    };
    static constexpr int kMaxGrains = 16;
    std::array<Grain, kMaxGrains> grains {};
    int grainSpawnCounter = 0;

    wolfsden::dsp::Adsr ampAdsr;
    wolfsden::dsp::Adsr filtAdsr;
    wolfsden::dsp::Lfo lfoLayer;

    wolfsden::dsp::Biquad f1, f2;

    float cachedAmpA = -1, cachedAmpD = -1, cachedAmpS = -1, cachedAmpR = -1;
    float cachedFilA = -1, cachedFilD = -1, cachedFilS = -1, cachedFilR = -1;

    float lastAmpEnvOut = 0.f;
    float lastFiltEnvOut = 0.f;

    void updateAdsrTargets(int layerIndex, const ParamPointers& p) noexcept;
    double readWavetable(double phase01) const noexcept;
    double processOscillator(int oscType, double hz, int layerIdx, const ParamPointers& p) noexcept;
    void updateFilterCoeffs(int filterType, double cutoffHz, double resonance, double modSemitones) noexcept;
    void spawnGrain() noexcept;
    double processGranular(double hz) noexcept;
    static float readP(std::atomic<float>* ap, float defV = 0) noexcept;
};

} // namespace wolfsden
