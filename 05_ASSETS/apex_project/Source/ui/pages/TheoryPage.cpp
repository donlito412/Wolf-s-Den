#include "TheoryPage.h"

#include "../../PluginProcessor.h"
#include "../../engine/TheoryEngine.h"

namespace wolfsden::ui
{
namespace
{
constexpr float kPi = 3.14159265f;

void styleKnob(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentPrimary());
    s.setColour(juce::Slider::thumbColourId, Theme::accentHot());
}
} // namespace

// =============================================================================
// CircleOfFifths
// =============================================================================

CircleOfFifths::CircleOfFifths(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
}

void CircleOfFifths::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(16.f);
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float rad = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    static const char* labels[] = { "C", "G", "D", "A", "E", "B", "F#", "Db", "Ab", "Eb", "Bb", "F" };

    int activeRoot = 0;
    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(processor.getAPVTS().getParameter("theory_scale_root")))
        activeRoot = p->get();
    static const int rootForSeg[] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

    for (int i = 0; i < 12; ++i)
    {
        const float a0 = (float)(i * juce::MathConstants<double>::twoPi / 12.0 - juce::MathConstants<double>::halfPi);
        const float a1 = (float)((i + 1) * juce::MathConstants<double>::twoPi / 12.0 - juce::MathConstants<double>::halfPi);
        juce::Path p;
        p.addPieSegment(cx - rad, cy - rad, rad * 2.f, rad * 2.f, a0, a1, 0.62f);

        const bool isActive = (rootForSeg[(size_t)i] == activeRoot);
        if (isActive)
        {
            g.setColour(Theme::accentHot().withAlpha(0.6f));
            g.fillPath(p);
            g.setColour(Theme::accentHot());
            g.strokePath(p, juce::PathStrokeType(2.f));
        }
        else
        {
            g.setColour(juce::Colour::fromHSV((float)i / 12.f, 0.4f, 0.3f, 1.f));
            g.fillPath(p);
            g.setColour(Theme::textDisabled());
            g.strokePath(p, juce::PathStrokeType(1.f));
        }

        const float am = (a0 + a1) * 0.5f;
        const float lr = rad * 0.78f;
        g.setColour(isActive ? Theme::textPrimary() : Theme::textSecondary());
        g.setFont(isActive ? Theme::fontPanelHeader() : Theme::fontValue());
        g.drawText(labels[(size_t)i],
                    juce::Rectangle<int>((int)(cx + std::cos(am) * lr - 14), (int)(cy + std::sin(am) * lr - 8), 28, 16),
                    juce::Justification::centred);
    }

    // Centre label
    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPanelHeader());
    g.drawText("Circle of\nFifths",
               juce::Rectangle<int>((int)(cx - 36), (int)(cy - 16), 72, 32),
               juce::Justification::centred);
}

void CircleOfFifths::mouseDown(const juce::MouseEvent& e)
{
    auto c = getLocalBounds().toFloat().getCentre();
    const float ang = std::atan2(e.position.y - c.y, e.position.x - c.x);
    float t = (ang + kPi + juce::MathConstants<float>::halfPi) / (2.f * kPi) * 12.f;
    while (t < 0.f)  t += 12.f;
    while (t >= 12.f) t -= 12.f;
    const int seg = (int)t % 12;
    static const int rootForSeg[] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };
    const int root = rootForSeg[(size_t)seg];
    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(processor.getAPVTS().getParameter("theory_scale_root")))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float)root));
        p->endChangeGesture();
    }
    repaint();
}

// =============================================================================
// ColorsGrid
// =============================================================================

ColorsGrid::ColorsGrid(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(4);
}

ColorsGrid::~ColorsGrid()
{
    stopTimer();
}

void ColorsGrid::timerCallback()
{
    repaint();
}

