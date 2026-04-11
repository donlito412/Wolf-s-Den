#include "WDSampleLibrary.h"

#include <juce_events/juce_events.h>

namespace wolfsden
{

//==============================================================================
// Static helpers

juce::File WDSampleLibrary::factoryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("WolfsDen/Samples/Factory");
}

juce::File WDSampleLibrary::userRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("WolfsDen/Samples/User");
}

juce::File WDSampleLibrary::expansionsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("WolfsDen/Expansions");
}

//==============================================================================
// Construction

WDSampleLibrary::WDSampleLibrary()
    : threadPool_ (std::make_unique<juce::ThreadPool> (1)) {}

//==============================================================================
// Scan

void WDSampleLibrary::scanAsync (std::function<void()> onComplete)
{
    threadPool_->addJob ([this, cb = std::move (onComplete)] {
        scanLibrary();
        if (cb)
            juce::MessageManager::callAsync (cb);
    });
}

void WDSampleLibrary::scanLibrary()
{
    std::vector<SampleEntry> fresh;
    fresh.reserve (256);
    int nextId = 1;

    // 0. Factory WAVs shipped inside the plugin bundle
    if (bundleFactoryDir_.isDirectory())
        scanDirectory (bundleFactoryDir_, "factory", fresh, nextId);

    // 1. Factory samples under Documents (user / installer copies)
    scanDirectory (factoryRoot(), "factory", fresh, nextId);

    // 2. User samples
    scanDirectory (userRoot(), "user", fresh, nextId);

    // 3. Expansion packs — each direct child of expansionsRoot is a pack slug
    const auto expRoot = expansionsRoot();
    if (expRoot.isDirectory())
    {
        for (const auto& packDir : expRoot.findChildFiles (juce::File::findDirectories, false))
        {
            const juce::String slug = packDir.getFileName();
            const juce::File samplesDir = packDir.getChildFile ("Samples");
            if (samplesDir.isDirectory())
                scanDirectory (samplesDir, slug, fresh, nextId);
        }
    }

    {
        std::lock_guard<std::mutex> lk (mutex_);
        entries_ = std::move (fresh);
    }
    ready.store (true);
}

void WDSampleLibrary::scanDirectory (const juce::File& dir,
                                     const juce::String& packId,
                                     std::vector<SampleEntry>& out,
                                     int& nextId)
{
    if (! dir.isDirectory())
        return;

    // Each immediate child directory is treated as a category
    for (const auto& categoryDir : dir.findChildFiles (juce::File::findDirectories, false))
    {
        const juce::String category = categoryDir.getFileName();

        auto addWav = [&] (const juce::File& wav, const juce::String& cat)
        {
            if (wav.hasFileExtension (".wav") || wav.hasFileExtension (".WAV"))
                out.push_back (entryFromFile (wav, cat, packId, nextId++));
        };

        for (const auto& f : categoryDir.findChildFiles (juce::File::findFiles, false))
            addWav (f, category);
    }

    // WAVs sitting directly in dir (no category sub-folder → "General")
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false))
    {
        if (f.hasFileExtension (".wav") || f.hasFileExtension (".WAV"))
            out.push_back (entryFromFile (f, "General", packId, nextId++));
    }
}

