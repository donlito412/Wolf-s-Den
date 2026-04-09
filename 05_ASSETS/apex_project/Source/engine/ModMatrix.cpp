#include "ModMatrix.h"

#include <cmath>

#include <juce_core/juce_core.h>

namespace wolfsden
{
namespace
{
inline int packFlags(bool muted, bool inverted, int layerScope) noexcept
{
    int f = 0;
    if (muted)
        f |= 1;
    if (inverted)
        f |= 2;
    f |= (juce::jlimit(0, 15, layerScope) << 8);
    return f;
}

inline bool isMuted(int f) noexcept
{
    return (f & 1) != 0;
}

inline bool isInverted(int f) noexcept
{
    return (f & 2) != 0;
}

inline int layerScope(int f) noexcept
{
    return (f >> 8) & 15;
}

inline float bipolarFrom01(float x) noexcept
{
    return juce::jlimit(-1.f, 1.f, x * 2.f - 1.f);
}

inline float rndBip(uint32_t& s) noexcept
{
    s = s * 1664525u + 1013904223u;
    return (float)((int32_t)s * (1.0f / 2147483648.0f));
}
} // namespace

ModMatrix::ModMatrix()
{
    reset();
}

void ModMatrix::reset() noexcept
{
    for (int i = 0; i < kSlots; ++i)
    {
        slotSrc[(size_t)i].store(Off, std::memory_order_relaxed);
        slotTgt[(size_t)i].store(None, std::memory_order_relaxed);
        slotAmt[(size_t)i].store(0.f, std::memory_order_relaxed);
        slotSmooth[(size_t)i].store(0.f, std::memory_order_relaxed);
        slotFlags[(size_t)i].store(0, std::memory_order_relaxed);
        smoothZ[(size_t)i] = 0.f;
    }
    xyX.store(0.5f, std::memory_order_relaxed);
    xyY.store(0.5f, std::memory_order_relaxed);
    modWheel01.store(0.f, std::memory_order_relaxed);
    aftertouch01.store(0.f, std::memory_order_relaxed);
    pitchBendBipolar.store(0.f, std::memory_order_relaxed);
    setSlot(0, XY_X, Layer0_FilterCutoffSemi, 0.6f, false, false, 0.05f, 0);
    setSlot(1, XY_Y, LFO_RateMul, 0.4f, false, false, 0.08f, 0);
    rng = 0xC001D00Du;
}

void ModMatrix::setSlot(int slotIndex,
                        Source src,
                        Target tgt,
                        float amount,
                        bool muted,
                        bool inverted,
                        float smooth01,
                        int layerScopeIn) noexcept
{
    if (slotIndex < 0 || slotIndex >= kSlots)
        return;
    slotSrc[(size_t)slotIndex].store((int)src, std::memory_order_relaxed);
    slotTgt[(size_t)slotIndex].store((int)tgt, std::memory_order_relaxed);
    slotAmt[(size_t)slotIndex].store(amount, std::memory_order_relaxed);
    slotSmooth[(size_t)slotIndex].store(juce::jlimit(0.f, 0.999f, smooth01), std::memory_order_relaxed);
    slotFlags[(size_t)slotIndex].store(packFlags(muted, inverted, layerScopeIn), std::memory_order_relaxed);
}

void ModMatrix::setXy(float x, float y) noexcept
{
    xyX.store(juce::jlimit(0.f, 1.f, x), std::memory_order_relaxed);
    xyY.store(juce::jlimit(0.f, 1.f, y), std::memory_order_relaxed);
}

float ModMatrix::getXyX() const noexcept
{
    return xyX.load(std::memory_order_relaxed);
}

float ModMatrix::getXyY() const noexcept
{
    return xyY.load(std::memory_order_relaxed);
}

void ModMatrix::setModWheel(float n01) noexcept
{
    modWheel01.store(juce::jlimit(0.f, 1.f, n01), std::memory_order_relaxed);
}

void ModMatrix::setAftertouch(float n01) noexcept
{
    aftertouch01.store(juce::jlimit(0.f, 1.f, n01), std::memory_order_relaxed);
}

void ModMatrix::setPitchBend(float bipolar) noexcept
{
    pitchBendBipolar.store(juce::jlimit(-1.f, 1.f, bipolar), std::memory_order_relaxed);
}

float ModMatrix::getPitchBendValue() const noexcept
{
    return pitchBendBipolar.load(std::memory_order_relaxed);
}

void ModMatrix::evaluate(float globalLfoBipolar,
                         float filtEnv01,
                         float ampEnv01,
                         float velocity01,
                         float pitchBend,
                         float layerCutSemi[4],
                         float layerResAdd[4],
                         float& lfoRateMul,
                         float& lfoDepthAdd,
                         float& masterMul,
                         float& fxReverbMixAdd,
                         float& fxDelayMixAdd,
                         float& fxChorusMixAdd,
                         double sampleRate) noexcept
{
    juce::ignoreUnused(sampleRate);
    for (int L = 0; L < 4; ++L)
    {
        layerCutSemi[(size_t)L] = 0.f;
        layerResAdd[(size_t)L] = 0.f;
    }
    lfoRateMul = 1.f;
    lfoDepthAdd = 0.f;
    masterMul = 1.f;
    fxReverbMixAdd = 0.f;
    fxDelayMixAdd = 0.f;
    fxChorusMixAdd = 0.f;

    const float xyx = xyX.load(std::memory_order_relaxed);
    const float xyy = xyY.load(std::memory_order_relaxed);
    const float mw = modWheel01.load(std::memory_order_relaxed);
    const float at = aftertouch01.load(std::memory_order_relaxed);

    for (int i = 0; i < kSlots; ++i)
    {
        const int fl = slotFlags[(size_t)i].load(std::memory_order_relaxed);
        if (isMuted(fl))
            continue;

        const int si = slotSrc[(size_t)i].load(std::memory_order_relaxed);
        const int ti = slotTgt[(size_t)i].load(std::memory_order_relaxed);
        if (si < 0 || ti < 0)
            continue;

        float v = 0.f;
        switch (si)
        {
            case GlobalLFO:
                v = globalLfoBipolar;
                break;
            case FilterEnv:
                v = bipolarFrom01(filtEnv01);
                break;
            case AmpEnv:
                v = bipolarFrom01(ampEnv01);
                break;
            case ModWheel:
                v = bipolarFrom01(mw);
                break;
            case Aftertouch:
                v = bipolarFrom01(at);
                break;
            case Velocity:
                v = bipolarFrom01(velocity01);
                break;
            case PitchBend:
                v = juce::jlimit(-1.f, 1.f, pitchBend);
                break;
            case XY_X:
                v = bipolarFrom01(xyx);
                break;
            case XY_Y:
                v = bipolarFrom01(xyy);
                break;
            case Random:
                v = rndBip(rng);
                break;
            default:
                continue;
        }

        float amt = slotAmt[(size_t)i].load(std::memory_order_relaxed);
        if (isInverted(fl))
            amt = -amt;

        const float sm = slotSmooth[(size_t)i].load(std::memory_order_relaxed);
        const float a = juce::jlimit(0.02f, 1.f, 1.f - sm);
        smoothZ[(size_t)i] += (v * amt - smoothZ[(size_t)i]) * a;
        const float c = smoothZ[(size_t)i];

        const int ls = layerScope(fl);

        auto applyLayerCut = [&](int layerIdx, float semis) {
            if (ls != 0 && ls != layerIdx + 1)
                return;
            layerCutSemi[(size_t)layerIdx] += semis;
        };

        switch (ti)
        {
            case Layer0_FilterCutoffSemi:
                applyLayerCut(0, c * 48.f);
                break;
            case Layer0_FilterRes:
                if (ls == 0 || ls == 1)
                    layerResAdd[0] += c * 0.4f;
                break;
            case Layer1_FilterCutoffSemi:
                applyLayerCut(1, c * 48.f);
                break;
            case Layer2_FilterCutoffSemi:
                applyLayerCut(2, c * 48.f);
                break;
            case Layer3_FilterCutoffSemi:
                applyLayerCut(3, c * 48.f);
                break;
            case LFO_RateMul:
                lfoRateMul += c * 0.5f;
                break;
            case LFO_DepthAdd:
                lfoDepthAdd += c * 0.25f;
                break;
            case MasterVolumeMul:
                masterMul += c * 0.35f;
                break;
            case Fx_ReverbMixAdd:
                fxReverbMixAdd += c * 0.45f;
                break;
            case Fx_DelayMixAdd:
                fxDelayMixAdd += c * 0.45f;
                break;
            case Fx_ChorusMixAdd:
                fxChorusMixAdd += c * 0.45f;
                break;
            default:
                break;
        }
    }

    lfoRateMul = juce::jmax(0.1f, lfoRateMul);
    masterMul = juce::jlimit(0.f, 2.f, masterMul);
    fxReverbMixAdd = juce::jlimit(-1.f, 1.f, fxReverbMixAdd);
    fxDelayMixAdd = juce::jlimit(-1.f, 1.f, fxDelayMixAdd);
    fxChorusMixAdd = juce::jlimit(-1.f, 1.f, fxChorusMixAdd);
}

} // namespace wolfsden
