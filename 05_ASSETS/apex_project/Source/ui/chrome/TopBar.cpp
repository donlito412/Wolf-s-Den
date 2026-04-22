#include "TopBar.h"

#include "../../PluginProcessor.h"
#include <map>

namespace wolfsden::ui
{
TopBar::TopBar(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
    startTimerHz(8);
    logoLabel.setText("HOWLING WOLVES", juce::dontSendNotification);
    logoLabel.setFont(Theme::fontPanelHeader());
    logoLabel.setColour(juce::Label::textColourId, Theme::accentHot());
    addAndMakeVisible(logoLabel);

    versionLabel.setText("v3.2", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(juce::FontOptions(10.f)));
    versionLabel.setColour(juce::Label::textColourId, Theme::textSecondary());
    versionLabel.setJustificationType(juce::Justification::bottomLeft);
    versionLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(versionLabel);



    for (auto* t : tabs)
    {
        t->setClickingTogglesState(false);
        t->setTriggeredOnMouseDown(true);
        addAndMakeVisible(t);
    }

    tabSynth.onClick       = [this] { tabClicked(0); };
    tabComposition.onClick = [this] { tabClicked(1); };
    tabFx.onClick          = [this] { tabClicked(2); };
    tabMod.onClick         = [this] { tabClicked(3); };

    presetLabel.setJustificationType(juce::Justification::centred);
    presetLabel.setFont(Theme::fontValue());
    presetLabel.setColour(juce::Label::textColourId, Theme::textPrimary());
    presetLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(presetLabel);

    addAndMakeVisible(presetPrev);
    addAndMakeVisible(presetNext);
    presetPrev.onClick = [this] {
        processor.cyclePreset (-1);
        refreshPresetLabel();
        if (onPresetNavigate)
            onPresetNavigate();
    };
    presetNext.onClick = [this] {
        processor.cyclePreset (+1);
        refreshPresetLabel();
        if (onPresetNavigate)
            onPresetNavigate();
    };

    addAndMakeVisible(settingsBtn);
    settingsBtn.onClick = [] {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                "Howling Wolves",
                                                "Settings — audio/MIDI from your DAW host.",
                                                "OK");
    };

    cpuLabel.setFont(Theme::fontLabel());
    cpuLabel.setColour(juce::Label::textColourId, Theme::textSecondary());
    cpuLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(cpuLabel);

    setActivePage(0);
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
    activePage = juce::jlimit(0, 3, index);
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

    for (int i = 0; i < 4; ++i)
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
    auto logoArea = r.removeFromLeft(160);
    logoLabel.setBounds(logoArea.removeFromTop(logoArea.getHeight() * 2 / 3));
    versionLabel.setBounds(logoArea);
    r.removeFromLeft(12);

    const int tabW = juce::jlimit(72, 100, r.getWidth() / 8);
    for (int i = 0; i < 4; ++i)
        tabs[(size_t)i]->setBounds(r.removeFromLeft(tabW).reduced(2, 0));

    r.removeFromLeft(16);
    auto centre = r.removeFromLeft(juce::jmin(420, juce::jmax(240, r.getWidth() / 2)));
    presetPrev.setBounds(centre.removeFromLeft(28));
    presetLabel.setBounds(centre.reduced(4, 0));
    presetNext.setBounds(centre.removeFromRight(28));

    cpuLabel.setBounds(r.removeFromRight(72));
    settingsBtn.setBounds(r.removeFromRight(88));
}

void TopBar::mouseDown(const juce::MouseEvent& e)
{
    if (presetLabel.getBounds().contains(e.getPosition()))
        showPresetMenu();
}

void TopBar::showPresetMenu()
{
    const auto& listings = processor.getTheoryEngine().getPresetListings();
    if (listings.empty())
        return;

    juce::PopupMenu menu;
    const int currentId = processor.getCurrentPresetId();

    // Group presets by category into submenus
    std::map<juce::String, juce::PopupMenu> categoryMenus;
    for (int i = 0; i < (int)listings.size(); ++i)
    {
        juce::String cat(listings[(size_t)i].category);
        if (cat.isEmpty()) cat = "Other";
        categoryMenus[cat].addItem(i + 1, juce::String(listings[(size_t)i].name), true, listings[(size_t)i].id == currentId);
    }
    for (auto& [cat, sub] : categoryMenus)
        menu.addSubMenu(cat, sub);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetLabel),
                       [this](int result)
                       {
                           if (result > 0)
                           {
                               processor.loadPresetById(processor.getTheoryEngine().getPresetListings()[(size_t)(result - 1)].id);
                               refreshPresetLabel();
                               if (onPresetNavigate)
                                   onPresetNavigate();
                           }
                       });
}

} // namespace wolfsden::ui
