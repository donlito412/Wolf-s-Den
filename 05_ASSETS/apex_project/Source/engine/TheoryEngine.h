#pragma once

// JUCE modules required:
//   juce_audio_basics, juce_audio_processors, juce_dsp (for FFT)
//   Add juce::juce_dsp to target_link_libraries in CMakeLists.txt

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Forward-declare sqlite3 so we don't pull it into every consuming header
struct sqlite3;

namespace wolfsden
{

// =============================================================================
// Data structures — loaded at init time, read-only thereafter
// =============================================================================

/** One chord definition loaded from the 'chords' SQLite table. */
struct ChordDefinition
{
    int id = 0;
    std::string name;            // "Major 7th"
    std::string symbol;          // "maj7"
    std::vector<int> intervals;  // semitone offsets from root, e.g. {0,4,7,11}
    std::string category;        // "triad" | "seventh" | "ninth" | "extended" | "sus"
    std::string quality;         // "major" | "minor" | "dominant" | "diminished" | "augmented"
};

/** One scale definition loaded from the 'scales' SQLite table. */
struct ScaleDefinition
{
    int id = 0;
    std::string name;            // "Dorian"
    std::vector<int> intervals;  // semitone offsets within one octave, e.g. {0,2,3,5,7,9,10}
    std::string modeFamily;      // "major" | "minor" | "pentatonic" | "exotic" | "chromatic"
};

/** One row from `chord_sets` for TASK_009 Browse preset cards. */
struct ChordSetListing
{
    int id = 0;
    std::string name;
    std::string author;
    std::string genre;
    std::string mood;
    std::string energy;
    int scaleId = 0;
};

/** One row from `packs` — a factory or expansion content pack. */
struct PackListing
{
    int id = 0;
    std::string name;        // "Wolf's Den Factory"
    std::string author;      // "Wolf Productions"
    std::string version;     // "1.0"
    std::string contentPath; // absolute path to WAV root (may be empty for built-in)
};

/**
 * One row from `presets` joined with `packs`.
 * Factory presets (isFactory == true) are recipe-based: the plugin applies
 * sample + envelope settings programmatically when loaded.
 * User presets (isFactory == false) carry a full APVTS+ModMatrix binary blob.
 */
struct PresetListing
{
    int id = 0;
    std::string name;
    std::string author;
    std::string category;        // "Bass" | "Keys" | "Leads" | "Pads" | "Plucks" | "Strings" | "Horns" | "Woodwinds" | "User"
    std::string tags;            // JSON array, e.g. ["dark","trap"]
    int packId = 0;
    std::string packName;        // joined from packs
    std::string samplePath;      // relative to pack content dir, e.g. "Keys/Rhodes.wav"
    int rootNote = 60;           // MIDI root for sample mapping
    bool loopEnabled = true;
    bool oneShot = false;
    float envAttack  = 0.005f;   // seconds
    float envDecay   = 0.3f;
    float envSustain = 0.8f;
    float envRelease = 0.4f;
    bool isFactory = false;      // true → state_blob is NULL; load via recipe
};

/** Pitch-class set: 12 slots indexed C=0 .. B=11 */
using PitchClassSet = std::array<bool, 12>;

// =============================================================================
// ChordMatch — a single chord detection result
// =============================================================================

struct ChordMatch
{
    int   chordId  = -1;   // index into TheoryEngine::getChordDefinitions() (-1 = none)
    int   rootNote =  0;   // 0-11  (C=0, C#=1 … B=11)
    float score    =  0.f; // Jaccard similarity [0.0, 1.0]
};

// =============================================================================
// VoicingResult — output of the voice-leading algorithm
// =============================================================================

struct VoicingResult
{
    static constexpr int kMaxNotes = 8;
    std::array<int, kMaxNotes> notes {}; // absolute MIDI note numbers (0-127)
    int numNotes = 0;
};

// =============================================================================
// TheoryEngine
// =============================================================================

/**
 * Wolf's Den music theory core.
 *
 * Responsibilities:
 *  - Loads chord / scale definitions from SQLite at startup (seeds DB if empty)
 *  - Collects active MIDI notes from the DSP thread via lock-free atomics
 *  - Runs chord detection + FFT audio analysis on a dedicated background thread
 *  - Computes voice-leading inversions on demand (message thread)
 *  - Provides a 128-entry scale remap table via a double-buffer (DSP-safe)
 *
 * Thread-safety model
 * -------------------
 *  DSP thread  : processMidi(), processAudio()  — no alloc, no lock, no throw
 *  Bg thread   : detection loop — reads activeNotes atomics, writes atomicMatches
 *  Msg thread  : initialise(), prepare(), reset(), setters, computeVoiceLeading()
 *  Any thread  : getBestMatch(), getTopMatches(), getScaleLookupTable()
 */
class TheoryEngine
{
public:
    TheoryEngine();
    ~TheoryEngine();

