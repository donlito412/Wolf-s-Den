#pragma once

#include "SynthDSP.h"
#include "../samples/WDSamplePlayer.h"

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

    juce::AudioParameterFloat* masterPitchF = nullptr;
    juce::AudioParameterFloat* masterVolF = nullptr;
    juce::AudioParameterFloat* filterAdsrAF = nullptr;
    juce::AudioParameterFloat* filterAdsrDF = nullptr;
    juce::AudioParameterFloat* filterAdsrSF = nullptr;
    juce::AudioParameterFloat* filterAdsrRF = nullptr;
    juce::AudioParameterFloat* lfoRateF = nullptr;
    juce::AudioParameterFloat* lfoDepthF = nullptr;

    std::atomic<float>* layerVol[4] {};
    std::atomic<float>* layerPan[4] {};
    std::atomic<float>* layerOsc[4] {};
    std::atomic<float>* layerCoarse[4] {};
    std::atomic<float>* layerFine[4] {};
    std::atomic<float>* layerCutoff[4] {};
    std::atomic<float>* layerRes[4] {};
    std::atomic<float>* layerFtype[4] {};
    std::atomic<float>* layerLfo2Rate[4] {};
    std::atomic<float>* layerLfo2Depth[4] {};
    std::atomic<float>* layerLfo2Shape[4] {};
    std::atomic<float>* layerFilterRoute[4] {};
    std::atomic<float>* layerUnisonVoices[4] {};
    std::atomic<float>* layerUnisonDetune[4] {};
    std::atomic<float>* layerUnisonSpread[4] {};
    std::atomic<float>* layerAA[4] {};
    std::atomic<float>* layerAD[4] {};
    std::atomic<float>* layerAS[4] {};
    std::atomic<float>* layerAR[4] {};

    juce::AudioParameterFloat* layerVolF[4] {};
    juce::AudioParameterFloat* layerPanF[4] {};
    juce::AudioParameterFloat* layerFineF[4] {};
    juce::AudioParameterFloat* layerCutoffF[4] {};
    juce::AudioParameterFloat* layerResF[4] {};
    juce::AudioParameterFloat* layerLfo2RateF[4] {};
    juce::AudioParameterFloat* layerLfo2DepthF[4] {};
    juce::AudioParameterFloat* layerUnisonDetuneF[4] {};
    juce::AudioParameterFloat* layerUnisonSpreadF[4] {};
    juce::AudioParameterFloat* layerAAF[4] {};
    juce::AudioParameterFloat* layerADF[4] {};
    juce::AudioParameterFloat* layerASF[4] {};
    juce::AudioParameterFloat* layerARF[4] {};
};

class VoiceLayer
{
public:
    void prepare(double sampleRate, const double* wavetable, int wavetableSize, const double* granularSource, int granularSize, int maxBlockSize = 512) noexcept;
    void reset() noexcept;
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff() noexcept;

    /** Load a sample into this layer's WDSamplePlayer (call from a single worker thread — see SynthEngine::loadLayerSample). */
    void loadSample (int sampleId, const juce::String& filePath,
                     int rootNoteMidi, bool loopEnabled, bool oneShot,
                     float startFrac = 0.f, float endFrac = 1.f)
    {
        samplePlayer.loadNow (sampleId, filePath, rootNoteMidi, loopEnabled, oneShot, startFrac, endFrac);
    }

    int currentSampleId() const noexcept { return samplePlayer.loadedId(); }

    /** Sum stereo into L/R (accumulate).
     *
     *  modMatrixCutSemi  — additive filter cutoff shift in semitones from mod matrix.
     *  modMatrixResAdd   — additive filter resonance (normalised) from mod matrix.
     *  modMatrixPitchSemi— additive pitch shift in semitones from mod matrix (vibrato).
     *  modMatrixAmpMul   — multiplicative amplitude scale from mod matrix (tremolo); 1.0 = unity.
     *  modMatrixPanAdd   — additive bipolar pan offset from mod matrix (auto-pan).
     */
    void renderAdd(double& outL,
                   double& outR,
                   int layerIndex,
                   int midiNote,
                   float velocity,
                   double globalLfoValue,
                   const ParamPointers& p,
                   float modMatrixCutSemi   = 0.f,
                   float modMatrixResAdd    = 0.f,
                   float modMatrixPitchSemi = 0.f,
                   float modMatrixAmpMul    = 1.f,
                   float modMatrixPanAdd    = 0.f) noexcept;

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
    wolfsden::dsp::Lfo lfoLayer2;

    wolfsden::dsp::Biquad f1, f2;

    std::array<double, 2048> combBuf {};
    int combWritePos = 0;
    int combDelaySamples = 0;
    double combFeedback = 0.0;

    std::array<double, 8> uniSin {};
    std::array<double, 8> uniSaw {};
    std::array<double, 8> uniSq {};
    std::array<double, 8> uniTri {};

    float cachedAmpA = -1, cachedAmpD = -1, cachedAmpS = -1, cachedAmpR = -1;
    float cachedFilA = -1, cachedFilD = -1, cachedFilS = -1, cachedFilR = -1;

    float lastAmpEnvOut = 0.f;
    float lastFiltEnvOut = 0.f;

    double smoothedVol = -1.0;
    double smoothedPan = 0.0;
    double smoothedCut = -1.0;

    WDSamplePlayer samplePlayer;

    void updateAdsrTargets(int layerIndex, const ParamPointers& p) noexcept;
    double readWavetable(double phase01) const noexcept;
    double processOscillator(int oscType, double hz, int layerIdx, const ParamPointers& p) noexcept;
    void updateFilterCoeffs(int filterType, double cutoffHz, double resonance, double modSemitones) noexcept;
    static void setBiquadIdentity(wolfsden::dsp::Biquad& b) noexcept;
    void spawnGrain() noexcept;
    double processGranular(double hz) noexcept;
};

} // namespace wolfsden
