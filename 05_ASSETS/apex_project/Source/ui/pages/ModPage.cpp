#include "ModPage.h"

#include "../../PluginProcessor.h"
#include "../PerfXyPad.h"

namespace wolfsden::ui
{
namespace
{
using S = wolfsden::ModMatrix::Source;
using T = wolfsden::ModMatrix::Target;

S comboToSource(int cid)
{
    if (cid <= 1)
        return S::Off;
    return (S)(cid - 2);
}

int sourceToCombo(S s)
{
    if (s == S::Off)
        return 1;
    return (int)s + 2;
}

T comboToTarget(int cid)
{
    if (cid <= 1)
        return T::None;
    return (T)(cid - 2);
}

int targetToCombo(T t)
{
    if (t == T::None)
        return 1;
    return (int)t + 2;
}

bool isMuted(int fl)
{
    return (fl & 1) != 0;
}
bool isInverted(int fl)
{
    return (fl & 2) != 0;
}
int layerScopeFromFlags(int fl)
{
    return (fl >> 8) & 15;
}

constexpr const char* kSrcNames[] = { "Global LFO", "Filter env", "Amp env",      "Mod wheel", "Aftertouch",
                                    "Velocity",   "Pitch bend", "XY X",         "XY Y",      "Random" };

constexpr const char* kTgtNames[] = {
    "L1 cutoff", "L1 res",     "L2 cutoff", "L2 res",     "L3 cutoff", "L3 res",     "L4 cutoff", "L4 res",
    "LFO rate",  "LFO depth",  "Master vol", "FX reverb", "FX delay",  "FX chorus",  "L1 pitch",  "L2 pitch",
    "L3 pitch",  "L4 pitch",   "L1 amp",    "L2 amp",     "L3 amp",    "L4 amp",     "L1 pan",    "L2 pan",
    "L3 pan",    "L4 pan",
};
static_assert(sizeof(kTgtNames) / sizeof(kTgtNames[0]) == (size_t)T::NumTargets, "target label count");

/** Return colour for source type highlighting. */
juce::Colour sourceRowColour(S src)
{
    switch (src)
    {
        case S::GlobalLFO:   return Theme::accentPrimary().withAlpha(0.15f);  // Cyan for LFO
        case S::FilterEnv:
        case S::AmpEnv:      return juce::Colour(0xff2dd4bf).withAlpha(0.15f); // Teal for envelopes
        case S::ModWheel:
        case S::Aftertouch:
        case S::Velocity:
        case S::PitchBend:   return Theme::warning().withAlpha(0.12f);         // Gold for MIDI
        case S::XY_X:
        case S::XY_Y:        return Theme::accentAlt().withAlpha(0.12f);       // Blue for XY
        case S::Random:      return Theme::error().withAlpha(0.08f);           // Red hint for random
        case S::Off:
        case S::NumSources:  return juce::Colours::transparentBlack;
    }
}

} // namespace

ModPage::ModPage(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
    for (int p = 1; p <= 4; ++p)
        slotPage.addItem("Slots " + juce::String((p - 1) * 8 + 1) + "–" + juce::String(p * 8), p);
    slotPage.setSelectedId(1, juce::dontSendNotification);
    slotPage.onChange = [this] { syncPageFromMatrix(); };
    addAndMakeVisible(slotPage);

    for (auto& row : rows)
    {
        row.src.addItem("Off", 1);
        for (int i = 0; i < (int)S::NumSources; ++i)
            row.src.addItem(kSrcNames[(size_t)i], i + 2);

        row.tgt.addItem("None", 1);
        for (int i = 0; i < (int)T::NumTargets; ++i)
            row.tgt.addItem(kTgtNames[(size_t)i], i + 2);

        row.scope.addItem("All", 1);
        row.scope.addItem("L1", 2);
        row.scope.addItem("L2", 3);
        row.scope.addItem("L3", 4);
        row.scope.addItem("L4", 5);

        row.amt.setRange(-1.0, 1.0, 0.01);
        row.amt.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);

        addAndMakeVisible(row.src);
        addAndMakeVisible(row.tgt);
        addAndMakeVisible(row.amt);
        addAndMakeVisible(row.inv);
        addAndMakeVisible(row.mute);
        addAndMakeVisible(row.scope);
    }

