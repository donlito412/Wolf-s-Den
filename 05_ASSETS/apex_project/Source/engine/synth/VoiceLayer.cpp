#include "VoiceLayer.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace wolfsden
{
namespace
} // namespace

float VoiceLayer::readP(std::atomic<float>* ap, float defV) noexcept
{
    return ap ? ap->load(std::memory_order_relaxed) : defV;
}

void VoiceLayer::setBiquadIdentity(dsp::Biquad& b) noexcept
{
    b.b0 = 1.0;
    b.b1 = b.b2 = b.a1 = b.a2 = 0.0;
}

void VoiceLayer::prepare(double sampleRate,
                         const double* wavetable,
                         int wavetableSize,
                         const double* granularSource,
                         int granularSize) noexcept
{
    sr = sampleRate;
    wtData = wavetable;
    wtLen = juce::jmax(8, wavetableSize);
    granSource = granularSource;
    granLen = juce::jmax(8, granularSize);
    ampAdsr.setSampleRate(sr);
    filtAdsr.setSampleRate(sr);
    lfoLayer.setSampleRate(sr);
    lfoLayer.setShape(0);
    lfoLayer2.setSampleRate(sr);
    lfoLayer2.setShape(0);
}

void VoiceLayer::reset() noexcept
{
    phaseSin = phaseSaw = phaseSq = triIntegrator = 0;
    phaseWt = phaseSmpl = phaseFmCar = phaseFmMod = 0;
    combBuf.fill(0.0);
    combWritePos = 0;
    combDelaySamples = 0;
    combFeedback = 0.0;
    uniSin.fill(0.0);
    uniSaw.fill(0.0);
    uniSq.fill(0.0);
    uniTri.fill(0.0);
    for (auto& g : grains)
        g.active = false;
    grainSpawnCounter = 0;
    ampAdsr.reset();
    filtAdsr.reset();
    f1.reset();
    f2.reset();
    cachedAmpA = cachedAmpD = cachedAmpS = cachedAmpR = -1.f;
    cachedFilA = cachedFilD = cachedFilS = cachedFilR = -1.f;
}

void VoiceLayer::noteOn(int midiNote, float velocity) noexcept
{
    currentNote = midiNote;
    currentVel = velocity;
    phaseSin = phaseSaw = phaseSq = phaseWt = phaseSmpl = phaseFmCar = phaseFmMod = 0;
    triIntegrator = 0;
    uniSin.fill(0.0);
    uniSaw.fill(0.0);
    uniSq.fill(0.0);
    uniTri.fill(0.0);
    ampAdsr.noteOn();
    filtAdsr.noteOn();
}

void VoiceLayer::noteOff() noexcept
{
    ampAdsr.noteOff();
    filtAdsr.noteOff();
}

void VoiceLayer::updateAdsrTargets(int layerIndex, const ParamPointers& p) noexcept
{
    const float aA = readP(p.layerAA[layerIndex], 0.1f);
    const float aD = readP(p.layerAD[layerIndex], 0.1f);
    const float aS = readP(p.layerAS[layerIndex], 0.8f);
    const float aR = readP(p.layerAR[layerIndex], 0.2f);
    if (aA != cachedAmpA || aD != cachedAmpD || aS != cachedAmpS || aR != cachedAmpR)
    {
        ampAdsr.setParameters(aA, aD, aS, aR);
        cachedAmpA = aA;
        cachedAmpD = aD;
        cachedAmpS = aS;
        cachedAmpR = aR;
    }

    const float fA = readP(p.filterAdsrA, 0.1f);
    const float fD = readP(p.filterAdsrD, 0.1f);
    const float fS = readP(p.filterAdsrS, 0.7f);
    const float fR = readP(p.filterAdsrR, 0.15f);
    if (fA != cachedFilA || fD != cachedFilD || fS != cachedFilS || fR != cachedFilR)
    {
        filtAdsr.setParameters(fA, fD, fS, fR);
        cachedFilA = fA;
        cachedFilD = fD;
        cachedFilS = fS;
        cachedFilR = fR;
    }
}

double VoiceLayer::readWavetable(double ph) const noexcept
{
    if (wtData == nullptr || wtLen < 2)
        return 0.0;
    while (ph >= 1.0)
        ph -= 1.0;
    while (ph < 0.0)
        ph += 1.0;
    const double x = ph * (double)(wtLen - 1);
    const int i0 = (int)x;
    const int i1 = juce::jmin(i0 + 1, wtLen - 1);
    const double f = x - (double)i0;
    return wtData[i0] * (1.0 - f) + wtData[i1] * f;
}

