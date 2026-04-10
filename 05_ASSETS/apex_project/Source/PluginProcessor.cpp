#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
static constexpr std::array<const char*, 5> kAutomationTestParamIds {
    "master_volume",
    "master_pan",
    "layer0_volume",
    "lfo_rate",
    "fx_reverb_mix",
};

bool isAutomationTestParam(const juce::String& id)
{
    for (auto* s : kAutomationTestParamIds)
        if (id == s)
            return true;
    return false;
}
} // namespace

WolfsDenAudioProcessor::WolfsDenAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ),
#endif
      apvts(*this, nullptr, "Parameters", wolfsden::makeParameterLayout())
{
    registerApvtsListeners();

    const auto dbDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("Wolf Productions")
                           .getChildFile("Wolf's Den");
    (void)dbDir.createDirectory();
    theoryEngine.initialise(dbDir.getChildFile("theory.db"));
    syncTheoryParamsFromApvts();
}

WolfsDenAudioProcessor::~WolfsDenAudioProcessor()
{
    unregisterApvtsListeners();
}

void WolfsDenAudioProcessor::registerApvtsListeners()
{
    for (auto* param : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            apvts.addParameterListener(withId->getParameterID(), this);
}

void WolfsDenAudioProcessor::unregisterApvtsListeners()
{
    for (auto* param : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            apvts.removeParameterListener(withId->getParameterID(), this);
}

void WolfsDenAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(newValue);

    if (theoryEngine.isDatabaseReady())
    {
        if (parameterID == "theory_scale_root")
        {
            if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("theory_scale_root")))
                theoryEngine.setScaleRoot(p->get());
        }
        else if (parameterID == "theory_scale_type")
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_scale_type")))
                theoryEngine.setScaleType(c->getIndex());
        }
        else if (parameterID == "theory_voice_leading")
        {
            if (auto* b = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("theory_voice_leading")))
                theoryEngine.setVoiceLeadingEnabled(b->get());
        }
    }

    if (!isAutomationTestParam(parameterID))
        return;

    UiToDspMessage m;
    m.paramIdHash = (uint32_t)parameterID.hashCode();
    m.value = newValue;
    int s1 = 0, z1 = 0, s2 = 0, z2 = 0;
    uiToDspFifo.prepareToWrite(1, s1, z1, s2, z2);
    if (z1 > 0)
        uiToDspBuffer[(size_t)s1] = m;
    else if (z2 > 0)
        uiToDspBuffer[(size_t)s2] = m;
    else
        return;
    uiToDspFifo.finishedWrite(1);
}

void WolfsDenAudioProcessor::drainUiToDspFifo() noexcept
{
    for (;;)
    {
        int s1 = 0, z1 = 0, s2 = 0, z2 = 0;
        uiToDspFifo.prepareToRead(256, s1, z1, s2, z2);
        const int n = z1 + z2;
        if (n <= 0)
            break;

        auto applyOne = [this](const UiToDspMessage& m) noexcept {
            for (auto* param : getParameters())
            {
                if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
                {
                    if ((uint32_t)wid->getParameterID().hashCode() != m.paramIdHash)
                        continue;
                    if (auto* rv = apvts.getRawParameterValue(wid->getParameterID()))
                        rv->store(m.value);
                    break;
                }
            }
        };

        for (int i = 0; i < z1; ++i)
            applyOne(uiToDspBuffer[(size_t)(s1 + i)]);
        for (int i = 0; i < z2; ++i)
            applyOne(uiToDspBuffer[(size_t)(s2 + i)]);

        uiToDspFifo.finishedRead(n);
    }
}

void WolfsDenAudioProcessor::syncTheoryParamsFromApvts()
{
    if (!theoryEngine.isDatabaseReady())
        return;

    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("theory_scale_root")))
        theoryEngine.setScaleRoot(p->get());
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_scale_type")))
        theoryEngine.setScaleType(c->getIndex());
    if (auto* b = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("theory_voice_leading")))
        theoryEngine.setVoiceLeadingEnabled(b->get());
}

void WolfsDenAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    theoryEngine.prepare(sampleRate, samplesPerBlock);
    syncTheoryParamsFromApvts();
    midiPipeline.prepare(sampleRate, samplesPerBlock, apvts);
    synthEngine.prepare(sampleRate, samplesPerBlock, apvts);
    synthLayerBus.setSize(8, samplesPerBlock);
    fxEngine.prepare(sampleRate, samplesPerBlock, apvts);
}

void WolfsDenAudioProcessor::releaseResources()
{
    theoryEngine.reset();
    midiPipeline.reset();
    synthEngine.reset();
    fxEngine.reset();
}

bool WolfsDenAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
}

void WolfsDenAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const juce::int64 t0 = juce::Time::getHighResolutionTicks();

    drainUiToDspFifo();

    bool playingInfo = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playingInfo = pos->getIsPlaying();
    isHostPlaying.store(playingInfo, std::memory_order_relaxed);

    const int n = buffer.getNumSamples();
    midiKeyboardState.processNextMidiBuffer(midi, 0, n, true);

    for (const auto metadata : midi)
    {
        const auto& msg = metadata.getMessage();
        if (msg.isNoteOn() || msg.isController() || msg.isPitchWheel() || msg.isAftertouch()
            || msg.isChannelPressure())
        {
            midiActivityFlag.store(1, std::memory_order_relaxed);
            break;
        }
    }

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    theoryEngine.processMidi(midi);
    midiPipeline.process(midi, n, getSampleRate(), getPlayHead(), theoryEngine);

    if (synthLayerBus.getNumChannels() < 8 || synthLayerBus.getNumSamples() < n)
        synthLayerBus.setSize(8, n);

    synthEngine.process(synthLayerBus, n, midi, apvts);
    fxEngine.processBlock(synthLayerBus,
                          buffer,
                          apvts,
                          synthEngine.getLastFxReverbMixAdd(),
                          synthEngine.getLastFxDelayMixAdd(),
                          synthEngine.getLastFxChorusMixAdd());

    const juce::int64 t1 = juce::Time::getHighResolutionTicks();
    const double sr = getSampleRate();
    const double blockSecs = (sr > 0.0 && n > 0) ? (double)n / sr : 0.0;
    if (blockSecs > 0.0)
    {
        const double used = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        const float ratio = (float)juce::jlimit(0.0, 2.0, used / blockSecs);
        const float c = cpuSmoothed.load(std::memory_order_relaxed);
        cpuSmoothed.store(c * 0.95f + ratio * 0.05f, std::memory_order_relaxed);
    }

    if (buffer.getNumChannels() >= 1 && n > 0)
    {
        const float* l = buffer.getReadPointer(0);
        float pl = 0.f;
        for (int i = 0; i < n; ++i)
            pl = juce::jmax(pl, std::abs(l[i]));
        const float oldL = outputPeakL.load(std::memory_order_relaxed);
        outputPeakL.store(juce::jmax(pl, oldL * 0.92f), std::memory_order_relaxed);

        if (buffer.getNumChannels() > 1)
        {
            const float* r = buffer.getReadPointer(1);
            float pr = 0.f;
            for (int i = 0; i < n; ++i)
                pr = juce::jmax(pr, std::abs(r[i]));
            const float oldR = outputPeakR.load(std::memory_order_relaxed);
            outputPeakR.store(juce::jmax(pr, oldR * 0.92f), std::memory_order_relaxed);
        }
        else
        {
            const float oldR = outputPeakR.load(std::memory_order_relaxed);
            outputPeakR.store(juce::jmax(pl, oldR * 0.92f), std::memory_order_relaxed);
        }
    }
}

void WolfsDenAudioProcessor::setTheoryDetectionMidi() noexcept
{
    theoryEngine.setDetectionMode(wolfsden::TheoryEngine::DetectionMode::Midi);
}

void WolfsDenAudioProcessor::setTheoryDetectionAudio() noexcept
{
    theoryEngine.setDetectionMode(wolfsden::TheoryEngine::DetectionMode::Audio);
}

bool WolfsDenAudioProcessor::consumeMidiActivityFlag() noexcept
{
    return midiActivityFlag.exchange(0, std::memory_order_acq_rel) != 0;
}

void WolfsDenAudioProcessor::setPresetDisplayName(juce::String name)
{
    const juce::ScopedLock sl(customStateLock);
    presetDisplayName = std::move(name);
}

juce::String WolfsDenAudioProcessor::getPresetDisplayName() const
{
    const juce::ScopedLock sl(customStateLock);
    return presetDisplayName;
}

void WolfsDenAudioProcessor::setLastEditorBounds(int width, int height)
{
    editorWidth.store(width, std::memory_order_relaxed);
    editorHeight.store(height, std::memory_order_relaxed);
}

void WolfsDenAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree root("WolfsDenState");
    root.appendChild(apvts.copyState(), nullptr);

    juce::ValueTree custom("CustomState");
    {
        const juce::ScopedLock sl(customStateLock);
        custom.setProperty("presetName", presetDisplayName, nullptr);
        custom.setProperty("chordData", chordProgressionBlob, nullptr);
    }
    custom.setProperty("editorWidth", editorWidth.load(std::memory_order_relaxed), nullptr);
    custom.setProperty("editorHeight", editorHeight.load(std::memory_order_relaxed), nullptr);
    root.appendChild(custom, nullptr);

    root.appendChild(synthEngine.getModMatrix().toValueTree(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary(*xml, destData);
}

void WolfsDenAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto root = juce::ValueTree::fromXml(*xml);
        if (!root.isValid())
            return;

        if (root.hasType("WolfsDenState"))
        {
            if (auto params = root.getChildWithName(apvts.state.getType()); params.isValid())
            {
                apvts.replaceState(params);
                syncTheoryParamsFromApvts();
            }

            if (auto custom = root.getChildWithName("CustomState"); custom.isValid())
            {
                const juce::ScopedLock sl(customStateLock);
                presetDisplayName = custom.getProperty("presetName").toString();
                chordProgressionBlob = custom.getProperty("chordData").toString();
                editorWidth.store((int)custom.getProperty("editorWidth", 480), std::memory_order_relaxed);
                editorHeight.store((int)custom.getProperty("editorHeight", 320), std::memory_order_relaxed);
            }

            if (auto mm = root.getChildWithName("ModMatrix"); mm.isValid())
                synthEngine.getModMatrix().fromValueTree(mm);
            else
                synthEngine.getModMatrix().reset();
        }
        else if (root.hasType(apvts.state.getType()))
        {
            apvts.replaceState(root);
            syncTheoryParamsFromApvts();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WolfsDenAudioProcessor();
}

juce::AudioProcessorEditor* WolfsDenAudioProcessor::createEditor()
{
    return new WolfsDenAudioProcessorEditor(*this);
}
