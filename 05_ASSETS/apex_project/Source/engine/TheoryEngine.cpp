#include "TheoryEngine.h"

// SQLite3 single-file amalgamation (bundled in ThirdParty/sqlite3/)
#include "../../ThirdParty/sqlite3/sqlite3.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>

namespace wolfsden
{

// =============================================================================
// Helpers — JSON interval array serialisation
// =============================================================================

namespace
{

/** Parse "[0,4,7,11]" → {0,4,7,11} */
std::vector<int> parseIntervalJsonImpl (const std::string& json)
{
    std::vector<int> result;
    std::istringstream ss (json);
    char ch;
    int val;
    while (ss >> ch)
    {
        if (ch == '[' || ch == ']' || ch == ',') continue;
        ss.putback (ch);
        if (ss >> val)
            result.push_back (val);
    }
    return result;
}

/** {0,4,7,11} → "[0,4,7,11]" */
std::string buildIntervalJsonImpl (const std::vector<int>& iv)
{
    std::string s = "[";
    for (int i = 0; i < (int) iv.size(); ++i)
    {
        if (i) s += ",";
        s += std::to_string (iv[i]);
    }
    s += "]";
    return s;
}

// Frequency of MIDI note 0 (C-1) in Hz


/** Portable 32-bit popcount (C++17). */
inline int popcount32 (unsigned x) noexcept
{
    int n = 0;
    while (x != 0)
    {
        x &= x - 1u;
        ++n;
    }
    return n;
}

/** Convert FFT bin index to nearest MIDI pitch class (0-11). */
int binToPitchClass (int bin, int fftSize, double sampleRate) noexcept
{
    if (bin <= 0) return 0;
    const double hz = static_cast<double> (bin) * sampleRate / static_cast<double> (fftSize);
    if (hz < 20.0 || hz > 20000.0) return -1;
    const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
    if (midi < 0.0 || midi > 127.0) return -1;
    return static_cast<int> (std::round (midi)) % 12;
}

} // anonymous namespace

// =============================================================================
// Constructor / Destructor
// =============================================================================

TheoryEngine::TheoryEngine()
    : fft (kFftOrder)
{
    for (auto& a : activeNotes)
        a.store (false, std::memory_order_relaxed);

    // Initialise scale lookup tables to chromatic (identity mapping)
    for (auto& tbl : scaleTables)
        for (int i = 0; i < 128; ++i)
            tbl[static_cast<size_t> (i)] = i;
}

TheoryEngine::~TheoryEngine()
{
    reset();
    closeDatabase();
}

// =============================================================================
// Lifecycle
// =============================================================================

void TheoryEngine::initialise (const juce::File& dbFile)
{
    // Close any previous connection
    closeDatabase();

    if (!openDatabase (dbFile))
    {
        jassertfalse;
        return;
    }

    createSchema();
    seedDatabase();
    loadChordDefinitions();
    loadScaleDefinitions();
    loadChordSetListings();
    loadPackListings();
    loadPresetListings();
    rebuildScaleLookupTable();

    databaseReady.store (true, std::memory_order_release);

    // Start background detection thread
    threadShouldStop.store (false, std::memory_order_release);
    detectionThread = std::make_unique<std::thread> ([this] { detectionThreadFunc(); });
}

void TheoryEngine::prepare (double sampleRate, int samplesPerBlock) noexcept
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    if (!databaseReady.load (std::memory_order_acquire))
        return;

    threadShouldStop.store (false, std::memory_order_release);
    if (!detectionThread)
        detectionThread = std::make_unique<std::thread> ([this] { detectionThreadFunc(); });
}

void TheoryEngine::reset() noexcept
{
    // Signal the background thread to stop and join it
    threadShouldStop.store (true, std::memory_order_release);
    if (detectionThread && detectionThread->joinable())
        detectionThread->join();
    detectionThread.reset();

    // Clear all active notes
    for (auto& a : activeNotes)
        a.store (false, std::memory_order_relaxed);

    // Reset audio FIFO
    audioFifo.reset();
}

// =============================================================================
// DSP thread — processMidi
// =============================================================================

void TheoryEngine::processMidi (const juce::MidiBuffer& midi) noexcept
{
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            activeNotes[static_cast<size_t> (msg.getNoteNumber())].store (true, std::memory_order_relaxed);
        else if (msg.isNoteOff())
            activeNotes[static_cast<size_t> (msg.getNoteNumber())].store (false, std::memory_order_relaxed);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            for (auto& a : activeNotes)
                a.store (false, std::memory_order_relaxed);
    }
}

// =============================================================================
// DSP thread — processAudio (sidechain)
// =============================================================================

void TheoryEngine::processAudio (const float* monoData, int numSamples) noexcept
{
    if (detectionModeAtom.load (std::memory_order_relaxed) != 1) return;
    if (!monoData || numSamples <= 0) return;

    int written = 0;
    while (written < numSamples)
    {
        int start1, size1, start2, size2;
        const int toWrite = numSamples - written;
        audioFifo.prepareToWrite (toWrite, start1, size1, start2, size2);
        if (size1 > 0) std::memcpy (&audioFifoData[static_cast<size_t> (start1)],
                                    monoData + written, static_cast<size_t> (size1) * sizeof (float));
        if (size2 > 0) std::memcpy (&audioFifoData[static_cast<size_t> (start2)],
                                    monoData + written + size1, static_cast<size_t> (size2) * sizeof (float));
        audioFifo.finishedWrite (size1 + size2);
        written += (size1 + size2);
        if (size1 + size2 == 0) break; // FIFO full — drop remaining
    }
}

// =============================================================================
// Parameter setters
// =============================================================================

void TheoryEngine::setScaleRoot (int root) noexcept
{
    scaleRoot.store (juce::jlimit (0, 11, root), std::memory_order_relaxed);
    rebuildScaleLookupTable();
}

void TheoryEngine::setScaleType (int index) noexcept
{
    scaleType.store (index, std::memory_order_relaxed);
    rebuildScaleLookupTable();
}

void TheoryEngine::setVoiceLeadingEnabled (bool on) noexcept
{
    voiceLeadingOn.store (on, std::memory_order_relaxed);
}

void TheoryEngine::setDetectionMode (DetectionMode mode) noexcept
{
    detectionModeAtom.store (mode == DetectionMode::Midi ? 0 : 1, std::memory_order_relaxed);
}

TheoryEngine::DetectionMode TheoryEngine::getDetectionMode() const noexcept
{
    return detectionModeAtom.load (std::memory_order_relaxed) != 0
               ? DetectionMode::Audio
               : DetectionMode::Midi;
}

// =============================================================================
// Detection results
// =============================================================================

ChordMatch TheoryEngine::getBestMatch() const noexcept
{
    return { atomicMatches[0].chordId.load  (std::memory_order_relaxed),
             atomicMatches[0].rootNote.load (std::memory_order_relaxed),
             static_cast<float> (atomicMatches[0].scoreX1k.load (std::memory_order_relaxed)) / 1000.f };
}

std::array<ChordMatch, 3> TheoryEngine::getTopMatches() const noexcept
{
    std::array<ChordMatch, 3> out;
    for (int i = 0; i < 3; ++i)
        out[static_cast<size_t> (i)] = { atomicMatches[static_cast<size_t> (i)].chordId.load (std::memory_order_relaxed),
                                         atomicMatches[static_cast<size_t> (i)].rootNote.load (std::memory_order_relaxed),
                                         static_cast<float> (atomicMatches[static_cast<size_t> (i)].scoreX1k.load (std::memory_order_relaxed)) / 1000.f };
    return out;
}

// =============================================================================
// Scale lookup table
// =============================================================================

void TheoryEngine::getScaleLookupTable (std::array<int, 128>& tableOut) const noexcept
{
    const int idx = activeTableIdx.load (std::memory_order_acquire);
    tableOut = scaleTables[static_cast<size_t> (idx)];
}

void TheoryEngine::rebuildScaleLookupTable() noexcept
{
    const int root  = scaleRoot.load (std::memory_order_relaxed);
    const int sType = scaleType.load (std::memory_order_relaxed);

    // Determine which scale intervals to use
    const std::vector<int>* intervals = nullptr;
    if (!scaleDefs.empty())
    {
        const int clamped = juce::jlimit (0, static_cast<int> (scaleDefs.size()) - 1, sType);
        intervals = &scaleDefs[static_cast<size_t> (clamped)].intervals;
    }

    // Write into the inactive buffer
    const int writeIdx = 1 - activeTableIdx.load (std::memory_order_relaxed);
    auto& tbl = scaleTables[static_cast<size_t> (writeIdx)];

    if (!intervals || intervals->empty())
    {
        // Chromatic fallback — identity
        for (int i = 0; i < 128; ++i) tbl[static_cast<size_t> (i)] = i;
    }
    else
    {
        for (int i = 0; i < 128; ++i)
            tbl[static_cast<size_t> (i)] = nearestInScaleNote (i, *intervals, root);
    }

    // Flip to make it the active buffer
    activeTableIdx.store (writeIdx, std::memory_order_release);
}

// static
int TheoryEngine::nearestInScaleNote (int midiNote,
                                      const std::vector<int>& scaleIntervals,
                                      int root) noexcept
{
    // Walk up from midiNote until we find a note whose pitch class is in the scale
    for (int offset = 0; offset < 12; ++offset)
    {
        const int candidate = midiNote + offset;
        if (candidate > 127) break;
        const int pc = ((candidate - root) % 12 + 12) % 12;
        for (int interval : scaleIntervals)
            if ((interval % 12) == pc)
                return candidate;
    }
    return midiNote; // Chromatic fallback
}

// =============================================================================
// Background detection thread
// =============================================================================

void TheoryEngine::detectionThreadFunc()
{
    using namespace std::chrono_literals;

    while (!threadShouldStop.load (std::memory_order_acquire))
    {
        if (detectionModeAtom.load (std::memory_order_relaxed) == 1)
            runFftDetection();
        else
            runMidiDetection();

        // ~50 ms between passes
        std::this_thread::sleep_for (50ms);
    }
}

void TheoryEngine::runMidiDetection()
{
    if (chordDefs.empty()) return;

    const PitchClassSet pitchClasses = snapshotActivePitchClasses();

    // Only proceed if at least 2 pitch classes are active
    int activeCount = 0;
    for (bool b : pitchClasses) if (b) ++activeCount;
    if (activeCount < 2) return;

    const auto matches = detectChords (pitchClasses);
    storeMatches (matches);
}

