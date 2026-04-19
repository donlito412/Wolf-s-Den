#include "SynthPage.h"

#include "../../PluginProcessor.h"
#include "../theme/WolfsDenLookAndFeel.h"

namespace wolfsden::ui
{
namespace
{
static constexpr const char* kOscBtnNames[8] = { "Sine", "Saw", "Square", "Tri", "WT", "Grain", "FM", "Smp" };

void addS(juce::AudioProcessorValueTreeState& apvts,
          const juce::String& id,
          juce::Slider& sl,
          std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>& v)
{
    if (apvts.getParameter(id) != nullptr)
        v.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, sl));
}

void addC(juce::AudioProcessorValueTreeState& apvts,
          const juce::String& id,
          juce::ComboBox& cb,
          std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>>& v)
{
    if (apvts.getParameter(id) != nullptr)
        v.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, id, cb));
}

void prepCap(juce::Label& L, const juce::String& t, juce::Justification j = juce::Justification::centred)
{
    L.setText(t, juce::dontSendNotification);
    L.setFont(Theme::fontLabel());
    L.setColour(juce::Label::textColourId, Theme::textSecondary());
    L.setJustificationType(j);
}
} // namespace

void AdsrPreview::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());
    g.setColour(Theme::textDisabled());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f), 4.f, 1.f);

    if (sa == nullptr || sd == nullptr || ss == nullptr || sr == nullptr)
        return;

    const float a = juce::jlimit(0.001f, 10.f, (float)sa->getValue());
    const float d = juce::jlimit(0.001f, 10.f, (float)sd->getValue());
    const float sus = juce::jlimit(0.f, 1.f, (float)ss->getValue());
    const float r = juce::jlimit(0.001f, 15.f, (float)sr->getValue());
    const float sum = a + d + r + 0.2f;
    const float scaleX = (float)getWidth() / sum;

    juce::Path p;
    auto rf = getLocalBounds().toFloat().reduced(6.f, 10.f);
    const float y0 = rf.getBottom();
    const float y1 = rf.getY();
    float x = rf.getX();
    p.startNewSubPath(x, y0);
    p.lineTo(x, y1);
    x += a * scaleX;
    p.lineTo(x, y1);
    const float yS = y0 - (y0 - y1) * sus;
    x += d * scaleX;
    p.lineTo(x, yS);
    const float xSusEnd = rf.getRight() - r * scaleX;
    p.lineTo(xSusEnd, yS);
    p.lineTo(rf.getRight(), y0);
    p.lineTo(rf.getX(), y0);
    p.closeSubPath();

    g.setColour(Theme::accentPrimary().withAlpha(0.35f));
    g.fillPath(p);
    g.setColour(Theme::accentHot());
    g.strokePath(p, juce::PathStrokeType(1.2f));
}

void WaveformPreview::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());
    g.setColour(Theme::textDisabled());
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f), 4.f, 1.f);
    g.setColour(Theme::accentAlt().withAlpha(0.4f));
    const auto r = getLocalBounds().toFloat().reduced(8.f);
    for (int i = 0; i < (int)r.getWidth(); i += 6)
    {
        const float t = (float)i / r.getWidth();
        const float y = r.getCentreY() + std::sin(t * 12.f) * r.getHeight() * 0.35f;
        g.fillEllipse(r.getX() + (float)i, y, 3.f, 3.f);
    }
    g.setColour(Theme::textSecondary());
    g.setFont(Theme::fontLabel());
    g.drawText("Waveform", r.toNearestInt(), juce::Justification::centred);
}

void SynthPage::styleKnob(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    WolfsDenLookAndFeel::configureRotarySlider(s);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 22);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentAlt());
    s.setColour(juce::Slider::thumbColourId, Theme::accentHot());
}

void SynthPage::styleHSlider(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
}

void SynthPage::styleVFader(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 22);
}

