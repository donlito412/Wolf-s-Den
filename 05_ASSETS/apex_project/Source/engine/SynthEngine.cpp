#include "SynthEngine.h"

#include <algorithm>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

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

inline bool readBoolNorm(std::atomic<float>* ap, bool defV) noexcept
{
    if (!ap)
        return defV;
    return ap->load(std::memory_order_relaxed) > 0.5f;
}

inline double beatsForSyncDivIndex(int divIdx) noexcept
{
    static constexpr double b[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    return b[juce::jlimit(0, 8, divIdx)];
}
} // namespace

SynthEngine::SynthEngine() = default;

int SynthEngine::countActiveVoices() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
    {
        if (!v.active)
            continue;
        ++n;
    }
    return n;
}

void SynthEngine::allNotesOff() noexcept
{
    keyDepth.fill(0);
    for (auto& v : voices)
        v.active = false;
}

void SynthEngine::bindParameterPointers(juce::AudioProcessorValueTreeState& apvts)
{
    ptrs.synthPolyphony = apvts.getRawParameterValue("synth_polyphony");
    ptrs.synthLegato = apvts.getRawParameterValue("synth_legato");
    ptrs.synthPortamento = apvts.getRawParameterValue("synth_portamento");

    ptrs.masterPitch = apvts.getRawParameterValue("master_pitch");
    ptrs.masterVol = apvts.getRawParameterValue("master_volume");
    ptrs.filterAdsrA = apvts.getRawParameterValue("filter_adsr_attack");
    ptrs.filterAdsrD = apvts.getRawParameterValue("filter_adsr_decay");
    ptrs.filterAdsrS = apvts.getRawParameterValue("filter_adsr_sustain");
    ptrs.filterAdsrR = apvts.getRawParameterValue("filter_adsr_release");
    ptrs.lfoRate = apvts.getRawParameterValue("lfo_rate");
    ptrs.lfoDepth = apvts.getRawParameterValue("lfo_depth");
    ptrs.lfoShape = apvts.getRawParameterValue("lfo_shape");
    ptrs.lfoSync = apvts.getRawParameterValue("lfo_sync");
    ptrs.lfoSyncDiv = apvts.getRawParameterValue("lfo_sync_div");
    ptrs.lfoDelay = apvts.getRawParameterValue("lfo_delay");
    ptrs.lfoFade = apvts.getRawParameterValue("lfo_fade");
    ptrs.lfoRetrigger = apvts.getRawParameterValue("lfo_retrigger");

    ptrs.masterPitchF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("master_pitch"));
    ptrs.masterVolF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("master_volume"));
    ptrs.filterAdsrAF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("filter_adsr_attack"));
    ptrs.filterAdsrDF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("filter_adsr_decay"));
    ptrs.filterAdsrSF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("filter_adsr_sustain"));
    ptrs.filterAdsrRF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("filter_adsr_release"));
    ptrs.lfoRateF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("lfo_rate"));
    ptrs.lfoDepthF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("lfo_depth"));
    ptrs.lfoDelayF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("lfo_delay"));
    ptrs.lfoFadeF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("lfo_fade"));

    for (int i = 0; i < kNumLayers; ++i)
    {
        const juce::String p = "layer" + juce::String(i) + "_";
        ptrs.layerVol[(size_t)i] = apvts.getRawParameterValue(p + "volume");
        ptrs.layerPan[(size_t)i] = apvts.getRawParameterValue(p + "pan");
        ptrs.layerOsc[(size_t)i] = apvts.getRawParameterValue(p + "osc_type");
        ptrs.layerCoarse[(size_t)i] = apvts.getRawParameterValue(p + "tune_coarse");
        ptrs.layerFine[(size_t)i] = apvts.getRawParameterValue(p + "tune_fine");
        ptrs.layerOctave[(size_t)i] = apvts.getRawParameterValue(p + "tune_octave");
        ptrs.layerCutoff[(size_t)i] = apvts.getRawParameterValue(p + "filter_cutoff");
        ptrs.layerRes[(size_t)i] = apvts.getRawParameterValue(p + "filter_resonance");
        ptrs.layerFtype[(size_t)i] = apvts.getRawParameterValue(p + "filter_type");
        ptrs.layerFilterDrive[(size_t)i] = apvts.getRawParameterValue(p + "filter_drive");
        ptrs.layerF2type[(size_t)i] = apvts.getRawParameterValue(p + "filter2_type");
        ptrs.layerF2cutoff[(size_t)i] = apvts.getRawParameterValue(p + "filter2_cutoff");
        ptrs.layerF2res[(size_t)i] = apvts.getRawParameterValue(p + "filter2_resonance");
        ptrs.layerF2drive[(size_t)i] = apvts.getRawParameterValue(p + "filter2_drive");
        ptrs.layerGranPos[(size_t)i] = apvts.getRawParameterValue(p + "gran_pos");
        ptrs.layerGranSize[(size_t)i] = apvts.getRawParameterValue(p + "gran_size");
        ptrs.layerGranDensity[(size_t)i] = apvts.getRawParameterValue(p + "gran_density");
        ptrs.layerGranScatter[(size_t)i] = apvts.getRawParameterValue(p + "gran_scatter");
        ptrs.layerGranFreeze[(size_t)i] = apvts.getRawParameterValue(p + "gran_freeze");
        ptrs.layerWtMorph[(size_t)i] = apvts.getRawParameterValue(p + "wavetable_morph");
        ptrs.layerKeytrack[(size_t)i] = apvts.getRawParameterValue(p + "filter_keytrack");
        ptrs.layerAA[(size_t)i] = apvts.getRawParameterValue(p + "amp_attack");
        ptrs.layerAD[(size_t)i] = apvts.getRawParameterValue(p + "amp_decay");
        ptrs.layerAS[(size_t)i] = apvts.getRawParameterValue(p + "amp_sustain");
        ptrs.layerAR[(size_t)i] = apvts.getRawParameterValue(p + "amp_release");
        ptrs.layerLfo2Rate[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_rate");
        ptrs.layerLfo2Depth[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_depth");
        ptrs.layerLfo2Shape[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_shape");
        ptrs.layerLfo2Sync[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_sync");
        ptrs.layerLfo2SyncDiv[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_sync_div");
        ptrs.layerLfo2Delay[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_delay");
        ptrs.layerLfo2Fade[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_fade");
        ptrs.layerLfo2Retrigger[(size_t)i] = apvts.getRawParameterValue(p + "lfo2_retrigger");
        ptrs.layerFilterRoute[(size_t)i] = apvts.getRawParameterValue(p + "filter_routing");
        ptrs.layerUnisonVoices[(size_t)i] = apvts.getRawParameterValue(p + "unison_voices");
        ptrs.layerUnisonDetune[(size_t)i] = apvts.getRawParameterValue(p + "unison_detune");
        ptrs.layerUnisonSpread[(size_t)i] = apvts.getRawParameterValue(p + "unison_spread");

        ptrs.layerVolF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "volume"));
        ptrs.layerPanF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "pan"));
        ptrs.layerFineF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "tune_fine"));
        ptrs.layerCutoffF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "filter_cutoff"));
        ptrs.layerResF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "filter_resonance"));
        ptrs.layerFilterDriveF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "filter_drive"));
        ptrs.layerF2cutoffF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "filter2_cutoff"));
        ptrs.layerF2resF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "filter2_resonance"));
        ptrs.layerF2driveF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "filter2_drive"));
        ptrs.layerGranPosF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "gran_pos"));
        ptrs.layerGranSizeF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "gran_size"));
        ptrs.layerGranDensityF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "gran_density"));
        ptrs.layerGranScatterF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "gran_scatter"));
        ptrs.layerWtMorphF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "wavetable_morph"));
        ptrs.layerKeytrackF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "filter_keytrack"));
        ptrs.layerLfo2RateF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "lfo2_rate"));
        ptrs.layerLfo2DepthF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "lfo2_depth"));
        ptrs.layerLfo2DelayF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "lfo2_delay"));
        ptrs.layerLfo2FadeF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "lfo2_fade"));
        ptrs.layerUnisonDetuneF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "unison_detune"));
        ptrs.layerUnisonSpreadF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "unison_spread"));
        ptrs.layerAAF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "amp_attack"));
        ptrs.layerADF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "amp_decay"));
        ptrs.layerASF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "amp_sustain"));
        ptrs.layerARF[(size_t)i] = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(p + "amp_release"));
    }
    pointersBound = true;
}

