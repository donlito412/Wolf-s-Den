#pragma once

#include "../theme/UITheme.h"

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

class WolfsDenAudioProcessor;

namespace wolfsden::ui
{

/** TASK_009 top bar: logo, page tabs, preset, settings, CPU. */
class TopBar : public juce::Component,
                 private juce::Timer
{
public:
    explicit TopBar(WolfsDenAudioProcessor& proc);
    ~TopBar() override;

    std::function<void(int pageIndex)> onSelectPage;
    /** Fired after < > cycles to a new preset; UI should sync card selection. */
    std::function<void()> onPresetNavigate;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setActivePage(int index);
    int getActivePage() const noexcept { return activePage; }

    void refreshPresetLabel();

private:
    void timerCallback() override;
    void tabClicked(int index);

    WolfsDenAudioProcessor& processor;

    juce::Label logoLabel;
    juce::TextButton tabBrowse { "BROWSE" };
    juce::TextButton tabSynth { "SYNTH" };
    juce::TextButton tabTheory { "THEORY" };
    juce::TextButton tabPerform { "PERFORM" };
    juce::TextButton tabFx { "FX" };
    juce::TextButton tabMod { "MOD" };
    juce::TextButton presetPrev { "<" };
    juce::TextButton presetNext { ">" };
    juce::Label presetLabel;
    juce::TextButton settingsBtn { "Settings" };
    juce::Label cpuLabel;

    juce::TextButton* const tabs[6] { &tabBrowse, &tabSynth, &tabTheory, &tabPerform, &tabFx, &tabMod };
    int activePage = 1; // default SYNTH

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};

} // namespace wolfsden::ui