    TheoryEngine (const TheoryEngine&) = delete;
    TheoryEngine& operator= (const TheoryEngine&) = delete;

    // =========================================================================
    // Lifecycle  (message / main thread)
    // =========================================================================

    /**
     * Open (or create) the SQLite database, seed it if empty, load chord and
     * scale definitions into memory, build the initial lookup table, and start
     * the background detection thread.  Call before prepare().
     */
    void initialise (const juce::File& dbFile);

    /** Called from PluginProcessor::prepareToPlay(). */
    void prepare (double sampleRate, int samplesPerBlock) noexcept;

    /** Called from PluginProcessor::releaseResources().  Stops bg thread. */
    void reset() noexcept;

    // =========================================================================
    // DSP thread  — called from processBlock()
    // No allocations · No OS locks · No exceptions
    // =========================================================================

    /** Update active-note atomics from the MIDI buffer for this block. */
    void processMidi (const juce::MidiBuffer& midi) noexcept;

    /**
     * Push mono sidechain samples into the audio ring-buffer for FFT analysis.
     * Only consumed when DetectionMode == Audio.
     */
    void processAudio (const float* monoData, int numSamples) noexcept;

    // =========================================================================
    // Parameter setters  (message thread via APVTS listener)
    // =========================================================================

    void setScaleRoot          (int root)   noexcept;  ///< 0-11
    void setScaleType          (int index)  noexcept;  ///< index into scaleDefs
    void setVoiceLeadingEnabled (bool on)   noexcept;

    enum class DetectionMode { Midi, Audio };
    void setDetectionMode (DetectionMode mode) noexcept;

    // =========================================================================
    // Detection results  (safe from any thread — reads atomics)
    // =========================================================================

    ChordMatch               getBestMatch()  const noexcept;
    std::array<ChordMatch,3> getTopMatches() const noexcept;

    int  getActiveScaleRoot() const noexcept { return scaleRoot.load (std::memory_order_relaxed); }
    int  getActiveScaleType() const noexcept { return scaleType.load (std::memory_order_relaxed); }
    bool isDatabaseReady()    const noexcept { return databaseReady.load (std::memory_order_acquire); }

    // =========================================================================
    // Voice leading  (message thread — may allocate)
    // =========================================================================

    /**
     * Given the current chord voicing and a target chord, return the inversion
     * of the target that minimises total voice-movement (sum of abs differences).
     *
     * @param currentNotes   Sorted MIDI note numbers of the held chord
     * @param nextChordIdx   Index into getChordDefinitions()
     * @param nextRoot       Target root MIDI note (any octave; 60 = middle C)
     */
    VoicingResult computeVoiceLeading (const std::vector<int>& currentNotes,
                                       int nextChordIdx,
                                       int nextRoot) const;

    // =========================================================================
    // Scale lookup table  (DSP-safe double-buffer)
    // =========================================================================

    /**
     * Copy the current 128-entry remap table into tableOut.
     * tableOut[inputNote] = nearest in-scale MIDI note >= inputNote.
     * Safe to call from the DSP thread.
     */
    void getScaleLookupTable (std::array<int, 128>& tableOut) const noexcept;

    // =========================================================================
    // Database accessors  (message thread)
    // =========================================================================

    const std::vector<ChordDefinition>& getChordDefinitions() const noexcept { return chordDefs; }
    const std::vector<ScaleDefinition>& getScaleDefinitions() const noexcept { return scaleDefs; }
    const std::vector<ChordSetListing>& getChordSetListings() const noexcept { return chordSetList; }
    const std::vector<PresetListing>&   getPresetListings()   const noexcept { return presetList; }
    const std::vector<PackListing>&     getPackListings()     const noexcept { return packList; }
    int getChordCount() const noexcept { return static_cast<int> (chordDefs.size()); }
    int getScaleCount() const noexcept { return static_cast<int> (scaleDefs.size()); }

    // =========================================================================
    // Preset system  (message thread)
    // =========================================================================

    /**
     * Seed the factory pack and 41 WAV-backed presets into the DB if not already
     * present.  contentDir should point to the bundle's Resources/Factory folder.
     * Safe to call every launch — is a no-op after first run.
     */
    void seedFactoryPresets (const juce::File& contentDir);

    /** Reload preset and pack listings from the DB (call after save/delete). */
    void reloadPresetListings();

    /**
     * Save the current plugin state as a named user preset.
     * blob/blobSize come from PluginProcessor::getStateInformation().
     * Returns the new row's id, or -1 on failure.
     */
    int savePreset (const std::string& name,
                    const std::string& author,
                    const std::string& category,
                    const std::string& tags,
                    int packId,
                    const void* blob,
                    int blobSize);

    /**
     * Load the binary state blob for a user preset.
     * Returns an empty MemoryBlock if the preset is factory-based (isFactory)
     * or if the id is invalid.
     */
    juce::MemoryBlock loadPresetBlob (int presetId) const;