SynthPage::SynthPage(WolfsDenAudioProcessor& proc)
    : processor(proc)
    , apvts(proc.getAPVTS())
{
    setOpaque(true);
    for (int i = 0; i < 4; ++i)
    {
        layerTab[(size_t)i].setButtonText(juce::String::charToString((juce::juce_wchar)('A' + i)));
        layerTab[(size_t)i].setClickingTogglesState(false);
        layerTab[(size_t)i].onClick = [this, i] { setActiveLayer(i); };
        addAndMakeVisible(layerTab[(size_t)i]);
    }

    for (int i = 0; i < 8; ++i)
    {
        oscModeBtn[(size_t)i].setButtonText(kOscBtnNames[(size_t)i]);
        oscModeBtn[(size_t)i].setClickingTogglesState(false);
        oscModeBtn[(size_t)i].onClick = [this, i] {
            const juce::String id = layerKey("osc_type");
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
                c->setValueNotifyingHost(c->convertTo0to1((float)i));
            syncOscButtons();
        };
        addAndMakeVisible(oscModeBtn[(size_t)i]);
    }

    addAndMakeVisible(wavePreview);
    granularHint.setText("",  juce::dontSendNotification); // hidden – replaced by real controls below
    granularHint.setVisible(false);

    // Granular controls
    static const char* kGranLabels[4] = { "Position", "Size", "Density", "Scatter" };
    juce::Slider* granSliders[4] = { &granPos, &granSize, &granDensity, &granScatter };
    juce::Label*  granLabels[4]  = { &lblGranPos, &lblGranSize, &lblGranDensity, &lblGranScatter };
    for (int i = 0; i < 4; ++i)
    {
        styleHSlider(*granSliders[(size_t)i]);
        granSliders[(size_t)i]->setVisible(false);
        addAndMakeVisible(*granSliders[(size_t)i]);

        granLabels[(size_t)i]->setText(kGranLabels[(size_t)i], juce::dontSendNotification);
        granLabels[(size_t)i]->setFont(Theme::fontLabel());
        granLabels[(size_t)i]->setColour(juce::Label::textColourId, Theme::textSecondary());
        granLabels[(size_t)i]->setJustificationType(juce::Justification::centredLeft);
        granLabels[(size_t)i]->setVisible(false);
        addAndMakeVisible(*granLabels[(size_t)i]);
    }

    // WT Controls
    for (int i = 0; i < kNumFactoryWavetables; ++i)
    {
        wtA.addItem(kFactoryWavetables[i].name, i + 1);
        wtB.addItem(kFactoryWavetables[i].name, i + 1);
    }

    wtA.setVisible(false);
    wtB.setVisible(false);
    loadWtABtn.setVisible(false);
    loadWtBBtn.setVisible(false);
    wavetableMorph.setVisible(false);
    lblWtMorph.setVisible(false);

    addAndMakeVisible(wtA);
    addAndMakeVisible(wtB);
    addAndMakeVisible(loadWtABtn);
    addAndMakeVisible(loadWtBBtn);

    styleKnob(wavetableMorph);
    addAndMakeVisible(wavetableMorph);

    prepCap(lblWtMorph, "MORPH A->B");
    addAndMakeVisible(lblWtMorph);

    // Sample controls
    styleHSlider(sampleStart);
    styleHSlider(sampleEnd);
    sampleStart.setRange(0.0, 1.0, 0.001);
    sampleEnd.setRange(0.0, 1.0, 0.001);
    sampleStart.setVisible(false);
    sampleEnd.setVisible(false);
    addAndMakeVisible(sampleStart);
    addAndMakeVisible(sampleEnd);

    sampleLoopBtn.setClickingTogglesState(true);
    sampleLoopBtn.setVisible(false);
    addAndMakeVisible(sampleLoopBtn);

    keymapBtn.setVisible(false);
    addAndMakeVisible(keymapBtn);

    prepCap(lblSampleStart, "Loop Start");
    prepCap(lblSampleEnd, "Loop End");
    lblSampleStart.setVisible(false);
    lblSampleEnd.setVisible(false);
    addAndMakeVisible(lblSampleStart);
    addAndMakeVisible(lblSampleEnd);

    // Keymap button handler
    keymapBtn.onClick = [this] {
        showKeymapDialog();
    };

    auto loadWavetable = [this](int slot) {
        // We'd ideally use FileChooser, but keeping it simple for the task structure
        // In a real implementation this opens a dialog.
        // For now, we simulate success if they pick a file.
        juce::FileChooser chooser("Select Wavetable", juce::File{}, "*.wav");
        chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, slot](const juce::FileChooser& fc) {
                if (fc.getResult().exists()) {
                    processor.getSynthEngine().loadLayerWavetableFromFile(activeLayer, slot, fc.getResult());
                }
            });
    };

    loadWtABtn.onClick = [=] { loadWavetable(0); };
    loadWtBBtn.onClick = [=] { loadWavetable(1); };

    auto prep = [](juce::Label& L, const juce::String& t, juce::Colour col) {
        L.setText(t, juce::dontSendNotification);
        L.setFont(Theme::fontLabel());
        L.setColour(juce::Label::textColourId, col);
        L.setJustificationType(juce::Justification::centredLeft);
    };
    prep(lblFilter1,    "Filter 1",  Theme::textPrimary());
    prep(lblFilter2Header, "Filter 2", Theme::textPrimary());
    prep(lblFilter2,    "",          Theme::textDisabled()); // kept for compat, hidden
    prep(lblFilterTopology, "Routing", Theme::textSecondary());
    prep(lblDrivePlanGap, "", Theme::textDisabled());
    prep(lblLfoPlanGap,   "", Theme::textDisabled());
    lblFilter2.setVisible(false);
    for (auto* L : { &lblFilter1, &lblFilter2Header, &lblFilterTopology, &lblDrivePlanGap, &lblLfoPlanGap })
        addAndMakeVisible(*L);

    prepCap(lblOct, "Octave");
    prepCap(lblSemi, "Semitone");
    prepCap(lblFine, "Fine");
    prepCap(lblCut1, "Cutoff");
    prepCap(lblRes1, "Res");
    prepCap(lblDrv1, "Drive");
    prepCap(lblCut2, "Cutoff");
    prepCap(lblRes2, "Res");
    prepCap(lblDrv2, "Drive");
    prepCap(lblFilAtk, "Attack", juce::Justification::centredRight);
    prepCap(lblFilDec, "Decay", juce::Justification::centredRight);
    prepCap(lblFilSus, "Sustain", juce::Justification::centredRight);
    prepCap(lblFilRel, "Release", juce::Justification::centredRight);
    prepCap(lblAmpAtk, "Attack", juce::Justification::centredRight);
    prepCap(lblAmpDec, "Decay", juce::Justification::centredRight);
    prepCap(lblAmpSus, "Sustain", juce::Justification::centredRight);
    prepCap(lblAmpRel, "Release", juce::Justification::centredRight);
    prepCap(lblUniV, "Voices");
    prepCap(lblUniDet, "Detune");
    prepCap(lblUniSpr, "Spread");
    prepCap(lblLevel, "Level");
    prepCap(lblPan, "Pan");
    prepCap(lblL1R, "Rate");
    prepCap(lblL1D, "Depth");
    prepCap(lblL1Sh, "Shape");
    prepCap(lblL2R, "Rate");
    prepCap(lblL2D, "Depth");
    prepCap(lblL2Sh, "Shape");
    for (auto* L : { &lblOct, &lblSemi, &lblFine, &lblCut1, &lblRes1, &lblDrv1, &lblCut2, &lblRes2, &lblDrv2,
                    &lblFilAtk, &lblFilDec, &lblFilSus, &lblFilRel, &lblAmpAtk, &lblAmpDec, &lblAmpSus, &lblAmpRel,
                    &lblUniV, &lblUniDet, &lblUniSpr, &lblLevel, &lblPan, &lblL1R, &lblL1D, &lblL1Sh, &lblL2R, &lblL2D, &lblL2Sh })
        addAndMakeVisible(*L);

    // OSC tune knobs — Octave / Semitone / Fine
    styleKnob(oscOctave);  oscOctave.setRange(-3, 3, 1);
    styleKnob(oscCoarse);
    styleKnob(oscFine);
    addAndMakeVisible(oscOctave);
    addAndMakeVisible(oscCoarse);
    addAndMakeVisible(oscFine);

    // Filter 1
    addAndMakeVisible(filterType);
    styleKnob(fCut);
    styleKnob(fRes);
    styleKnob(fDrive);
    addAndMakeVisible(fCut);
    addAndMakeVisible(fRes);
    addAndMakeVisible(fDrive);

    // Filter 2 (lblFilter2Header already added via label loop above)
    addAndMakeVisible(filter2Type);
    styleKnob(fCut2);
    styleKnob(fRes2);
    styleKnob(fDrive2);
    addAndMakeVisible(fCut2);
    addAndMakeVisible(fRes2);
    addAndMakeVisible(fDrive2);

    addAndMakeVisible(filterRoute);

    styleHSlider(filEnvA);
    styleHSlider(filEnvD);
    styleHSlider(filEnvS);
    styleHSlider(filEnvR);
    for (auto* s : { &filEnvA, &filEnvD, &filEnvS, &filEnvR })
        addAndMakeVisible(*s);
    filEnvShape.setSources(&filEnvA, &filEnvD, &filEnvS, &filEnvR);
    addAndMakeVisible(filEnvShape);

    styleHSlider(ampA);
    styleHSlider(ampD);
    styleHSlider(ampS);
    styleHSlider(ampR);
    for (auto* s : { &ampA, &ampD, &ampS, &ampR })
        addAndMakeVisible(*s);
    ampShape.setSources(&ampA, &ampD, &ampS, &ampR);
    addAndMakeVisible(ampShape);

    styleVFader(ampVol);
    ampVol.setRange(0, 1, 0);
    addAndMakeVisible(ampVol);

    styleKnob(ampPan);
    addAndMakeVisible(ampPan);

    styleKnob(uniVoices);
    styleKnob(uniDetune);
    styleKnob(uniSpread);
    addAndMakeVisible(uniVoices);
    addAndMakeVisible(uniDetune);
    addAndMakeVisible(uniSpread);

    styleKnob(lfo1Rate);
    styleKnob(lfo1Depth);
    addAndMakeVisible(lfo1Rate);
    addAndMakeVisible(lfo1Depth);
    addAndMakeVisible(lfo1Shape);

    styleKnob(lfo2Rate);
    styleKnob(lfo2Depth);
    addAndMakeVisible(lfo2Rate);
    addAndMakeVisible(lfo2Depth);
    addAndMakeVisible(lfo2Shape);

    addS(apvts, "filter_adsr_attack", filEnvA, globalSAtt);
    addS(apvts, "filter_adsr_decay", filEnvD, globalSAtt);
    addS(apvts, "filter_adsr_sustain", filEnvS, globalSAtt);
    addS(apvts, "filter_adsr_release", filEnvR, globalSAtt);
    addS(apvts, "lfo_rate", lfo1Rate, globalSAtt);
    addS(apvts, "lfo_depth", lfo1Depth, globalSAtt);
    
    for (auto& s : juce::StringArray({ "Sine", "Triangle", "Saw Up", "Square", "Random", "S&H" }))
        lfo1Shape.addItem(s, lfo1Shape.getNumItems() + 1);
        
    addC(apvts, "lfo_shape", lfo1Shape, globalCAtt);

    for (auto* s : { &filEnvA, &filEnvD, &filEnvS, &filEnvR, &ampA, &ampD, &ampS, &ampR })
        s->onValueChange = [this] {
            filEnvShape.repaint();
            ampShape.repaint();
        };


    setActiveLayer(0);
    startTimerHz(15);
    juce::ignoreUnused(processor);
}

