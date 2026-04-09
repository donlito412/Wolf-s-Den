#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class WolfsDenAudioProcessor;

namespace wolfsden
{

class MainComponent : public juce::Component
{
public:
    explicit MainComponent(WolfsDenAudioProcessor&);
    ~MainComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    WolfsDenAudioProcessor& processor;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace wolfsden