void TheoryEngine::runFftDetection()
{
    if (chordDefs.empty()) return;

    // Need a full FFT frame of audio
    if (audioFifo.getNumReady() < kFftSize) return;

    // Read kFftSize samples
    {
        int start1, size1, start2, size2;
        audioFifo.prepareToRead (kFftSize, start1, size1, start2, size2);

        // Copy into work buffer (real part; imaginary = 0)
        for (int i = 0; i < size1; ++i)
        {
            fftWorkBuf[static_cast<size_t> (i * 2)]     = audioFifoData[static_cast<size_t> (start1 + i)];
            fftWorkBuf[static_cast<size_t> (i * 2 + 1)] = 0.f;
        }
        for (int i = 0; i < size2; ++i)
        {
            fftWorkBuf[static_cast<size_t> ((size1 + i) * 2)]     = audioFifoData[static_cast<size_t> (start2 + i)];
            fftWorkBuf[static_cast<size_t> ((size1 + i) * 2 + 1)] = 0.f;
        }

        // Zero-pad remainder (if any)
        const int total = size1 + size2;
        for (int i = total; i < kFftSize; ++i)
        {
            fftWorkBuf[static_cast<size_t> (i * 2)]     = 0.f;
            fftWorkBuf[static_cast<size_t> (i * 2 + 1)] = 0.f;
        }

        audioFifo.finishedRead (size1 + size2);
    }

    // Apply Hanning window to the real part
    for (int i = 0; i < kFftSize; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.f * juce::MathConstants<float>::pi
                                                     * static_cast<float> (i)
                                                     / static_cast<float> (kFftSize - 1));
        fftWorkBuf[static_cast<size_t> (i * 2)] *= w;
    }

    // Perform FFT (complex in-place, interleaved)
    fft.performRealOnlyForwardTransform (fftWorkBuf.data(), true);

    // Compute magnitudes for positive frequencies
    for (int i = 0; i < kFftSize / 2; ++i)
    {
        const float re = fftWorkBuf[static_cast<size_t> (i * 2)];
        const float im = fftWorkBuf[static_cast<size_t> (i * 2 + 1)];
        fftMagnitudes[static_cast<size_t> (i)] = std::sqrt (re * re + im * im);
    }

    // Pick the top 6 magnitude peaks -> map to pitch classes
    PitchClassSet pitchClasses {};
    pitchClasses.fill (false);

    // Simple local-max peak detection (skip DC bin 0)
    std::array<std::pair<float, int>, 6> peaks {};
    peaks.fill ({ 0.f, -1 });

    for (int i = 1; i < kFftSize / 2 - 1; ++i)
    {
        const float mag = fftMagnitudes[static_cast<size_t> (i)];
        if (mag > fftMagnitudes[static_cast<size_t> (i - 1)] &&
            mag > fftMagnitudes[static_cast<size_t> (i + 1)])
        {
            // Replace weakest peak if this is stronger
            auto weakest = std::min_element (peaks.begin(), peaks.end(),
                [] (const auto& a, const auto& b) { return a.first < b.first; });
            if (mag > weakest->first)
                *weakest = { mag, i };
        }
    }

    for (const auto& p : peaks)
    {
        const int bin = p.second;
        if (bin < 0) continue;
        const int pc = binToPitchClass (bin, kFftSize, currentSampleRate);
        if (pc >= 0)
            pitchClasses[static_cast<size_t> (pc)] = true;
    }

    int activeCount = 0;
    for (bool b : pitchClasses) if (b) ++activeCount;
    if (activeCount < 2) return;

    const auto matches = detectChords (pitchClasses);
    storeMatches (matches);
}

// =============================================================================
// Chord detection core
// =============================================================================

PitchClassSet TheoryEngine::snapshotActivePitchClasses() const noexcept
{
    PitchClassSet pcs {};
    pcs.fill (false);
    for (int note = 0; note < 128; ++note)
        if (activeNotes[static_cast<size_t> (note)].load (std::memory_order_relaxed))
            pcs[static_cast<size_t> (note % 12)] = true;
    return pcs;
}

float TheoryEngine::jaccardScore (const PitchClassSet& pitchClasses,
                                  const std::vector<int>& intervals,
                                  int root) const noexcept
{
    // Build the chord pitch-class set for this root
    int chordBits = 0;
    for (int iv : intervals)
        chordBits |= (1 << ((root + iv) % 12));

    // Build active-note bitmask
    int activeBits = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (pitchClasses[static_cast<size_t> (pc)])
            activeBits |= (1 << pc);

    const int intersection = popcount32 (static_cast<unsigned> (activeBits & chordBits));
    const int unionBits    = popcount32 (static_cast<unsigned> (activeBits | chordBits));

    if (unionBits == 0) return 0.f;
    return static_cast<float> (intersection) / static_cast<float> (unionBits);
}

std::array<ChordMatch, 3> TheoryEngine::detectChords (const PitchClassSet& pitchClasses) const
{
    // Initialise with sentinel
    std::array<ChordMatch, 3> top;
    top[0] = top[1] = top[2] = { -1, 0, 0.f };

    for (size_t idx = 0; idx < chordDefs.size(); ++idx)
    {
        const auto& cd = chordDefs[idx];
        for (int root = 0; root < 12; ++root)
        {
            const float score = jaccardScore (pitchClasses, cd.intervals, root);
            if (score < 0.5f) continue; // Minimum threshold

            // Insert into top-3 if it beats the weakest entry (chordId = index into chordDefs)
            ChordMatch candidate { static_cast<int> (idx), root, score };
            for (auto& slot : top)
            {
                if (candidate.score > slot.score)
                {
                    std::swap (slot, candidate);
                }
            }
        }
    }

    return top;
}

void TheoryEngine::storeMatches (const std::array<ChordMatch, 3>& matches) noexcept
{
    for (int i = 0; i < 3; ++i)
    {
        atomicMatches[static_cast<size_t> (i)].chordId.store  (matches[static_cast<size_t> (i)].chordId,  std::memory_order_relaxed);
        atomicMatches[static_cast<size_t> (i)].rootNote.store (matches[static_cast<size_t> (i)].rootNote, std::memory_order_relaxed);
        atomicMatches[static_cast<size_t> (i)].scoreX1k.store (
            static_cast<int> (matches[static_cast<size_t> (i)].score * 1000.f), std::memory_order_relaxed);
    }
}

// =============================================================================
// Voice leading
// =============================================================================

VoicingResult TheoryEngine::computeVoiceLeading (const int* currentNotes,
                                                  int numCurrent,
                                                  int nextChordIdx,
                                                  int nextRoot) const noexcept
{
    VoicingResult result;

    if (nextChordIdx < 0 || nextChordIdx >= static_cast<int> (chordDefs.size()))
        return result;

    const auto& cd = chordDefs[static_cast<size_t> (nextChordIdx)];

    // Reference octave: centre around the median of current notes
    int refOctaveMidi = 60; // middle C fallback
    if (numCurrent > 0 && currentNotes != nullptr)
    {
        int sum = 0;
        for (int i = 0; i < numCurrent; ++i) sum += currentNotes[i];
        refOctaveMidi = sum / numCurrent;
    }

    constexpr int kMaxCand = 16;
    int candNotes[kMaxCand][VoicingResult::kMaxNotes];
    const int numCand = generateInversions (cd.intervals, nextRoot, refOctaveMidi, candNotes, kMaxCand);

    if (numCand <= 0) return result;

    // Find the candidate with the minimum total voice movement
    int bestIdx = 0;
    int bestCost = std::numeric_limits<int>::max();

    const int nTarget = static_cast<int>(cd.intervals.size());
    const int nB = std::min(nTarget, (int)VoicingResult::kMaxNotes);

    for (int i = 0; i < numCand; ++i)
    {
        const int cost = totalVoiceMovement (currentNotes, numCurrent, candNotes[i], nB);
        if (cost < bestCost)
        {
            bestCost = cost;
            bestIdx = i;
        }
    }

    // Fill VoicingResult
    result.numNotes = nB;
    for (int i = 0; i < nB; ++i)
        result.notes[static_cast<size_t> (i)] = candNotes[bestIdx][i];

    return result;
}

int TheoryEngine::generateInversions (const std::vector<int>& intervals,
                                      int root,
                                      int referenceOctaveMidi,
                                      int (*outNotes)[VoicingResult::kMaxNotes],
                                      int maxCandidates) const noexcept
{
    if (intervals.empty() || maxCandidates <= 0 || outNotes == nullptr) return 0;

    const int numNotes = static_cast<int> (intervals.size());
    const int nToCopy = std::min(numNotes, (int)VoicingResult::kMaxNotes);

    // Build root-position notes centred near referenceOctaveMidi
    int rootBase = (referenceOctaveMidi / 12) * 12 + (root % 12);
    if (rootBase < referenceOctaveMidi - 6) rootBase += 12;
    if (rootBase > referenceOctaveMidi + 6) rootBase -= 12;

    int currentCand = 0;

    // Root-position voicing as starting point
    int working[VoicingResult::kMaxNotes];
    for (int i = 0; i < nToCopy; ++i)
        working[i] = juce::jlimit (0, 127, rootBase + intervals[(size_t)i]);

    // Generate inversions by rotating
    for (int inv = 0; inv < numNotes && currentCand < maxCandidates; ++inv)
    {
        for (int i = 0; i < nToCopy; ++i) outNotes[currentCand][i] = working[i];
        currentCand++;

        // Rotate: lowest note goes up one octave
        int lowIdx = 0;
        for (int i = 1; i < nToCopy; ++i) if (working[i] < working[lowIdx]) lowIdx = i;
        working[lowIdx] += 12;
        for (int i = 0; i < nToCopy; ++i) working[i] = juce::jlimit(0, 127, working[i]);
    }

    // Octave shifts of root pos
    if (currentCand < maxCandidates)
    {
        for (int i = 0; i < nToCopy; ++i) outNotes[currentCand][i] = juce::jlimit(0, 127, rootBase + intervals[(size_t)i] + 12);
        currentCand++;
    }
    if (currentCand < maxCandidates)
    {
        for (int i = 0; i < nToCopy; ++i) outNotes[currentCand][i] = juce::jlimit(0, 127, rootBase + intervals[(size_t)i] - 12);
        currentCand++;
    }

    return currentCand;
}

