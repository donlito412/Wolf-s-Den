#include "BrowsePage.h"

#include "../../PluginProcessor.h"
#include "../../engine/TheoryEngine.h"

#include <cmath>
#include <set>

namespace wolfsden::ui
{

// =============================================================================
// BrowsePatchCard  — one card in PATCHES mode
// =============================================================================

class BrowsePatchCard : public juce::Component
{
public:
    BrowsePatchCard (BrowsePage& ownerIn, const PresetListing& preset, juce::Colour accentIn)
        : page (ownerIn)
        , presetId (preset.id)
        , cardName (preset.name.c_str())
        , categoryTag (preset.category.c_str())
        , packTag (preset.packName.c_str())
        , isFactory (preset.isFactory)
        , accent (accentIn)
    {
        starBtn.setClickingTogglesState (false);
        starBtn.setTooltip ("Favourite");
        addAndMakeVisible (starBtn);

        if (!isFactory)
        {
            delBtn.setTooltip ("Delete preset");
            addAndMakeVisible (delBtn);
        }

        starBtn.onClick = [this] { page.togglePresetFavourite (presetId); syncStar(); };
        delBtn.onClick  = [this] { page.deletePresetForCard (presetId); };

        addMouseListener (this, true);
        syncStar();
    }

    ~BrowsePatchCard() override { removeMouseListener (this); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &starBtn || e.eventComponent == &delBtn)
            return;
        page.selectPresetForCard (presetId);
    }

    void setCardSelected (bool on) { selected = on; repaint(); }
    int  getPresetId()    const noexcept { return presetId; }

    void syncStar()
    {
        starBtn.setButtonText (page.isPresetFavourite (presetId)
                               ? juce::CharPointer_UTF8 ("\xe2\x98\x85")
                               : juce::CharPointer_UTF8 ("\xe2\x98\x86"));
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (Theme::panelSurface());
        g.fillRoundedRectangle (bounds, 6.f);

        auto strip = bounds.removeFromLeft (5.f);
        g.setColour (accent);
        g.fillRoundedRectangle (strip, 2.f);

        bounds.removeFromLeft (6.f);
        if (selected)
        {
            g.setColour (Theme::accentHot().withAlpha (0.18f));
            g.fillRoundedRectangle (bounds, 6.f);
            g.setColour (Theme::accentHot().withAlpha (0.85f));
            g.drawRoundedRectangle (bounds, 6.f, 2.f);
        }

        auto textArea = bounds.reduced (8.f, 6.f);
        g.setColour (Theme::textPrimary());
        g.setFont (Theme::fontPanelHeader());
        g.drawText (cardName, textArea.removeFromTop (18.f), juce::Justification::centredLeft);

        g.setFont (Theme::fontLabel());
        g.setColour (Theme::textSecondary());
        const juce::String sub = categoryTag + "  ·  " + (isFactory ? packTag : juce::String("User"));
        g.drawText (sub, textArea.removeFromTop (14.f), juce::Justification::centredLeft);

        if (isFactory)
        {
            // Small "FACTORY" badge
            auto badge = textArea.removeFromTop (14.f).removeFromLeft (52.f);
            g.setColour (Theme::accentAlt().withAlpha (0.25f));
            g.fillRoundedRectangle (badge, 3.f);
            g.setColour (Theme::accentAlt());
            g.setFont (Theme::fontLabel().withHeight (9.f));
            g.drawText ("FACTORY", badge, juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4);
        r.removeFromTop (40);
        starBtn.setBounds (r.removeFromRight (28).reduced (0, 2));
        if (!isFactory)
        {
            r.removeFromRight (4);
            delBtn.setBounds (r.removeFromRight (28).reduced (0, 2));
        }
    }

private:
    BrowsePage& page;
    int presetId = -1;
    juce::String cardName, categoryTag, packTag;
    bool isFactory = false;
    juce::Colour accent;
    bool selected = false;
    juce::TextButton starBtn;
    juce::TextButton delBtn { "X" };
};

// =============================================================================
// BrowsePresetCard  — one card in CHORD SETS mode (original)
// =============================================================================

class BrowsePresetCard : public juce::Component
{
public:
    BrowsePresetCard(BrowsePage& ownerIn, const ChordSetListing& row, juce::Colour accentIn)
        : page(ownerIn)
        , chordSetId(row.id)
        , cardName(row.name.c_str())
        , genreTag(row.genre.c_str())
        , moodTag(row.mood.c_str())
        , accent(accentIn)
    {
        starBtn.setClickingTogglesState(false);
        playBtn.setClickingTogglesState(false);
        starBtn.setTooltip("Favourite");
        playBtn.setTooltip("Load name to preset bar (audio preview when available)");
        addAndMakeVisible(starBtn);
        addAndMakeVisible(playBtn);
        starBtn.onClick = [this] {
            page.toggleFavouriteForCard(chordSetId);
        };
        playBtn.onClick = [this] {
            page.playPreviewForCard(chordSetId);
        };
        addMouseListener(this, true);
        syncStar();
    }

