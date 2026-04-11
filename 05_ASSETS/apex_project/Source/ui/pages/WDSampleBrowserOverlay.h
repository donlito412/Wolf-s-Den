#pragma once

#include "../../engine/samples/WDSampleLibrary.h"
#include "../theme/UITheme.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class WolfsDenAudioProcessor;

namespace wolfsden::ui
{

/** Modal sample browser overlay for Wolf's Den.
 *
 *  Shows the WDSampleLibrary entries in a searchable list.
 *  Pressing "Load to Layer" fires onSampleChosen(layerIndex, entry).
 *  A dedicated AudioDeviceManager provides click-to-preview independent of the voice engine.
 */
class WDSampleBrowserOverlay : public juce::Component,
                                private juce::ListBoxModel,
                                private juce::TextEditor::Listener,
                                private juce::Timer
{
public:
    /** Called when the user confirms a sample.  Layer index is passed through from open(). */
    std::function<void(int layerIndex, const SampleEntry&)> onSampleChosen;

    explicit WDSampleBrowserOverlay (WolfsDenAudioProcessor& proc);
    ~WDSampleBrowserOverlay() override;

    /** Show the overlay for a specific layer and refresh from the library. */
    void open (int layerIndex);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // ── ListBoxModel ──────────────────────────────────────────────────────────
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    // ── TextEditor::Listener ──────────────────────────────────────────────────
    void textEditorTextChanged (juce::TextEditor&) override;

    // ── Timer (preview polling) ───────────────────────────────────────────────
    void timerCallback() override;

    void refreshList();
    void previewEntry (const SampleEntry& e);
    void stopPreview();

    WolfsDenAudioProcessor& processor;
    int activeLayerIndex = 0;

    // UI
    juce::Label        titleLabel;
    juce::TextEditor   searchBox;
    juce::ComboBox     categoryCombo;
    juce::ListBox      listBox;
    juce::TextButton   previewBtn { "▶ Preview" };
    juce::TextButton   loadBtn    { "Load to Layer" };
    juce::TextButton   closeBtn   { "✕" };

    // Data
    std::vector<SampleEntry> displayedEntries;

    // Preview engine (independent of voice engine)
    juce::AudioDeviceManager      previewDeviceManager;
    juce::AudioFormatManager      previewFormatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> previewSource;
    std::unique_ptr<juce::AudioSourcePlayer>       previewPlayer;
    bool previewSetup = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WDSampleBrowserOverlay)
};

} // namespace wolfsden::ui
