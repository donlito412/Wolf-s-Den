/**
 * CompositionPage.cpp — Wolf's Den
 *
 * Redesigned layout (TASK_011):
 *   · No vertical sidebar — genre/mood as horizontal pill rows.
 *   · Chord names on every pad ("Am7", "Cmaj7") instead of numbers.
 *   · MIDI keyboard (C3-B3) maps to audition pads so you can play them live.
 *   · Drag from audition pad → composition slot (or click pad to auto-fill).
 *   · OUT VOL knob always visible, bottom-right corner.
 */

#include "CompositionPage.h"
#include "../../PluginProcessor.h"
#include "../../engine/TheoryEngine.h"
#include "../theme/WolfsDenLookAndFeel.h"
#include <algorithm>
#include <cstring>

namespace wolfsden::ui
{

// =============================================================================
// Internal helpers
// =============================================================================
namespace
{
    /** Style a slider as a compact rotary knob. */
    void styleKnob(juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        WolfsDenLookAndFeel::configureRotarySlider(s);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 16);
        s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accentAlt());
        s.setColour(juce::Slider::thumbColourId,            Theme::accentHot());
    }

    /** Style a pill-shaped filter button. Active = accent fill. */
    void stylePill(juce::TextButton& b, bool active)
    {
        b.setColour(juce::TextButton::buttonColourId,
                    active ? Theme::accentPrimary() : Theme::panelSurface());
        b.setColour(juce::TextButton::textColourOffId,
                    active ? Theme::textPrimary() : Theme::textSecondary());
        b.setColour(juce::TextButton::textColourOnId, Theme::textPrimary());
    }

    /** Mood → accent colour, matching design spec. */
    juce::Colour moodAccent(const juce::String& mood)
    {
        if (mood == "Dark")       return juce::Colour(0xff38bdf8);
        if (mood == "Bright")     return juce::Colour(0xffeab308);
        if (mood == "Tense")      return Theme::error();
        if (mood == "Dreamy")     return Theme::accentPrimary();
        if (mood == "Aggressive") return juce::Colour(0xffff6b35);
        if (mood == "Calm")       return juce::Colour(0xff4ade80);
        return Theme::accentAlt();
    }

    /** Root pitch-class → note letter, e.g. 0→"C", 1→"C#", 9→"A". */
    const char* rootName(int pc)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        return names[pc % 12];
    }

    // (no DnD prefix needed — using MouseListener drag tracking)
}

// =============================================================================
// BrowseCard — inner struct defined in .cpp
// =============================================================================
struct CompositionPage::BrowseCard : public juce::Component
{
    BrowseCard(CompositionPage& ownerIn, const ProgressionListing& row)
        : owner(ownerIn),
          progressionId(row.id),
          name(row.name.c_str()),
          genre(row.genre.c_str()),
          mood(row.mood.c_str()),
          accent(moodAccent(mood))
    {}

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.f);

        // Card background
        g.setColour(selected ? Theme::panelSurface().brighter(0.08f) : Theme::panelSurface());
        g.fillRoundedRectangle(b, 6.f);

        // Left accent stripe
        g.setColour(accent);
        g.fillRoundedRectangle(b.removeFromLeft(5.f), 4.f);

        // Selection ring
        if (selected)
        {
            g.setColour(Theme::accentHot());
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 6.f, 1.5f);
        }

        // Text
        auto txt = b.reduced(8.f, 6.f);
        g.setColour(Theme::textPrimary());
        g.setFont(Theme::fontLabel().boldened());
        g.drawText(name, txt.removeFromTop(16.f), juce::Justification::centredLeft, true);

        g.setColour(Theme::textSecondary());
        g.setFont(Theme::fontLabel().withHeight(10.f));
        g.drawText(genre + "  ·  " + mood, txt, juce::Justification::centredLeft, true);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        owner.selectProgression(progressionId);
    }

    void setSelected(bool s) { selected = s; repaint(); }

    CompositionPage& owner;
    int              progressionId;
    juce::String     name, genre, mood;
    juce::Colour     accent;
    bool             selected = false;
};

// =============================================================================
// Constructor
// =============================================================================