void ColorsGrid::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    static const char* rowLabels[] = { "Triad", "7th", "9th", "11th", "Sus", "Altered" };
    constexpr int numRows = 6;

    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady())
    {
        g.setColour(Theme::textSecondary());
        g.setFont(Theme::fontValue());
        g.drawText("Loading chord database...", getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Get active scale info for colour-coding
    const int activeRoot = th.getActiveScaleRoot();
    std::array<int, 128> scaleLut {};
    th.getScaleLookupTable(scaleLut);

    // Build pitch class set for active scale
    std::array<bool, 12> scaleNotes {};
    for (int i = 0; i < 12; ++i)
        scaleNotes[(size_t)i] = (scaleLut[(size_t)i] == i);

    const int headerW = 70;
    const int colW = juce::jmax(44, (getWidth() - headerW) / 7);
    const int rowH = juce::jmax(24, getHeight() / (numRows + 1));

    // Column headers (scale degrees I–VII)
    static const char* degreeNames[] = { "I", "ii", "iii", "IV", "V", "vi", "vii" };
    g.setFont(Theme::fontLabel());
    for (int c = 0; c < 7; ++c)
    {
        auto r = juce::Rectangle<int>(headerW + c * colW, 0, colW, rowH);
        g.setColour(Theme::textSecondary());
        g.drawText(degreeNames[(size_t)c], r, juce::Justification::centred);
    }

    // Row headers + cells
    for (int row = 0; row < numRows; ++row)
    {
        auto labelR = juce::Rectangle<int>(0, (row + 1) * rowH, headerW, rowH);
        g.setColour(Theme::textSecondary());
        g.setFont(Theme::fontLabel());
        g.drawText(rowLabels[(size_t)row], labelR, juce::Justification::centredRight);

        for (int col = 0; col < 7; ++col)
        {
            auto cellR = juce::Rectangle<int>(headerW + col * colW + 2, (row + 1) * rowH + 2, colW - 4, rowH - 4).toFloat();

            // Determine if this voicing is diatonic, modal, or outside
            // Simplified: odd rows get different colours
            const int noteInScale = (activeRoot + col * 2) % 12;
            const bool isDiatonic = scaleNotes[(size_t)(noteInScale % 12)];

            juce::Colour cellCol;
            if (isDiatonic)
                cellCol = Theme::accentAlt().withAlpha(0.3f);     // blue = diatonic
            else if (row < 3)
                cellCol = Theme::warning().withAlpha(0.25f);       // gold = modal
            else
                cellCol = Theme::textDisabled().withAlpha(0.2f);   // gray = outside

            g.setColour(cellCol);
            g.fillRoundedRectangle(cellR, 3.f);
            g.setColour(Theme::textDisabled());
            g.drawRoundedRectangle(cellR, 3.f, 0.5f);
        }
    }
}

void ColorsGrid::mouseDown(const juce::MouseEvent& e)
{
    const int headerW = 70;
    const int colW = juce::jmax(44, (getWidth() - headerW) / 7);
    const int rowH = juce::jmax(24, getHeight() / 7);

    const int col = (int)(e.position.x - (float)headerW) / colW;
    const int row = (int)(e.position.y / (float)rowH) - 1;

    if (col >= 0 && col < 7 && row >= 0 && row < 6)
    {
        // Could trigger audition — placeholder
        juce::ignoreUnused(col, row);
    }
}

// =============================================================================
// TheoryPage
// =============================================================================

TheoryPage::TheoryPage(WolfsDenAudioProcessor& proc)
    : processor(proc)
    , apvts(proc.getAPVTS())
    , cof(proc)
    , colorsGrid(proc)
{
    for (auto* t : { &tabBrowse, &tabCof, &tabExplore, &tabColors })
    {
        t->setClickingTogglesState(false);
        t->setTriggeredOnMouseDown(true);
        addAndMakeVisible(t);
    }
    tabBrowse.onClick = [this] { showSub(0); };
    tabCof.onClick = [this] { showSub(1); };
    tabExplore.onClick = [this] { showSub(2); };
    tabColors.onClick = [this] { showSub(3); };

    for (int i = 0; i < 12; ++i)
    {
        static const char* rn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        scaleRoot.addItem(rn[(size_t)i], i + 1);
    }
    scaleRoot.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(scaleRoot);

    addAndMakeVisible(scaleType);
    addAndMakeVisible(voiceLead);

    detectMidi.onClick = [this] { processor.setTheoryDetectionMidi(); };
    detectAudio.onClick = [this] { processor.setTheoryDetectionAudio(); };
    addAndMakeVisible(detectMidi);
    addAndMakeVisible(detectAudio);

    attRoot = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "theory_scale_root", scaleRoot);
    attScale = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "theory_scale_type", scaleType);
    attVL = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "theory_voice_leading", voiceLead);

    // --- Browse Chords panel ---
    addChildComponent(panelBrowse);
    for (auto& L : matchLabs)
    {
        L.setFont(Theme::fontValue());
        L.setColour(juce::Label::textColourId, Theme::textPrimary());
        panelBrowse.addAndMakeVisible(L);
    }

    // Section A: 8 chord pads
    for (int i = 0; i < 8; ++i)
    {
        chordPads[(size_t)i].setButtonText("-");
        chordPads[(size_t)i].setColour(juce::TextButton::buttonColourId, Theme::panelSurface());
        chordPads[(size_t)i].setColour(juce::TextButton::buttonOnColourId, Theme::accentPrimary().withAlpha(0.45f));
        panelBrowse.addAndMakeVisible(chordPads[(size_t)i]);
    }

    // Section B: 7 diatonic degree pads
    static const char* deg[] = { "I", "ii", "iii", "IV", "V", "vi", "vii°" };
    for (int i = 0; i < 7; ++i)
    {
        degreePads[(size_t)i].setButtonText(deg[(size_t)i]);
        degreePads[(size_t)i].setColour(juce::TextButton::buttonColourId, Theme::backgroundMid());
        panelBrowse.addAndMakeVisible(degreePads[(size_t)i]);
    }

    // Section C: progression track
    progressionLabel.setText("Drag chords here to build a progression", juce::dontSendNotification);
    progressionLabel.setFont(Theme::fontLabel());
    progressionLabel.setColour(juce::Label::textColourId, Theme::textDisabled());
    progressionLabel.setJustificationType(juce::Justification::centred);
    panelBrowse.addAndMakeVisible(progressionLabel);

    // Modifier sidebar
    styleKnob(inversionSlider);
    inversionSlider.setRange(0, 3, 1);
    inversionSlider.setValue(0, juce::dontSendNotification);
    panelBrowse.addAndMakeVisible(inversionSlider);

    styleKnob(densitySlider);
    densitySlider.setRange(0, 4, 1);
    densitySlider.setValue(0, juce::dontSendNotification);
    panelBrowse.addAndMakeVisible(densitySlider);

    styleKnob(octaveSlider);
    octaveSlider.setRange(-2, 2, 1);
    octaveSlider.setValue(0, juce::dontSendNotification);
    panelBrowse.addAndMakeVisible(octaveSlider);

    // --- Circle of Fifths ---
    addChildComponent(cof);

    // --- Explore ---
    addChildComponent(exploreVp);
    exploreVp.setViewedComponent(&exploreInner, false);

    auto& th = processor.getTheoryEngine();
    if (th.isDatabaseReady())
    {
        const auto& scales = th.getScaleDefinitions();
        int y = 0;
        for (int i = 0; i < (int)scales.size(); ++i)
        {
            auto* b = exploreBtns.add(new juce::TextButton(scales[(size_t)i].name));
            b->setBounds(0, y, 280, 28);
            b->setColour(juce::TextButton::buttonColourId, Theme::panelSurface());
            b->onClick = [this, i] {
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_scale_type")))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost(p->convertTo0to1((float)i));
                    p->endChangeGesture();
                }
            };
            exploreInner.addAndMakeVisible(b);
            y += 30;
        }
        exploreInner.setSize(300, y + 4);
    }

    // --- Colors ---
    addChildComponent(colorsGrid);

    showSub(0);
    startTimerHz(10);
}

