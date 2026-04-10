#include "PluginEditor.h"
#include "PluginProcessor.h"

WolfsDenAudioProcessorEditor::WolfsDenAudioProcessorEditor(WolfsDenAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , audioProcessor(p)
    , mainComponent(p)
{
    addAndMakeVisible(mainComponent);
    setSize(1200, 780);
    setResizable(true, false);
    setResizeLimits(800, 550, 2560, 1600);
}

void WolfsDenAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void WolfsDenAudioProcessorEditor::resized()
{
    mainComponent.setBounds(getLocalBounds());
    audioProcessor.setLastEditorBounds(getWidth(), getHeight());
}
