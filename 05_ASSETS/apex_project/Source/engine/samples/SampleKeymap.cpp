#include "SampleKeymap.h"

namespace wolfsden
{

void SampleKeymap::addZone(const SampleZone& zone)
{
    zones.push_back(zone);
}

void SampleKeymap::clearZones()
{
    zones.clear();
}

int SampleKeymap::zoneCount() const
{
    return static_cast<int>(zones.size());
}

const SampleZone* SampleKeymap::findZone(int midiNote, int velocity) const noexcept
{
    if (zones.empty())
        return nullptr;
    
    const SampleZone* bestMatch = nullptr;
    int bestScore = -1;
    
    for (const auto& zone : zones)
    {
        // Check if note and velocity are within this zone's range
        bool noteInRange = (midiNote >= zone.loNote && midiNote <= zone.hiNote);
        bool velInRange = (velocity >= zone.loVel && velocity <= zone.hiVel);
        
        if (noteInRange && velInRange)
        {
            // Perfect match - both note and velocity in range
            return &zone;
        }
        else if (noteInRange)
        {
            // Note matches but velocity doesn't - score this as secondary option
            int score = 1;
            
            // Prefer zones with velocity ranges closer to the played velocity
            if (velocity < zone.loVel)
                score += (zone.loVel - velocity);
            else if (velocity > zone.hiVel)
                score += (velocity - zone.hiVel);
            
            if (bestMatch == nullptr || score < bestScore)
            {
                bestMatch = &zone;
                bestScore = score;
            }
        }
        else
        {
            // No note match - calculate distance to nearest root note
            int noteDistance = std::abs(midiNote - zone.rootNote);
            int score = noteDistance * 100; // Much lower priority than note range matches
            
            if (bestMatch == nullptr || score < bestScore)
            {
                bestMatch = &zone;
                bestScore = score;
            }
        }
    }
    
    return bestMatch;
}

juce::ValueTree SampleKeymap::toValueTree() const
{
    juce::ValueTree tree("SampleKeymap");
    
    for (const auto& zone : zones)
    {
        tree.appendChild(zone.toValueTree(), nullptr);
    }
    
    return tree;
}

void SampleKeymap::fromValueTree(const juce::ValueTree& tree)
{
    zones.clear();
    
    if (tree.hasType("SampleKeymap"))
    {
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            SampleZone zone;
            zone.fromValueTree(tree.getChild(i));
            zones.push_back(zone);
        }
    }
}

} // namespace wolfsden
