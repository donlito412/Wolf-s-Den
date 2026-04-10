#include "PerformPage.h"

#include "../../PluginProcessor.h"
#include "../../engine/TheoryEngine.h"

namespace wolfsden::ui
{
namespace
{
static const char* kRootNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

void styleKnob(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentPrimary());
    s.setColour(juce::Slider::thumbColourId, Theme::accentHot());
}

void styleH(juce::Slider& s)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
}

void sectionTitle(juce::Label& L, const juce::String& t)
{
    L.setText(t, juce::dontSendNotification);
    L.setFont(Theme::fontPanelHeader());
    L.setColour(juce::Label::textColourId, Theme::textPrimary());
    L.setJustificationType(juce::Justification::centred);
}

} // namespace

// =============================================================================
// RootNotePicker
// =============================================================================

RootNotePicker::RootNotePicker(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
}

void RootNotePicker::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.f);
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float rad = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    int activeRoot = 0;
    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("theory_scale_root")))
        activeRoot = p->get();

    for (int i = 0; i < 12; ++i)
    {
        const float angle = (float)(i * juce::MathConstants<double>::twoPi / 12.0 - juce::MathConstants<double>::halfPi);
        const float nx = cx + std::cos(angle) * rad * 0.72f;
        const float ny = cy + std::sin(angle) * rad * 0.72f;
        const float dotR = rad * 0.14f;

        if (i == activeRoot)
        {
            g.setColour(Theme::accentHot());
            g.fillEllipse(nx - dotR, ny - dotR, dotR * 2.f, dotR * 2.f);
            // Glow
            g.setColour(Theme::accentHot().withAlpha(0.25f));
            g.fillEllipse(nx - dotR - 4.f, ny - dotR - 4.f, dotR * 2.f + 8.f, dotR * 2.f + 8.f);
        }
        else
        {
            g.setColour(Theme::panelSurface().brighter(0.1f));
            g.fillEllipse(nx - dotR, ny - dotR, dotR * 2.f, dotR * 2.f);
        }

        g.setColour(i == activeRoot ? Theme::textPrimary() : Theme::textSecondary());
        g.setFont(Theme::fontLabel());
        g.drawText(kRootNames[(size_t)i],
                   juce::Rectangle<int>((int)(nx - 12), (int)(ny - 6), 24, 12),
                   juce::Justification::centred);
    }
}

void RootNotePicker::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.f);
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float rad = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // Find closest note
    float bestDist = 999.f;
    int bestNote = 0;
    for (int i = 0; i < 12; ++i)
    {
        const float angle = (float)(i * juce::MathConstants<double>::twoPi / 12.0 - juce::MathConstants<double>::halfPi);
        const float nx = cx + std::cos(angle) * rad * 0.72f;
        const float ny = cy + std::sin(angle) * rad * 0.72f;
        const float dist = std::hypot(e.position.x - nx, e.position.y - ny);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestNote = i;
        }
    }

    if (bestDist < rad * 0.25f)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("theory_scale_root")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1((float)bestNote));
            p->endChangeGesture();
        }
        repaint();
    }
}

void RootNotePicker::resized()
{
    repaint();
}

// =============================================================================
// MiniKeyboard
// =============================================================================

MiniKeyboard::MiniKeyboard(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(8);
}

MiniKeyboard::~MiniKeyboard()
{
    stopTimer();
}

void MiniKeyboard::timerCallback()
{
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady()) return;

    std::array<int, 128> remap {};
    th.getScaleLookupTable(remap);
    std::array<bool, 12> newScale {};
    for (int i = 0; i < 12; ++i)
        newScale[(size_t)i] = (remap[(size_t)i] == i);

    if (newScale != inScale)
    {
        inScale = newScale;
        repaint();
    }
}