CompositionPage::CompositionPage(WolfsDenAudioProcessor& proc)
    : processor(proc),
      apvts(proc.getAPVTS())
{
    // -------------------------------------------------------------------------
    // Browse grid (scrollable)
    // -------------------------------------------------------------------------
    addAndMakeVisible(gridViewport);
    gridViewport.setViewedComponent(&gridHolder, false);
    gridViewport.setScrollBarsShown(true, false);

    // Search box (top-right of filter row)
    searchBox.setTextToShowWhenEmpty("Search...", Theme::textDisabled());
    searchBox.onTextChange = [this]
    {
        rebuildFilteredIndices();
        rebuildBrowseGrid();
    };
    addAndMakeVisible(searchBox);

    // -------------------------------------------------------------------------
    // Audition pads — chord names populated in refreshAuditionPads()
    // Mouse-drag tracking handled via MouseListener (see mouseDrag/mouseUp)
    // -------------------------------------------------------------------------
    for (int i = 0; i < kMaxAuditionPads; ++i)
    {
        auto& p = audPads[(size_t)i];
        p.setButtonText("-");
        p.setVisible(true);
        p.setColour(juce::TextButton::buttonColourId,
                    Theme::panelSurface());
        p.setColour(juce::TextButton::textColourOffId, Theme::textSecondary());

        // Click: preview + auto-fill next free slot
        p.onClick = [this, i] { playAuditionPad(i); addAuditionPadToSlot(i); };

        // Register CompositionPage as MouseListener so we can detect drags
        p.addMouseListener(this, false);

        addAndMakeVisible(p);
    }

    // -------------------------------------------------------------------------
    // Composition slots
    // -------------------------------------------------------------------------
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& s = slots[(size_t)i];
        s.setButtonText("-");
        s.setColour(juce::TextButton::buttonColourId,
                    Theme::backgroundMid());
        s.setColour(juce::TextButton::buttonOnColourId,
                    Theme::accentPrimary().withAlpha(0.55f));
        s.setColour(juce::TextButton::textColourOffId, Theme::textDisabled());
        s.onClick = [this, i] { slotClicked(i); };
        addAndMakeVisible(s);
    }

    clearBtn.onClick = [this]
    {
        for (auto& sd : slotData) sd.reset();
        refreshSlotLabels();
    };
    addAndMakeVisible(clearBtn);

    keysBtn.setColour(juce::TextButton::buttonColourId,
                      Theme::accentHot().withAlpha(0.35f));
    keysBtn.onClick = [this]
    {
        processor.getMidiPipeline().setChordModeEnabled(true);
        processor.getMidiPipeline().setKeysLockMode(5);
    };
    addAndMakeVisible(keysBtn);

    captureBtn.onClick = [this]
    {
        if (processor.isMidiCaptureActive())
            processor.stopMidiCaptureAndSave();
        else
            processor.startMidiCapture();
    };
    addAndMakeVisible(captureBtn);

    // -------------------------------------------------------------------------
    // Perform bar
    // -------------------------------------------------------------------------
    setupArpControls();

    addAndMakeVisible(masterVol);
    styleKnob(masterVol);
    attVol = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "master_volume", masterVol);

    // ---- Labels for all perform-bar controls ----
    auto setupLabel = [this](juce::Label& lbl)
    {
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour(juce::Label::textColourId, Theme::textSecondary());
        lbl.setFont(Theme::fontLabel().withHeight(10.f));
        addAndMakeVisible(lbl);
    };
    setupLabel(lblVol);
    setupLabel(lblOct);
    setupLabel(lblSwing);
    setupLabel(lblRate);
    setupLabel(lblPat);

    // --- Debug label (temporary, shows DB progression count) ---
    dbgLabel.setFont(juce::Font(11.f));
    dbgLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff8800));
    dbgLabel.setText("DB: ...", juce::dontSendNotification);
    addAndMakeVisible(dbgLabel);

    // -------------------------------------------------------------------------
    // MIDI keyboard listener — maps note-ons to audition pads
    // -------------------------------------------------------------------------
    processor.getMidiKeyboardState().addListener(this);

    startTimerHz(10);
}

CompositionPage::~CompositionPage()
{
    processor.getMidiKeyboardState().removeListener(this);
    stopTimer();
}

// =============================================================================
// MIDI keyboard listener
// =============================================================================

void CompositionPage::handleNoteOn(juce::MidiKeyboardState*, int channel,
                                    int note, float /*vel*/)
{
    // Ignore channel 16 — that is our own internal preview channel.
    // Without this guard, startPreview() would re-trigger itself in a loop
    // and allNotesOff() would silence every real note the user plays.
    if (channel == 16) return;

    // Only respond when this page is visible and a chord set is selected
    if (!isVisible() || selectedProgressionId < 0) return;

    // Map C3(48)..B3(59) → pads 0..11
    const int kBase = 48;
    const int kTop  = kBase + kMaxAuditionPads - 1;
    if (note < kBase || note > kTop) return;

    const int padIdx = note - kBase;
    if (padIdx < (int)currentChordSequence.size())
    {
        // Trigger on the message thread (we're already there via MidiKeyboardState)
        juce::MessageManager::callAsync([this, padIdx]
        {
            playAuditionPad(padIdx);
        });
    }
}

