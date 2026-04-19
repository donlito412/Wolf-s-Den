#pragma once

#include "SynthDSP.h"
#include "../samples/WDSamplePlayer.h"
#include "../samples/SampleKeymap.h"

#include <array>
#include <atomic>

namespace juce
{
class AudioParameterFloat;
}

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
    std::atomic<float>* lfoSync = nullptr;
    std::atomic<float>* lfoSyncDiv = nullptr;
    std::atomic<float>* lfoDelay = nullptr;
    std::atomic<float>* lfoFade = nullptr;
    std::atomic<float>* lfoRetrigger = nullptr;

    std::atomic<float>* synthPolyphony = nullptr;
    std::atomic<float>* synthLegato = nullptr;
    std::atomic<float>* synthPortamento = nullptr;


    std::atomic<float>* layerVol[4] {};
    std::atomic<float>* layerPan[4] {};
    std::atomic<float>* layerOsc[4] {};
    std::atomic<float>* layerCoarse[4] {};
    std::atomic<float>* layerFine[4] {};
    std::atomic<float>* layerOctave[4] {};
    std::atomic<float>* layerCutoff[4] {};
    std::atomic<float>* layerRes[4] {};
    std::atomic<float>* layerFtype[4] {};
    std::atomic<float>* layerFilterDrive[4] {};
    std::atomic<float>* layerF2type[4] {};
    std::atomic<float>* layerF2cutoff[4] {};
    std::atomic<float>* layerF2res[4] {};
    std::atomic<float>* layerF2drive[4] {};
    std::atomic<float>* layerGranPos[4] {};
    std::atomic<float>* layerGranSize[4] {};
    std::atomic<float>* layerGranDensity[4] {};
    std::atomic<float>* layerGranScatter[4] {};
    std::atomic<float>* layerGranFreeze[4] {};
    std::atomic<float>* layerWtMorph[4] {};
    std::atomic<float>* layerKeytrack[4] {};
    std::atomic<float>* layerLfo2Rate[4] {};
    std::atomic<float>* layerLfo2Depth[4] {};
    std::atomic<float>* layerLfo2Shape[4] {};
    std::atomic<float>* layerLfo2Sync[4] {};
    std::atomic<float>* layerLfo2SyncDiv[4] {};
    std::atomic<float>* layerLfo2Delay[4] {};
    std::atomic<float>* layerLfo2Fade[4] {};
    std::atomic<float>* layerLfo2Retrigger[4] {};
    std::atomic<float>* layerFilterRoute[4] {};
    std::atomic<float>* layerUnisonVoices[4] {};
    std::atomic<float>* layerUnisonDetune[4] {};
    std::atomic<float>* layerUnisonSpread[4] {};
    std::atomic<float>* layerAA[4] {};
    std::atomic<float>* layerAD[4] {};
    std::atomic<float>* layerAS[4] {};
    std::atomic<float>* layerAR[4] {};

    std::atomic<float>* layerWtIndexA[4] {};
    std::atomic<float>* layerWtIndexB[4] {};
};

class VoiceLayer
{
public:
    void prepare(double sampleRate,
                 const double* wavetableA,
                 int wavetableSizeA,
                 const double* wavetableB,
                 int wavetableSizeB,
                 const double* granularSource,
                 int granularSize,
                 int maxBlockSize = 512) noexcept;
    void reset() noexcept;
    void noteOn(int midiNote, float velocity, int layerIndex, const ParamPointers& p) noexcept;
    void noteOn(int midiNote, float velocity, int layerIndex, const ParamPointers& p, const SampleKeymap& keymap) noexcept; // Task 017
    void noteOnLegato(int midiNote, float velocity, int layerIndex, const ParamPointers& p) noexcept;
    void noteOnLegato(int midiNote, float velocity, int layerIndex, const ParamPointers& p, const SampleKeymap& keymap) noexcept; // Task 017
    void noteOff() noexcept;

    void noteOnSteal(int midiNote,
                     float velocity,
                     int layerIndex,
                     const ParamPointers& p,
                     double ampEnvLevel,
                     double filtEnvLevel) noexcept;

    void loadSample(int sampleId,
                    const juce::String& filePath,
                    int rootNoteMidi,
                    bool loopEnabled,
                    bool oneShot,
                    float startFrac = 0.f,
                    float endFrac = 1.f)
    {
        samplePlayer.loadNow(sampleId, filePath, rootNoteMidi, loopEnabled, oneShot, startFrac, endFrac);
    }

    int currentSampleId() const noexcept { return samplePlayer.loadedId(); }

    // Multi-sample mapping methods for Task 017
    void selectSampleFromKeymap(int midiNote, float velocity, const SampleKeymap& keymap);
    const SampleZone* getCurrentSampleZone() const noexcept { return currentSampleZone; }

    void renderAdd(double& outL,
                   double& outR,
                   int layerIndex,
                   int midiNote,
                   float velocity,
                   double globalLfoValue,
                   double hostBpm,
                   const ParamPointers& p,
                   float modMatrixCutSemi = 0.f,
                   float modMatrixResAdd = 0.f,
                   double modMatrixPitchFactor = 1.0,
                   float modMatrixAmpMul = 1.f,
                   float modMatrixPanAdd = 0.f) noexcept;

