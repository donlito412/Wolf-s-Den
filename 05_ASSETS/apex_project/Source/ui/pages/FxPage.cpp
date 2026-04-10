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

void styleH(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
}
} // namespace

// =============================================================================
// FxPage::ExpandedPanel
// =============================================================================

std::array<juce::String, 4> FxPage::paramLabelsForType(int typeIdx)
{
    // typeIdx matches fxTypeNames ordering in WolfsDenParameterLayout.cpp
    switch (typeIdx)
    {
        case 1:  return { "Threshold", "Ratio", "Attack", "Release" };  // Compressor
        case 2:  return { "Threshold", "Release", "Knee", "Makeup" };   // Limiter
        case 3:  return { "Threshold", "Attack", "Hold", "Release" };   // Gate
        case 4:  return { "Low shelf", "Low-mid", "High-mid", "Hi shelf" }; // Param EQ
        case 5:  return { "Frequency", "Resonance", "–", "–" };         // HPF
        case 6:  return { "Frequency", "Resonance", "–", "–" };         // LPF
        case 7:  return { "Drive", "Tone", "–", "–" };                  // Soft Clip
        case 8:  return { "Drive", "Tone", "–", "–" };                  // Hard Clip
        case 9:  return { "Bit depth", "Sample rate", "Mix", "–" };     // Bit Crusher
        case 10: return { "Drive", "Shape", "Tone", "Mix" };             // Waveshaper
        case 11: return { "Rate", "Depth", "Delay", "Mix" };             // Chorus
        case 12: return { "Rate", "Depth", "Feedback", "Mix" };          // Flanger
        case 13: return { "Rate", "Depth", "Feedback", "Mix" };          // Phaser
        case 14: return { "Rate", "Depth", "–", "–" };                  // Vibrato
        case 15: return { "Rate", "Depth", "–", "–" };                  // Tremolo
        case 16: return { "Rate", "Width", "–", "–" };                  // Auto-Pan
        case 17: return { "Time", "Feedback", "Damping", "Mix" };        // Stereo Delay
        case 18: return { "Time", "Feedback", "Damping", "Mix" };        // Ping-Pong Delay
        case 19: return { "Decay", "Pre-delay", "Damping", "Size" };     // Reverb hall
        case 20: return { "Decay", "Pre-delay", "Damping", "Brightness" }; // Reverb plate
        case 21: return { "Decay", "Pre-delay", "Damping", "Mix" };      // Reverb spring
        case 22: return { "Width", "–", "–", "–" };                     // Stereo width
        case 23: return { "Blend", "–", "–", "–" };                     // Mono blend
        default: return { "Param A", "Param B", "Param C", "Param D" };  // Off / unknown
    }
}

void FxPage::ExpandedPanel::build(juce::AudioProcessorValueTreeState& vts, int globalIdx, int typeIdx)
{
    const auto lblArr = FxPage::paramLabelsForType(typeIdx);
    const juce::String idx = juce::String(globalIdx).paddedLeft('0', 2);

    // For Parametric EQ (typeIdx == 4) bind to eq band params; otherwise generic p0-p3
    const bool isEq = (typeIdx == 4);

    for (int p = 0; p < 4; ++p)
    {
        atts[(size_t)p].reset();

        labels[(size_t)p].setText(lblArr[(size_t)p], juce::dontSendNotification);
        labels[(size_t)p].setFont(Theme::fontLabel());
        labels[(size_t)p].setColour(juce::Label::textColourId, Theme::textSecondary());

        if (lblArr[(size_t)p] == "–")
        {
            sliders[(size_t)p].setVisible(false);
            labels[(size_t)p].setVisible(false);
            addChildComponent(sliders[(size_t)p]);
            addChildComponent(labels[(size_t)p]);
            continue;
        }

        styleH(sliders[(size_t)p]);
        sliders[(size_t)p].setVisible(true);
        labels[(size_t)p].setVisible(true);
        addAndMakeVisible(sliders[(size_t)p]);
        addAndMakeVisible(labels[(size_t)p]);

        const juce::String paramId = isEq
            ? ("fx_s" + idx + "_eq" + juce::String(p))
            : ("fx_s" + idx + "_p"  + juce::String(p));

        if (vts.getParameter(paramId) != nullptr)
            atts[(size_t)p] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramId, sliders[(size_t)p]);
    }
}

