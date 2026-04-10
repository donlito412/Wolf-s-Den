#include "SynthPage.h"

#include "../../PluginProcessor.h"

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
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentPrimary());
    s.setColour(juce::Slider::thumbColourId, Theme::accentHot());
}

void SynthPage::styleHSlider(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
}

void SynthPage::styleVFader(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
}

SynthPage::SynthPage(WolfsDenAudioProcessor& proc)
    : processor(proc)
    , apvts(proc.getAPVTS())
{
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
    granularHint.setText("Granular: position / size / density / scatter",
                         juce::dontSendNotification);
    granularHint.setFont(Theme::fontLabel());
    granularHint.setColour(juce::Label::textColourId, Theme::textSecondary());
    granularHint.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(granularHint);

    auto prep = [](juce::Label& L, const juce::String& t, juce::Colour col) {
        L.setText(t, juce::dontSendNotification);
        L.setFont(Theme::fontLabel());
        L.setColour(juce::Label::textColourId, col);
        L.setJustificationType(juce::Justification::centredLeft);
    };
    prep(lblFilter1, "Filter 1", Theme::textPrimary());
    prep(lblFilter2, "Filter 2", Theme::textPrimary());
    prep(lblFilterTopology,
         "Dual filter — serial / parallel routing per layer",
         Theme::textSecondary());
    prep(lblDrivePlanGap, "",
         Theme::textDisabled());
    prep(lblLfoPlanGap, "",
         Theme::textDisabled());
    for (auto* L : { &lblFilter1, &lblFilter2, &lblFilterTopology, &lblDrivePlanGap, &lblLfoPlanGap })
        addAndMakeVisible(*L);

    styleKnob(oscCoarse);
    styleKnob(oscFine);
    addAndMakeVisible(oscCoarse);
    addAndMakeVisible(oscFine);

    addAndMakeVisible(filterType);
    styleKnob(fCut);
    styleKnob(fRes);
    addAndMakeVisible(fCut);
    addAndMakeVisible(fRes);
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
    addS(apvts, layerKey("tune_coarse"), oscCoarse, layerSAtt);
    addS(apvts, layerKey("tune_fine"), oscFine, layerSAtt);
    addC(apvts, layerKey("filter_type"), filterType, layerCAtt);
    addS(apvts, layerKey("filter_cutoff"), fCut, layerSAtt);
    addS(apvts, layerKey("filter_resonance"), fRes, layerSAtt);
    addC(apvts, layerKey("filter_routing"), filterRoute, layerCAtt);

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

    granularHint.setVisible(idx == 5); // Granular
    filEnvShape.repaint();
    ampShape.repaint();
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
        g.drawText(title, zone.withHeight(18).reduced(4, 0), juce::Justification::centredLeft);
    };
    drawColTitle(zoneOsc, "OSCILLATOR");
    drawColTitle(zoneFilt, "FILTER");
    drawColTitle(zoneAmp, "AMPLIFIER");

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
        g.drawText("LFO 1", z1.removeFromTop(14).reduced(8, 0), juce::Justification::centredLeft);
        g.drawText("LFO 2 (active layer)", z2.removeFromTop(14).reduced(8, 0), juce::Justification::centredLeft);
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
    constexpr int kLfoControlsH = 52;
    constexpr int kLfoNoteH = 14;
    constexpr int kFootH = 18;

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
    lfo1Area.removeFromTop(14);
    lfo2Area.removeFromTop(14);
    {
        const int t = lfo1Area.getWidth() / 3;
        lfo1Rate.setBounds(lfo1Area.removeFromLeft(t).reduced(2, 0));
        lfo1Depth.setBounds(lfo1Area.removeFromLeft(t).reduced(2, 0));
        lfo1Shape.setBounds(lfo1Area.reduced(2, 0));
    }
    {
        const int t = lfo2Area.getWidth() / 3;
        lfo2Rate.setBounds(lfo2Area.removeFromLeft(t).reduced(2, 0));
        lfo2Depth.setBounds(lfo2Area.removeFromLeft(t).reduced(2, 0));
        lfo2Shape.setBounds(lfo2Area.reduced(2, 0));
    }

    const int totalW = bounds.getWidth();
    const int wOsc = juce::roundToInt((float)totalW * 0.28f);
    const int wFilt = juce::roundToInt((float)totalW * 0.38f);
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
    const int btnH = 26;
    const int btnW = cw / 4;
    for (int row = 0; row < 2; ++row)
        for (int c = 0; c < 4; ++c)
            oscModeBtn[(size_t)(row * 4 + c)]
                .setBounds(x0 + c * btnW + 1, y + row * btnH, btnW - 2, btnH - 2);
    y += btnH * 2 + 8;
    const int knobColH = 76;
    const int belowWave = (granularHint.isVisible() ? 46 : 8) + knobColH + 8;
    const int waveH = juce::jlimit(48, 96, juce::jmax(48, o.getBottom() - y - belowWave));
    wavePreview.setBounds(x0, y, cw, waveH);
    y += wavePreview.getHeight() + 6;
    granularHint.setBounds(x0, y, cw, granularHint.isVisible() ? 40 : 4);
    y += granularHint.getHeight() + 6;
    const int kw = (cw - 8) / 2;
    const int knobH = juce::jmax(knobColH, o.getBottom() - y);
    oscCoarse.setBounds(x0, y, kw, knobH);
    oscFine.setBounds(x0 + kw + 8, y, kw, knobH);

    auto f = colFilt.reduced(4, 0);
    f.removeFromTop(kSecTitle);
    y = f.getY();
    const int fx0 = f.getX();
    const int fcw = f.getWidth();
    lblFilter1.setBounds(fx0, y, fcw, 16);
    y += 18;
    filterType.setBounds(fx0, y, fcw, 24);
    y += 28;
    const int fk = (fcw - 6) / 2;
    fCut.setBounds(fx0, y, fk, 68);
    fRes.setBounds(fx0 + fk + 6, y, fk, 68);
    y += 74;
    lblFilter2.setBounds(fx0, y, fcw, 16);
    y += 18;
    lblFilterTopology.setBounds(fx0, y, fcw, 32);
    y += 36;
    filterRoute.setBounds(fx0, y, fcw, 24);
    y += 30;
    const int feH = 18;
    filEnvA.setBounds(fx0, y, fcw, feH);
    y += feH + 2;
    filEnvD.setBounds(fx0, y, fcw, feH);
    y += feH + 2;
    filEnvS.setBounds(fx0, y, fcw, feH);
    y += feH + 2;
    filEnvR.setBounds(fx0, y, fcw, feH);
    y += feH + 4;
    filEnvShape.setBounds(fx0, y, fcw, juce::jmin(52, f.getBottom() - y - 40));
    y += filEnvShape.getHeight() + 6;
    lblDrivePlanGap.setBounds(fx0, y, fcw, juce::jmax(20, f.getBottom() - y));

    auto a = colAmp.reduced(4, 0);
    a.removeFromTop(kSecTitle);
    y = a.getY();
    const int ax0 = a.getX();
    const int acw = a.getWidth();
    const int ah = 19;
    ampA.setBounds(ax0, y, acw, ah);
    y += ah + 2;
    ampD.setBounds(ax0, y, acw, ah);
    y += ah + 2;
    ampS.setBounds(ax0, y, acw, ah);
    y += ah + 2;
    ampR.setBounds(ax0, y, acw, ah);
    y += ah + 6;
    ampShape.setBounds(ax0, y, acw, 52);
    y += 58;
    auto uniR = juce::Rectangle<int>(ax0, y, acw, 66);
    const int uw = uniR.getWidth() / 3;
    uniVoices.setBounds(uniR.removeFromLeft(uw).reduced(2, 0));
    uniDetune.setBounds(uniR.removeFromLeft(uw).reduced(2, 0));
    uniSpread.setBounds(uniR.reduced(2, 0));
    y += 72;
    auto tail = juce::Rectangle<int>(ax0, y, acw, a.getBottom() - y);
    auto faderR = tail.removeFromRight(56);
    ampVol.setBounds(faderR.reduced(4, 8));
    const int panSz = juce::jmin(88, tail.getWidth());
    ampPan.setBounds(tail.withSizeKeepingCentre(panSz, juce::jmin(76, tail.getHeight())).withY(tail.getY()));
}

} // namespace wolfsden::ui