int TheoryEngine::totalVoiceMovement (const int* a, int nA,
                                      const int* b, int nB) noexcept
{
    if (nA <= 0 || nB <= 0 || a == nullptr || b == nullptr) return std::numeric_limits<int>::max();

    int sa[VoicingResult::kMaxNotes];
    int sb[VoicingResult::kMaxNotes];
    const int countA = std::min(nA, (int)VoicingResult::kMaxNotes);
    const int countB = std::min(nB, (int)VoicingResult::kMaxNotes);

    for (int i = 0; i < countA; ++i) sa[i] = a[i];
    for (int i = 0; i < countB; ++i) sb[i] = b[i];

    // Simple bubble sort (DSP safe, tiny N)
    for (int i = 0; i < countA - 1; ++i)
        for (int j = 0; j < countA - i - 1; ++j)
            if (sa[j] > sa[j+1]) { int t = sa[j]; sa[j] = sa[j+1]; sa[j+1] = t; }

    for (int i = 0; i < countB - 1; ++i)
        for (int j = 0; j < countB - i - 1; ++j)
            if (sb[j] > sb[j+1]) { int t = sb[j]; sb[j] = sb[j+1]; sb[j+1] = t; }

    const int n = std::min (countA, countB);
    int total = 0;
    for (int i = 0; i < n; ++i)
        total += std::abs (sa[i] - sb[i]);

    return total;
}

// =============================================================================
// Database — open / close / schema / seed / load
// =============================================================================

