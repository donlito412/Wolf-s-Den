#pragma once

#include "../theme/UITheme.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

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

    WolfsDenAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // Sub-tab buttons
    juce::TextButton tabBrowse { "Browse chords" };
    juce::TextButton tabCof { "Circle of fifths" };
    juce::TextButton tabExplore { "Explore" };
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

    // --- Browse Chords sub-tab ---
    juce::Component panelBrowse;
    std::array<juce::Label, 3> matchLabs {};       // top chord detection results
    std::array<juce::TextButton, 8> chordPads {};  // Section A: 8 chord pads
    std::array<juce::TextButton, 7> degreePads {}; // Section B: diatonic degrees
    juce::Label progressionLabel;                   // Section C placeholder
    // Modifier sidebar
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