    ~BrowsePresetCard() override { removeMouseListener(this); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &starBtn || e.eventComponent == &playBtn)
            return;
        page.selectChordSetForCard(chordSetId);
    }

    void setCardSelected(bool on) { selected = on; repaint(); }

    void syncStar()
    {
        const bool on = page.isChordSetFavourite(chordSetId);
        starBtn.setButtonText(on ? juce::CharPointer_UTF8("\xe2\x98\x85") : juce::CharPointer_UTF8("\xe2\x98\x86"));
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.5f);
        g.setColour(Theme::panelSurface());
        g.fillRoundedRectangle(bounds, 6.f);

        auto strip = bounds.removeFromLeft(5.f);
        g.setColour(accent);
        g.fillRoundedRectangle(strip, 2.f);

        bounds.removeFromLeft(6.f);
        if (selected)
        {
            g.setColour(Theme::accentHot().withAlpha(0.18f));
            g.fillRoundedRectangle(bounds, 6.f);
            g.setColour(Theme::accentHot().withAlpha(0.85f));
            g.drawRoundedRectangle(bounds, 6.f, 2.f);
        }

        auto textArea = bounds.reduced(8.f, 6.f);
        g.setColour(Theme::textPrimary());
        g.setFont(Theme::fontPanelHeader());
        g.drawText(cardName, textArea.removeFromTop(18.f), juce::Justification::centredLeft);

