TASK ID: 016

PHASE: 2

STATUS: DONE

GOAL:
Complete the granular oscillator so grain pitch tracks the played MIDI note and the scatter control produces meaningful, audible randomization. Currently grains play from the source audio at their internal read speed regardless of what MIDI note is pressed — the `hz` parameter is explicitly ignored (`juce::ignoreUnused(hz)` at line 541 of VoiceLayer.cpp). The result is a granular engine that produces the right texture but at the wrong pitch. This task fixes pitch tracking and makes scatter (position jitter + speed variance) work with musically useful ranges.

ASSIGNED TO:
Cursor

INPUTS:
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.h
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.cpp   (processGranular lines 539–601, spawnGrain lines 502–537)
/05_ASSETS/apex_project/Source/ui/pages/SynthPage.h
/05_ASSETS/apex_project/Source/ui/pages/SynthPage.cpp

OUTPUT:
/03_OUTPUTS/016_granular_report.md
(Code changes in /05_ASSETS/apex_project/Source/)

---

CURRENT STATE — READ BEFORE IMPLEMENTING

spawnGrain (VoiceLayer.cpp line 502):
```cpp
g.speed = std::pow(2.0, (((int)(r >> 8) % 513) / 512.0 - 0.5) * scatN * 0.04);
```
- Grain speed is set to a random value near 1.0, scaled by scatter
- Speed = 1.0 means grains advance at 1 sample per sample (plays at original pitch)
- There is no relationship between grain speed and the played MIDI note

processGranular (VoiceLayer.cpp line 539):
```cpp
juce::ignoreUnused(hz);
```
- The target frequency (from MIDI note) is passed in but explicitly discarded

The granular engine uses `granSource` (a static wavetable sine wave passed at prepare time) as its default audio source. When a sample is loaded into the layer, `useSampleSource = true` and grains read from the WDSamplePlayer's buffer. Both paths are broken for pitch because `hz` is ignored.

---

FIXES — IMPLEMENT ALL

FIX 1 — GRAIN PITCH TRACKING
The grain's read speed must be set so the grain sounds at the target MIDI note.

For the granular wavetable source (`useSampleSource = false`):
The `granSource` array is a single-cycle waveform. To play at frequency `hz`, the grain must advance `hz / sr` × `granLen` samples per output sample.
```cpp
// Base speed for target pitch (wavetable source):
const double baseSpeed = hz * (double)granLen / sr;
```

For the sample source (`useSampleSource = true`):
The sample has a known root note (from `samplePlayer.rootNote()`). To transpose to `hz`:
```cpp
// Base speed for target pitch (sample source):
const double rootHz = dsp::midiNoteToHz(samplePlayer.rootNote());
const double baseSpeed = hz / rootHz;  // pitch ratio applied to read speed
```

In spawnGrain(), accept `hz` and `useSampleSource` as parameters (they are already available in the caller). Set the base grain speed to the appropriate formula above, then apply scatter as a multiplier:
```cpp
// After computing baseSpeed:
const double scatterRange = (double)scatN * 0.5;  // ±50% pitch at max scatter
const double rndFactor = ((double)(r >> 8) % 1024) / 512.0 - 1.0;  // [-1, 1]
g.speed = baseSpeed * std::pow(2.0, rndFactor * scatterRange);
```
The `scatterRange` of 0.5 means at max scatter (scatN = 1.0) grains deviate ±0.5 octave from the target pitch. At scatN = 0, all grains play at exactly the target pitch. This is the Omnisphere/standard granular behavior.

FIX 2 — REMOVE juce::ignoreUnused(hz)
File: VoiceLayer.cpp, line 541
Delete: `juce::ignoreUnused(hz);`
Pass `hz` to `spawnGrain()` (update the function signature to accept `double hz`).

FIX 3 — SCATTER POSITION JITTER RANGE
File: VoiceLayer.cpp, line 513
Current scatter position jitter:
```cpp
const double jit = ((double)(r & 0xffff) / 32768.0 - 1.0) * scatN * 0.08 * (double)span;
```
The scale factor 0.08 means at max scatter, grains randomly start within ±8% of the source length. This is too small to be audible.
Change to:
```cpp
const double jit = ((double)(r & 0xffff) / 32768.0 - 1.0) * scatN * 0.35 * (double)span;
```
±35% of source length at max scatter — wide enough to be musically useful without complete chaos.

FIX 4 — FREEZE PLAYHEAD ADVANCE
File: VoiceLayer.cpp, line 543–556 (freeze block in processGranular)
Currently freeze captures a single position and all grains spawn from there. This is correct. Confirm that when freeze = false, `frozenPlayhead` does not influence grain positions (it does not — it is only read in the freeze block).
No code change required here — document as confirmed correct in the report.

FIX 5 — SYNTHPAGE GRANULAR CONTROLS VISIBILITY
File: SynthPage.cpp
Confirm the granular controls (Position, Size, Density, Scatter) are shown/hidden based on the layer osc mode selection. If the osc mode = Granular (case 5) and the controls are not visible, fix the show/hide logic.
Also confirm: the Freeze toggle button is visible and wired to `layer{N}_gran_freeze`.

---

TEST PROTOCOL

All tests in Logic Pro with granular oscillator selected on Layer 1:

TEST 1 — Pitch tracking (default wavetable source)
- Set scatter to 0
- Play C3, then C4, then C5
- RESULT: Pitch clearly moves up one octave between each note
- FAIL condition: all notes sound at the same pitch

TEST 2 — Pitch tracking (sample source)
- Load any factory WAV sample into the layer
- Select Granular osc mode
- Set scatter to 0
- Play C3, C4, C5
- RESULT: Each note clearly at a different pitch, root note at correct pitch

TEST 3 — Scatter pitch range
- Set scatter to 0 → hold a note → clean, pitched texture
- Set scatter to 0.5 → hold same note → slight pitch spread between grains (chorus-like)
- Set scatter to 1.0 → hold same note → wide pitch randomization, still centered on played note
- FAIL condition: scatter has no audible effect

TEST 4 — Scatter position range
- Load a sample, set position to 0.5, scatter to 1.0
- RESULT: Grains start from different positions in the sample — you can hear different textural fragments
- FAIL condition: sounds identical to scatter = 0

TEST 5 — Freeze
- Set scatter to 0, hold a note, turn freeze on
- RESULT: Granular texture freezes at current position — no further movement through source
- FAIL condition: source continues to scan

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Old vs new spawnGrain() function signature and body
- Confirmation of all 5 tests (PASS/FAIL with notes)
- Confirmation that scatter = 0 produces clean pitched output
- Confirmation that scatter = 1.0 produces wide pitch randomization
- Build: cmake --build build --config Release — zero errors

CONSTRAINTS:
- No allocations in processGranular() or spawnGrain() — both are called from the audio thread
- Do not change APVTS parameter IDs for gran_pos, gran_size, gran_density, gran_scatter, gran_freeze
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE