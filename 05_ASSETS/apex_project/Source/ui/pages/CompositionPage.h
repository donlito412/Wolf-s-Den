#pragma once

#include "../theme/UITheme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class WolfsDenAudioProcessor;

namespace wolfsden
{
struct ChordSetListing;
struct ChordSetEntry;
struct ChordDefinition;
struct ProgressionListing;
}

namespace wolfsden::ui
{

/**
 * CompositionPage — single unified page for chords, progressions, and arp.
 *
 * Scaler-style workflow (top → bottom):
 *   1. FILTER ROW    — Horizontal genre pill row + mood pill row (no sidebar).
 *   2. BROWSE GRID   — Scrollable progression cards (4 cols).
 *   3. AUDITION ROW  — Pads showing chord names for selected progression.
 *                      Playable by mouse click AND by MIDI keyboard (C3-B3).
 *   4. COMPOSITION   — 16 named slots. Drag from audition row or click to fill.
 *   5. PERFORM BAR   — Arpeggiator controls + prominent OUT VOL knob.
 */
class CompositionPage : public juce::Component,
                        private juce::Timer,
                        private juce::MidiKeyboardState::Listener
{
public:
    explicit CompositionPage(WolfsDenAudioProcessor& proc);
    ~CompositionPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    // =========================================================================
    // Timer / MIDI keyboard listener
    // =========================================================================
    void timerCallback() override;
    /** Maps MIDI note-ons C3(48)..B3(59) to audition pads 0..11. */
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float vel) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float vel) override;

    // =========================================================================
    // Filter row
    // =========================================================================
    void buildFilterPills();
    void applyGenreFilter(const juce::String& genre);
    void applyMoodFilter(const juce::String& mood);
    void rebuildFilteredIndices();
    bool rowMatchesFilters(const wolfsden::ProgressionListing& row) const;

    // =========================================================================
    // Browse grid
    // =========================================================================
    void rebuildBrowseGrid();
    void selectProgression(int progressionId);

    // =========================================================================
    // Audition pads
    // =========================================================================
    void refreshAuditionPads();
    void playAuditionPad(int padIndex);
    void addAuditionPadToSlot(int entryIndex);

    /** Returns a short chord name string, e.g. "Am7", "Cmaj7", "G". */
    juce::String chordName(int rootPc, int chordId) const;

    // =========================================================================
    // Composition slots
    // =========================================================================
    void refreshSlotLabels();
    void slotClicked(int slotIndex);

    // =========================================================================
    // Perform / Arp setup
    // =========================================================================
    void setupArpControls();

    // =========================================================================
    // Internal MIDI preview (channel 16, timed auto-release)
    // =========================================================================
    void startPreview(int rootMidi, const std::vector<int>& intervals);
    void stopPreview();

    // =========================================================================
    // MouseListener — tracks drag from audition pad → composition slot
    // =========================================================================
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // =========================================================================
    // Members
    // =========================================================================
    WolfsDenAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // --- Filter row (horizontal pills, no sidebar) ---
    juce::OwnedArray<juce::TextButton> genrePills;
    juce::OwnedArray<juce::TextButton> moodPills;
    juce::TextEditor searchBox;
    juce::String activeGenre { "All" };
    juce::String activeMood  { "All" };
    bool pillsBuilt = false;

    // --- Browse grid ---
    juce::Component gridHolder;
    juce::Viewport  gridViewport;

    struct BrowseCard;                     // defined in .cpp
    std::vector<BrowseCard*> browseCards;
    std::vector<int>         filteredIndices;
    int selectedProgressionId = -1;
    std::vector<wolfsden::ProgressionListing> cachedProgressions;

    // --- Audition row ---
    static constexpr int kMaxAuditionPads = 16;
    std::array<juce::TextButton, kMaxAuditionPads> audPads;
    std::vector<int> currentChordSequence;
    int currentRootKey = 0;

    // --- Drag state (audition pad → slot tracking via MouseListener) ---
    int dragSourcePad     = -1;   ///< audition pad index being dragged, or -1
    int dropHighlightSlot = -1;   ///< slot currently highlighted during drag

    // --- Composition slots ---
    static constexpr int kNumSlots = 16;
    std::array<juce::TextButton, kNumSlots> slots;
    struct SlotData { int chordId; int rootPc; };
    std::array<std::optional<SlotData>, kNumSlots> slotData;
    juce::TextButton clearBtn   { "CLEAR ALL" };
    juce::TextButton keysBtn    { "LINK TO KEYS" };
    juce::TextButton captureBtn { "* MIDI CAPTURE" };

    // --- Perform bar ---
    juce::ToggleButton chordOn  { "CHORD" };
    juce::ToggleButton arpOn    { "ARP" };
    juce::ToggleButton arpSync  { "SYNC" };
    juce::ToggleButton arpLatch { "LATCH" };
    juce::ComboBox arpRate;
    juce::Slider   arpOct, arpSwing;
    juce::ComboBox arpPattern;
    juce::Slider   masterVol;
    juce::Label    lblVol { {}, "OUT VOL" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attChordOn, attArpOn, attSync, attLatch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attArpOct, attArpSwing, attVol;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        attArpRate, attArpPat;

    // --- Preview state ---
    static constexpr int kMaxPreviewNotes = 12;
    std::array<int, kMaxPreviewNotes> previewNotes_ {};
    int previewTicksLeft_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompositionPage)
};

} // namespace wolfsden::ui