    bool isActive() const noexcept { return ampAdsr.stage != wolfsden::dsp::Adsr::Stage::idle; }

    float getLastAmpEnv() const noexcept { return lastAmpEnvOut; }
    float getLastFiltEnv() const noexcept { return lastFiltEnvOut; }
    double getAmpEnvLevel() const noexcept { return ampAdsr.level; }
    double getFiltEnvLevel() const noexcept { return filtAdsr.level; }

private:
    double sr = 44100;
    const double* wtData = nullptr;
    const double* wtBData = nullptr;
    int wtLen = 2048;
    const double* granSource = nullptr;
    int granLen = 8192;

    int currentNote = 60;
    float currentVel = 0.8f;
    double glideHz = 440.0;
    double glideTargetHz = 440.0;

    double phaseSin = 0, phaseSaw = 0, phaseSq = 0, triIntegrator = 0;
    double phaseWt = 0, phaseSmpl = 0;
    double phaseFmCar = 0, phaseFmMod = 0;
    uint32_t noiseOscState = 0x12345678u;

    struct Grain
    {
        bool active = false;
        double pos = 0;
        double speed = 1;
        double winPos = 0;
        double winInc = 0;
        double amp = 1;
        double pan = 0;
    };
    static constexpr int kMaxGrains = 16;
    std::array<Grain, kMaxGrains> grains {};
    int grainSpawnCounter = 0;
    double granPlayhead = 0;
    double frozenPlayhead = 0;
    bool hadFreezeOn = false;

    wolfsden::dsp::Adsr ampAdsr;
    wolfsden::dsp::Adsr filtAdsr;
    wolfsden::dsp::Lfo lfoLayer;
    wolfsden::dsp::Lfo lfoLayer2;

    wolfsden::dsp::Biquad f1, f1s, f2, f2s;

    std::array<double, 2048> combBuf1 {};
    std::array<double, 2048> combBuf2 {};
    int combWritePos1 = 0;
    int combWritePos2 = 0;
    int combDelaySamples1 = 0;
    int combDelaySamples2 = 0;
    double combFeedback1 = 0.0;
    double combFeedback2 = 0.0;

    std::array<double, 8> uniSin {};
    std::array<double, 8> uniSaw {};
    std::array<double, 8> uniSq {};
    std::array<double, 8> uniTri {};

    float cachedAmpA = -1, cachedAmpD = -1, cachedAmpS = -1, cachedAmpR = -1;
    float cachedFilA = -1, cachedFilD = -1, cachedFilS = -1, cachedFilR = -1;

    // Sub-rate filter coefficient update: recalculate every N samples.
    // sin/cos in setLowPass/setHighPass are expensive — recalculating
    // every sample multiplies cost by 3-4 voices × 4 layers = 12-16x.
    static constexpr int kFilterUpdateInterval = 4;
    int filterUpdateTick_ = 0;

    // RC1: pre-baked fine-tune factor — std::pow(2, fineCents/1200) computed once per noteOn/change
    double cachedFineTuneFactor = 1.0;
    double lastFineCents = -99999.0;

    // RC2: pre-baked unison pitch factors — recomputed only when nv or detune changes
    std::array<double, 8> uniPitchFactor {};
    int    lastNv        = 0;
    double lastUniDetune = -1.0;

    // RC3: pre-baked keytrack factor — std::pow(2, ktSemi/12) computed once per noteOn/param change
    double cachedKtFactor = 1.0;
    float  lastKt         = -99999.f;

    float lastAmpEnvOut = 0.f;
    float lastFiltEnvOut = 0.f;

    double smoothedVol = -1.0;
    double smoothedPan = 0.0;
    double smoothedCut1 = -1.0;
    double smoothedCut2 = -1.0;

    double lfo2DelayRem = 0;
    double lfo2FadeTotal = 0;
    double lfo2FadeProg = 0;

    WDSamplePlayer samplePlayer;
    
    // Task 017: Multi-sample mapping support
    const SampleZone* currentSampleZone = nullptr;

    void updateAdsrTargets(int layerIndex, const ParamPointers& p) noexcept;
    double readWavetableMorph(double phase01, float morph01) const noexcept;
    double processOscillator(int oscType, double hz, int layerIdx, const ParamPointers& p) noexcept;
    void configureFilterBank(wolfsden::dsp::Biquad& f,
                             wolfsden::dsp::Biquad& fs,
                             int filterType,
                             double cutoffHz,
                             double resonance,
                             double modSemitones,
                             int& combDelay,
                             double& combFb,
                             int& combWritePos,
                             std::array<double, 2048>& combBuf) noexcept;
    static void setBiquadIdentity(wolfsden::dsp::Biquad& b) noexcept;
    void spawnGrain(double hz, int layerIdx, const ParamPointers& p, bool useSampleSource) noexcept;
    double processGranular(double hz, int layerIdx, const ParamPointers& p, bool useSampleSource) noexcept;
    static double beatsForSyncDivIndex(int divIdx) noexcept;
    static bool readBoolNorm(std::atomic<float>* ap, bool defV) noexcept;
};

} // namespace wolfsden