#include "FxEngine.h"

#include <cmath>

#include <juce_dsp/juce_dsp.h>

namespace wolfsden
{
namespace
{
inline float readFloatAP(std::atomic<float>* ap, juce::AudioParameterFloat* pf, float defV) noexcept
{
    if (!ap)
        return defV;
    const float v = ap->load(std::memory_order_relaxed);
    if (pf != nullptr)
    {
        const auto r = pf->getNormalisableRange();
        return juce::jlimit(r.start, r.end, v);
    }
    return v;
}

inline void wetMix(float dryL, float dryR, float wetL, float wetR, float m, float& outL, float& outR) noexcept
{
    const float om = 1.f - m;
    outL = dryL * om + wetL * m;
    outR = dryR * om + wetR * m;
}

inline float softClip(float x) noexcept
{
    return std::tanh(x);
}

inline float hardClip(float x, float ceiling) noexcept
{
    return juce::jlimit(-ceiling, ceiling, x);
}
} // namespace

struct FxEngine::SlotDSP
{
    juce::dsp::Reverb rev;
    juce::dsp::DelayLine<float> dL { 192000 }, dR { 192000 };
    juce::dsp::DelayLine<float> revPreL { 48000 }, revPreR { 48000 };
    juce::dsp::StateVariableTPTFilter<float> hpL, hpR, lpL, lpR;
    juce::dsp::IIR::Filter<float> eqL[4], eqR[4];
    float lfo = 0.f;
    float fbL = 0.f, fbR = 0.f;
    float cEnv = 0.f;
    float gateEnv = 0.f;
    float phz[8] {};
    float decimPhase = 0.f, holdL = 0.f, holdR = 0.f;
    float monoLpL = 0.f, monoLpR = 0.f;
    float lastEqDb[4] { -1000.f, -1000.f, -1000.f, -1000.f };
    bool prepared = false;
    /** 0 hall, 1 plate, 2 spring — skip setParameters unless changed (less work / fewer internal updates). */
    int revPresetKey = -1;

    void updateEqIfNeeded(double sr, float g0dB, float g1dB, float g2dB, float g3dB) noexcept
    {
        constexpr float eps = 0.0005f;
        auto moved = [&](int k, float g) noexcept {
            return std::abs(g - lastEqDb[(size_t)k]) > eps;
        };
        if (!moved(0, g0dB) && !moved(1, g1dB) && !moved(2, g2dB) && !moved(3, g3dB))
            return;
        lastEqDb[0] = g0dB;
        lastEqDb[1] = g1dB;
        lastEqDb[2] = g2dB;
        lastEqDb[3] = g3dB;

        const float srf = (float)sr;
        const float qShelf = 0.707f;
        const float qLoMid = 1.0f;
        const float qHiMid = 1.15f;

        auto c0 = juce::dsp::IIR::Coefficients<float>::makeLowShelf(srf, 100.f, qShelf, juce::Decibels::decibelsToGain(g0dB));
        auto c1 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(srf, 280.f, qLoMid, juce::Decibels::decibelsToGain(g1dB));
        auto c2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(srf, 2400.f, qHiMid, juce::Decibels::decibelsToGain(g2dB));
        auto c3 = juce::dsp::IIR::Coefficients<float>::makeHighShelf(srf, 8000.f, qShelf, juce::Decibels::decibelsToGain(g3dB));

        *eqL[0].coefficients = *c0;
        *eqR[0].coefficients = *c0;
        *eqL[1].coefficients = *c1;
        *eqR[1].coefficients = *c1;
        *eqL[2].coefficients = *c2;
        *eqR[2].coefficients = *c2;
        *eqL[3].coefficients = *c3;
        *eqR[3].coefficients = *c3;
    }

