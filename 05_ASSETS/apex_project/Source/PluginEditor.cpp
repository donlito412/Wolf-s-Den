#include "PluginEditor.h"
#include "PluginProcessor.h"

WolfsDenAudioProcessorEditor::WolfsDenAudioProcessorEditor(WolfsDenAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , audioProcessor(p)
    , mainComponent(p)
{
    addAndMakeVisible(mainComponent);
    setSize(720, 520);
    setResizable(true, true);
    setResizeLimits(480, 400, 2000, 1200);
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
