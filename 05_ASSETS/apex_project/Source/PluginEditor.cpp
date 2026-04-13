#include "PluginEditor.h"
#include "PluginProcessor.h"

WolfsDenAudioProcessorEditor::WolfsDenAudioProcessorEditor(WolfsDenAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , audioProcessor(p)
    , mainComponent(p)
{
    addAndMakeVisible(mainComponent);
    
    // Restore size from processor state, or default to 1200x780
    const int w = p.getEditorWidth();
    const int h = p.getEditorHeight();
    // Minimum size keeps labelled knobs / ADSR rows readable (narrow windows used to crunch layout).
    constexpr int kMinW = 960;
    constexpr int kMinH = 700;
    setSize(juce::jlimit(kMinW, 2560, w), juce::jlimit(kMinH, 1600, h));

    // Second arg = show corner resizer. With false, many hosts never expose a resize affordance.
    setResizable(true, true);
    setResizeLimits(kMinW, kMinH, 2560, 1600);
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
