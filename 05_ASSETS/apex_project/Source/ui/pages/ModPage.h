#pragma once

#include "../theme/UITheme.h"

#include "../../engine/ModMatrix.h"

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

class WolfsDenAudioProcessor;

namespace wolfsden
{
class PerfXyPad;
}

namespace wolfsden::ui
{

/**
 * TASK_009 MOD page:
 *   Left 40%: Mod Matrix table (32 slots, paged 8-at-a-time)
 *     Columns: Source | → | Target | Amount | Inv | Smooth | Mute
 *     Color-coded rows: LFO=cyan, Envelope=teal, MIDI=gold
 *     Red dot on target when out-of-range
 *
 *   Right 60%: XY Pad
 *     400x400 interactive square
 *     Physics mode: Direct | Inertia | Chaos
 *     Depth slider, Dice button, Record button
 */
class ModPage : public juce::Component
{
public:
    explicit ModPage(WolfsDenAudioProcessor& proc);
    ~ModPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void syncPageFromMatrix();
    void applyRow(int localRow);
    void diceAssignments();

    WolfsDenAudioProcessor& processor;

    juce::ComboBox slotPage;
    struct RowUi
    {
        juce::ComboBox src;
        juce::ComboBox tgt;
        juce::Slider amt { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::ToggleButton inv { "Inv" };
        juce::ToggleButton mute { "M" };
        juce::ComboBox scope;
    };
    std::array<RowUi, 8> rows;

    // XY pad section
    juce::ComboBox xyPhysics;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attXyPhys;
    juce::Slider xyDepth; // overall intensity
    juce::TextButton diceBtn { "Dice" };
    juce::TextButton recordBtn { "Record" };

    std::unique_ptr<wolfsden::PerfXyPad> xyPad;

    // XY recording
    struct XyRecordEvent { juce::int64 timeMs; float x; float y; };
    std::vector<XyRecordEvent> xyRecordBuffer;
    bool xyRecording = false;
    juce::int64 xyRecordStartMs = 0;
    void startXyRecord();
    juce::File stopXyRecordAndSave();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModPage)
};

} // namespace wolfsden::ui