SynthPage::~SynthPage() = default;

juce::String SynthPage::layerKey(const juce::String& tail) const
{
    return "layer" + juce::String(activeLayer) + "_" + tail;
}

void SynthPage::clearLayerBindings()
{
    layerSAtt.clear();
    layerCAtt.clear();
}

void SynthPage::bindLayer(int layerIndex)
{
    juce::ignoreUnused(layerIndex);
    // OSC
    addS(apvts, layerKey("tune_coarse"),  oscCoarse, layerSAtt);
    addS(apvts, layerKey("tune_fine"),    oscFine,   layerSAtt);
    addS(apvts, layerKey("tune_octave"),  oscOctave, layerSAtt);

    // Wavetable
    addC(apvts, layerKey("wt_index_a"), wtA, layerCAtt);
    addC(apvts, layerKey("wt_index_b"), wtB, layerCAtt);
    addS(apvts, layerKey("wavetable_morph"), wavetableMorph, layerSAtt);

    // Filter 1
    filterType.clear();
    for (auto& s : juce::StringArray({ "LP24", "LP12", "BP", "HP12", "Notch", "HP24", "Comb", "Formant" }))
        filterType.addItem(s, filterType.getNumItems() + 1);
    addC(apvts, layerKey("filter_type"),      filterType, layerCAtt);
    addS(apvts, layerKey("filter_cutoff"),    fCut,   layerSAtt);
    addS(apvts, layerKey("filter_resonance"), fRes,   layerSAtt);
    addS(apvts, layerKey("filter_drive"),     fDrive, layerSAtt);
    // Filter 2
    filter2Type.clear();
    for (auto& s : juce::StringArray({ "LP24", "LP12", "BP", "HP12", "Notch", "HP24", "Comb", "Formant" }))
        filter2Type.addItem(s, filter2Type.getNumItems() + 1);
    addC(apvts, layerKey("filter2_type"),      filter2Type, layerCAtt);
    addS(apvts, layerKey("filter2_cutoff"),    fCut2,   layerSAtt);
    addS(apvts, layerKey("filter2_resonance"), fRes2,   layerSAtt);
    addS(apvts, layerKey("filter2_drive"),     fDrive2, layerSAtt);
    
    filterRoute.clear();
    for (auto& s : juce::StringArray({ "Serial", "Parallel" }))
        filterRoute.addItem(s, filterRoute.getNumItems() + 1);
    addC(apvts, layerKey("filter_routing"), filterRoute, layerCAtt);
    // Granular
    addS(apvts, layerKey("gran_pos"),     granPos,     layerSAtt);
    addS(apvts, layerKey("gran_size"),    granSize,    layerSAtt);
    addS(apvts, layerKey("gran_density"), granDensity, layerSAtt);
    addS(apvts, layerKey("gran_scatter"), granScatter, layerSAtt);

    // Sample controls
    addS(apvts, layerKey("sample_start"), sampleStart, layerSAtt);
    addS(apvts, layerKey("sample_end"),   sampleEnd,   layerSAtt);

    addS(apvts, layerKey("amp_attack"), ampA, layerSAtt);
    addS(apvts, layerKey("amp_decay"), ampD, layerSAtt);
    addS(apvts, layerKey("amp_sustain"), ampS, layerSAtt);
    addS(apvts, layerKey("amp_release"), ampR, layerSAtt);

    addS(apvts, layerKey("volume"), ampVol, layerSAtt);
    addS(apvts, layerKey("pan"), ampPan, layerSAtt);

    addS(apvts, layerKey("unison_voices"), uniVoices, layerSAtt);
    addS(apvts, layerKey("unison_detune"), uniDetune, layerSAtt);
    addS(apvts, layerKey("unison_spread"), uniSpread, layerSAtt);

    addS(apvts, layerKey("lfo2_rate"), lfo2Rate, layerSAtt);
    addS(apvts, layerKey("lfo2_depth"), lfo2Depth, layerSAtt);
    lfo2Shape.clear();
    for (auto& s : juce::StringArray({ "Sine", "Triangle", "Saw Up", "Square", "Random", "S&H" }))
        lfo2Shape.addItem(s, lfo2Shape.getNumItems() + 1);
    addC(apvts, layerKey("lfo2_shape"), lfo2Shape, layerCAtt);
}