    for (int r = 0; r < 8; ++r)
    {
        rows[(size_t)r].src.onChange = [this, r] { applyRow(r); };
        rows[(size_t)r].tgt.onChange = [this, r] { applyRow(r); };
        rows[(size_t)r].scope.onChange = [this, r] { applyRow(r); };
        rows[(size_t)r].amt.onValueChange = [this, r] { applyRow(r); };
        rows[(size_t)r].inv.onClick = [this, r] { applyRow(r); };
        rows[(size_t)r].mute.onClick = [this, r] { applyRow(r); };
    }

    xyPhysics.addItem("Direct", 1);
    xyPhysics.addItem("Inertia", 2);
    xyPhysics.addItem("Chaos", 3);
    addAndMakeVisible(xyPhysics);
    attXyPhys = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "perf_xy_physics", xyPhysics);

    // XY Depth slider
    xyDepth.setSliderStyle(juce::Slider::LinearHorizontal);
    xyDepth.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
    xyDepth.setRange(0.0, 1.0, 0.01);
    xyDepth.setValue(1.0, juce::dontSendNotification);
    addAndMakeVisible(xyDepth);

    addAndMakeVisible(diceBtn);
    diceBtn.onClick = [this] { diceAssignments(); };

    addAndMakeVisible(recordBtn);
    recordBtn.onClick = [this] {
        if (xyRecording)
        {
            const juce::File written = stopXyRecordAndSave();
            recordBtn.setButtonText("Record");
            recordBtn.setColour(juce::TextButton::buttonColourId, juce::Colour());
            if (written.existsAsFile())
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "XY Record",
                    "Saved to Desktop:\n" + written.getFileName()
                    + "\n\nCC 74 = X axis, CC 1 = Y axis.\nDrag into your DAW as MIDI automation.",
                    "OK");
            else
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon, "XY Record",
                    "Nothing was recorded.", "OK");
        }
        else
        {
            startXyRecord();
            recordBtn.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x96\xa0")) + juce::String(" Stop"));
            recordBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffcc2222));
        }
    };

    xyPad = std::make_unique<wolfsden::PerfXyPad>(processor);
    xyPad->onPositionChanged = [this](float x, float y) {
        if (!xyRecording) return;
        XyRecordEvent ev;
        ev.timeMs = juce::Time::currentTimeMillis() - xyRecordStartMs;
        ev.x = x;
        ev.y = y;
        xyRecordBuffer.push_back(ev);
    };
    addAndMakeVisible(*xyPad);

    syncPageFromMatrix();
}

ModPage::~ModPage() = default;

void ModPage::syncPageFromMatrix()
{
    auto& mm = processor.getModMatrix();
    const int base = (slotPage.getSelectedId() - 1) * 8;
    for (int r = 0; r < 8; ++r)
    {
        const int si = base + r;
        const int s = mm.slotSrc[(size_t)si].load(std::memory_order_relaxed);
        const int t = mm.slotTgt[(size_t)si].load(std::memory_order_relaxed);
        const float a = mm.slotAmt[(size_t)si].load(std::memory_order_relaxed);
        const int fl = mm.slotFlags[(size_t)si].load(std::memory_order_relaxed);

        rows[(size_t)r].src.setSelectedId(sourceToCombo((S)s), juce::dontSendNotification);
        rows[(size_t)r].tgt.setSelectedId(targetToCombo((T)t), juce::dontSendNotification);
        rows[(size_t)r].amt.setValue(a, juce::dontSendNotification);
        rows[(size_t)r].mute.setToggleState(isMuted(fl), juce::dontSendNotification);
        rows[(size_t)r].inv.setToggleState(isInverted(fl), juce::dontSendNotification);
        const int ls = layerScopeFromFlags(fl);
        rows[(size_t)r].scope.setSelectedId(ls == 0 ? 1 : ls + 1, juce::dontSendNotification);
    }
    repaint();
}