bool TheoryEngine::openDatabase (const juce::File& dbFile)
{
    // Create parent directory if it doesn't exist
    dbFile.getParentDirectory().createDirectory();

    const int rc = sqlite3_open (dbFile.getFullPathName().toRawUTF8(), &db);
    if (rc != SQLITE_OK)
    {
        sqlite3_close (db);
        db = nullptr;
        return false;
    }

    // Enable WAL mode for better concurrent read performance
    sqlite3_exec (db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec (db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    return true;
}

void TheoryEngine::closeDatabase() noexcept
{
    if (db)
    {
        sqlite3_close (db);
        db = nullptr;
    }
}

void TheoryEngine::createSchema()
{
    if (!db) return;

    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS chords (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            name             TEXT NOT NULL,
            symbol           TEXT NOT NULL,
            interval_pattern TEXT NOT NULL,   -- JSON: [0,4,7]
            category         TEXT NOT NULL,   -- triad/seventh/ninth/extended/sus
            quality          TEXT NOT NULL    -- major/minor/dominant/diminished/augmented
        );

        CREATE TABLE IF NOT EXISTS scales (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            name             TEXT NOT NULL,
            interval_pattern TEXT NOT NULL,   -- JSON: [0,2,4,5,7,9,11]
            mode_family      TEXT NOT NULL    -- major/minor/pentatonic/exotic/chromatic
        );

        CREATE TABLE IF NOT EXISTS chord_sets (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL,
            author     TEXT DEFAULT 'Wolf Den Factory',
            genre      TEXT,
            mood       TEXT,
            energy     TEXT,          -- Low / Medium / High
            root_note  INTEGER DEFAULT 0,
            scale_id   INTEGER REFERENCES scales(id)
        );

        CREATE TABLE IF NOT EXISTS chord_set_entries (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            chord_set_id INTEGER NOT NULL REFERENCES chord_sets(id),
            position     INTEGER NOT NULL,   -- 1-based order
            root_note    INTEGER NOT NULL,   -- 0=C, 1=C#, ... 11=B
            chord_id     INTEGER NOT NULL REFERENCES chords(id),
            voicing      TEXT,               -- JSON: specific MIDI note layout
            motion_id    INTEGER DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS packs (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT NOT NULL,
            author       TEXT NOT NULL DEFAULT 'Wolf Productions',
            version      TEXT NOT NULL DEFAULT '1.0',
            content_path TEXT,
            installed_at INTEGER DEFAULT (strftime('%s','now'))
        );

        CREATE TABLE IF NOT EXISTS presets (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            name            TEXT NOT NULL,
            author          TEXT,
            tags            TEXT,
            category        TEXT,
            pack_id         INTEGER REFERENCES packs(id),
            sample_path     TEXT,
            root_note       INTEGER DEFAULT 60,
            loop_enabled    INTEGER DEFAULT 1,
            one_shot        INTEGER DEFAULT 0,
            env_attack      REAL DEFAULT 0.005,
            env_decay       REAL DEFAULT 0.3,
            env_sustain     REAL DEFAULT 0.8,
            env_release     REAL DEFAULT 0.4,
            created_at      INTEGER DEFAULT (strftime('%s','now')),
            state_blob      BLOB
        );

        CREATE TABLE IF NOT EXISTS progressions (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            name            TEXT NOT NULL,
            genre           TEXT NOT NULL,
            mood            TEXT,
            energy          INTEGER DEFAULT 2,
            root_key        INTEGER DEFAULT 0,
            chord_sequence  TEXT NOT NULL,  -- JSON array of chord_definition_id values
            root_sequence   TEXT NOT NULL DEFAULT '[0,0,0,0]',  -- JSON array of semitone offsets
            created_at      TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_chord_set_entries_set ON chord_set_entries(chord_set_id);
        CREATE INDEX IF NOT EXISTS idx_chord_sets_genre      ON chord_sets(genre);
        CREATE INDEX IF NOT EXISTS idx_chord_sets_mood       ON chord_sets(mood);
        CREATE INDEX IF NOT EXISTS idx_presets_pack          ON presets(pack_id);
        CREATE INDEX IF NOT EXISTS idx_presets_category      ON presets(category);
    )SQL";

    char* errMsg = nullptr;
    sqlite3_exec (db, sql, nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free (errMsg);

    // Migration: add columns to presets that may not exist in older DBs.
    // ALTER TABLE ADD COLUMN is a no-op-safe pattern in SQLite (fails silently via ignoring error).
    const char* migrations[] = {
        "ALTER TABLE presets ADD COLUMN pack_id      INTEGER REFERENCES packs(id);",
        "ALTER TABLE presets ADD COLUMN sample_path  TEXT;",
        "ALTER TABLE presets ADD COLUMN root_note    INTEGER DEFAULT 60;",
        "ALTER TABLE presets ADD COLUMN loop_enabled INTEGER DEFAULT 1;",
        "ALTER TABLE presets ADD COLUMN one_shot     INTEGER DEFAULT 0;",
        "ALTER TABLE presets ADD COLUMN env_attack   REAL DEFAULT 0.005;",
        "ALTER TABLE presets ADD COLUMN env_decay    REAL DEFAULT 0.3;",
        "ALTER TABLE presets ADD COLUMN env_sustain  REAL DEFAULT 0.8;",
        "ALTER TABLE presets ADD COLUMN env_release  REAL DEFAULT 0.4;",
        // Progressions migration: add root_sequence column introduced in TASK_020.
        // SQLite returns an error if the column already exists — that error is intentionally ignored
        // by the loop below, making this a safe no-op for fresh DBs.
        "ALTER TABLE progressions ADD COLUMN root_sequence TEXT DEFAULT '[0,0,0,0]';",
        nullptr
    };
    for (int i = 0; migrations[i] != nullptr; ++i)
        sqlite3_exec (db, migrations[i], nullptr, nullptr, nullptr); // intentionally ignore errors
}

// =============================================================================
// Database — seed
// =============================================================================

void TheoryEngine::seedDatabase()
{
    if (!db) return;

    // Check if chords table already has data
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2 (db, "SELECT COUNT(*) FROM chords;", -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step (stmt) == SQLITE_ROW)
            count = sqlite3_column_int (stmt, 0);
        sqlite3_finalize (stmt);
    }
    // Check if chord_sets table already has data
    int set_count = 0;
    if (sqlite3_prepare_v2 (db, "SELECT COUNT(*) FROM chord_sets;", -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step (stmt) == SQLITE_ROW)
            set_count = sqlite3_column_int (stmt, 0);
        sqlite3_finalize (stmt);
    }

    // Each seeding section below manages its own count guard — do not early-exit
    // here, or progressions won't seed into existing DBs that already have chords.
    const bool chordsAlreadySeeded = (count > 0 && set_count > 0);


    // -------------------------------------------------------------------------
    // Seed chords / scales / chord_sets only on a fresh DB
    // -------------------------------------------------------------------------
    if (!chordsAlreadySeeded)
    {
    sqlite3_exec (db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);


    // =========================================================================
    // 1. CHORD DEFINITIONS  (42 types)
    //    Guard: only insert if table is empty to avoid duplicate rows when
    //    chord_sets is being added to an existing DB.
    // =========================================================================
    if (count == 0)
    {
    const char* chordSql = R"SQL(
    INSERT INTO chords (name,symbol,interval_pattern,category,quality) VALUES
      ('Major',              'maj',     '[0,4,7]',              'triad',    'major'),
      ('Minor',              'm',       '[0,3,7]',              'triad',    'minor'),
      ('Diminished',         'dim',     '[0,3,6]',              'triad',    'diminished'),
      ('Augmented',          'aug',     '[0,4,8]',              'triad',    'augmented'),
      ('Suspended 2nd',      'sus2',    '[0,2,7]',              'sus',      'major'),
      ('Suspended 4th',      'sus4',    '[0,5,7]',              'sus',      'major'),
      ('Power',              '5',       '[0,7]',                'triad',    'major'),
      ('Major 6th',          'maj6',    '[0,4,7,9]',            'seventh',  'major'),
      ('Minor 6th',          'm6',      '[0,3,7,9]',            'seventh',  'minor'),
      ('Dominant 7th',       '7',       '[0,4,7,10]',           'seventh',  'dominant'),
      ('Major 7th',          'maj7',    '[0,4,7,11]',           'seventh',  'major'),
      ('Minor 7th',          'm7',      '[0,3,7,10]',           'seventh',  'minor'),
      ('Diminished 7th',     'dim7',    '[0,3,6,9]',            'seventh',  'diminished'),
      ('Half-Diminished',    'm7b5',    '[0,3,6,10]',           'seventh',  'diminished'),
      ('Minor/Major 7th',    'mMaj7',   '[0,3,7,11]',           'seventh',  'minor'),
      ('Augmented Major 7th','augMaj7', '[0,4,8,11]',           'seventh',  'augmented'),
      ('Dominant 7th Sus4',  '7sus4',   '[0,5,7,10]',           'seventh',  'dominant'),
      ('Major Add 9',        'add9',    '[0,2,4,7]',            'triad',    'major'),
      ('Minor Add 9',        'madd9',   '[0,2,3,7]',            'triad',    'minor'),
      ('Dominant 9th',       '9',       '[0,4,7,10,14]',        'ninth',    'dominant'),
      ('Major 9th',          'maj9',    '[0,4,7,11,14]',        'ninth',    'major'),
      ('Minor 9th',          'm9',      '[0,3,7,10,14]',        'ninth',    'minor'),
      ('Dominant 11th',      '11',      '[0,4,7,10,14,17]',     'extended', 'dominant'),
      ('Major 11th',         'maj11',   '[0,4,7,11,14,17]',     'extended', 'major'),
      ('Minor 11th',         'm11',     '[0,3,7,10,14,17]',     'extended', 'minor'),
      ('Dominant 13th',      '13',      '[0,4,7,10,14,21]',     'extended', 'dominant'),
      ('Major 13th',         'maj13',   '[0,4,7,11,14,21]',     'extended', 'major'),
      ('Minor 13th',         'm13',     '[0,3,7,10,14,21]',     'extended', 'minor'),
      ('Dom 7th Flat 9',     '7b9',     '[0,4,7,10,13]',        'extended', 'dominant'),
      ('Dom 7th Sharp 9',    '7#9',     '[0,4,7,10,15]',        'extended', 'dominant'),
      ('Dom 7th Flat 5',     '7b5',     '[0,4,6,10]',           'seventh',  'dominant'),
      ('Dom 7th Sharp 5',    '7#5',     '[0,4,8,10]',           'seventh',  'dominant'),
      ('Dom 7th Sharp 11',   '7#11',    '[0,4,7,10,18]',        'extended', 'dominant'),
      ('Major 7th Sharp 11', 'maj7#11', '[0,4,7,11,18]',        'extended', 'major'),
      ('Minor/Major 9th',    'mMaj9',   '[0,3,7,11,14]',        'ninth',    'minor'),
      ('Augmented 7th',      'aug7',    '[0,4,8,10]',           'seventh',  'augmented'),
      ('Quartal',            'qrt',     '[0,5,10]',             'triad',    'major'),
      ('Quintal',            'qnt',     '[0,7,14]',             'triad',    'major'),
      ('Neapolitan',         'N',       '[0,1,5,8]',            'triad',    'major'),
      ('Dominant b9 b13',    '7b9b13',  '[0,4,7,10,13,20]',     'extended', 'dominant'),
      ('Major 6/9',          '6/9',     '[0,2,4,7,9]',          'extended', 'major'),
      ('Minor 6/9',          'm6/9',    '[0,2,3,7,9]',          'extended', 'minor');
    )SQL";

    char* errMsg = nullptr;
    sqlite3_exec (db, chordSql, nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free (errMsg);

    // =========================================================================
    // 2. SCALE DEFINITIONS  (58 scales)
    // =========================================================================
    const char* scaleSql = R"SQL(
    INSERT INTO scales (name,interval_pattern,mode_family) VALUES
      ('Major',                    '[0,2,4,5,7,9,11]',          'major'),
      ('Natural Minor',            '[0,2,3,5,7,8,10]',          'minor'),
      ('Harmonic Minor',           '[0,2,3,5,7,8,11]',          'minor'),
      ('Melodic Minor',            '[0,2,3,5,7,9,11]',          'minor'),
      ('Dorian',                   '[0,2,3,5,7,9,10]',          'minor'),
      ('Phrygian',                 '[0,1,3,5,7,8,10]',          'minor'),
      ('Lydian',                   '[0,2,4,6,7,9,11]',          'major'),
      ('Mixolydian',               '[0,2,4,5,7,9,10]',          'major'),
      ('Locrian',                  '[0,1,3,5,6,8,10]',          'minor'),
      ('Pentatonic Major',         '[0,2,4,7,9]',               'pentatonic'),
      ('Pentatonic Minor',         '[0,3,5,7,10]',              'pentatonic'),
      ('Blues',                    '[0,3,5,6,7,10]',            'pentatonic'),
      ('Chromatic',                '[0,1,2,3,4,5,6,7,8,9,10,11]','chromatic'),
      ('Whole Tone',               '[0,2,4,6,8,10]',            'exotic'),
      ('Diminished Half-Whole',    '[0,1,3,4,6,7,9,10]',        'exotic'),
      ('Diminished Whole-Half',    '[0,2,3,5,6,8,9,11]',        'exotic'),
      ('Augmented',                '[0,3,4,7,8,11]',            'exotic'),
      ('Double Harmonic',          '[0,1,4,5,7,8,11]',          'exotic'),
      ('Phrygian Dominant',        '[0,1,4,5,7,8,10]',          'major'),
      ('Lydian Dominant',          '[0,2,4,6,7,9,10]',          'major'),
      ('Lydian Augmented',         '[0,2,4,6,8,9,11]',          'major'),
      ('Super Locrian',            '[0,1,3,4,6,8,10]',          'minor'),
      ('Bebop Major',              '[0,2,4,5,7,8,9,11]',        'major'),
      ('Bebop Minor',              '[0,2,3,4,5,7,9,10]',        'minor'),
      ('Bebop Dominant',           '[0,2,4,5,7,9,10,11]',       'major'),
      ('Enigmatic',                '[0,1,4,6,8,10,11]',         'exotic'),
      ('Neapolitan Major',         '[0,1,3,5,7,9,11]',          'major'),
      ('Neapolitan Minor',         '[0,1,3,5,7,8,11]',          'minor'),
      ('Persian',                  '[0,1,4,5,6,8,11]',          'exotic'),
      ('Arabian',                  '[0,2,4,5,6,8,10]',          'exotic'),
      ('Hungarian Major',          '[0,3,4,6,7,9,10]',          'major'),
      ('Hungarian Minor',          '[0,2,3,6,7,8,11]',          'minor'),
      ('Romanian Minor',           '[0,2,3,6,7,9,10]',          'minor'),
      ('Spanish',                  '[0,1,3,4,5,7,8,10]',        'minor'),
      ('Spanish 8-Tone',           '[0,1,3,4,5,6,8,10]',        'minor'),
      ('Japanese',                 '[0,2,3,7,8]',               'pentatonic'),
      ('Hirajoshi',                '[0,2,3,7,8]',               'pentatonic'),
      ('Iwato',                    '[0,1,5,6,10]',              'pentatonic'),
      ('In Sen',                   '[0,1,5,7,10]',              'pentatonic'),
      ('Yo',                       '[0,2,5,7,9]',               'pentatonic'),
      ('Ryukyu',                   '[0,4,5,7,11]',              'pentatonic'),
      ('Balinese',                 '[0,1,3,7,8]',               'pentatonic'),
      ('Javanese',                 '[0,1,3,5,7,9,10]',          'exotic'),
      ('Chinese',                  '[0,4,6,7,11]',              'pentatonic'),
      ('Egyptian',                 '[0,2,5,7,10]',              'pentatonic'),
      ('Prometheus',               '[0,2,4,6,9,10]',            'exotic'),
      ('Tritone',                  '[0,1,4,6,7,10]',            'exotic'),
      ('Two Semitone Tritone',     '[0,1,2,6,7,8]',             'exotic'),
      ('Flamenco',                 '[0,1,4,5,7,8,11]',          'exotic'),
      ('Gypsy Major',              '[0,2,3,6,7,8,11]',          'exotic'),
      ('Gypsy Minor',              '[0,2,3,6,7,8,10]',          'minor'),
      ('Byzantine',                '[0,1,4,5,7,8,11]',          'exotic'),
      ('Hawaiian',                 '[0,2,3,5,7,9,11]',          'minor'),
      ('Overtone',                 '[0,2,4,6,7,9,10]',          'major'),
      ('Leading Whole-Tone',       '[0,2,4,6,8,10,11]',         'exotic'),
      ('Altered',                  '[0,1,3,4,6,8,11]',          'minor'),
      ('Acoustic',                 '[0,2,4,6,7,9,10]',          'major'),
      ('Pelog',                    '[0,1,3,6,7]',               'pentatonic');
    )SQL";

    sqlite3_exec (db, scaleSql, nullptr, nullptr, &errMsg);
    if (errMsg) sqlite3_free (errMsg);

    } // end if (count == 0) — chords/scales only inserted when tables were empty

    // =========================================================================
    // 3. CHORD SETS  (216 sets: 18 progressions × 12 keys)
    //    chord_id references:  1=maj  2=m  3=dim  5=sus2  6=sus4
    //                         10=dom7  11=maj7  12=m7  14=m7b5  21=maj9  22=m9
    // =========================================================================

    //  Helper: note names for set naming
    const char* noteNames[12] = { "C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B" };

    // Scale IDs for progressions (1-based from the scales insert above)
    // 1=Major  2=Natural Minor  5=Dorian  6=Phrygian  8=Mixolydian  10=Pent.Major
    // 11=Pent.Minor  12=Blues

    struct ProgEntry { int rootOffset; int chordIdx; }; // rootOffset = semitones from key root

    struct Progression
    {
        const char* genre;
        const char* mood;
        const char* energy;
        int scaleId;                       // references scales table
        std::vector<ProgEntry> entries;    // { rootOffset, chordIdx(1-based) }
    };

    // 18 progressions (entries use 1-based chord IDs from the chords table insert)
    const std::vector<Progression> progs = {
        { "Pop",       "Anthemic",  "High",   1,  {{0,1},{5,1},{7,1},{5,1},{9,2},{5,1},{7,1},{0,1}} },       // I-IV-V-IV-vi-IV-V-I
        { "Pop",       "Emotional", "Medium", 1,  {{0,1},{7,1},{9,2},{5,1},{0,1},{7,1},{9,2},{5,1}} },       // I-V-vi-IV (8 bars)
        { "Pop",       "Chill",     "Low",    1,  {{0,11},{5,11},{2,12},{7,10},{0,11},{5,11},{2,12},{7,10}} }, // Imaj7-IVmaj7-iim7-V7
        { "Pop",       "Summer",    "High",   1,  {{5,1},{7,1},{9,2},{0,1},{5,1},{7,1},{9,2},{0,1}} },       // IV-V-vi-I
        { "Rock",      "Gritty",    "High",   2,  {{0,2},{5,2},{7,1},{0,2},{0,2},{5,2},{7,1},{0,2}} },       // i-iv-V-i
        { "Rock",      "Classic",   "Medium", 8,  {{0,1},{10,1},{5,1},{0,1},{0,1},{10,1},{5,1},{0,1}} },      // I-bVII-IV-I
        { "Jazz",      "Smooth",    "Low",    1,  {{2,12},{7,10},{0,11},{9,12},{2,12},{7,10},{0,11},{9,12}} }, // iim7-V7-Imaj7-vim7
        { "Jazz",      "Bluesy",    "Medium", 12, {{0,10},{5,10},{0,10},{7,10},{0,10},{5,10},{0,10},{7,10}} }, // 12-bar start
        { "Jazz",      "Soul",      "Medium", 1,  {{0,41},{5,41},{2,41},{7,32},{0,41},{5,41},{2,41},{7,32}} }, // I6/9-IV6/9-II6/9-V7#5
        { "Lo-Fi",     "Rainy",     "Low",    5,  {{0,12},{5,12},{7,10},{0,12},{0,12},{5,12},{7,10},{0,12}} }, // im7-ivm7-V7-im7
        { "Lo-Fi",     "Vintage",   "Low",    1,  {{0,21},{5,21},{2,22},{7,29},{0,21},{5,21},{2,22},{7,29}} }, // Imaj9-IVmaj9-iim9-V7b9
        { "R&B",       "Sexy",      "Low",    1,  {{0,21},{9,22},{2,22},{7,29},{0,21},{9,22},{2,22},{7,29}} }, // Imaj9-vim9-iim9-V7b9
        { "Electronic","Hard",      "High",   2,  {{0,2},{3,1},{8,1},{10,1},{0,2},{3,1},{8,1},{10,1}} },      // i-III-VI-VII
        { "Electronic","Deep",      "Medium", 5,  {{0,12},{3,11},{10,10},{5,12},{0,12},{3,11},{10,10},{5,12}} },// im7-IIImaj7-bVII7-ivm7
        { "Cinematic", "Heroic",    "High",   1,  {{0,1},{8,1},{3,1},{7,1},{0,1},{8,1},{3,1},{7,1}} },       // I-VI-III-V
        { "Cinematic", "Sad",       "Low",    2,  {{0,2},{5,2},{8,1},{7,2},{0,2},{5,2},{8,1},{7,2}} },       // i-iv-VI-v
        { "Trap",      "Dark",      "High",   2,  {{0,2},{1,1},{0,2},{1,1},{0,2},{1,1},{3,2},{1,1}} },       // i-bII-i-bII-iv-bII
        { "Blues",     "Standard",  "Medium", 12, {{0,10},{5,10},{0,10},{0,10},{5,10},{5,10},{0,10},{0,10}} }, // 8-bar blues loop
    };

    // Insert chord sets for all 12 keys × 18 progressions
    // First, clear existing entries if any (though seedDatabase usually exits early if chords exist)
    sqlite3_exec (db, "DELETE FROM chord_set_entries;", nullptr, nullptr, nullptr);
    sqlite3_exec (db, "DELETE FROM chord_sets;", nullptr, nullptr, nullptr);

    for (int key = 0; key < 12; ++key)
    {
        for (int pi = 0; pi < (int) progs.size(); ++pi)
        {
            const auto& prog = progs[static_cast<size_t> (pi)];
            const char* rootName = noteNames[key];

            // Build set name
            std::string setName = std::string (rootName) + " " + prog.genre + " " + prog.mood;

            // Insert chord_set row
            std::string insertSet =
                "INSERT INTO chord_sets (name,genre,mood,energy,root_note,scale_id) VALUES ('"
                + setName + "','"
                + prog.genre + "','"
                + prog.mood + "','"
                + prog.energy + "',"
                + std::to_string (key) + ","
                + std::to_string (prog.scaleId) + ");";

            sqlite3_exec (db, insertSet.c_str(), nullptr, nullptr, nullptr);

            // Get the inserted row id
            const sqlite3_int64 setId = sqlite3_last_insert_rowid (db);

            // Insert entries
            for (int pos = 0; pos < (int) prog.entries.size(); ++pos)
            {
                const auto& entry = prog.entries[static_cast<size_t> (pos)];
                const int entryRoot = (key + entry.rootOffset) % 12;

                std::string insertEntry =
                    "INSERT INTO chord_set_entries (chord_set_id,position,root_note,chord_id) VALUES ("
                    + std::to_string (setId) + ","
                    + std::to_string (pos + 1) + ","
                    + std::to_string (entryRoot) + ","
                    + std::to_string (entry.chordIdx) + ");";

                sqlite3_exec (db, insertEntry.c_str(), nullptr, nullptr, nullptr);
            }
        }
    }

    sqlite3_exec (db, "COMMIT;", nullptr, nullptr, nullptr);
    } // end if (!chordsAlreadySeeded)

    // =========================================================================
    // 4. PROGRESSIONS TABLE — seeded independently (own count-guard)

    //    chord_id references (1-based, matches chords INSERT above):
    //     1=maj  2=m  3=dim  4=aug  5=sus2  6=sus4  7=pow
    //     8=maj6  9=m6  10=dom7  11=maj7  12=m7  13=dim7  14=m7b5
    //     20=dom9  21=maj9  22=m9  29=7b9  30=7#9  36=aug7
    //     41=6/9  42=m6/9
    // All progressions seeded in C (root_key=0); the UI transposes.
    // =========================================================================
    {
        // Version stamp: bump this integer any time the seed data changes.
        // We store it in a tiny meta table and force a full reseed on mismatch.
        constexpr int kProgSeedVersion = 5;  // bumped: replaces seed data with researched progressions

        sqlite3_exec(db,
            "CREATE TABLE IF NOT EXISTS prog_seed_meta (key TEXT PRIMARY KEY, val INTEGER);",
            nullptr, nullptr, nullptr);

        int stored_version = -1;
        {
            sqlite3_stmt* vs = nullptr;
            if (sqlite3_prepare_v2(db,
                "SELECT val FROM prog_seed_meta WHERE key='version';",
                -1, &vs, nullptr) == SQLITE_OK)
            {
                if (sqlite3_step(vs) == SQLITE_ROW)
                    stored_version = sqlite3_column_int(vs, 0);
                sqlite3_finalize(vs);
            }
        }

        if (stored_version < kProgSeedVersion)
        {
            // Delete and re-seed with correct root_sequence data
            sqlite3_exec(db, "DELETE FROM progressions;", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

            // Helper lambda: insert one progression row
            auto ins = [&](const char* name, const char* genre, const char* mood, int energy,
                           const char* seq, const char* roots)
            {
                std::string sql =
                    std::string("INSERT OR IGNORE INTO progressions (name,genre,mood,energy,root_key,chord_sequence,root_sequence) VALUES ('")
                    + name + "','" + genre + "','" + mood + "',"
                    + std::to_string(energy) + ",0,'" + seq + "','" + roots + "');";
                sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
            };

            // ---- Hip-Hop (8) ----
            ins("Classic Boom Bap",    "Hip-Hop","Dark",       2,"[12,12,10,11]","[9,2,7,0]");  // Am7-Dm7-G7-Cmaj7
            ins("Trap Mode",           "Hip-Hop","Dark",       3,"[2,1,1,1]",    "[0,8,10,7]"); // Cm-Ab-Bb-G
            ins("Soul Sample",         "Hip-Hop","Melancholic",2,"[12,12,11,10]","[9,2,5,7]");  // Am7-Dm7-Fmaj7-G7
            ins("Dilla Bounce",        "Hip-Hop","Warm",       2,"[11,12,10,22]","[0,5,7,9]");  // Cmaj7-Fm7-G7-Am9
            ins("Golden Era",          "Hip-Hop","Nostalgic",  2,"[11,12,11,10]","[0,9,5,7]");  // Cmaj7-Am7-Fmaj7-G7
            ins("Night Drive",         "Hip-Hop","Dark",       2,"[12,12,10,12]","[9,2,7,9]");  // Am7-Dm7-G7-Am7
            ins("West Coast",          "Hip-Hop","Smooth",     2,"[12,11,12,10]","[2,5,9,7]");  // Dm7-Fmaj7-Am7-G7
            ins("Brooklyn",            "Hip-Hop","Tense",      3,"[12,10,11,12]","[2,7,0,9]");  // Dm7-G7-Cmaj7-Am7

            // ---- Trap (8) ----
            ins("Dark Trap",           "Trap","Dark",          3,"[2,1,1,1]",    "[9,5,7,4]");  // Am-F-G-Em
            ins("Street Menace",       "Trap","Dark",          3,"[2,1,1,1]",    "[0,8,10,7]"); // Cm-Ab-Bb-G
            ins("Night Rider",         "Trap","Dark",          2,"[2,1,1,1]",    "[4,0,2,11]"); // Em-C-D-B
            ins("808 Slide",           "Trap","Dark",          3,"[2,1,1,1]",    "[11,7,9,6]"); // Bm-G-A-F#
            ins("Cold World",          "Trap","Dark",          2,"[2,1,1,1]",    "[2,10,0,9]"); // Dm-Bb-C-A
            ins("Trap Goth",           "Trap","Dark",          3,"[2,1,1,1]",    "[5,1,3,0]");  // Fm-Db-Eb-C
            ins("Melodic Drill",       "Trap","Dark",          2,"[2,1,1,1]",    "[7,3,5,2]");  // Gm-Eb-F-D
            ins("Plug Walk",           "Trap","Dark",          3,"[2,1,1,2]",    "[9,5,0,7]");  // Am-F-C-G

            // ---- R&B (8) ----
            ins("Neo Soul Classic",    "R&B","Smooth",         2,"[21,22,21,26]","[0,9,5,7]");  // Cmaj9-Am9-Fmaj9-G13
            ins("Silk Road",           "R&B","Smooth",         2,"[11,12,12,20]","[5,4,2,7]");  // Fmaj7-Em7-Dm7-G9
            ins("Late Night RnB",      "R&B","Dark",           1,"[11,12,11,20]","[8,5,1,3]");  // Abmaj7-Fm7-Dbmaj7-Eb9
            ins("Velvet Groove",       "R&B","Smooth",         2,"[11,12,11,20]","[7,4,0,2]");  // Gmaj7-Em7-Cmaj7-D9
            ins("Soul Glow",           "R&B","Uplifting",      2,"[11,12,11,20]","[3,0,8,10]"); // Ebmaj7-Cm7-Abmaj7-Bb9
            ins("Midnight Drive",      "R&B","Smooth",         1,"[11,12,11,20]","[10,7,3,5]"); // Bbmaj7-Gm7-Ebmaj7-F9
            ins("Desire",             "R&B","Smooth",          2,"[22,21,21,22]","[2,7,0,9]");  // Dm9-Gmaj9-Cmaj9-Am9
            ins("Golden Hour",         "R&B","Uplifting",      2,"[22,22,21,21]","[9,2,7,0]");  // Am9-Dm9-Gmaj9-Cmaj9

            // ---- Pop (8) ----
            ins("Radio One",           "Pop","Happy",          2,"[1,1,2,1]",    "[0,7,9,5]");  // C-G-Am-F
            ins("Minor Pop",           "Pop","Happy",          2,"[2,1,1,1]",    "[9,5,0,7]");  // Am-F-C-G
            ins("Classic Pop",         "Pop","Happy",          2,"[1,2,1,1]",    "[0,9,5,7]");  // C-Am-F-G
            ins("Four Chord Pop",      "Pop","Happy",          2,"[1,1,1,1]",    "[0,5,7,5]");  // C-F-G-F
            ins("Stadium Anthem",      "Pop","Uplifting",      3,"[1,1,2,1]",    "[0,5,9,7]");  // C-F-Am-G
            ins("G-Pop Bounce",        "Pop","Happy",          2,"[1,1,2,1]",    "[7,2,4,0]");  // G-D-Em-C
            ins("Indie Folk",          "Pop","Chill",          2,"[2,2,1,1]",    "[9,4,7,2]");  // Am-Em-G-D
            ins("Summer Pop",          "Pop","Happy",          3,"[1,1,2,1]",    "[2,9,11,7]"); // D-A-Bm-G

            // ---- Jazz (8) ----
            ins("ii-V-I Jazz",         "Jazz","Smooth",        2,"[12,10,11,11]","[2,7,0,0]");  // Dm7-G7-Cmaj7-Cmaj7
            ins("Rhythm Changes",      "Jazz","Uplifting",     2,"[11,12,12,10]","[0,9,2,7]");  // Cmaj7-Am7-Dm7-G7
            ins("Minor ii-V-i",        "Jazz","Dark",          2,"[14,29,12,12]","[2,7,0,0]");  // Dm7b5-G7b9-Cm7-Cm7
            ins("Jazz Ballad",         "Jazz","Smooth",        1,"[21,22,22,26]","[0,9,2,7]");  // Cmaj9-Am9-Dm9-G13
            ins("Coltrane Cycle",      "Jazz","Intense",       2,"[11,11,11,11]","[0,3,8,11]"); // Cmaj7-Ebmaj7-Abmaj7-Bmaj7
            ins("Bebop Groove",        "Jazz","Smooth",        2,"[22,26,21,22]","[2,7,0,9]");  // Dm9-G13-Cmaj9-Am9
            ins("Gypsy Jazz",          "Jazz","Dark",          2,"[2,10,2,10]",  "[9,4,9,4]");  // Am-E7-Am-E7
            ins("Modal Vamp",          "Jazz","Chill",         1,"[12,12,26,26]","[2,2,7,7]");  // Dm7-Dm7-G13-G13

            // ---- Lo-Fi (8) ----
            ins("Rainy Afternoon",     "Lo-Fi","Nostalgic",    1,"[11,12,11,10]","[0,9,5,7]");  // Cmaj7-Am7-Fmaj7-G7
            ins("Dusty Records",       "Lo-Fi","Nostalgic",    1,"[11,12,12,12]","[5,2,9,4]");  // Fmaj7-Dm7-Am7-Em7
            ins("Hazy Summer",         "Lo-Fi","Dreamy",       1,"[11,12,11,10]","[8,5,1,10]"); // Abmaj7-Fm7-Dbmaj7-Bb7
            ins("Slow Motion",         "Lo-Fi","Chill",        1,"[11,12,11,10]","[7,4,0,2]");  // Gmaj7-Em7-Cmaj7-D7
            ins("Tape Hiss",           "Lo-Fi","Dreamy",       1,"[11,12,11,12]","[10,7,3,0]"); // Bbmaj7-Gm7-Ebmaj7-Cm7
            ins("Warm Vinyl",          "Lo-Fi","Nostalgic",    1,"[12,12,11,10]","[2,9,10,0]"); // Dm7-Am7-Bbmaj7-C7
            ins("Saturday Morning",    "Lo-Fi","Chill",        1,"[12,11,11,10]","[9,5,0,7]");  // Am7-Fmaj7-Cmaj7-G7
            ins("Late Study",          "Lo-Fi","Dreamy",       1,"[11,12,11,10]","[4,1,9,11]"); // Emaj7-C#m7-Amaj7-B7

            // ---- Afrobeats (8) ----
            ins("Lagos Nights",        "Afrobeats","Groovy",   3,"[2,1,1,2]",   "[9,7,5,4]");  // Am-G-F-Em
            ins("Highlife Groove",     "Afrobeats","Groovy",   2,"[11,12,11,10]","[5,2,10,0]"); // Fmaj7-Dm7-Bbmaj7-C7
            ins("Amapiano Flow",       "Afrobeats","Groovy",   3,"[11,12,12,10]","[1,10,3,8]"); // Dbmaj7-Bbm7-Ebm7-Ab7
            ins("Afropop Summer",      "Afrobeats","Happy",    3,"[1,2,1,1]",   "[7,4,0,2]");  // G-Em-C-D
            ins("Naija Groove",        "Afrobeats","Groovy",   2,"[11,12,11,10]","[3,0,8,10]"); // Ebmaj7-Cm7-Abmaj7-Bb7
            ins("Afroswing",           "Afrobeats","Smooth",   2,"[12,12,11,12]","[4,9,2,11]"); // Em7-Am7-Dmaj7-Bm7
            ins("West African Pull",   "Afrobeats","Groovy",   3,"[2,1,1,2]",   "[2,0,10,9]"); // Dm-C-Bb-Am
            ins("Zanku Energy",        "Afrobeats","Groovy",   3,"[2,1,1,1]",   "[0,10,8,7]"); // Cm-Bb-Ab-G

            // ---- EDM (8) ----
            ins("Euphoric Drop",       "EDM","Uplifting",      3,"[2,1,1,1]",   "[9,5,0,7]");  // Am-F-C-G
            ins("Trance Build",        "EDM","Uplifting",      3,"[2,2,1,1]",   "[9,4,5,7]");  // Am-Em-F-G
            ins("House Classic",       "EDM","Groovy",         3,"[2,1,1,2]",   "[2,10,0,9]"); // Dm-Bb-C-Am
            ins("Deep House",          "EDM","Dark",           2,"[2,2,1,1]",   "[5,0,8,3]");  // Fm-Cm-Ab-Eb
            ins("Progressive Lift",    "EDM","Uplifting",      3,"[2,1,1,2]",   "[9,5,7,4]");  // Am-F-G-Em
            ins("Melodic Drop",        "EDM","Dark",           3,"[2,1,1,1]",   "[0,8,3,10]"); // Cm-Ab-Eb-Bb
            ins("Festival Anthem",     "EDM","Uplifting",      3,"[2,1,1,2]",   "[9,5,0,4]");  // Am-F-C-Em
            ins("Tech House",          "EDM","Groovy",         2,"[2,1,1,2]",   "[9,7,5,4]");  // Am-G-F-Em

            // ---- Gospel (8) ----
            ins("Sunday Morning",      "Gospel","Uplifting",   2,"[11,12,11,10]","[0,9,5,7]"); // Cmaj7-Am7-Fmaj7-G7
            ins("Praise Break",        "Gospel","Uplifting",   3,"[11,12,11,10]","[3,0,8,10]");// Ebmaj7-Cm7-Abmaj7-Bb7
            ins("Worship Anthem",      "Gospel","Uplifting",   3,"[1,2,1,1]",   "[7,4,0,2]");  // G-Em-C-D
            ins("Old Time Gospel",     "Gospel","Uplifting",   2,"[1,1,1,1]",   "[5,10,0,5]"); // F-Bb-C-F
            ins("Contemporary Praise", "Gospel","Uplifting",   3,"[1,1,2,1]",   "[2,9,11,7]"); // D-A-Bm-G
            ins("Choir Swell",         "Gospel","Uplifting",   2,"[11,12,11,20]","[10,7,3,5]");// Bbmaj7-Gm7-Ebmaj7-F9
            ins("Spirit Move",         "Gospel","Uplifting",   3,"[1,2,1,1]",   "[0,9,5,7]");  // C-Am-F-G
            ins("Joyful Noise",        "Gospel","Uplifting",   3,"[1,1,1,1]",   "[8,3,10,5]"); // Ab-Eb-Bb-F

            // ---- Cinematic (8) ----
            ins("Epic Rise",           "Cinematic","Intense",  3,"[2,1,1,1]",   "[9,5,0,7]");  // Am-F-C-G
            ins("Dark Tension",        "Cinematic","Dark",     2,"[2,1,1,1]",   "[0,8,10,7]"); // Cm-Ab-Bb-G
            ins("Hero Theme",          "Cinematic","Uplifting",3,"[1,1,2,1]",   "[0,7,9,5]");  // C-G-Am-F
            ins("Emotional Score",     "Cinematic","Melancholic",2,"[21,22,21,20]","[0,9,5,7]");// Cmaj9-Am9-Fmaj9-G9
            ins("Suspense",            "Cinematic","Dark",     2,"[14,29,12,12]","[2,7,0,0]"); // Dm7b5-G7b9-Cm7-Cm7
            ins("Triumph",             "Cinematic","Uplifting",3,"[1,1,2,1]",   "[3,10,0,8]"); // Eb-Bb-Cm-Ab
            ins("Melancholy",          "Cinematic","Melancholic",1,"[2,2,1,1]", "[9,4,5,7]");  // Am-Em-F-G
            ins("Adventure",           "Cinematic","Uplifting",3,"[1,1,2,1]",   "[7,2,4,0]");  // G-D-Em-C

            // ---- Blues (8) ----
            ins("Blues in C",          "Blues","Gritty",       2,"[10,10,10,10]","[0,5,7,0]"); // C7-F7-G7-C7
            ins("Minor Blues",         "Blues","Dark",         2,"[12,12,10,12]","[0,5,7,0]"); // Cm7-Fm7-G7-Cm7
            ins("Texas Shuffle",       "Blues","Gritty",       3,"[10,10,10,10]","[9,2,4,9]"); // A7-D7-E7-A7
            ins("Slow Blues",          "Blues","Melancholic",  1,"[10,10,10,10]","[4,9,11,4]");// E7-A7-B7-E7
            ins("Jump Blues",          "Blues","Groovy",       3,"[10,10,10,10]","[5,10,0,5]");// F7-Bb7-C7-F7
            ins("Jazz Blues",          "Blues","Smooth",       2,"[11,20,12,26]","[0,5,2,7]"); // Cmaj7-F9-Dm7-G13
            ins("Blues Ballad",        "Blues","Melancholic",  1,"[12,12,10,11]","[9,2,7,0]"); // Am7-Dm7-G7-Cmaj7
            ins("Chicago Blues",       "Blues","Gritty",       3,"[10,10,10,10]","[7,0,2,7]"); // G7-C7-D7-G7

            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

            // Only stamp the version if at least one progression was actually inserted.
            // This prevents a repeat of the TASK_024 bug: failed INSERTs silently leaving
            // an empty table but version marked as up-to-date.
            int newCount = 0;
            {
                sqlite3_stmt* cs = nullptr;
                if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM progressions;",
                                       -1, &cs, nullptr) == SQLITE_OK)
                {
                    if (sqlite3_step(cs) == SQLITE_ROW)
                        newCount = sqlite3_column_int(cs, 0);
                    sqlite3_finalize(cs);
                }
            }
            if (newCount > 0)
            {
                sqlite3_exec(db,
                    "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 5);",
                    nullptr, nullptr, nullptr);
            }
        }
    }


} // end seedDatabase()

// =============================================================================
// Database — load into memory
// =============================================================================


void TheoryEngine::loadChordDefinitions()
{
    if (!db) return;
    chordDefs.clear();

    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2 (db,
        "SELECT id, name, symbol, interval_pattern, category, quality FROM chords ORDER BY id;",
        -1, &stmt, nullptr);

    if (rc != SQLITE_OK) return;

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        ChordDefinition cd;
        cd.id       = sqlite3_column_int  (stmt, 0);
        cd.name     = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 1));
        cd.symbol   = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 2));
        cd.intervals = parseIntervalJson (reinterpret_cast<const char*> (sqlite3_column_text (stmt, 3)));
        cd.category = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 4));
        cd.quality  = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 5));
        chordDefs.push_back (std::move (cd));
    }
    sqlite3_finalize (stmt);
}

