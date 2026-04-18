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
    /** Fired when < > buttons are clicked. */
    std::function<void(int delta)> onPresetNavigateRequested;
    /** Fired after a preset is selected (arrows or popup menu). */
    std::function<void()> onPresetNavigate;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setActivePage(int index);
    int getActivePage() const noexcept { return activePage; }

    void refreshPresetLabel();

private:
    void timerCallback() override;
    void tabClicked(int index);
    /** Shows a PopupMenu of all presets when the preset label is clicked. */
    void mouseDown(const juce::MouseEvent& e) override;
    void showPresetMenu();

    WolfsDenAudioProcessor& processor;

    juce::Label logoLabel;
    juce::TextButton tabSynth { "SYNTH" };
    juce::TextButton tabComposition { "COMPOSE" };
    juce::TextButton tabFx { "FX" };
    juce::TextButton tabMod { "MOD" };
    juce::TextButton presetPrev { "<" };
    juce::TextButton presetNext { ">" };
    juce::Label presetLabel;   // click to open preset popup menu
    juce::TextButton settingsBtn { "Settings" };
    juce::TextButton undoBtn { "Undo" };
    juce::TextButton redoBtn { "Redo" };
    juce::Label cpuLabel;

    juce::TextButton* const tabs[4] { &tabSynth, &tabComposition, &tabFx, &tabMod };
    int activePage = 0; // default SYNTH

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};

} // namespace wolfsden::ui