void CompositionPage::handleNoteOff(juce::MidiKeyboardState*, int channel,
                                     int /*note*/, float /*vel*/)
{
    if (channel == 16) return; // ignore our own preview channel
    // Real note-offs from user keyboard — nothing to do here for audition pads.
}

// =============================================================================
// Filter pills
// =============================================================================

void CompositionPage::buildFilterPills()
{
    if (pillsBuilt) return;
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady()) return;

    // We will query ALL progressions first to gather genres/moods
    cachedProgressions = th.getProgressionListings("");

    // Collect unique genres and moods from the DB
    juce::StringArray genres, moods;
    genres.add("All");
    moods.add("All");
    for (auto& row : cachedProgressions)
    {
        juce::String g(row.genre.c_str()), m(row.mood.c_str());
        if (!genres.contains(g)) genres.add(g);
        if (!moods.contains(m))  moods.add(m);
    }

    for (auto& g : genres)
    {
        auto* btn = genrePills.add(std::make_unique<juce::TextButton>(g));
        stylePill(*btn, g == activeGenre);
        btn->onClick = [this, g] { applyGenreFilter(g); };
        addAndMakeVisible(btn);
    }
    for (auto& m : moods)
    {
        auto* btn = moodPills.add(std::make_unique<juce::TextButton>(m));
        stylePill(*btn, m == activeMood);
        btn->onClick = [this, m] { applyMoodFilter(m); };
        addAndMakeVisible(btn);
    }

    pillsBuilt = true;
    resized();             // lay out viewport bounds FIRST
    rebuildFilteredIndices();
    rebuildBrowseGrid();   // NOW viewport.getViewWidth() is valid
}

void CompositionPage::applyGenreFilter(const juce::String& genre)
{
    DBG("CompositionPage::applyGenreFilter - Selecting genre: " << genre);
    activeGenre = genre;
    for (auto* p : genrePills)
        stylePill(*p, p->getButtonText() == genre);

    // Update the cached progressions from DB filtered by genre (for efficiency)
    auto& th = processor.getTheoryEngine();
    if (th.isDatabaseReady()) {
        cachedProgressions = th.getProgressionListings(activeGenre.toStdString());
        DBG("CompositionPage::applyGenreFilter - DB returned " << cachedProgressions.size() << " progressions for genre " << genre);
    }

    rebuildFilteredIndices();
    rebuildBrowseGrid();
}

void CompositionPage::applyMoodFilter(const juce::String& mood)
{
    activeMood = mood;
    for (auto* p : moodPills)
        stylePill(*p, p->getButtonText() == mood);
    rebuildFilteredIndices();
    rebuildBrowseGrid();
}

void CompositionPage::rebuildFilteredIndices()
{
    filteredIndices.clear();
    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady()) return;
    for (int i = 0; i < (int)cachedProgressions.size(); ++i)
        if (rowMatchesFilters(cachedProgressions[(size_t)i]))
            filteredIndices.push_back(i);
}

bool CompositionPage::rowMatchesFilters(const ProgressionListing& row) const
{
    // Genre is mostly handled by the SQL query now via cachedProgressions, but keep check for safety
    if (activeGenre != "All" && juce::String(row.genre.c_str()) != activeGenre)
        return false;
    if (activeMood != "All" && juce::String(row.mood.c_str()) != activeMood)
        return false;
    juce::String q = searchBox.getText().trim();
    if (q.isNotEmpty())
    {
        if (!juce::String(row.name.c_str()).containsIgnoreCase(q) &&
            !juce::String(row.genre.c_str()).containsIgnoreCase(q) &&
            !juce::String(row.mood.c_str()).containsIgnoreCase(q))
            return false;
    }
    return true;
}

// =============================================================================
// Browse grid
// =============================================================================

