#include "PluginEditor.h"
#include "PluginProcessor.h"

WolfsDenAudioProcessorEditor::WolfsDenAudioProcessorEditor(WolfsDenAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , audioProcessor(p)
    , mainComponent(p)
{
    addAndMakeVisible(mainComponent);
    setSize(1200, 780);
    // Second arg = show corner resizer. With false, many hosts never expose a resize affordance.
    setResizable(true, true);
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
