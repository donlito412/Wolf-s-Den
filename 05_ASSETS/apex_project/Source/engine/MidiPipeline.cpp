#include "MidiPipeline.h"

#include "TheoryEngine.h"

#include <algorithm>
#include <cmath>

namespace wolfsden
{
namespace
{

inline float readAP(std::atomic<float>* p, float d = 0.f) noexcept
{
    return p ? p->load(std::memory_order_relaxed) : d;
}

inline int readChoice(std::atomic<float>* p, int maxIndex, int defV = 0) noexcept
{
    if (!p || maxIndex <= 0)
        return defV;
    const float n = p->load(std::memory_order_relaxed);
    return juce::jlimit(0, maxIndex - 1, (int)(n * (float)maxIndex * 0.9999f));
}

bool pitchClassInScale(int notePc, int rootPc, const int* iv, int nIv) noexcept
{
    if (iv == nullptr || nIv < 1)
        return true;
    for (int i = 0; i < nIv; ++i)
    {
        const int degPc = ((rootPc + iv[i]) % 12 + 12) % 12;
        if (degPc == notePc)
            return true;
    }
    return false;
}

/** Nearest MIDI note >= midiNote whose pitch class is in the scale. */
int nearestInScaleNote(int midiNote, const int* iv, int nIv, int rootPc) noexcept
{
    if (iv == nullptr || nIv < 1)
        return juce::jlimit(0, 127, midiNote);
    for (int offset = 0; offset < 12; ++offset)
    {
        const int candidate = midiNote + offset;
        if (candidate > 127)
            break;
        const int npc = candidate % 12;
        for (int i = 0; i < nIv; ++i)
        {
            const int degPc = ((rootPc + iv[i]) % 12 + 12) % 12;
            if (degPc == npc)
                return candidate;
        }
    }
    return juce::jlimit(0, 127, midiNote);
}

void buildScaleLookupTable(std::array<int, 128>& table, int rootPc, const int* iv, int nIv) noexcept
{
    for (int i = 0; i < 128; ++i)
        table[(size_t)i] = nearestInScaleNote(i, iv, nIv, rootPc);
}

int nearestChordPitchClass(int inputPc, int chordRootPc, const int* iv, int nIv) noexcept
{
    int best = inputPc;
    int bestDist = 24;
    for (int i = 0; i < nIv; ++i)
    {
        const int cpc = ((chordRootPc + iv[i]) % 12 + 12) % 12;
        int d = std::abs(inputPc - cpc);
        if (d > 6)
            d = 12 - d;
        if (d < bestDist)
        {
            bestDist = d;
            best = cpc;
        }
    }
    return best;
}

int mapToChordToneMidi(int note, int chordRootPc, const int* iv, int nIv) noexcept
{
    const int oct = note / 12;
    const int ipc = note % 12;
    const int targetPc = nearestChordPitchClass(ipc, chordRootPc, iv, nIv);
    int out = oct * 12 + targetPc;
    if (out < 0)
        out = 0;
    if (out > 127)
        out = 127;
    return out;
}

/** Chord type index → intervals (semitones from root). Matches APVTS theory_chord_type order + extras. */
struct ChordTypeData
{
    const int* iv;
    int n;
};

// 0 Major … 10 Dim7 (11 UI types)
constexpr int kMaj[] = { 0, 4, 7 };
constexpr int kMin[] = { 0, 3, 7 };
constexpr int kDim[] = { 0, 3, 6 };
constexpr int kAug[] = { 0, 4, 8 };
constexpr int kSus2[] = { 0, 2, 7 };
constexpr int kSus4[] = { 0, 5, 7 };
constexpr int kMaj7[] = { 0, 4, 7, 11 };
constexpr int kMin7[] = { 0, 3, 7, 10 };
constexpr int kDom7[] = { 0, 4, 7, 10 };
constexpr int kHalfDim[] = { 0, 3, 6, 10 };
constexpr int kDim7[] = { 0, 3, 6, 9 };
constexpr int kMinMaj7[] = { 0, 3, 7, 11 };
constexpr int kAdd9[] = { 0, 4, 7, 14 };
constexpr int kMaj9[] = { 0, 4, 7, 11, 14 };
constexpr int kMin9[] = { 0, 3, 7, 10, 14 };
const ChordTypeData kChordTypes[] = {
    { kMaj, 3 },
    { kMin, 3 },
    { kDim, 3 },
    { kAug, 3 },
    { kSus2, 3 },
    { kSus4, 3 },
    { kMaj7, 4 },
    { kMin7, 4 },
    { kDom7, 4 },
    { kHalfDim, 4 },
    { kDim7, 4 },
    { kMinMaj7, 4 },
    { kAdd9, 4 },
    { kMaj9, 5 },
    { kMin9, 5 },
};

constexpr int kNumChordTypesUi = 15;

} // namespace

void MidiPipeline::prepare(double sampleRate, int maxBlock, juce::AudioProcessorValueTreeState& apvts)
{
    juce::ignoreUnused(sampleRate, maxBlock);
    bindPointers(apvts);
    reset();
}

void MidiPipeline::bindPointers(juce::AudioProcessorValueTreeState& apvts)
{
    ptrs.keysLockMode = apvts.getRawParameterValue("midi_keys_lock_mode");
    ptrs.chordMode = apvts.getRawParameterValue("midi_chord_mode");
    ptrs.chordType = apvts.getRawParameterValue("theory_chord_type");
    ptrs.chordInversion = apvts.getRawParameterValue("midi_chord_inversion");
    ptrs.arpOn = apvts.getRawParameterValue("midi_arp_on");
    ptrs.arpRate = apvts.getRawParameterValue("midi_arp_rate");
    ptrs.arpPattern = apvts.getRawParameterValue("midi_arp_pattern");
    ptrs.arpLatch = apvts.getRawParameterValue("midi_arp_latch");
    ptrs.arpSwing = apvts.getRawParameterValue("midi_arp_swing");
    ptrs.arpOctaves = apvts.getRawParameterValue("midi_arp_octaves");
    ptrs.arpSyncPpq = apvts.getRawParameterValue("midi_arp_sync_ppq");
    for (int si = 0; si < kArpSteps; ++si)
    {
        const juce::String idx = juce::String(si).paddedLeft('0', 2);
        const juce::String pfx = "midi_arp_s" + idx + "_";
        arpStepPtrs[(size_t)si].on = apvts.getRawParameterValue(pfx + "on");
        arpStepPtrs[(size_t)si].vel = apvts.getRawParameterValue(pfx + "vel");
        arpStepPtrs[(size_t)si].dur = apvts.getRawParameterValue(pfx + "dur");
        arpStepPtrs[(size_t)si].trn = apvts.getRawParameterValue(pfx + "trn");
        arpStepPtrs[(size_t)si].rkt = apvts.getRawParameterValue(pfx + "rkt");
    }
    pointersBound = true;
}

bool MidiPipeline::readStepOn(int stepIndex) const noexcept
{
    if (stepIndex < 0 || stepIndex >= kArpSteps)
        return true;
    auto* p = arpStepPtrs[(size_t)stepIndex].on;
    return !p || readAP(p, 1.f) > 0.5f;
}

int MidiPipeline::readStepVel(int stepIndex) const noexcept
{
    if (stepIndex < 0 || stepIndex >= kArpSteps)
        return 100;
    auto* p = arpStepPtrs[(size_t)stepIndex].vel;
    if (!p)
        return 100;
    const float n = readAP(p, (100.f - 1.f) / 126.f);
    return juce::jlimit(1, 127, (int)std::lround(1.f + n * 126.f));
}

float MidiPipeline::readStepDur(int stepIndex) const noexcept
{
    if (stepIndex < 0 || stepIndex >= kArpSteps)
        return 1.f;
    auto* p = arpStepPtrs[(size_t)stepIndex].dur;
    if (!p)
        return 1.f;
    const float n = readAP(p, (1.f - 0.1f) / 1.9f);
    return juce::jlimit(0.1f, 2.f, 0.1f + n * 1.9f);
}

int MidiPipeline::readStepTrn(int stepIndex) const noexcept
{
    if (stepIndex < 0 || stepIndex >= kArpSteps)
        return 0;
    auto* p = arpStepPtrs[(size_t)stepIndex].trn;
    if (!p)
        return 0;
    const float n = readAP(p, 0.5f);
    return juce::jlimit(-24, 24, (int)std::lround(-24.f + n * 48.f));
}

int MidiPipeline::readStepRkt(int stepIndex) const noexcept
{
    if (stepIndex < 0 || stepIndex >= kArpSteps)
        return 1;
    auto* p = arpStepPtrs[(size_t)stepIndex].rkt;
    if (!p)
        return 1;
    const float n = readAP(p, 0.f);
    return juce::jlimit(1, 4, 1 + (int)std::lround(n * 3.f));
}

void MidiPipeline::reset() noexcept
{
    for (auto& m : chordMaps)
        m = {};
    arpPoolCount = 0;
    arpSortedCount = 0;
    pressCount = 0;
    arpPatternIndex = 0;
    arpUpDownDir = 1;
    arpRandomLast = -1;
    arpTimeInStep = 0;
    lastRatchetSubIdx = -1;
    samplesPerArpStep = 0;
    lastChordScaleRoot = -1;
    lastChordScaleId = -1;
    chordScaleTable.fill(0);
    for (int i = 0; i < 128; ++i)
        chordScaleTable[(size_t)i] = i;
    arpPlayingNote = 0;
    arpNoteActive = false;
    arpGateSamplesLeft = 0;
    arpPolyCount = 0;
    arpPolyHeld.fill(0);
    arpFullStepCount = 0;
    for (auto& p : physicalHeld)
        p = false;
    lastLatch = false;
}

int MidiPipeline::readKeysLockMode() const noexcept
{
    return readChoice(ptrs.keysLockMode, 5, 0);
}

bool MidiPipeline::readChordMode() const noexcept
{
    return readAP(ptrs.chordMode) > 0.5f;
}

int MidiPipeline::readChordTypeIndex() const noexcept
{
    return readChoice(ptrs.chordType, kNumChordTypesUi, 0);
}

int MidiPipeline::readChordInversion() const noexcept
{
    const float n = readAP(ptrs.chordInversion, 0.f);
    return juce::jlimit(0, 3, (int)std::lround(n * 3.f));
}

bool MidiPipeline::readArpOn() const noexcept
{
    return readAP(ptrs.arpOn) > 0.5f;
}

float MidiPipeline::readArpRate() const noexcept
{
    return readAP(ptrs.arpRate, 4.f);
}

MidiPipeline::ArpPattern MidiPipeline::readArpPattern() const noexcept
{
    return (ArpPattern)readChoice(ptrs.arpPattern, (int)ArpPattern::numPatterns, 0);
}

bool MidiPipeline::readArpLatch() const noexcept
{
    return readAP(ptrs.arpLatch) > 0.5f;
}

float MidiPipeline::readArpSwing() const noexcept
{
    return juce::jlimit(0.f, 0.5f, readAP(ptrs.arpSwing, 0.f));
}

int MidiPipeline::readArpOctaves() const noexcept
{
    const float n = readAP(ptrs.arpOctaves, 0.f);
    return juce::jlimit(1, 4, 1 + (int)std::lround(n * 3.f));
}

std::optional<int> MidiPipeline::applyKeysLock(int note,
                                                int keysMode,
                                                const std::array<int, 128>& scaleTable,
                                                int scaleRootPc,
                                                const int* scaleIv,
                                                int nScaleIv,
                                                const TheoryEngine& theory) const noexcept
{
    if (keysMode == 0)
        return note;

    if (keysMode == 1) // Remap
        return scaleTable[(size_t)juce::jlimit(0, 127, note)];

    if (keysMode == 2) // Mute (scale only)
    {
        const int pc = note % 12;
        if (pitchClassInScale(pc, scaleRootPc, scaleIv, nScaleIv))
            return note;
        return std::nullopt;
    }

    if (keysMode == 3) // Chord tones (from detection)
    {
        const ChordMatch m = theory.getBestMatch();
        if (m.chordId < 0 || m.score < 0.05f)
            return note;
        const auto& defs = theory.getChordDefinitions();
        if (m.chordId >= (int)defs.size())
            return note;
        const auto& cd = defs[(size_t)m.chordId];
        if (cd.intervals.empty())
            return note;
        const int nIv = juce::jmin((int)cd.intervals.size(), 8);
        int ivBuf[8];
        for (int i = 0; i < nIv; ++i)
            ivBuf[i] = cd.intervals[(size_t)i];
        return mapToChordToneMidi(note, m.rootNote, ivBuf, nIv);
    }

    if (keysMode == 4) // Chord scales — table rebuilt in process()
    {
        return chordScaleTable[(size_t)juce::jlimit(0, 127, note)];
    }

    return note;
}

void MidiPipeline::buildChordNotes(int rootMidi,
                                    const int* iv,
                                    int nIv,
                                    int inversion,
                                    std::array<uint8_t, kMaxChordTones>& out,
                                    uint8_t& nOut) const noexcept
{
    nOut = 0;
    if (nIv < 1 || iv == nullptr)
        return;

    int absNotes[8];
    int n = juce::jmin(nIv, 8);
    const int baseOct = (rootMidi / 12) * 12;
    for (int i = 0; i < n; ++i)
        absNotes[i] = juce::jlimit(0, 127, baseOct + iv[i]);

    std::sort(absNotes, absNotes + n);
    const int inv = juce::jlimit(0, n - 1, inversion);
    for (int k = 0; k < inv; ++k)
    {
        const int lowest = absNotes[0];
        for (int i = 0; i < n - 1; ++i)
            absNotes[i] = absNotes[i + 1];
        absNotes[n - 1] = lowest + 12;
    }

    for (int i = 0; i < n; ++i)
    {
        out[(size_t)i] = (uint8_t)juce::jlimit(0, 127, absNotes[i]);
        ++nOut;
    }
}

int MidiPipeline::findChordMapForSource(uint8_t src) const noexcept
{
    for (int i = 0; i < kMaxMap; ++i)
        if (chordMaps[(size_t)i].used && chordMaps[(size_t)i].source == src)
            return i;
    return -1;
}

void MidiPipeline::freeChordMap(int idx) noexcept
{
    if (idx >= 0 && idx < kMaxMap)
        chordMaps[(size_t)idx] = {};
}

void MidiPipeline::removeNoteFromArpPool(uint8_t note) noexcept
{
    uint8_t newPool[kMaxPool];
    uint8_t n = 0;
    for (uint8_t i = 0; i < arpPoolCount; ++i)
    {
        if (arpPool[(size_t)i] != note)
            newPool[(size_t)n++] = arpPool[(size_t)i];
    }
    for (uint8_t i = 0; i < n; ++i)
        arpPool[(size_t)i] = newPool[(size_t)i];
    arpPoolCount = n;

    uint8_t newOrd[kMaxPool];
    uint8_t m = 0;
    for (uint8_t i = 0; i < pressCount; ++i)
    {
        if (pressOrder[(size_t)i] != note)
            newOrd[(size_t)m++] = pressOrder[(size_t)i];
    }
    for (uint8_t i = 0; i < m; ++i)
        pressOrder[(size_t)i] = newOrd[(size_t)i];
    pressCount = m;
}

void MidiPipeline::addNoteToArpPool(uint8_t note) noexcept
{
    for (uint8_t i = 0; i < arpPoolCount; ++i)
        if (arpPool[(size_t)i] == note)
            return;
    if (arpPoolCount < kMaxPool)
        arpPool[(size_t)arpPoolCount++] = note;
    if (pressCount < kMaxPool)
        pressOrder[(size_t)pressCount++] = note;
}

void MidiPipeline::rebuildArpSortBuffer() noexcept
{
    arpSortedCount = arpPoolCount;
    for (uint8_t i = 0; i < arpPoolCount; ++i)
        arpSorted[(size_t)i] = arpPool[(size_t)i];
    std::sort(arpSorted.begin(), arpSorted.begin() + arpSortedCount);
}

inline int rngMod(uint32_t& s, int mod) noexcept
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    if (mod <= 0)
        return 0;
    return (int)(s % (uint32_t)mod);
}