void CompositionPage::rebuildBrowseGrid()
{
    gridHolder.deleteAllChildren();
    browseCards.clear();

    auto& th = processor.getTheoryEngine();
    if (!th.isDatabaseReady()) return;

    
    const int vw   = juce::jmax(1, gridViewport.getViewWidth());
    const int cols = juce::jmax(1, vw / 200);
    const int cw   = (vw - 8 * (cols + 1)) / cols;
    const int ch   = 60;
    const int gap  = 8;

    int row = 0, col = 0;
    for (int idx : filteredIndices)
    {
        auto* card = new BrowseCard(*this, cachedProgressions[(size_t)idx]);
        gridHolder.addAndMakeVisible(card);
        browseCards.push_back(card);
        card->setBounds(gap + col * (cw + gap),
                        gap + row * (ch + gap),
                        cw, ch);
        card->setSelected(cachedProgressions[(size_t)idx].id == selectedProgressionId);
        if (++col >= cols) { col = 0; ++row; }
    }
    gridHolder.setSize(vw, (row + 1) * (ch + gap) + gap);
}

void CompositionPage::selectProgression(int id)
{
    selectedProgressionId = id;
    for (auto* c : browseCards)
        c->setSelected(c->progressionId == id);

    // Search the in-memory cache first (fast path)
    currentChordSequence.clear();
    currentRootSequence.clear();
    for (const auto& p : cachedProgressions) {
        if (p.id == id) {
            currentChordSequence = p.chordSequence;
            currentRootSequence  = p.rootSequence;
            currentRootKey       = p.rootKey;
            break;
        }
    }

    DBG("CompositionPage selectProgression: id=" << id << " cache_size=" << (int)cachedProgressions.size());
    DBG("CompositionPage selectProgression: after cache, seq_size=" << (int)currentChordSequence.size());

    // Fallback: if cache lookup failed or returned empty sequence, query DB directly.
    // This catches stale-cache scenarios (e.g. DB was reseeded between page loads).
    if (currentChordSequence.empty())
    {
        auto& th = processor.getTheoryEngine();
        if (th.isDatabaseReady())
        {
            for (const auto& p : th.getProgressionListings(""))
            {
                if (p.id == id)
                {
                    currentChordSequence = p.chordSequence;
                    currentRootSequence  = p.rootSequence;
                    currentRootKey       = p.rootKey;
                    // Refresh cache too so subsequent interactions are fast
                    cachedProgressions   = th.getProgressionListings(activeGenre.toStdString());
                    break;
                }
            }
        }
    }

    DBG("CompositionPage selectProgression: final seq_size=" << (int)currentChordSequence.size());

    refreshAuditionPads();
}

// =============================================================================
// Audition pads
// =============================================================================

juce::String CompositionPage::chordName(int rootPc, int chordId) const
{
    const char* root = rootName(rootPc);
    auto& defs = processor.getTheoryEngine().getChordDefinitions();
    for (auto& d : defs)
    {
        if (d.id == chordId)
            return juce::String(root) + juce::String(d.symbol.c_str());
    }
    return juce::String(root);
}

void CompositionPage::refreshAuditionPads()
{
    for (int i = 0; i < kMaxAuditionPads; ++i)
    {
        auto& p = audPads[(size_t)i];
        if (i < (int)currentChordSequence.size())
        {
            int chordId = currentChordSequence[(size_t)i];

            // Reconstruct the root note for this chord.
            // (For this progression library, we'll assume the root is just the chord's base interval
            // relative to the progression's root key. A more advanced library might store the exact root.)
            // Here, we look up the first interval of the chord (which is 0) + rootKey.
            // To make it more musical without exact root offsets stored, we'll just use rootKey for now,
            // or if the library stores them differently we'd need that.
            // Since the seed SQL only stores chord_ids, we map them directly.
            // We will just use the rootKey for now to get it displaying something, but ideally
            // the progression should store the roots.

            // Use per-chord root offset from rootSequence, fall back to progression root if missing
            int chordRootOffset = (i < (int)currentRootSequence.size()) ? currentRootSequence[(size_t)i] : 0;
            int rootPc = (currentRootKey + chordRootOffset) % 12;

            p.setButtonText(chordName(rootPc, chordId));
            p.setColour(juce::TextButton::textColourOffId, Theme::textPrimary());
            p.setVisible(true);
        }
        else
        {
            p.setButtonText("-");
            p.setColour(juce::TextButton::textColourOffId, Theme::textDisabled());
            p.setVisible(true); // keep visible but greyed out
        }
    }
}

