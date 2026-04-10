#include "BottomBar.h"

#include "../../PluginProcessor.h"
#include "../../engine/TheoryEngine.h"

namespace wolfsden::ui
{
namespace
{
const char* const kRoots[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

BottomBar::BottomBar(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(30);

    auto setup = [](juce::Label& L, const juce::String& t) {
        L.setText(t, juce::dontSendNotification);
        L.setFont(Theme::fontLabel());
        L.setColour(juce::Label::textColourId, Theme::textSecondary());
        L.setJustificationType(juce::Justification::centredLeft);
    };

    setup(midiLabel, "MIDI");
    setup(chordLabel, "Chord —");
    setup(scaleLabel, "Scale —");
    setup(polyLabel, "Poly 0/16");
    setup(syncLabel, "Sync —");

    addAndMakeVisible(midiLabel);
    addAndMakeVisible(chordLabel);
    addAndMakeVisible(scaleLabel);
    addAndMakeVisible(polyLabel);
    addAndMakeVisible(syncLabel);
}

BottomBar::~BottomBar()
{
    stopTimer();
}

void BottomBar::timerCallback()
{
    if (processor.consumeMidiActivityFlag())
        midiFlash = 1.f;
    else
        midiFlash = juce::jmax(0.f, midiFlash - 0.08f);

    meterL = processor.getOutputPeakL();
    meterR = processor.getOutputPeakR();

    auto& th = processor.getTheoryEngine();
    const auto m = th.getBestMatch();
    juce::String chordTxt = "Chord —";
    if (m.chordId >= 0 && m.score > 0.02f)
    {
        const auto& defs = th.getChordDefinitions();
        if (m.chordId < (int)defs.size())
        {
            const int r = juce::jlimit(0, 11, m.rootNote);
            chordTxt = juce::String(kRoots[(size_t)r]) + " " + juce::String(defs[(size_t)m.chordId].symbol);
        }
    }
    chordLabel.setText(chordTxt, juce::dontSendNotification);

    juce::String scaleTxt = "Scale —";
    if (th.isDatabaseReady())
    {
        const int root = juce::jlimit(0, 11, th.getActiveScaleRoot());
        const int st = th.getActiveScaleType();
        const auto& scales = th.getScaleDefinitions();
        if (st >= 0 && st < (int)scales.size())
            scaleTxt = juce::String(kRoots[(size_t)root]) + " " + juce::String(scales[(size_t)st].name);
    }
    scaleLabel.setText(scaleTxt, juce::dontSendNotification);

    const int v = processor.getSynthEngine().countActiveVoices();
    polyLabel.setText("Poly " + juce::String(v) + "/16", juce::dontSendNotification);

    bool playing = false;
    if (auto* ph = static_cast<juce::AudioProcessor&>(processor).getPlayHead())
    {
        if (const auto pos = ph->getPosition())
            playing = pos->getIsPlaying();
    }
    syncLabel.setText(playing ? "Sync Live" : "Sync Stopped", juce::dontSendNotification);

    repaint();
}

void BottomBar::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(Theme::backgroundMid(),
                               0.f,
                               0.f,
                               Theme::backgroundMid().darker(0.12f),
                               0.f,
                               (float)getHeight(),
                               false);
    g.setGradientFill(grad);
    g.fillAll();

    g.setColour(Theme::textDisabled().withAlpha(0.65f));
    g.drawLine(0.f, 0.f, (float)getWidth(), 0.f, 1.f);

    auto r = getLocalBounds().reduced(8, 4);
    const float meterW = 8.f;
    const float gap = 6.f;
    auto mr = r.removeFromRight(48);
    const float h = (float)mr.getHeight() - 4.f;
    const float xL = (float)mr.getX();
    const float xR = xL + meterW + gap;
    const float y0 = (float)mr.getY() + 2.f;

    g.setColour(Theme::backgroundDark());
    g.fillRect(xL, y0, meterW, h);
    g.fillRect(xR, y0, meterW, h);

    g.setColour(Theme::accentAlt());
    g.fillRect(xL, y0 + h * (1.f - juce::jlimit(0.f, 1.f, meterL)), meterW, h * juce::jlimit(0.f, 1.f, meterL));
    g.setColour(Theme::accentPrimary());
    g.fillRect(xR, y0 + h * (1.f - juce::jlimit(0.f, 1.f, meterR)), meterW, h * juce::jlimit(0.f, 1.f, meterR));

    const float cx = 24.f;
    const float cy = (float)getHeight() * 0.5f;
    if (midiFlash > 0.01f)
    {
        g.setColour(Theme::success().withAlpha(midiFlash));
        g.fillEllipse(cx - 5.f, cy - 5.f, 10.f, 10.f);
    }
    else
    {
        g.setColour(Theme::textDisabled());
        g.drawEllipse(cx - 5.f, cy - 5.f, 10.f, 10.f, 1.f);
    }
}

void BottomBar::resized()
{
    auto r = getLocalBounds().reduced(8, 4);
    r.removeFromLeft(44); // MIDI LED area
    midiLabel.setBounds(r.removeFromLeft(40));
    chordLabel.setBounds(r.removeFromLeft(juce::jmax(120, getWidth() / 6)));
    scaleLabel.setBounds(r.removeFromLeft(juce::jmax(140, getWidth() / 6)));
    polyLabel.setBounds(r.removeFromLeft(72));
    syncLabel.setBounds(r.removeFromLeft(100));
}

} // namespace wolfsden::ui
