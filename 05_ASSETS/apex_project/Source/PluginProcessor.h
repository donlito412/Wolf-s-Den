#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/FxEngine.h"
#include "engine/MidiPipeline.h"
#include "engine/ModMatrix.h"
#include "engine/SynthEngine.h"
#include "engine/TheoryEngine.h"
#include "parameters/WolfsDenParameterLayout.h"

struct UiToDspMessage
{
    uint32_t paramIdHash = 0;
    float value = 0.f;
};

class WolfsDenAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    WolfsDenAudioProcessor();
    ~WolfsDenAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    wolfsden::ModMatrix& getModMatrix() noexcept { return synthEngine.getModMatrix(); }

    void setPresetDisplayName(juce::String name);
    juce::String getPresetDisplayName() const;

    void setLastEditorBounds(int width, int height);

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void drainUiToDspFifo() noexcept;
    void registerApvtsListeners();
    void unregisterApvtsListeners();
    void syncTheoryParamsFromApvts();

    static constexpr int kUiFifoSize = 512;
    juce::AbstractFifo uiToDspFifo { kUiFifoSize }; // lock-free SPSC queue (UI / message → audio)
    std::array<UiToDspMessage, (size_t)kUiFifoSize> uiToDspBuffer {};

    juce::AudioProcessorValueTreeState apvts;

    wolfsden::MidiPipeline midiPipeline;
    wolfsden::TheoryEngine theoryEngine;
    wolfsden::SynthEngine synthEngine;
    wolfsden::FxEngine fxEngine;

    mutable juce::CriticalSection customStateLock;
    juce::String presetDisplayName { "Init" };
    juce::String chordProgressionBlob;

    std::atomic<int> editorWidth { 480 };
    std::atomic<int> editorHeight { 320 };

    /** 8 ch: per-layer stereo before FxEngine (L0,R0,…,L3,R3). */
    juce::AudioBuffer<float> synthLayerBus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WolfsDenAudioProcessor)
};
