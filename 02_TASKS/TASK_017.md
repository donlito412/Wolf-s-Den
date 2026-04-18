TASK ID: 017

PHASE: 2

STATUS: PENDING

GOAL:
Extend the sample playback engine (WDSamplePlayer / osc case 7) beyond single-WAV-per-layer to support multi-sample mapping, velocity layers, and loop point editing. Currently the engine loads one WAV file into layer 0 for factory presets. A professional sample instrument needs multiple samples across the MIDI range (so notes don't pitch-shift too far from root), velocity layers (different samples for soft vs loud playing), and configurable loop points. This task delivers all three.

ASSIGNED TO:
Cursor

INPUTS:
/05_ASSETS/apex_project/Source/engine/samples/WDSamplePlayer.h   (full implementation — read entirely)
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.h
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.cpp       (case 7, lines 417–421; noteOn lines 148–225)
/05_ASSETS/apex_project/Source/engine/SynthEngine.h
/05_ASSETS/apex_project/Source/engine/SynthEngine.cpp
/05_ASSETS/apex_project/Source/PluginProcessor.h
/05_ASSETS/apex_project/Source/PluginProcessor.cpp
/05_ASSETS/apex_project/Source/ui/pages/SynthPage.h
/05_ASSETS/apex_project/Source/ui/pages/SynthPage.cpp

OUTPUT:
/03_OUTPUTS/017_sample_depth_report.md
(Code changes in /05_ASSETS/apex_project/Source/)

---

CURRENT STATE — READ BEFORE IMPLEMENTING

WDSamplePlayer (WDSamplePlayer.h):
- Loads a single WAV file per instance (requestLoad / loadNow)
- Supports loop on/off, one-shot, start/end frame (as fractions 0–1)
- Pitch is applied via pitchRatioFromRoot = pow(2, (note - rootNote) / 12)
- One WDSamplePlayer per VoiceLayer (so per layer, not per voice)

The limitation: if the user loads a C3 piano sample and plays C5, the sample plays at 4× speed — audibly unnatural (thin, chipmunk quality). Professional samplers solve this with a keymap: multiple samples at different root notes, each covering a range of keys. The nearest sample to the played note is used.

Current multi-sample workaround: none. The existing code always uses the single loaded sample and stretches it across the full range.

---

ARCHITECTURE TO IMPLEMENT

MULTI-SAMPLE KEYMAP:

Define a new struct `SampleZone`:
```cpp
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
};
```

Add a `SampleKeymap` class (new file: `Source/engine/samples/SampleKeymap.h/.cpp`):
```cpp
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
```

SYNTHENGINE INTEGRATION:
- Add `SampleKeymap layerKeymaps[4]` to SynthEngine
- Add `void addSampleZone(int layerIndex, const SampleZone& zone)` — message thread only
- Add `void clearSampleKeymap(int layerIndex)` — message thread only
- In SynthEngine::noteOn() (where voice is assigned): call `layerKeymaps[L].findZone(midiNote, velocity)` to select the correct zone. Pass the zone's file, rootNote, startFrac, endFrac, loopEnabled, oneShot to the voice's WDSamplePlayer via requestLoad().

VOICELAYER INTEGRATION:
- noteOn in VoiceLayer is called with (midiNote, velocity, layerIndex, params)
- The zone selection happens in SynthEngine before calling VoiceLayer::noteOn() — VoiceLayer itself doesn't need to change except that it receives the correct pitchRatio from its WDSamplePlayer which already has the right rootNote

LOOP POINT EDITING UI (SynthPage):
- Add to SynthPage, visible when osc mode = Sample:
  - "Loop Start" knob (0.0–1.0) → writes to `layer{N}_sample_start` APVTS param (already exists as startFrac in requestLoad)
  - "Loop End" knob (0.0–1.0) → writes to `layer{N}_sample_end` APVTS param (already exists as endFrac)
  - "Loop" toggle button → existing one-shot/loop behavior
  - "Keymap" button → opens a simple AlertWindow or native dialog listing loaded zones (name, lo-hi note range, velocity range), plus an "Add Zone" button that opens FileChooser

VELOCITY LAYER BEHAVIOR:
- When a note is played, SynthEngine selects the zone whose [loNote, hiNote] contains the MIDI note AND whose [loVel, hiVel] contains the velocity (0–127)
- If multiple zones match on note range, select the one whose velocity range contains the played velocity
- If no velocity-matched zone is found, fall back to the nearest-note zone regardless of velocity

PRESET SAVE/RECALL FOR KEYMAPS:
- Serialize all 4 layer keymaps into CustomState (XML) — file paths + zone parameters
- On preset load: restore keymaps and call addSampleZone() for each
- If a file path in the preset no longer exists on disk: log a warning, skip that zone (do not crash)

---

MINIMUM DELIVERABLE (PHASE 2 SCOPE)

Do not overscope this task. The minimum that constitutes completion:
1. Multi-sample keymap works: loading 3+ zones across the keyboard produces correct pitch without extreme stretching
2. Velocity layers work: soft notes (vel < 64) use a different sample than loud notes (vel >= 64) when configured
3. Loop start/end knobs are wired and audibly change the loop window
4. Keymap survives preset save/reload
5. Existing single-WAV factory preset behavior is unchanged (backward compatible)

---

DELIVERABLES INSIDE OUTPUT REPORT:
- SampleZone struct and SampleKeymap class API (as implemented)
- Demonstration: 3-zone keymap (e.g. C2, C4, C6 root notes covering 3 octave ranges each) — confirmed that playing across the range uses different samples with smooth pitch transitions
- Confirmation that velocity switching works (soft/loud)
- Confirmation that loop start/end knobs change the loop audibly
- Confirmation that factory presets still load and play unchanged
- Build: cmake --build build --config Release — zero errors

CONSTRAINTS:
- Zone selection (findZone) must be real-time safe — no heap allocation, no mutex in the audio thread
- File loading (loadNow) remains on background thread — never on audio thread
- Do not change WDSamplePlayer's existing public API — only extend SynthEngine / SampleKeymap
- Do not change existing APVTS parameter IDs
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
