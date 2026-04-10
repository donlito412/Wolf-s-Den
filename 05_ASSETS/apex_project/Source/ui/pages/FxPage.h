#pragma once

#include "../theme/UITheme.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class WolfsDenAudioProcessor;

namespace wolfsden::ui
{

/**
 * TASK_009 FX page:
 *   - Layer selector: [A] [B] [C] [D] [COMMON] [MASTER]
 *   - 4 horizontal FX slots per rack
 *   - Each slot: FX name, On/Off LED toggle, wet/dry knob, expand for full params
 *   - Signal flow visual: Layer → Sum → Common → Master
 */
class FxPage : public juce::Component
{
public:
    explicit FxPage(WolfsDenAudioProcessor& proc);
    ~FxPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildRack();
    void expandSlot(int slotIndex);

    WolfsDenAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // Rack selector — tabs instead of combo
    std::array<juce::TextButton, 6> rackTab;
    int activeRack = 0; // 0=common, 1-4=layers, 5=master

    struct SlotRow
    {
        juce::Label lab;
        juce::ComboBox type;
        juce::Slider mix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::ToggleButton onOff { "On" };
        juce::TextButton expandBtn { "…" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
    };
    std::vector<std::unique_ptr<SlotRow>> slots;

    int expandedSlot = -1;

    // Legacy common-rack mix sliders (reverb/delay/chorus/compressor)
    juce::Slider revMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider delMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider choMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider compMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRev, attDel, attCho, attComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPage)
};

} // namespace wolfsden::ui
