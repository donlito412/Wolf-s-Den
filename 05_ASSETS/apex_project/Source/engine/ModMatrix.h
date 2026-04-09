#pragma once

#include <array>
#include <atomic>
#include <cstdint>

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
        Layer2_FilterCutoffSemi,
        Layer3_FilterCutoffSemi,
        LFO_RateMul,
        LFO_DepthAdd,
        MasterVolumeMul,
        Fx_ReverbMixAdd,
        Fx_DelayMixAdd,
        Fx_ChorusMixAdd,
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

    /**
     * Per-sample accumulation. layerCutSemi[4] / layerResAdd[4] are additive (semitones / res norm).
     * lfoRateMul and masterMul are multiplicative (1 = unity).
     */
    void evaluate(float globalLfoBipolar,
                  float filtEnv01,
                  float ampEnv01,
                  float velocity01,
                  float pitchBendBipolar,
                  float layerCutSemi[4],
                  float layerResAdd[4],
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
