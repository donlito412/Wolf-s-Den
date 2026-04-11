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

    // Factory WAVs live at …/Contents/Resources/Factory (CMake POST_BUILD copy)
    {
        const auto exe      = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        const auto contents = exe.getParentDirectory().getParentDirectory();
        factoryContentDir   = contents.getChildFile ("Resources").getChildFile ("Factory");
        getSynthEngine().getSampleLibrary().setBundleFactoryDirectory (factoryContentDir);

        // Seed the factory pack + 41 instrument presets on first launch (no-op thereafter)
        theoryEngine.seedFactoryPresets (factoryContentDir);
    }
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

    // UI thread requested an immediate voice-kill (e.g. preset change)
    if (pendingAllNotesOff.exchange(false, std::memory_order_acq_rel))
        synthEngine.allNotesOff();

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

void WolfsDenAudioProcessor::setBrowseChordSetSelection(int chordSetDbId)
{
    const juce::ScopedLock sl(customStateLock);
    browseChordSetId = chordSetDbId;
    if (!theoryEngine.isDatabaseReady())
        return;
    for (const auto& r : theoryEngine.getChordSetListings())
    {
        if (r.id == chordSetDbId)
        {
            presetDisplayName = r.name.c_str();
            return;
        }
    }
}

int WolfsDenAudioProcessor::getBrowseChordSetId() const
{
    const juce::ScopedLock sl(customStateLock);
    return browseChordSetId;
}

void WolfsDenAudioProcessor::cycleBrowseChordSet(int delta)
{
    if (!theoryEngine.isDatabaseReady())
        return;

    std::vector<std::pair<int, juce::String>> rows;
    for (const auto& r : theoryEngine.getChordSetListings())
        rows.push_back({r.id, r.name.c_str()});
    if (rows.empty())
        return;

    int curId = -1;
    juce::String curName;
    {
        const juce::ScopedLock sl(customStateLock);
        curId = browseChordSetId;
        curName = presetDisplayName;
    }

    int idx = 0;
    bool found = false;
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        if (rows[(size_t)i].first == curId)
        {
            idx = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            if (rows[(size_t)i].second == curName)
            {
                idx = i;
                found = true;
                break;
            }
        }
    }

    idx += delta;
    idx = (idx % (int)rows.size() + (int)rows.size()) % (int)rows.size();
    setBrowseChordSetSelection(rows[(size_t)idx].first);
}

// =============================================================================
// Patch preset system
// =============================================================================

int WolfsDenAudioProcessor::saveCurrentAsPreset (const juce::String& name,
                                                   const juce::String& category)
{
    juce::MemoryBlock blob;
    getStateInformation (blob);

    const int newId = theoryEngine.savePreset (
        name.toStdString(),
        "User",
        category.toStdString(),
        "[]",   // tags — empty JSON array; future UI can populate
        0,      // pack_id 0 = user preset (no pack)
        blob.getData(),
        (int) blob.getSize());

    if (newId > 0)
    {
        const juce::ScopedLock sl (customStateLock);
        presetDisplayName = name;
        currentPresetId   = newId;
    }
    return newId;
}

bool WolfsDenAudioProcessor::loadPresetById (int presetId)
{
    if (!theoryEngine.isDatabaseReady()) return false;

    const auto& list = theoryEngine.getPresetListings();
    const auto  it   = std::find_if (list.begin(), list.end(),
                                     [presetId](const wolfsden::PresetListing& p){ return p.id == presetId; });
    if (it == list.end()) return false;

    const wolfsden::PresetListing& preset = *it;

    // Silence any currently playing voices before swapping the preset.
    // The flag is consumed on the next audio-thread processBlock() call, which
    // guarantees no race condition — allNotesOff() only ever runs on the audio thread.
    pendingAllNotesOff.store(true, std::memory_order_release);

    if (preset.isFactory)
    {
        // Recipe load: configure layer 0 for this instrument sample.
        // All other layers/FX/Mod state are left as-is (instrument slot swap).
        applyFactoryPreset (preset);
    }
    else
    {
        // Full state restore from binary blob.
        const juce::MemoryBlock blob = theoryEngine.loadPresetBlob (presetId);
        if (blob.getSize() == 0) return false;
        setStateInformation (blob.getData(), (int) blob.getSize());
    }

    {
        const juce::ScopedLock sl (customStateLock);
        presetDisplayName = preset.name.c_str();
        currentPresetId   = presetId;
    }
    return true;
}

void WolfsDenAudioProcessor::applyFactoryPreset (const wolfsden::PresetListing& preset)
{
    // Resolve the WAV path inside the bundle's Factory folder
    const juce::File wavFile = factoryContentDir.getChildFile (preset.samplePath.c_str());

    // Helper: set a RangedAudioParameter from its physical (unnormalized) value
    auto setPhysical = [this](const juce::String& id, float physVal)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (physVal));
    };

    // Layer 0: switch oscillator mode to Sample (index 7)
    // AudioParameterChoice stores choices 0-N; convertTo0to1(index) = index / (N-1)
    if (auto* osc = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("layer0_osc_type")))
        osc->setValueNotifyingHost (osc->convertTo0to1 (7));

    // Layer 0 ADSR — physical seconds / linear values
    setPhysical ("layer0_amp_attack",  preset.envAttack);
    setPhysical ("layer0_amp_decay",   preset.envDecay);
    setPhysical ("layer0_amp_sustain", preset.envSustain);
    setPhysical ("layer0_amp_release", preset.envRelease);

    // Load the sample into layer 0
    if (wavFile.existsAsFile())
    {
        requestLayerSampleLoad (0,
                                preset.id,
                                wavFile.getFullPathName(),
                                preset.rootNote,
                                preset.loopEnabled,
                                preset.oneShot);
    }
}

void WolfsDenAudioProcessor::cyclePreset (int delta)
{
    if (!theoryEngine.isDatabaseReady()) return;
    const auto& list = theoryEngine.getPresetListings();
    if (list.empty()) return;

    int curId = -1;
    {
        const juce::ScopedLock sl (customStateLock);
        curId = currentPresetId;
    }

    // Find current index
    int idx = 0;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (list[(size_t)i].id == curId)
        {
            idx = i;
            break;
        }
    }

    idx += delta;
    idx = (idx % (int)list.size() + (int)list.size()) % (int)list.size();
    loadPresetById (list[(size_t)idx].id);
}

int WolfsDenAudioProcessor::getCurrentPresetId() const noexcept
{
    const juce::ScopedLock sl (customStateLock);
    return currentPresetId;
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
        custom.setProperty("presetName",      presetDisplayName,  nullptr);
        custom.setProperty("browseChordSetId",browseChordSetId,   nullptr);
        custom.setProperty("chordData",       chordProgressionBlob, nullptr);
        custom.setProperty("currentPresetId", currentPresetId,    nullptr);
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
                presetDisplayName    = custom.getProperty("presetName").toString();
                browseChordSetId     = (int)custom.getProperty("browseChordSetId", -1);
                chordProgressionBlob = custom.getProperty("chordData").toString();
                currentPresetId      = (int)custom.getProperty("currentPresetId", -1);
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