        g.setFont(Theme::fontLabel());
        g.setColour(Theme::textSecondary());
        const juce::String sub = genreTag + "  ·  " + moodTag;
        g.drawText(sub, textArea.removeFromTop(14.f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(6, 4);
        r.removeFromTop(40);
        playBtn.setBounds(r.removeFromRight(52).reduced(0, 2));
        r.removeFromRight(4);
        starBtn.setBounds(r.removeFromRight(32).reduced(0, 2));
    }

    int getChordSetId() const noexcept { return chordSetId; }

private:
    BrowsePage& page;
    int chordSetId = -1;
    juce::String cardName, genreTag, moodTag;
    juce::Colour accent;
    bool selected = false;
    juce::TextButton starBtn;
    juce::TextButton playBtn { "Play" };
};

// =============================================================================

BrowsePage::BrowsePage(WolfsDenAudioProcessor& proc)
    : processor(proc)
{
    setOpaque(true);

    // -------------------------------------------------------------------------
    // Mode toggle buttons
    // -------------------------------------------------------------------------
    modePatchesBtn.setClickingTogglesState (false);
    modeChordSetsBtn.setClickingTogglesState (false);
    modePatchesBtn.onClick   = [this] { switchMode (BrowseMode::Patches);   };
    modeChordSetsBtn.onClick = [this] { switchMode (BrowseMode::ChordSets); };
    addAndMakeVisible (modePatchesBtn);
    addAndMakeVisible (modeChordSetsBtn);

    // -------------------------------------------------------------------------
    // PATCHES sidebar
    // -------------------------------------------------------------------------
    patchSearchBox.setMultiLine (false);
    patchSearchBox.setReturnKeyStartsNewLine (false);
    patchSearchBox.setTextToShowWhenEmpty ("Search instruments...", Theme::textDisabled());
    patchSearchBox.setIndents (6, 4);
    patchSearchBox.setColour (juce::TextEditor::backgroundColourId, Theme::backgroundMid());
    patchSearchBox.setColour (juce::TextEditor::textColourId,        Theme::textPrimary());
    patchSearchBox.setColour (juce::TextEditor::outlineColourId,     Theme::textDisabled());
    patchSearchBox.onTextChange = [this] { rebuildFilteredPresetIndices(); rebuildPatchCardGrid(); };

    patchLabelCategory.setFont (Theme::fontLabel());
    patchLabelCategory.setColour (juce::Label::textColourId, Theme::textSecondary());
    patchLabelCategory.setJustificationType (juce::Justification::centredLeft);
    patchSidebarInner.addAndMakeVisible (patchLabelCategory);

    patchLabelPack.setFont (Theme::fontLabel());
    patchLabelPack.setColour (juce::Label::textColourId, Theme::textSecondary());
    patchLabelPack.setJustificationType (juce::Justification::centredLeft);
    patchSidebarInner.addAndMakeVisible (patchLabelPack);

    patchPackFilter.setColour (juce::ComboBox::backgroundColourId, Theme::backgroundMid());
    patchPackFilter.setColour (juce::ComboBox::textColourId,        Theme::textPrimary());
    patchPackFilter.setColour (juce::ComboBox::outlineColourId,     Theme::textDisabled());
    patchPackFilter.onChange = [this] { rebuildFilteredPresetIndices(); rebuildPatchCardGrid(); };
    patchSidebarInner.addAndMakeVisible (patchPackFilter);
    patchSidebarInner.addAndMakeVisible (patchSearchBox);

    savePresetBtn.setColour (juce::TextButton::buttonColourId, Theme::accentPrimary().withAlpha (0.8f));
    savePresetBtn.setColour (juce::TextButton::textColourOffId, Theme::textPrimary());
    savePresetBtn.onClick = [this] { onSavePresetClicked(); };
    patchSidebarInner.addAndMakeVisible (savePresetBtn);

    addAndMakeVisible (patchSidebarViewport);
    patchSidebarViewport.setViewedComponent (&patchSidebarInner, false);

    // -------------------------------------------------------------------------
    // CHORD SETS sidebar (original)
    // -------------------------------------------------------------------------
    searchBox.setMultiLine(false);
    searchBox.setReturnKeyStartsNewLine(false);
    searchBox.setTextToShowWhenEmpty("Search chord sets...", Theme::textDisabled());
    searchBox.setIndents(6, 4);
    searchBox.setColour(juce::TextEditor::backgroundColourId, Theme::backgroundMid());
    searchBox.setColour(juce::TextEditor::textColourId, Theme::textPrimary());
    searchBox.setColour(juce::TextEditor::outlineColourId, Theme::textDisabled());
    searchBox.onTextChange = [this] {
        rebuildFilteredIndices();
        rebuildCardGrid();
    };

    for (auto* L : { &labelGenre, &labelMood, &labelEnergy, &labelScale })
    {
        L->setFont(Theme::fontLabel());
        L->setColour(juce::Label::textColourId, Theme::textSecondary());
        L->setJustificationType(juce::Justification::centredLeft);
        sidebarInner.addAndMakeVisible(*L);
    }

    for (auto* b : { &energyLow, &energyMed, &energyHigh })
    {
        b->setClickingTogglesState(false);
        sidebarInner.addAndMakeVisible(*b);
    }

    auto syncEnergyVisual = [this] {
        auto dim = Theme::backgroundMid();
        auto hi = Theme::accentPrimary();
        energyLow.setColour(juce::TextButton::buttonColourId, energyFilter == 0 ? hi : dim);
        energyMed.setColour(juce::TextButton::buttonColourId, energyFilter == 1 ? hi : dim);
        energyHigh.setColour(juce::TextButton::buttonColourId, energyFilter == 2 ? hi : dim);
    };

    energyLow.onClick = [this, syncEnergyVisual] {
        energyFilter = (energyFilter == 0 ? -1 : 0);
        syncEnergyVisual();
        rebuildFilteredIndices();
        rebuildCardGrid();
    };
    energyMed.onClick = [this, syncEnergyVisual] {
        energyFilter = (energyFilter == 1 ? -1 : 1);
        syncEnergyVisual();
        rebuildFilteredIndices();
        rebuildCardGrid();
    };
    energyHigh.onClick = [this, syncEnergyVisual] {
        energyFilter = (energyFilter == 2 ? -1 : 2);
        syncEnergyVisual();
        rebuildFilteredIndices();
        rebuildCardGrid();
    };

    scaleFilter.setColour(juce::ComboBox::backgroundColourId, Theme::backgroundMid());
    scaleFilter.setColour(juce::ComboBox::textColourId, Theme::textPrimary());
    scaleFilter.setColour(juce::ComboBox::outlineColourId, Theme::textDisabled());
    scaleFilter.onChange = [this] {
        rebuildFilteredIndices();
        rebuildCardGrid();
    };
    sidebarInner.addAndMakeVisible(scaleFilter);

    sidebarInner.addAndMakeVisible(searchBox);
    addAndMakeVisible(sidebarViewport);
    sidebarViewport.setViewedComponent(&sidebarInner, false);

    addAndMakeVisible(cardViewport);
    cardViewport.setViewedComponent(&cardHolder, false);

    // Start in Patches mode
    switchMode (BrowseMode::Patches);

    setSize(800, 600);
}

void BrowsePage::switchMode (BrowseMode newMode)
{
    currentMode = newMode;

    const bool patches = (currentMode == BrowseMode::Patches);

    modePatchesBtn.setColour   (juce::TextButton::buttonColourId, patches ? Theme::accentPrimary() : Theme::backgroundMid());
    modeChordSetsBtn.setColour (juce::TextButton::buttonColourId, patches ? Theme::backgroundMid() : Theme::accentPrimary());

    patchSidebarViewport.setVisible (patches);
    sidebarViewport.setVisible (!patches);

    if (patches)
    {
        rebuildFilteredPresetIndices();
        rebuildPatchCardGrid();
    }
    else
    {
        rebuildFilteredIndices();
        rebuildCardGrid();
    }
    resized();
}

void BrowsePage::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (!isVisible())
        return;

    if (currentMode == BrowseMode::Patches)
    {
        if (processor.getTheoryEngine().isDatabaseReady())
            processor.getTheoryEngine().reloadPresetListings();

        ensurePatchFiltersBuilt();
        if (patchFiltersBuilt)
        {
            layoutPatchSidebar();
            rebuildFilteredPresetIndices();
            rebuildPatchCardGrid();
        }
    }
    else
    {
        ensureFiltersBuilt();
        if (filtersBuilt)
        {
            layoutSidebarContent();
            sidebarViewport.setViewedComponent(&sidebarInner, false);
            rebuildFilteredIndices();
            rebuildCardGrid();
        }
    }
}

