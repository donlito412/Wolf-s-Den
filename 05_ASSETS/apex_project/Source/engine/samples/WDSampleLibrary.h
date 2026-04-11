#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace wolfsden
{

/** Metadata for one audio sample in the Wolf's Den library.
 *
 *  Audio data is NOT stored here — only file location and display info.
 *  IDs are assigned sequentially at scan time; they are NOT persisted to
 *  disk, so always look samples up by filePath across restarts.
 */
struct SampleEntry
{
    int          id       = 0;
    juce::String name;       ///< Display name, e.g. "Dark Sub"
    juce::String filePath;   ///< Absolute path on disk
    juce::String category;   ///< "Bass" | "Keys" | "Pads" | "Leads" | "Plucks" | "Strings" | "Horns" | "Woodwinds" | "FX" | "Loops"
    juce::String packId;     ///< "factory" or expansion pack slug
    int          rootNote = 60;    ///< MIDI note that plays at original pitch (C4 = 60)
    float        bpm      = 0.f;   ///< Loop tempo (0 = not a loop / don't sync to host)
    bool         isLoop    = false; ///< Loop sample end-to-start while note held
    bool         isOneShot = false; ///< Play to end regardless of note-off
};

/** Central metadata registry for all Wolf's Den audio samples.
 *
 *  Call scanAsync() once on startup.  The engine and browser both access
 *  the registry through getAll() / findById() / filter() — all thread-safe.
 *
 *  Search paths (in order):
 *    0. Plugin bundle …/Contents/Resources/Factory/ (CMake ships DistributionContent here)
 *    1. {userDocuments}/WolfsDen/Samples/Factory/
 *    2. {userDocuments}/WolfsDen/Samples/User/
 *    3. {userDocuments}/WolfsDen/Expansions/{packId}/Samples/
 *
 *  Each WAV may have a companion .wdsample JSON sidecar:
 *    { "name":"808 Kick","category":"Bass","rootNote":36,"isOneShot":true,"bpm":0 }
 *  If no sidecar, metadata is derived from the directory name (category) and
 *  filename stem (display name).
 */
class WDSampleLibrary
{
public:
    WDSampleLibrary();
    ~WDSampleLibrary() = default;

    /** Scan all search paths on a background thread.
     *  onComplete is called on the message thread when scanning finishes.
     *  Safe to call multiple times (re-scans the library).
     */
    void scanAsync (std::function<void()> onComplete = {});

    /** Synchronous scan — call only from a non-audio, non-message thread. */
    void scanLibrary();

    /** Returns every known sample.  Thread-safe (copies under lock). */
    std::vector<SampleEntry> getAll() const;

    /** Filtered view.  Pass empty strings to skip that filter. */
    std::vector<SampleEntry> filter (const juce::String& category,
                                     const juce::String& packId,
                                     const juce::String& searchText) const;

    /** Returns nullptr if id not found. */
    const SampleEntry* findById (int id) const;

    /** Returns absolute path for an id, or empty string if not found. */
    juce::String pathForId (int id) const;

    /** Returns how many samples are currently registered. */
    int size() const;

    /** True once at least one scan has completed successfully. */
    bool isReady() const noexcept { return ready.load(); }

    /** Base factory samples directory: {userDocuments}/WolfsDen/Samples/Factory/ */
    static juce::File factoryRoot();

    /** User samples directory: {userDocuments}/WolfsDen/Samples/User/ */
    static juce::File userRoot();

    /** Expansion pack samples root: {userDocuments}/WolfsDen/Expansions/ */
    static juce::File expansionsRoot();

    /** Call from processor startup: bundle path from …/Contents/Resources/Factory */
    void setBundleFactoryDirectory (juce::File directory)
    {
        bundleFactoryDir_ = std::move (directory);
    }

private:
    void scanDirectory (const juce::File& dir, const juce::String& packId,
                        std::vector<SampleEntry>& out, int& nextId);

    SampleEntry entryFromFile (const juce::File& wavFile,
                               const juce::String& category,
                               const juce::String& packId,
                               int id) const;

    /** Parse root note from filename, e.g. "My Sample C4.wav" → 60.
     *  Returns -1 if no note found. */
    static int parseRootNoteFromFilename (const juce::String& stem);

    mutable std::mutex         mutex_;
    std::vector<SampleEntry>   entries_;
    std::atomic<bool>          ready { false };
    std::unique_ptr<juce::ThreadPool> threadPool_;
    juce::File                 bundleFactoryDir_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WDSampleLibrary)
};

} // namespace wolfsden
