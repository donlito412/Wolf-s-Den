#include "BrowsePage.h"

#include "../../PluginProcessor.h"
#include "../../engine/TheoryEngine.h"

#include <cmath>
#include <set>

namespace wolfsden::ui
{

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
        g.setColour(selected ? Theme::accentHot().withAlpha(0.12f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle(bounds, 6.f);

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
    searchBox.setMultiLine(false);
    searchBox.setReturnKeyStartsNewLine(false);
    searchBox.setTextToShowWhenEmpty("Search presets…", Theme::textDisabled());
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

    setSize(800, 600);
}

void BrowsePage::visibilityChanged()
{
    juce::Component::visibilityChanged();
    if (!isVisible())
        return;
    ensureFiltersBuilt();
    if (filtersBuilt)
    {
        layoutSidebarContent();
        sidebarViewport.setViewedComponent(&sidebarInner, false);
        rebuildFilteredIndices();
        rebuildCardGrid();
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
    main.removeFromTop(36);
    const int detailH = 120;
    auto detail = main.removeFromBottom(detailH);
    g.setColour(Theme::backgroundMid());
    g.fillRoundedRectangle(detail.toFloat().reduced(0.5f), 6.f);
    g.setColour(Theme::textDisabled());
    g.drawRoundedRectangle(detail.toFloat().reduced(0.5f), 6.f, 1.f);

    auto pad = detail.reduced(14, 10);
    g.setColour(Theme::textSecondary());
    g.setFont(Theme::fontLabel());
    g.drawText("Selected preset", pad.removeFromTop(14), juce::Justification::centredLeft);

    juce::String title = "None";
    juce::String author, tags, blurb;

    auto& th = processor.getTheoryEngine();
    if (th.isDatabaseReady())
    {
        for (const auto& r : th.getChordSetListings())
        {
            if (r.id == selectedChordSetId)
            {
                title = r.name.c_str();
                author = r.author.c_str();
                tags = juce::String(r.genre.c_str()) + " · " + juce::String(r.mood.c_str()) + " · " + juce::String(r.energy.c_str());
                blurb = "Chord progression pack · " + scaleNameForId(r.scaleId);
                break;
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
    area.removeFromTop(36);
    const int detailH = 120;
    auto detailArea = area.removeFromBottom(detailH); // detail strip at bottom
    area.removeFromBottom(8);                          // gap above detail
    // area is now the main sidebar + cards region
    const int gap = 8;
    const int leftW = juce::jlimit(180, 360, (int) std::lround(area.proportionOfWidth(0.30f)));
    auto leftCol = area.removeFromLeft(leftW);
    area.removeFromLeft(gap);
    auto rightCol = area;

    sidebarViewport.setBounds(leftCol);
    const int sw = juce::jmax(120, sidebarViewport.getWidth() - sidebarViewport.getScrollBarThickness());
    sidebarInner.setSize(sw, sidebarInner.getHeight());
    ensureFiltersBuilt();
    layoutSidebarContent();
    sidebarViewport.setViewedComponent(&sidebarInner, false);

    cardViewport.setBounds(rightCol);
    lastGridW = juce::jmax(200, cardViewport.getViewWidth());
    rebuildFilteredIndices();
    rebuildCardGrid();

    juce::ignoreUnused(detailArea); // painted in paint(), not laid out here
}

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

    labelScale.setBounds(4, y, w - 8, 14);
    y += 16;
    scaleFilter.setBounds(4, y, w - 8, 28);
    y += 28 + 12;

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
        card->setCardSelected(row.id == selectedChordSetId);
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
    selectedChordSetId = id;
    juce::String name = "Browse";
    for (const auto& r : processor.getTheoryEngine().getChordSetListings())
        if (r.id == id)
        {
            name = r.name.c_str();
            break;
        }
    processor.setPresetDisplayName(name);
    if (onPresetOrSelectionChanged)
        onPresetOrSelectionChanged();

    for (auto* pc : activePresetCards)
        pc->setCardSelected(pc->getChordSetId() == id);
    repaint();
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