    void prepare(double sr, uint32_t maxB)
    {
        juce::dsp::ProcessSpec sp { sr, maxB, 2 };
        rev.prepare(sp);
        {
            juce::dsp::Reverb::Parameters p;
            p.roomSize = 0.55f;
            p.damping = 0.45f;
            p.wetLevel = 1.f;
            p.dryLevel = 0.f;
            p.width = 1.f;
            rev.setParameters(p);
        }

        juce::dsp::ProcessSpec sp1 { sr, maxB, 1 };
        dL.prepare(sp1);
        dR.prepare(sp1);
        dL.setMaximumDelayInSamples(juce::jmin(192000, (int)(sr * 2.0)));
        dR.setMaximumDelayInSamples(juce::jmin(192000, (int)(sr * 2.0)));

        revPreL.prepare(sp1);
        revPreR.prepare(sp1);
        revPreL.setMaximumDelayInSamples(juce::jmin(48000, (int)(sr * 0.1)));
        revPreR.setMaximumDelayInSamples(juce::jmin(48000, (int)(sr * 0.1)));

        juce::dsp::ProcessSpec spF { sr, maxB, 1 };
        hpL.prepare(spF);
        hpR.prepare(spF);
        lpL.prepare(spF);
        lpR.prepare(spF);
        hpL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        hpR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        lpL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        lpR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        const float q = 1.f / juce::MathConstants<float>::sqrt2;
        hpL.setCutoffFrequency(120.f);
        hpR.setCutoffFrequency(120.f);
        hpL.setResonance(q);
        hpR.setResonance(q);
        lpL.setCutoffFrequency(8000.f);
        lpR.setCutoffFrequency(8000.f);
        lpL.setResonance(q);
        lpR.setResonance(q);

        for (int i = 0; i < 4; ++i)
        {
            eqL[(size_t)i].prepare(sp1);
            eqR[(size_t)i].prepare(sp1);
        }
        updateEqIfNeeded(sr, 0.f, 0.f, 0.f, 0.f);

        prepared = true;
    }

    void reset()
    {
        rev.reset();
        dL.reset();
        dR.reset();
        revPreL.reset();
        revPreR.reset();
        lfo = fbL = fbR = cEnv = gateEnv = 0.f;
        for (auto& z : phz)
            z = 0.f;
        decimPhase = holdL = holdR = 0.f;
        monoLpL = monoLpR = 0.f;
        revPresetKey = -1;
        lastEqDb[0] = lastEqDb[1] = lastEqDb[2] = lastEqDb[3] = -1000.f;
        for (int i = 0; i < 4; ++i)
        {
            eqL[(size_t)i].reset();
            eqR[(size_t)i].reset();
        }
    }
};

FxEngine::FxEngine()
{
    slotDSPs.resize(24);
    for (auto& p : slotDSPs)
        p = std::make_unique<SlotDSP>();
}

FxEngine::~FxEngine() = default;

void FxEngine::bindPointers(juce::AudioProcessorValueTreeState& state)
{
    apvts = &state;
    for (int L = 0; L < 4; ++L)
        for (int s = 0; s < 4; ++s)
        {
            const juce::String id = "fx_l" + juce::String(L) + "_s" + juce::String(s) + "_mix";
            const size_t ix = (size_t)(L * 4 + s);
            layerMixPtrs[ix] = state.getRawParameterValue(id);
            layerMixParamF[ix] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(id));
        }

