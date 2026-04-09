#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace wolfsden
{

/** FX unit type per rack slot (TASK_008 list + Off). */
enum class FxUnitType : int
{
    Off = 0,
    Compressor,
    Limiter,
    Gate,
    Eq4Band,
    HighPass,
    LowPass,
    SoftClip,
    HardClip,
    BitCrusher,
    Waveshaper,
    Chorus,
    Flanger,
    Phaser,
    Vibrato,
    Tremolo,
    AutoPan,
    DelayStereo,
    DelayPingPong,
    Reverb,
    ReverbPlate,
    ReverbSpring,
    StereoWidth,
    MonoBass,
    NumTypes
};

static constexpr int kFxSlotsPerRack = 4;
static constexpr int kFxNumLayerRacks = 4;
static constexpr int kFxNumCommonSlots = 4;
static constexpr int kFxNumMasterSlots = 4;

class FxEngine
{
public:
    FxEngine();
    ~FxEngine();

    void prepare(double sampleRate, int maxBlock, juce::AudioProcessorValueTreeState& apvts);
    void reset() noexcept;

    void processBlock(juce::AudioBuffer<float>& layerBus,
                      juce::AudioBuffer<float>& outStereo,
                      juce::AudioProcessorValueTreeState& apvts,
                      float fxReverbMixAdd,
                      float fxDelayMixAdd,
                      float fxChorusMixAdd) noexcept;

    FxEngine(const FxEngine&) = delete;
    FxEngine& operator=(const FxEngine&) = delete;

private:
    struct SlotDSP;

    void bindPointers(juce::AudioProcessorValueTreeState& apvts);
    void cacheRoutingForBlock(juce::AudioProcessorValueTreeState& apvts) noexcept;

    void processRack(juce::AudioBuffer<float>& stereo,
                     int dspBaseIndex,
                     const std::array<int, kFxSlotsPerRack>& types,
                     const std::array<float, kFxSlotsPerRack>& mixes,
                     int numSamples) noexcept;

    juce::AudioProcessorValueTreeState* apvts = nullptr;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    bool prepared = false;

    std::array<std::atomic<float>*, (size_t)(kFxNumLayerRacks * kFxSlotsPerRack)> layerMixPtrs {};
    std::array<std::atomic<float>*, (size_t)kFxNumMasterSlots> masterMixPtrs {};
    std::atomic<float>* commonMixPtrs[4] {};

    std::array<int, (size_t)(kFxNumLayerRacks * kFxSlotsPerRack)> cachedLayerType {};
    std::array<int, (size_t)kFxNumCommonSlots> cachedCommonType {};
    std::array<int, (size_t)kFxNumMasterSlots> cachedMasterType {};

    /** Per FX slot (0–23) × 4 bands: parametric EQ gain in dB (`fx_sNN_eq0`…`eq3`). */
    std::array<std::atomic<float>*, 96> slotEqBandDb {};

    std::vector<std::unique_ptr<SlotDSP>> slotDSPs;
};

} // namespace wolfsden