void CompositionPage::playAuditionPad(int idx)
{
    if (idx < 0 || idx >= (int)currentChordSequence.size()) return;
    int chordId = currentChordSequence[(size_t)idx];

    auto& defs = processor.getTheoryEngine().getChordDefinitions();
    std::vector<int> iv { 0, 4, 7 };
    for (auto& d : defs)
        if (d.id == chordId) { iv = d.intervals; break; }

    int chordRootOffset = (idx < (int)currentRootSequence.size()) ? currentRootSequence[(size_t)idx] : 0;
    int midiRoot = juce::jlimit(24, 96, 48 + currentRootKey + chordRootOffset);
    startPreview(midiRoot, iv);
}

void CompositionPage::addAuditionPadToSlot(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= (int)currentChordSequence.size()) return;
    int chordId = currentChordSequence[(size_t)entryIndex];
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (!slotData[(size_t)i].has_value())
        {
            int chordRootOffset = (entryIndex < (int)currentRootSequence.size()) ? currentRootSequence[(size_t)entryIndex] : 0;
            slotData[(size_t)i] = { chordId, (currentRootKey + chordRootOffset) % 12 };
            refreshSlotLabels();
            return;
        }
    }
}

// =============================================================================
// Composition slots
// =============================================================================

void CompositionPage::refreshSlotLabels()
{
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& s = slots[(size_t)i];
        if (slotData[(size_t)i].has_value())
        {
            auto& d = *slotData[(size_t)i];
            s.setButtonText(chordName(d.rootPc, d.chordId));
            s.setColour(juce::TextButton::textColourOffId, Theme::textPrimary());
        }
        else
        {
            s.setButtonText("-");
            s.setColour(juce::TextButton::textColourOffId, Theme::textDisabled());
        }
    }
}

void CompositionPage::slotClicked(int idx)
{
    if (!slotData[(size_t)idx].has_value()) return;
    auto& d    = *slotData[(size_t)idx];
    auto& defs = processor.getTheoryEngine().getChordDefinitions();
    std::vector<int> iv { 0, 4, 7 };
    for (auto& def : defs)
        if (def.id == d.chordId) { iv = def.intervals; break; }
    startPreview(48 + d.rootPc, iv);
}

// =============================================================================
// MouseListener — drag from audition pad → composition slot
// =============================================================================

void CompositionPage::mouseDrag(const juce::MouseEvent& e)
{
    // Identify which audition pad the drag originated from
    int srcPad = -1;
    for (int i = 0; i < kMaxAuditionPads; ++i)
    {
        if (e.eventComponent == &audPads[(size_t)i])
        {
            srcPad = i;
            break;
        }
    }
    if (srcPad < 0 || srcPad >= (int)currentChordSequence.size()) return;

    dragSourcePad = srcPad;

    // Convert drag position to our coordinate space and find slot under cursor
    auto localPt = e.getEventRelativeTo(this).getPosition();
    int newHighlight = -1;
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (slots[(size_t)i].getBounds().contains(localPt))
        {
            newHighlight = i;
            break;
        }
    }

    // Update highlight colour
    if (newHighlight != dropHighlightSlot)
    {
        if (dropHighlightSlot >= 0)
            slots[(size_t)dropHighlightSlot].setColour(
                juce::TextButton::buttonColourId, Theme::backgroundMid());
        if (newHighlight >= 0)
            slots[(size_t)newHighlight].setColour(
                juce::TextButton::buttonColourId,
                Theme::accentPrimary().withAlpha(0.4f));
        dropHighlightSlot = newHighlight;
    }
}

void CompositionPage::mouseUp(const juce::MouseEvent& e)
{
    // Only act if we were tracking a drag from an audition pad
    if (dragSourcePad < 0) return;
    const int srcPad = dragSourcePad;
    dragSourcePad = -1;

    // Clear slot highlight
    if (dropHighlightSlot >= 0)
    {
        slots[(size_t)dropHighlightSlot].setColour(
            juce::TextButton::buttonColourId, Theme::backgroundMid());
    }

    if (srcPad >= (int)currentChordSequence.size()) return;
    int chordId = currentChordSequence[(size_t)srcPad];

    // If released over a slot, place chord there; else fill first free slot
    if (dropHighlightSlot >= 0)
    {
        slotData[(size_t)dropHighlightSlot] = { chordId, currentRootKey };
        refreshSlotLabels();
    }
    else
    {
        // Only auto-fill if the mouse-up is over the slots area (not a spurious release)
        auto localPt = e.getEventRelativeTo(this).getPosition();
        for (int i = 0; i < kNumSlots; ++i)
        {
            if (slots[(size_t)i].getBounds().contains(localPt))
            {
                slotData[(size_t)i] = { chordId, currentRootKey };
                refreshSlotLabels();
                return;
            }
        }
    }

    dropHighlightSlot = -1;
}

