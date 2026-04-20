#pragma once

#include "ModMatrix.h"
#include "synth/SynthDSP.h"
#include "synth/VoiceLayer.h"
#include "samples/WDSampleLibrary.h"
#include "samples/SampleKeymap.h"
#include "synth/WavetableBank.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <mutex>
#include <vector>

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

    /** Add a sample zone to a layer's keymap (message thread only). */
    void addSampleZone(int layerIndex, const SampleZone& zone);
    
    /** Clear all zones from a layer's keymap (message thread only). */
    void clearSampleKeymap(int layerIndex);

    WDSampleLibrary& getSampleLibrary() noexcept { return sampleLibrary; }
    
    /** Get a layer's keymap for UI purposes (message thread only). */
    const SampleKeymap& getLayerKeymap(int layerIndex) const;
    
    /** Get a layer's keymap for loading (non-const version). */
    SampleKeymap& getLayerKeymap(int layerIndex);

    void setLayerWavetable(int layerIndex, int tableIndexA, int tableIndexB);
    void loadLayerWavetableFromFile(int layerIndex, int slot, const juce::File& file);

    SynthEngine(const SynthEngine&) = delete;
    SynthEngine& operator=(const SynthEngine&) = delete;

private:
    static constexpr int kNumVoices = 16;
    static constexpr int kNumLayers = 4;
    static constexpr int kWtSize = 256; // Updated to match WavetableBank
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
    void fillGranularSource() noexcept;

    int voicePolyLimit() const noexcept;
    int findFreeVoice() noexcept;
    int findOldestVoice() noexcept;
    void startVoice(int voiceIndex, int note, float velocity, bool softSteal) noexcept;
    void noteOffKey(int note) noexcept;
    void deactivateFinishedVoices() noexcept;

    double sampleRate = 44100.0;
    int maxBlock = 512;

    std::array<std::vector<double>, 4> wtBufA;
    std::array<std::vector<double>, 4> wtBufB;

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
    std::array<int, 4> cachedWtIndexA {};
    std::array<int, 4> cachedWtIndexB {};
    float lastFxReverbMixAdd = 0.f;
    float lastFxDelayMixAdd = 0.f;
    float lastFxChorusMixAdd = 0.f;

    ParamPointers ptrs {};
    bool pointersBound = false;

    WDSampleLibrary sampleLibrary;

    /** Keymaps for each layer - stores sample zones for multi-sample mapping */
    std::array<SampleKeymap, 4> layerKeymaps;

    /** One in-flight layer load at a time — overlapping Thread::launch calls interleave loadNow and corrupt buffers. */
    std::mutex layerSampleLoadMutex;
};

} // namespace wolfsden