TheoryPage::~TheoryPage()
{
    stopTimer();
    exploreVp.setViewedComponent(nullptr); // disconnect before exploreInner is destroyed
}

void TheoryPage::timerCallback()
{
    if (!processor.getTheoryEngine().isDatabaseReady())
        return;
    const auto top3 = processor.getTheoryEngine().getTopMatches();
    for (int k = 0; k < 3; ++k)
    {
        juce::String t = "-";
        const auto& m = top3[(size_t)k];
        if (m.chordId >= 0 && m.score > 0.01f)
        {
            const auto& defs = processor.getTheoryEngine().getChordDefinitions();
            if (m.chordId < (int)defs.size())
                t = juce::String(defs[(size_t)m.chordId].name) + "  (" + juce::String(m.score, 2) + ")";
        }
        matchLabs[(size_t)k].setText(t, juce::dontSendNotification);
    }
}

void TheoryPage::showSub(int i)
{
    subTab = juce::jlimit(0, 3, i);
    panelBrowse.setVisible(subTab == 0);
    cof.setVisible(subTab == 1);
    exploreVp.setVisible(subTab == 2);
    colorsGrid.setVisible(subTab == 3);

    // Update tab button highlights
    tabBrowse.setColour(juce::TextButton::buttonColourId, subTab == 0 ? Theme::accentPrimary().withAlpha(0.35f) : Theme::panelSurface());
    tabCof.setColour(juce::TextButton::buttonColourId, subTab == 1 ? Theme::accentPrimary().withAlpha(0.35f) : Theme::panelSurface());
    tabExplore.setColour(juce::TextButton::buttonColourId, subTab == 2 ? Theme::accentPrimary().withAlpha(0.35f) : Theme::panelSurface());
    tabColors.setColour(juce::TextButton::buttonColourId, subTab == 3 ? Theme::accentPrimary().withAlpha(0.35f) : Theme::panelSurface());

    repaint();
}