void VoiceLayer::spawnGrain() noexcept
{
    for (auto& g : grains)
    {
        if (!g.active)
        {
            g.active = true;
            g.pos = (double)(grainSpawnCounter % 9973) / 9973.0 * (double)juce::jmax(1, granLen - 1);
            const double lenMs = 35.0 + (double)(grainSpawnCounter % 80);
            const double lenSamp = juce::jlimit(32.0, (double)granLen * 0.5, lenMs * 0.001 * sr);
            g.winInc = 1.0 / lenSamp;
            g.winPos = 0;
            g.speed = 1.0 + 0.02 * std::sin((double)grainSpawnCounter * 0.1);
            g.amp = 0.35;
            return;
        }
    }
}

double VoiceLayer::processGranular(double hz) noexcept
{
    juce::ignoreUnused(hz);
    grainSpawnCounter++;
    const int interval = juce::jmax(1, (int)(sr / 42.0));
    if ((grainSpawnCounter % interval) == 0)
        spawnGrain();

    double sum = 0;
    for (auto& g : grains)
    {
        if (!g.active)
            continue;
        const double base = std::floor(g.pos);
        const int i0 = ((int)base) % granLen;
        const int i1 = (i0 + 1) % granLen;
        const double frac = g.pos - base;
        const double s = granSource[i0] * (1.0 - frac) + granSource[i1] * frac;
        const double w = 0.5 - 0.5 * std::cos(dsp::twoPi * std::min(1.0, g.winPos));
        sum += s * w * g.amp;
        g.pos += g.speed * (double)granLen / (sr * 0.35);
        while (g.pos >= granLen)
            g.pos -= granLen;
        while (g.pos < 0)
            g.pos += granLen;
        g.winPos += g.winInc;
        if (g.winPos >= 1.0)
            g.active = false;
    }
    return sum;
}

double VoiceLayer::processOscillator(int oscType, double hz, int layerIdx, const ParamPointers& p) noexcept
{
    const int nv = juce::jlimit(1, 8, (int)std::lround(readP(p.layerUnisonVoices[layerIdx], 1.f)));
    const double detMax = (double)readP(p.layerUnisonDetune[layerIdx], 25.f);
    const float spN = readP(p.layerUnisonSpread[layerIdx], 0.5f);
    const double spreadMul = 0.25 + 1.75 * (double)std::clamp(spN, 0.f, 1.f);
    const double detNominalHalf = 0.5 * detMax * spreadMul;

    auto centOffset = [&](int v, int nVoices) -> double {
        if (nVoices <= 1)
            return 0.0;
        const double span = 2.0 * detNominalHalf;
        const int denom = juce::jmax(1, nVoices - 1);
        return (v - 0.5 * (double)(nVoices - 1)) * (span / (double)denom);
    };

    const double inc = hz / sr;
    const double fmIndex = (double)readP(p.layerRes[layerIdx], 1.f) * 0.35;

    switch (oscType)
    {
        case 0: // sine
        {
            if (nv <= 1)
            {
                phaseSin += inc;
                if (phaseSin >= 1.0)
                    phaseSin -= 1.0;
                return std::sin(dsp::twoPi * phaseSin);
            }
            double acc = 0;
            for (int v = 0; v < nv; ++v)
            {
                const double cents = centOffset(v, nv);
                const double hzv = hz * std::pow(2.0, cents / 1200.0);
                const double incv = hzv / sr;
                uniSin[(size_t)v] += incv;
                if (uniSin[(size_t)v] >= 1.0)
                    uniSin[(size_t)v] -= 1.0;
                acc += std::sin(dsp::twoPi * uniSin[(size_t)v]);
            }
            return acc / (double)nv;
        }
        case 1: // saw
        {
            if (nv <= 1)
            {
                phaseSaw += inc;
                if (phaseSaw >= 1.0)
                    phaseSaw -= 1.0;
                return dsp::blepSaw(phaseSaw, inc);
            }
            double acc = 0;
            for (int v = 0; v < nv; ++v)
            {
                const double cents = centOffset(v, nv);
                const double hzv = hz * std::pow(2.0, cents / 1200.0);
                const double incv = hzv / sr;
                uniSaw[(size_t)v] += incv;
                if (uniSaw[(size_t)v] >= 1.0)
                    uniSaw[(size_t)v] -= 1.0;
                acc += dsp::blepSaw(uniSaw[(size_t)v], incv);
            }
            return acc / (double)nv;
        }
        case 2: // square
        {
            if (nv <= 1)
            {
                phaseSq += inc;
                if (phaseSq >= 1.0)
                    phaseSq -= 1.0;
                return dsp::blepSquare(phaseSq, inc);
            }
            double acc = 0;
            for (int v = 0; v < nv; ++v)
            {
                const double cents = centOffset(v, nv);
                const double hzv = hz * std::pow(2.0, cents / 1200.0);
                const double incv = hzv / sr;
                uniSq[(size_t)v] += incv;
                if (uniSq[(size_t)v] >= 1.0)
                    uniSq[(size_t)v] -= 1.0;
                acc += dsp::blepSquare(uniSq[(size_t)v], incv);
            }
            return acc / (double)nv;
        }
        case 3: // triangle
        {
            if (nv <= 1)
            {
                phaseSq += inc;
                if (phaseSq >= 1.0)
                    phaseSq -= 1.0;
                return dsp::blepTriangle(triIntegrator, phaseSq, inc);
            }
            double acc = 0;
            for (int v = 0; v < nv; ++v)
            {
                const double cents = centOffset(v, nv);
                const double hzv = hz * std::pow(2.0, cents / 1200.0);
                const double incv = hzv / sr;
                uniSq[(size_t)v] += incv;
                if (uniSq[(size_t)v] >= 1.0)
                    uniSq[(size_t)v] -= 1.0;
                acc += dsp::blepTriangle(uniTri[(size_t)v], uniSq[(size_t)v], incv);
            }
            return acc / (double)nv;
        }
        case 4: // wavetable
        {
            phaseWt += inc;
            if (phaseWt >= 1.0)
                phaseWt -= 1.0;
            return readWavetable(phaseWt);
        }
        case 5: // granular
            return processGranular(hz);
        case 6: // FM
        {
            phaseFmMod += inc * 2.0;
            if (phaseFmMod >= 1.0)
                phaseFmMod -= 1.0;
            phaseFmCar += inc;
            if (phaseFmCar >= 1.0)
                phaseFmCar -= 1.0;
            const double mod = fmIndex * std::sin(dsp::twoPi * phaseFmMod);
            return std::sin(dsp::twoPi * phaseFmCar + mod);
        }
        case 7: // sample (octave-up table playback)
        {
            phaseSmpl += inc * 2.0;
            if (phaseSmpl >= 1.0)
                phaseSmpl -= 1.0;
            return readWavetable(phaseSmpl);
        }
        default:
            return 0.0;
    }
}

