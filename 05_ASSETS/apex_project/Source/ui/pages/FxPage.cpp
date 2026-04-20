#include "FxPage.h"

#include "../../PluginProcessor.h"
#include "../theme/WolfsDenLookAndFeel.h"

namespace wolfsden::ui
{
namespace
{
void styleKnob(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    WolfsDenLookAndFeel::configureRotarySlider(s);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 20);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentAlt());
    s.setColour(juce::Slider::thumbColourId, Theme::accentHot());
}

void styleH(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
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

        // Apply clean value formatting (overrides APVTS default which shows many decimals).
        if (isEq)
        {
            // EQ params are -18..+18 dB
            sliders[(size_t)p].textFromValueFunction = [](double v) -> juce::String {
                return juce::String(v, 1) + " dB";
            };
        }
        else
        {
            // Generic 0-1 params — show as percentage
            sliders[(size_t)p].textFromValueFunction = [](double v) -> juce::String {
                return juce::String(juce::roundToInt(v * 100.0)) + "%";
            };
        }
        sliders[(size_t)p].updateText();
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
    const int slotH = 24;
    for (int p = 0; p < 4; ++p)
    {
        if (!sliders[(size_t)p].isVisible()) continue;
        labels[(size_t)p].setBounds(r.removeFromTop(15));
        sliders[(size_t)p].setBounds(r.removeFromTop(slotH));
        r.removeFromTop(3);
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

    auto prepMixLbl = [](juce::Label& L, const juce::String& t) {
        L.setText(t, juce::dontSendNotification);
        L.setFont(Theme::fontLabel());
        L.setColour(juce::Label::textColourId, Theme::textSecondary());
        L.setJustificationType(juce::Justification::centred);
    };
    prepMixLbl(lblRevMix, "Reverb");
    prepMixLbl(lblDelMix, "Delay");
    prepMixLbl(lblChoMix, "Chorus");
    prepMixLbl(lblCompMix, "Comp");
    for (auto* L : { &lblRevMix, &lblDelMix, &lblChoMix, &lblCompMix })
        addAndMakeVisible(*L);

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

    // Clean percentage display on legacy mix knobs
    for (auto* s : { &revMix, &delMix, &choMix, &compMix })
    {
        s->textFromValueFunction = [](double v) -> juce::String {
            return juce::String(juce::roundToInt(v * 100.0)) + "%";
        };
        s->updateText();
    }

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

        styleH(row->mix);
        row->onOff.setToggleState(true, juce::dontSendNotification);

        const juce::String typeId = pfx + "type";
        const juce::String mixId  = pfx + "mix";
        
        for (auto& typeName : juce::StringArray({
            "Off", "Compressor", "Limiter", "Gate", "Parametric EQ (4-band)", 
            "HPF (12 dB/oct)", "LPF (12 dB/oct)", "Soft Clip", "Hard Clip", 
            "Bit Crusher", "Waveshaper", "Chorus", "Flanger", "Phaser", "Vibrato", 
            "Tremolo", "Auto-Pan", "Stereo Delay", "Ping-Pong Delay", 
            "Reverb (algorithmic hall)", "Reverb (bright / plate-style)", 
            "Reverb (spring-style decay)", "Stereo width", "Mono blend (L+R)"
        }))
            row->type.addItem(typeName, row->type.getNumItems() + 1);

        if (apvts.getParameter(typeId) != nullptr)
            row->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, typeId, row->type);
        if (apvts.getParameter(mixId) != nullptr)
            row->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, mixId, row->mix);

        // Clean percentage display on the per-slot mix slider
        row->mix.textFromValueFunction = [](double v) -> juce::String {
            return juce::String(juce::roundToInt(v * 100.0)) + "%";
        };
        row->mix.updateText();

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

        row->mixLabel.setText("Mix", juce::dontSendNotification);
        row->mixLabel.setFont(Theme::fontLabel());
        row->mixLabel.setColour(juce::Label::textColourId, Theme::textSecondary());
        row->mixLabel.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(row->lab);
        addAndMakeVisible(row->type);
        addAndMakeVisible(row->mixLabel);
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

    // Slot panel backgrounds — derive bounds from actual laid-out components
    for (int i = 0; i < (int)slots.size(); ++i)
    {
        auto labBounds = slots[(size_t)i]->lab.getBounds();
        auto slotBounds = labBounds.withRight(getWidth() - 12);
        if (expandedSlot == i && slots[(size_t)i]->panel != nullptr)
            slotBounds = slotBounds.getUnion(slots[(size_t)i]->panel->getBounds());
        else
            slotBounds = slotBounds.withBottom(slots[(size_t)i]->mix.getBounds().getBottom() + 4);
        g.setColour(Theme::backgroundMid().withAlpha(0.3f));
        g.fillRoundedRectangle(slotBounds.toFloat().reduced(2.f), 6.f);
    }
}

