#pragma once

#include "../theme/UITheme.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class WolfsDenAudioProcessor;

namespace wolfsden::ui
{

/** TASK_009: circle of 12 note buttons for root note picking. */
class RootNotePicker : public juce::Component
{
public:
    explicit RootNotePicker(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RootNotePicker)
};

/** TASK_009: mini keyboard showing which notes are in the active scale (highlighted). */
class MiniKeyboard : public juce::Component, private juce::Timer
{
public:
    explicit MiniKeyboard(WolfsDenAudioProcessor& proc);
    ~MiniKeyboard() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    WolfsDenAudioProcessor& processor;
    std::array<bool, 12> inScale {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniKeyboard)
};

/** TASK_009: visual 32-step velocity bar grid for the arpeggiator. */
class ArpStepGrid : public juce::Component, private juce::Timer
{
public:
    explicit ArpStepGrid(juce::AudioProcessorValueTreeState& apvts);
    ~ArpStepGrid() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void editStep(const juce::MouseEvent& e);
    juce::AudioProcessorValueTreeState& apvts;
    std::array<float, 32> velocities {};
    std::array<bool, 32> enabled {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpStepGrid)
};

/**
 * TASK_009 PERFORM page: 3-section layout
 *   Left: KEYS LOCK — mode selector, root note picker circle, scale dropdown, mini keyboard
 *   Center: CHORD MODE — on/off, chord type, inversion, scale constraint toggle
 *   Right: ARPEGGIATOR — on/off, rate, play mode, octaves, swing, latch, restart, 32-step grid, MIDI capture
 */
class PerformPage : public juce::Component
{
public:
    explicit PerformPage(WolfsDenAudioProcessor& proc);
    ~PerformPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildStepAttachments();

    WolfsDenAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // --- Keys Lock section ---
    juce::Label keysLockTitle;
    juce::ComboBox keysMode;
    RootNotePicker rootPicker;
    juce::ComboBox scalePicker;
    MiniKeyboard miniKb;

    // --- Chord Mode section ---
    juce::Label chordTitle;
    juce::ToggleButton chordMode { "Chord mode" };
    juce::ComboBox chordType;
    juce::Slider chordInv;
    juce::ToggleButton scaleConstraint { "Scale constraint" };

    // --- Arpeggiator section ---
    juce::Label arpTitle;
    juce::ToggleButton arpOn { "Arp" };
    juce::Slider arpRate;
    juce::ComboBox arpPattern;
    juce::Slider arpOct;
    juce::Slider arpSwing;
    juce::ToggleButton arpLatch { "Latch" };
    juce::ToggleButton arpRestart { "Restart" };
    juce::ToggleButton arpSync { "Sync host" };
    ArpStepGrid stepGrid;
    juce::TextButton midiCapture { "MIDI Capture" };

    // --- Per-step detail (below grid) ---
    juce::ComboBox stepSelect;
    juce::ToggleButton stepOn { "On" };
    juce::Slider stepVel;
    juce::Slider stepDur;
    juce::Slider stepTrn;
    juce::Slider stepRkt;

    // --- APVTS attachments ---
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attKeys;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attScale;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attChord;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attChordType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attChordInv;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attArpOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attArpRate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attArpPat;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attLatch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSwing;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attOct;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attSync;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attStepOn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attVel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDur;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attTrn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRkt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformPage)
};

} // namespace wolfsden::ui