void VoiceLayer::updateFilterCoeffs(int filterType, double cutoffHz, double resonance, double modSemitones) noexcept
{
    combDelaySamples = 0;
    double fc = cutoffHz * std::pow(2.0, modSemitones / 12.0);
    fc = juce::jlimit(40.0, sr * 0.45, fc);
    const double Q = juce::jlimit(0.5, 18.0, resonance * 0.12 + 0.55);

    auto bypassF2 = [&] {
        f2.b0 = 1;
        f2.b1 = f2.b2 = f2.a1 = f2.a2 = 0;
        f2.z1 = f2.z2 = 0;
    };

    switch (filterType)
    {
        case 0: // LP24
            f1.setLowPass(sr, fc, Q);
            f2.setLowPass(sr, fc, Q * 0.92);
            break;
        case 1: // LP12
            f1.setLowPass(sr, fc, Q);
            bypassF2();
            break;
        case 2: // BP
            f1.setBandPass(sr, fc, Q);
            bypassF2();
            break;
        case 3: // HP12
            f1.setHighPass(sr, fc, Q);
            bypassF2();
            break;
        case 4: // Notch
            f1.setNotch(sr, fc, Q);
            bypassF2();
            break;
        case 5: // HP24
            f1.setHighPass(sr, fc, Q);
            f2.setHighPass(sr, fc, Q * 0.92);
            break;
        case 6: // Comb (delay + feedback; coeffs unused)
        {
            combDelaySamples = (int)(sr / juce::jmax(50.0, fc));
            combDelaySamples = juce::jlimit(2, 1024, combDelaySamples);
            combFeedback = juce::jlimit(0.0, 0.92, (resonance / 42.0) * 0.9);
            setBiquadIdentity(f1);
            setBiquadIdentity(f2);
            f1.z1 = f1.z2 = f2.z1 = f2.z2 = 0;
            break;
        }
        case 7: // Formant (dual band-pass vowel morph)
        {
            const double vowel = juce::jlimit(0.0, 1.0, (fc - 150.0) / 8000.0);
            const double f1f = 320.0 + vowel * 720.0;
            const double f2f = 900.0 + vowel * 2200.0;
            const double q = juce::jlimit(2.0, 14.0, 3.0 + resonance * 0.22);
            f1.setBandPass(sr, f1f, q);
            f2.setBandPass(sr, f2f, q * 0.92);
            combDelaySamples = 0;
            break;
        }
        default:
            combDelaySamples = 0;
            f1.setLowPass(sr, fc, Q);
            bypassF2();
            break;
    }
}

