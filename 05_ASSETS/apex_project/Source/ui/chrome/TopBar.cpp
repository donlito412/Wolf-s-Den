#include "TopBar.h"

#include "../../PluginProcessor.h"

namespace wolfsden::ui
{
TopBar::TopBar(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(8);
    logoLabel.setText("Wolf's Den", juce::dontSendNotification);
    logoLabel.setFont(Theme::fontPanelHeader());
    logoLabel.setColour(juce::Label::textColourId, Theme::accentHot());
    addAndMakeVisible(logoLabel);

    for (auto* t : tabs)
    {
        t->setClickingTogglesState(false);
        t->setTriggeredOnMouseDown(true);
        addAndMakeVisible(t);
    }

    tabBrowse.onClick = [this] { tabClicked(0); };
    tabSynth.onClick = [this] { tabClicked(1); };
    tabTheory.onClick = [this] { tabClicked(2); };
    tabPerform.onClick = [this] { tabClicked(3); };
    tabFx.onClick = [this] { tabClicked(4); };
    tabMod.onClick = [this] { tabClicked(5); };

    presetLabel.setJustificationType(juce::Justification::centred);
    presetLabel.setFont(Theme::fontValue());
    presetLabel.setColour(juce::Label::textColourId, Theme::textPrimary());
    addAndMakeVisible(presetLabel);

    addAndMakeVisible(presetPrev);
    addAndMakeVisible(presetNext);
    presetPrev.onClick = [this] {
        processor.setPresetDisplayName("Preset (prev)");
        refreshPresetLabel();
    };
    presetNext.onClick = [this] {
        processor.setPresetDisplayName("Preset (next)");
        refreshPresetLabel();
    };

    addAndMakeVisible(settingsBtn);
    settingsBtn.onClick = [] {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                "Wolf's Den",
                                                "Settings — audio/MIDI from your DAW host.",
                                                "OK");
    };

    cpuLabel.setFont(Theme::fontLabel());
    cpuLabel.setColour(juce::Label::textColourId, Theme::textSecondary());
    cpuLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(cpuLabel);

    setActivePage(1);
    refreshPresetLabel();
}

TopBar::~TopBar()
{
    stopTimer();
}

void TopBar::timerCallback()
{
    const float c = processor.getSmoothedCpuLoad();
    cpuLabel.setText("CPU " + juce::String((int)juce::jlimit(0.f, 999.f, c * 100.f)) + "%", juce::dontSendNotification);
}

void TopBar::refreshPresetLabel()
{
    presetLabel.setText(processor.getPresetDisplayName(), juce::dontSendNotification);
}

void TopBar::setActivePage(int index)
{
    activePage = juce::jlimit(0, 5, index);
    repaint();
}

void TopBar::tabClicked(int index)
{
    setActivePage(index);
    if (onSelectPage)
        onSelectPage(index);
}

void TopBar::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(Theme::backgroundMid().brighter(0.045f),
                               0.f,
                               0.f,
                               Theme::backgroundMid(),
                               0.f,
                               (float)getHeight(),
                               false);
    g.setGradientFill(grad);
    g.fillAll();

    g.setColour(Theme::textDisabled().withAlpha(0.65f));
    g.drawLine(0.f, (float)getHeight() - 1.f, (float)getWidth(), (float)getHeight() - 1.f, 1.f);

    for (int i = 0; i < 6; ++i)
    {
        if (i == activePage)
        {
            auto b = tabs[(size_t)i]->getBounds().toFloat();
            g.setColour(Theme::accentHot());
            g.drawLine(b.getX() + 4.f, b.getBottom() - 2.f, b.getRight() - 4.f, b.getBottom() - 2.f, 2.f);
        }
    }
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced(8, 6);
    logoLabel.setBounds(r.removeFromLeft(100));
    r.removeFromLeft(12);

    const int tabW = juce::jlimit(72, 100, r.getWidth() / 10);
    for (int i = 0; i < 6; ++i)
        tabs[(size_t)i]->setBounds(r.removeFromLeft(tabW).reduced(2, 0));

    r.removeFromLeft(16);
    auto centre = r.removeFromLeft(juce::jmin(280, r.getWidth() / 3));
    presetPrev.setBounds(centre.removeFromLeft(28));
    presetLabel.setBounds(centre.reduced(4, 0));
    presetNext.setBounds(centre.removeFromRight(28));

    cpuLabel.setBounds(r.removeFromRight(72));
    settingsBtn.setBounds(r.removeFromRight(88));
}

} // namespace wolfsden::ui
