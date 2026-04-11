#include "WDSampleBrowserOverlay.h"
#include "../../PluginProcessor.h"

namespace wolfsden::ui
{

WDSampleBrowserOverlay::WDSampleBrowserOverlay (WolfsDenAudioProcessor& proc)
    : processor (proc)
{
    // Opaque root avoids subpixel text fringing when compositing over the synth page.
    setOpaque (true);

    titleLabel.setText ("Sample Browser", juce::dontSendNotification);
    titleLabel.setFont (Theme::fontPanelHeader());
    titleLabel.setColour (juce::Label::textColourId, Theme::textPrimary());
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    searchBox.setTextToShowWhenEmpty ("Search samples…", Theme::textDisabled());
    searchBox.setFont (Theme::fontLabel());
    searchBox.setIndents (8, 6);
    searchBox.setMultiLine (false);
    searchBox.setScrollbarsShown (false);
    searchBox.setColour (juce::TextEditor::backgroundColourId, Theme::panelSurface());
    searchBox.setColour (juce::TextEditor::textColourId, Theme::textPrimary());
    searchBox.setColour (juce::TextEditor::outlineColourId, Theme::textDisabled());
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    categoryCombo.addItem ("All Categories", 1);
    for (auto& cat : { "Bass","Horns","Keys","Leads","Pads","Plucks","Strings","Woodwinds","FX","Loops","General" })
        categoryCombo.addItem (cat, categoryCombo.getNumItems() + 1);
    categoryCombo.setSelectedId (1, juce::dontSendNotification);
    categoryCombo.onChange = [this] { refreshList(); };
    addAndMakeVisible (categoryCombo);

    listBox.setModel (this);
    listBox.setRowHeight (30);
    listBox.setColour (juce::ListBox::backgroundColourId, Theme::backgroundMid());
    listBox.setColour (juce::ListBox::outlineColourId, Theme::textDisabled());
    listBox.setOutlineThickness (1);
    addAndMakeVisible (listBox);

    previewBtn.setColour (juce::TextButton::buttonColourId, Theme::panelSurface());
    previewBtn.setColour (juce::TextButton::textColourOffId, Theme::accentAlt());
    previewBtn.onClick = [this]
    {
        const int row = listBox.getSelectedRow();
        if (row >= 0 && row < (int) displayedEntries.size())
            previewEntry (displayedEntries[(size_t) row]);
    };
    addAndMakeVisible (previewBtn);

    loadBtn.setColour (juce::TextButton::buttonColourId, Theme::accentPrimary());
    loadBtn.setColour (juce::TextButton::textColourOffId, Theme::textPrimary());
    loadBtn.onClick = [this]
    {
        const int row = listBox.getSelectedRow();
        if (row >= 0 && row < (int) displayedEntries.size())
        {
            if (onSampleChosen)
                onSampleChosen (activeLayerIndex, displayedEntries[(size_t) row]);
            stopPreview();
            setVisible (false);
        }
    };
    addAndMakeVisible (loadBtn);

    closeBtn.setColour (juce::TextButton::buttonColourId, Theme::panelSurface());
    closeBtn.setColour (juce::TextButton::textColourOffId, Theme::textSecondary());
    closeBtn.onClick = [this] { stopPreview(); setVisible (false); };
    addAndMakeVisible (closeBtn);

    previewFormatManager.registerBasicFormats();

    setVisible (false);
}

WDSampleBrowserOverlay::~WDSampleBrowserOverlay()
{
    stopPreview();
}

void WDSampleBrowserOverlay::open (int layerIndex)
{
    activeLayerIndex = layerIndex;
    refreshList();
    setVisible (true);
    toFront (true);
}

void WDSampleBrowserOverlay::paint (juce::Graphics& g)
{
    g.fillAll (Theme::backgroundDark());
    const auto inner = getLocalBounds().toFloat().reduced (10.f);
    g.setColour (Theme::panelSurface());
    g.fillRoundedRectangle (inner, 8.f);
    g.setColour (Theme::accentPrimary());
    g.drawRoundedRectangle (inner, 8.f, 1.5f);
}

void WDSampleBrowserOverlay::resized()
{
    auto r = getLocalBounds().reduced (16, 12);

    // Title row
    auto titleRow = r.removeFromTop (28);
    closeBtn.setBounds (titleRow.removeFromRight (28));
    titleRow.removeFromRight (6);
    titleLabel.setBounds (titleRow);

    r.removeFromTop (8);

    // Search + category row (taller row fits single-line combo text)
    auto filterRow = r.removeFromTop (30);
    categoryCombo.setBounds (filterRow.removeFromRight (148));
    filterRow.removeFromRight (6);
    searchBox.setBounds (filterRow);

    r.removeFromTop (8);

    // Button row at bottom
    auto btnRow = r.removeFromBottom (30);
    loadBtn.setBounds (btnRow.removeFromRight (120));
    btnRow.removeFromRight (8);
    previewBtn.setBounds (btnRow.removeFromRight (100));

    r.removeFromBottom (8);

    // List takes remaining space
    listBox.setBounds (r);
}

//==============================================================================
// ListBoxModel

int WDSampleBrowserOverlay::getNumRows()
{
    return (int) displayedEntries.size();
}

void WDSampleBrowserOverlay::paintListBoxItem (int row, juce::Graphics& g,
                                               int w, int h, bool selected)
{
    if (selected)
        g.fillAll (Theme::accentPrimary().withAlpha (0.35f));
    else if (row % 2 == 0)
        g.fillAll (Theme::backgroundDark().withAlpha (0.4f));

    if (row < 0 || row >= (int) displayedEntries.size())
        return;

    const auto& e = displayedEntries[(size_t) row];

    const int pad = 8;
    const int wPack = juce::jmin (100, juce::jmax (56, w / 6));
    const int wName = juce::jmin (juce::jmax (160, w * 52 / 100), w - pad * 3 - wPack - 48);
    const int wCat  = juce::jmax (40, w - pad * 2 - wName - wPack);

    g.setFont (Theme::fontLabel());
    g.setColour (Theme::textPrimary());
    g.drawText (e.name, pad, 0, wName, h, juce::Justification::centredLeft, true);

    g.setColour (Theme::textSecondary());
    g.drawText (e.category, pad + wName, 0, wCat, h, juce::Justification::centredLeft, true);

    g.setColour (Theme::textDisabled());
    g.drawText (e.packId, pad + wName + wCat, 0, wPack, h, juce::Justification::centredRight, true);
}

void WDSampleBrowserOverlay::listBoxItemClicked (int /*row*/, const juce::MouseEvent&) {}

void WDSampleBrowserOverlay::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) displayedEntries.size())
        previewEntry (displayedEntries[(size_t) row]);
}

