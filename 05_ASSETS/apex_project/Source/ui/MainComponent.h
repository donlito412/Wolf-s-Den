#pragma once

#include "chrome/BottomBar.h"
#include "chrome/TopBar.h"
#include "pages/BrowsePage.h"
#include "pages/FxPage.h"
#include "pages/ModPage.h"
#include "pages/PerformPage.h"
#include "pages/SynthPage.h"
#include "pages/TheoryPage.h"
#include "theme/WolfsDenLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

class WolfsDenAudioProcessor;

namespace juce
{
class MidiKeyboardComponent;
}

namespace wolfsden::ui
{

/** TASK_009 root: top bar, page stack, bottom bar. */
class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    explicit MainComponent(WolfsDenAudioProcessor&);
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void showPage(int index);
    void timerCallback() override;

    enum class PageFade { idle, fadingOut, fadingIn };

    WolfsDenAudioProcessor& processor;

    std::unique_ptr<WolfsDenLookAndFeel> laf;
    TopBar topBar;
    std::unique_ptr<juce::MidiKeyboardComponent> midiKeyboard;
    BottomBar bottomBar;

    BrowsePage pageBrowse;
    SynthPage pageSynth;
    TheoryPage pageTheory;
    PerformPage pagePerform;
    FxPage pageFx;
    ModPage pageMod;

    juce::Component* pages[6] {};
    int currentPage = 1;

    bool pageFirstShowDone = false;
    PageFade pageFadePhase = PageFade::idle;
    int pageFadeAccumMs = 0;
    uint32_t pageFadeLastCounterMs = 0;
    int pageFadeFrom = 0;
    int pageFadeTo = 0;
    int pageFadeQueued = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace wolfsden::ui

using MainComponent = wolfsden::ui::MainComponent;
