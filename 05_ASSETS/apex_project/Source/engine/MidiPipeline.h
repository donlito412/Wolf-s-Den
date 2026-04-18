#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace wolfsden
{

class TheoryEngine;

/**
 * Keys Lock → Chord Mode → Arpeggiator → output MIDI for SynthEngine.
 * Real-time safe: no heap allocation in process().
 */
class MidiPipeline
{
public:
    MidiPipeline() = default;
    ~MidiPipeline() = default;

    MidiPipeline(const MidiPipeline&) = delete;
    MidiPipeline& operator=(const MidiPipeline&) = delete;

    void prepare(double sampleRate, int maxBlock, juce::AudioProcessorValueTreeState& apvts);
    void reset() noexcept;

    /** Replace \a midi contents with processed events (same timebase / numSamples). */
    void process(juce::MidiBuffer& midi,
                 int numSamples,
                 double sampleRate,
                 juce::AudioPlayHead* playHead,
                 const TheoryEngine& theory,
                 juce::AudioProcessorValueTreeState& apvts,
                 juce::MidiKeyboardState* keyboardState = nullptr) noexcept;

    void setChordModeEnabled(bool enabled) noexcept;
    void setKeysLockMode(int modeIndex) noexcept;
    void setTheoryVoiceLeadingEnabled(bool enabled) noexcept;

private:
    struct Ptrs
    {
        std::atomic<float>* keysLockMode = nullptr;
        std::atomic<float>* chordMode = nullptr;
        std::atomic<float>* chordType = nullptr;
        std::atomic<float>* chordRootAnchor = nullptr;
        std::atomic<float>* chordOctaveShift = nullptr;
        std::atomic<float>* chordDensity = nullptr;
        std::atomic<float>* chordInversion = nullptr;
        std::atomic<float>* arpOn = nullptr;
        std::atomic<float>* arpRate = nullptr;
        std::atomic<float>* arpPattern = nullptr;
        std::atomic<float>* arpLatch = nullptr;
        std::atomic<float>* arpSwing = nullptr;
        std::atomic<float>* arpOctaves = nullptr;
        std::atomic<float>* arpSyncPpq = nullptr;
        std::atomic<float>* voiceLeading = nullptr;  // cached to avoid hot-path APVTS string lookup
    };

    struct ArpStepPtrs
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* vel = nullptr;
        std::atomic<float>* dur = nullptr;
        std::atomic<float>* trn = nullptr;
        std::atomic<float>* rkt = nullptr;
    };

    static constexpr int kMaxMap = 16;
    static constexpr int kMaxChordTones = 8;
    static constexpr int kArpSteps = 32;
    static constexpr int kMaxPool = 16;

    struct ChordMapSlot
    {
        bool used = false;
        uint8_t source = 0;
        uint8_t nOut = 0;
        std::array<uint8_t, kMaxChordTones> outs {};
    };

    enum class ArpPattern : int
    {
        Up = 0,
        Down,
        UpDown,
        Order,
        Chord,
        Random,
        numPatterns
    };

    void bindPointers(juce::AudioProcessorValueTreeState& apvts);

    int readKeysLockMode() const noexcept;
    bool readChordMode() const noexcept;
    int readChordTypeIndex() const noexcept;
    int readChordRootAnchor() const noexcept;
    int readChordOctaveShift() const noexcept;
    int readChordDensity() const noexcept;
    int readChordInversion() const noexcept;
    bool readArpOn() const noexcept;
    float readArpRate() const noexcept;
    ArpPattern readArpPattern() const noexcept;
    bool readArpLatch() const noexcept;
    float readArpSwing() const noexcept;
    int readArpOctaves() const noexcept;

    std::optional<int> applyKeysLock(int note,
                                      int keysMode,
                                      const std::array<int, 128>& scaleTable,
                                      int scaleRootPc,
                                      const int* scaleIv,
                                      int nScaleIv,
                                      const TheoryEngine& theory) const noexcept;

    void buildChordNotes(int rootMidi,
                         const int* iv,
                         int nIv,
                         int inversion,
                         std::array<uint8_t, kMaxChordTones>& out,
                         uint8_t& nOut) const noexcept;

    int findChordMapForSource(uint8_t src) const noexcept;
    void freeChordMap(int idx) noexcept;

    void removeNoteFromArpPool(uint8_t note) noexcept
    {
        uint8_t n = 0;
        for (uint8_t i = 0; i < arpPoolCount; ++i)
        {
            if (arpPool[(size_t)i] != note)
                arpPool[(size_t)n++] = arpPool[(size_t)i];
        }
        arpPoolCount = n;

        uint8_t m = 0;
        for (uint8_t i = 0; i < pressCount; ++i)
        {
            if (pressOrder[(size_t)i] != note)
                pressOrder[(size_t)m++] = pressOrder[(size_t)i];
        }
        pressCount = m;
    }
    void addNoteToArpPool(uint8_t note) noexcept;
    void rebuildArpSortBuffer() noexcept;

    void cleanup() noexcept
    {
        // Safety check for Arp pool and maps
        if (arpPoolCount > kMaxPool) arpPoolCount = kMaxPool;

        const bool latch = readArpLatch();
        if (!latch)
        {
            uint8_t n = 0;
            for (uint8_t i = 0; i < arpPoolCount; ++i)
            {
                const uint8_t note = arpPool[(size_t)i];
                bool found = physicalHeld[(size_t)(note & 127)];

                if (!found)
                {
                    for (int j = 0; j < kMaxMap; ++j)
                    {
                        if (chordMaps[(size_t)j].used)
                        {
                            for (uint8_t k = 0; k < chordMaps[(size_t)j].nOut; ++k)
                            {
                                if (chordMaps[(size_t)j].outs[(size_t)k] == note)
                                {
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (found) break;
                    }
                }

                if (found)
                {
                    arpPool[(size_t)n++] = note;
                }
                // Else: note is removed by not copying it to the new position
            }
            arpPoolCount = n;

            // Also cleanup pressOrder
            uint8_t m = 0;
            for (uint8_t i = 0; i < pressCount; ++i)
            {
                const uint8_t note = pressOrder[(size_t)i];
                bool found = physicalHeld[(size_t)(note & 127)];
                if (!found)
                {
                    for (int j = 0; j < kMaxMap; ++j)
                    {
                        if (chordMaps[(size_t)j].used)
                        {
                            for (uint8_t k = 0; k < chordMaps[(size_t)j].nOut; ++k)
                                if (chordMaps[(size_t)j].outs[(size_t)k] == note) { found = true; break; }
                        }
                        if (found) break;
                    }
                }
                if (found) pressOrder[(size_t)m++] = note;
            }
            pressCount = m;
        }
    }

    void allNotesOffOutput(juce::MidiBuffer& out, int samplePos) noexcept;
    void fireArpStep(juce::MidiBuffer& out,
                     int samplePos,
                     int stepIndex,
                     float gate01,
                     double stepLenSamples,
                     double gateScaleMul,
                     double sampleRate,
                     bool advanceSequencer,
                     juce::MidiKeyboardState* keyboardState = nullptr) noexcept;

    bool readStepOn(int stepIndex) const noexcept;
    int readStepVel(int stepIndex) const noexcept;
    float readStepDur(int stepIndex) const noexcept;
    int readStepTrn(int stepIndex) const noexcept;
    int readStepRkt(int stepIndex) const noexcept;

    Ptrs ptrs {};
    std::array<ArpStepPtrs, kArpSteps> arpStepPtrs {};
    juce::AudioParameterChoice* arpRateParam = nullptr;
    juce::AudioParameterFloat* arpSwingParam = nullptr;
    juce::AudioParameterChoice* arpPatternChoice = nullptr;
    std::array<juce::AudioParameterFloat*, kArpSteps> arpStepDurParams {};
    bool pointersBound = false;

    /** PPQ phase snap only on arp enable — avoids fighting per-sample advance (MIDI zipper/static). */
    bool lastProcessArpOn = false;
    bool lastChordOn = false;

    /** End-of-last-block PPQ (predicted) for transport jump detection when sync is on. */
    double lastArpSyncPpq = 0.0;
    bool arpSyncPpqPrimed = false;

    std::array<ChordMapSlot, kMaxMap> chordMaps {};

    std::array<uint8_t, kMaxPool> arpPool {};
    uint8_t arpPoolCount = 0;
    std::array<uint8_t, kMaxPool> arpSorted {};
    uint8_t arpSortedCount = 0;
    std::array<uint8_t, kMaxPool> pressOrder {};
    uint8_t pressCount = 0;

    int arpPatternIndex = 0;
    int arpUpDownDir = 1;
    int arpRandomLast = -1;

    double arpTimeInStep = 0;
    int lastRatchetSubIdx = -1;
    double samplesPerArpStep = 0;

    int lastChordScaleRoot = -1;
    int lastChordScaleId = -1;
    std::array<int, 128> chordScaleTable {};

    uint8_t arpPlayingNote = 0;
    bool arpNoteActive = false;
    double arpGateSamplesLeft = 0;

    uint32_t arpRng = 0xC0FFEEu;
    /** Advances only when an arp step actually fires (not skipped 32-step slots) — drives Up/Down/Order/Random. */
    uint32_t arpNoteWalk = 0;
    int arpFullStepCount = 0;
    double currentSampleRate = 44100.0;
    std::array<uint8_t, kMaxChordTones> arpPolyHeld {};
    uint8_t arpPolyCount = 0;

    bool physicalHeld[128] {};
    bool lastLatch = false;
};

} // namespace wolfsden