void SynthEngine::fillWavetable() noexcept
{
    for (int i = 0; i < kWtSize; ++i)
    {
        const double ph = (double)i / (double)kWtSize;
        const double a1 = std::sin(dsp::twoPi * ph);
        const double a3 = std::sin(dsp::twoPi * ph * 3.0) * 0.25;
        const double a5 = std::sin(dsp::twoPi * ph * 5.0) * 0.12;
        wavetable[(size_t)i] = (a1 + a3 + a5) * 0.55;
    }
}

void SynthEngine::fillWavetableB() noexcept
{
    for (int i = 0; i < kWtSize; ++i)
    {
        const double ph = (double)i / (double)kWtSize;
        const double sq = ph < 0.5 ? 1.0 : -1.0;
        const double a1 = std::sin(dsp::twoPi * ph) * 0.45;
        const double a2 = std::sin(dsp::twoPi * ph * 2.0) * 0.35;
        const double a4 = std::sin(dsp::twoPi * ph * 4.0) * 0.2;
        wavetableB[(size_t)i] = juce::jlimit(-1.0, 1.0, (a1 + a2 + a4 + sq * 0.08) * 0.5);
    }
}

void SynthEngine::fillGranularSource() noexcept
{
    uint32_t s = 0xACE1u;
    for (int i = 0; i < kGranSize; ++i)
    {
        const double wt = wavetable[(size_t)(i % kWtSize)];
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        const double n = (double)(s & 0xffff) / 32768.0 - 1.0;
        granularBuffer[(size_t)i] = wt * 0.85 + n * 0.12;
    }
}