void SynthPage::setActiveLayer(int layerIndex)
{
    activeLayer = juce::jlimit(0, 3, layerIndex);
    clearLayerBindings();
    bindLayer(activeLayer);
    for (int i = 0; i < 4; ++i)
        layerTab[(size_t)i].setColour(juce::TextButton::buttonColourId,
                                      i == activeLayer ? Theme::accentPrimary().withAlpha(0.5f) : Theme::panelSurface());
    syncOscButtons();
    resized();
}

void SynthPage::syncOscButtons()
{
    const juce::String id = layerKey("osc_type");
    int idx = 0;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
        idx = c->getIndex();

    for (int i = 0; i < 8; ++i)
    {
        const bool on = (i == idx);
        oscModeBtn[(size_t)i].setColour(juce::TextButton::buttonOnColourId,
                                        on ? Theme::accentHot().withAlpha(0.45f) : Theme::panelSurface());
        oscModeBtn[(size_t)i].setColour(juce::TextButton::buttonColourId,
                                        on ? Theme::accentPrimary().withAlpha(0.35f) : Theme::panelSurface());
    }

    const bool isGranular = (idx == 5);
    const bool isWt = (idx == 4);
    const bool isSample   = (idx == 7);

    for (auto* s : { &granPos, &granSize, &granDensity, &granScatter })
        s->setVisible(isGranular);
    for (auto* L : { &lblGranPos, &lblGranSize, &lblGranDensity, &lblGranScatter })
        L->setVisible(isGranular);

    wtA.setVisible(isWt);
    wtB.setVisible(isWt);
    loadWtABtn.setVisible(isWt);
    loadWtBBtn.setVisible(isWt);
    wavetableMorph.setVisible(isWt);
    lblWtMorph.setVisible(isWt);

    // Sample controls
    sampleStart.setVisible(isSample);
    sampleEnd.setVisible(isSample);
    sampleLoopBtn.setVisible(isSample);
    keymapBtn.setVisible(isSample);
    lblSampleStart.setVisible(isSample);
    lblSampleEnd.setVisible(isSample);

    filEnvShape.repaint();
    ampShape.repaint();
    resized();
}