void FxPage::mouseDrag(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    // TODO: implement drag-to-reorder from slot handle
}

bool FxPage::isInterestedInDragSource(const SourceDetails& details)
{
    juce::ignoreUnused(details);
    return true;
}

void FxPage::itemDragMove(const SourceDetails& details)
{
    juce::ignoreUnused(details);
    dragHighlightSlot = -1;
    repaint();
}

void FxPage::itemDropped(const SourceDetails& details)
{
    juce::ignoreUnused(details);
    dragHighlightSlot = -1;
    repaint();
}

void FxPage::swapSlotParameters(int slotA, int slotB)
{
    juce::ignoreUnused(slotA, slotB);
    // TODO: implement parameter swap between slots
}

void FxPage::resized()
{
    auto r = getLocalBounds().reduced(12);
    r.removeFromTop(28);

    const int availH = r.getHeight();
    const bool compact = availH < 420;

    // Rack tabs
    auto tabRow = r.removeFromTop(compact ? 28 : 32);
    const int tabW = juce::jmin(88, tabRow.getWidth() / 6);
    for (int i = 0; i < 6; ++i)
        rackTab[(size_t)i].setBounds(tabRow.removeFromLeft(tabW).reduced(2, 2));

    r.removeFromTop(compact ? 20 : 32); // signal flow + gap

    // Legacy mix knobs (shown on Common rack)
    constexpr int kMixCap = 14;
    auto legRow = r.removeFromTop(compact ? 90 : 120);
    const int qw = juce::jmax(90, legRow.getWidth() / 4);
    {
        auto c = legRow.removeFromLeft(qw).reduced(4, 2);
        lblRevMix.setBounds(c.removeFromTop(kMixCap));
        revMix.setBounds(c);
    }
    {
        auto c = legRow.removeFromLeft(qw).reduced(4, 2);
        lblDelMix.setBounds(c.removeFromTop(kMixCap));
        delMix.setBounds(c);
    }
    {
        auto c = legRow.removeFromLeft(qw).reduced(4, 2);
        lblChoMix.setBounds(c.removeFromTop(kMixCap));
        choMix.setBounds(c);
    }
    {
        auto c = legRow.removeFromLeft(qw).reduced(4, 2);
        lblCompMix.setBounds(c.removeFromTop(kMixCap));
        compMix.setBounds(c);
    }
    r.removeFromTop(8);

    // FX slots
    const int kCollapsedH   = compact ? 44 : 66;
    const int kExpandedH    = compact ? 110 : 160;  // header + params panel

    for (int i = 0; i < (int)slots.size(); ++i)
    {
        const bool isExpanded = (expandedSlot == i);
        const int slotH = isExpanded ? kExpandedH : kCollapsedH;
        auto slotR = r.removeFromTop(slotH);

        // Header row
        auto top = slotR.removeFromTop(kCollapsedH);
        const int vPad = compact ? 6 : 12;
        const int vPad2 = compact ? 4 : 8;
        slots[(size_t)i]->onOff.setBounds(top.removeFromLeft(40).reduced(4, vPad));
        slots[(size_t)i]->lab.setBounds(top.removeFromLeft(60).reduced(2, vPad + 2));
        slots[(size_t)i]->type.setBounds(top.removeFromLeft(juce::jmax(220, top.getWidth() * 55 / 100)).reduced(2, vPad2));
        slots[(size_t)i]->expandBtn.setBounds(top.removeFromRight(36).reduced(4, vPad));
        auto mixCol = top.reduced(4, 2);
        slots[(size_t)i]->mixLabel.setBounds(mixCol.removeFromTop(compact ? 9 : 11));
        slots[(size_t)i]->mix.setBounds(mixCol.removeFromTop(juce::jmax(18, mixCol.getHeight())));

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