void MiniKeyboard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.f);
    const float keyW = bounds.getWidth() / 12.f;
    const float h = bounds.getHeight();

    // Black key pattern: 1, 3, 6, 8, 10
    static const bool isBlack[] = { false, true, false, true, false, false, true, false, true, false, true, false };

    // White keys
    for (int i = 0; i < 12; ++i)
    {
        if (isBlack[(size_t)i]) continue;
        auto r = juce::Rectangle<float>(bounds.getX() + (float)i * keyW, bounds.getY(), keyW - 1.f, h);
        g.setColour(inScale[(size_t)i] ? Theme::accentHot().withAlpha(0.45f) : Theme::panelSurface().brighter(0.15f));
        g.fillRoundedRectangle(r, 2.f);
        g.setColour(Theme::textDisabled());
        g.drawRoundedRectangle(r, 2.f, 0.5f);
    }

    // Black keys
    for (int i = 0; i < 12; ++i)
    {
        if (!isBlack[(size_t)i]) continue;
        auto r = juce::Rectangle<float>(bounds.getX() + (float)i * keyW + 1.f, bounds.getY(), keyW - 2.f, h * 0.65f);
        g.setColour(inScale[(size_t)i] ? Theme::accentPrimary().withAlpha(0.6f) : Theme::backgroundDark());
        g.fillRoundedRectangle(r, 2.f);
    }
}

// =============================================================================
// ArpStepGrid
// =============================================================================

ArpStepGrid::ArpStepGrid(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    velocities.fill(100.f / 127.f);
    enabled.fill(true);
    startTimerHz(8);
}

ArpStepGrid::~ArpStepGrid()
{
    stopTimer();
}

void ArpStepGrid::timerCallback()
{
    bool dirty = false;
    for (int i = 0; i < 32; ++i)
    {
        const juce::String idx = juce::String(i).paddedLeft('0', 2);
        const juce::String pfx = "midi_arp_s" + idx + "_";
        if (auto* pOn = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(pfx + "on")))
        {
            bool on = pOn->get();
            if (on != enabled[(size_t)i]) { enabled[(size_t)i] = on; dirty = true; }
        }
        if (auto* pVel = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(pfx + "vel")))
        {
            float v = (float)pVel->get() / 127.f;
            if (std::abs(v - velocities[(size_t)i]) > 0.01f) { velocities[(size_t)i] = v; dirty = true; }
        }
    }
    if (dirty) repaint();
}

void ArpStepGrid::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.f);
    g.setColour(Theme::backgroundDark());
    g.fillRoundedRectangle(bounds, 4.f);
    g.setColour(Theme::textDisabled());
    g.drawRoundedRectangle(bounds, 4.f, 1.f);

    const float stepW = bounds.getWidth() / 32.f;
    const float maxH = bounds.getHeight() - 4.f;

    for (int i = 0; i < 32; ++i)
    {
        const float x = bounds.getX() + (float)i * stepW + 1.f;
        const float barH = maxH * juce::jlimit(0.f, 1.f, velocities[(size_t)i]);
        const float y = bounds.getBottom() - 2.f - barH;
        const float w = stepW - 2.f;

        if (!enabled[(size_t)i])
        {
            g.setColour(Theme::textDisabled().withAlpha(0.3f));
            g.fillRoundedRectangle(x, y, w, barH, 1.f);
        }
        else
        {
            // Gradient from accentPrimary to accentHot based on velocity
            juce::Colour barCol = Theme::accentPrimary().interpolatedWith(Theme::accentHot(), velocities[(size_t)i]);
            g.setColour(barCol);
            g.fillRoundedRectangle(x, y, w, barH, 1.f);
        }

        // Beat accent line every 4 steps
        if (i % 4 == 0 && i > 0)
        {
            g.setColour(Theme::textDisabled().withAlpha(0.4f));
            g.drawLine(x, bounds.getY() + 2.f, x, bounds.getBottom() - 2.f, 0.5f);
        }
    }
}

void ArpStepGrid::mouseDown(const juce::MouseEvent& e)
{
    editStep(e);
}

void ArpStepGrid::mouseDrag(const juce::MouseEvent& e)
{
    editStep(e);
}