void SynthEngine::prepare(double sr, int samplesPerBlock, juce::AudioProcessorValueTreeState& apvts)
{
    sampleRate = sr;
    maxBlock = juce::jmax(1, samplesPerBlock);
    bindParameterPointers(apvts);
    fillWavetable();
    fillWavetableB();
    fillGranularSource();
    globalLfo.setSampleRate(sr);
    globalLfo.setShape(0);
    globalLfoInitialised = false;
    globalLfoDelayRem = 0;
    globalLfoFadeTotal = globalLfoFadeProg = 0;
    keyDepth.fill(0);

    for (auto& v : voices)
    {
        v.active = false;
        v.age = 0;
        for (int L = 0; L < kNumLayers; ++L)
            v.layers[(size_t)L].prepare(sr,
                                        wavetable.data(),
                                        kWtSize,
                                        wavetableB.data(),
                                        kWtSize,
                                        granularBuffer.data(),
                                        kGranSize,
                                        maxBlock);
    }

    sampleLibrary.scanAsync();
}

void SynthEngine::reset() noexcept
{
    globalLfo.phase = 0;
    lastGlobalLfoForMod = 0.0;
    lastFxReverbMixAdd = lastFxDelayMixAdd = lastFxChorusMixAdd = 0.f;
    globalLfoInitialised = false;
    globalLfoDelayRem = 0;
    globalLfoFadeTotal = globalLfoFadeProg = 0;
    keyDepth.fill(0);
    modMatrix.reset();
    for (auto& v : voices)
    {
        v.active = false;
        for (auto& L : v.layers)
            L.reset();
    }
}

int SynthEngine::voicePolyLimit() const noexcept
{
    const int poly = readIntFromNorm(ptrs.synthPolyphony, 1, 16, 16);
    return juce::jlimit(1, kNumVoices, poly);
}

int SynthEngine::findFreeVoice() noexcept
{
    const int poly = voicePolyLimit();
    for (int i = 0; i < poly; ++i)
        if (!voices[(size_t)i].active)
            return i;
    return -1;
}

int SynthEngine::findOldestVoice() noexcept
{
    const int poly = voicePolyLimit();
    int best = 0;
    uint32_t ma = 0;
    for (int i = 0; i < poly; ++i)
    {
        if (!voices[(size_t)i].active)
            return i;
        if (voices[(size_t)i].age >= ma)
        {
            ma = voices[(size_t)i].age;
            best = i;
        }
    }
    return best;
}