void SynthPage::timerCallback()
{
    syncOscButtons();
}

void SynthPage::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPageHeader());
    g.drawText("Synth", getLocalBounds().removeFromTop(26).reduced(10, 0), juce::Justification::centredLeft);

    auto drawColTitle = [&g](juce::Rectangle<int> zone, const juce::String& title) {
        if (zone.isEmpty())
            return;
        g.setColour(Theme::textSecondary());
        g.setFont(Theme::fontPanelHeader());
        g.drawText(title, zone.withHeight(20).reduced(4, 0), juce::Justification::centredLeft);
    };
    drawColTitle(zoneOsc,  "OSCILLATOR");
    drawColTitle(zoneFilt, "FILTER");
    drawColTitle(zoneAmp,  "AMPLIFIER");

    // Granular region indicator
    if (!zoneGranular.isEmpty() && (granPos.isVisible() || wtA.isVisible()))
    {
        g.setColour(Theme::accentAlt().withAlpha(0.08f));
        g.fillRoundedRectangle(zoneGranular.toFloat().reduced(2.f), 4.f);
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto b = layerTab[(size_t)i].getBounds();
        const float cx = (float)b.getCentreX() - 3.f;
        const float cy = (float)b.getBottom() + 4.f;
        g.setColour(i == activeLayer ? Theme::accentHot() : Theme::textDisabled().withAlpha(0.35f));
        g.fillEllipse(cx, cy, 6.f, 6.f);
    }

    if (!zoneLfo.isEmpty())
    {
        g.setColour(Theme::textSecondary());
        g.setFont(Theme::fontPanelHeader());
        const int halfW = zoneLfo.getWidth() / 2;
        auto z1 = zoneLfo.withWidth(halfW);
        auto z2 = zoneLfo.withTrimmedLeft(halfW);
        g.drawText("LFO 1", z1.removeFromTop(18).reduced(8, 0), juce::Justification::centredLeft);
        g.drawText("LFO 2 (active layer)", z2.removeFromTop(18).reduced(8, 0), juce::Justification::centredLeft);
    }

    // Panel backgrounds for three columns
    auto drawPanelBg = [&g](juce::Rectangle<int> zone) {
        if (zone.isEmpty()) return;
        g.setColour(Theme::backgroundMid().withAlpha(0.25f));
        g.fillRoundedRectangle(zone.toFloat().reduced(2.f), 6.f);
    };
    drawPanelBg(zoneOsc);
    drawPanelBg(zoneFilt);
    drawPanelBg(zoneAmp);
}