void VoiceLayer::renderAdd(double& outL,
                           double& outR,
                           int layerIndex,
                           int midiNote,
                           float velocity,
                           double globalLfoValue,
                           const ParamPointers& p,
                           float modMatrixCutSemi,
                           float modMatrixResAdd,
                           float modMatrixPitchSemi,
                           float modMatrixAmpMul,
                           float modMatrixPanAdd) noexcept
{
    if (ampAdsr.stage == dsp::Adsr::Stage::idle)
        return;

    updateAdsrTargets(layerIndex, p);

    // Master pitch (global) + per-layer coarse + mod matrix pitch (vibrato)
    const double masterPitchSemi = (double)readP(p.masterPitch, 0.f);
    const int coarse = (int)std::lround(readP(p.layerCoarse[layerIndex], 0.f));
    const double fineCents = (double)readP(p.layerFine[layerIndex], 0.f);

    // Combine integer semitone offsets; fractional mod pitch handled via pow(2, semis/12)
    double hz = dsp::midiNoteToHz(midiNote + coarse + (int)std::lround(masterPitchSemi));
    hz *= std::pow(2.0, fineCents / 1200.0);
    // Apply mod matrix pitch modulation (vibrato) as a fractional semitone multiplier
    if (modMatrixPitchSemi != 0.f)
        hz *= std::pow(2.0, (double)modMatrixPitchSemi / 12.0);

    const int oscType = (int)readP(p.layerOsc[layerIndex], 0.f);
    const double osc = processOscillator(oscType, hz, layerIndex, p);

    const double ampEnv = ampAdsr.process();
    const double filtEnv = filtAdsr.process();

    const int lfoSh = (int)readP(p.lfoShape, 0.f);
    lfoLayer.setShape(juce::jlimit(0, 5, lfoSh));
    const float lfoDep = readP(p.lfoDepth, 0.f);
    const double layerLfo = lfoLayer.tick(0.23 + 0.11 * (double)layerIndex);

    const int lfo2Sh = (int)readP(p.layerLfo2Shape[layerIndex], 0.f);
    lfoLayer2.setShape(juce::jlimit(0, 5, lfo2Sh));
    const float lfo2DepN = readP(p.layerLfo2Depth[layerIndex], 0.f);
    const double lfo2Hz = (double)readP(p.layerLfo2Rate[layerIndex], 2.f);
    const double layerLfo2 = lfoLayer2.tick(lfo2Hz);

    const double baseCut = (double)readP(p.layerCutoff[layerIndex], 5000.f);
    const double resCtrl = (double)readP(p.layerRes[layerIndex], 1.f) + (double)modMatrixResAdd;
    const double modSemi = filtEnv * 0.0 + globalLfoValue * 14.0 * (double)lfoDep + layerLfo * 10.0
                           + (double)modMatrixCutSemi + layerLfo2 * 10.0 * (double)lfo2DepN;

    const int ftype = juce::jlimit(0, 7, (int)readP(p.layerFtype[layerIndex], 0.f));
    updateFilterCoeffs(ftype, baseCut, resCtrl, modSemi);

    const bool parallel = (int)readP(p.layerFilterRoute[layerIndex], 0.f) == 1;

    double y = 0;
    if (ftype == 6 && combDelaySamples > 0)
    {
        const int d = combDelaySamples;
        const int wp = combWritePos;
        const int rp = (wp - d + 2048) % 2048;
        const double delayed = combBuf[(size_t)rp];
        y = osc + combFeedback * delayed;
        combBuf[(size_t)wp] = y;
        combWritePos = (wp + 1) % 2048;
    }
    else if (ftype == 7)
        y = f2.process(f1.process(osc));
    else if (parallel)
        y = 0.5 * (f1.process(osc) + f2.process(osc));
    else
        y = f2.process(f1.process(osc));

    double sig = y * (double)velocity * ampEnv;

    const float vol = readP(p.layerVol[layerIndex], 0.75f);
    const float pan = readP(p.layerPan[layerIndex], 0.5f);
    sig *= (double)vol;

    // Apply mod matrix amplitude modulation (tremolo): multiplicative, clamped [0, 2]
    sig *= (double)juce::jlimit(0.f, 2.f, modMatrixAmpMul);

    // Apply mod matrix pan modulation (auto-pan): additive bipolar offset clamped to [-1, 1]
    const double panBip = std::clamp((double)pan * 2.0 - 1.0 + (double)modMatrixPanAdd, -1.0, 1.0);
    const double pan01 = (panBip + 1.0) * 0.5;
    const double angle = pan01 * dsp::twoPi * 0.25;
    outL += sig * std::cos(angle);
    outR += sig * std::sin(angle);

    lastAmpEnvOut = (float)ampEnv;
    lastFiltEnvOut = (float)filtEnv;
}

} // namespace wolfsden