    commonMixPtrs[0] = state.getRawParameterValue("fx_reverb_mix");
    commonMixPtrs[1] = state.getRawParameterValue("fx_delay_mix");
    commonMixPtrs[2] = state.getRawParameterValue("fx_chorus_mix");
    commonMixPtrs[3] = state.getRawParameterValue("fx_compressor");
    commonMixParamF[0] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter("fx_reverb_mix"));
    commonMixParamF[1] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter("fx_delay_mix"));
    commonMixParamF[2] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter("fx_chorus_mix"));
    commonMixParamF[3] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter("fx_compressor"));

    for (int m = 0; m < 4; ++m)
    {
        const juce::String mid = "fx_m" + juce::String(m) + "_mix";
        masterMixPtrs[(size_t)m] = state.getRawParameterValue(mid);
        masterMixParamF[(size_t)m] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(mid));
    }

    for (int si = 0; si < 24; ++si)
    {
        const juce::String pfxEq = "fx_s" + juce::String(si).paddedLeft('0', 2) + "_eq";
        const juce::String pfxP = "fx_s" + juce::String(si).paddedLeft('0', 2) + "_p";
        for (int b = 0; b < 4; ++b)
        {
            const size_t flat = (size_t)(si * 4 + b);
            
            const juce::String eid = pfxEq + juce::String(b);
            slotEqBandDb[flat] = state.getRawParameterValue(eid);
            slotEqBandParamF[flat] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(eid));
            
            const juce::String pid = pfxP + juce::String(b);
            slotParamDb[flat] = state.getRawParameterValue(pid);
            slotParamParamF[flat] = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(pid));
        }
    }
}

void FxEngine::ensureProcessingSpace(int numSamples) noexcept
{
    if (numSamples <= maxBlockSize)
        return;
    maxBlockSize = numSamples;
    scratchLayerStereo.setSize(2, maxBlockSize, false, false, true);
    scratchMixStereo.setSize(2, maxBlockSize, false, false, true);
    scratchReverbWet.setSize(2, maxBlockSize, false, false, true);
    const uint32_t mb = (uint32_t)maxBlockSize;
    for (auto& s : slotDSPs)
        s->prepare(sampleRate, mb);
}

void FxEngine::prepare(double sr, int maxBlock, juce::AudioProcessorValueTreeState& state)
{
    sampleRate = sr;
    maxBlockSize = juce::jmax(1, maxBlock);
    bindPointers(state);
    scratchLayerStereo.setSize(2, maxBlockSize, false, false, true);
    scratchMixStereo.setSize(2, maxBlockSize, false, false, true);
    scratchReverbWet.setSize(2, maxBlockSize, false, false, true);
    const uint32_t mb = (uint32_t)maxBlockSize;
    for (auto& s : slotDSPs)
        s->prepare(sr, mb);
    prepared = true;
}

void FxEngine::reset() noexcept
{
    for (auto& s : slotDSPs)
        s->reset();
}

void FxEngine::cacheRoutingForBlock(juce::AudioProcessorValueTreeState& ap) noexcept
{
    auto readChoice = [&](const juce::String& id, int fallback) noexcept {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(ap.getParameter(id)))
            return p->getIndex();
        return fallback;
    };

    for (int i = 0; i < 16; ++i)
    {
        const int L = i / 4;
        const int s = i % 4;
        cachedLayerType[(size_t)i] = readChoice("fx_l" + juce::String(L) + "_s" + juce::String(s) + "_type", 0);
    }
    for (int c = 0; c < 4; ++c)
        cachedCommonType[(size_t)c] = readChoice("fx_c" + juce::String(c) + "_type", 0);
    for (int m = 0; m < 4; ++m)
        cachedMasterType[(size_t)m] = readChoice("fx_m" + juce::String(m) + "_type", 0);
}