void BrowsePage::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    auto header = getLocalBounds().reduced(12, 8).removeFromTop(28);
    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPageHeader());
    g.drawText("Browse", header, juce::Justification::centredLeft);

    auto main = getLocalBounds().reduced(12, 8);
    main.removeFromTop(36); // page header
    main.removeFromTop(32); // mode toggle strip
    const int detailH = 120;
    auto detail = main.removeFromBottom(detailH);
    g.setColour(Theme::backgroundMid());
    g.fillRoundedRectangle(detail.toFloat().reduced(0.5f), 6.f);
    g.setColour(Theme::textDisabled());
    g.drawRoundedRectangle(detail.toFloat().reduced(0.5f), 6.f, 1.f);

    auto pad = detail.reduced(14, 10);
    g.setColour(Theme::textSecondary());
    g.setFont(Theme::fontLabel());

    juce::String title = "None";
    juce::String author, tags, blurb;
    auto& th = processor.getTheoryEngine();

    if (currentMode == BrowseMode::Patches)
    {
        g.drawText("Selected instrument", pad.removeFromTop(14), juce::Justification::centredLeft);
        if (th.isDatabaseReady())
        {
            const int curId = processor.getCurrentPresetId();
            for (const auto& p : th.getPresetListings())
            {
                if (p.id == curId)
                {
                    title  = p.name.c_str();
                    author = p.author.c_str();
                    tags   = juce::String(p.category.c_str()) + "  ·  " + juce::String(p.packName.c_str());
                    blurb  = p.isFactory ? "Factory instrument — WAV sample" : "User preset";
                    break;
                }
            }
        }
    }
    else
    {
        g.drawText("Selected chord set", pad.removeFromTop(14), juce::Justification::centredLeft);
        if (th.isDatabaseReady())
        {
            for (const auto& r : th.getChordSetListings())
            {
                if (r.id == processor.getBrowseChordSetId())
                {
                    title  = r.name.c_str();
                    author = r.author.c_str();
                    tags   = juce::String(r.genre.c_str()) + " · " + juce::String(r.mood.c_str()) + " · " + juce::String(r.energy.c_str());
                    blurb  = "Chord progression pack · " + scaleNameForId(r.scaleId);
                    break;
                }
            }
        }
    }

    g.setColour(Theme::textPrimary());
    g.setFont(Theme::fontPanelHeader());
    g.drawText(title, pad.removeFromTop(20), juce::Justification::centredLeft);

    g.setFont(Theme::fontValue());
    g.setColour(Theme::textSecondary());
    if (author.isNotEmpty())
        g.drawText("Author: " + author, pad.removeFromTop(16), juce::Justification::centredLeft);
    if (tags.isNotEmpty())
        g.drawText(tags, pad.removeFromTop(16), juce::Justification::centredLeft);
    if (blurb.isNotEmpty())
        g.drawText(blurb, pad, juce::Justification::centredLeft);
}

void BrowsePage::resized()
{
    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(36); // page header row

    // Mode toggle strip
    auto modeStrip = area.removeFromTop(28);
    modeStrip.removeFromBottom(4); // bottom gap
    modePatchesBtn.setBounds   (modeStrip.removeFromLeft(110).reduced(0, 2));
    modeStrip.removeFromLeft(6);
    modeChordSetsBtn.setBounds (modeStrip.removeFromLeft(120).reduced(0, 2));

    area.removeFromTop(4); // gap below strip

    const int detailH = 120;
    auto detailArea = area.removeFromBottom(detailH);
    area.removeFromBottom(8);

    const int gap  = 8;
    const int leftW = juce::jlimit(180, 360, (int) std::lround(area.proportionOfWidth(0.30f)));
    auto leftCol = area.removeFromLeft(leftW);
    area.removeFromLeft(gap);
    auto rightCol = area;

    // Sidebar — show the correct one based on mode
    if (currentMode == BrowseMode::Patches)
    {
        patchSidebarViewport.setBounds (leftCol);
        const int sw = juce::jmax(120, patchSidebarViewport.getWidth() - patchSidebarViewport.getScrollBarThickness());
        patchSidebarInner.setSize (sw, juce::jmax(200, patchSidebarInner.getHeight()));
        ensurePatchFiltersBuilt();
        layoutPatchSidebar();
        patchSidebarViewport.setViewedComponent (&patchSidebarInner, false);
    }
    else
    {
        sidebarViewport.setBounds (leftCol);
        const int sw = juce::jmax(120, sidebarViewport.getWidth() - sidebarViewport.getScrollBarThickness());
        sidebarInner.setSize (sw, sidebarInner.getHeight());
        ensureFiltersBuilt();
        layoutSidebarContent();
        sidebarViewport.setViewedComponent (&sidebarInner, false);
    }

    cardViewport.setBounds(rightCol);
    lastGridW = juce::jmax(200, cardViewport.getViewWidth());

    if (currentMode == BrowseMode::Patches)
    {
        rebuildFilteredPresetIndices();
        rebuildPatchCardGrid();
    }
    else
    {
        rebuildFilteredIndices();
        rebuildCardGrid();
    }

    juce::ignoreUnused(detailArea);
}

