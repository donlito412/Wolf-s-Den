#pragma once

#include "../theme/UITheme.h"

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_set>
#include <vector>

class WolfsDenAudioProcessor;

namespace wolfsden
{
struct ChordSetListing;
}

namespace wolfsden::ui
{

class BrowsePresetCard;

/** TASK_009 BROWSE: 30% filter sidebar, 70% preset cards, bottom detail strip. */
class BrowsePage : public juce::Component
{
public:
    explicit BrowsePage(WolfsDenAudioProcessor& proc);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    /** Fires when selection or play updates the processor preset display name. */
    std::function<void()> onPresetOrSelectionChanged;

private:
    friend class BrowsePresetCard;
    void selectChordSetForCard(int chordSetId);
    void toggleFavouriteForCard(int chordSetId);
    void playPreviewForCard(int chordSetId);
    bool isChordSetFavourite(int chordSetId) const;

    void ensureFiltersBuilt();
    void layoutSidebarContent();
    int computeSidebarContentHeight(int width) const;
    void rebuildFilteredIndices();
    void rebuildCardGrid();
    void selectChordSet(int id);
    void toggleFavourite(int id);
    void playPreview(int id);

    juce::String scaleNameForId(int scaleId) const;
    static juce::Colour moodAccentColour(const juce::String& mood);
    bool rowMatchesFilters(const wolfsden::ChordSetListing& row) const;

    WolfsDenAudioProcessor& processor;

    juce::TextEditor searchBox;
    juce::Component sidebarInner;
    juce::Viewport sidebarViewport;
    juce::Label labelGenre { {}, "Genre" };
    juce::Label labelMood { {}, "Mood" };
    juce::Label labelEnergy { {}, "Energy" };
    juce::Label labelScale { {}, "Scale filter" };

    juce::OwnedArray<juce::TextButton> genrePills;
    juce::OwnedArray<juce::TextButton> moodPills;
    juce::TextButton energyLow { "Low" };
    juce::TextButton energyMed { "Med" };
    juce::TextButton energyHigh { "High" };
    juce::ComboBox scaleFilter;

    juce::Component cardHolder;
    juce::Viewport cardViewport;

    int energyFilter = -1;
    int selectedChordSetId = -1;
    std::unordered_set<int> favouriteIds;
    std::vector<int> filteredRowIndices;

    bool filtersBuilt = false;
    int lastGridW = -1;

    std::vector<BrowsePresetCard*> activePresetCards;

    static constexpr int kComboAllScales = 1;
    static constexpr int kScaleComboBase = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowsePage)
};

} // namespace wolfsden::ui