// =============================================================================
// Perform / Arp controls
// =============================================================================

void CompositionPage::setupArpControls()
{
    for (auto* b : { (juce::Button*)&chordOn,
                     (juce::Button*)&arpOn,
                     (juce::Button*)&arpSync,
                     (juce::Button*)&arpLatch })
        addAndMakeVisible(b);

    for (auto* s : { &arpOct, &arpSwing })
    {
        addAndMakeVisible(s);
        styleKnob(*s);
    }

    // Arp rate is an AudioParameterChoice — use ComboBox (grouped by note value)
    {
        juce::StringArray rateLabels { "1/1", "1/1D", "1/1T",
                                       "1/2", "1/2D", "1/2T",
                                       "1/4", "1/4D", "1/4T",
                                       "1/8", "1/8D", "1/8T",
                                       "1/16", "1/16D", "1/16T",
                                       "1/32", "1/32D", "1/32T",
                                       "1/64", "1/64D", "1/64T",
                                       "1/128", "1/128D", "1/128T" };
        for (auto& label : rateLabels)
            arpRate.addItem(label, arpRate.getNumItems() + 1);

        // Add section headings to visually group and limit visible items
        auto* popup = arpRate.getRootMenu();
        if (popup != nullptr)
        {
            // Rebuild as grouped submenus so the popup doesn't get too tall
            popup->clear();
            const char* groups[] = { "Whole", "Half", "Quarter", "Eighth",
                                     "16th", "32nd", "64th", "128th" };
            for (int g = 0; g < 8; ++g)
            {
                juce::PopupMenu sub;
                for (int v = 0; v < 3; ++v)
                {
                    int idx = g * 3 + v;
                    sub.addItem(idx + 1, rateLabels[idx]);
                }
                popup->addSubMenu(juce::String(groups[g]), sub);
            }
        }
    }
    addAndMakeVisible(arpRate);

    for (auto& label : juce::StringArray({ "Up", "Down", "Up-Down", "Order", "Chord", "Random" }))
        arpPattern.addItem(label, arpPattern.getNumItems() + 1);
    addAndMakeVisible(arpPattern);

    attChordOn = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "midi_chord_mode",   chordOn);
    attArpOn  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "midi_arp_on",       arpOn);
    attSync   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "midi_arp_sync_ppq", arpSync);
    attLatch  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "midi_arp_latch",    arpLatch);
    attArpRate = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "midi_arp_rate",     arpRate);
    attArpPat  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "midi_arp_pattern",  arpPattern);
    attArpOct  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "midi_arp_octaves",  arpOct);
    attArpSwing = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, "midi_arp_swing",    arpSwing);

    // Clean value display (APVTS attachments override Slider defaults — set after binding)
    masterVol.textFromValueFunction = [](double v) -> juce::String {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    masterVol.updateText();

    arpOct.textFromValueFunction = [](double v) -> juce::String {
        return juce::String(juce::roundToInt(v));
    };
    arpOct.updateText();

    arpSwing.textFromValueFunction = [](double v) -> juce::String {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    arpSwing.updateText();
}

// =============================================================================
// Preview
// =============================================================================

void CompositionPage::startPreview(int root, const std::vector<int>& iv)
{
    stopPreview();
    int n = 0;
    for (int interval : iv)
    {
        int note = juce::jlimit(0, 127, root + interval);
        processor.getMidiKeyboardState().noteOn(16, note, 0.8f);
        if (n < kMaxPreviewNotes) previewNotes_[(size_t)n++] = note;
    }
    previewTicksLeft_ = 15; // ~1.5 seconds at 10 Hz
}

void CompositionPage::stopPreview()
{
    processor.getMidiKeyboardState().allNotesOff(16);
    previewTicksLeft_ = 0;
}

// =============================================================================
// Timer
// =============================================================================

void CompositionPage::timerCallback()
{
    // Retry pill/grid building if the DB wasn't ready on first visibility
    if (!pillsBuilt)
        buildFilterPills();

    if (previewTicksLeft_ > 0)
        if (--previewTicksLeft_ == 0) stopPreview();

    // Update MIDI Capture button label
    captureBtn.setButtonText(processor.isMidiCaptureActive()
                             ? "* STOP CAPTURE"
                             : "* MIDI CAPTURE");

    // Debug: update progression count label from live DB
    auto& th = processor.getTheoryEngine();
    if (th.isDatabaseReady())
    {
        auto all = th.getProgressionListings("");
        dbgLabel.setText("DB: " + juce::String((int)all.size()), juce::dontSendNotification);
    }
}

// =============================================================================
// Visibility
// =============================================================================

void CompositionPage::visibilityChanged()
{
    if (isVisible())
        buildFilterPills();
    else
        stopPreview();
}

// =============================================================================
// paint()
// =============================================================================

void CompositionPage::paint(juce::Graphics& g)
{
    g.fillAll(Theme::backgroundDark());

    auto b = getLocalBounds().reduced(8);
    const int px = b.getX(), pw = b.getWidth(), H = b.getHeight();

    // ---- Divider lines — mirror the fixed heights from resized() ----
    constexpr int kFilterH = 48, kGap = 4, kAudH = 34, kCompHdr = 22, kSlotRows = 54;
    const int fixedUsed = kFilterH + kGap + kAudH + kGap + kCompHdr + kSlotRows + kGap + kGap;
    const int kPerfH    = juce::jmax(70, H - fixedUsed - juce::jmax(120, (int)(H * 0.28f)));
    const int gridH     = juce::jmax(120, H - fixedUsed - kPerfH);

    auto drawDiv = [&](int y)
    {
        g.setColour(Theme::panelSurface().brighter(0.08f));
        g.drawHorizontalLine(y, (float)px, (float)(px + pw));
    };

    int y = b.getY() + kFilterH + kGap;
    drawDiv(y);
    y += gridH + kGap;
    drawDiv(y);
    y += kAudH + kGap;
    drawDiv(y);
    y += kCompHdr + kSlotRows + kGap;
    drawDiv(y);

    // ---- Section labels ----
    g.setFont(Theme::fontLabel().withHeight(10.f).boldened());

    // "BROWSE" label top-right
    g.setColour(Theme::textSecondary());
    g.drawText("BROWSE PROGRESSIONS",
               b.getRight() - 160, b.getY() + 2, 160, 12,
               juce::Justification::right, true);

    // "AUDITION" label — shows MIDI hint when a set is selected
    int audY = b.getY() + kFilterH + kGap + gridH + kGap;
    if (selectedProgressionId >= 0)
    {
        g.setColour(Theme::accentAlt());
        g.drawText("AUDITION - MIDI: C3-B3",
                   px + 2, audY + 2, 220, 12,
                   juce::Justification::left, true);
    }

    // "YOUR PROGRESSION" label
    int compY = audY + kAudH + kGap;
    g.setColour(Theme::textSecondary());
    g.drawText("YOUR PROGRESSION",
               px + 2, compY + 2, 180, 12,
               juce::Justification::left, true);
}

// =============================================================================
// resized()
// All heights are fixed so the page fits at 800×560 minimum.
// Layout (top → bottom):
//   Filter row  : 48px  (genre pills row 22px + gap 4 + mood pills row 22px)
//   Gap         :  4px
//   Browse grid : flexible (~28% of total, min 120px)
//   Gap         :  4px
//   Audition    : 34px
//   Gap         :  4px
//   Comp header : 22px
//   Comp slots  : 54px  (2 rows of ~25px)
//   Gap         :  4px
//   Perform bar : remaining (≥ 70px for vol knob + arp)
// =============================================================================

void CompositionPage::resized()
{
    auto area = getLocalBounds().reduced(8);
    const int W = area.getWidth();
    const int H = area.getHeight();

    // ---- Fixed section heights ----
    constexpr int kFilterH  = 48;  // 2 pill rows × 22 + gap 4
    constexpr int kAudH     = 34;
    constexpr int kCompHdr  = 22;
    constexpr int kSlotRows = 54;  // 2 rows
    constexpr int kGap      = 4;
    // Grid takes whatever is left after all fixed sections
    const int fixedUsed = kFilterH + kGap + kAudH + kGap + kCompHdr + kSlotRows + kGap + kGap;
    const int kVolH     = 74;      // vol knob + label
    const int kArpTogH  = 24;
    const int kArpKnobH = juce::jmax(60, H - fixedUsed - kVolH - kArpTogH - kGap);
    const int kPerfH    = kArpTogH + kArpKnobH + kGap;
    const int gridH     = juce::jmax(120, H - fixedUsed - kPerfH);

    // ---- FILTER ROW ----
    {
        // Row 1: genre pills left, search box right
        auto row1 = area.removeFromTop(22);
        const int searchW = juce::jmin(140, W / 5);
        searchBox.setBounds(row1.removeFromRight(searchW).reduced(0, 1));
        if (!genrePills.isEmpty())
        {
            const int n    = genrePills.size();
            const int pillW = juce::jmin(62, (row1.getWidth() - kGap * (n - 1)) / n);
            for (auto* p : genrePills)
            {
                p->setBounds(row1.removeFromLeft(pillW).reduced(0, 1));
                row1.removeFromLeft(kGap);
            }
        }

        area.removeFromTop(kGap);

        // Row 2: mood pills
        auto row2 = area.removeFromTop(22);
        if (!moodPills.isEmpty())
        {
            const int n    = moodPills.size();
            const int pillW = juce::jmin(62, (row2.getWidth() - kGap * (n - 1)) / n);
            for (auto* p : moodPills)
            {
                p->setBounds(row2.removeFromLeft(pillW).reduced(0, 1));
                row2.removeFromLeft(kGap);
            }
        }
    }
    area.removeFromTop(kGap);

    // ---- BROWSE GRID ----
    gridViewport.setBounds(area.removeFromTop(gridH));
    area.removeFromTop(kGap);

    // ---- AUDITION ROW ----
    {
        auto audArea = area.removeFromTop(kAudH);
        const int n  = kMaxAuditionPads;
        const int pw = (audArea.getWidth() - kGap * (n - 1)) / n;
        for (int i = 0; i < n; ++i)
            audPads[(size_t)i].setBounds(audArea.getX() + i * (pw + kGap),
                                         audArea.getY(), pw, audArea.getHeight());
    }
    area.removeFromTop(kGap);

    // ---- COMPOSITION SLOTS ----
    {
        // Header buttons
        auto hdr = area.removeFromTop(kCompHdr);
        captureBtn.setBounds(hdr.removeFromRight(130).reduced(2, 1));
        keysBtn   .setBounds(hdr.removeFromRight(100).reduced(2, 1));
        clearBtn  .setBounds(hdr.removeFromRight(80) .reduced(2, 1));

        // 16 slots — 2 rows of 8
        auto slotsArea = area.removeFromTop(kSlotRows);
        const int cols = 8, rows = 2;
        const int sw = (slotsArea.getWidth()  - kGap * (cols - 1)) / cols;
        const int sh = (slotsArea.getHeight() - kGap * (rows - 1)) / rows;
        for (int i = 0; i < kNumSlots; ++i)
        {
            int r = i / cols, c = i % cols;
            slots[(size_t)i].setBounds(slotsArea.getX() + c * (sw + kGap),
                                       slotsArea.getY() + r * (sh + kGap),
                                       sw, sh);
        }
    }
    area.removeFromTop(kGap);

    // ---- PERFORM BAR ----
    {
        auto bot = area;

        // OUT VOL — right column, full perform height
        auto volCol = bot.removeFromRight(82);
        masterVol.setBounds(volCol.removeFromTop(kVolH - 16));
        lblVol   .setBounds(volCol.removeFromTop(16));

        // Toggle row (chord + arp controls)
        auto togRow = bot.removeFromTop(kArpTogH);
        chordOn .setBounds(togRow.removeFromLeft(60).reduced(1, 2));
        arpOn   .setBounds(togRow.removeFromLeft(52).reduced(1, 2));
        arpSync .setBounds(togRow.removeFromLeft(52).reduced(1, 2));
        arpLatch.setBounds(togRow.removeFromLeft(52).reduced(1, 2));

        // Arp knobs + pattern — each column gets a label above the control
        const int kw        = bot.getWidth() / 4;
        constexpr int kLblH = 14;
        constexpr int kComboH = 26;

        // Rate column
        {
            auto col = bot.removeFromLeft(kw).reduced(3, 0);
            lblRate .setBounds(col.removeFromTop(kLblH));
            arpRate .setBounds(col.removeFromTop(kComboH));
        }
        // Pattern column
        {
            auto col = bot.removeFromLeft(kw).reduced(3, 0);
            lblPat  .setBounds(col.removeFromTop(kLblH));
            arpPattern.setBounds(col.removeFromTop(kComboH));
        }
        // Octaves column
        {
            auto col = bot.removeFromLeft(kw).reduced(3, 0);
            lblOct  .setBounds(col.removeFromTop(kLblH));
            arpOct  .setBounds(col);
        }
        // Swing column
        {
            auto col = bot.reduced(3, 0);
            lblSwing.setBounds(col.removeFromTop(kLblH));
            arpSwing.setBounds(col);
        }
    }

    // Debug label: top-right corner, small
    dbgLabel.setBounds(getWidth() - 90, 4, 86, 16);
}

} // namespace wolfsden::ui
