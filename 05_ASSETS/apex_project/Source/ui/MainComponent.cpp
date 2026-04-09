#include "MainComponent.h"

#include "PerfXyPad.h"

#include "../PluginProcessor.h"
#include "../engine/ModMatrix.h"

namespace wolfsden
{
namespace
{
void styleSlider(juce::Slider& s, const juce::String& name)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    s.setName(name);
}

juce::String sourceLabel(ModMatrix::Source s)
{
    switch (s)
    {
        case ModMatrix::GlobalLFO:
            return "Global LFO";
        case ModMatrix::FilterEnv:
            return "Filter env";
        case ModMatrix::AmpEnv:
            return "Amp env";
        case ModMatrix::ModWheel:
            return "Mod wheel";
        case ModMatrix::Aftertouch:
            return "Aftertouch";
        case ModMatrix::Velocity:
            return "Velocity";
        case ModMatrix::PitchBend:
            return "Pitch bend";
        case ModMatrix::XY_X:
            return "XY X";
        case ModMatrix::XY_Y:
            return "XY Y";
        case ModMatrix::Random:
            return "Random";
        case ModMatrix::Off:
        case ModMatrix::NumSources:
        default:
            return "?";
    }
}

ModMatrix::Target targetForParamId(const juce::String& id)
{
    using T = ModMatrix::Target;
    if (id == "master_volume")
        return T::MasterVolumeMul;
    if (id == "lfo_rate")
        return T::LFO_RateMul;
    if (id == "lfo_depth")
        return T::LFO_DepthAdd;
    if (id == "layer0_filter_cutoff")
        return T::Layer0_FilterCutoffSemi;
    if (id == "layer0_filter_resonance")
        return T::Layer0_FilterRes;
    if (id == "layer1_filter_cutoff")
        return T::Layer1_FilterCutoffSemi;
    if (id == "layer1_filter_resonance")
        return T::Layer1_FilterRes;
    if (id == "layer2_filter_cutoff")
        return T::Layer2_FilterCutoffSemi;
    if (id == "layer2_filter_resonance")
        return T::Layer2_FilterRes;
    if (id == "layer3_filter_cutoff")
        return T::Layer3_FilterCutoffSemi;
    if (id == "layer3_filter_resonance")
        return T::Layer3_FilterRes;
    if (id == "fx_reverb_mix")
        return T::Fx_ReverbMixAdd;
    if (id == "fx_delay_mix")
        return T::Fx_DelayMixAdd;
    if (id == "fx_chorus_mix")
        return T::Fx_ChorusMixAdd;
    return T::None;
}

int firstFreeModSlot(ModMatrix& m)
{
    for (int i = 2; i < ModMatrix::kSlots; ++i)
        if (m.slotTgt[(size_t)i].load(std::memory_order_relaxed) == (int)ModMatrix::None)
            return i;
    return ModMatrix::kSlots - 1;
}

void assignSlot(ModMatrix& m, int slot, ModMatrix::Source src, ModMatrix::Target tgt)
{
    m.setSlot(slot, src, tgt, 0.5f, false, false, 0.08f, 0);
}

} // namespace

MainComponent::MainComponent(WolfsDenAudioProcessor& p)
    : processor(p)
{
    titleLabel.setText("Wolf's Den — modulation + automation test", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    xyPhysicsLabel.setText("XY physics", juce::dontSendNotification);
    xyPhysicsLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(xyPhysicsLabel);

    addAndMakeVisible(xyPhysicsCombo);

    auto& ap = processor.getAPVTS();
    xyPhysicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "perf_xy_physics", xyPhysicsCombo);

    styleSlider(masterVolSlider, "Master Volume");
    styleSlider(masterPanSlider, "Master Pan");
    styleSlider(layer0VolSlider, "Layer 1 Volume");
    styleSlider(layer0CutoffSlider, "Layer 1 Filter Cutoff");
    styleSlider(lfoRateSlider, "LFO Rate");
    styleSlider(lfoDepthSlider, "LFO Depth");
    styleSlider(reverbMixSlider, "Reverb Mix");
    styleSlider(layer0ResSlider, "Layer 1 Filter Resonance");
    styleSlider(delayMixSlider, "Delay Mix");
    styleSlider(chorusMixSlider, "Chorus Mix");

    addAndMakeVisible(masterVolSlider);
    addAndMakeVisible(masterPanSlider);
    addAndMakeVisible(layer0VolSlider);
    addAndMakeVisible(layer0CutoffSlider);
    addAndMakeVisible(lfoRateSlider);
    addAndMakeVisible(lfoDepthSlider);
    addAndMakeVisible(reverbMixSlider);
    addAndMakeVisible(layer0ResSlider);
    addAndMakeVisible(delayMixSlider);
    addAndMakeVisible(chorusMixSlider);

    attachMasterVol = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("master_volume")), masterVolSlider, nullptr);
    attachMasterPan = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("master_pan")), masterPanSlider, nullptr);
    attachLayer0Vol = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("layer0_volume")), layer0VolSlider, nullptr);
    attachLayer0Cutoff = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("layer0_filter_cutoff")), layer0CutoffSlider, nullptr);
    attachLfoRate = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("lfo_rate")), lfoRateSlider, nullptr);
    attachLfoDepth = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("lfo_depth")), lfoDepthSlider, nullptr);
    attachReverbMix = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("fx_reverb_mix")), reverbMixSlider, nullptr);
    attachLayer0Res = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("layer0_filter_resonance")), layer0ResSlider, nullptr);
    attachDelayMix = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("fx_delay_mix")), delayMixSlider, nullptr);
    attachChorusMix = std::make_unique<juce::SliderParameterAttachment>(
        *dynamic_cast<juce::AudioParameterFloat*>(ap.getParameter("fx_chorus_mix")), chorusMixSlider, nullptr);

    flexSliders = { { &masterVolSlider, "master_volume" },
                    { &masterPanSlider, "master_pan" },
                    { &layer0VolSlider, "layer0_volume" },
                    { &layer0CutoffSlider, "layer0_filter_cutoff" },
                    { &layer0ResSlider, "layer0_filter_resonance" },
                    { &lfoRateSlider, "lfo_rate" },
                    { &lfoDepthSlider, "lfo_depth" },
                    { &reverbMixSlider, "fx_reverb_mix" },
                    { &delayMixSlider, "fx_delay_mix" },
                    { &chorusMixSlider, "fx_chorus_mix" } };

    for (auto& pr : flexSliders)
        pr.first->addMouseListener(this, false);

    xyPad = std::make_unique<PerfXyPad>(processor);
    addAndMakeVisible(*xyPad);
}