void ArpStepGrid::editStep(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.f);
    const float stepW = bounds.getWidth() / 32.f;
    int stepIdx = (int)((e.position.x - bounds.getX()) / stepW);
    stepIdx = juce::jlimit(0, 31, stepIdx);

    // Y position → velocity
    float normY = 1.f - (e.position.y - bounds.getY()) / bounds.getHeight();
    normY = juce::jlimit(0.f, 1.f, normY);
    int vel = juce::jlimit(1, 127, (int)(normY * 127.f));

    const juce::String idx = juce::String(stepIdx).paddedLeft('0', 2);
    if (auto* p = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("midi_arp_s" + idx + "_vel")))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float)vel));
        p->endChangeGesture();
    }

    velocities[(size_t)stepIdx] = (float)vel / 127.f;
    repaint();
}

// =============================================================================
// PerformPage
// =============================================================================

PerformPage::PerformPage(WolfsDenAudioProcessor& proc)
    : processor(proc)
    , apvts(proc.getAPVTS())
    , rootPicker(proc.getAPVTS())
    , miniKb(proc)
    , stepGrid(proc.getAPVTS())
{
    // Section titles
    sectionTitle(keysLockTitle, "KEYS LOCK");
    sectionTitle(chordTitle, "CHORD MODE");
    sectionTitle(arpTitle, "ARPEGGIATOR");
    addAndMakeVisible(keysLockTitle);
    addAndMakeVisible(chordTitle);
    addAndMakeVisible(arpTitle);

    // --- Keys Lock ---
    addAndMakeVisible(keysMode);
    addAndMakeVisible(rootPicker);
    addAndMakeVisible(scalePicker);
    addAndMakeVisible(miniKb);

    // --- Chord Mode ---
    addAndMakeVisible(chordMode);
    addAndMakeVisible(chordType);
    styleH(chordInv);
    chordInv.setRange(0, 3, 1);
    addAndMakeVisible(chordInv);
    addAndMakeVisible(scaleConstraint);

    // --- Arpeggiator ---
    addAndMakeVisible(arpOn);
    styleKnob(arpRate);
    addAndMakeVisible(arpRate);
    addAndMakeVisible(arpPattern);
    styleH(arpOct);
    addAndMakeVisible(arpOct);
    styleH(arpSwing);
    addAndMakeVisible(arpSwing);
    addAndMakeVisible(arpLatch);
    addAndMakeVisible(arpRestart);
    addAndMakeVisible(arpSync);
    addAndMakeVisible(stepGrid);
    addAndMakeVisible(midiCapture);
    midiCapture.onClick = [] {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                "MIDI capture",
                                                "MIDI clip export will be added with the arrange/preset pipeline.",
                                                "OK");
    };

    // Per-step detail
    for (int i = 1; i <= 32; ++i)
        stepSelect.addItem("Step " + juce::String(i), i);
    stepSelect.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(stepSelect);
    addAndMakeVisible(stepOn);
    for (auto* s : { &stepVel, &stepDur, &stepTrn, &stepRkt })
    {
        styleH(*s);
        addAndMakeVisible(*s);
    }

    // Scale picker populated from Theory engine
    auto& th = proc.getTheoryEngine();
    if (th.isDatabaseReady())
    {
        const auto& scales = th.getScaleDefinitions();
        for (int i = 0; i < (int)scales.size(); ++i)
            scalePicker.addItem(juce::String(scales[(size_t)i].name), i + 1);
    }
    else
    {
        // Fallback
        const char* fallbackScales[] = { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor",
                                          "Dorian", "Phrygian", "Lydian", "Mixolydian",
                                          "Pent Maj", "Pent Min", "Blues", "Whole Tone",
                                          "Diminished", "Chromatic" };
        for (int i = 0; i < 14; ++i)
            scalePicker.addItem(fallbackScales[(size_t)i], i + 1);
    }
    scalePicker.setSelectedId(1, juce::dontSendNotification);

    // --- APVTS bindings ---
    attKeys = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "midi_keys_lock_mode", keysMode);
    attScale = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "theory_scale_type", scalePicker);
    attChord = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "midi_chord_mode", chordMode);
    attChordType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "theory_chord_type", chordType);
    attChordInv = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "midi_chord_inversion", chordInv);
    attArpOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "midi_arp_on", arpOn);
    attArpRate = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "midi_arp_rate", arpRate);
    attArpPat = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "midi_arp_pattern", arpPattern);
    attLatch = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "midi_arp_latch", arpLatch);
    attSwing = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "midi_arp_swing", arpSwing);
    attOct = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "midi_arp_octaves", arpOct);
    attSync = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "midi_arp_sync_ppq", arpSync);

    stepSelect.onChange = [this] { rebuildStepAttachments(); };
    rebuildStepAttachments();
    juce::ignoreUnused(processor);
}