void ModPage::applyRow(int r)
{
    auto& mm = processor.getModMatrix();
    const int base = (slotPage.getSelectedId() - 1) * 8;
    const int si = juce::jlimit(0, wolfsden::ModMatrix::kSlots - 1, base + r);
    const int sc = rows[(size_t)r].scope.getSelectedId();
    const int lscope = (sc <= 1) ? 0 : (sc - 1);

    mm.setSlot(si,
               comboToSource(rows[(size_t)r].src.getSelectedId()),
               comboToTarget(rows[(size_t)r].tgt.getSelectedId()),
               (float)rows[(size_t)r].amt.getValue(),
               rows[(size_t)r].mute.getToggleState(),
               rows[(size_t)r].inv.getToggleState(),
               0.08f,
               lscope);
    repaint();
}

void ModPage::diceAssignments()
{
    juce::Random rng(juce::Time::getMillisecondCounter());
    auto& mm = processor.getModMatrix();
    for (int si = 2; si < wolfsden::ModMatrix::kSlots; ++si)
    {
        const S src = (S)rng.nextInt((int)S::NumSources);
        const T tgt = (T)rng.nextInt((int)T::NumTargets);
        const float amt = rng.nextFloat() * 1.4f - 0.7f;
        mm.setSlot(si, src, tgt, amt, false, rng.nextBool(), 0.06f + rng.nextFloat() * 0.1f, 0);
    }
    syncPageFromMatrix();
}

void ModPage::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    // Page title
    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPageHeader());
    g.drawText("Modulation", getLocalBounds().removeFromTop(26).reduced(14, 0), juce::Justification::centredLeft);

    // Color-coded row backgrounds
    auto r = getLocalBounds().reduced(14, 12);
    r.removeFromTop(26);
    const int leftW = juce::jmin(560, juce::jmax(420, (int)((float)r.getWidth() * 0.56f)));
    auto left = r.withWidth(leftW);
    left.removeFromTop(28); // match resized title push down
    left.removeFromTop(32); // slot page combo + gap

    // Column widths — purely proportional so they always fit inside leftW
    const int aw    = leftW;
    const int invW  = 28;
    const int mutW  = 24;
    const int scpW  = 48;
    const int flexW = aw - invW - mutW - scpW;
    const int srcW  = flexW * 28 / 100;
    const int tgtW  = flexW * 38 / 100;
    const int amtW  = flexW - srcW - tgtW;

    const int rowH = 28;
    for (int i = 0; i < 8; ++i)
    {
        const S src = comboToSource(rows[(size_t)i].src.getSelectedId());
        const juce::Colour bg = sourceRowColour(src);
        if (!bg.isTransparent())
        {
            auto rowR = juce::Rectangle<int>(left.getX(), left.getY() + i * rowH, left.getWidth(), rowH);
            g.setColour(bg);
            g.fillRoundedRectangle(rowR.toFloat(), 3.f);
        }

        // Red dot out-of-range indicator on amount
        const float amt = (float)rows[(size_t)i].amt.getValue();
        if (std::abs(amt) > 0.9f)
        {
            auto amtBounds = rows[(size_t)i].amt.getBounds();
            g.setColour(Theme::error());
            g.fillEllipse((float)(amtBounds.getRight() - 8), (float)(amtBounds.getY() + 2), 5.f, 5.f);
        }
    }

    // XY Pad section labels
    auto titleR = r.withTrimmedLeft(leftW + 10);
    g.setColour(Theme::textSecondary());
    g.setFont(Theme::fontPanelHeader());
    g.drawText("XY Performance Pad", titleR.removeFromTop(20), juce::Justification::centred);

    r.removeFromTop(28); // push BOTH sides down by title height

    // Matrix column headers — positioned after slotPage (26) + gap (6)
    const int hdrY = r.getY() + 26 + 6;
    auto hdrR = juce::Rectangle<int>(r.getX(), hdrY, aw, 14);
    auto h = hdrR;
    g.setColour(Theme::textSecondary());
    g.setFont(Theme::fontLabel());
    g.drawText("Source", h.removeFromLeft(srcW), juce::Justification::centredLeft);
    g.drawText("Target", h.removeFromLeft(tgtW), juce::Justification::centredLeft);
    g.drawText("Amount", h.removeFromLeft(amtW), juce::Justification::centredLeft);
    g.drawText("Inv", h.removeFromLeft(invW), juce::Justification::centred);
    g.drawText("M",   h.removeFromLeft(mutW), juce::Justification::centred);
    g.drawText("Scope", h, juce::Justification::centred);
}

