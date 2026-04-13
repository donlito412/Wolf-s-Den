#pragma once

#include "ModMatrix.h"
#include "synth/SynthDSP.h"
#include "synth/VoiceLayer.h"
#include "samples/WDSampleLibrary.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <mutex>

namespace wolfsden
{

class SynthEngine
{
public:
    SynthEngine();
    ~SynthEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock, juce::AudioProcessorValueTreeState& apvts);
    void reset() noexcept;

    /** layerBus: 8 channels (L0,R0,L1,R1,L2,R2,L3,R3) — dry per layer before FX. */
    void process(juce::AudioBuffer<float>& layerBus,
                 int numSamplesToProcess,
                 juce::MidiBuffer& midi,
                 juce::AudioProcessorValueTreeState& apvts,
                 juce::AudioPlayHead* playHead) noexcept;

    ModMatrix& getModMatrix() noexcept { return modMatrix; }
    const ModMatrix& getModMatrix() const noexcept { return modMatrix; }

    float getLastFxReverbMixAdd() const noexcept { return lastFxReverbMixAdd; }
    float getLastFxDelayMixAdd() const noexcept { return lastFxDelayMixAdd; }
    float getLastFxChorusMixAdd() const noexcept { return lastFxChorusMixAdd; }

    /** Active voices with any layer still sounding (same gate as voice stealing). */
    int countActiveVoices() const noexcept;

    /** Silence every voice immediately (must be called from the audio thread). */
    void allNotesOff() noexcept;

    /** Load a sample into every voice's layer at layerIndex (queued on a single worker thread). */
    void loadLayerSample (int layerIndex, int sampleId, const juce::String& filePath,
                          int rootNoteMidi, bool loopEnabled, bool oneShot,
                          float startFrac = 0.f, float endFrac = 1.f);

    WDSampleLibrary& getSampleLibrary() noexcept { return sampleLibrary; }

    SynthEngine(const SynthEngine&) = delete;
    SynthEngine& operator=(const SynthEngine&) = delete;

private:
    static constexpr int kNumVoices = 16;
    static constexpr int kNumLayers = 4;
    static constexpr int kWtSize = 2048;
    static constexpr int kGranSize = 16384;

    struct Voice
    {
        bool active = false;
        int midiNote = 60;
        float velocity = 0.8f;
        uint32_t age = 0;
        std::array<VoiceLayer, kNumLayers> layers {};
    };

    void bindParameterPointers(juce::AudioProcessorValueTreeState& apvts);
    void fillWavetable() noexcept;
    void fillWavetableB() noexcept;
    void fillGranularSource() noexcept;

    int voicePolyLimit() const noexcept;
    int findFreeVoice() noexcept;
    int findOldestVoice() noexcept;
    void startVoice(int voiceIndex, int note, float velocity, bool softSteal) noexcept;
    void noteOffKey(int note) noexcept;
    void deactivateFinishedVoices() noexcept;

    double sampleRate = 44100.0;
    int maxBlock = 512;

    std::array<double, kWtSize> wavetable {};
    std::array<double, kWtSize> wavetableB {};
    std::array<double, kGranSize> granularBuffer {};
    std::array<Voice, kNumVoices> voices {};
    wolfsden::dsp::Lfo globalLfo;
    ModMatrix modMatrix;

    double lastGlobalLfoForMod = 0.0;
    double globalLfoDelayRem = 0;
    double globalLfoFadeTotal = 0;
    double globalLfoFadeProg = 0;
    bool globalLfoInitialised = false;
    std::array<int, 128> keyDepth {};
    float lastFxReverbMixAdd = 0.f;
    float lastFxDelayMixAdd = 0.f;
    float lastFxChorusMixAdd = 0.f;

    ParamPointers ptrs {};
    bool pointersBound = false;

    WDSampleLibrary sampleLibrary;

    /** One in-flight layer load at a time — overlapping Thread::launch calls interleave loadNow and corrupt buffers. */
    std::mutex layerSampleLoadMutex;
};

} // namespace wolfsden