void FxEngine::processRack(juce::AudioBuffer<float>& stereo,
                           int dspBaseIndex,
                           const std::array<int, kFxSlotsPerRack>& types,
                           const std::array<float, kFxSlotsPerRack>& mixes,
                           int numSamples) noexcept
{
    float* Lw = stereo.getWritePointer(0);
    float* Rw = stereo.getWritePointer(1);
    if (Lw == nullptr || Rw == nullptr)
        return;

    for (int s = 0; s < kFxSlotsPerRack; ++s)
    {
        const int ti = types[(size_t)s];
        if (ti <= 0)
            continue;

        const float m = juce::jlimit(0.f, 1.f, mixes[(size_t)s]);
        if (m <= 0.0001f)
            continue;

        auto& st = *slotDSPs[(size_t)(dspBaseIndex + s)];
        const auto t = (FxUnitType)ti;
        const double sr = sampleRate;
        const int flat = dspBaseIndex + s;
        
        auto readP = [&](int pIdx) {
            return readFloatAP(slotParamDb[(size_t)(flat * 4 + pIdx)], slotParamParamF[(size_t)(flat * 4 + pIdx)], 0.5f);
        };

        if (t == FxUnitType::Reverb || t == FxUnitType::ReverbPlate || t == FxUnitType::ReverbSpring)
        {
            const int revKey = (t == FxUnitType::ReverbPlate) ? 1 : (t == FxUnitType::ReverbSpring) ? 2 : 0;
            const float size = readP(0);
            const float damp = readP(1);
            
            if (st.revPresetKey != revKey)
            {
                st.revPresetKey = revKey;
                juce::dsp::Reverb::Parameters p;
                p.roomSize = size;
                p.damping = damp;
                p.wetLevel = 1.f;
                p.dryLevel = 0.f;
                p.width = 1.f;
                st.rev.setParameters(p);
            }
            else
            {
                // Update size/damp if changed
                auto p = st.rev.getParameters();
                if (std::abs(p.roomSize - size) > 0.001f || std::abs(p.damping - damp) > 0.001f)
                {
                    p.roomSize = size;
                    p.damping = damp;
                    st.rev.setParameters(p);
                }
            }

            const float preMs = (t == FxUnitType::ReverbPlate) ? 22.f : (t == FxUnitType::ReverbSpring) ? 14.f : (32.f * readP(2));
            const int maxPre = juce::jmax(2, st.revPreL.getMaximumDelayInSamples() - 1);
            const int preSamps = juce::jlimit(1, maxPre, (int)(sr * 0.001 * (double)preMs));

            float* p0 = scratchReverbWet.getWritePointer(0);
            float* p1 = scratchReverbWet.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i)
            {
                st.revPreL.pushSample(0, Lw[i]);
                st.revPreR.pushSample(0, Rw[i]);
                p0[i] = st.revPreL.popSample(0, (float)preSamps, true);
                p1[i] = st.revPreR.popSample(0, (float)preSamps, true);
            }

            std::array<float*, 2> ch { p0, p1 };
            juce::dsp::AudioBlock<float> block(ch.data(), 2, (size_t)numSamples);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            st.rev.process(ctx);
            for (int i = 0; i < numSamples; ++i)
                wetMix(Lw[i], Rw[i], p0[i], p1[i], m, Lw[i], Rw[i]);
            continue;
        }

        if (t == FxUnitType::Eq4Band)
        {
            st.updateEqIfNeeded(sr,
                                readFloatAP(slotEqBandDb[(size_t)(flat * 4 + 0)], slotEqBandParamF[(size_t)(flat * 4 + 0)], 0.f),
                                readFloatAP(slotEqBandDb[(size_t)(flat * 4 + 1)], slotEqBandParamF[(size_t)(flat * 4 + 1)], 0.f),
                                readFloatAP(slotEqBandDb[(size_t)(flat * 4 + 2)], slotEqBandParamF[(size_t)(flat * 4 + 2)], 0.f),
                                readFloatAP(slotEqBandDb[(size_t)(flat * 4 + 3)], slotEqBandParamF[(size_t)(flat * 4 + 3)], 0.f));
        }

        const float p0 = readP(0);
        const float p1 = readP(1);
        const float p2 = readP(2);
        const float p3 = readP(3);

        for (int i = 0; i < numSamples; ++i)
        {
            // Simple denormal protection
            const float dryL = std::abs(Lw[i]) < 1e-12f ? 0.f : Lw[i];
            const float dryR = std::abs(Rw[i]) < 1e-12f ? 0.f : Rw[i];
            float wetL = dryL, wetR = dryR;

            switch (t)
            {
                case FxUnitType::Off:
                case FxUnitType::Reverb:
                case FxUnitType::ReverbPlate:
                case FxUnitType::ReverbSpring:
                case FxUnitType::NumTypes:
                    wetL = dryL;
                    wetR = dryR;
                    break;
                case FxUnitType::Compressor:
                {
                    const float det = std::abs(0.5f * (dryL + dryR));
                    const float atk = std::exp(-1.f / (float)(sr * (0.001 + p0 * 0.1)));
                    const float rel = std::exp(-1.f / (float)(sr * (0.01 + p1 * 1.0)));
                    st.cEnv = det > st.cEnv ? atk * st.cEnv + (1.f - atk) * det : rel * st.cEnv + (1.f - rel) * det;
                    const float thresh = 0.01f + (1.f - p2) * 0.9f;
                    const float ratio = 1.f + p3 * 19.f;
                    float g = 1.f;
                    if (st.cEnv > thresh)
                        g = thresh + (st.cEnv - thresh) / ratio;
                    g = juce::jlimit(0.01f, 1.f, g / juce::jmax(1.e-5f, st.cEnv));
                    wetL *= g;
                    wetR *= g;
                    break;
                }
                case FxUnitType::Limiter:
                {
                    const float ceil = 0.5f + p0 * 0.49f;
                    const float drv = 1.f + p1 * 3.f;
                    wetL = softClip(dryL * drv) * ceil;
                    wetR = softClip(dryR * drv) * ceil;
                    break;
                }
                case FxUnitType::Gate:
                {
                    const float det = std::abs(0.5f * (dryL + dryR));
                    const float thr = 0.001f + p0 * 0.5f;
                    const float atk = std::exp(-1.f / (float)(sr * (0.0001 + p1 * 0.01)));
                    const float rel = std::exp(-1.f / (float)(sr * (0.001 + p2 * 0.5)));
                    const float target = det > thr ? 1.f : 0.f;
                    st.gateEnv = target > st.gateEnv ? atk * st.gateEnv + (1.f - atk) * target
                                                     : rel * st.gateEnv + (1.f - rel) * target;
                    wetL = dryL * st.gateEnv;
                    wetR = dryR * st.gateEnv;
                    break;
                }
                case FxUnitType::Eq4Band:
                {
                    wetL = dryL;
                    wetR = dryR;
                    for (int k = 0; k < 4; ++k)
                        wetL = st.eqL[(size_t)k].processSample(wetL);
                    for (int k = 0; k < 4; ++k)
                        wetR = st.eqR[(size_t)k].processSample(wetR);
                    break;
                }
                case FxUnitType::HighPass:
                {
                    const float f = juce::jlimit(20.f, 20000.f, 20.f * std::pow(1000.f, p0));
                    st.hpL.setCutoffFrequency(f);
                    st.hpR.setCutoffFrequency(f);
                    st.hpL.setResonance(0.707f + p1 * 4.f);
                    st.hpR.setResonance(0.707f + p1 * 4.f);
                    wetL = st.hpL.processSample(0, dryL);
                    wetR = st.hpR.processSample(0, dryR);
                    break;
                }
                case FxUnitType::LowPass:
                {
                    const float f = juce::jlimit(20.f, 20000.f, 20.f * std::pow(1000.f, p0));
                    st.lpL.setCutoffFrequency(f);
                    st.lpR.setCutoffFrequency(f);
                    st.lpL.setResonance(0.707f + p1 * 4.f);
                    st.lpR.setResonance(0.707f + p1 * 4.f);
                    wetL = st.lpL.processSample(0, dryL);
                    wetR = st.lpR.processSample(0, dryR);
                    break;
                }
                case FxUnitType::SoftClip:
                {
                    const float d = 1.f + p0 * 20.f;
                    wetL = softClip(dryL * d);
                    wetR = softClip(dryR * d);
                    break;
                }
                case FxUnitType::HardClip:
                {
                    const float drv = 1.f + p0 * 20.f;
                    const float c = 0.05f + p1 * 0.95f;
                    wetL = hardClip(dryL * drv, c);
                    wetR = hardClip(dryR * drv, c);
                    break;
                }
                case FxUnitType::BitCrusher:
                {
                    const int bits = juce::jlimit(1, 16, (int)(1 + p0 * 15.f));
                    const float scl = std::pow(2.f, (float)bits - 1.f);
                    st.decimPhase += 1.f;
                    const float holdEvery = 1.f + p1 * 64.f;
                    if (st.decimPhase >= holdEvery)
                    {
                        st.decimPhase = 0.f;
                        st.holdL = std::round(dryL * scl) / scl;
                        st.holdR = std::round(dryR * scl) / scl;
                    }
                    wetL = st.holdL;
                    wetR = st.holdR;
                    break;
                }
                case FxUnitType::Waveshaper:
                {
                    const float d = 1.f + p0 * 12.f;
                    wetL = std::tanh(dryL * d);
                    wetR = std::tanh(dryR * d);
                    break;
                }
                case FxUnitType::Chorus:
                case FxUnitType::Vibrato:
                {
                    const float rate = (t == FxUnitType::Vibrato) ? (0.1f + p0 * 10.f) : (0.05f + p0 * 2.f);
                    st.lfo += juce::MathConstants<float>::twoPi * rate / (float)sr;
                    if (st.lfo > juce::MathConstants<float>::twoPi)
                        st.lfo -= juce::MathConstants<float>::twoPi;
                    const float modScl = (t == FxUnitType::Vibrato ? 0.005f : 0.002f) * p1;
                    const float mod = std::sin(st.lfo) * modScl;
                    const int del = juce::jlimit(1, (int)(sr * 0.05), (int)(sr * (0.01 + mod)));
                    st.dL.pushSample(0, dryL);
                    st.dR.pushSample(0, dryR);
                    wetL = st.dL.popSample(0, (float)del, true);
                    wetR = st.dR.popSample(0, (float)del, true);
                    break;
                }
                case FxUnitType::Flanger:
                {
                    const float rate = 0.05f + p0 * 5.f;
                    st.lfo += juce::MathConstants<float>::twoPi * rate / (float)sr;
                    if (st.lfo > juce::MathConstants<float>::twoPi)
                        st.lfo -= juce::MathConstants<float>::twoPi;
                    const float modScl = 0.001f + p1 * 0.005f;
                    const float mod = (0.5f + 0.5f * std::sin(st.lfo)) * (float)(sr * modScl);
                    const float fb = p2 * 0.85f;
                    st.dL.pushSample(0, softClip(dryL + st.fbL * fb));
                    st.dR.pushSample(0, softClip(dryR + st.fbR * fb));
                    wetL = st.dL.popSample(0, 1.f + mod, true);
                    wetR = st.dR.popSample(0, 1.f + mod, true);
                    st.fbL = std::isfinite(wetL) ? wetL : 0.f;
                    st.fbR = std::isfinite(wetR) ? wetR : 0.f;
                    break;
                }
                case FxUnitType::Phaser:
                {
                    const float rate = 0.05f + p0 * 5.f;
                    st.lfo += juce::MathConstants<float>::twoPi * rate / (float)sr;
                    if (st.lfo > juce::MathConstants<float>::twoPi)
                        st.lfo -= juce::MathConstants<float>::twoPi;
                    const float coef = 0.1f + p1 * 0.85f + 0.05f * std::sin(st.lfo);
                    auto ap = [&](float x, int k) {
                        const float y = coef * (x - st.phz[(size_t)k]) + st.phz[(size_t)k];
                        st.phz[(size_t)k] = y;
                        return 2.f * y - x;
                    };
                    wetL = ap(ap(ap(dryL, 0), 1), 2);
                    wetR = ap(ap(ap(dryR, 4), 5), 6);
                    break;
                }
                case FxUnitType::Tremolo:
                {
                    const float rate = 0.1f + p0 * 20.f;
                    st.lfo += juce::MathConstants<float>::twoPi * rate / (float)sr;
                    if (st.lfo > juce::MathConstants<float>::twoPi)
                        st.lfo -= juce::MathConstants<float>::twoPi;
                    const float g = 1.f - p1 * (0.5f + 0.5f * std::sin(st.lfo));
                    wetL = dryL * g;
                    wetR = dryR * g;
                    break;
                }
                case FxUnitType::AutoPan:
                {
                    const float rate = 0.1f + p0 * 10.f;
                    st.lfo += juce::MathConstants<float>::twoPi * rate / (float)sr;
                    if (st.lfo > juce::MathConstants<float>::twoPi)
                        st.lfo -= juce::MathConstants<float>::twoPi;
                    const float depth = p1;
                    const float p = 0.5f + 0.5f * std::sin(st.lfo) * depth;
                    const float gL = std::sqrt(p);
                    const float gR = std::sqrt(1.f - p);
                    wetL = dryL * gL;
                    wetR = dryR * gR;
                    break;
                }
                case FxUnitType::DelayStereo:
                {
                    const float ms = 1.f + p0 * 999.f;
                    const int del = juce::jlimit(1, (int)(sr * 1.0), (int)(sr * 0.001 * (double)ms));
                    const float fb = p1 * 0.95f;
                    st.dL.pushSample(0, softClip(dryL + st.fbL * fb));
                    st.dR.pushSample(0, softClip(dryR + st.fbR * fb));
                    wetL = st.dL.popSample(0, (float)del, true);
                    wetR = st.dR.popSample(0, (float)del, true);
                    st.fbL = std::isfinite(wetL) ? wetL : 0.f;
                    st.fbR = std::isfinite(wetR) ? wetR : 0.f;
                    break;
                }
                case FxUnitType::DelayPingPong:
                {
                    const float ms = 1.f + p0 * 999.f;
                    const int del = juce::jlimit(1, (int)(sr * 1.0), (int)(sr * 0.001 * (double)ms));
                    const float fb = p1 * 0.95f;
                    st.dL.pushSample(0, softClip(dryL + st.fbR * fb));
                    st.dR.pushSample(0, softClip(dryR + st.fbL * fb));
                    wetL = st.dL.popSample(0, (float)del, true);
                    wetR = st.dR.popSample(0, (float)del, true);
                    st.fbL = std::isfinite(wetL) ? wetL : 0.f;
                    st.fbR = std::isfinite(wetR) ? wetR : 0.f;
                    break;
                }
                case FxUnitType::StereoWidth:
                {
                    const float w = p0 * 2.f;
                    const float M = 0.5f * (dryL + dryR);
                    float S = 0.5f * (dryL - dryR);
                    S *= w;
                    wetL = M + S;
                    wetR = M - S;
                    break;
                }
                case FxUnitType::MonoBass:
                {
                    const float f = 20.f + p0 * 480.f;
                    const float alpha = 1.f / (1.f + (float)sr / (juce::MathConstants<float>::twoPi * f));
                    const float mono = 0.5f * (dryL + dryR);
                    st.monoLpL = (1.f - alpha) * st.monoLpL + alpha * mono;
                    const float S = 0.5f * (dryL - dryR) * (1.f - p1);
                    wetL = st.monoLpL + S;
                    wetR = st.monoLpL - S;
                    break;
                }
                default:
                    wetL = dryL;
                    wetR = dryR;
                    break;
            }

            wetMix(dryL, dryR, wetL, wetR, m, Lw[i], Rw[i]);
        }
    }
}