void TheoryEngine::loadScaleDefinitions()
{
    if (!db) return;
    scaleDefs.clear();

    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2 (db,
        "SELECT id, name, interval_pattern, mode_family FROM scales ORDER BY id;",
        -1, &stmt, nullptr);

    if (rc != SQLITE_OK) return;

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        ScaleDefinition sd;
        sd.id         = sqlite3_column_int  (stmt, 0);
        sd.name       = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 1));
        sd.intervals  = parseIntervalJson (reinterpret_cast<const char*> (sqlite3_column_text (stmt, 2)));
        sd.modeFamily = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 3));
        scaleDefs.push_back (std::move (sd));
    }
    sqlite3_finalize (stmt);
}

void TheoryEngine::loadChordSetListings()
{
    if (!db) return;
    chordSetList.clear();

    sqlite3_stmt* stmt = nullptr;
    const int rc = sqlite3_prepare_v2 (db,
        "SELECT id, name, IFNULL(author,'Wolf Den Factory'), genre, mood, energy, scale_id, IFNULL(root_note,0) "
        "FROM chord_sets ORDER BY id;",
        -1, &stmt, nullptr);

    if (rc != SQLITE_OK) return;

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        ChordSetListing row;
        row.id      = sqlite3_column_int (stmt, 0);
        row.name    = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 1));
        row.author  = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 2));
        if (sqlite3_column_type (stmt, 3) != SQLITE_NULL)
            row.genre = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 3));
        if (sqlite3_column_type (stmt, 4) != SQLITE_NULL)
            row.mood = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 4));
        if (sqlite3_column_type (stmt, 5) != SQLITE_NULL)
            row.energy = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 5));
        row.scaleId  = sqlite3_column_int (stmt, 6);
        row.rootNote = sqlite3_column_int (stmt, 7);
        chordSetList.push_back (std::move (row));
    }
    sqlite3_finalize (stmt);
}

