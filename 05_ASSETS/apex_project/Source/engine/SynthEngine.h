#pragma once

#include "ModMatrix.h"
#include "synth/SynthDSP.h"
#include "synth/VoiceLayer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

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
                 juce::MidiBuffer& midi,
                 juce::AudioProcessorValueTreeState& apvts) noexcept;

    ModMatrix& getModMatrix() noexcept { return modMatrix; }
    const ModMatrix& getModMatrix() const noexcept { return modMatrix; }

    float getLastFxReverbMixAdd() const noexcept { return lastFxReverbMixAdd; }
    float getLastFxDelayMixAdd() const noexcept { return lastFxDelayMixAdd; }
    float getLastFxChorusMixAdd() const noexcept { return lastFxChorusMixAdd; }

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
    void fillGranularSource() noexcept;

    int findFreeVoice() noexcept;
    int findOldestVoice() noexcept;
    void startVoice(int voiceIndex, int note, float velocity) noexcept;
    void noteOffKey(int note) noexcept;
    void deactivateFinishedVoices() noexcept;

    double sampleRate = 44100.0;
    int maxBlock = 512;

    std::array<double, kWtSize> wavetable {};
    std::array<double, kGranSize> granularBuffer {};
    std::array<Voice, kNumVoices> voices {};
    wolfsden::dsp::Lfo globalLfo;
    ModMatrix modMatrix;

    double lastGlobalLfoForMod = 0.0;
    float lastFxReverbMixAdd = 0.f;
    float lastFxDelayMixAdd = 0.f;
    float lastFxChorusMixAdd = 0.f;

    ParamPointers ptrs {};
    bool pointersBound = false;
};

} // namespace wolfsden
