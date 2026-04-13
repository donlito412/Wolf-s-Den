#include "TheoryPage.h"

#include "../../PluginProcessor.h"
#include "../../engine/TheoryEngine.h"
#include "../theme/WolfsDenLookAndFeel.h"

#include <algorithm>
#include <vector>

namespace wolfsden::ui
{
namespace
{
constexpr float kPi = 3.14159265f;

/** Normalise chord intervals to sorted unique pitch classes (root at 0). */
std::vector<int> normaliseChordPCs(std::vector<int> iv)
{
    if (iv.empty())
        return {};
    const int root = iv[0];
    for (int& x : iv)
        x = ((x - root) % 12 + 12) % 12;
    std::sort(iv.begin(), iv.end());
    iv.erase(std::unique(iv.begin(), iv.end()), iv.end());
    return iv;
}

/** Match DB chord shape to APVTS theory_chord_type index (same order as WolfsDenParameterLayout). */
int mapChordIntervalsToTypeIndex(const std::vector<int>& raw)
{
    const auto pc = normaliseChordPCs(raw);
    static const std::vector<std::vector<int>> refs = {
        { 0, 4, 7 },       // Major
        { 0, 3, 7 },       // Minor
        { 0, 3, 6 },       // Dim
        { 0, 4, 8 },       // Aug
        { 0, 2, 7 },       // Sus2
        { 0, 5, 7 },       // Sus4
        { 0, 4, 7, 11 },   // Maj7
        { 0, 3, 7, 10 },   // Min7
        { 0, 4, 7, 10 },   // Dom7
        { 0, 3, 6, 10 },   // Min7b5
        { 0, 3, 6, 9 },    // Dim7
        { 0, 3, 7, 11 },   // MinMaj7
        { 0, 2, 4, 7 },    // Add9 (as PCs)
        { 0, 2, 4, 7, 11 }, // Maj9
        { 0, 2, 3, 7, 10 }, // Min9
    };
    for (int i = 0; i < (int)refs.size(); ++i)
        if (pc == refs[(size_t)i])
            return i;
    return 0;
}

juce::String shortChordLabel(const wolfsden::ChordDefinition& d)
{
    juce::String s = juce::String(d.symbol.c_str()).trim();
    if (s.isEmpty())
        s = juce::String(d.name.c_str());
    return s.substring(0, 10);
}

/** Same intervals/order as MidiPipeline::kChordTypes — used for preview only. */
struct IvPack
{
    const int* p = nullptr;
    int n = 0;
};

static constexpr int prvMaj[] = { 0, 4, 7 };
static constexpr int prvMin[] = { 0, 3, 7 };
static constexpr int prvDim[] = { 0, 3, 6 };
static constexpr int prvAug[] = { 0, 4, 8 };
static constexpr int prvSus2[] = { 0, 2, 7 };
static constexpr int prvSus4[] = { 0, 5, 7 };
static constexpr int prvMaj7[] = { 0, 4, 7, 11 };
static constexpr int prvMin7[] = { 0, 3, 7, 10 };
static constexpr int prvDom7[] = { 0, 4, 7, 10 };
static constexpr int prvHalfDim[] = { 0, 3, 6, 10 };
static constexpr int prvDim7[] = { 0, 3, 6, 9 };
static constexpr int prvMinMaj7[] = { 0, 3, 7, 11 };
static constexpr int prvAdd9[] = { 0, 4, 7, 14 };
static constexpr int prvMaj9[] = { 0, 4, 7, 11, 14 };
static constexpr int prvMin9[] = { 0, 3, 7, 10, 14 };

static const IvPack kIvByChordType[15] = {
    { prvMaj, 3 },
    { prvMin, 3 },
    { prvDim, 3 },
    { prvAug, 3 },
    { prvSus2, 3 },
    { prvSus4, 3 },
    { prvMaj7, 4 },
    { prvMin7, 4 },
    { prvDom7, 4 },
    { prvHalfDim, 4 },
    { prvDim7, 4 },
    { prvMinMaj7, 4 },
    { prvAdd9, 4 },
    { prvMaj9, 5 },
    { prvMin9, 5 },
};

struct DiatonicTriadResult
{
    int chordRootPc = 0;
    int triadTypeIdx = 0; // APVTS chord type: Maj, Min, Dim, Aug
};

/** Stack thirds on scale degrees (wraps for pentatonic etc.). */
DiatonicTriadResult diatonicTriadForDegree(const std::vector<int>& iv, int scaleRootPc, int degreeIdx)
{
    DiatonicTriadResult out;
    out.chordRootPc = scaleRootPc % 12;
    if (iv.empty())
        return out;

    const int n = juce::jmax(1, (int)iv.size());
    const int i = juce::jlimit(0, n - 1, degreeIdx);
    const int i3 = (i + 2) % n;
    const int i5 = (i + 4) % n;

    auto degPc = [&](int deg) {
        const int k = ((deg % n) + n) % n;
        return (scaleRootPc + iv[(size_t)k]) % 12;
    };

    const int r = degPc(i);
    const int t = degPc(i3);
    const int f = degPc(i5);
    const int third = (t - r + 12) % 12;
    const int fifth = (f - r + 12) % 12;

    if (third == 4 && fifth == 7)
        out.triadTypeIdx = 0;
    else if (third == 3 && fifth == 7)
        out.triadTypeIdx = 1;
    else if (third == 3 && fifth == 6)
        out.triadTypeIdx = 2;
    else if (third == 4 && fifth == 8)
        out.triadTypeIdx = 3;
    else
        out.triadTypeIdx = 0;

    out.chordRootPc = r;
    return out;
}

void setChordRootAnchorPc(juce::AudioProcessorValueTreeState& apvts, int pitchClass012)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_root_anchor")))
    {
        const int idx = juce::jlimit(1, 12, 1 + juce::jlimit(0, 11, pitchClass012));
        c->beginChangeGesture();
        c->setValueNotifyingHost(c->convertTo0to1((float)idx));
        c->endChangeGesture();
    }
}

int readTheoryScaleRootPc(juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* pr = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("theory_scale_root")))
        return juce::jlimit(0, 11, pr->get());
    return 0;
}

