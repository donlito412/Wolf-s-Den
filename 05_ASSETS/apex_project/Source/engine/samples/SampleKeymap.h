#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace wolfsden
{

struct SampleZone
{
    juce::File    file;
    int           rootNote    = 60;  // MIDI note this sample was recorded at
    int           loNote      = 0;   // lowest MIDI note this zone covers
    int           hiNote      = 127; // highest MIDI note this zone covers
    int           loVel       = 0;   // lowest velocity this zone covers (0–127)
    int           hiVel       = 127; // highest velocity this zone covers (0–127)
    float         startFrac   = 0.f;
    float         endFrac     = 1.f;
    bool          loopEnabled = true;
    bool          oneShot     = false;
    
    // For serialization
    juce::ValueTree toValueTree() const
    {
        juce::ValueTree tree("SampleZone");
        tree.setProperty("filePath", file.getFullPathName(), nullptr);
        tree.setProperty("rootNote", rootNote, nullptr);
        tree.setProperty("loNote", loNote, nullptr);
        tree.setProperty("hiNote", hiNote, nullptr);
        tree.setProperty("loVel", loVel, nullptr);
        tree.setProperty("hiVel", hiVel, nullptr);
        tree.setProperty("startFrac", startFrac, nullptr);
        tree.setProperty("endFrac", endFrac, nullptr);
        tree.setProperty("loopEnabled", loopEnabled, nullptr);
        tree.setProperty("oneShot", oneShot, nullptr);
        return tree;
    }
    
    void fromValueTree(const juce::ValueTree& tree)
    {
        if (tree.hasType("SampleZone"))
        {
            file = juce::File(tree.getProperty("filePath", ""));
            rootNote = tree.getProperty("rootNote", 60);
            loNote = tree.getProperty("loNote", 0);
            hiNote = tree.getProperty("hiNote", 127);
            loVel = tree.getProperty("loVel", 0);
            hiVel = tree.getProperty("hiVel", 127);
            startFrac = tree.getProperty("startFrac", 0.f);
            endFrac = tree.getProperty("endFrac", 1.f);
            loopEnabled = tree.getProperty("loopEnabled", true);
            oneShot = tree.getProperty("oneShot", false);
        }
    }
};

class SampleKeymap
{
public:
    void addZone(const SampleZone& zone);
    void clearZones();
    int  zoneCount() const;
    
    // Find best zone for a given note + velocity.
    // Priority: exact note range match → nearest root → any zone.
    // Returns nullptr if no zones loaded.
    const SampleZone* findZone(int midiNote, int velocity) const noexcept;
    
    // Serialize/deserialize for preset save
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree&);
    
private:
    std::vector<SampleZone> zones;
};

} // namespace wolfsden
