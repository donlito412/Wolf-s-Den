#include "SynthEngine.h"

#include <algorithm>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace wolfsden
{
namespace
{
inline double denormLfoHz(float n01) noexcept
{
    return 0.01 + (double)n01 * (40.0 - 0.01);
}

inline float readPtr(std::atomic<float>* ap, float defV = 0.f) noexcept
{
    return ap ? ap->load(std::memory_order_relaxed) : defV;
}
} // namespace

SynthEngine::SynthEngine() = default;

void SynthEngine::bindParameterPointers(juce::AudioProcessorValueTreeState& apvts)
{
    ptrs.masterPitch = apvts.getRawParameterValue("master_pitch");
    ptrs.masterVol = apvts.getRawParameterValue("master_volume");
    ptrs.filterAdsrA = apvts.getRawParameterValue("filter_adsr_attack");
    ptrs.filterAdsrD = apvts.getRawParameterValue("filter_adsr_decay");
    ptrs.filterAdsrS = apvts.getRawParameterValue("filter_adsr_sustain");
    ptrs.filterAdsrR = apvts.getRawParameterValue("filter_adsr_release");
    ptrs.lfoRate = apvts.getRawParameterValue("lfo_rate");
    ptrs.lfoDepth = apvts.getRawParameterValue("lfo_depth");
    ptrs.lfoShape = apvts.getRawParameterValue("lfo_shape");

    for (int i = 0; i < kNumLayers; ++i)
    {
        const juce::String p = "layer" + juce::String(i) + "_";
        ptrs.layerVol[(size_t)i] = apvts.getRawParameterValue(p + "volume");
        ptrs.layerPan[(size_t)i] = apvts.getRawParameterValue(p + "pan");
        ptrs.layerOsc[(size_t)i] = apvts.getRawParameterValue(p + "osc_type");
        ptrs.layerCoarse[(size_t)i] = apvts.getRawParameterValue(p + "tune_coarse");
        ptrs.layerFine[(size_t)i] = apvts.getRawParameterValue(p + "tune_fine");
        ptrs.layerCutoff[(size_t)i] = apvts.getRawParameterValue(p + "filter_cutoff");
        ptrs.layerRes[(size_t)i] = apvts.getRawParameterValue(p + "filter_resonance");
        ptrs.layerFtype[(size_t)i] = apvts.getRawParameterValue(p + "filter_type");
        ptrs.layerAA[(size_t)i] = apvts.getRawParameterValue(p + "amp_attack");
        ptrs.layerAD[(size_t)i] = apvts.getRawParameterValue(p + "amp_decay");
        ptrs.layerAS[(size_t)i] = apvts.getRawParameterValue(p + "amp_sustain");
        ptrs.layerAR[(size_t)i] = apvts.getRawParameterValue(p + "amp_release");
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
    fillGranularSource();
    globalLfo.setSampleRate(sr);
    globalLfo.setShape(0);

    for (auto& v : voices)
    {
        v.active = false;
        v.age = 0;
        for (int L = 0; L < kNumLayers; ++L)
            v.layers[(size_t)L].prepare(sr, wavetable.data(), kWtSize, granularBuffer.data(), kGranSize);
    }
}

void SynthEngine::reset() noexcept
{
    globalLfo.phase = 0;
    lastGlobalLfoForMod = 0.0;
    lastFxReverbMixAdd = lastFxDelayMixAdd = lastFxChorusMixAdd = 0.f;
    modMatrix.reset();
    for (auto& v : voices)
    {
        v.active = false;
        for (auto& L : v.layers)
            L.reset();
    }
}

int SynthEngine::findFreeVoice() noexcept
{
    for (int i = 0; i < kNumVoices; ++i)
        if (!voices[(size_t)i].active)
            return i;
    return -1;
}

int SynthEngine::findOldestVoice() noexcept
{
    int best = 0;
    uint32_t ma = 0;
    for (int i = 0; i < kNumVoices; ++i)
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

void SynthEngine::startVoice(int voiceIndex, int note, float velocity) noexcept
{
    auto& v = voices[(size_t)voiceIndex];
    v.active = true;
    v.midiNote = note;
    v.velocity = velocity;
    v.age = 0;
    for (auto& L : v.layers)
    {
        L.reset();
        L.noteOn(note, velocity);
    }
}

void SynthEngine::noteOffKey(int note) noexcept
{
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
                          juce::MidiBuffer& midi,
                          juce::AudioProcessorValueTreeState& apvts) noexcept
{
    juce::ignoreUnused(apvts);
    if (!pointersBound)
        return;

    const int numCh = layerBus.getNumChannels();
    const int numSamples = layerBus.getNumSamples();
    if (numCh < 8 || numSamples < 1)
        return;

    layerBus.clear();

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

    for (int i = 0; i < numSamples; ++i)
    {
        while (evIx < numEv && events[evIx].pos == i)
        {
            const auto& m = events[evIx].msg;
            if (m.isNoteOn() && m.getVelocity() > 0)
            {
                int vi = findFreeVoice();
                if (vi < 0)
                    vi = findOldestVoice();
                startVoice(vi, m.getNoteNumber(), m.getFloatVelocity());
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

        const float lfoShapeN = readPtr(ptrs.lfoShape, 0.f);
        globalLfo.setShape(juce::jlimit(0, 5, (int)(lfoShapeN * 5.99f)));
        const float baseLfoDepth = readPtr(ptrs.lfoDepth, 0.5f);
        const float effLfoDepth = juce::jlimit(0.01f, 1.f, baseLfoDepth + lfoDepthAdd);
        const float depthRatio = juce::jlimit(0.25f, 4.f, effLfoDepth / juce::jmax(0.01f, baseLfoDepth));
        const double lfoHz = denormLfoHz(readPtr(ptrs.lfoRate, 0.5f)) * (double)lfoRateMul;
        const double gLfoRaw = globalLfo.tick(lfoHz);
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
                                              ptrs,
                                              layerCutSemi[(size_t)L],
                                              layerResAdd[(size_t)L]);
        }

        const float master = readPtr(ptrs.masterVol, 0.8f);
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

} // namespace wolfsden