/** APVTS chord type index for a diatonic seventh chord on \a degreeIdx (heptatonic / wrapped). */
int diatonicSeventhChordTypeIdx(const std::vector<int>& iv, int scaleRootPc, int degreeIdx)
{
    if (iv.empty())
        return 8; // Dom7

    const int n = juce::jmax(1, (int)iv.size());
    const int i = juce::jlimit(0, n - 1, degreeIdx);
    auto degPc = [&](int k) {
        const int kk = ((k % n) + n) % n;
        return (scaleRootPc + iv[(size_t)kk]) % 12;
    };

    const int r = degPc(i);
    const int t = degPc((i + 2) % n);
    const int f = degPc((i + 4) % n);
    const int s = degPc((i + 6) % n);
    const int third = (t - r + 12) % 12;
    const int fifth = (f - r + 12) % 12;
    const int sev = (s - r + 12) % 12;
    const bool maj3 = (third == 4);
    const bool min3 = (third == 3);
    const bool p5 = (fifth == 7);
    const bool d5 = (fifth == 6);

    if (sev == 11 && maj3 && p5)
        return 6; // Maj7
    if (sev == 10 && maj3 && p5)
        return 8; // Dom7
    if (sev == 10 && min3 && p5)
        return 7; // Min7
    if (sev == 10 && min3 && d5)
        return 9; // Min7b5
    if (sev == 9 && min3 && d5)
        return 10; // Dim7
    if (sev == 11 && min3 && p5)
        return 11; // MinMaj7
    return 8;
}