PerformPage::~PerformPage() = default;

void PerformPage::rebuildStepAttachments()
{
    attStepOn.reset();
    attVel.reset();
    attDur.reset();
    attTrn.reset();
    attRkt.reset();

    const int si = juce::jlimit(0, 31, stepSelect.getSelectedId() - 1);
    const juce::String idx = juce::String(si).paddedLeft('0', 2);
    const juce::String pfx = "midi_arp_s" + idx + "_";

    const juce::String idOn = pfx + "on";
    const juce::String idVel = pfx + "vel";
    const juce::String idDur = pfx + "dur";
    const juce::String idTrn = pfx + "trn";
    const juce::String idRkt = pfx + "rkt";
    if (apvts.getParameter(idOn) != nullptr)
        attStepOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, idOn, stepOn);
    if (apvts.getParameter(idVel) != nullptr)
        attVel = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idVel, stepVel);
    if (apvts.getParameter(idDur) != nullptr)
        attDur = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idDur, stepDur);
    if (apvts.getParameter(idTrn) != nullptr)
        attTrn = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idTrn, stepTrn);
    if (apvts.getParameter(idRkt) != nullptr)
        attRkt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idRkt, stepRkt);
}

void PerformPage::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    // Page title
    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPageHeader());
    g.drawText("Perform", getLocalBounds().removeFromTop(28).reduced(12, 0), juce::Justification::centredLeft);

    // Section dividers
    auto r = getLocalBounds().reduced(12);
    r.removeFromTop(28);
    const int totalW = r.getWidth();
    const int secW0 = (int)((float)totalW * 0.30f);
    const int secW1 = (int)((float)totalW * 0.25f);

    // Vertical lines between sections
    g.setColour(Theme::textDisabled().withAlpha(0.3f));
    g.drawLine((float)(r.getX() + secW0), (float)(r.getY() + 28), (float)(r.getX() + secW0), (float)r.getBottom(), 1.f);
    g.drawLine((float)(r.getX() + secW0 + secW1), (float)(r.getY() + 28), (float)(r.getX() + secW0 + secW1), (float)r.getBottom(), 1.f);

    // Section panel backgrounds
    auto renderPanel = [&](juce::Rectangle<int> area) {
        auto pf = area.toFloat().reduced(4.f, 2.f);
        g.setColour(Theme::backgroundMid().withAlpha(0.35f));
        g.fillRoundedRectangle(pf, 6.f);
    };

    auto keysR = juce::Rectangle<int>(r.getX(), r.getY(), secW0, r.getHeight());
    auto chordR = juce::Rectangle<int>(r.getX() + secW0, r.getY(), secW1, r.getHeight());
    auto arpR = juce::Rectangle<int>(r.getX() + secW0 + secW1, r.getY(), totalW - secW0 - secW1, r.getHeight());
    renderPanel(keysR);
    renderPanel(chordR);
    renderPanel(arpR);

    // Step detail labels
    g.setColour(Theme::textSecondary());
    g.setFont(Theme::fontLabel());
}