// =============================================================================
// Chord set entries
// =============================================================================

std::vector<ChordSetEntry> TheoryEngine::getChordSetEntries (int setId) const
{
    std::vector<ChordSetEntry> out;
    if (!db || setId <= 0) return out;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT position, root_note, chord_id "
        "FROM chord_set_entries WHERE chord_set_id = ? ORDER BY position;";
    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    sqlite3_bind_int (stmt, 1, setId);
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        ChordSetEntry e;
        e.position  = sqlite3_column_int (stmt, 0);
        e.rootNote  = sqlite3_column_int (stmt, 1);
        e.chordId   = sqlite3_column_int (stmt, 2);
        out.push_back (e);
    }
    sqlite3_finalize (stmt);
    return out;
}

std::vector<ProgressionListing> TheoryEngine::getProgressionListings(const std::string& genre) const
{
    std::vector<ProgressionListing> out;
    if (!db) return out;

    std::string sql = "SELECT id, name, genre, IFNULL(mood,''), energy, root_key, "
                  "chord_sequence, IFNULL(root_sequence,'[0,0,0,0]') "
                  "FROM progressions";
    if (!genre.empty() && genre != "All")
        sql += " WHERE genre = ?";
    sql += " ORDER BY genre ASC, name ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    if (!genre.empty() && genre != "All")
        sqlite3_bind_text(stmt, 1, genre.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ProgressionListing p;
        p.id            = sqlite3_column_int(stmt, 0);
        p.name          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.genre         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        p.mood          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.energy        = sqlite3_column_int(stmt, 4);
        p.rootKey       = sqlite3_column_int(stmt, 5);
        p.chordSequence = parseIntervalJson(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
        p.rootSequence  = parseIntervalJson(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
        // Ensure rootSequence has same length as chordSequence (pad with zeros if old data)
        while ((int)p.rootSequence.size() < (int)p.chordSequence.size())
            p.rootSequence.push_back(0);
        out.push_back(std::move(p));
    }
    sqlite3_finalize(stmt);
    return out;
}

// =============================================================================
// Preset system — load
// =============================================================================

void TheoryEngine::loadPackListings()
{
    if (!db) return;
    packList.clear();

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db,
            "SELECT id, name, author, version, IFNULL(content_path,'') FROM packs ORDER BY id;",
            -1, &stmt, nullptr) != SQLITE_OK)
        return;

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        PackListing p;
        p.id          = sqlite3_column_int  (stmt, 0);
        p.name        = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 1));
        p.author      = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 2));
        p.version     = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 3));
        p.contentPath = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 4));
        packList.push_back (std::move (p));
    }
    sqlite3_finalize (stmt);
}