void SynthPage::resized()
{
    constexpr int kMargin = 8;
    constexpr int kTitleH = 26;
    constexpr int kTabH = 36;
    constexpr int kSecTitle = 18;
    constexpr int kFootH = 18;
    constexpr int kCap = 12;

    const int totalH = getLocalBounds().reduced(kMargin).getHeight();
    const bool compact = totalH < 480;
    const int kLfoControlsH = compact ? 52 : 72;
    const int kLfoNoteH = compact ? 12 : 18;

    auto bounds = getLocalBounds().reduced(kMargin);
    bounds.removeFromTop(kTitleH);

    auto tabRow = bounds.removeFromTop(kTabH);
    const int tabBtnW = 46;
    for (int i = 0; i < 4; ++i)
        layerTab[(size_t)i].setBounds(tabRow.removeFromLeft(tabBtnW).reduced(3, 2));

    bounds.removeFromBottom(kFootH);

    auto lfoBlock = bounds.removeFromBottom(kLfoControlsH + kLfoNoteH);
    lblLfoPlanGap.setBounds(lfoBlock.removeFromBottom(kLfoNoteH).reduced(10, 0));
    zoneLfo = lfoBlock;
    auto lfo1Area = lfoBlock.removeFromLeft(lfoBlock.getWidth() / 2).reduced(6, 2);
    auto lfo2Area = lfoBlock.reduced(6, 2);
    lfo1Area.removeFromTop(18);
    lfo2Area.removeFromTop(18);
    {
        const int t = lfo1Area.getWidth() / 3;
        auto a0 = lfo1Area.removeFromLeft(t).reduced(2, 0);
        lblL1R.setBounds(a0.removeFromTop(kCap));
        lfo1Rate.setBounds(a0);
        auto a1 = lfo1Area.removeFromLeft(t).reduced(2, 0);
        lblL1D.setBounds(a1.removeFromTop(kCap));
        lfo1Depth.setBounds(a1);
        auto a2 = lfo1Area.reduced(2, 0);
        lblL1Sh.setBounds(a2.removeFromTop(kCap));
        lfo1Shape.setBounds(a2);
    }
    {
        const int t = lfo2Area.getWidth() / 3;
        auto b0 = lfo2Area.removeFromLeft(t).reduced(2, 0);
        lblL2R.setBounds(b0.removeFromTop(kCap));
        lfo2Rate.setBounds(b0);
        auto b1 = lfo2Area.removeFromLeft(t).reduced(2, 0);
        lblL2D.setBounds(b1.removeFromTop(kCap));
        lfo2Depth.setBounds(b1);
        auto b2 = lfo2Area.reduced(2, 0);
        lblL2Sh.setBounds(b2.removeFromTop(kCap));
        lfo2Shape.setBounds(b2);
    }

    const int totalW = bounds.getWidth();
    int wOsc = juce::roundToInt((float)totalW * 0.28f);
    int wFilt = juce::roundToInt((float)totalW * 0.38f);
    int wAmp = totalW - wOsc - wFilt;
    wOsc = juce::jmax(140, wOsc);
    wFilt = juce::jmax(208, wFilt);
    wAmp = juce::jmax(158, wAmp);
    if (wOsc + wFilt + wAmp > totalW)
    {
        const float s = (float)totalW / (float)(wOsc + wFilt + wAmp);
        wOsc = juce::jmax(120, (int)((float)wOsc * s));
        wFilt = juce::jmax(180, (int)((float)wFilt * s));
        wAmp = juce::jmax(120, totalW - wOsc - wFilt);
        wFilt = totalW - wOsc - wAmp;
    }
    auto colOsc = bounds.removeFromLeft(wOsc);
    auto colFilt = bounds.removeFromLeft(wFilt);
    auto colAmp = bounds;

    zoneOsc = colOsc;
    zoneFilt = colFilt;
    zoneAmp = colAmp;

    auto o = colOsc.reduced(4, 0);
    o.removeFromTop(kSecTitle);
    int y = o.getY();
    const int x0 = o.getX();
    const int cw = o.getWidth();
    const int btnH = compact ? 22 : 26;
    const int btnW = juce::jmax(1, cw / 4);
    for (int row = 0; row < 2; ++row)
        for (int c = 0; c < 4; ++c)
            oscModeBtn[(size_t)(row * 4 + c)]
                .setBounds(x0 + c * btnW + 1, y + row * btnH, btnW - 2, btnH - 2);
    y += btnH * 2 + 8;

    // Waveform preview (leave room below for granular block, optional sample row, and tune knobs + captions)
    const bool isGranular = granPos.isVisible();
    const bool isWt = wtA.isVisible();
    const bool isSample = sampleStart.isVisible();
    const int granControlsH = isGranular ? (4 * (15 + 20 + 3) + 4) : 0; // label + slider + gap per row
    const int wtControlsH = isWt ? 80 : 0;
    const int specH = juce::jmax(granControlsH, wtControlsH);
    const int sampleH = 0;
    const int tuneBlock = specH + sampleH + kCap + 58 + 8;
    const int waveH = juce::jlimit(40, 80, juce::jmax(40, o.getBottom() - y - tuneBlock));
    wavePreview.setBounds(x0, y, cw, waveH);
    y += wavePreview.getHeight() + 6;

    // Granular controls — only visible in Granular mode
    zoneGranular = juce::Rectangle<int>(x0, y, cw, specH);
    if (isGranular)
    {
        juce::Slider* gs[4] = { &granPos, &granSize, &granDensity, &granScatter };
        juce::Label*  gl[4] = { &lblGranPos, &lblGranSize, &lblGranDensity, &lblGranScatter };
        for (int i = 0; i < 4; ++i)
        {
            gl[(size_t)i]->setBounds(x0, y, cw, 15);
            y += 15;
            gs[(size_t)i]->setBounds(x0, y, cw, 20);
            y += 23;
        }
    }
    else if (isWt)
    {
        wtA.setBounds(x0, y, cw - 40, 20);
        loadWtABtn.setBounds(x0 + cw - 38, y, 38, 20);
        y += 24;
        wtB.setBounds(x0, y, cw - 40, 20);
        loadWtBBtn.setBounds(x0 + cw - 38, y, 38, 20);
        y += 24;

        lblWtMorph.setBounds(x0, y, cw / 2, kCap);
        wavetableMorph.setBounds(x0 + cw/2, y, cw/2, 30);
        y += 32;
    }
    else if (isSample)
    {
        // Sample controls layout
        zoneSample = juce::Rectangle<int>(x0, y, cw, 120);
        
        // Loop start
        lblSampleStart.setBounds(x0, y, cw, 15);
        y += 15;
        sampleStart.setBounds(x0, y, cw, 20);
        y += 24;
        
        // Loop end
        lblSampleEnd.setBounds(x0, y, cw, 15);
        y += 15;
        sampleEnd.setBounds(x0, y, cw, 20);
        y += 24;
        
        // Loop and Keymap buttons
        const int btnWidth = (cw - 8) / 2;
        sampleLoopBtn.setBounds(x0, y, btnWidth - 4, 24);
        keymapBtn.setBounds(x0 + btnWidth + 4, y, btnWidth - 4, 24);
        y += 32;
    }
    else
    {
        y += specH;
    }


    // Tune: Octave / Semitone / Fine — captions + knobs
    const int kw3 = juce::jmax(40, (cw - 4) / 3);
    const int tuneBodyH = juce::jmax(52, o.getBottom() - y - kCap);
    lblOct.setBounds(x0, y, kw3, kCap);
    oscOctave.setBounds(x0, y + kCap, kw3, tuneBodyH);
    lblSemi.setBounds(x0 + kw3 + 2, y, kw3, kCap);
    oscCoarse.setBounds(x0 + kw3 + 2, y + kCap, kw3, tuneBodyH);
    lblFine.setBounds(x0 + 2 * kw3 + 4, y, kw3, kCap);
    oscFine.setBounds(x0 + 2 * kw3 + 4, y + kCap, kw3, tuneBodyH);

    auto f = colFilt.reduced(4, 0);
    f.removeFromTop(kSecTitle);
    y = f.getY();
    const int fx0 = f.getX();
    const int fcw = f.getWidth();

    // Filter 1 header
    lblFilter1.setBounds(fx0, y, fcw, 18);
    y += 20;
    filterType.setBounds(fx0, y, fcw, 24);
    y += 28;
    const int fhMin = compact ? 40 : 54;
    const int fhMax = compact ? 56 : 76;
    // Filter 1: Cutoff / Resonance / Drive knobs
    {
        const int fk = juce::jmax(44, (fcw - 4) / 3);
        const int fh = juce::jlimit(fhMin, fhMax, juce::jmax(fhMin, (f.getBottom() - y - 130) / 4));
        lblCut1.setBounds(fx0, y, fk, kCap);
        fCut.setBounds(fx0, y + kCap, fk, fh);
        lblRes1.setBounds(fx0 + fk + 2, y, fk, kCap);
        fRes.setBounds(fx0 + fk + 2, y + kCap, fk, fh);
        lblDrv1.setBounds(fx0 + 2 * fk + 4, y, fk, kCap);
        fDrive.setBounds(fx0 + 2 * fk + 4, y + kCap, fk, fh);
        y += kCap + fh + 6;
    }

    // Filter 2 header
    lblFilter2Header.setBounds(fx0, y, fcw, 18);
    y += 20;
    filter2Type.setBounds(fx0, y, fcw, 24);
    y += 28;
    // Filter 2: Cutoff / Resonance / Drive knobs
    {
        const int fk = juce::jmax(44, (fcw - 4) / 3);
        const int fh = juce::jlimit(fhMin, fhMax, juce::jmax(fhMin, (f.getBottom() - y - 96) / 3));
        lblCut2.setBounds(fx0, y, fk, kCap);
        fCut2.setBounds(fx0, y + kCap, fk, fh);
        lblRes2.setBounds(fx0 + fk + 2, y, fk, kCap);
        fRes2.setBounds(fx0 + fk + 2, y + kCap, fk, fh);
        lblDrv2.setBounds(fx0 + 2 * fk + 4, y, fk, kCap);
        fDrive2.setBounds(fx0 + 2 * fk + 4, y + kCap, fk, fh);
        y += kCap + fh + 6;
    }

    // Routing
    lblFilterTopology.setBounds(fx0, y, fcw, 16);
    y += 18;
    filterRoute.setBounds(fx0, y, fcw, compact ? 20 : 24);
    y += compact ? 22 : 28;

    // Filter envelope (caption + slider per row)
    const int feH = compact ? 18 : 22;
    const int feLab = juce::jmin(52, juce::jmax(36, fcw / 5));
    filEnvA.setBounds(fx0 + feLab, y, fcw - feLab, feH);
    lblFilAtk.setBounds(fx0, y, feLab, feH);
    y += feH + 2;
    filEnvD.setBounds(fx0 + feLab, y, fcw - feLab, feH);
    lblFilDec.setBounds(fx0, y, feLab, feH);
    y += feH + 2;
    filEnvS.setBounds(fx0 + feLab, y, fcw - feLab, feH);
    lblFilSus.setBounds(fx0, y, feLab, feH);
    y += feH + 2;
    filEnvR.setBounds(fx0 + feLab, y, fcw - feLab, feH);
    lblFilRel.setBounds(fx0, y, feLab, feH);
    y += feH + 4;
    filEnvShape.setBounds(fx0, y, fcw, juce::jmax(30, juce::jmin(compact ? 38 : 52, f.getBottom() - y - 4)));
    lblDrivePlanGap.setBounds(fx0, y, 0, 0); // hidden, kept to avoid null ref

    auto a = colAmp.reduced(4, 0);
    a.removeFromTop(kSecTitle);
    y = a.getY();
    const int ax0 = a.getX();
    const int acw = a.getWidth();
    const int ah = compact ? 18 : 22;
    const int aeLab = juce::jmin(52, juce::jmax(38, acw / 5));
    ampA.setBounds(ax0 + aeLab, y, acw - aeLab, ah);
    lblAmpAtk.setBounds(ax0, y, aeLab, ah);
    y += ah + 2;
    ampD.setBounds(ax0 + aeLab, y, acw - aeLab, ah);
    lblAmpDec.setBounds(ax0, y, aeLab, ah);
    y += ah + 2;
    ampS.setBounds(ax0 + aeLab, y, acw - aeLab, ah);
    lblAmpSus.setBounds(ax0, y, aeLab, ah);
    y += ah + 2;
    ampR.setBounds(ax0 + aeLab, y, acw - aeLab, ah);
    lblAmpRel.setBounds(ax0, y, aeLab, ah);
    y += ah + 6;
    ampShape.setBounds(ax0, y, acw, juce::jmax(30, juce::jmin(compact ? 38 : 52, a.getBottom() - y - 88)));
    y += ampShape.getHeight() + 6;
    const int uniRowH = kCap + 56;
    const int uniH = juce::jmin(uniRowH, juce::jmax(kCap + 48, a.getBottom() - y - 4));
    auto uniR = juce::Rectangle<int>(ax0, y, acw, uniH);
    const int uw = juce::jmax(44, uniR.getWidth() / 3);
    {
        auto u0 = uniR.removeFromLeft(uw).reduced(2, 0);
        lblUniV.setBounds(u0.removeFromTop(kCap));
        uniVoices.setBounds(u0);
        auto u1 = uniR.removeFromLeft(uw).reduced(2, 0);
        lblUniDet.setBounds(u1.removeFromTop(kCap));
        uniDetune.setBounds(u1);
        auto u2 = uniR.reduced(2, 0);
        lblUniSpr.setBounds(u2.removeFromTop(kCap));
        uniSpread.setBounds(u2);
    }
    y += uniH + 8;
    auto tail = juce::Rectangle<int>(ax0, y, acw, a.getBottom() - y);
    auto faderR = tail.removeFromRight(58);
    lblLevel.setBounds(faderR.removeFromTop(kCap).reduced(2, 0));
    ampVol.setBounds(faderR.reduced(4, 4));
    const int panSz = juce::jmax(64, juce::jmin(92, tail.getWidth()));
    const int panH = juce::jmax(58, juce::jmin(80, tail.getHeight()));
    auto panR = tail.withSizeKeepingCentre(panSz, panH).withY(tail.getY());
    lblPan.setBounds(panR.removeFromTop(kCap));
    ampPan.setBounds(panR.reduced(0, 2));
}

