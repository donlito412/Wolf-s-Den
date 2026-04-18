#include "PluginEditor.h"
#include "PluginProcessor.h"

WolfsDenAudioProcessorEditor::WolfsDenAudioProcessorEditor(WolfsDenAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , audioProcessor(p)
    , mainComponent(p)
{
    addAndMakeVisible(mainComponent);
    
    // Restore size from processor state. Default opens at 900×620.
    // If the stored value is the placeholder (480×320), use the default instead.
    const int storedW = p.getEditorWidth();
    const int storedH = p.getEditorHeight();
    constexpr int kDefaultW = 900;
    constexpr int kDefaultH = 620;
    constexpr int kMinW     = 800;
    constexpr int kMinH     = 560;
    const int w = (storedW <= 480) ? kDefaultW : storedW;
    const int h = (storedH <= 320) ? kDefaultH : storedH;
    setSize(juce::jlimit(kMinW, 2560, w), juce::jlimit(kMinH, 1600, h));

    setResizable(true, true);
    setResizeLimits(kMinW, kMinH, 2560, 1600);
    setWantsKeyboardFocus(true);
}

bool WolfsDenAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    const bool cmd   = key.getModifiers().isCommandDown();
    const bool shift = key.getModifiers().isShiftDown();

    if (cmd && key.getKeyCode() == 'Z')
    {
        if (shift)
            audioProcessor.getUndoManager().redo();
        else
            audioProcessor.getUndoManager().undo();
        return true;
    }
    return false;
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
