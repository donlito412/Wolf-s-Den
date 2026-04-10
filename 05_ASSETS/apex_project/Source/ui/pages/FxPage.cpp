#include "FxPage.h"

#include "../../PluginProcessor.h"

namespace wolfsden::ui
{
namespace
{
void styleKnob(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentPrimary());
    s.setColour(juce::Slider::thumbColourId, Theme::accentHot());
}
} // namespace

FxPage::FxPage(WolfsDenAudioProcessor& proc)
    : processor(proc)
    , apvts(proc.getAPVTS())
{
    static const char* tabNames[] = { "COMMON", "A", "B", "C", "D", "MASTER" };
    for (int i = 0; i < 6; ++i)
    {
        rackTab[(size_t)i].setButtonText(tabNames[(size_t)i]);
        rackTab[(size_t)i].setClickingTogglesState(false);
        rackTab[(size_t)i].onClick = [this, i] {
            activeRack = i;
            for (int k = 0; k < 6; ++k)
                rackTab[(size_t)k].setColour(juce::TextButton::buttonColourId,
                                              k == activeRack ? Theme::accentPrimary().withAlpha(0.45f) : Theme::panelSurface());
            rebuildRack();
        };
        addAndMakeVisible(rackTab[(size_t)i]);
    }
    rackTab[0].setColour(juce::TextButton::buttonColourId, Theme::accentPrimary().withAlpha(0.45f));

    // Legacy mix knobs
    for (auto* s : { &revMix, &delMix, &choMix, &compMix })
    {
        styleKnob(*s);
        addAndMakeVisible(*s);
    }
    attRev = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "fx_reverb_mix", revMix);
    attDel = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "fx_delay_mix", delMix);
    attCho = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "fx_chorus_mix", choMix);
    attComp = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "fx_compressor", compMix);

    rebuildRack();
    juce::ignoreUnused(processor);
}

FxPage::~FxPage() = default;

void FxPage::rebuildRack()
{
    slots.clear();
    expandedSlot = -1;

    for (int s = 0; s < 4; ++s)
    {
        juce::String pfx;
        if (activeRack == 0)
            pfx = "fx_c" + juce::String(s) + "_";
        else if (activeRack >= 1 && activeRack <= 4)
            pfx = "fx_l" + juce::String(activeRack - 1) + "_s" + juce::String(s) + "_";
        else
            pfx = "fx_m" + juce::String(s) + "_";

        auto row = std::make_unique<SlotRow>();
        row->lab.setText("Slot " + juce::String(s + 1), juce::dontSendNotification);
        row->lab.setFont(Theme::fontPanelHeader());
        row->lab.setColour(juce::Label::textColourId, Theme::textPrimary());

        styleKnob(row->mix);

        row->onOff.setToggleState(true, juce::dontSendNotification);
        row->expandBtn.onClick = [this, s] { expandSlot(s); };

        const juce::String typeId = pfx + "type";
        const juce::String mixId = pfx + "mix";
        if (apvts.getParameter(typeId) != nullptr)
            row->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, typeId, row->type);
        if (apvts.getParameter(mixId) != nullptr)
            row->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, mixId, row->mix);

        addAndMakeVisible(row->lab);
        addAndMakeVisible(row->type);
        addAndMakeVisible(row->mix);
        addAndMakeVisible(row->onOff);
        addAndMakeVisible(row->expandBtn);
        slots.push_back(std::move(row));
    }

    resized();
}

void FxPage::expandSlot(int slotIndex)
{
    expandedSlot = (expandedSlot == slotIndex) ? -1 : slotIndex;
    resized();
    repaint();
}

void FxPage::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    // Page title
    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPageHeader());
    g.drawText("FX", getLocalBounds().removeFromTop(28).reduced(12, 0), juce::Justification::centredLeft);

    // Signal flow indicator
    auto r = getLocalBounds().reduced(12);
    r.removeFromTop(28);
    auto tabRow = r.removeFromTop(32);
    juce::ignoreUnused(tabRow);

    // Draw signal flow diagram below tabs
    auto flowR = r.removeFromTop(24);
    g.setFont(Theme::fontLabel());
    g.setColour(Theme::textDisabled());

    const int segments = 4;
    const int segW = flowR.getWidth() / segments;
    static const char* flowLabels[] = { "Layer FX", "→ Sum", "→ Common FX", "→ Master FX" };
    for (int i = 0; i < segments; ++i)
    {
        auto s = flowR.removeFromLeft(segW);
        g.setColour((activeRack == 0 && i == 2) || (activeRack >= 1 && activeRack <= 4 && i == 0)
                     || (activeRack == 5 && i == 3)
                     ? Theme::accentHot() : Theme::textDisabled());
        g.drawText(flowLabels[(size_t)i], s, juce::Justification::centred);
    }

    // Slot panel backgrounds
    auto slotsR = getLocalBounds().reduced(12);
    slotsR.removeFromTop(28 + 32 + 24 + 8 + 72);
    for (int i = 0; i < (int)slots.size(); ++i)
    {
        auto slotBounds = slots[(size_t)i]->lab.getBounds().withRight(getWidth() - 12).withHeight(
            expandedSlot == i ? 120 : 60);
        g.setColour(Theme::backgroundMid().withAlpha(0.3f));
        g.fillRoundedRectangle(slotBounds.toFloat().reduced(2.f), 6.f);
    }
}

void FxPage::resized()
{
    auto r = getLocalBounds().reduced(12);
    r.removeFromTop(28);

    // Rack tabs
    auto tabRow = r.removeFromTop(32);
    const int tabW = juce::jmin(88, tabRow.getWidth() / 6);
    for (int i = 0; i < 6; ++i)
        rackTab[(size_t)i].setBounds(tabRow.removeFromLeft(tabW).reduced(2, 2));

    r.removeFromTop(24 + 8); // signal flow + gap

    // Legacy mix knobs (shown on Common rack)
    auto legRow = r.removeFromTop(72);
    const int qw = juce::jmax(72, legRow.getWidth() / 4);
    revMix.setBounds(legRow.removeFromLeft(qw).reduced(4, 2));
    delMix.setBounds(legRow.removeFromLeft(qw).reduced(4, 2));
    choMix.setBounds(legRow.removeFromLeft(qw).reduced(4, 2));
    compMix.setBounds(legRow.removeFromLeft(qw).reduced(4, 2));
    r.removeFromTop(8);

    // FX slots
    for (int i = 0; i < (int)slots.size(); ++i)
    {
        const int slotH = (expandedSlot == i) ? 120 : 60;
        auto slotR = r.removeFromTop(slotH);

        auto top = slotR.removeFromTop(60);
        slots[(size_t)i]->onOff.setBounds(top.removeFromLeft(40).reduced(4, 12));
        slots[(size_t)i]->lab.setBounds(top.removeFromLeft(60).reduced(2, 14));
        slots[(size_t)i]->type.setBounds(top.removeFromLeft(juce::jmax(140, top.getWidth() / 2)).reduced(2, 10));
        slots[(size_t)i]->expandBtn.setBounds(top.removeFromRight(36).reduced(4, 12));
        slots[(size_t)i]->mix.setBounds(top.reduced(4, 2));

        r.removeFromTop(4);
    }
}

} // namespace wolfsden::ui