void SynthPage::showKeymapDialog()
{
    auto& keymap = processor.getSynthEngine().getLayerKeymap(activeLayer);
    
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Sample Keymap - Layer " + juce::String::charToString((juce::juce_wchar)('A' + activeLayer));
    options.content.setOwned(new juce::Component());
    auto* content = options.content.get();
    content->setSize(500, 400);
    
    // Create a simple list of zones
    struct SimpleListModel : public juce::ListBoxModel
    {
        int getNumRows() override { return 1; }
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            g.setColour(juce::Colours::white);
            g.drawText("No zones loaded", 0, 0, width, height, juce::Justification::centred);
        }
    };
    
    auto* listBox = new juce::ListBox();
    listBox->setModel(new SimpleListModel());
    content->addAndMakeVisible(listBox);
    listBox->setBounds(10, 10, 480, 300);
    
    // Add zone button
    auto* addZoneBtn = new juce::TextButton("Add Zone");
    content->addAndMakeVisible(addZoneBtn);
    addZoneBtn->setBounds(10, 320, 100, 30);
    
    addZoneBtn->onClick = [this] {
        juce::FileChooser chooser("Select Sample File", juce::File{}, "*.wav");
        chooser.launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                if (fc.getResult().exists()) {
                    auto file = fc.getResult();
                    SampleZone zone;
                    zone.file = file;
                    zone.rootNote = 60; // Default to C4
                    zone.loNote = 48;
                    zone.hiNote = 72;
                    zone.loVel = 0;
                    zone.hiVel = 127;
                    zone.startFrac = 0.0f;
                    zone.endFrac = 1.0f;
                    zone.loopEnabled = true;
                    zone.oneShot = false;
                    
                    processor.getSynthEngine().addSampleZone(activeLayer, zone);
                }
            });
    };
    
    // Clear zones button
    auto* clearBtn = new juce::TextButton("Clear All");
    content->addAndMakeVisible(clearBtn);
    clearBtn->setBounds(120, 320, 100, 30);
    
    clearBtn->onClick = [this] {
        processor.getSynthEngine().clearSampleKeymap(activeLayer);
    };
    
    // Close button
    auto* closeBtn = new juce::TextButton("Close");
    content->addAndMakeVisible(closeBtn);
    closeBtn->setBounds(390, 320, 100, 30);
    
    auto dialog = options.launchAsync();
    if (dialog != nullptr)
    {
        closeBtn->onClick = [dialog] {
            dialog->closeButtonPressed();
        };
    }
}

} // namespace wolfsden::ui