void MidiPipeline::allNotesOffOutput(juce::MidiBuffer& out, int samplePos) noexcept
{
    if (arpNoteActive)
    {
        out.addEvent(juce::MidiMessage::noteOff(1, (int)arpPlayingNote), samplePos);
        arpNoteActive = false;
    }
    for (uint8_t i = 0; i < arpPolyCount; ++i)
        out.addEvent(juce::MidiMessage::noteOff(1, (int)arpPolyHeld[(size_t)i]), samplePos);
    arpPolyCount = 0;
}

void MidiPipeline::fireArpStep(juce::MidiBuffer& out,
                                int samplePos,
                                int stepIndex,
                                float vel01,
                                float gate01,
                                double gateScaleMul,
                                double sampleRate,
                                bool advanceSequencer) noexcept
{
    juce::ignoreUnused(sampleRate);
    const int step = juce::jlimit(0, kArpSteps - 1, stepIndex);
    const ArpPattern pat = readArpPattern();
    if (arpPoolCount == 0)
    {
        allNotesOffOutput(out, samplePos);
        return;
    }

    rebuildArpSortBuffer();

    if (!readStepOn(step))
    {
        if (advanceSequencer)
        {
            arpPatternIndex = (arpPatternIndex + 1) % kArpSteps;
            ++arpFullStepCount;
        }
        return;
    }

    int noteIdx = 0;
    const int nNotes = (int)arpSortedCount;
    if (nNotes < 1)
        return;

    const int vel = juce::jlimit(1, 127, (int)(vel01 * (float)readStepVel(step)));
    const float vNorm = (float)vel / 127.f;
    const int nOct = juce::jmax(1, readArpOctaves());
    const int octShift = (arpFullStepCount % nOct) * 12;
    const int trn = readStepTrn(step);

    if (pat == ArpPattern::Chord)
    {
        allNotesOffOutput(out, samplePos);
        arpPolyCount = 0;
        for (int i = 0; i < nNotes && arpPolyCount < kMaxChordTones; ++i)
        {
            int nn = (int)arpSorted[(size_t)i];
            nn += trn + octShift;
            nn = juce::jlimit(0, 127, nn);
            out.addEvent(juce::MidiMessage::noteOn(1, nn, vNorm), samplePos);
            arpPolyHeld[(size_t)arpPolyCount++] = (uint8_t)nn;
        }
        arpGateSamplesLeft = (double)juce::jlimit(0.1f, 2.f, gate01) * samplesPerArpStep * juce::jmax(1.0e-6, gateScaleMul);
        arpNoteActive = false;
        if (advanceSequencer)
        {
            arpPatternIndex = (arpPatternIndex + 1) % kArpSteps;
            ++arpFullStepCount;
        }
        return;
    }

    if (pat == ArpPattern::Up)
        noteIdx = arpPatternIndex % nNotes;
    else if (pat == ArpPattern::Down)
        noteIdx = nNotes - 1 - (arpPatternIndex % nNotes);
    else if (pat == ArpPattern::UpDown)
    {
        const int cycleLen = nNotes > 1 ? (nNotes * 2 - 2) : 1;
        int pos = arpPatternIndex % cycleLen;
        if (pos < nNotes)
            noteIdx = pos;
        else
            noteIdx = nNotes - 2 - (pos - nNotes);
        noteIdx = juce::jlimit(0, nNotes - 1, noteIdx);
    }
    else if (pat == ArpPattern::Order)
    {
        if (pressCount > 0)
        {
            const uint8_t want = pressOrder[(size_t)(arpPatternIndex % pressCount)];
            noteIdx = 0;
            for (int i = 0; i < nNotes; ++i)
                if (arpSorted[(size_t)i] == want)
                {
                    noteIdx = i;
                    break;
                }
        }
    }
    else if (pat == ArpPattern::Random)
    {
        if (nNotes == 1)
            noteIdx = 0;
        else
        {
            int r = rngMod(arpRng, nNotes);
            int guard = 0;
            while (r == arpRandomLast && guard++ < 8)
                r = rngMod(arpRng, nNotes);
            arpRandomLast = r;
            noteIdx = r;
        }
    }

    int nn = (int)arpSorted[(size_t)noteIdx];
    nn += trn + octShift;
    nn = juce::jlimit(0, 127, nn);

    allNotesOffOutput(out, samplePos);
    out.addEvent(juce::MidiMessage::noteOn(1, nn, vNorm), samplePos);
    arpPlayingNote = (uint8_t)nn;
    arpNoteActive = true;
    arpGateSamplesLeft = (double)juce::jlimit(0.1f, 2.f, gate01) * samplesPerArpStep * juce::jmax(1.0e-6, gateScaleMul);

    if (advanceSequencer)
    {
        arpPatternIndex = (arpPatternIndex + 1) % kArpSteps;
        ++arpFullStepCount;
    }
}

