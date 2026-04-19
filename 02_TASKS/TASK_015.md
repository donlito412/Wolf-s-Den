TASK ID: 015

PHASE: 2

STATUS: DONE

GOAL:
Complete the wavetable oscillator so it is a real, usable synthesis mode. Currently the engine (case 4 in VoiceLayer::processOscillator) interpolates between two internal double arrays (wtData / wtBData) that are set at prepare() time with no file loading mechanism and no user-facing wavetable library. The morph knob exists and the interpolation math is correct, but there is nothing to morph between. This task delivers: a factory wavetable library of at least 20 tables, a file loading pipeline so the user can import their own .wav wavetables, and a UI control on the Synth page to browse and select wavetables per layer.

ASSIGNED TO:
Claude (wavetable design + data) + Cursor (loading pipeline + UI wiring)

INPUTS:
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.h
/05_ASSETS/apex_project/Source/engine/synth/VoiceLayer.cpp         (readWavetableMorph lines 272–291, case 4 lines 399–403)
/05_ASSETS/apex_project/Source/engine/SynthEngine.h
/05_ASSETS/apex_project/Source/engine/SynthEngine.cpp
/05_ASSETS/apex_project/Source/PluginProcessor.h
/05_ASSETS/apex_project/Source/PluginProcessor.cpp
/05_ASSETS/apex_project/Source/ui/pages/SynthPage.h
/05_ASSETS/apex_project/Source/ui/pages/SynthPage.cpp

OUTPUT:
/03_OUTPUTS/015_wavetable_report.md
(Code changes in /05_ASSETS/apex_project/Source/ and Resources/)

---

CURRENT STATE — READ BEFORE IMPLEMENTING

readWavetableMorph (VoiceLayer.cpp line 272):
- Reads from `wtData` (wavetableA pointer) and `wtBData` (wavetableB pointer)
- Interpolates between them using `morph01` (the layer_wt_morph APVTS parameter)
- The interpolation is correct

VoiceLayer::prepare (line 71):
- Takes `wavetableA`, `wavetableSizeA`, `wavetableB`, `wavetableSizeB` pointers from SynthEngine
- SynthEngine owns the actual wavetable arrays and passes pointers down

What is missing:
1. SynthEngine has no wavetable library — no factory tables exist in the codebase
2. There is no file load path for user wavetables
3. SynthPage has no wavetable browser or selection control
4. The morph knob morphs between the SAME table (wtBData falls back to wtData if null)

---

PART A — CLAUDE: Factory Wavetable Library

Design and generate 20 single-cycle wavetables in C++ as constexpr arrays (256 samples each, normalized to [-1, 1] range).

Required tables (generate mathematically — no audio file dependency):
1.  Sine — pure sin(2π × phase)
2.  Saw — linear ramp, PolyBLEP corrected
3.  Square — 50% duty, PolyBLEP corrected
4.  Triangle — bipolar triangle
5.  Soft Saw — saw filtered through mild one-pole LP (rounded top)
6.  Fat Square — square + slight pulse width modulation
7.  Organ — sine + 2nd + 3rd harmonic (1.0, 0.5, 0.25 amplitudes)
8.  Electric Piano — sine + inharmonic partials (jazz EP character)
9.  Strings — rich overtone stack (1/n harmonic series, 8 partials)
10. Brass — bright harmonic content, odd partials dominant
11. Vocal Aah — formant-shaped spectrum, rounded
12. Vocal Eeh — higher formant center, brighter
13. Bell — inharmonic partials (1.0, 2.756, 5.404, 8.933 × fundamental)
14. Digital — harsh, clipped sine (waveshaping applied)
15. Noise Sine — sine with phase noise (slight detune character)
16. Pulse 25 — 25% pulse width
17. Pulse 12 — 12% pulse width (thin, percussive)
18. Half Sine — rectified sine (abs value of sin)
19. Staircase — quantized saw (4 steps)
20. Additive Sweep — odd harmonics 1–15, falling amplitude (1/n)

