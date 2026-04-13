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

    /** Compute a stable global slot index (0–23) from (rack, slot) for the fx_s{idx} param namespace. */
    static int globalSlotIdx(int rack, int slot) noexcept
    {
        if (rack == 0) return slot;                          // Common 0–3
        if (rack >= 1 && rack <= 4) return 4 + (rack - 1) * 4 + slot; // Layer 4–19
        return 20 + slot;                                    // Master 20–23
    }

    /** Returns context-sensitive labels for p0–p3 based on an FX type index. */
    static std::array<juce::String, 4> paramLabelsForType(int typeIdx);

    struct ExpandedPanel : public juce::Component
    {
        std::array<juce::Slider, 4> sliders;
        std::array<juce::Label,  4> labels;
        std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> atts;

        void build(juce::AudioProcessorValueTreeState& vts, int globalIdx, int typeIdx);
        void paint(juce::Graphics& g) override;
        void resized() override;
    };

    struct SlotRow
    {
        juce::Label lab;
        juce::ComboBox type;
        juce::Label mixLabel;
        juce::Slider mix { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::ToggleButton onOff { "On" };
        juce::TextButton expandBtn { "…" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<ExpandedPanel> panel;
    };
    std::vector<std::unique_ptr<SlotRow>> slots;

    int expandedSlot = -1;

    // Add FX button + popup browser
    juce::TextButton addFxBtn { "+ Add FX" };
    void showAddFxBrowser(int slotIndex);

    juce::Label lblRevMix, lblDelMix, lblChoMix, lblCompMix;

    // Legacy common-rack mix sliders (reverb/delay/chorus/compressor)
    juce::Slider revMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider delMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider choMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider compMix { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRev, attDel, attCho, attComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPage)
};

} // namespace wolfsden::ui