// =============================================================================
// PATCHES mode — implementation
// =============================================================================

juce::Colour BrowsePage::categoryAccentColour (const juce::String& cat)
{
    if (cat == "Bass")      return juce::Colour (0xff6366f1);  // indigo
    if (cat == "Keys")      return juce::Colour (0xffeab308);  // amber
    if (cat == "Leads")     return juce::Colour (0xffef4444);  // red
    if (cat == "Pads")      return juce::Colour (0xff38bdf8);  // sky
    if (cat == "Plucks")    return juce::Colour (0xff2dd4bf);  // teal
    if (cat == "Strings")   return juce::Colour (0xffc084fc);  // violet
    if (cat == "Horns")     return juce::Colour (0xfffb923c);  // orange
    if (cat == "Woodwinds") return juce::Colour (0xff4ade80);  // green
    return Theme::accentAlt();
}

void BrowsePage::ensurePatchFiltersBuilt()
{
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady())
        return;

    const size_t n = th.getPresetListings().size();
    if (patchFiltersBuilt && n == patchFilterPresetCount)
        return;

    categoryPills.clear();
    patchPackFilter.clear();
    patchFiltersBuilt = false;
    patchFilterPresetCount = n;

    if (n == 0)
    {
        patchFiltersBuilt = true;
        return;
    }

    // Category pills from distinct categories in preset list
    std::vector<juce::String> cats;
    std::set<juce::String> seen;
    for (const auto& p : th.getPresetListings())
    {
        juce::String c (p.category.c_str());
        if (c.isNotEmpty() && seen.insert(c).second)
            cats.push_back (c);
    }
    std::sort (cats.begin(), cats.end());

    for (const auto& cat : cats)
    {
        auto* b = categoryPills.add (new juce::TextButton (cat));
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonColourId,   Theme::backgroundMid());
        b->setColour (juce::TextButton::buttonOnColourId, categoryAccentColour (cat).withAlpha (0.7f));
        b->setColour (juce::TextButton::textColourOffId,  Theme::textSecondary());
        b->setColour (juce::TextButton::textColourOnId,   Theme::textPrimary());
        b->onClick = [this] { rebuildFilteredPresetIndices(); rebuildPatchCardGrid(); };
        patchSidebarInner.addAndMakeVisible (b);
    }

    // Pack filter combo
    patchPackFilter.addItem ("All packs", kComboAllPacks);
    for (const auto& pk : th.getPackListings())
        patchPackFilter.addItem (pk.name.c_str(), pk.id + kPackComboBase);
    patchPackFilter.setSelectedId (kComboAllPacks, juce::dontSendNotification);

    patchFiltersBuilt = true;
}

void BrowsePage::layoutPatchSidebar()
{
    const int w = patchSidebarInner.getWidth();
    int y = 8;
    const int vGap = 8;
    const int pillH = 22;

    patchSearchBox.setBounds (4, y, w - 8, 26);
    y += 26 + vGap;

    patchLabelCategory.setBounds (4, y, w - 8, 14);
    y += 16;
    {
        int x = 4, rowY = y;
        for (auto* b : categoryPills)
        {
            const int textW = (int) std::ceil ((float) b->getButtonText().length() * 6.5f + 14.f);
            const int pw = juce::jlimit (36, w - 8, textW);
            if (x + pw > w - 4) { x = 4; rowY += pillH + 4; }
            b->setBounds (x, rowY, pw, pillH);
            x += pw + 4;
        }
        y = rowY + pillH + vGap;
    }

    patchLabelPack.setBounds (4, y, w - 8, 14);
    y += 16;
    patchPackFilter.setBounds (4, y, w - 8, 28);
    y += 30 + vGap;

    savePresetBtn.setBounds (4, y, w - 8, 30);
    y += 32 + 8;

    patchSidebarInner.setSize (w, y);
}

bool BrowsePage::presetRowMatchesFilters (const wolfsden::PresetListing& p) const
{
    const juce::String q = patchSearchBox.getText().trim();
    if (q.isNotEmpty())
    {
        const juce::String name (p.name.c_str());
        const juce::String cat  (p.category.c_str());
        if (!name.containsIgnoreCase (q) && !cat.containsIgnoreCase (q))
            return false;
    }

    bool anyCategory = false;
    for (auto* b : categoryPills)
        if (b->getToggleState()) { anyCategory = true; break; }
    if (anyCategory)
    {
        const juce::String cat (p.category.c_str());
        bool hit = false;
        for (auto* b : categoryPills)
            if (b->getToggleState() && b->getButtonText() == cat) { hit = true; break; }
        if (!hit) return false;
    }

    const int pid = patchPackFilter.getSelectedId();
    if (pid != kComboAllPacks)
    {
        const int wantPack = pid - kPackComboBase;
        if (p.packId != wantPack) return false;
    }

    return true;
}

