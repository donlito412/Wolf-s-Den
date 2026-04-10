#pragma once

#include "../theme/UITheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

class WolfsDenAudioProcessor;

namespace wolfsden::ui
{

class BottomBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit BottomBar(WolfsDenAudioProcessor& proc);
    ~BottomBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    WolfsDenAudioProcessor& processor;

    juce::Label midiLabel;
    juce::Label chordLabel;
    juce::Label scaleLabel;
    juce::Label polyLabel;
    juce::Label syncLabel;

    float midiFlash = 0.f;
    float meterL = 0.f;
    float meterR = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomBar)
};

} // namespace wolfsden::ui