void SynthEngine::startVoice(int voiceIndex, int note, float velocity, bool softSteal) noexcept
{
    auto& v = voices[(size_t)voiceIndex];
    const bool wasActive = v.active && v.midiNote != note;
    double ae = 0, fe = 0;
    if (softSteal && wasActive)
    {
        ae = v.layers[0].getAmpEnvLevel();
        fe = v.layers[0].getFiltEnvLevel();
    }
    v.active = true;
    v.midiNote = note;
    v.velocity = velocity;
    v.age = 0;
    for (int L = 0; L < kNumLayers; ++L)
    {
        if (softSteal && wasActive)
            v.layers[(size_t)L].noteOnSteal(note, velocity, L, ptrs, ae, fe);
        else
            v.layers[(size_t)L].noteOn(note, velocity, L, ptrs);
    }
}

void SynthEngine::noteOffKey(int note) noexcept
{
    if (note >= 0 && note < 128 && keyDepth[(size_t)note] > 0)
        keyDepth[(size_t)note]--;

    for (auto& v : voices)
    {
        if (v.active && v.midiNote == note)
            for (auto& L : v.layers)
                L.noteOff();
    }
}

void SynthEngine::deactivateFinishedVoices() noexcept
{
    for (auto& v : voices)
    {
        if (!v.active)
            continue;
        bool any = false;
        for (auto& L : v.layers)
        {
            if (L.isActive())
            {
                any = true;
                break;
            }
        }
        if (!any)
            v.active = false;
    }
}