SampleEntry WDSampleLibrary::entryFromFile (const juce::File& wavFile,
                                            const juce::String& category,
                                            const juce::String& packId,
                                            int id) const
{
    SampleEntry e;
    e.id       = id;
    e.filePath = wavFile.getFullPathName();
    e.category = category;
    e.packId   = packId;

    // Try sidecar first (.wdsample or legacy .hwsample both accepted)
    for (auto ext : { ".wdsample", ".hwsample" })
    {
        const juce::File sidecar = wavFile.withFileExtension (ext);
        if (sidecar.existsAsFile())
        {
            if (auto json = juce::JSON::parse (sidecar))
            {
                e.name      = json.getProperty ("name", wavFile.getFileNameWithoutExtension()).toString();
                e.category  = json.getProperty ("category", category).toString();
                e.rootNote  = (int) (double) json.getProperty ("rootNote", 60);
                e.bpm       = (float) (double) json.getProperty ("bpm", 0.0);
                e.isLoop    = (bool) json.getProperty ("isLoop", false);
                e.isOneShot = (bool) json.getProperty ("isOneShot", false);
                return e;
            }
        }
    }

    // Fallback: derive from filename
    const juce::String stem = wavFile.getFileNameWithoutExtension();
    e.name = stem;

    const int parsedRoot = parseRootNoteFromFilename (stem);
    e.rootNote = (parsedRoot >= 0) ? parsedRoot : 60;

    // Heuristics for one-shot vs loop
    const juce::String lower = stem.toLowerCase();
    e.isLoop    = lower.contains ("loop") || lower.contains ("lp ");
    e.isOneShot = ! e.isLoop && (category.equalsIgnoreCase ("FX") ||
                                 lower.contains ("hit")  ||
                                 lower.contains ("stab") ||
                                 lower.contains ("one shot"));
    return e;
}

//==============================================================================
// Root-note parser

int WDSampleLibrary::parseRootNoteFromFilename (const juce::String& stem)
{
    static const char* noteNames[]    = { "C","D","E","F","G","A","B" };
    static const int   pitchClasses[] = {   0,  2,  4,  5,  7,  9, 11 };

    const int len = stem.length();
    for (int i = 0; i < len; ++i)
    {
        const juce::juce_wchar ch = juce::CharacterFunctions::toUpperCase (stem[i]);
        int pc = -1;
        for (int n = 0; n < 7; ++n)
            if (ch == (juce::juce_wchar) noteNames[n][0]) { pc = pitchClasses[n]; break; }
        if (pc < 0) continue;

        if (i > 0)
        {
            const juce::juce_wchar prev = stem[i - 1];
            if (prev != ' ' && prev != '_' && prev != '-') continue;
        }

        int j = i + 1;
        if (j < len)
        {
            const juce::juce_wchar acc = stem[j];
            if (acc == '#' || acc == 's') { pc = (pc + 1) % 12; ++j; }
            else if (acc == 'b' || acc == 'B')
            {
                if (j + 1 < len && juce::CharacterFunctions::isDigit (stem[j + 1]))
                { pc = (pc + 11) % 12; ++j; }
            }
        }

        if (j < len && juce::CharacterFunctions::isDigit (stem[j]))
        {
            const int octave = (int) (stem[j] - '0');
            const int midi   = (octave + 1) * 12 + pc;
            if (midi >= 0 && midi <= 127)
                return midi;
        }
    }
    return -1;
}

//==============================================================================
// Public accessors

std::vector<SampleEntry> WDSampleLibrary::getAll() const
{
    std::lock_guard<std::mutex> lk (mutex_);
    return entries_;
}

std::vector<SampleEntry> WDSampleLibrary::filter (const juce::String& category,
                                                   const juce::String& packId,
                                                   const juce::String& searchText) const
{
    std::lock_guard<std::mutex> lk (mutex_);
    std::vector<SampleEntry> result;
    result.reserve (entries_.size());

    const juce::String lowerSearch = searchText.toLowerCase();

    for (const auto& e : entries_)
    {
        if (! category.isEmpty()   && ! e.category.equalsIgnoreCase (category))      continue;
        if (! packId.isEmpty()     && e.packId != packId)                             continue;
        if (! searchText.isEmpty() && ! e.name.toLowerCase().contains (lowerSearch))  continue;
        result.push_back (e);
    }
    return result;
}

const SampleEntry* WDSampleLibrary::findById (int id) const
{
    for (const auto& e : entries_)
        if (e.id == id) return &e;
    return nullptr;
}

juce::String WDSampleLibrary::pathForId (int id) const
{
    std::lock_guard<std::mutex> lk (mutex_);
    const SampleEntry* e = findById (id);
    return e ? e->filePath : juce::String{};
}

int WDSampleLibrary::size() const
{
    std::lock_guard<std::mutex> lk (mutex_);
    return (int) entries_.size();
}

} // namespace wolfsden
