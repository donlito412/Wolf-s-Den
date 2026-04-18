#include "VoiceLayer.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include <cmath>

namespace wolfsden
{
namespace
{
inline int readChoiceIndex(std::atomic<float>* ap, int numItems, int defIdx) noexcept
{
    if (!ap || numItems < 2)
        return defIdx;
    const float v = ap->load(std::memory_order_relaxed);
    if (v >= 0.f && v <= 1.0001f)
        return juce::jlimit(0, numItems - 1, (int)(v * (float)numItems * 0.9999f));
    return juce::jlimit(0, numItems - 1, (int)std::lround(v));
}

inline int readIntFromNorm(std::atomic<float>* ap, int minV, int maxV, int defV) noexcept
{
    if (!ap || maxV < minV)
        return defV;
    const float v = ap->load(std::memory_order_relaxed);
    if (v >= 0.f && v <= 1.0001f && maxV > minV)
        return juce::jlimit(minV, maxV, minV + (int)std::lround(v * (float)(maxV - minV)));
    return juce::jlimit(minV, maxV, (int)std::lround(v));
}

inline float readFloatAP(std::atomic<float>* ap, float defV) noexcept
{
    if (!ap)
        return defV;
    const float v = ap->load(std::memory_order_relaxed);
    {
    }
    return v;
}

inline uint32_t rngU32(int salt) noexcept
{
    uint32_t x = (uint32_t)(salt * 1103515245 + 12345);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}
} // namespace

bool VoiceLayer::readBoolNorm(std::atomic<float>* ap, bool defV) noexcept
{
    if (!ap)
        return defV;
    return ap->load(std::memory_order_relaxed) > 0.5f;
}

double VoiceLayer::beatsForSyncDivIndex(int divIdx) noexcept
{
    static constexpr double b[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    return b[juce::jlimit(0, 8, divIdx)];
}

void VoiceLayer::setBiquadIdentity(dsp::Biquad& b) noexcept
{
    b.b0 = 1.0;
    b.b1 = b.b2 = b.a1 = b.a2 = 0.0;
}

void VoiceLayer::prepare(double sampleRate,
                         const double* wavetableA,
                         int wavetableSizeA,
                         const double* wavetableB,
                         int wavetableSizeB,
                         const double* granularSource,
                         int granularSize,
                         int maxBlockSize) noexcept
{
    sr = sampleRate;
    wtData = wavetableA;
    wtBData = wavetableB != nullptr ? wavetableB : wavetableA;
    wtLen = juce::jmax(8, wavetableSizeA);
    juce::ignoreUnused(wavetableSizeB);
    granSource = granularSource;
    granLen = juce::jmax(8, granularSize);
    ampAdsr.setSampleRate(sr);
    filtAdsr.setSampleRate(sr);
    lfoLayer.setSampleRate(sr);
    lfoLayer.setShape(0);
    lfoLayer2.setSampleRate(sr);
    lfoLayer2.setShape(0);
    samplePlayer.prepare(sampleRate, maxBlockSize);
}

void VoiceLayer::reset() noexcept
{
    phaseSin = phaseSaw = phaseSq = triIntegrator = 0;
    phaseWt = phaseSmpl = phaseFmCar = phaseFmMod = 0;
    noiseOscState = 0x12345678u;
    combBuf1.fill(0.0);
    combBuf2.fill(0.0);
    combWritePos1 = combWritePos2 = 0;
    combDelaySamples1 = combDelaySamples2 = 0;
    combFeedback1 = combFeedback2 = 0.0;
    uniSin.fill(0.0);
    uniSaw.fill(0.0);
    uniSq.fill(0.0);
    uniTri.fill(0.0);
    for (auto& g : grains)
        g.active = false;
    grainSpawnCounter = 0;
    granPlayhead = 0;
    frozenPlayhead = 0;
    hadFreezeOn = false;
    ampAdsr.reset();
    filtAdsr.reset();
    f1.reset();
    f1s.reset();
    f2.reset();
    f2s.reset();
    cachedAmpA = cachedAmpD = cachedAmpS = cachedAmpR = -1.f;
    cachedFilA = cachedFilD = cachedFilS = cachedFilR = -1.f;
    smoothedVol = -1.0;
    smoothedPan = 0.0;
    smoothedCut1 = smoothedCut2 = -1.0;
    lfo2DelayRem = 0;
    lfo2FadeTotal = lfo2FadeProg = 0;
    glideHz = glideTargetHz = 440.0;
}

static double computeTargetHz(int midiNote,
                              int layerIndex,
                              const ParamPointers& p,
                              float modMatrixPitchSemi) noexcept
{
    const double masterPitchSemi = (double)readFloatAP(p.masterPitch, 0.f);
    const int coarse = readIntFromNorm(p.layerCoarse[layerIndex], -24, 24, 0);
    const int octave = readIntFromNorm(p.layerOctave[layerIndex], -3, 3, 0);
    const double fineCents = (double)readFloatAP(p.layerFine[layerIndex], 0.f);
    double hz = dsp::midiNoteToHz(midiNote + coarse + octave * 12 + (int)std::lround(masterPitchSemi));
    hz *= std::pow(2.0, fineCents / 1200.0);
    if (modMatrixPitchSemi != 0.f)
        hz *= std::pow(2.0, (double)modMatrixPitchSemi / 12.0);
    return hz;
}

void VoiceLayer::noteOn(int midiNote, float velocity, int layerIndex, const ParamPointers& p) noexcept
{
    currentNote = midiNote;
    currentVel = velocity;
    filterUpdateTick_ = kFilterUpdateInterval; // force immediate filter recalc on next render
    phaseSin = phaseSaw = phaseSq = phaseWt = phaseSmpl = phaseFmCar = phaseFmMod = 0;
    triIntegrator = 0;
    uniSin.fill(0.0);
    uniSaw.fill(0.0);
    uniSq.fill(0.0);
    uniTri.fill(0.0);
    ampAdsr.noteOn();
    filtAdsr.noteOn();
    glideTargetHz = computeTargetHz(midiNote, layerIndex, p, 0.f);
    glideHz = glideTargetHz;

    samplePlayer.syncPendingToCache();
    if (samplePlayer.hasReadableSource())
    {
        const float pitchRatio = std::pow(2.f, (float)(midiNote - samplePlayer.rootNote()) / 12.f);
        samplePlayer.startNote(pitchRatio, velocity);
    }

    const float l2d = readFloatAP(p.layerLfo2Delay[layerIndex], 0.f);
    const float l2f = readFloatAP(p.layerLfo2Fade[layerIndex], 0.f);
    lfo2DelayRem = (double)l2d * sr;
    lfo2FadeTotal = (double)l2f * sr;
    lfo2FadeProg = 0;
    if (readBoolNorm(p.layerLfo2Retrigger[layerIndex], false))
        lfoLayer2.phase = 0;
}

void VoiceLayer::noteOnLegato(int midiNote, float velocity, int layerIndex, const ParamPointers& p) noexcept
{
    currentNote = midiNote;
    currentVel = velocity;
    glideTargetHz = computeTargetHz(midiNote, layerIndex, p, 0.f);
    samplePlayer.syncPendingToCache();
    if (samplePlayer.hasReadableSource())
    {
        const float pitchRatio = std::pow(2.f, (float)(midiNote - samplePlayer.rootNote()) / 12.f);
        samplePlayer.startNote(pitchRatio, velocity);
    }
    if (readBoolNorm(p.layerLfo2Retrigger[layerIndex], false))
    {
        lfoLayer2.phase = 0;
        const float l2d = readFloatAP(p.layerLfo2Delay[layerIndex], 0.f);
        const float l2f = readFloatAP(p.layerLfo2Fade[layerIndex], 0.f);
        lfo2DelayRem = (double)l2d * sr;
        lfo2FadeTotal = (double)l2f * sr;
        lfo2FadeProg = 0;
    }
}

void VoiceLayer::noteOnSteal(int midiNote,
                             float velocity,
                             int layerIndex,
                             const ParamPointers& p,
                             double ampEnvLevel,
                             double filtEnvLevel) noexcept
{
    currentNote = midiNote;
    currentVel = velocity;
    filterUpdateTick_ = kFilterUpdateInterval; // force immediate filter recalc on next render
    ampAdsr.noteOnFromLevel(ampEnvLevel * 0.92);
    filtAdsr.noteOnFromLevel(filtEnvLevel * 0.92);
    glideTargetHz = computeTargetHz(midiNote, layerIndex, p, 0.f);
    glideHz = glideTargetHz;
    samplePlayer.syncPendingToCache();
    if (samplePlayer.hasReadableSource())
    {
        const float pitchRatio = std::pow(2.f, (float)(midiNote - samplePlayer.rootNote()) / 12.f);
        samplePlayer.startNote(pitchRatio, velocity);
    }
    const float l2d = readFloatAP(p.layerLfo2Delay[layerIndex], 0.f);
    const float l2f = readFloatAP(p.layerLfo2Fade[layerIndex], 0.f);
    lfo2DelayRem = (double)l2d * sr;
    lfo2FadeTotal = (double)l2f * sr;
    lfo2FadeProg = 0;
    if (readBoolNorm(p.layerLfo2Retrigger[layerIndex], false))
        lfoLayer2.phase = 0;
}

void VoiceLayer::noteOff() noexcept
{
    ampAdsr.noteOff();
    filtAdsr.noteOff();
    samplePlayer.stopNote();
}

void VoiceLayer::updateAdsrTargets(int layerIndex, const ParamPointers& p) noexcept
{
    const float aA = readFloatAP(p.layerAA[layerIndex], 0.01f);
    const float aD = readFloatAP(p.layerAD[layerIndex], 0.1f);
    const float aS = readFloatAP(p.layerAS[layerIndex], 0.8f);
    const float aR = readFloatAP(p.layerAR[layerIndex], 0.2f);
    const float ampEpsilon = 1e-6f;
    if (std::fabs(aA - cachedAmpA) > ampEpsilon || std::fabs(aD - cachedAmpD) > ampEpsilon
        || std::fabs(aS - cachedAmpS) > ampEpsilon || std::fabs(aR - cachedAmpR) > ampEpsilon)
    {
        ampAdsr.setParameters(aA, aD, aS, aR);
        cachedAmpA = aA;
        cachedAmpD = aD;
        cachedAmpS = aS;
        cachedAmpR = aR;
    }

    const float fA = readFloatAP(p.filterAdsrA, 0.01f);
    const float fD = readFloatAP(p.filterAdsrD, 0.1f);
    const float fS = readFloatAP(p.filterAdsrS, 0.7f);
    const float fR = readFloatAP(p.filterAdsrR, 0.15f);
    const float filtEpsilon = 1e-6f;
    if (std::fabs(fA - cachedFilA) > filtEpsilon || std::fabs(fD - cachedFilD) > filtEpsilon
        || std::fabs(fS - cachedFilS) > filtEpsilon || std::fabs(fR - cachedFilR) > filtEpsilon)
    {
        filtAdsr.setParameters(fA, fD, fS, fR);
        cachedFilA = fA;
        cachedFilD = fD;
        cachedFilS = fS;
        cachedFilR = fR;
    }
}

double VoiceLayer::readWavetableMorph(double ph, float morph01) const noexcept
{
    if (wtData == nullptr || wtLen < 2)
        return 0.0;
    auto rd = [](const double* d, int len, double phase) noexcept {
        while (phase >= 1.0)
            phase -= 1.0;
        while (phase < 0.0)
            phase += 1.0;
        const double x = phase * (double)(len - 1);
        const int i0 = (int)x;
        const int i1 = juce::jmin(i0 + 1, len - 1);
        const double f = x - (double)i0;
        return d[i0] * (1.0 - f) + d[i1] * f;
    };
    const double va = rd(wtData, wtLen, ph);
    const double vb = rd(wtBData != nullptr ? wtBData : wtData, wtLen, ph);
    const double t = juce::jlimit(0.0, 1.0, (double)morph01);
    return va * (1.0 - t) + vb * t;
}

double VoiceLayer::processOscillator(int oscType, double hz, int layerIdx, const ParamPointers& p) noexcept
{
    const int nv = readIntFromNorm(p.layerUnisonVoices[layerIdx], 1, 8, 1);
    const double detMax = (double)readFloatAP(p.layerUnisonDetune[layerIdx], 25.f);
    const float spN = readFloatAP(p.layerUnisonSpread[layerIdx], 0.5f);
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
    const double fmIndex = (double)readFloatAP(p.layerRes[layerIdx], 1.f) * 0.35;
    const float morphN = readFloatAP(p.layerWtMorph[layerIdx], 0.f);

    switch (oscType)
    {
        case 0: {
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
        case 1: {
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
        case 2: {
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
        case 3: {
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
        case 4: {
            phaseWt += inc;
            if (phaseWt >= 1.0)
                phaseWt -= 1.0;
            return readWavetableMorph(phaseWt, morphN);
        }
        case 5:
            return processGranular(hz, layerIdx, p, samplePlayer.hasReadableSource());
        case 6: {
            phaseFmMod += inc * 2.0;
            if (phaseFmMod >= 1.0)
                phaseFmMod -= 1.0;
            phaseFmCar += inc;
            if (phaseFmCar >= 1.0)
                phaseFmCar -= 1.0;
            const double mod = fmIndex * std::sin(dsp::twoPi * phaseFmMod);
            return std::sin(dsp::twoPi * phaseFmCar + mod);
        }
        case 7: {
            if (samplePlayer.hasReadableSource() && samplePlayer.isActive())
                return (double)samplePlayer.nextSampleMono();
            return 0.0;
        }
        case 8:
            return (double)dsp::xorshift32(noiseOscState) * 0.35;
        default:
            return 0.0;
    }
}

void VoiceLayer::configureFilterBank(dsp::Biquad& f,
                                     dsp::Biquad& fs,
                                     int filterType,
                                     double cutoffHz,
                                     double resonance,
                                     double modSemitones,
                                     int& combDelay,
                                     double& combFb,
                                     int& combWritePos,
                                     std::array<double, 2048>& combBuf) noexcept
{
    juce::ignoreUnused(combBuf);
    juce::ignoreUnused(combWritePos);
    combDelay = 0;
    double fc = cutoffHz * std::pow(2.0, modSemitones / 12.0);
    fc = juce::jlimit(40.0, sr * 0.45, fc);
    const double Q = juce::jlimit(0.5, 8.0, resonance * 0.08 + 0.55);

    auto bypass = [&](dsp::Biquad& b) {
        setBiquadIdentity(b);
        b.z1 = b.z2 = 0;
    };

    switch (filterType)
    {
        case 0:
            f.setLowPass(sr, fc, Q);
            fs.setLowPass(sr, fc, Q * 0.92);
            break;
        case 1:
            f.setLowPass(sr, fc, Q);
            bypass(fs);
            break;
        case 2:
            f.setBandPass(sr, fc, Q);
            bypass(fs);
            break;
        case 3:
            f.setHighPass(sr, fc, Q);
            bypass(fs);
            break;
        case 4:
            f.setNotch(sr, fc, Q);
            bypass(fs);
            break;
        case 5:
            f.setHighPass(sr, fc, Q);
            fs.setHighPass(sr, fc, Q * 0.92);
            break;
        case 6:
            combDelay = (int)(sr / juce::jmax(50.0, fc));
            combDelay = juce::jlimit(2, 1024, combDelay);
            combFb = juce::jlimit(0.0, 0.92, (resonance / 42.0) * 0.9);
            setBiquadIdentity(f);
            setBiquadIdentity(fs);
            f.z1 = f.z2 = fs.z1 = fs.z2 = 0;
            break;
        case 7: {
            const double vowel = juce::jlimit(0.0, 1.0, (fc - 150.0) / 8000.0);
            const double f1f = 320.0 + vowel * 720.0;
            const double f2f = 900.0 + vowel * 2200.0;
            const double q = juce::jlimit(2.0, 14.0, 3.0 + resonance * 0.22);
            f.setBandPass(sr, f1f, q);
            fs.setBandPass(sr, f2f, q * 0.92);
            break;
        }
        default:
            bypass(f);
            bypass(fs);
            break;
    }
}

void VoiceLayer::spawnGrain(int layerIdx, const ParamPointers& p, bool useSampleSource) noexcept
{
    const float posN = readFloatAP(p.layerGranPos[layerIdx], 0.5f);
    const float sizeN = readFloatAP(p.layerGranSize[layerIdx], 0.1f);
    const float scatN = readFloatAP(p.layerGranScatter[layerIdx], 0.f);
    const bool freeze = readBoolNorm(p.layerGranFreeze[layerIdx], false);

    const int span = useSampleSource ? samplePlayer.getLoopFrameCount() : granLen;
    const double basePos = freeze ? frozenPlayhead
                                   : (double)posN * (double)juce::jmax(1, span - 1);
    const uint32_t r = rngU32(grainSpawnCounter + layerIdx * 7919);
    const double jit = ((double)(r & 0xffff) / 32768.0 - 1.0) * scatN * 0.08 * (double)span;
    double gpos = basePos + jit;
    while (gpos < 0)
        gpos += span;
    while (gpos >= span)
        gpos -= span;

    const double lenMs = 1.0 + (double)sizeN * (double)sizeN * 499.0;
    const double lenSamp = juce::jlimit(sr * 0.001, sr * 0.5, lenMs * 0.001 * sr);

    for (auto& g : grains)
    {
        if (!g.active)
        {
            g.active = true;
            g.pos = gpos;
            g.winInc = 1.0 / lenSamp;
            g.winPos = 0;
            g.speed = std::pow(2.0, (((int)(r >> 8) % 513) / 512.0 - 0.5) * scatN * 0.04);
            g.amp = 0.4;
            g.pan = (((int)(r >> 16) % 513) / 512.0 - 0.5) * scatN;
            return;
        }
    }
}

double VoiceLayer::processGranular(double hz, int layerIdx, const ParamPointers& p, bool useSampleSource) noexcept
{
    juce::ignoreUnused(hz);
    const float densN = readFloatAP(p.layerGranDensity[layerIdx], 0.5f);
    const bool freeze = readBoolNorm(p.layerGranFreeze[layerIdx], false);
    if (freeze)
    {
        if (!hadFreezeOn)
        {
            const int span = useSampleSource ? samplePlayer.getLoopFrameCount() : granLen;
            frozenPlayhead = (double)readFloatAP(p.layerGranPos[layerIdx], 0.5f)
                             * (double)juce::jmax(1, span - 1);
        }
        hadFreezeOn = true;
    }
    else
    {
        hadFreezeOn = false;
    }

    grainSpawnCounter++;
    const int baseIv = juce::jmax(1, (int)(sr / (4.0 + (double)densN * 80.0)));
    if ((grainSpawnCounter % baseIv) == 0)
        spawnGrain(layerIdx, p, useSampleSource);

    double sumL = 0, sumR = 0;
    for (auto& g : grains)
    {
        if (!g.active)
            continue;
        float sMono = 0.f;
        if (useSampleSource)
        {
            if (!samplePlayer.readMonoAt(g.pos, sMono))
            {
                g.active = false;
                continue;
            }
        }
        else
        {
            const int i0 = ((int)std::floor(g.pos)) % granLen;
            const int i1 = (i0 + 1) % granLen;
            const double frac = g.pos - std::floor(g.pos);
            sMono = (float)(granSource[i0] * (1.0 - frac) + granSource[i1] * frac);
        }
        const double w = 0.5 - 0.5 * std::cos(dsp::twoPi * std::min(1.0, g.winPos));
        const double sig = (double)sMono * w * g.amp;
        const double ang = (g.pan + 1.0) * dsp::twoPi * 0.25;
        sumL += sig * std::cos(ang);
        sumR += sig * std::sin(ang);
        g.pos += g.speed;
        const int sp = useSampleSource ? samplePlayer.getLoopFrameCount() : granLen;
        while (g.pos >= sp)
            g.pos -= sp;
        while (g.pos < 0)
            g.pos += sp;
        g.winPos += g.winInc;
        if (g.winPos >= 1.0)
            g.active = false;
    }
    return (sumL + sumR) * 0.5;
}

static double applyDrive(double x, double drive01) noexcept
{
    const double g = 1.0 + drive01 * 8.0;
    return std::tanh(x * g) / std::tanh(juce::jmax(0.01, g));
}

static double processComb(double osc,
                          int d,
                          double fb,
                          int& wp,
                          std::array<double, 2048>& buf) noexcept
{
    if (d <= 0)
        return osc;
    const int w = wp;
    const int rp = (w - d + 2048) % 2048;
    const double delayed = buf[(size_t)rp];
    const double y = osc + fb * delayed;
    buf[(size_t)w] = y;
    wp = (w + 1) % 2048;
    return y;
}

static double processFilterPair(dsp::Biquad& f,
                              dsp::Biquad& fs,
                              int t,
                              double x,
                              int& combD,
                              double& combFb,
                              int& combWp,
                              std::array<double, 2048>& combBuf) noexcept
{
    if (t == 6 && combD > 0)
        return processComb(x, combD, combFb, combWp, combBuf);
    if (t == 7)
        return fs.process(f.process(x));
    if (t == 0 || t == 5)
        return fs.process(f.process(x));
    return f.process(x);
}

void VoiceLayer::renderAdd(double& outL,
                           double& outR,
                           int layerIndex,
                           int midiNote,
                           float velocity,
                           double globalLfoValue,
                           double hostBpm,
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

    glideTargetHz = computeTargetHz(midiNote, layerIndex, p, modMatrixPitchSemi);
    const float portSec = p.synthPortamento != nullptr
                              ? readFloatAP(p.synthPortamento, 0.f)
                              : 0.f;
    if (portSec <= 1e-5f)
        glideHz = glideTargetHz;
    else
    {
        const double a = 1.0 - std::exp(-1.0 / (juce::jmax(0.001, (double)portSec) * sr));
        glideHz += (glideTargetHz - glideHz) * a;
    }

    const int oscType = readChoiceIndex(p.layerOsc[layerIndex], 9, 0);
    const double osc = processOscillator(oscType, glideHz, layerIndex, p);

    const double ampEnv = ampAdsr.process();
    const double filtEnv = filtAdsr.process();

    const int lfoSh = readChoiceIndex(p.lfoShape, 8, 0);
    lfoLayer.setShape(juce::jlimit(0, 7, lfoSh));
    const float lfoDep = readFloatAP(p.lfoDepth, 0.f);
    const double layerLfo = lfoLayer.tick(0.23 + 0.11 * (double)layerIndex);

    const int lfo2Sh = readChoiceIndex(p.layerLfo2Shape[layerIndex], 8, 0);
    lfoLayer2.setShape(juce::jlimit(0, 7, lfo2Sh));
    const float lfo2DepN = readFloatAP(p.layerLfo2Depth[layerIndex], 0.f);
    const bool l2sync = readBoolNorm(p.layerLfo2Sync[layerIndex], false);
    double lfo2Hz = (double)readFloatAP(p.layerLfo2Rate[layerIndex], 2.f);
    if (l2sync && hostBpm > 20.0)
    {
        const int divi = readChoiceIndex(p.layerLfo2SyncDiv[layerIndex], 9, 4);
        const double beats = beatsForSyncDivIndex(divi);
        lfo2Hz = hostBpm / (60.0 * beats);
    }

    double l2v = 0;
    if (lfo2DelayRem > 0)
    {
        lfo2DelayRem -= 1.0;
        l2v = 0;
    }
    else
    {
        l2v = lfoLayer2.tick(lfo2Hz);
        if (lfo2FadeTotal > 0 && lfo2FadeProg < lfo2FadeTotal)
        {
            l2v *= lfo2FadeProg / juce::jmax(1.0, lfo2FadeTotal);
            lfo2FadeProg += 1.0;
            if (lfo2FadeProg >= lfo2FadeTotal)
                lfo2FadeTotal = 0;
        }
    }

    const double baseCut1 = (double)readFloatAP(p.layerCutoff[layerIndex], 5000.f);
    const double baseCut2 = (double)readFloatAP(p.layerF2cutoff[layerIndex], 20000.f);
    if (smoothedCut1 < 0)
        smoothedCut1 = baseCut1;
    if (smoothedCut2 < 0)
        smoothedCut2 = baseCut2;
    smoothedCut1 += (baseCut1 - smoothedCut1) * 0.005;
    smoothedCut2 += (baseCut2 - smoothedCut2) * 0.005;

    const float kt = readFloatAP(p.layerKeytrack[layerIndex], 0.f);
    const double ktSemi = (double)(midiNote - 60) * (double)kt * 0.12;
    const double cut1 = smoothedCut1 * std::pow(2.0, ktSemi / 12.0);
    const double cut2 = smoothedCut2 * std::pow(2.0, ktSemi / 12.0);

    const double res1 = (double)readFloatAP(p.layerRes[layerIndex], 1.f) + (double)modMatrixResAdd;
    const double res2 = (double)readFloatAP(p.layerF2res[layerIndex], 1.f)
                        + (double)modMatrixResAdd * 0.5;
    const double modSemi = filtEnv * 36.0 + globalLfoValue * 14.0 * (double)lfoDep + layerLfo * 10.0
                           + (double)modMatrixCutSemi + l2v * 10.0 * (double)lfo2DepN;

    const int t1 = readChoiceIndex(p.layerFtype[layerIndex], 8, 0);
    const int t2 = readChoiceIndex(p.layerF2type[layerIndex], 8, 0);

    // Only recalculate biquad coefficients every kFilterUpdateInterval samples.
    // configureFilterBank calls std::sin/std::cos per sample which is the main
    // CPU culprit at 3+ simultaneous notes (3 voices × 4 layers × 2 banks).
    // Updating at ~11 kHz (every 4 samples @ 44.1kHz) is more than sufficient
    // for envelopes and LFOs and causes no audible difference.
    if (++filterUpdateTick_ >= kFilterUpdateInterval)
    {
        filterUpdateTick_ = 0;
        configureFilterBank(f1, f1s, t1, cut1, res1, modSemi, combDelaySamples1, combFeedback1, combWritePos1, combBuf1);
        configureFilterBank(f2, f2s, t2, cut2, res2, modSemi, combDelaySamples2, combFeedback2, combWritePos2, combBuf2);
    }

    const double d1 = (double)readFloatAP(p.layerFilterDrive[layerIndex], 0.f);
    const double d2 = (double)readFloatAP(p.layerF2drive[layerIndex], 0.f);
    const double x0 = applyDrive(osc, d1);

    const bool parallel = readChoiceIndex(p.layerFilterRoute[layerIndex], 2, 0) == 1;

    double y = 0;
    if (parallel)
    {
        const double a = processFilterPair(f1, f1s, t1, x0, combDelaySamples1, combFeedback1, combWritePos1, combBuf1);
        const double b = processFilterPair(f2, f2s, t2, applyDrive(osc, d2), combDelaySamples2, combFeedback2, combWritePos2, combBuf2);
        y = 0.5 * (a + b);
    }
    else
    {
        const double a = processFilterPair(f1, f1s, t1, x0, combDelaySamples1, combFeedback1, combWritePos1, combBuf1);
        y = processFilterPair(f2, f2s, t2, applyDrive(a, d2), combDelaySamples2, combFeedback2, combWritePos2, combBuf2);
    }

    double sig = y * (double)velocity * ampEnv;

    const float vol = readFloatAP(p.layerVol[layerIndex], 0.75f);
    const float pan = readFloatAP(p.layerPan[layerIndex], 0.f);

    if (smoothedVol < 0)
        smoothedVol = (double)vol;
    smoothedVol += ((double)vol - smoothedVol) * 0.005;
    smoothedPan += ((double)pan - smoothedPan) * 0.005;

    sig *= smoothedVol;
    sig *= (double)juce::jlimit(0.f, 2.f, modMatrixAmpMul);

    if (!(sig >= -4.0 && sig <= 4.0))
        sig = 0.0;

    const double panBip = std::clamp(smoothedPan + (double)modMatrixPanAdd, -1.0, 1.0);
    // Fast linear equal-power-ish panning (avoids per-sample std::cos/std::sin)
    const double pan01 = (panBip + 1.0) * 0.5;
    const double gainR = pan01;
    const double gainL = 1.0 - pan01;
    outL += sig * gainL;
    outR += sig * gainR;

    lastAmpEnvOut = (float)ampEnv;
    lastFiltEnvOut = (float)filtEnv;
}

} // namespace wolfsden