//==============================================================================
// TextEditor::Listener

void WDSampleBrowserOverlay::textEditorTextChanged (juce::TextEditor&)
{
    refreshList();
}

//==============================================================================
// Timer

void WDSampleBrowserOverlay::timerCallback()
{
    // Nothing needed; preview plays asynchronously via AudioSourcePlayer
}

//==============================================================================
// Helpers

void WDSampleBrowserOverlay::refreshList()
{
    auto& lib = processor.getSampleLibrary();

    const juce::String search = searchBox.getText();
    const int catId = categoryCombo.getSelectedId();
    const juce::String cat = (catId <= 1) ? juce::String{} : categoryCombo.getText();

    displayedEntries = lib.filter (cat, {}, search);
    listBox.updateContent();
    listBox.repaint();
}

void WDSampleBrowserOverlay::previewEntry (const SampleEntry& e)
{
    stopPreview();

    const juce::File f (e.filePath);
    if (! f.existsAsFile()) return;

    auto* reader = previewFormatManager.createReaderFor (f);
    if (! reader) return;

    if (! previewSetup)
    {
        previewDeviceManager.initialiseWithDefaultDevices (0, 2);
        previewPlayer = std::make_unique<juce::AudioSourcePlayer>();
        previewDeviceManager.addAudioCallback (previewPlayer.get());
        previewSetup = true;
    }

    previewSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    previewSource->setLooping (false);
    previewPlayer->setSource (previewSource.get());
}

void WDSampleBrowserOverlay::stopPreview()
{
    if (previewPlayer)
        previewPlayer->setSource (nullptr);
    previewSource.reset();
}

} // namespace wolfsden::ui