void FxEngine::processBlock(juce::AudioBuffer<float>& layerBus,
                            juce::AudioBuffer<float>& outStereo,
                            juce::AudioProcessorValueTreeState& apvtsRef,
                            float fxReverbMixAdd,
                            float fxDelayMixAdd,
                            float fxChorusMixAdd) noexcept
{
    if (!prepared || apvts == nullptr)
        return;

    const int n = juce::jmin(layerBus.getNumSamples(), outStereo.getNumSamples());
    if (n < 1 || layerBus.getNumChannels() < 8 || outStereo.getNumChannels() < 1)
        return;

    ensureProcessingSpace(n);

    cacheRoutingForBlock(apvtsRef);

    scratchMixStereo.clear(0, 0, n);
    if (scratchMixStereo.getNumChannels() > 1)
        scratchMixStereo.clear(1, 0, n);

    for (int L = 0; L < 4; ++L)
    {
        scratchLayerStereo.copyFrom(0, 0, layerBus, L * 2, 0, n);
        scratchLayerStereo.copyFrom(1, 0, layerBus, L * 2 + 1, 0, n);

        std::array<int, kFxSlotsPerRack> lt {};
        std::array<float, kFxSlotsPerRack> lm {};
        for (int s = 0; s < 4; ++s)
        {
            lt[(size_t)s] = cachedLayerType[(size_t)(L * 4 + s)];
            const size_t mixIx = (size_t)(L * 4 + s);
            lm[(size_t)s] = readFloatAP(layerMixPtrs[mixIx], layerMixParamF[mixIx], 0.f);
        }
        processRack(scratchLayerStereo, L * 4, lt, lm, n);

        scratchMixStereo.addFrom(0, 0, scratchLayerStereo, 0, 0, n);
        scratchMixStereo.addFrom(1, 0, scratchLayerStereo, 1, 0, n);
    }

    std::array<int, kFxSlotsPerRack> ct {};
    std::array<float, kFxSlotsPerRack> cm {};
    for (int c = 0; c < 4; ++c)
    {
        ct[(size_t)c] = cachedCommonType[(size_t)c];
        float base = readFloatAP(commonMixPtrs[c], commonMixParamF[c], 0.f);
        if (c == 0)
            base = juce::jlimit(0.f, 1.f, base + fxReverbMixAdd);
        else if (c == 1)
            base = juce::jlimit(0.f, 1.f, base + fxDelayMixAdd);
        else if (c == 2)
            base = juce::jlimit(0.f, 1.f, base + fxChorusMixAdd);
        cm[(size_t)c] = base;
    }
    processRack(scratchMixStereo, 16, ct, cm, n);

    std::array<int, kFxSlotsPerRack> mt {};
    std::array<float, kFxSlotsPerRack> mm {};
    for (int m = 0; m < 4; ++m)
    {
        mt[(size_t)m] = cachedMasterType[(size_t)m];
        mm[(size_t)m] = readFloatAP(masterMixPtrs[(size_t)m], masterMixParamF[(size_t)m],
                                    m == 0 ? 1.f : 0.f);
    }
    processRack(scratchMixStereo, 20, mt, mm, n);

    outStereo.clear(0, 0, n);
    if (outStereo.getNumChannels() > 1)
        outStereo.clear(1, 0, n);

    // Safety limiter: soft-clip final output to prevent speaker damage without harsh distortion
    for (int ch = 0; ch < scratchMixStereo.getNumChannels(); ++ch)
    {
        float* d = scratchMixStereo.getWritePointer(ch);
        for (int i = 0; i < n; ++i)
        {
            if (!std::isfinite(d[i]))
                d[i] = 0.f;
            else if (std::abs(d[i]) > 1.0f)
                d[i] = std::tanh(d[i]);
        }
    }

    if (outStereo.getNumChannels() > 1)
    {
        outStereo.copyFrom(0, 0, scratchMixStereo, 0, 0, n);
        outStereo.copyFrom(1, 0, scratchMixStereo, 1, 0, n);
    }
    else if (outStereo.getNumChannels() == 1)
    {
        outStereo.copyFrom(0, 0, scratchMixStereo, 0, 0, n);
        outStereo.addFrom(0, 0, scratchMixStereo, 1, 0, n);
        outStereo.applyGain(0, 0, n, 0.5f); // Downmix stereo to mono
    }
    else
    {
        float* d = outStereo.getWritePointer(0);
        for (int i = 0; i < n; ++i)
            d[i] = 0.5f * (scratchMixStereo.getSample(0, i) + scratchMixStereo.getSample(1, i));
    }
}

} // namespace wolfsden
