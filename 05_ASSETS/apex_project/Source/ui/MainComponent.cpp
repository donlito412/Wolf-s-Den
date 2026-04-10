#include "MainComponent.h"

#include "../PluginProcessor.h"
#include "theme/UITheme.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace wolfsden::ui
{
namespace
{
constexpr int kFadeOutMs = 75;
constexpr int kFadeInMs = 75;

float smooth01(float t)
{
    t = juce::jlimit(0.f, 1.f, t);
    return t * t * (3.f - 2.f * t);
}
} // namespace

MainComponent::MainComponent(WolfsDenAudioProcessor& p)
    : processor(p)
    , topBar(p)
    , bottomBar(p)
    , pageBrowse(p)
    , pageSynth(p)
    , pageTheory(p)
    , pagePerform(p)
    , pageFx(p)
    , pageMod(p)
{
    laf = std::make_unique<WolfsDenLookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel(laf.get());

    addAndMakeVisible(topBar);

    midiKeyboard = std::make_unique<juce::MidiKeyboardComponent>(p.getMidiKeyboardState(),
                                                                 juce::MidiKeyboardComponent::horizontalKeyboard);
    addAndMakeVisible(*midiKeyboard);
    midiKeyboard->setVelocity(0.85f, false);
    midiKeyboard->setKeyWidth(15.0f);
    midiKeyboard->setScrollButtonWidth(28);
    midiKeyboard->setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, Theme::textDisabled());
    midiKeyboard->setColour(juce::MidiKeyboardComponent::whiteNoteColourId, Theme::panelSurface());
    midiKeyboard->setColour(juce::MidiKeyboardComponent::blackNoteColourId, Theme::backgroundDark());
    midiKeyboard->setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Theme::accentHot().withAlpha(0.25f));
    midiKeyboard->setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, Theme::accentPrimary().withAlpha(0.45f));

    addAndMakeVisible(bottomBar);

    pages[0] = &pageBrowse;
    pages[1] = &pageSynth;
    pages[2] = &pageTheory;
    pages[3] = &pagePerform;
    pages[4] = &pageFx;
    pages[5] = &pageMod;

    for (auto* pg : pages)
    {
        pg->setOpaque(false);
        addChildComponent(*pg);
    }

    topBar.onSelectPage = [this](int i) { showPage(i); };
    pageBrowse.onPresetOrSelectionChanged = [this] { topBar.refreshPresetLabel(); };
    showPage(1);
    juce::ignoreUnused(processor);
}

MainComponent::~MainComponent()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MainComponent::showPage(int index)
{
    const int next = juce::jlimit(0, 5, index);
    topBar.setActivePage(next);

    if (!pageFirstShowDone)
    {
        pageFirstShowDone = true;
        currentPage = next;
        for (int k = 0; k < 6; ++k)
        {
            pages[(size_t)k]->setVisible(k == currentPage);
            pages[(size_t)k]->setAlpha(1.f);
        }
        resized();
        return;
    }

    if (pageFadePhase != PageFade::idle)
    {
        pageFadeQueued = next;
        return;
    }

    if (next == currentPage)
        return;

    pageFadeFrom = currentPage;
    pageFadeTo = next;
    pageFadePhase = PageFade::fadingOut;
    pageFadeAccumMs = 0;
    pageFadeLastCounterMs = juce::Time::getMillisecondCounter();
    startTimerHz(60);
}

void MainComponent::timerCallback()
{
    const uint32_t now = juce::Time::getMillisecondCounter();
    const int dt = pageFadeLastCounterMs == 0 ? 16 : (int)(now - pageFadeLastCounterMs);
    pageFadeLastCounterMs = now;
    pageFadeAccumMs += juce::jmax(1, dt);

    if (pageFadePhase == PageFade::fadingOut)
    {
        const float t = smooth01((float)pageFadeAccumMs / (float)kFadeOutMs);
        pages[(size_t)pageFadeFrom]->setAlpha(1.f - t);
        if (pageFadeAccumMs >= kFadeOutMs)
        {
            pages[(size_t)pageFadeFrom]->setVisible(false);
            pages[(size_t)pageFadeFrom]->setAlpha(1.f);
            currentPage = pageFadeTo;
            pages[(size_t)currentPage]->setVisible(true);
            pages[(size_t)currentPage]->setAlpha(0.f);
            pageFadePhase = PageFade::fadingIn;
            pageFadeAccumMs = 0;
            resized();
        }
    }
    else if (pageFadePhase == PageFade::fadingIn)
    {
        const float t = smooth01((float)pageFadeAccumMs / (float)kFadeInMs);
        pages[(size_t)currentPage]->setAlpha(t);
        if (pageFadeAccumMs >= kFadeInMs)
        {
            pages[(size_t)currentPage]->setAlpha(1.f);
            pageFadePhase = PageFade::idle;
            stopTimer();
            pageFadeLastCounterMs = 0;
            if (pageFadeQueued >= 0)
            {
                const int q = pageFadeQueued;
                pageFadeQueued = -1;
                if (q != currentPage)
                    showPage(q);
            }
        }
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    topBar.setBounds(r.removeFromTop(52));
    bottomBar.setBounds(r.removeFromBottom(44));
    if (midiKeyboard != nullptr)
        midiKeyboard->setBounds(r.removeFromBottom(68));
    for (int k = 0; k < 6; ++k)
        if (pages[(size_t)k]->isVisible())
            pages[(size_t)k]->setBounds(r);
}

} // namespace wolfsden::ui