void BrowsePage::rebuildFilteredPresetIndices()
{
    filteredPresetIndices.clear();
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady()) return;
    const auto& list = th.getPresetListings();
    for (int i = 0; i < (int) list.size(); ++i)
        if (presetRowMatchesFilters (list[(size_t)i]))
            filteredPresetIndices.push_back (i);
}

void BrowsePage::rebuildPatchCardGrid()
{
    // Remove only patch cards from cardHolder; chord-set cards are rebuilt separately
    for (auto* c : activePatchCards)
        cardHolder.removeChildComponent (c);
    activePatchCards.clear();
    // deleteAllChildren would nuke both sets — instead, delete owned cards individually
    // (BrowsePresetCard / BrowsePatchCard ownership is via cardHolder's children array)
    cardHolder.deleteAllChildren();
    activePresetCards.clear(); // chord-set cards also cleared; rebuilt when mode switches

    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady()) { cardHolder.setSize (100, 40); return; }

    const auto& list = th.getPresetListings();
    const int vw   = juce::jmax (200, cardViewport.getViewWidth() - 4);
    constexpr int gap    = 8;
    constexpr int minCard = 168;
    const int cols  = juce::jmax (1, (vw + gap) / (minCard + gap));
    const int cellW = (vw - gap * (cols + 1)) / cols;
    const int cardH = 92;

    int col = 0, x = gap, y = gap;
    const int curId = processor.getCurrentPresetId();

    for (int idx : filteredPresetIndices)
    {
        if (idx < 0 || idx >= (int) list.size()) continue;
        const auto& preset = list[(size_t)idx];
        auto* card = new BrowsePatchCard (*this, preset, categoryAccentColour (juce::String (preset.category.c_str())));
        cardHolder.addAndMakeVisible (card);
        activePatchCards.push_back (card);
        card->setBounds (x, y, cellW, cardH);
        card->setCardSelected (preset.id == curId);
        ++col;
        if (col >= cols) { col = 0; x = gap; y += cardH + gap; }
        else x += cellW + gap;
    }

    const int totalH = filteredPresetIndices.empty() ? 120 : (y + cardH + gap);
    cardHolder.setSize (vw, totalH);
}

void BrowsePage::selectPresetForCard (int presetId)
{
    processor.loadPresetById (presetId);

    for (auto* c : activePatchCards)
        c->setCardSelected (c->getPresetId() == presetId);
    repaint();

    if (onPresetOrSelectionChanged)
        onPresetOrSelectionChanged();
}

void BrowsePage::deletePresetForCard (int presetId)
{
    processor.getTheoryEngine().deletePreset (presetId);
    rebuildFilteredPresetIndices();
    rebuildPatchCardGrid();
    repaint();
}

void BrowsePage::togglePresetFavourite (int presetId)
{
    if (favouritePresetIds.count (presetId) != 0u)
        favouritePresetIds.erase (presetId);
    else
        favouritePresetIds.insert (presetId);
    for (auto* c : activePatchCards) c->syncStar();
}

bool BrowsePage::isPresetFavourite (int presetId) const
{
    return favouritePresetIds.count (presetId) != 0u;
}

void BrowsePage::onSavePresetClicked()
{
    auto* dialog = new juce::AlertWindow ("Save Preset",
                                          "Enter a name for your preset:",
                                          juce::AlertWindow::NoIcon);
    dialog->addTextEditor ("name", processor.getPresetDisplayName(), "Name:");
    dialog->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                const juce::String name = dialog->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                {
                    processor.saveCurrentAsPreset (name, "User");
                    patchFiltersBuilt = false; // force pill rebuild (new category possible)
                    ensurePatchFiltersBuilt();
                    rebuildFilteredPresetIndices();
                    rebuildPatchCardGrid();
                    if (onPresetOrSelectionChanged)
                        onPresetOrSelectionChanged();
                }
            }
            delete dialog;
        }), false);
}

void BrowsePage::syncPresetSelectionFromProcessor()
{
    if (currentMode != BrowseMode::Patches) return;
    const int curId = processor.getCurrentPresetId();
    for (auto* c : activePatchCards)
        c->setCardSelected (c->getPresetId() == curId);
    repaint();
}

// =============================================================================
// CHORD SETS mode — original implementation follows
// =============================================================================