void SynthEngine::process(juce::AudioBuffer<float>& layerBus,
                          int numSamplesToProcess,
                          juce::MidiBuffer& midi,
                          juce::AudioProcessorValueTreeState& apvts,
                          juce::AudioPlayHead* playHead) noexcept
{
    juce::ignoreUnused(apvts);
    if (!pointersBound)
        return;

    const int numCh = layerBus.getNumChannels();
    const int numSamples = numSamplesToProcess;
    if (numCh < 8 || numSamples < 1)
        return;

    double hostBpm = 120.0;
    if (playHead != nullptr)
        if (auto pos = playHead->getPosition())
            if (pos->getBpm().hasValue())
                hostBpm = juce::jlimit(20.0, 999.0, (double)*pos->getBpm());

    for (int ch = 0; ch < 8; ++ch)
        layerBus.clear(ch, 0, numSamples);

    float* out0 = layerBus.getWritePointer(0);
    float* out1 = layerBus.getWritePointer(1);
    float* out2 = layerBus.getWritePointer(2);
    float* out3 = layerBus.getWritePointer(3);
    float* out4 = layerBus.getWritePointer(4);
    float* out5 = layerBus.getWritePointer(5);
    float* out6 = layerBus.getWritePointer(6);
    float* out7 = layerBus.getWritePointer(7);

    struct MidiEv
    {
        juce::MidiMessage msg;
        int pos = 0;
    };
    std::array<MidiEv, 256> events {};
    size_t numEv = 0;
    for (const auto metadata : midi)
    {
        if (numEv >= events.size())
            break;
        events[numEv].msg = metadata.getMessage();
        events[numEv].pos = metadata.samplePosition;
        ++numEv;
    }
    std::sort(events.begin(), events.begin() + (ptrdiff_t)numEv, [](const MidiEv& a, const MidiEv& b) {
        return a.pos < b.pos;
    });
    size_t evIx = 0;

    const bool legato = readBoolNorm(ptrs.synthLegato, false);

    for (int i = 0; i < numSamples; ++i)
    {
        while (evIx < numEv && events[evIx].pos == i)
        {
            const auto& m = events[evIx].msg;
            const int ch = m.getChannel();
            if (ch != 1 && ch != 16 && ch != 0) // Listen to 1 (main), 16 (preview), or 0 (omni-ish)
            {
                ++evIx;
                continue;
            }

            if (m.isNoteOn() && m.getVelocity() > 0)
            {
                const int note = m.getNoteNumber();
                int keysAlready = 0;
                for (int k = 0; k < 128; ++k)
                    if (keyDepth[(size_t)k] > 0)
                        ++keysAlready;

                keyDepth[(size_t)note]++;

                if (readBoolNorm(ptrs.lfoRetrigger, false))
                {
                    globalLfo.phase = 0;
                    globalLfoDelayRem = (double)readFloatAP(ptrs.lfoDelay, ptrs.lfoDelayF, 0.f) * sampleRate;
                    globalLfoFadeTotal = (double)readFloatAP(ptrs.lfoFade, ptrs.lfoFadeF, 0.f) * sampleRate;
                    globalLfoFadeProg = 0;
                }

                if (legato && keysAlready > 0 && voices[0].active)
                {
                    auto& v0 = voices[0];
                    v0.active = true;
                    v0.midiNote = note;
                    v0.velocity = m.getFloatVelocity();
                    v0.age = 0;
                    for (int L = 0; L < kNumLayers; ++L)
                        v0.layers[(size_t)L].noteOnLegato(note, m.getFloatVelocity(), L, ptrs);
                }
                else
                {
                    int vi = -1;
                    for (int vIter = 0; vIter < voicePolyLimit(); ++vIter)
                    {
                        if (voices[(size_t)vIter].active && voices[(size_t)vIter].midiNote == note)
                        {
                            vi = vIter;
                            break;
                        }
                    }

                    if (vi < 0)
                        vi = findFreeVoice();
                    if (vi < 0)
                        vi = findOldestVoice();

                    const bool steal = voices[(size_t)vi].active && voices[(size_t)vi].midiNote != note;
                    startVoice(vi, note, m.getFloatVelocity(), steal);
                }
            }
            else if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
            {
                noteOffKey(m.getNoteNumber());
            }
            else if (m.isPitchWheel())
            {
                const int pw = m.getPitchWheelValue();
                modMatrix.setPitchBend((float)(pw - 8192) / 8192.f);
            }
            else if (m.isController() && m.getControllerNumber() == 1)
            {
                modMatrix.setModWheel(m.getControllerValue() / 127.f);
            }
            else if (m.isChannelPressure())
            {
                modMatrix.setAftertouch(m.getChannelPressureValue() / 127.f);
            }
            else if (m.isAftertouch())
            {
                modMatrix.setAftertouch(m.getAfterTouchValue() / 127.f);
            }
            ++evIx;
        }

        if (!globalLfoInitialised)
        {
            globalLfoDelayRem = (double)readFloatAP(ptrs.lfoDelay, ptrs.lfoDelayF, 0.f) * sampleRate;
            globalLfoFadeTotal = (double)readFloatAP(ptrs.lfoFade, ptrs.lfoFadeF, 0.f) * sampleRate;
            globalLfoFadeProg = 0;
            globalLfoInitialised = true;
        }

        float filtEnv01 = 0.f;
        float ampEnv01 = 0.f;
        float vel01 = 0.f;
        for (auto& v : voices)
        {
            if (!v.active)
                continue;
            filtEnv01 = juce::jmax(filtEnv01, v.layers[0].getLastFiltEnv());
            ampEnv01 = juce::jmax(ampEnv01, v.layers[0].getLastAmpEnv());
            vel01 = juce::jmax(vel01, v.velocity);
        }

        float layerCutSemi[4];
        float layerResAdd[4];
        float layerPitchSemi[4];
        float layerAmpMul[4];
        float layerPanAdd[4];
        float lfoRateMul = 1.f;
        float lfoDepthAdd = 0.f;
        float masterMul = 1.f;
        float fxRev = 0.f, fxDel = 0.f, fxCho = 0.f;
        modMatrix.evaluate((float)lastGlobalLfoForMod,
                           filtEnv01,
                           ampEnv01,
                           vel01,
                           modMatrix.getPitchBendValue(),
                           layerCutSemi,
                           layerResAdd,
                           layerPitchSemi,
                           layerAmpMul,
                           layerPanAdd,
                           lfoRateMul,
                           lfoDepthAdd,
                           masterMul,
                           fxRev,
                           fxDel,
                           fxCho,
                           sampleRate);
        lastFxReverbMixAdd = fxRev;
        lastFxDelayMixAdd = fxDel;
        lastFxChorusMixAdd = fxCho;

        const int lfoShapeN = readChoiceIndex(ptrs.lfoShape, 8, 0);
        globalLfo.setShape(juce::jlimit(0, 7, lfoShapeN));
        const float baseLfoDepth = readFloatAP(ptrs.lfoDepth, ptrs.lfoDepthF, 0.f);
        const float effLfoDepth = juce::jlimit(0.01f, 1.f, baseLfoDepth + lfoDepthAdd);
        const float depthRatio = juce::jlimit(0.25f, 4.f, effLfoDepth / juce::jmax(0.01f, baseLfoDepth));

        double lfoHz = (double)readFloatAP(ptrs.lfoRate, ptrs.lfoRateF, 2.f) * (double)lfoRateMul;
        if (readBoolNorm(ptrs.lfoSync, false) && hostBpm > 20.0)
        {
            const int divi = readChoiceIndex(ptrs.lfoSyncDiv, 9, 4);
            const double beats = beatsForSyncDivIndex(divi);
            lfoHz = hostBpm / (60.0 * beats);
        }

        double gLfoRaw = 0;
        if (globalLfoDelayRem > 0)
        {
            globalLfoDelayRem -= 1.0;
            gLfoRaw = 0;
        }
        else
        {
            gLfoRaw = globalLfo.tick(lfoHz);
            if (globalLfoFadeTotal > 0 && globalLfoFadeProg < globalLfoFadeTotal)
            {
                gLfoRaw *= globalLfoFadeProg / juce::jmax(1.0, globalLfoFadeTotal);
                globalLfoFadeProg += 1.0;
                if (globalLfoFadeProg >= globalLfoFadeTotal)
                    globalLfoFadeTotal = 0;
            }
        }

        const double gLfoToVoice = gLfoRaw * (double)depthRatio;
        lastGlobalLfoForMod = gLfoRaw;

        double layL[4] = {}, layR[4] = {};
        for (auto& v : voices)
        {
            if (!v.active)
                continue;
            for (int L = 0; L < kNumLayers; ++L)
                v.layers[(size_t)L].renderAdd(layL[(size_t)L],
                                              layR[(size_t)L],
                                              L,
                                              v.midiNote,
                                              v.velocity,
                                              gLfoToVoice,
                                              hostBpm,
                                              ptrs,
                                              layerCutSemi[(size_t)L],
                                              layerResAdd[(size_t)L],
                                              layerPitchSemi[(size_t)L],
                                              layerAmpMul[(size_t)L],
                                              layerPanAdd[(size_t)L]);
        }

        const float master = readFloatAP(ptrs.masterVol, ptrs.masterVolF, 0.8f);
        const double masterScaled = (double)master * (double)masterMul;
        float* layerOut[4][2] = { { out0, out1 }, { out2, out3 }, { out4, out5 }, { out6, out7 } };
        for (int L = 0; L < 4; ++L)
        {
            layerOut[(size_t)L][0][i] = (float)(layL[(size_t)L] * masterScaled);
            layerOut[(size_t)L][1][i] = (float)(layR[(size_t)L] * masterScaled);
        }
    }

    for (auto& v : voices)
        if (v.active)
            v.age += (uint32_t)numSamples;

    deactivateFinishedVoices();
}

void SynthEngine::loadLayerSample(int layerIndex,
                                  int sampleId,
                                  const juce::String& filePath,
                                  int rootNoteMidi,
                                  bool loopEnabled,
                                  bool oneShot,
                                  float startFrac,
                                  float endFrac)
{
    if (layerIndex < 0 || layerIndex >= kNumLayers)
        return;

    juce::Thread::launch([=] {
        std::lock_guard<std::mutex> loadLock(layerSampleLoadMutex);
        for (auto& v : voices)
            v.layers[(size_t)layerIndex].loadSample(sampleId, filePath, rootNoteMidi, loopEnabled, oneShot, startFrac, endFrac);
    });
}

} // namespace wolfsden