void PerformPage::resized()
{
    constexpr int kPageTitle = 28;
    constexpr int kSectionTitle = 28;
    constexpr int kPad = 12;
    constexpr int kVGap = 6;

    auto r = getLocalBounds().reduced(kPad);
    r.removeFromTop(kPageTitle);

    const int totalW = r.getWidth();
    const int secW0 = (int)((float)totalW * 0.30f); // Keys Lock
    const int secW1 = (int)((float)totalW * 0.25f); // Chord Mode

    auto keysArea = r.removeFromLeft(secW0).reduced(8, 4);
    auto chordArea = r.removeFromLeft(secW1).reduced(8, 4);
    auto arpArea = r.reduced(8, 4);

    // ---- KEYS LOCK ----
    keysLockTitle.setBounds(keysArea.removeFromTop(kSectionTitle));
    keysArea.removeFromTop(kVGap);

    keysMode.setBounds(keysArea.removeFromTop(26));
    keysArea.removeFromTop(kVGap);

    const int pickerSz = juce::jmin(160, keysArea.getWidth(), keysArea.getHeight() - 90);
    rootPicker.setBounds(keysArea.removeFromTop(pickerSz).withSizeKeepingCentre(pickerSz, pickerSz));
    keysArea.removeFromTop(kVGap);

    scalePicker.setBounds(keysArea.removeFromTop(26));
    keysArea.removeFromTop(kVGap);

    miniKb.setBounds(keysArea.removeFromTop(juce::jmin(40, keysArea.getHeight())));

    // ---- CHORD MODE ----
    chordTitle.setBounds(chordArea.removeFromTop(kSectionTitle));
    chordArea.removeFromTop(kVGap);

    chordMode.setBounds(chordArea.removeFromTop(24));
    chordArea.removeFromTop(kVGap);
    chordType.setBounds(chordArea.removeFromTop(26));
    chordArea.removeFromTop(kVGap);
    chordInv.setBounds(chordArea.removeFromTop(26));
    chordArea.removeFromTop(kVGap);
    scaleConstraint.setBounds(chordArea.removeFromTop(24));

    // ---- ARPEGGIATOR ----
    arpTitle.setBounds(arpArea.removeFromTop(kSectionTitle));
    arpArea.removeFromTop(kVGap);

    auto arpTopRow = arpArea.removeFromTop(26);
    arpOn.setBounds(arpTopRow.removeFromLeft(80));
    arpTopRow.removeFromLeft(6);
    arpSync.setBounds(arpTopRow.removeFromLeft(100));
    arpArea.removeFromTop(kVGap);

    auto arpKnobRow = arpArea.removeFromTop(68);
    const int knobW = juce::jmin(80, arpKnobRow.getWidth() / 3);
    arpRate.setBounds(arpKnobRow.removeFromLeft(knobW).reduced(2, 0));
    arpPattern.setBounds(arpKnobRow.removeFromLeft(juce::jmin(120, arpKnobRow.getWidth() / 2)).reduced(2, 8));
    arpOct.setBounds(arpKnobRow.reduced(2, 8));
    arpArea.removeFromTop(kVGap);

    auto arpToggleRow = arpArea.removeFromTop(24);
    arpSwing.setBounds(arpToggleRow.removeFromLeft(arpToggleRow.getWidth() / 2).reduced(0, 0));
    auto right = arpToggleRow;
    arpLatch.setBounds(right.removeFromLeft(right.getWidth() / 2));
    arpRestart.setBounds(right);
    arpArea.removeFromTop(kVGap);

    // Visual 32-step grid
    const int gridH = juce::jlimit(48, 100, arpArea.getHeight() / 3);
    stepGrid.setBounds(arpArea.removeFromTop(gridH));
    arpArea.removeFromTop(kVGap);

    // Step detail row
    auto stepRow = arpArea.removeFromTop(26);
    stepSelect.setBounds(stepRow.removeFromLeft(100));
    stepRow.removeFromLeft(6);
    stepOn.setBounds(stepRow.removeFromLeft(50));
    stepRow.removeFromLeft(6);
    const int detW = juce::jmax(60, stepRow.getWidth() / 4);
    stepVel.setBounds(stepRow.removeFromLeft(detW).reduced(1, 0));
    stepDur.setBounds(stepRow.removeFromLeft(detW).reduced(1, 0));
    stepTrn.setBounds(stepRow.removeFromLeft(detW).reduced(1, 0));
    stepRkt.setBounds(stepRow.reduced(1, 0));
    arpArea.removeFromTop(kVGap);

    midiCapture.setBounds(arpArea.removeFromTop(28).removeFromLeft(140));
}

} // namespace wolfsden::ui
