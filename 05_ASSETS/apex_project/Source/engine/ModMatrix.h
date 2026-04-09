#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_data_structures/juce_data_structures.h>

namespace wolfsden
{

/** 32-slot modulation matrix — slot data written from UI; evaluate() runs on audio thread. */
class ModMatrix
{
public:
    static constexpr int kSlots = 32;

    enum Source : int
    {
        Off = -1,
        GlobalLFO = 0,
        FilterEnv,
        AmpEnv,
        ModWheel,
        Aftertouch,
        Velocity,
        PitchBend,
        XY_X,
        XY_Y,
        Random,
        NumSources
    };

    enum Target : int
    {
        None = -1,
        Layer0_FilterCutoffSemi,
        Layer0_FilterRes,
        Layer1_FilterCutoffSemi,
        Layer1_FilterRes,
        Layer2_FilterCutoffSemi,
        Layer2_FilterRes,
        Layer3_FilterCutoffSemi,
        Layer3_FilterRes,
        LFO_RateMul,
        LFO_DepthAdd,
        MasterVolumeMul,
        Fx_ReverbMixAdd,
        Fx_DelayMixAdd,
        Fx_ChorusMixAdd,
        // Per-layer pitch modulation (semitones, ±48 range) — enables vibrato per layer
        Layer0_PitchSemi,
        Layer1_PitchSemi,
        Layer2_PitchSemi,
        Layer3_PitchSemi,
        // Per-layer amplitude modulation (multiplicative, centred at 1.0) — enables tremolo
        Layer0_AmpMul,
        Layer1_AmpMul,
        Layer2_AmpMul,
        Layer3_AmpMul,
        // Per-layer stereo pan modulation (bipolar additive, ±1.0) — enables auto-pan per layer
        Layer0_PanAdd,
        Layer1_PanAdd,
        Layer2_PanAdd,
        Layer3_PanAdd,
        NumTargets
    };

    ModMatrix();

    void reset() noexcept;

    /** UI / message thread */
    void setSlot(int slotIndex,
                 Source src,
                 Target tgt,
                 float amount,
                 bool muted,
                 bool inverted,
                 float smooth01,
                 int layerScope) noexcept;

    void setXy(float x, float y) noexcept;
    float getXyX() const noexcept;
    float getXyY() const noexcept;

    void setModWheel(float n01) noexcept;
    void setAftertouch(float n01) noexcept;
    void setPitchBend(float bipolar) noexcept;
    float getPitchBendValue() const noexcept;

    /** Preset / state (message thread). */
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& v) noexcept;

    /**
     * Per-sample accumulation.
     * layerCutSemi[4]   — additive filter cutoff mod in semitones per layer.
     * layerResAdd[4]    — additive filter resonance mod (normalised) per layer.
     * layerPitchSemi[4] — additive pitch mod in semitones per layer (vibrato).
     * layerAmpMul[4]    — multiplicative amplitude mod per layer (tremolo); 1.0 = unity.
     * layerPanAdd[4]    — additive pan mod (bipolar ±1) per layer (auto-pan).
     * lfoRateMul and masterMul are multiplicative (1 = unity).
     */
    void evaluate(float globalLfoBipolar,
                  float filtEnv01,
                  float ampEnv01,
                  float velocity01,
                  float pitchBendBipolar,
                  float layerCutSemi[4],
                  float layerResAdd[4],
                  float layerPitchSemi[4],
                  float layerAmpMul[4],
                  float layerPanAdd[4],
                  float& lfoRateMul,
                  float& lfoDepthAdd,
                  float& masterMul,
                  float& fxReverbMixAdd,
                  float& fxDelayMixAdd,
                  float& fxChorusMixAdd,
                  double sampleRate) noexcept;

    std::array<std::atomic<int>, kSlots> slotSrc {};
    std::array<std::atomic<int>, kSlots> slotTgt {};
    std::array<std::atomic<float>, kSlots> slotAmt {};
    std::array<std::atomic<float>, kSlots> slotSmooth {};
    std::array<std::atomic<int>, kSlots> slotFlags {}; // bit0 muted, bit1 invert, bits 8-11 layerScope 0=all

private:
    std::array<float, kSlots> smoothZ {};
    std::atomic<float> xyX { 0.5f };
    std::atomic<float> xyY { 0.5f };
    std::atomic<float> modWheel01 {};
    std::atomic<float> aftertouch01 {};
    std::atomic<float> pitchBendBipolar {};
    uint32_t rng = 0xC001D00Du;
};

} // namespace wolfsden