void FxPage::ExpandedPanel::paint(juce::Graphics& g)
{
    g.setColour(Theme::backgroundMid().withAlpha(0.55f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f);
    g.setColour(Theme::accentPrimary().withAlpha(0.25f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f, 1.f);
}

void FxPage::ExpandedPanel::resized()
{
    auto r = getLocalBounds().reduced(8, 4);
    const int slotH = 22;
    for (int p = 0; p < 4; ++p)
    {
        if (!sliders[(size_t)p].isVisible()) continue;
        labels[(size_t)p].setBounds(r.removeFromTop(12));
        sliders[(size_t)p].setBounds(r.removeFromTop(slotH));
        r.removeFromTop(2);
    }
}

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

    addFxBtn.onClick = [this] {
        if (expandedSlot >= 0)
            showAddFxBrowser(expandedSlot);
    };
    addFxBtn.setColour(juce::TextButton::buttonColourId, Theme::accentPrimary().withAlpha(0.25f));
    addAndMakeVisible(addFxBtn);
    addFxBtn.setVisible(false); // shown only when a slot is expanded

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

        const juce::String typeId = pfx + "type";
        const juce::String mixId  = pfx + "mix";
        if (apvts.getParameter(typeId) != nullptr)
            row->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, typeId, row->type);
        if (apvts.getParameter(mixId) != nullptr)
            row->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, mixId, row->mix);

        // Expanded panel — built now; type changes will rebuild it
        const int globalIdx = globalSlotIdx(activeRack, s);
        const int initialType = [&] {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(typeId)))
                return p->getIndex();
            return 0;
        }();

        row->panel = std::make_unique<ExpandedPanel>();
        row->panel->build(apvts, globalIdx, initialType);
        row->panel->setVisible(false);
        addChildComponent(*row->panel);

        // Rebuild panel when FX type changes
        row->type.onChange = [this, s, globalIdx] {
            if ((size_t)s < slots.size() && slots[(size_t)s]->panel)
            {
                const int tid = slots[(size_t)s]->type.getSelectedItemIndex();
                slots[(size_t)s]->panel->build(apvts, globalIdx, tid);
                slots[(size_t)s]->panel->resized();
                repaint();
            }
        };

        row->expandBtn.onClick = [this, s] { expandSlot(s); };

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

    // Show / hide panels and update Add FX button visibility
    for (int s = 0; s < (int)slots.size(); ++s)
    {
        if (slots[(size_t)s]->panel)
            slots[(size_t)s]->panel->setVisible(s == expandedSlot);
    }
    addFxBtn.setVisible(expandedSlot >= 0);

    resized();
    repaint();
}

