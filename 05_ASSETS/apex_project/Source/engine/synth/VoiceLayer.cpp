#include "VoiceLayer.h"

#include <juce_core/juce_core.h>

#include <cmath>

namespace wolfsden
{
namespace
{
inline double denormTime(float n01, double lo, double hi) noexcept
{
    return lo + (double)n01 * (hi - lo);
}

inline int denormChoice(float n01, int numChoices) noexcept
{
    if (numChoices <= 1)
        return 0;
    const int maxIdx = numChoices - 1;
    const double x = (double)std::clamp(n01, 0.f, 1.f) * (double)numChoices - 1e-9;
    return (int)std::clamp(x, 0.0, (double)maxIdx);
}

inline double denormCutoffSkewed(float n01) noexcept
{
    return 20.0 * std::pow(20000.0 / 20.0, (double)std::clamp(n01, 0.f, 1.f));
}

inline double denormRes(float n01) noexcept
{
    return 0.1 + (double)std::clamp(n01, 0.f, 1.f) * 39.9;
}
} // namespace

float VoiceLayer::readP(std::atomic<float>* ap, float defV) noexcept
{
    return ap ? ap->load(std::memory_order_relaxed) : defV;
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
}

void VoiceLayer::reset() noexcept
{
    phaseSin = phaseSaw = phaseSq = triIntegrator = 0;
    phaseWt = phaseSmpl = phaseFmCar = phaseFmMod = 0;
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
        ampAdsr.setParameters(denormTime(aA, 0.001, 10.0),
                              denormTime(aD, 0.001, 10.0),
                              (double)std::clamp(aS, 0.f, 1.f),
                              denormTime(aR, 0.001, 15.0));
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
        filtAdsr.setParameters(denormTime(fA, 0.001, 10.0),
                               denormTime(fD, 0.001, 10.0),
                               (double)std::clamp(fS, 0.f, 1.f),
                               denormTime(fR, 0.001, 15.0));
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
    const double inc = hz / sr;
    const float resN = readP(p.layerRes[layerIdx], 0.3f);
    const double fmIndex = denormRes(resN) * 0.35;

    switch (oscType)
    {
        case 0: // sine
        {
            phaseSin += inc;
            if (phaseSin >= 1.0)
                phaseSin -= 1.0;
            return std::sin(dsp::twoPi * phaseSin);
        }
        case 1: // saw
        {
            phaseSaw += inc;
            if (phaseSaw >= 1.0)
                phaseSaw -= 1.0;
            return dsp::blepSaw(phaseSaw, inc);
        }
        case 2: // square
        {
            phaseSq += inc;
            if (phaseSq >= 1.0)
                phaseSq -= 1.0;
            return dsp::blepSquare(phaseSq, inc);
        }
        case 3: // triangle
        {
            phaseSq += inc;
            if (phaseSq >= 1.0)
                phaseSq -= 1.0;
            return dsp::blepTriangle(triIntegrator, phaseSq, inc);
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
        default:
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
                           float modMatrixResAdd) noexcept
{
    if (ampAdsr.stage == dsp::Adsr::Stage::idle)
        return;

    updateAdsrTargets(layerIndex, p);

    const double mPitchSemi = denormTime(readP(p.masterPitch, 0.5f), -48.0, 48.0);
    const int coarse = (int)std::lround(denormTime(readP(p.layerCoarse[layerIndex], 0.5f), -24.0, 24.0));
    const float fineN = readP(p.layerFine[layerIndex], 0.5f);
    const double fineCents = denormTime(fineN, -100.0, 100.0);

    double hz = dsp::midiNoteToHz(midiNote + coarse + (int)std::lround(mPitchSemi));
    hz *= std::pow(2.0, fineCents / 1200.0);

    const int oscType = denormChoice(readP(p.layerOsc[layerIndex], 0.f), 8);
    const double osc = processOscillator(oscType, hz, layerIndex, p);

    const double ampEnv = ampAdsr.process();
    const double filtEnv = filtAdsr.process();

    const int lfoSh = denormChoice(readP(p.lfoShape, 0.f), 6);
    lfoLayer.setShape(juce::jlimit(0, 5, lfoSh));
    const float lfoDep = readP(p.lfoDepth, 0.f);
    const double layerLfo = lfoLayer.tick(0.23 + 0.11 * (double)layerIndex);

    const double baseCut = denormCutoffSkewed(readP(p.layerCutoff[layerIndex], 0.5f));
    const double resCtrl = denormRes(readP(p.layerRes[layerIndex], 0.2f)) + (double)modMatrixResAdd;
    const double modSemi = filtEnv * 36.0 + globalLfoValue * 14.0 * (double)lfoDep + layerLfo * 10.0
                           + (double)modMatrixCutSemi;

    const int ftype = denormChoice(readP(p.layerFtype[layerIndex], 0.f), 6);
    updateFilterCoeffs(juce::jlimit(0, 5, ftype), baseCut, resCtrl, modSemi);

    const double x1 = f1.process(osc);
    const double y = f2.process(x1);

    double sig = y * (double)velocity * ampEnv;

    const float vol = readP(p.layerVol[layerIndex], 0.75f);
    const float pan = readP(p.layerPan[layerIndex], 0.5f);
    sig *= (double)vol;

    const double panBip = std::clamp((double)pan * 2.0 - 1.0, -1.0, 1.0);
    const double pan01 = (panBip + 1.0) * 0.5;
    const double angle = pan01 * dsp::twoPi * 0.25;
    outL += sig * std::cos(angle);
    outR += sig * std::sin(angle);

    lastAmpEnvOut = (float)ampEnv;
    lastFiltEnvOut = (float)filtEnv;
}

} // namespace wolfsden
