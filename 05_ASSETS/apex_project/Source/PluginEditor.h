#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/MainComponent.h"

class WolfsDenAudioProcessor;

class WolfsDenAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit WolfsDenAudioProcessorEditor(WolfsDenAudioProcessor&);
    ~WolfsDenAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    WolfsDenAudioProcessor& audioProcessor;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WolfsDenAudioProcessorEditor)
};
