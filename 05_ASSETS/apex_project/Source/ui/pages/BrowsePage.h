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
struct PresetListing;
}

namespace wolfsden::ui
{

class BrowsePresetCard;
class BrowsePatchCard;

/** TASK_009 BROWSE: mode toggle (PATCHES | CHORD SETS), 30% sidebar, 70% cards, bottom detail. */
class BrowsePage : public juce::Component
{
public:
    explicit BrowsePage(WolfsDenAudioProcessor& proc);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    /** Fires when selection or play updates the processor preset display name. */
    std::function<void()> onPresetOrSelectionChanged;

    /** Keep card highlights in sync after TopBar < > cycles presets. */
    void syncSelectionFromProcessor();

    /** Switch to Patches mode and highlight the given preset id (called from TopBar). */
    void syncPresetSelectionFromProcessor();

    const std::vector<int>& getFilteredPresetIndices() const noexcept { return filteredPresetIndices; }

    enum class BrowseMode { Patches, ChordSets };
    BrowseMode getMode() const noexcept { return currentMode; }

private:
    // ------------------------------------------------------------------
    // Shared
    // ------------------------------------------------------------------
    void switchMode (BrowseMode newMode);
    void rebuildCardGrid();

    // ------------------------------------------------------------------
    // PATCHES mode
    // ------------------------------------------------------------------
    friend class BrowsePatchCard;
    void selectPresetForCard  (int presetId);
    void deletePresetForCard  (int presetId);
    bool isPresetFavourite    (int presetId) const;
    void togglePresetFavourite(int presetId);

    void ensurePatchFiltersBuilt();
    void layoutPatchSidebar();
    void rebuildFilteredPresetIndices();
    void rebuildPatchCardGrid();
    void onSavePresetClicked();

    bool presetRowMatchesFilters (const wolfsden::PresetListing& p) const;
    static juce::Colour categoryAccentColour (const juce::String& category);

    // ------------------------------------------------------------------
    // CHORD SETS mode (original behaviour)
    // ------------------------------------------------------------------
    friend class BrowsePresetCard;
    void selectChordSetForCard(int chordSetId);
    void toggleFavouriteForCard(int chordSetId);
    void playPreviewForCard(int chordSetId);
    bool isChordSetFavourite(int chordSetId) const;

    void ensureFiltersBuilt();
    void layoutSidebarContent();
    void rebuildFilteredIndices();

    juce::String scaleNameForId(int scaleId) const;
    static juce::Colour moodAccentColour(const juce::String& mood);
    bool rowMatchesFilters(const wolfsden::ChordSetListing& row) const;

    // ------------------------------------------------------------------
    // Members
    // ------------------------------------------------------------------
    WolfsDenAudioProcessor& processor;
    BrowseMode currentMode = BrowseMode::Patches;

    // Mode toggle strip
    juce::TextButton modePatchesBtn   { "PATCHES"    };
    juce::TextButton modeChordSetsBtn { "CHORD SETS" };

    // Shared card area
    juce::Component  cardHolder;
    juce::Viewport   cardViewport;
    int lastGridW = -1;

    // ---- PATCHES sidebar ----
    juce::Component  patchSidebarInner;
    juce::Viewport   patchSidebarViewport;
    juce::TextEditor patchSearchBox;
    juce::Label      patchLabelCategory { {}, "Category" };
    juce::Label      patchLabelPack     { {}, "Pack"     };
    juce::ComboBox   patchPackFilter;
    juce::OwnedArray<juce::TextButton> categoryPills;
    juce::TextButton savePresetBtn { "Save Preset" };

    std::vector<int> filteredPresetIndices;
    std::unordered_set<int> favouritePresetIds;
    std::vector<BrowsePatchCard*> activePatchCards;
    bool patchFiltersBuilt = false;
    /** Invalidate patch sidebar when preset row count changes (e.g. first DB seed after empty load). */
    size_t patchFilterPresetCount = (size_t) -1;

    // ---- CHORD SETS sidebar (original) ----
    juce::TextEditor searchBox;
    juce::Component  sidebarInner;
    juce::Viewport   sidebarViewport;
    juce::Label labelGenre { {}, "Genre" };
    juce::Label labelMood  { {}, "Mood"  };
    juce::Label labelEnergy{ {}, "Energy"};
    juce::Label labelScale { {}, "Scale filter" };

    juce::OwnedArray<juce::TextButton> genrePills;
    juce::OwnedArray<juce::TextButton> moodPills;
    juce::TextButton energyLow  { "Low"  };
    juce::TextButton energyMed  { "Med"  };
    juce::TextButton energyHigh { "High" };
    juce::ComboBox   scaleFilter;

    int energyFilter = -1;
    std::unordered_set<int> favouriteIds;
    std::vector<int> filteredRowIndices;
    bool filtersBuilt = false;

    std::vector<BrowsePresetCard*> activePresetCards;

    static constexpr int kComboAllScales = 1;
    static constexpr int kScaleComboBase = 1000;
    static constexpr int kComboAllPacks  = 1;
    static constexpr int kPackComboBase  = 2000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowsePage)
};

} // namespace wolfsden::ui