MainComponent::~MainComponent()
{
    for (auto& pr : flexSliders)
        if (pr.first != nullptr)
            pr.first->removeMouseListener(this);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isPopupMenu())
        return;

    for (auto& pr : flexSliders)
    {
        if (e.eventComponent != pr.first)
            continue;

        const auto tgt = targetForParamId(pr.second);
        auto& mm = processor.getModMatrix();

        if (e.mods.isShiftDown())
        {
            juce::PopupMenu menu;
            if (tgt == ModMatrix::None)
            {
                menu.addItem("No mod matrix target mapped for this control", false, false, [] {});
            }
            else
            {
                int count = 0;
                for (int i = 0; i < ModMatrix::kSlots; ++i)
                {
                    if (mm.slotTgt[(size_t)i].load(std::memory_order_relaxed) != (int)tgt)
                        continue;
                    ++count;
                    const int src = mm.slotSrc[(size_t)i].load(std::memory_order_relaxed);
                    const float amt = mm.slotAmt[(size_t)i].load(std::memory_order_relaxed);
                    menu.addItem("Slot " + juce::String(i) + ": " + sourceLabel((ModMatrix::Source)src)
                                     + "  amt " + juce::String(amt, 2),
                                 false,
                                 false,
                                 [] {});
                }
                if (count == 0)
                    menu.addItem("No routings to this target", false, false, [] {});
            }
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pr.first));
            return;
        }

        if (tgt == ModMatrix::None)
        {
            juce::PopupMenu m;
            m.addItem("No mod matrix target for this parameter", false, false, [] {});
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pr.first));
            return;
        }

        juce::PopupMenu menu;
        menu.addItem("Modulate with Global LFO", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::GlobalLFO, tgt);
        });
        menu.addItem("Modulate with Filter env", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::FilterEnv, tgt);
        });
        menu.addItem("Modulate with Amp env", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::AmpEnv, tgt);
        });
        menu.addItem("Modulate with XY X", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::XY_X, tgt);
        });
        menu.addItem("Modulate with XY Y", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::XY_Y, tgt);
        });
        menu.addItem("Modulate with Random", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::Random, tgt);
        });
        menu.addItem("Modulate with Mod wheel", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::ModWheel, tgt);
        });
        menu.addItem("Modulate with Aftertouch", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::Aftertouch, tgt);
        });
        menu.addItem("Modulate with Velocity", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::Velocity, tgt);
        });
        menu.addItem("Modulate with Pitch bend", true, false, [&, tgt] {
            assignSlot(mm, firstFreeModSlot(mm), ModMatrix::PitchBend, tgt);
        });
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pr.first));
        return;
    }
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced(12);
    titleLabel.setBounds(r.removeFromTop(28));
    r.removeFromTop(6);

    auto topRow = r.removeFromTop(28);
    xyPhysicsLabel.setBounds(topRow.removeFromLeft(90));
    xyPhysicsCombo.setBounds(topRow.removeFromLeft(160));
    r.removeFromTop(8);

    auto padArea = r.removeFromRight(308).reduced(0, 0);
    xyPad->setBounds(padArea.removeFromTop(300));

    const int rowH = 40;
    for (auto* s : { &masterVolSlider,
                     &masterPanSlider,
                     &layer0VolSlider,
                     &layer0CutoffSlider,
                     &layer0ResSlider,
                     &lfoRateSlider,
                     &lfoDepthSlider,
                     &reverbMixSlider,
                     &delayMixSlider,
                     &chorusMixSlider })
        s->setBounds(r.removeFromTop(rowH));
}

} // namespace wolfsden