Storage: Create a new file `Source/engine/synth/WavetableBank.h`
- Each table as `constexpr double kWT_Name[256] = { ... };`
- A registry: `struct WavetableEntry { const char* name; const double* data; int len; };`
- `constexpr WavetableEntry kFactoryWavetables[] = { { "Sine", kWT_Sine, 256 }, ... };`
- `constexpr int kNumFactoryWavetables = 20;`

---

PART B — CURSOR: Loading Pipeline + Morph System + UI

1. WAVETABLE SELECTION IN SYNTHENGINE
   SynthEngine needs to own two wavetable buffers per layer (A and B) to enable morph.
   Add to SynthEngine:
   ```cpp
   std::array<std::vector<double>, 4> wtBufA;  // per-layer wavetable A
   std::array<std::vector<double>, 4> wtBufB;  // per-layer wavetable B (morph target)
   ```
   Add public methods:
   ```cpp
   void setLayerWavetable(int layerIndex, int tableIndexA, int tableIndexB);
   // tableIndex: 0–19 = factory table, or -1 to preserve current
   
   void loadLayerWavetableFromFile(int layerIndex, int slot, const juce::File& file);
   // slot: 0 = A, 1 = B
   // Loads a WAV file, reads first 256–4096 samples, resamples to 256, stores in wtBufA/B
   // Must NOT be called from audio thread — call from message thread only
   ```
   After setting, call `voice.layers[L].prepare(...)` to update the voice's pointers.

2. USER WAVETABLE FILE LOADING
   Format: single-cycle WAV, mono, any length (16 to 4096 samples)
   Loading process (message thread):
   - Open file with JUCE AudioFormatManager
   - Read all samples into a temp buffer
   - Resample to exactly 256 samples using linear interpolation
   - Normalize to peak 1.0
   - Store in wtBufA[layer] or wtBufB[layer]
   - Call setLayerWavetable() to pass new pointers to voices

3. PRESET SAVE/RECALL FOR WAVETABLE SELECTION
   Add two integer parameters per layer to APVTS (not float — these are indices):
   - `layer{N}_wt_index_a` (0–19, factory table index for slot A)
   - `layer{N}_wt_index_b` (0–19, factory table index for slot B)
   On preset load: read these, call setLayerWavetable().
   User-loaded files: store the file path in CustomState (same mechanism as other custom state).

4. SYNTHPAGE UI — WAVETABLE BROWSER
   Add to SynthPage, visible only when layer osc mode = Wavetable (case 4):
   - Two combo boxes: "WT A" and "WT B" — populated with factory table names from kFactoryWavetables[]
   - A "Load File" button next to each combo — opens juce::FileChooser for WAV files
   - The morph knob already exists on SynthPage — wire its label to show "MORPH A→B"
   - Show/hide the WT controls using the same show/hide pattern as the granular controls

5. CONFIRMATION THAT MORPH WORKS
   With WT A = Sine and WT B = Saw:
   - Morph knob at 0.0 → pure sine output
   - Morph knob at 1.0 → pure saw output
   - Morph knob at 0.5 → blend of both audible

---

DELIVERABLES INSIDE OUTPUT REPORT:
- List of all 20 factory wavetables with a one-line description of the waveform character
- Confirmation that WT A/B selection changes the oscillator sound in Logic Pro
- Confirmation that morph knob sweeps smoothly from A to B without clicks
- Confirmation that user-loaded WAV file plays back at correct pitch across MIDI range
- Confirmation that wavetable selection saves and restores with presets
- Build: cmake --build build --config Release — zero errors

CONSTRAINTS:
- Wavetable loading must NOT happen on the audio thread
- No new allocations in renderAdd() or processOscillator()
- Factory tables are constexpr — zero runtime cost
- Do not change the morph APVTS parameter ID (layer_wt_morph)
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE