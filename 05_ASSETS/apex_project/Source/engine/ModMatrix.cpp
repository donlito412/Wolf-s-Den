#include "ModMatrix.h"

#include <cmath>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

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

juce::ValueTree ModMatrix::toValueTree() const
{
    juce::ValueTree root("ModMatrix");
    for (int i = 0; i < kSlots; ++i)
    {
        juce::ValueTree s("Slot");
        s.setProperty("i", i, nullptr);
        s.setProperty("src", slotSrc[(size_t)i].load(std::memory_order_relaxed), nullptr);
        s.setProperty("tgt", slotTgt[(size_t)i].load(std::memory_order_relaxed), nullptr);
        s.setProperty("amt", slotAmt[(size_t)i].load(std::memory_order_relaxed), nullptr);
        s.setProperty("sm", slotSmooth[(size_t)i].load(std::memory_order_relaxed), nullptr);
        s.setProperty("fl", slotFlags[(size_t)i].load(std::memory_order_relaxed), nullptr);
        root.appendChild(s, nullptr);
    }
    root.setProperty("xyX", getXyX(), nullptr);
    root.setProperty("xyY", getXyY(), nullptr);
    root.setProperty("mw", modWheel01.load(std::memory_order_relaxed), nullptr);
    root.setProperty("at", aftertouch01.load(std::memory_order_relaxed), nullptr);
    root.setProperty("pb", pitchBendBipolar.load(std::memory_order_relaxed), nullptr);
    return root;
}

void ModMatrix::fromValueTree(const juce::ValueTree& v) noexcept
{
    if (!v.isValid() || !v.hasType("ModMatrix"))
        return;
    for (auto& z : smoothZ)
        z = 0.f;
    for (int i = 0; i < kSlots; ++i)
    {
        slotSrc[(size_t)i].store(Off, std::memory_order_relaxed);
        slotTgt[(size_t)i].store(None, std::memory_order_relaxed);
        slotAmt[(size_t)i].store(0.f, std::memory_order_relaxed);
        slotSmooth[(size_t)i].store(0.f, std::memory_order_relaxed);
        slotFlags[(size_t)i].store(0, std::memory_order_relaxed);
    }
    for (int ci = 0; ci < v.getNumChildren(); ++ci)
    {
        const auto s = v.getChild(ci);
        if (!s.hasType("Slot"))
            continue;
        const int i = (int)s.getProperty("i", -1);
        if (i < 0 || i >= kSlots)
            continue;
        slotSrc[(size_t)i].store((int)s.getProperty("src", Off), std::memory_order_relaxed);
        slotTgt[(size_t)i].store((int)s.getProperty("tgt", None), std::memory_order_relaxed);
        slotAmt[(size_t)i].store((float)s.getProperty("amt", 0.f), std::memory_order_relaxed);
        slotSmooth[(size_t)i].store((float)s.getProperty("sm", 0.f), std::memory_order_relaxed);
        slotFlags[(size_t)i].store((int)s.getProperty("fl", 0), std::memory_order_relaxed);
    }
    setXy((float)v.getProperty("xyX", 0.5f), (float)v.getProperty("xyY", 0.5f));
    modWheel01.store(juce::jlimit(0.f, 1.f, (float)v.getProperty("mw", 0.f)), std::memory_order_relaxed);
    aftertouch01.store(juce::jlimit(0.f, 1.f, (float)v.getProperty("at", 0.f)), std::memory_order_relaxed);
    pitchBendBipolar.store(juce::jlimit(-1.f, 1.f, (float)v.getProperty("pb", 0.f)), std::memory_order_relaxed);
}