void ModPage::resized()
{
    auto r = getLocalBounds().reduced(14, 12);
    r.removeFromTop(26);
    const int leftW = juce::jmin(560, juce::jmax(420, (int)((float)r.getWidth() * 0.56f)));
    r.removeFromTop(28); // match title space

    auto left = r.removeFromLeft(leftW);
    r.removeFromLeft(10);
    auto right = r;

    slotPage.setBounds(left.removeFromTop(26));
    left.removeFromTop(6);
    left.removeFromTop(16); // header label space

    const int rowH  = 28;
    // Purely proportional — never exceed leftW
    const int aw    = left.getWidth();
    const int invW  = 28;
    const int mutW  = 24;
    const int scpW  = 48;
    const int flexW = aw - invW - mutW - scpW;
    const int srcW  = flexW * 28 / 100;
    const int tgtW  = flexW * 38 / 100;
    const int amtW  = flexW - srcW - tgtW;

    for (int i = 0; i < 8; ++i)
    {
        auto row = left.removeFromTop(rowH);
        rows[(size_t)i].src.setBounds(row.removeFromLeft(srcW).reduced(0, 1));
        rows[(size_t)i].tgt.setBounds(row.removeFromLeft(tgtW).reduced(0, 1));
        rows[(size_t)i].amt.setBounds(row.removeFromLeft(amtW).reduced(0, 1));
        rows[(size_t)i].inv.setBounds(row.removeFromLeft(invW).reduced(0, 2));
        rows[(size_t)i].mute.setBounds(row.removeFromLeft(mutW).reduced(0, 2));
        rows[(size_t)i].scope.setBounds(row.removeFromLeft(scpW).reduced(0, 1));
    }

    // Right side: XY pad section
    xyPhysics.setBounds(right.removeFromTop(26));
    right.removeFromTop(4);

    auto depthRow = right.removeFromTop(24);
    xyDepth.setBounds(depthRow);
    right.removeFromTop(4);

    auto btnRow = right.removeFromTop(28);
    diceBtn.setBounds(btnRow.removeFromLeft(72).reduced(0, 2));
    btnRow.removeFromLeft(6);
    recordBtn.setBounds(btnRow.removeFromLeft(88).reduced(0, 2));

    right.removeFromTop(8);
    const int xy = juce::jmin(400, juce::jmin(right.getWidth(), right.getHeight() - 8));
    xyPad->setBounds(right.withSizeKeepingCentre(xy, xy));
}

// =============================================================================
// XY Recording
// =============================================================================

void ModPage::startXyRecord()
{
    xyRecordBuffer.clear();
    xyRecordBuffer.reserve(8192);
    xyRecordStartMs = juce::Time::currentTimeMillis();
    xyRecording = true;
}

juce::File ModPage::stopXyRecordAndSave()
{
    xyRecording = false;
    if (xyRecordBuffer.empty()) return {};

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(960);

    juce::MidiMessageSequence seq;
    constexpr double kBPM = 120.0;
    constexpr double kTicksPerSec = (kBPM / 60.0) * 960.0;

    for (const auto& ev : xyRecordBuffer)
    {
        const double ticks = (double)ev.timeMs * 0.001 * kTicksPerSec;
        // X → CC 74 (Filter Cutoff — standard XY X mapping)
        seq.addEvent(juce::MidiMessage::controllerEvent(1, 74, juce::jlimit(0, 127, (int)(ev.x * 127.f))), ticks);
        // Y → CC 1 (Mod Wheel — standard XY Y mapping)
        seq.addEvent(juce::MidiMessage::controllerEvent(1,  1, juce::jlimit(0, 127, (int)(ev.y * 127.f))), ticks);
    }
    mf.addTrack(seq);

    const auto desktop = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    const auto file = desktop.getNonexistentChildFile("WolfsDen_XY_Automation", ".mid");
    if (auto out = file.createOutputStream())
    {
        mf.writeTo(*out);
        out->flush();
    }
    file.revealToUser();
    return file;
}

} // namespace wolfsden::ui