void MidiPipeline::process(juce::MidiBuffer& midi,
                           int numSamples,
                           double sampleRate,
                           juce::AudioPlayHead* playHead,
                           const TheoryEngine& theory) noexcept
{
    if (!pointersBound || numSamples < 1)
        return;

    currentSampleRate = sampleRate;

    double bpm = 120.0;
    if (playHead != nullptr)
        if (const auto pos = playHead->getPosition())
            if (const auto bt = pos->getBpm())
                if (*bt > 1.0 && *bt < 999.0)
                    bpm = *bt;

    const int keysMode = readKeysLockMode();
    const bool chordOn = readChordMode();
    const bool arpOn = readArpOn();
    const float stepsPerBeat = juce::jlimit(0.25f, 32.f, readArpRate());
    const double beatsPerSecond = bpm / 60.0;
    const double stepsPerSecond = beatsPerSecond * (double)stepsPerBeat;
    samplesPerArpStep = stepsPerSecond > 1.0e-6 ? sampleRate / stepsPerSecond : sampleRate;

    if (readAP(ptrs.arpSyncPpq, 1.f) > 0.5f && playHead != nullptr)
        if (const auto pos = playHead->getPosition())
            if (const auto oPpq = pos->getPpqPosition())
            {
                const double ppq = *oPpq;
                const double stepBeats = 1.0 / (double)stepsPerBeat;
                if (stepBeats > 1e-12)
                {
                    const double phase01 = std::fmod(std::abs(ppq), stepBeats) / stepBeats;
                    arpTimeInStep = phase01 * samplesPerArpStep;
                    lastRatchetSubIdx = -1;
                }
            }

    std::array<int, 128> scaleTable {};
    theory.getScaleLookupTable(scaleTable);

    int scaleRootPc = theory.getActiveScaleRoot();
    static constexpr int kChrom[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    const int* scaleIv = kChrom;
    int nScaleIv = 12;
    const auto& scaleDefs = theory.getScaleDefinitions();
    if (theory.isDatabaseReady() && !scaleDefs.empty())
    {
        const int st = juce::jlimit(0, (int)scaleDefs.size() - 1, theory.getActiveScaleType());
        const auto& ivs = scaleDefs[(size_t)st].intervals;
        if (!ivs.empty())
        {
            scaleIv = ivs.data();
            nScaleIv = (int)ivs.size();
        }
    }

    const ChordMatch cm = theory.getBestMatch();
    if (keysMode == 4 && theory.isDatabaseReady() && cm.chordId >= 0 && cm.score > 0.05f)
    {
        if (cm.chordId != lastChordScaleId || cm.rootNote != lastChordScaleRoot)
        {
            lastChordScaleId = cm.chordId;
            lastChordScaleRoot = cm.rootNote;
            static constexpr int kMajor[] = { 0, 2, 4, 5, 7, 9, 11 };
            buildScaleLookupTable(chordScaleTable, cm.rootNote, kMajor, 7);
        }
    }
    else if (keysMode == 4)
    {
        lastChordScaleId = -1;
        lastChordScaleRoot = -1;
        chordScaleTable = scaleTable;
    }

    struct Ev
    {
        int t = 0;
        juce::MidiMessage m;
    };
    std::array<Ev, 512> inEv {};
    size_t nIn = 0;
    for (const auto meta : midi)
    {
        if (nIn >= inEv.size())
            break;
        inEv[nIn].t = meta.samplePosition;
        inEv[nIn].m = meta.getMessage();
        ++nIn;
    }
    std::sort(inEv.begin(), inEv.begin() + (ptrdiff_t)nIn, [](const Ev& a, const Ev& b) {
        return a.t < b.t;
    });

    juce::MidiBuffer out;

    const bool latch = readArpLatch();
    if (lastLatch && !latch)
    {
        for (int n = 0; n < 128; ++n)
        {
            if (!physicalHeld[(size_t)n])
                removeNoteFromArpPool((uint8_t)n);
        }
    }
    lastLatch = latch;

    size_t inIx = 0;

    for (int s = 0; s < numSamples; ++s)
    {
        while (inIx < nIn && inEv[inIx].t == s)
        {
            const auto msg = inEv[inIx].m;
            ++inIx;

            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                const int raw = msg.getNoteNumber();
                physicalHeld[(size_t)raw] = true;

                const auto locked = applyKeysLock(raw, keysMode, scaleTable, scaleRootPc, scaleIv, nScaleIv, theory);
                if (!locked.has_value())
                    continue;

                if (chordOn)
                {
                    const int tidx = readChordTypeIndex();
                    const ChordTypeData& cd = kChordTypes[juce::jmin(tidx, kNumChordTypesUi - 1)];
                    std::array<uint8_t, kMaxChordTones> notes {};
                    uint8_t nBuilt = 0;
                    buildChordNotes(*locked, cd.iv, cd.n, readChordInversion(), notes, nBuilt);

                    std::array<uint8_t, kMaxChordTones> finalNotes {};
                    uint8_t nF = 0;
                    for (uint8_t i = 0; i < nBuilt; ++i)
                    {
                        const auto o2 = applyKeysLock((int)notes[(size_t)i], keysMode, scaleTable, scaleRootPc, scaleIv, nScaleIv, theory);
                        if (o2.has_value())
                            finalNotes[(size_t)nF++] = (uint8_t)juce::jlimit(0, 127, *o2);
                    }
                    if (nF == 0)
                        continue;

                    int slot = -1;
                    for (int i = 0; i < kMaxMap; ++i)
                        if (!chordMaps[(size_t)i].used)
                        {
                            slot = i;
                            break;
                        }
                    if (slot >= 0)
                    {
                        chordMaps[(size_t)slot].used = true;
                        chordMaps[(size_t)slot].source = (uint8_t)raw;
                        chordMaps[(size_t)slot].nOut = nF;
                        for (uint8_t i = 0; i < nF; ++i)
                            chordMaps[(size_t)slot].outs[(size_t)i] = finalNotes[(size_t)i];
                    }

                    if (arpOn)
                    {
                        for (uint8_t i = 0; i < nF; ++i)
                            addNoteToArpPool(finalNotes[(size_t)i]);
                    }
                    else
                    {
                        for (uint8_t i = 0; i < nF; ++i)
                            out.addEvent(juce::MidiMessage::noteOn(msg.getChannel(), (int)finalNotes[(size_t)i], msg.getFloatVelocity()), s);
                    }
                }
                else
                {
                    const int nn = *locked;
                    if (arpOn)
                        addNoteToArpPool((uint8_t)nn);
                    else
                        out.addEvent(juce::MidiMessage::noteOn(msg.getChannel(), nn, msg.getFloatVelocity()), s);
                }
            }
            else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                const int raw = msg.getNoteNumber();
                physicalHeld[(size_t)raw] = false;

                if (chordOn)
                {
                    const int idx = findChordMapForSource((uint8_t)raw);
                    if (idx >= 0)
                    {
                        const auto& slot = chordMaps[(size_t)idx];
                        if (arpOn)
                        {
                            if (!latch)
                            {
                                for (uint8_t i = 0; i < slot.nOut; ++i)
                                    removeNoteFromArpPool(slot.outs[(size_t)i]);
                            }
                        }
                        else
                        {
                            for (uint8_t i = 0; i < slot.nOut; ++i)
                                out.addEvent(juce::MidiMessage::noteOff(msg.getChannel(), (int)slot.outs[(size_t)i]), s);
                        }
                        freeChordMap(idx);
                    }
                }
                else
                {
                    auto mapped = applyKeysLock(raw, keysMode, scaleTable, scaleRootPc, scaleIv, nScaleIv, theory);
                    if (!mapped.has_value())
                        continue;
                    const int nn = *mapped;
                    if (arpOn)
                    {
                        if (!latch)
                            removeNoteFromArpPool((uint8_t)nn);
                    }
                    else
                    {
                        out.addEvent(juce::MidiMessage::noteOff(msg.getChannel(), nn), s);
                    }
                }
            }
        }

        if (arpOn)
        {
            if (arpGateSamplesLeft > 0)
            {
                arpGateSamplesLeft -= 1.0;
                if (arpGateSamplesLeft <= 0)
                    allNotesOffOutput(out, s);
            }

            if (arpPoolCount > 0)
            {
                const int si = arpPatternIndex % kArpSteps;
                const float swing = readArpSwing();
                double stepLen = samplesPerArpStep;
                if ((si & 1) != 0 && swing > 0.f)
                    stepLen *= 1.0 + (double)swing;

                const double prevT = arpTimeInStep;
                arpTimeInStep += 1.0;

                if (readStepOn(si))
                {
                    const int R = juce::jmax(1, readStepRkt(si));
                    const double subLen = stepLen / (double)R;
                    const float vel01 = (float)readStepVel(si) / 127.f;
                    const float gate01 = readStepDur(si);
                    for (int k = 1; k <= R; ++k)
                    {
                        const double b = (double)k * subLen;
                        if (prevT < b - 1e-9 && arpTimeInStep >= b - 1e-9)
                            fireArpStep(out, s, si, vel01, gate01, 1.0 / (double)R, sampleRate, k == R);
                    }
                }

                if (arpTimeInStep >= stepLen - 1e-9)
                {
                    arpTimeInStep -= stepLen;
                    if (!readStepOn(si))
                        arpPatternIndex = (arpPatternIndex + 1) % kArpSteps;
                }
            }
            else
            {
                allNotesOffOutput(out, s);
                arpTimeInStep = 0;
            }
        }
    }

    midi.clear();
    midi.addEvents(out, 0, numSamples, 0);
}

} // namespace wolfsden