void BrowsePage::ensureFiltersBuilt()
{
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady() || filtersBuilt)
        return;

    const auto& rows = th.getChordSetListings();

    const juce::String specGenres[] = { "Trap", "Lo-Fi", "R&B", "Jazz", "EDM", "Cinematic", "Classical", "World" };
    juce::StringArray orderedGenres;
    std::set<juce::String> seen;

    for (auto& s : specGenres)
    {
        orderedGenres.add(s);
        seen.insert(s);
    }

    std::vector<juce::String> extras;
    for (const auto& r : rows)
    {
        juce::String g(r.genre.c_str());
        if (g.isNotEmpty() && seen.insert(g).second)
            extras.push_back(g);
    }
    std::sort(extras.begin(), extras.end(), [](const juce::String& a, const juce::String& b) {
        return a.compareIgnoreCase(b) < 0;
    });
    for (auto& g : extras)
        orderedGenres.add(g);

    for (int i = 0; i < orderedGenres.size(); ++i)
    {
        auto* b = genrePills.add(new juce::TextButton(orderedGenres[i]));
        b->setClickingTogglesState(true);
        b->setColour(juce::TextButton::buttonColourId, Theme::backgroundMid());
        b->setColour(juce::TextButton::buttonOnColourId, Theme::accentPrimary());
        b->setColour(juce::TextButton::textColourOffId, Theme::textSecondary());
        b->setColour(juce::TextButton::textColourOnId, Theme::textPrimary());
        b->onClick = [this] {
            rebuildFilteredIndices();
            rebuildCardGrid();
        };
        sidebarInner.addAndMakeVisible(b);
    }

    const juce::String specMoods[] = { "Dark", "Bright", "Tense", "Dreamy", "Aggressive", "Calm" };
    juce::StringArray orderedMoods;
    seen.clear();

    for (auto& s : specMoods)
    {
        orderedMoods.add(s);
        seen.insert(s);
    }

    extras.clear();
    for (const auto& r : rows)
    {
        juce::String m(r.mood.c_str());
        if (m.isNotEmpty() && seen.insert(m).second)
            extras.push_back(m);
    }
    std::sort(extras.begin(), extras.end(), [](const juce::String& a, const juce::String& b) {
        return a.compareIgnoreCase(b) < 0;
    });
    for (auto& m : extras)
        orderedMoods.add(m);

    for (int i = 0; i < orderedMoods.size(); ++i)
    {
        auto* b = moodPills.add(new juce::TextButton(orderedMoods[i]));
        b->setClickingTogglesState(true);
        b->setColour(juce::TextButton::buttonColourId, Theme::backgroundMid());
        b->setColour(juce::TextButton::buttonOnColourId, Theme::accentPrimary());
        b->setColour(juce::TextButton::textColourOffId, Theme::textSecondary());
        b->setColour(juce::TextButton::textColourOnId, Theme::textPrimary());
        b->onClick = [this] {
            rebuildFilteredIndices();
            rebuildCardGrid();
        };
        sidebarInner.addAndMakeVisible(b);
    }

    scaleFilter.addItem("All scales", kComboAllScales);
    for (const auto& sd : th.getScaleDefinitions())
        scaleFilter.addItem(juce::String(sd.name.c_str()), sd.id + kScaleComboBase);
    scaleFilter.setSelectedId(kComboAllScales, juce::dontSendNotification);

    filtersBuilt = true;
}

void BrowsePage::layoutSidebarContent()
{
    const int w = sidebarInner.getWidth();
    int y = 8;
    const int vGap = 8;
    const int pillH = 22;

    searchBox.setBounds(4, y, w - 8, 26);
    y += 26 + vGap;

    labelGenre.setBounds(4, y, w - 8, 14);
    y += 16;
    {
        int x = 4;
        int rowY = y;
        for (auto* b : genrePills)
        {
            const int textW = (int) std::ceil((float) b->getButtonText().length() * 6.5f + 14.f);
            const int pw = juce::jlimit(36, w - 8, textW);
            if (x + pw > w - 4)
            {
                x = 4;
                rowY += pillH + 4;
            }
            b->setBounds(x, rowY, pw, pillH);
            x += pw + 4;
        }
        y = rowY + pillH + vGap;
    }

    labelMood.setBounds(4, y, w - 8, 14);
    y += 16;
    {
        int x = 4;
        int rowY = y;
        for (auto* b : moodPills)
        {
            const int textW = (int) std::ceil((float) b->getButtonText().length() * 6.5f + 14.f);
            const int pw = juce::jlimit(36, w - 8, textW);
            if (x + pw > w - 4)
            {
                x = 4;
                rowY += pillH + 4;
            }
            b->setBounds(x, rowY, pw, pillH);
            x += pw + 4;
        }
        y = rowY + pillH + vGap;
    }

    labelEnergy.setBounds(4, y, w - 8, 14);
    y += 16;
    const int ew = (w - 16) / 3;
    energyLow.setBounds(4, y, ew, 26);
    energyMed.setBounds(4 + ew + 4, y, ew, 26);
    energyHigh.setBounds(4 + 2 * (ew + 4), y, ew, 26);
    y += 26 + vGap;

    labelScale.setBounds(4, y, w - 8, 16);
    y += 18;
    scaleFilter.setBounds(4, y, w - 8, 30);
    y += 32 + 12;

    sidebarInner.setSize(w, y);
}

juce::String BrowsePage::scaleNameForId(int scaleId) const
{
    for (const auto& sd : processor.getTheoryEngine().getScaleDefinitions())
        if (sd.id == scaleId)
            return sd.name.c_str();
    return {};
}