void TheoryEngine::loadPresetListings()
{
    if (!db) return;
    presetList.clear();

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT p.id, p.name, IFNULL(p.author,''), IFNULL(p.category,''), IFNULL(p.tags,''), "
        "       IFNULL(p.pack_id,0), IFNULL(pk.name,''), IFNULL(p.sample_path,''), "
        "       p.root_note, p.loop_enabled, p.one_shot, "
        "       p.env_attack, p.env_decay, p.env_sustain, p.env_release, "
        "       (p.state_blob IS NULL) as is_factory "
        "FROM presets p "
        "LEFT JOIN packs pk ON pk.id = p.pack_id "
        "ORDER BY p.pack_id ASC, p.category ASC, p.name ASC;";

    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return;

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        PresetListing pr;
        pr.id           = sqlite3_column_int  (stmt, 0);
        pr.name         = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 1));
        pr.author       = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 2));
        pr.category     = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 3));
        pr.tags         = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 4));
        pr.packId       = sqlite3_column_int  (stmt, 5);
        pr.packName     = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 6));
        pr.samplePath   = reinterpret_cast<const char*> (sqlite3_column_text (stmt, 7));
        pr.rootNote     = sqlite3_column_int  (stmt, 8);
        pr.loopEnabled  = sqlite3_column_int  (stmt, 9) != 0;
        pr.oneShot      = sqlite3_column_int  (stmt, 10) != 0;
        pr.envAttack    = (float) sqlite3_column_double (stmt, 11);
        pr.envDecay     = (float) sqlite3_column_double (stmt, 12);
        pr.envSustain   = (float) sqlite3_column_double (stmt, 13);
        pr.envRelease   = (float) sqlite3_column_double (stmt, 14);
        pr.isFactory    = sqlite3_column_int  (stmt, 15) != 0;
        presetList.push_back (std::move (pr));
    }
    sqlite3_finalize (stmt);
}

void TheoryEngine::reloadPresetListings()
{
    loadPackListings();
    loadPresetListings();
}

// =============================================================================
// Preset system — CRUD
// =============================================================================

