#pragma once

#include "../theme/UITheme.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class WolfsDenAudioProcessor;

namespace wolfsden::ui
{

/** TASK_009: Interactive circle of fifths with HSV‐coloured pie segments. */
class CircleOfFifths : public juce::Component
{
public:
    explicit CircleOfFifths(WolfsDenAudioProcessor& proc);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    WolfsDenAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircleOfFifths)
};

/** TASK_009: voicing colour grid — rows = extension type, cols = chords. */
class ColorsGrid : public juce::Component, private juce::Timer
{
public:
    explicit ColorsGrid(WolfsDenAudioProcessor& proc);
    ~ColorsGrid() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

    /** If set, invoked with (row 0–5, col 0–6) when a harmony cell is clicked. */
    std::function<void(int, int)> onColorsCell;

private:
    void timerCallback() override;
    WolfsDenAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorsGrid)
};

/**
 * TASK_009 THEORY page:
 *   Top: Key + Scale display | Detect MIDI | Detect Audio
 *   Sub-tabs: Browse Chords | Circle of Fifths | Explore | Colors
 *
 *   Browse Chords:
 *     Section A: 8 chord pads (active chord set)
 *     Section B: 7 diatonic degree pads
 *     Section C: Progression track area
 *     Right sidebar: Inversion / Density / Octave modifiers
 *
 *   Circle of Fifths: interactive wheel
 *   Explore: scrollable list of all available scales
 *   Colors: voicing grid with colour‐coding
 */
class TheoryPage : public juce::Component,
                   private juce::Timer
{
public:
    explicit TheoryPage(WolfsDenAudioProcessor& proc);
    ~TheoryPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void showSub(int i);
    void chordPadClicked(int padIndex);
    void degreePadClicked(int degreeIndex);
    void progressionAddClicked();
    void progressionClearClicked();
    void progressionSlotClicked(int slotIndex);
    void refreshProgressionSlotUi();
    void colorsCellClicked(int row, int col);

    /** Def index into TheoryEngine chord defs, or -1 for detector-driven pads 0–2. */
    std::array<int, 8> chordPadLibraryDefId_ {};
    struct ProgStep
    {
        int typeIdx = 0;
        int rootPc = 0;
    };
    std::array<std::optional<ProgStep>, 8> progressionSteps_ {};

    /** Short MIDI preview via shared keyboard state; released after a few timer ticks. */
    std::array<int, 8> previewNotes_ {};
    int previewCount_ = 0;
    int previewTicksLeft_ = 0;

    void stopChordPreview();
    void startChordPreview(int rootMidi, const int* iv, int nIv);
    void startChordPreviewFromIntervals(int rootPc012, const std::vector<int>& intervalsFromRoot);
    void applyChordTypeRootPreview(int chordTypeApvtsIdx, int rootPc012);
    void applyChordFromLibraryDefIndex(int defIndex, int rootPc012);

    WolfsDenAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // Sub-tab buttons
    juce::TextButton tabBrowse { "Browse chords" };
    juce::TextButton tabCof { "Circle of fifths" };
    juce::TextButton tabExplore { "Scales" };
    juce::TextButton tabColors { "Colors" };

    // Top bar controls
    juce::ComboBox scaleRoot;
    juce::ComboBox scaleType;
    juce::ToggleButton voiceLead { "Voice leading" };
    juce::TextButton detectMidi { "Detect MIDI" };
    juce::TextButton detectAudio { "Detect Audio" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attRoot;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attScale;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attVL;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attChordRoot;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attInv;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDensity;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attOct;

    // --- Browse Chords sub-tab ---
    juce::Component panelBrowse;
    juce::Label lblChordRoot;
    juce::ComboBox chordRootAnchor;
    std::array<juce::Label, 3> matchLabs {};       // top chord detection results
    std::array<juce::TextButton, 8> chordPads {};  // Section A: 8 chord pads
    std::array<juce::TextButton, 7> degreePads {}; // Section B: diatonic degrees
    juce::Label progressionHeader;
    juce::TextButton progressionAdd { "Add" };
    juce::TextButton progressionClear { "Clear" };
    std::array<juce::TextButton, 8> progressionSlots {};
    // Modifier sidebar
    juce::Label lblInv, lblDens, lblOct;
    juce::Slider inversionSlider;
    juce::Slider densitySlider;
    juce::Slider octaveSlider;

    // --- Circle of Fifths sub-tab ---
    CircleOfFifths cof;

    // --- Explore sub-tab ---
    juce::Component exploreInner;                       // MUST be declared before exploreVp (destroyed after)
    juce::Viewport exploreVp;
    juce::OwnedArray<juce::TextButton> exploreBtns;     // owns the dynamically-created buttons

    // --- Colors sub-tab ---
    ColorsGrid colorsGrid;

    int subTab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TheoryPage)
};

} // namespace wolfsden::ui