void styleKnob(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    WolfsDenLookAndFeel::configureRotarySlider(s);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentAlt());
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
            // Cyan band only (avoid full hue wheel purples)
            const float h = 0.48f + (float)i * (0.10f / 12.f);
            g.setColour(juce::Colour::fromHSV(h, 0.38f, 0.32f, 1.f));
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

    // Column headers (scale degrees I–VII); dim. seventh uses Unicode degree (not UTF-8 literal).
    static const char* degreeNames[] = { "I", "ii", "iii", "IV", "V", "vi" };
    g.setFont(Theme::fontLabel());
    for (int c = 0; c < 7; ++c)
    {
        auto r = juce::Rectangle<int>(headerW + c * colW, 0, colW, rowH);
        g.setColour(Theme::textSecondary());
        const juce::String label = (c < 6 ? juce::String(degreeNames[(size_t)c])
                                          : juce::String("vii") + juce::String::charToString((juce::juce_wchar) 0x00B0));
        g.drawText(label, r, juce::Justification::centred);
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
        if (onColorsCell)
            onColorsCell(row, col);
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
    chordPadLibraryDefId_.fill(-1);

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

    for (auto& s : juce::StringArray({ "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Pent Maj", "Pent Min", "Blues", "Whole Tone", "Diminished", "Chromatic" }))
        scaleType.addItem(s, scaleType.getNumItems() + 1);
        
    attRoot = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "theory_scale_root", scaleRoot);
    attScale = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "theory_scale_type", scaleType);
    attVL = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "theory_voice_leading", voiceLead);

    // --- Browse Chords panel ---
    addChildComponent(panelBrowse);
    panelBrowse.setOpaque(true);
    panelBrowse.setInterceptsMouseClicks(true, true);

    for (auto& L : matchLabs)
    {
        L.setFont(Theme::fontValue());
        L.setColour(juce::Label::textColourId, Theme::textPrimary());
        panelBrowse.addAndMakeVisible(L);
    }

    // Section A: 8 chord pads (0–2 = top detection matches; 3–7 reserved / inactive until wired to sets)
    for (int i = 0; i < 8; ++i)
    {
        chordPads[(size_t)i].setButtonText("-");
        chordPads[(size_t)i].setColour(juce::TextButton::buttonColourId, Theme::panelSurface());
        chordPads[(size_t)i].setColour(juce::TextButton::buttonOnColourId, Theme::accentPrimary().withAlpha(0.45f));
        chordPads[(size_t)i].onClick = [this, i] { chordPadClicked(i); };
        panelBrowse.addAndMakeVisible(chordPads[(size_t)i]);
    }

    // Section B: 7 diatonic degree pads (degree sign as Unicode char — avoids mojibake "viiÂ°")
    static const juce::String degNames[] = {
        "I", "ii", "iii", "IV", "V", "vi",
        juce::String("vii") + juce::String::charToString((juce::juce_wchar) 0x00B0),
    };
    for (int i = 0; i < 7; ++i)
    {
        degreePads[(size_t)i].setButtonText(degNames[(size_t)i]);
        degreePads[(size_t)i].setColour(juce::TextButton::buttonColourId, Theme::backgroundMid());
        degreePads[(size_t)i].onClick = [this, i] { degreePadClicked(i); };
        panelBrowse.addAndMakeVisible(degreePads[(size_t)i]);
    }

    // Section C: progression (store & recall chord steps)
    progressionHeader.setText("Progression", juce::dontSendNotification);
    progressionHeader.setFont(Theme::fontLabel());
    progressionHeader.setColour(juce::Label::textColourId, Theme::textSecondary());
    progressionHeader.setJustificationType(juce::Justification::centredLeft);
    panelBrowse.addAndMakeVisible(progressionHeader);

    progressionAdd.setColour(juce::TextButton::buttonColourId, Theme::panelSurface());
    progressionAdd.onClick = [this] { progressionAddClicked(); };
    panelBrowse.addAndMakeVisible(progressionAdd);

    progressionClear.setColour(juce::TextButton::buttonColourId, Theme::panelSurface());
    progressionClear.onClick = [this] { progressionClearClicked(); };
    panelBrowse.addAndMakeVisible(progressionClear);

    for (int i = 0; i < 8; ++i)
    {
        progressionSlots[(size_t)i].setButtonText("-");
        progressionSlots[(size_t)i].setColour(juce::TextButton::buttonColourId, Theme::backgroundMid());
        progressionSlots[(size_t)i].setColour(juce::TextButton::buttonOnColourId, Theme::accentPrimary().withAlpha(0.35f));
        const int si = i;
        progressionSlots[(size_t)i].onClick = [this, si] { progressionSlotClicked(si); };
        panelBrowse.addAndMakeVisible(progressionSlots[(size_t)i]);
    }

    // Modifier sidebar
    auto prepMod = [](juce::Label& L, const juce::String& t) {
        L.setText(t, juce::dontSendNotification);
        L.setFont(Theme::fontLabel());
        L.setColour(juce::Label::textColourId, Theme::textSecondary());
        L.setJustificationType(juce::Justification::centred);
    };
    prepMod(lblInv, "Inversion");
    prepMod(lblDens, "Voicing");
    prepMod(lblOct, "Octave");
    for (auto* L : { &lblInv, &lblDens, &lblOct })
        panelBrowse.addAndMakeVisible(*L);

    prepMod(lblChordRoot, "Chord root");
    panelBrowse.addAndMakeVisible(lblChordRoot);
    if (auto* pChordRoot = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_root_anchor")))
        for (int i = 0; i < pChordRoot->choices.size(); ++i)
            chordRootAnchor.addItem(pChordRoot->choices[(size_t)i], i + 1);
    chordRootAnchor.setSelectedId(1, juce::dontSendNotification);
    panelBrowse.addAndMakeVisible(chordRootAnchor);
    attChordRoot = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "theory_chord_root_anchor", chordRootAnchor);

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

    attInv = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "midi_chord_inversion", inversionSlider);
    attDensity = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "theory_chord_density", densitySlider);
    attOct = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "theory_chord_octave_shift", octaveSlider);

    // --- Circle of Fifths ---
    addChildComponent(cof);

    // --- Explore ---
    addChildComponent(exploreVp);
    exploreVp.setViewedComponent(&exploreInner, false);

    if (auto* scaleChoice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_scale_type")))
    {
        int y = 0;
        for (int i = 0; i < scaleChoice->choices.size(); ++i)
        {
            auto* b = exploreBtns.add(new juce::TextButton(scaleChoice->choices[(size_t)i]));
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
    colorsGrid.onColorsCell = [this](int row, int col) { colorsCellClicked(row, col); };

    showSub(0);
    refreshProgressionSlotUi();
    startTimerHz(10);
}

TheoryPage::~TheoryPage()
{
    stopChordPreview();
    stopTimer();
    exploreVp.setViewedComponent(nullptr, false); // disconnect safely without deleting stack memory
}

void TheoryPage::stopChordPreview()
{
    auto& kbd = processor.getMidiKeyboardState();
    for (int i = 0; i < previewCount_; ++i)
        kbd.noteOff(1, previewNotes_[(size_t)i], 0.f);
    previewCount_ = 0;
}

void TheoryPage::startChordPreview(int rootMidi, const int* iv, int nIv)
{
    stopChordPreview();
    if (iv == nullptr || nIv < 1)
        return;

    nIv = juce::jmin(nIv, (int)previewNotes_.size());
    auto& kbd = processor.getMidiKeyboardState();
    previewCount_ = nIv;
    for (int i = 0; i < nIv; ++i)
    {
        const int n = juce::jlimit(0, 127, rootMidi + iv[i]);
        previewNotes_[(size_t)i] = n;
        kbd.noteOn(1, n, 0.82f);
    }
    previewTicksLeft_ = 14; // 10 Hz → ~1.4 s
}

void TheoryPage::startChordPreviewFromIntervals(int rootPc012, const std::vector<int>& ivFromRoot)
{
    stopChordPreview();
    if (ivFromRoot.empty())
        return;

    const int n = juce::jmin((int)ivFromRoot.size(), (int)previewNotes_.size());
    auto& kbd = processor.getMidiKeyboardState();
    const int base = 60 + juce::jlimit(0, 11, rootPc012);
    previewCount_ = n;
    for (int i = 0; i < n; ++i)
    {
        const int nn = juce::jlimit(0, 127, base + ivFromRoot[(size_t)i]);
        previewNotes_[(size_t)i] = nn;
        kbd.noteOn(1, nn, 0.82f);
    }
    previewTicksLeft_ = 14;
}

void TheoryPage::applyChordTypeRootPreview(int chordTypeApvtsIdx, int rootPc012)
{
    const int choiceIdx = juce::jlimit(0, 14, chordTypeApvtsIdx);
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_type")))
    {
        const int nChoices = c->choices.size();
        const int idx = juce::jmin(choiceIdx, juce::jmax(0, nChoices - 1));
        c->beginChangeGesture();
        c->setValueNotifyingHost(c->convertTo0to1((float)idx));
        c->endChangeGesture();
    }

    if (auto* b = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("midi_chord_mode")))
    {
        b->beginChangeGesture();
        b->setValueNotifyingHost(1.f);
        b->endChangeGesture();
    }

    setChordRootAnchorPc(apvts, rootPc012);

    const int rootMidi = 60 + juce::jlimit(0, 11, rootPc012);
    const auto& ivp = kIvByChordType[(size_t)choiceIdx];
    startChordPreview(rootMidi, ivp.p, ivp.n);
}

void TheoryPage::applyChordFromLibraryDefIndex(int defIndex, int rootPc012)
{
    if (!processor.getTheoryEngine().isDatabaseReady())
        return;

    const auto& defs = processor.getTheoryEngine().getChordDefinitions();
    if (defIndex < 0 || defIndex >= (int)defs.size())
        return;

    const auto& def = defs[(size_t)defIndex];
    const int mapped = mapChordIntervalsToTypeIndex(def.intervals);
    const int choiceIdx = juce::jlimit(0, 14, mapped);

    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_type")))
    {
        const int nChoices = c->choices.size();
        const int idx = juce::jmin(choiceIdx, juce::jmax(0, nChoices - 1));
        c->beginChangeGesture();
        c->setValueNotifyingHost(c->convertTo0to1((float)idx));
        c->endChangeGesture();
    }

    if (auto* b = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("midi_chord_mode")))
    {
        b->beginChangeGesture();
        b->setValueNotifyingHost(1.f);
        b->endChangeGesture();
    }

    setChordRootAnchorPc(apvts, juce::jlimit(0, 11, rootPc012));
    startChordPreviewFromIntervals(juce::jlimit(0, 11, rootPc012), def.intervals);
}