int TheoryEngine::savePreset (const std::string& name,
                               const std::string& author,
                               const std::string& category,
                               const std::string& tags,
                               int packId,
                               const void* blob,
                               int blobSize)
{
    if (!db) return -1;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO presets (name, author, category, tags, pack_id, state_blob) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return -1;

    sqlite3_bind_text (stmt, 1, name.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 2, author.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 3, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text (stmt, 4, tags.c_str(),     -1, SQLITE_TRANSIENT);

    if (packId > 0)
        sqlite3_bind_int (stmt, 5, packId);
    else
        sqlite3_bind_null (stmt, 5);

    if (blob != nullptr && blobSize > 0)
        sqlite3_bind_blob (stmt, 6, blob, blobSize, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null (stmt, 6);

    sqlite3_step (stmt);
    sqlite3_finalize (stmt);

    const int newId = (int) sqlite3_last_insert_rowid (db);
    loadPresetListings(); // refresh cache
    return newId;
}

juce::MemoryBlock TheoryEngine::loadPresetBlob (int presetId) const
{
    if (!db) return {};

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db,
            "SELECT state_blob FROM presets WHERE id = ?;",
            -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    sqlite3_bind_int (stmt, 1, presetId);

    juce::MemoryBlock result;
    if (sqlite3_step (stmt) == SQLITE_ROW && sqlite3_column_type (stmt, 0) != SQLITE_NULL)
    {
        const int bytes = sqlite3_column_bytes (stmt, 0);
        if (bytes > 0)
            result.append (sqlite3_column_blob (stmt, 0), (size_t) bytes);
    }
    sqlite3_finalize (stmt);
    return result;
}

void TheoryEngine::deletePreset (int presetId)
{
    if (!db) return;

    // Guard: do not allow deleting factory presets (pack_id = 1, state_blob IS NULL)
    sqlite3_stmt* check = nullptr;
    if (sqlite3_prepare_v2 (db,
            "SELECT (state_blob IS NULL) FROM presets WHERE id = ?;",
            -1, &check, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int (check, 1, presetId);
        if (sqlite3_step (check) == SQLITE_ROW && sqlite3_column_int (check, 0) != 0)
        {
            sqlite3_finalize (check);
            return; // factory preset — not deletable
        }
        sqlite3_finalize (check);
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db,
            "DELETE FROM presets WHERE id = ?;",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int (stmt, 1, presetId);
        sqlite3_step (stmt);
        sqlite3_finalize (stmt);
    }
    loadPresetListings();
}

// =============================================================================
// Preset system — factory seeding
// =============================================================================

void TheoryEngine::seedFactoryPresets (const juce::File& contentDir)
{
    if (!db) return;

    // -------------------------------------------------------------------------
    // 1. Ensure factory pack row exists (id = 1)
    // -------------------------------------------------------------------------
    sqlite3_exec (db,
        "INSERT OR IGNORE INTO packs (id, name, author, version, content_path) "
        "VALUES (1, 'Wolf''s Den Factory', 'Wolf Productions', '1.0', '');",
        nullptr, nullptr, nullptr);

    // -------------------------------------------------------------------------
    // 2. If factory presets already seeded, skip
    // -------------------------------------------------------------------------
    {
        sqlite3_stmt* s = nullptr;
        int count = 0;
        if (sqlite3_prepare_v2 (db,
                "SELECT COUNT(*) FROM presets WHERE pack_id = 1;",
                -1, &s, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step (s) == SQLITE_ROW)
                count = sqlite3_column_int (s, 0);
            sqlite3_finalize (s);
        }
        if (count > 0)
        {
            // Update the stored content_path to match current bundle location
            sqlite3_stmt* upd = nullptr;
            if (sqlite3_prepare_v2 (db,
                    "UPDATE packs SET content_path = ? WHERE id = 1;",
                    -1, &upd, nullptr) == SQLITE_OK)
            {
                const std::string cp = contentDir.getFullPathName().toStdString();
                sqlite3_bind_text (upd, 1, cp.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step (upd);
                sqlite3_finalize (upd);
            }
            loadPackListings();
            loadPresetListings();
            return;
        }
    }

    // Store absolute path so loader can resolve WAV files
    {
        sqlite3_stmt* upd = nullptr;
        if (sqlite3_prepare_v2 (db,
                "UPDATE packs SET content_path = ? WHERE id = 1;",
                -1, &upd, nullptr) == SQLITE_OK)
        {
            const std::string cp = contentDir.getFullPathName().toStdString();
            sqlite3_bind_text (upd, 1, cp.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step (upd);
            sqlite3_finalize (upd);
        }
    }

    // -------------------------------------------------------------------------
    // 3. Seed 41 factory presets (state_blob = NULL → recipe-based load)
    // -------------------------------------------------------------------------
    struct FactoryDef
    {
        const char* name;
        const char* category;
        const char* relPath;
        int rootNote;
        int loop;    // 1=loop, 0=one-shot
        int oneShot; // 1=one-shot trigger
        float atk, dec, sus, rel;
    };

    static const FactoryDef kFactory[] =
    {
        // Bass — root C2 (36), loop, punchy envelope
        { "Concrete Flex",   "Bass",      "Bass/Concrete Flex.wav",   36, 1, 0, 0.002f, 0.25f, 0.70f, 0.25f },
        { "Graveyard 808",   "Bass",      "Bass/Graveyard 808.wav",   36, 1, 0, 0.002f, 0.30f, 0.75f, 0.30f },
        { "Low End Phantom", "Bass",      "Bass/Low End Phantom.wav", 36, 1, 0, 0.003f, 0.35f, 0.70f, 0.35f },
        { "Night Stalker",   "Bass",      "Bass/Night Stalker.wav",   36, 1, 0, 0.002f, 0.25f, 0.65f, 0.20f },

        // Horns — root C4 (60), loop, wind-style envelope
        { "Midnight Horn",   "Horns",     "Horns/Midnight Horn.wav",  60, 1, 0, 0.015f, 0.20f, 0.80f, 0.20f },
        { "Street Brass",    "Horns",     "Horns/Street Brass.wav",   60, 1, 0, 0.010f, 0.15f, 0.85f, 0.15f },
        { "War Call",        "Horns",     "Horns/War Call.wav",       60, 1, 0, 0.008f, 0.18f, 0.82f, 0.18f },

        // Keys — root C4 (60), natural decay
        { "Accustic Keys",   "Keys",      "Keys/Accustic Keys.wav",   60, 0, 1, 0.003f, 0.50f, 0.00f, 0.40f },
        { "Afterhours Suite","Keys",      "Keys/Afterhours Suite.wav",60, 0, 1, 0.004f, 0.55f, 0.00f, 0.50f },
        { "Grand Ivory",     "Keys",      "Keys/Grand Ivory.wav",     60, 0, 1, 0.002f, 0.60f, 0.00f, 0.60f },
        { "Midnight Organ",  "Keys",      "Keys/Midnight Organ.wav",  60, 1, 0, 0.008f, 0.20f, 0.90f, 0.30f },
        { "Rhodes",          "Keys",      "Keys/Rhodes.wav",          60, 0, 1, 0.003f, 0.45f, 0.00f, 0.45f },
        { "Velvet Room",     "Keys",      "Keys/Velvet Room.wav",     60, 1, 0, 0.006f, 0.25f, 0.85f, 0.35f },

        // Leads — root C4 (60), fast attack, sustain
        { "Chrome Runner",   "Leads",     "Leads/Chrome Runner.wav",  60, 1, 0, 0.003f, 0.10f, 0.90f, 0.15f },
        { "Dark Star",       "Leads",     "Leads/Dark Star.wav",      60, 1, 0, 0.004f, 0.12f, 0.88f, 0.18f },
        { "Howling Lead",    "Leads",     "Leads/Howling Lead.wav",   60, 1, 0, 0.005f, 0.10f, 0.92f, 0.20f },
        { "Midnight Siren",  "Leads",     "Leads/Midnight Siren.wav", 60, 1, 0, 0.006f, 0.12f, 0.88f, 0.22f },
        { "Neon Blade",      "Leads",     "Leads/Neon Blade.wav",     60, 1, 0, 0.003f, 0.08f, 0.92f, 0.12f },
        { "Street Prophet",  "Leads",     "Leads/Street Prophet.wav", 60, 1, 0, 0.004f, 0.10f, 0.90f, 0.15f },
        { "Wolf Cry",        "Leads",     "Leads/Wolf Cry.wav",       60, 1, 0, 0.005f, 0.12f, 0.88f, 0.20f },

        // Pads — root C4 (60), slow attack, long release
        { "Black Cathedral", "Pads",      "Pads/Black Cathedral.wav", 60, 1, 0, 0.40f,  1.20f, 0.85f, 2.00f },
        { "Frozen Horizon",  "Pads",      "Pads/Frozen Horizon.wav",  60, 1, 0, 0.50f,  1.00f, 0.88f, 2.50f },
        { "Low Gravity",     "Pads",      "Pads/Low Gravity.wav",     60, 1, 0, 0.35f,  0.80f, 0.82f, 1.80f },
        { "Neon Halo",       "Pads",      "Pads/Neon Halo.wav",       60, 1, 0, 0.30f,  0.90f, 0.85f, 2.00f },

        // Plucks — root C4 (60), instant attack, fast decay, one-shot
        { "Frostbite",       "Plucks",    "Plucks/Frostbite.wav",     60, 0, 1, 0.001f, 0.35f, 0.00f, 0.25f },
        { "Ghost Notes",     "Plucks",    "Plucks/Ghost Notes.wav",   60, 0, 1, 0.001f, 0.40f, 0.00f, 0.30f },
        { "Hollow Picc",     "Plucks",    "Plucks/Hollow Picc.wav",   60, 0, 1, 0.001f, 0.30f, 0.00f, 0.20f },
        { "Ice Fang",        "Plucks",    "Plucks/Ice Fang.wav",      60, 0, 1, 0.001f, 0.28f, 0.00f, 0.18f },
        { "Pulse Drop",      "Plucks",    "Plucks/Pulse Drop.wav",    60, 0, 1, 0.001f, 0.32f, 0.00f, 0.22f },
        { "Silver Accustic", "Plucks",    "Plucks/Silver Accustic.wav",60,0, 1, 0.001f, 0.38f, 0.00f, 0.28f },
        { "Wireframe",       "Plucks",    "Plucks/Wireframe.wav",     60, 0, 1, 0.001f, 0.25f, 0.00f, 0.15f },

        // Strings — root C4 (60), bowed attack, loop sustain
        { "Broken Cello",    "Strings",   "Strings/Broken Cello.wav", 60, 1, 0, 0.06f,  0.40f, 0.85f, 0.80f },
        { "Cold Arco",       "Strings",   "Strings/Cold Arco.wav",    60, 1, 0, 0.08f,  0.35f, 0.88f, 0.70f },
        { "Midnight Strings","Strings",   "Strings/Midnight Strings.wav",60,1,0,0.10f,  0.50f, 0.85f, 1.00f },
        { "Street Cello",    "Strings",   "Strings/Street Cello.wav", 60, 1, 0, 0.07f,  0.40f, 0.82f, 0.75f },
        { "Velvet Violin",   "Strings",   "Strings/Velvet Violin.wav",60, 1, 0, 0.05f,  0.35f, 0.88f, 0.65f },

        // Woodwinds — root C4 (60), breath attack, loop
        { "Lonely Sax",      "Woodwinds", "Woodwinds/Lonely Sax.wav",    60, 1, 0, 0.02f, 0.20f, 0.85f, 0.25f },
        { "Midnight Flute",  "Woodwinds", "Woodwinds/Midnight Flute.wav",60, 1, 0, 0.02f, 0.18f, 0.88f, 0.22f },
        { "Silver Reed",     "Woodwinds", "Woodwinds/Silver Reed.wav",   60, 1, 0, 0.018f,0.22f, 0.85f, 0.28f },
        { "Smoked Oboe",     "Woodwinds", "Woodwinds/Smoked Oboe.wav",   60, 1, 0, 0.015f,0.20f, 0.85f, 0.25f },
        { "Urban Bassoon",   "Woodwinds", "Woodwinds/Urban Bassoon.wav", 60, 1, 0, 0.025f,0.25f, 0.82f, 0.30f },
    };

    sqlite3_exec (db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    sqlite3_stmt* ins = nullptr;
    const char* insSql =
        "INSERT INTO presets "
        "(name, author, category, pack_id, sample_path, root_note, loop_enabled, one_shot, "
        " env_attack, env_decay, env_sustain, env_release, state_blob) "
        "VALUES (?, 'Wolf Productions', ?, 1, ?, ?, ?, ?, ?, ?, ?, ?, NULL);";

    if (sqlite3_prepare_v2 (db, insSql, -1, &ins, nullptr) == SQLITE_OK)
    {
        for (const auto& def : kFactory)
        {
            sqlite3_reset (ins);
            sqlite3_bind_text (ins, 1,  def.name,     -1, SQLITE_STATIC);
            sqlite3_bind_text (ins, 2,  def.category, -1, SQLITE_STATIC);
            sqlite3_bind_text (ins, 3,  def.relPath,  -1, SQLITE_STATIC);
            sqlite3_bind_int  (ins, 4,  def.rootNote);
            sqlite3_bind_int  (ins, 5,  def.loop);
            sqlite3_bind_int  (ins, 6,  def.oneShot);
            sqlite3_bind_double (ins, 7,  (double) def.atk);
            sqlite3_bind_double (ins, 8,  (double) def.dec);
            sqlite3_bind_double (ins, 9,  (double) def.sus);
            sqlite3_bind_double (ins, 10, (double) def.rel);
            sqlite3_step (ins);
        }
        sqlite3_finalize (ins);
    }

    sqlite3_exec (db, "COMMIT;", nullptr, nullptr, nullptr);

    loadPackListings();
    loadPresetListings();
}

// =============================================================================
// Static helpers
// =============================================================================

// static
std::vector<int> TheoryEngine::parseIntervalJson (const std::string& json)
{
    return parseIntervalJsonImpl (json);
}

// static
std::string TheoryEngine::buildIntervalJson (const std::vector<int>& iv)
{
    return buildIntervalJsonImpl (iv);
}

} // namespace wolfsden