void FxPage::showAddFxBrowser(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= (int)slots.size()) return;

    // FX type names matching WolfsDenParameterLayout order
    static const char* kCategories[][5] = {
        { "Dynamics",   "Compressor", "Limiter", "Gate", nullptr },
        { "EQ / Filter","Parametric EQ (4-band)", "HPF (12 dB/oct)", "LPF (12 dB/oct)", nullptr },
        { "Distortion", "Soft Clip", "Hard Clip", "Bit Crusher", nullptr },
        { "Modulation", "Chorus", "Flanger", "Phaser", "Vibrato" },
        { "Time / Verb","Stereo Delay","Ping-Pong Delay","Reverb (algorithmic hall)","Reverb (bright / plate-style)" },
        { nullptr }
    };

    // Build a flat ordered list matching the APVTS combo box indices
    static const char* kAllTypes[] = {
        "Off", "Compressor", "Limiter", "Gate",
        "Parametric EQ (4-band)", "HPF (12 dB/oct)", "LPF (12 dB/oct)",
        "Soft Clip", "Hard Clip", "Bit Crusher", "Waveshaper",
        "Chorus", "Flanger", "Phaser", "Vibrato", "Tremolo", "Auto-Pan",
        "Stereo Delay", "Ping-Pong Delay",
        "Reverb (algorithmic hall)", "Reverb (bright / plate-style)", "Reverb (spring-style decay)",
        "Stereo width", "Mono blend (L+R)"
    };
    constexpr int kNumTypes = 24;

    juce::PopupMenu menu;
    menu.addSectionHeader("Select FX Type for Slot " + juce::String(slotIndex + 1));
    menu.addItem(1, "Off");
    menu.addSeparator();

    // Dynamics
    juce::PopupMenu dynSub;
    for (int t = 1; t <= 3; ++t) dynSub.addItem(t + 1, kAllTypes[(size_t)t]);
    menu.addSubMenu("Dynamics",  dynSub);

    // EQ / Filter
    juce::PopupMenu eqSub;
    for (int t = 4; t <= 6; ++t) eqSub.addItem(t + 1, kAllTypes[(size_t)t]);
    menu.addSubMenu("EQ & Filter", eqSub);

    // Distortion
    juce::PopupMenu distSub;
    for (int t = 7; t <= 10; ++t) distSub.addItem(t + 1, kAllTypes[(size_t)t]);
    menu.addSubMenu("Distortion",  distSub);

    // Modulation
    juce::PopupMenu modSub;
    for (int t = 11; t <= 16; ++t) modSub.addItem(t + 1, kAllTypes[(size_t)t]);
    menu.addSubMenu("Modulation",  modSub);

    // Time / Reverb
    juce::PopupMenu rvbSub;
    for (int t = 17; t <= 21; ++t) rvbSub.addItem(t + 1, kAllTypes[(size_t)t]);
    menu.addSubMenu("Delay & Reverb", rvbSub);

    // Stereo utils
    juce::PopupMenu stereoSub;
    for (int t = 22; t <= 23; ++t) stereoSub.addItem(t + 1, kAllTypes[(size_t)t]);
    menu.addSubMenu("Stereo Utilities", stereoSub);

    juce::ignoreUnused(kCategories, kNumTypes);

    auto* targetBtn = &slots[(size_t)slotIndex]->expandBtn;
    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(targetBtn)
                       .withParentComponent(this);

    menu.showMenuAsync(options, [this, slotIndex](int result) {
        if (result <= 0)
            return;

        const int newTypeIdx = result - 1; // result 1="Off"=idx 0, etc.

        juce::String pfx;
        if (activeRack == 0)
            pfx = "fx_c" + juce::String(slotIndex) + "_";
        else if (activeRack >= 1 && activeRack <= 4)
            pfx = "fx_l" + juce::String(activeRack - 1) + "_s" + juce::String(slotIndex) + "_";
        else
            pfx = "fx_m" + juce::String(slotIndex) + "_";

        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(pfx + "type")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1((float)newTypeIdx));
            p->endChangeGesture();
            slots[(size_t)slotIndex]->type.setSelectedItemIndex(newTypeIdx, juce::sendNotification);
        }
    });
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
    constexpr int kCollapsedH   = 60;
    constexpr int kExpandedH    = 160;  // header (60) + params panel (100)

    for (int i = 0; i < (int)slots.size(); ++i)
    {
        const bool isExpanded = (expandedSlot == i);
        const int slotH = isExpanded ? kExpandedH : kCollapsedH;
        auto slotR = r.removeFromTop(slotH);

        // Header row
        auto top = slotR.removeFromTop(kCollapsedH);
        slots[(size_t)i]->onOff.setBounds(top.removeFromLeft(40).reduced(4, 12));
        slots[(size_t)i]->lab.setBounds(top.removeFromLeft(60).reduced(2, 14));
        slots[(size_t)i]->type.setBounds(top.removeFromLeft(juce::jmax(140, top.getWidth() / 2)).reduced(2, 10));
        slots[(size_t)i]->expandBtn.setBounds(top.removeFromRight(36).reduced(4, 12));
        slots[(size_t)i]->mix.setBounds(top.reduced(4, 2));

        // Expanded panel
        if (slots[(size_t)i]->panel)
            slots[(size_t)i]->panel->setBounds(slotR.reduced(8, 2));

        r.removeFromTop(4);
    }

    // Add FX button — appears below the last slot when something is expanded
    if (expandedSlot >= 0)
        addFxBtn.setBounds(r.removeFromTop(26).removeFromLeft(110));
}

} // namespace wolfsden::ui
