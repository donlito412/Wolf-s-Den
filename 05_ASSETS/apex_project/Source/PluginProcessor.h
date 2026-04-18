#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/FxEngine.h"
#include "engine/MidiPipeline.h"
#include "engine/ModMatrix.h"
#include "engine/SynthEngine.h"
#include "engine/TheoryEngine.h"
#include "parameters/WolfsDenParameterLayout.h"

struct UiToDspMessage
{
    uint32_t paramIdHash = 0;
    float value = 0.f;
};

class WolfsDenAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener
{
public:
    WolfsDenAudioProcessor();
    ~WolfsDenAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    /** Shared with on-screen MIDI keyboard(s); merged into processBlock MIDI stream. */
    juce::MidiKeyboardState& getMidiKeyboardState() noexcept { return midiKeyboardState; }

    wolfsden::ModMatrix& getModMatrix() noexcept { return synthEngine.getModMatrix(); }

    void setPresetDisplayName(juce::String name);
    juce::String getPresetDisplayName() const;

    /** Browse-page chord-set pack (SQL `chord_sets.id`); drives preset bar name when set from cards or < >. */
    void setBrowseChordSetSelection(int chordSetDbId);
    int getBrowseChordSetId() const;
    void cycleBrowseChordSet(int delta);

    // =========================================================================
    // Patch preset system  (message thread)
    // =========================================================================

    /**
     * Save the current full plugin state (APVTS + ModMatrix + custom) as a named
     * user preset in the DB.  category is e.g. "User", "Synth", "Keys".
     * Returns the new preset id, or -1 on failure.
     */
    int  saveCurrentAsPreset (const juce::String& name,
                              const juce::String& category = "User");

    /**
     * Load a preset by DB id.
     * Factory presets (isFactory) apply a recipe (sample + ADSR) to layer 0
     * while leaving other layers/FX at their current settings — like an
     * instrument slot swap.  User presets fully restore all state.
     * Returns true on success.
     */
    bool loadPresetById (int presetId);

    /** Step forward (+1) or backward (-1) through the loaded preset list.
     *  filteredIndices, if non-empty, restricts cycling to that subset (indices into getPresetListings()). */
    void cyclePreset (int delta, const std::vector<int>& filteredIndices = {});

    /** Step through factory presets only. */
    void cycleFactoryPreset (int delta);

    /** Preview a chord set (plays a short chord on ch 16, which bypasses chord/arp). */
    void previewChordSet (int chordSetId);

    /** Start recording incoming MIDI to an in-memory buffer (cleared on call). */
    void startMidiCapture();

    /** Stop recording and save the captured MIDI to a .mid file on the Desktop.
     *  Returns the File on success, or an empty File if nothing was captured. */
    juce::File stopMidiCaptureAndSave();

    /** The DB id of the last successfully loaded/saved preset (-1 = none). */
    int  getCurrentPresetId() const noexcept;

    /** Absolute path to the factory sample content directory. */
    juce::File getFactoryContentDir() const noexcept { return factoryContentDir; }

    void setLastEditorBounds(int width, int height);
    int getEditorWidth() const noexcept { return editorWidth.load(std::memory_order_relaxed); }
    int getEditorHeight() const noexcept { return editorHeight.load(std::memory_order_relaxed); }

    wolfsden::TheoryEngine& getTheoryEngine() noexcept { return theoryEngine; }
    const wolfsden::TheoryEngine& getTheoryEngine() const noexcept { return theoryEngine; }
    wolfsden::SynthEngine& getSynthEngine() noexcept { return synthEngine; }
    wolfsden::MidiPipeline& getMidiPipeline() noexcept { return midiPipeline; }

    /** Returns true while MIDI capture is recording. */
    bool isMidiCaptureActive() const noexcept { return midiCaptureActive.load(std::memory_order_relaxed); }

    juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    wolfsden::WDSampleLibrary& getSampleLibrary() noexcept { return synthEngine.getSampleLibrary(); }