void TheoryPage::chordPadClicked(int padIndex)
{
    if (padIndex < 0 || padIndex > 7)
        return;
    if (!processor.getTheoryEngine().isDatabaseReady())
        return;

    if (padIndex <= 2)
    {
        const auto top3 = processor.getTheoryEngine().getTopMatches();
        const auto& m = top3[(size_t)padIndex];
        if (m.chordId < 0 || m.score <= 0.01f)
            return;

        const auto& defs = processor.getTheoryEngine().getChordDefinitions();
        if (m.chordId >= (int)defs.size())
            return;

        const auto& def = defs[(size_t)m.chordId];
        const int typeIdx = mapChordIntervalsToTypeIndex(def.intervals);
        applyChordTypeRootPreview(typeIdx, m.rootNote);
        return;
    }

    const int defIx = chordPadLibraryDefId_[(size_t)padIndex];
    if (defIx < 0)
        return;

    applyChordFromLibraryDefIndex(defIx, readTheoryScaleRootPc(apvts));
}

void TheoryPage::degreePadClicked(int degreeIndex)
{
    if (degreeIndex < 0 || degreeIndex > 6)
        return;

    const int sr = readTheoryScaleRootPc(apvts);
    int st = 0;
    if (auto* sc = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_scale_type")))
        st = sc->getIndex();

    std::vector<int> intervals;
    const auto& scales = processor.getTheoryEngine().getScaleDefinitions();
    if (!scales.empty())
    {
        st = juce::jlimit(0, (int)scales.size() - 1, st);
        intervals = scales[(size_t)st].intervals;
    }

    const DiatonicTriadResult dt = diatonicTriadForDegree(intervals, sr, degreeIndex);
    applyChordTypeRootPreview(dt.triadTypeIdx, dt.chordRootPc);
}

void TheoryPage::colorsCellClicked(int row, int col)
{
    if (row < 0 || row > 5 || col < 0 || col > 6)
        return;
    if (!processor.getTheoryEngine().isDatabaseReady())
        return;

    const int sr = readTheoryScaleRootPc(apvts);
    int st = 0;
    if (auto* sc = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_scale_type")))
        st = sc->getIndex();

    std::vector<int> intervals;
    const auto& scales = processor.getTheoryEngine().getScaleDefinitions();
    if (!scales.empty())
    {
        st = juce::jlimit(0, (int)scales.size() - 1, st);
        intervals = scales[(size_t)st].intervals;
    }

    const DiatonicTriadResult dt = diatonicTriadForDegree(intervals, sr, col);
    int typeIdx = dt.triadTypeIdx;
    switch (row)
    {
        case 0:
            break;
        case 1:
            typeIdx = diatonicSeventhChordTypeIdx(intervals, sr, col);
            break;
        case 2:
            typeIdx = 12;
            break; // Add9
        case 3:
            typeIdx = 13;
            break; // Maj9
        case 4:
            typeIdx = (col & 1) == 0 ? 4 : 5;
            break; // Sus2 / Sus4
        case 5:
            typeIdx = 2;
            break; // Dim
        default:
            return;
    }

    applyChordTypeRootPreview(typeIdx, dt.chordRootPc);
}

void TheoryPage::refreshProgressionSlotUi()
{
    static const char* rn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto* typeParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_type"));

    for (int i = 0; i < 8; ++i)
    {
        if (!progressionSteps_[(size_t)i].has_value())
        {
            progressionSlots[(size_t)i].setButtonText("-");
            continue;
        }

        const auto& s = *progressionSteps_[(size_t)i];
        juce::String t = "?";
        if (typeParam != nullptr && s.typeIdx >= 0 && s.typeIdx < typeParam->choices.size())
            t = typeParam->choices[(size_t)s.typeIdx];
        const int rpc = juce::jlimit(0, 11, s.rootPc);
        progressionSlots[(size_t)i].setButtonText(juce::String(rn[(size_t)rpc]) + " " + t.substring(0, 5));
    }
}

void TheoryPage::progressionAddClicked()
{
    if (auto* anchor = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_root_anchor")))
    {
        if (anchor->getIndex() <= 0)
            return;
    }

    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_type")))
    {
        const int t = c->getIndex();
        int root = 0;
        if (auto* a = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("theory_chord_root_anchor")))
            root = juce::jlimit(0, 11, a->getIndex() - 1);

        for (int i = 0; i < 8; ++i)
        {
            if (!progressionSteps_[(size_t)i].has_value())
            {
                progressionSteps_[(size_t)i] = ProgStep{ t, root };
                refreshProgressionSlotUi();
                return;
            }
        }
    }
}

void TheoryPage::progressionClearClicked()
{
    for (auto& s : progressionSteps_)
        s.reset();
    refreshProgressionSlotUi();
}

void TheoryPage::progressionSlotClicked(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 8)
        return;
    if (!progressionSteps_[(size_t)slotIndex].has_value())
        return;

    const auto& s = *progressionSteps_[(size_t)slotIndex];
    applyChordTypeRootPreview(s.typeIdx, s.rootPc);
}

void TheoryPage::timerCallback()
{
    if (previewTicksLeft_ > 0)
    {
        --previewTicksLeft_;
        if (previewTicksLeft_ == 0)
            stopChordPreview();
    }

    const auto detHi = Theme::accentPrimary().withAlpha(0.4f);
    const auto detLo = Theme::panelSurface();
    if (processor.getTheoryEngine().isDatabaseReady())
    {
        const bool audioDet = processor.getTheoryEngine().getDetectionMode() == wolfsden::TheoryEngine::DetectionMode::Audio;
        detectMidi.setColour(juce::TextButton::buttonColourId, audioDet ? detLo : detHi);
        detectAudio.setColour(juce::TextButton::buttonColourId, audioDet ? detHi : detLo);
    }

    if (!processor.getTheoryEngine().isDatabaseReady())
    {
        for (auto& L : matchLabs)
            L.setText("Loading music library…", juce::dontSendNotification);
        for (int k = 0; k < 8; ++k)
        {
            chordPadLibraryDefId_[(size_t)k] = -1;
            chordPads[(size_t)k].setButtonText("…");
        }
        return;
    }

    const auto top3 = processor.getTheoryEngine().getTopMatches();
    const auto& defs = processor.getTheoryEngine().getChordDefinitions();

    for (int k = 0; k < 3; ++k)
    {
        juce::String t = (k == 0 ? "Play MIDI or audio to detect chords" : "-");
        const auto& m = top3[(size_t)k];
        if (m.chordId >= 0 && m.score > 0.01f && m.chordId < (int)defs.size())
            t = juce::String(defs[(size_t)m.chordId].name) + "  (" + juce::String(m.score, 2) + ")";
        matchLabs[(size_t)k].setText(t, juce::dontSendNotification);
    }

    static const char* rootNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int sr = readTheoryScaleRootPc(apvts);

    for (int k = 0; k < 3; ++k)
    {
        juce::String lab = "-";
        const auto& m = top3[(size_t)k];
        if (m.chordId >= 0 && m.score > 0.01f && m.chordId < (int)defs.size())
            lab = shortChordLabel(defs[(size_t)m.chordId]);
        chordPads[(size_t)k].setButtonText(lab);
        chordPadLibraryDefId_[(size_t)k] = -1;
    }

    for (int k = 3; k < 8; ++k)
    {
        const int di = k - 3;
        if (di < (int)defs.size())
        {
            chordPadLibraryDefId_[(size_t)k] = di;
            juce::String sym = juce::String(defs[(size_t)di].symbol.c_str()).trim();
            if (sym.isEmpty())
                sym = shortChordLabel(defs[(size_t)di]);
            juce::String lab = juce::String(rootNames[(size_t)sr]) + sym;
            chordPads[(size_t)k].setButtonText(lab.substring(0, 14));
        }
        else
        {
            chordPadLibraryDefId_[(size_t)k] = -1;
            chordPads[(size_t)k].setButtonText("-");
        }
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
        const int modW = juce::jmin(118, juce::jmax(96, pb.getWidth() / 4));
        auto modCol = pb.removeFromRight(modW);

        // Modifier sidebar (caption + knob per cell)
        constexpr int kCap = 12;
        const int third = modCol.getHeight() / 3;
        const int bodyH = juce::jmax(50, juce::jmin(68, third - kCap - 2));
        {
            auto cell = modCol.removeFromTop(kCap + bodyH + 2);
            lblInv.setBounds(cell.removeFromTop(kCap));
            inversionSlider.setBounds(cell.reduced(4, 0));
        }
        {
            auto cell = modCol.removeFromTop(kCap + bodyH + 2);
            lblDens.setBounds(cell.removeFromTop(kCap));
            densitySlider.setBounds(cell.reduced(4, 0));
        }
        {
            auto cell = modCol.removeFromTop(kCap + bodyH + 2);
            lblOct.setBounds(cell.removeFromTop(kCap));
            octaveSlider.setBounds(cell.reduced(4, 0));
        }

        auto crRow = pb.removeFromTop(24);
        lblChordRoot.setBounds(crRow.removeFromLeft(76).reduced(0, 2));
        chordRootAnchor.setBounds(crRow.reduced(4, 0));
        pb.removeFromTop(4);

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

        // Section C: progression
        auto progTop = pb.removeFromTop(22);
        progressionHeader.setBounds(progTop.removeFromLeft(88));
        progressionAdd.setBounds(progTop.removeFromLeft(48).reduced(0, 2));
        progTop.removeFromLeft(4);
        progressionClear.setBounds(progTop.removeFromLeft(52).reduced(0, 2));
        pb.removeFromTop(4);
        auto progRow = pb.removeFromTop(30);
        const int psw = juce::jmax(34, progRow.getWidth() / 8);
        for (int i = 0; i < 8; ++i)
            progressionSlots[(size_t)i].setBounds(progRow.removeFromLeft(psw).reduced(1, 0));
        pb.removeFromTop(6);
    }

    cof.setBounds(r);
    exploreVp.setBounds(r);
    colorsGrid.setBounds(r);
}

} // namespace wolfsden::ui