juce::Colour BrowsePage::moodAccentColour(const juce::String& mood)
{
    if (mood == "Dark") return juce::Colour(0xff38bdf8);
    if (mood == "Bright") return juce::Colour(0xffeab308);
    if (mood == "Tense") return Theme::error();
    if (mood == "Dreamy") return Theme::accentPrimary();
    if (mood == "Aggressive") return juce::Colour(0xfffb7185);
    if (mood == "Calm") return juce::Colour(0xff2dd4bf);
    if (mood == "Energetic") return Theme::error().withAlpha(0.85f);
    if (mood == "Peaceful") return Theme::accentAlt();
    if (mood == "Soulful") return juce::Colour(0xffc4b5fd);
    if (mood == "Sophisticated") return juce::Colour(0xff94a3b8);
    return Theme::accentAlt();
}

bool BrowsePage::rowMatchesFilters(const ChordSetListing& c) const
{
    const juce::String q = searchBox.getText().trim();
    if (q.isNotEmpty())
    {
        const juce::String name(c.name.c_str());
        const juce::String genre(c.genre.c_str());
        const juce::String mood(c.mood.c_str());
        if (!name.containsIgnoreCase(q) && !genre.containsIgnoreCase(q) && !mood.containsIgnoreCase(q))
            return false;
    }

    bool anyGenre = false;
    for (auto* b : genrePills)
        if (b->getToggleState())
        {
            anyGenre = true;
            break;
        }
    if (anyGenre)
    {
        const juce::String g(c.genre.c_str());
        bool hit = false;
        for (auto* b : genrePills)
            if (b->getToggleState() && b->getButtonText() == g)
            {
                hit = true;
                break;
            }
        if (!hit)
            return false;
    }

    bool anyMood = false;
    for (auto* b : moodPills)
        if (b->getToggleState())
        {
            anyMood = true;
            break;
        }
    if (anyMood)
    {
        const juce::String m(c.mood.c_str());
        bool hit = false;
        for (auto* b : moodPills)
            if (b->getToggleState() && b->getButtonText() == m)
            {
                hit = true;
                break;
            }
        if (!hit)
            return false;
    }

    if (energyFilter >= 0)
    {
        static const char* energies[] = { "Low", "Medium", "High" };
        if (juce::String(c.energy.c_str()) != energies[energyFilter])
            return false;
    }

    const int sid = scaleFilter.getSelectedId();
    if (sid != kComboAllScales)
    {
        const int wantScale = sid - kScaleComboBase;
        if (c.scaleId != wantScale)
            return false;
    }

    return true;
}

void BrowsePage::rebuildFilteredIndices()
{
    filteredRowIndices.clear();
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady())
        return;

    const auto& rows = th.getChordSetListings();
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rowMatchesFilters(rows[(size_t) i]))
            filteredRowIndices.push_back(i);
}

void BrowsePage::rebuildCardGrid()
{
    cardHolder.deleteAllChildren();
    activePresetCards.clear();
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady())
    {
        cardHolder.setSize(100, 40);
        return;
    }

    const auto& rows = th.getChordSetListings();
    const int vw = juce::jmax(200, cardViewport.getViewWidth() - 4);
    constexpr int gap = 8;
    constexpr int minCard = 168;
    const int cols = juce::jmax(1, (vw + gap) / (minCard + gap));
    const int cellW = (vw - gap * (cols + 1)) / cols;
    const int cardH = 88;

    int col = 0, x = gap, y = gap;
    for (int idx : filteredRowIndices)
    {
        if (idx < 0 || idx >= (int) rows.size())
            continue;
        const auto& row = rows[(size_t) idx];
        auto* card = new BrowsePresetCard(*this, row, moodAccentColour(juce::String(row.mood.c_str())));
        cardHolder.addAndMakeVisible(card);
        activePresetCards.push_back(card);
        card->setBounds(x, y, cellW, cardH);
        card->setCardSelected(row.id == processor.getBrowseChordSetId());
        ++col;
        if (col >= cols)
        {
            col = 0;
            x = gap;
            y += cardH + gap;
        }
        else
            x += cellW + gap;
    }

    const int totalH = filteredRowIndices.empty() ? 120 : (y + cardH + gap);
    cardHolder.setSize(vw, totalH);
}

void BrowsePage::selectChordSetForCard(int id)
{
    processor.setBrowseChordSetSelection(id);
    if (onPresetOrSelectionChanged)
        onPresetOrSelectionChanged();

    for (auto* pc : activePresetCards)
        pc->setCardSelected(pc->getChordSetId() == id);
    repaint();
}

void BrowsePage::syncSelectionFromProcessor()
{
    if (currentMode == BrowseMode::Patches)
    {
        syncPresetSelectionFromProcessor();
    }
    else
    {
        const int id = processor.getBrowseChordSetId();
        for (auto* pc : activePresetCards)
            pc->setCardSelected (pc->getChordSetId() == id);
        repaint();
    }
}

void BrowsePage::toggleFavouriteForCard(int id)
{
    if (favouriteIds.count(id) != 0u)
        favouriteIds.erase(id);
    else
        favouriteIds.insert(id);

    for (auto* pc : activePresetCards)
        pc->syncStar();
}

void BrowsePage::playPreviewForCard(int id)
{
    selectChordSetForCard(id);
}

bool BrowsePage::isChordSetFavourite(int id) const
{
    return favouriteIds.count(id) != 0u;
}

} // namespace wolfsden::ui