    /** Permanently remove a user preset (factory presets cannot be deleted). */
    void deletePreset (int presetId);

private:
    // =========================================================================
    // Database helpers  (background / message thread only)
    // =========================================================================

    bool openDatabase   (const juce::File& dbFile);
    void closeDatabase  () noexcept;
    void createSchema   ();                  ///< CREATE TABLE IF NOT EXISTS (incl. migration)
    void seedDatabase   ();                  ///< populate chord/scale tables when empty
    void loadChordDefinitions ();
    void loadScaleDefinitions ();
    void loadChordSetListings ();
    void loadPresetListings   ();
    void loadPackListings     ();

    static std::vector<int> parseIntervalJson (const std::string& json);
    static std::string      buildIntervalJson  (const std::vector<int>& iv);

    // =========================================================================
    // Scale lookup helpers
    // =========================================================================

    void rebuildScaleLookupTable () noexcept; ///< write inactive buffer, flip idx

    static int nearestInScaleNote (int midiNote,
                                   const std::vector<int>& scaleIntervals,
                                   int root) noexcept;

    // =========================================================================
    // Chord detection helpers  (background thread)
    // =========================================================================

    void detectionThreadFunc ();
    void runMidiDetection    ();
    void runFftDetection     ();

    PitchClassSet snapshotActivePitchClasses () const noexcept;

    float jaccardScore (const PitchClassSet& pitchClasses,
                        const std::vector<int>& intervals,
                        int root) const noexcept;

    std::array<ChordMatch, 3> detectChords (const PitchClassSet& pitchClasses) const;
    void storeMatches (const std::array<ChordMatch, 3>& matches) noexcept;

    // =========================================================================
    // Voice-leading helpers
    // =========================================================================

    std::vector<std::vector<int>> generateInversions (const std::vector<int>& intervals,
                                                      int root,
                                                      int referenceOctaveMidi) const;

    static int totalVoiceMovement (const std::vector<int>& a,
                                   const std::vector<int>& b) noexcept;

    // =========================================================================
    // Loaded data — immutable after initialise()
    // =========================================================================

    std::vector<ChordDefinition> chordDefs;
    std::vector<ScaleDefinition> scaleDefs;
    std::vector<ChordSetListing> chordSetList;
    std::vector<PresetListing>   presetList;
    std::vector<PackListing>     packList;

    // =========================================================================
    // Audio ring-buffer  (DSP thread → bg thread, lock-free SPSC)
    // =========================================================================

    static constexpr int kFftOrder = 11;               // 2^11 = 2048-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;   // 2048
    static constexpr int kFifoSize = kFftSize * 4;     // ring-buffer capacity

    juce::AbstractFifo              audioFifo { kFifoSize };
    std::array<float, kFifoSize>    audioFifoData {};
    juce::dsp::FFT                  fft;

    // Work buffers — bg thread only
    std::array<float, kFftSize * 2> fftWorkBuf {};     // interleaved real/imag
    std::array<float, kFftSize / 2> fftMagnitudes {};

    // =========================================================================
    // Active MIDI notes  (DSP thread writes, bg thread reads — one atomic/note)
    // =========================================================================

    std::array<std::atomic<bool>, 128> activeNotes {};

    // =========================================================================
    // Detection results  (bg thread writes, any thread reads — one atomic/field)
    // =========================================================================

    struct AtomicMatch
    {
        std::atomic<int> chordId  { -1 };
        std::atomic<int> rootNote {  0 };
        std::atomic<int> scoreX1k {  0 }; ///< score * 1000 stored as int
    };
    std::array<AtomicMatch, 3> atomicMatches;

    // =========================================================================
    // Parameters  (written msg thread, read DSP / bg threads)
    // =========================================================================

    std::atomic<int>  scaleRoot         { 0 };
    std::atomic<int>  scaleType         { 0 };
    std::atomic<bool> voiceLeadingOn    { true };
    std::atomic<int>  detectionModeAtom { 0 };  // 0=MIDI, 1=Audio

    // =========================================================================
    // Scale lookup double-buffer
    // DSP thread reads scaleTables[activeTableIdx].
    // Message thread writes to scaleTables[1 - activeTableIdx], then flips.
    // =========================================================================

    std::array<std::array<int, 128>, 2> scaleTables {};
    std::atomic<int>                    activeTableIdx { 0 };

    // =========================================================================
    // Background thread
    // =========================================================================

    std::unique_ptr<std::thread> detectionThread;
    std::atomic<bool>            threadShouldStop { false };
    std::atomic<bool>            databaseReady    { false };

    // =========================================================================
    // Misc
    // =========================================================================

    double   currentSampleRate = 44100.0;
    int      currentBlockSize  = 512;
    sqlite3* db                = nullptr;
};

} // namespace wolfsden