void TheoryPage::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    // Page header
    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPageHeader());
    g.drawText("Theory", getLocalBounds().removeFromTop(28).reduced(12, 0), juce::Justification::centredLeft);

    // Underline active sub-tab
    auto* activeTab = subTab == 0 ? &tabBrowse : (subTab == 1 ? &tabCof : (subTab == 2 ? &tabExplore : &tabColors));
    auto tb = activeTab->getBounds().toFloat();
    g.setColour(Theme::accentHot());
    g.drawLine(tb.getX() + 4.f, tb.getBottom() - 1.f, tb.getRight() - 4.f, tb.getBottom() - 1.f, 2.f);
}

void TheoryPage::resized()
{
    auto r = getLocalBounds().reduced(12);
    r.removeFromTop(28);
    auto tabs = r.removeFromTop(28);
    const int tw = juce::jmin(120, tabs.getWidth() / 4);
    tabBrowse.setBounds(tabs.removeFromLeft(tw).reduced(1, 0));
    tabCof.setBounds(tabs.removeFromLeft(tw).reduced(1, 0));
    tabExplore.setBounds(tabs.removeFromLeft(tw).reduced(1, 0));
    tabColors.setBounds(tabs.removeFromLeft(tw).reduced(1, 0));
    r.removeFromTop(6);

    // Top controls row
    auto top = r.removeFromTop(32);
    scaleRoot.setBounds(top.removeFromLeft(100).reduced(0, 2));
    top.removeFromLeft(8);
    scaleType.setBounds(top.removeFromLeft(juce::jmin(220, top.getWidth() / 2)).reduced(0, 2));
    top.removeFromLeft(8);
    voiceLead.setBounds(top.removeFromLeft(120));
    top.removeFromLeft(8);
    detectMidi.setBounds(top.removeFromLeft(100));
    top.removeFromLeft(6);
    detectAudio.setBounds(top.removeFromLeft(100));

    r.removeFromTop(8);

    // All sub-panels fill the remaining area
    panelBrowse.setBounds(r);
    {
        auto pb = panelBrowse.getLocalBounds().reduced(8);
        const int modW = juce::jmin(100, pb.getWidth() / 5);
        auto modCol = pb.removeFromRight(modW);

        // Modifier sidebar
        const int knobH = juce::jmin(72, modCol.getHeight() / 3);
        inversionSlider.setBounds(modCol.removeFromTop(knobH).reduced(4, 2));
        densitySlider.setBounds(modCol.removeFromTop(knobH).reduced(4, 2));
        octaveSlider.setBounds(modCol.removeFromTop(knobH).reduced(4, 2));

        // Match labels
        matchLabs[0].setBounds(pb.removeFromTop(22));
        matchLabs[1].setBounds(pb.removeFromTop(22));
        matchLabs[2].setBounds(pb.removeFromTop(22));
        pb.removeFromTop(8);

        // Section A: 8 chord pads
        auto chordRow = pb.removeFromTop(36);
        const int cpw = juce::jmax(40, chordRow.getWidth() / 8);
        for (int i = 0; i < 8; ++i)
            chordPads[(size_t)i].setBounds(chordRow.removeFromLeft(cpw).reduced(2, 2));
        pb.removeFromTop(8);

        // Section B: 7 diatonic degree pads
        auto degRow = pb.removeFromTop(28);
        const int pw = juce::jmax(36, degRow.getWidth() / 7);
        for (int i = 0; i < 7; ++i)
            degreePads[(size_t)i].setBounds(degRow.removeFromLeft(pw).reduced(2, 0));
        pb.removeFromTop(8);

        // Section C: progression track area
        progressionLabel.setBounds(pb.removeFromTop(juce::jmin(60, pb.getHeight())));
    }

    cof.setBounds(r);
    exploreVp.setBounds(r);
    colorsGrid.setBounds(r);
}

} // namespace wolfsden::ui
