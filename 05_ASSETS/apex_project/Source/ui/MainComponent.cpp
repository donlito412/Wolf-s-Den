#include "MainComponent.h"

#include "../PluginProcessor.h"

namespace wolfsden
{

MainComponent::MainComponent(WolfsDenAudioProcessor& p)
    : processor(p)
{
    juce::ignoreUnused(processor);

    titleLabel.setText("Wolf's Den", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions().withHeight(28.0f)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void MainComponent::resized()
{
    titleLabel.setBounds(getLocalBounds().withSizeKeepingCentre(300, 40));
}

} // namespace wolfsden