    /** Call from the UI (message thread) when the user selects a sample for a layer. */
    void requestLayerSampleLoad (int layerIndex, int sampleId, const juce::String& filePath,
                                 int rootNoteMidi, bool loopEnabled, bool oneShot,
                                 float startFrac = 0.f, float endFrac = 1.f)
    {
        synthEngine.loadLayerSample (layerIndex, sampleId, filePath, rootNoteMidi, loopEnabled, oneShot, startFrac, endFrac);
    }

    void setTheoryDetectionMidi() noexcept;
    void setTheoryDetectionAudio() noexcept;

    float getSmoothedCpuLoad() const noexcept { return cpuSmoothed.load(std::memory_order_relaxed); }
    float getOutputPeakL() const noexcept { return outputPeakL.load(std::memory_order_relaxed); }
    float getOutputPeakR() const noexcept { return outputPeakR.load(std::memory_order_relaxed); }
    bool getIsHostPlaying() const noexcept { return isHostPlaying.load(std::memory_order_relaxed); }
    /** Call from UI timer: returns true if MIDI arrived since last call (one-shot). */
    bool consumeMidiActivityFlag() noexcept;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void drainUiToDspFifo() noexcept;
    void registerApvtsListeners();
    void unregisterApvtsListeners();
    void syncTheoryParamsFromApvts();

    static constexpr int kUiFifoSize = 512;
    juce::AbstractFifo uiToDspFifo { kUiFifoSize }; // lock-free SPSC queue (UI / message → audio)
    std::array<UiToDspMessage, (size_t)kUiFifoSize> uiToDspBuffer {};

    juce::AudioProcessorValueTreeState apvts;

    juce::MidiKeyboardState midiKeyboardState;

    wolfsden::MidiPipeline midiPipeline;
    wolfsden::TheoryEngine theoryEngine;
    wolfsden::SynthEngine synthEngine;
    wolfsden::FxEngine fxEngine;

    /** Apply a factory preset recipe to layer 0 (called on message thread). */
    void applyFactoryPreset (const wolfsden::PresetListing& preset);

    mutable juce::CriticalSection customStateLock;
    juce::String presetDisplayName { "Init" };
    juce::String chordProgressionBlob;
    int browseChordSetId { -1 };
    int currentPresetId  { -1 };   ///< DB id of active preset; -1 = unsaved/init

    juce::File factoryContentDir;  ///< path to bundle Resources/Factory (WAV root)

    std::atomic<int> editorWidth { 480 };
    std::atomic<int> editorHeight { 320 };
    juce::UndoManager undoManager;

    /** 8 ch: per-layer stereo before FxEngine (L0,R0,…,L3,R3). */
    juce::AudioBuffer<float> synthLayerBus;

    std::atomic<float> outputPeakL { 0.f };
    std::atomic<float> outputPeakR { 0.f };
    std::atomic<float> cpuSmoothed { 0.f };
    std::atomic<bool> isHostPlaying { false };
    std::atomic<int> midiActivityFlag { 0 }; // 1 = flash LED until UI consumes
    std::atomic<bool> pendingAllNotesOff { false }; // set by UI thread; consumed by audio thread

    // -------------------------------------------------------------------------
    // MIDI capture (records note/CC events to an in-memory ring, then exports MIDI)
    // -------------------------------------------------------------------------
    static constexpr int kMaxCaptureEvents = 4096;
    struct MidiCaptureEvent
    {
        juce::int64 timeMs = 0;
        uint8_t     data[4] = {};
    };
    std::array<MidiCaptureEvent, (size_t)kMaxCaptureEvents> midiCaptureBuffer {};
    std::atomic<bool>        midiCaptureActive { false };
    std::atomic<int>         midiCaptureFill   { 0 };
    juce::int64              midiCaptureStartMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WolfsDenAudioProcessor)
};