void ModMatrix::evaluate(float globalLfoBipolar,
                         float filtEnv01,
                         float ampEnv01,
                         float velocity01,
                         float pitchBend,
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
                         double sampleRate) noexcept
{
    juce::ignoreUnused(sampleRate);
    for (int L = 0; L < 4; ++L)
    {
        layerCutSemi[(size_t)L]   = 0.f;
        layerResAdd[(size_t)L]    = 0.f;
        layerPitchSemi[(size_t)L] = 0.f;
        layerAmpMul[(size_t)L]    = 1.f;  // multiplicative: 1 = unity
        layerPanAdd[(size_t)L]    = 0.f;
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
        // Map 0-1 to something more useful for smoothing. 
        // 0.05f is a reasonable default if sm is 0.
        // We want sm=1.0 to be very slow (e.g. 0.0001) and sm=0.0 to be fast (e.g. 0.1).
        const float a = juce::jlimit(0.0001f, 0.2f, 0.15f * std::pow(1.f - sm, 2.5f) + 0.0001f);
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
            case Layer1_FilterRes:
                if (ls == 0 || ls == 2)
                    layerResAdd[1] += c * 0.4f;
                break;
            case Layer2_FilterCutoffSemi:
                applyLayerCut(2, c * 48.f);
                break;
            case Layer2_FilterRes:
                if (ls == 0 || ls == 3)
                    layerResAdd[2] += c * 0.4f;
                break;
            case Layer3_FilterCutoffSemi:
                applyLayerCut(3, c * 48.f);
                break;
            case Layer3_FilterRes:
                if (ls == 0 || ls == 4)
                    layerResAdd[3] += c * 0.4f;
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

            // --- Per-layer pitch modulation (vibrato) ---
            // Scale: c * 24 gives ±24 semitones at full mod amount.
            case Layer0_PitchSemi:
                if (ls == 0 || ls == 1) layerPitchSemi[0] += c * 24.f;
                break;
            case Layer1_PitchSemi:
                if (ls == 0 || ls == 2) layerPitchSemi[1] += c * 24.f;
                break;
            case Layer2_PitchSemi:
                if (ls == 0 || ls == 3) layerPitchSemi[2] += c * 24.f;
                break;
            case Layer3_PitchSemi:
                if (ls == 0 || ls == 4) layerPitchSemi[3] += c * 24.f;
                break;

            // --- Per-layer amplitude modulation (tremolo) ---
            // c is bipolar [-1..1]; full negative (-1) halves level, full positive (+1) adds 35%.
            // Applied multiplicatively: layerAmpMul[L] *= (1.0 + c * 0.35).
            case Layer0_AmpMul:
                if (ls == 0 || ls == 1) layerAmpMul[0] *= juce::jmax(0.f, 1.f + c * 0.35f);
                break;
            case Layer1_AmpMul:
                if (ls == 0 || ls == 2) layerAmpMul[1] *= juce::jmax(0.f, 1.f + c * 0.35f);
                break;
            case Layer2_AmpMul:
                if (ls == 0 || ls == 3) layerAmpMul[2] *= juce::jmax(0.f, 1.f + c * 0.35f);
                break;
            case Layer3_AmpMul:
                if (ls == 0 || ls == 4) layerAmpMul[3] *= juce::jmax(0.f, 1.f + c * 0.35f);
                break;

            // --- Per-layer pan modulation (auto-pan) ---
            // c * 0.5 gives ±0.5 pan range at full mod amount (pan is bipolar ±1).
            case Layer0_PanAdd:
                if (ls == 0 || ls == 1) layerPanAdd[0] += c * 0.5f;
                break;
            case Layer1_PanAdd:
                if (ls == 0 || ls == 2) layerPanAdd[1] += c * 0.5f;
                break;
            case Layer2_PanAdd:
                if (ls == 0 || ls == 3) layerPanAdd[2] += c * 0.5f;
                break;
            case Layer3_PanAdd:
                if (ls == 0 || ls == 4) layerPanAdd[3] += c * 0.5f;
                break;

            default:
                break;
        }
    }

    // Clamp accumulated per-layer values to safe ranges
    for (int L = 0; L < 4; ++L)
    {
        layerPitchSemi[(size_t)L] = juce::jlimit(-48.f, 48.f, layerPitchSemi[(size_t)L]);
        layerAmpMul[(size_t)L]    = juce::jlimit(0.f, 2.f,   layerAmpMul[(size_t)L]);
        layerPanAdd[(size_t)L]    = juce::jlimit(-1.f, 1.f,   layerPanAdd[(size_t)L]);
    }

    lfoRateMul = juce::jmax(0.1f, lfoRateMul);
    masterMul = juce::jlimit(0.f, 2.f, masterMul);
    fxReverbMixAdd = juce::jlimit(-1.f, 1.f, fxReverbMixAdd);
    fxDelayMixAdd = juce::jlimit(-1.f, 1.f, fxDelayMixAdd);
    fxChorusMixAdd = juce::jlimit(-1.f, 1.f, fxChorusMixAdd);
}

} // namespace wolfsden
