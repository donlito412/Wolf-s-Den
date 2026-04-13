# ARCHITECTURE DESIGN DOCUMENT
## Wolf's Den VST — All-in-One Music Production Plugin
### Task: TASK_001 | Status: COMPLETE | Date: 2026-04-09

---

# TABLE OF CONTENTS

1. System Overview & Block Diagram
2. Full Component List & Responsibilities
3. Thread Model
4. Data Flow: MIDI In → Audio Out
5. Parameter System
6. State Management & Presets
7. Database Schema
8. Full UI Page Map
9. Build System Notes
10. TASK_001 Closure — Deliverable Crosswalk & Scope

---

# 1. SYSTEM OVERVIEW & BLOCK DIAGRAM

Wolf's Den is a hybrid VST instrument plugin built on JUCE 7+ (C++). It produces real audio output while simultaneously understanding musical harmony. The plugin is organized around three core pillars — a Harmony Engine, a Synthesis Engine, and a Performance Engine — all coordinated through a central AudioProcessor host.

## Top-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        WOLF'S DEN VST                               │
│                                                                     │
│   ┌───────────────┐        ┌──────────────────────────────────┐    │
│   │  PLUGIN EDITOR│        │         PLUGIN PROCESSOR         │    │
│   │  (UI Thread)  │◄──────►│         (DSP / Audio Thread)     │    │
│   │               │  APVTS │                                  │    │
│   │  MainComponent│        │  ┌────────────────────────────┐  │    │
│   │  + All Pages  │        │  │     MIDI PIPELINE          │  │    │
│   └───────────────┘        │  │  (Performance Engine)      │  │    │
│                            │  │  ScaleLock | ChordMode | Arp│  │    │
│                            │  └────────────┬───────────────┘  │    │
│                            │               │ MIDI events       │    │
│                            │  ┌────────────▼───────────────┐  │    │
│                            │  │     THEORY ENGINE          │  │    │
│                            │  │     (Harmony Engine)       │  │    │
│                            │  │  ChordDetect | VoiceLead   │  │    │
│                            │  │  ScaleAware | ChordDB      │  │    │
│                            │  └────────────┬───────────────┘  │    │
│                            │               │ MIDI + chord ctx  │    │
│                            │  ┌────────────▼───────────────┐  │    │
│                            │  │     SYNTHESIS ENGINE       │  │    │
│                            │  │  Layer A | B | C | D       │  │    │
│                            │  │  Osc | Filter | Env | LFO  │  │    │
│                            │  │  + Modulation Matrix       │  │    │
│                            │  └────────────┬───────────────┘  │    │
│                            │               │ audio buffers     │    │
│                            │  ┌────────────▼───────────────┐  │    │
│                            │  │       FX ENGINE            │  │    │
│                            │  │  LayerFX | CommonFX        │  │    │
│                            │  │  MasterFX | 30+ Units      │  │    │
│                            │  └────────────┬───────────────┘  │    │
│                            │               │ final audio out   │    │
│                            └───────────────┼──────────────────┘    │
│                                            │                        │
│                            ┌───────────────▼──────────────────┐    │
│                            │         PRESET SYSTEM            │    │
│                            │    PresetBrowser | SQLite DB     │    │
│                            └──────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
         │ Audio Out (L/R stereo, or multi-channel)
         ▼
       DAW
```

## Signal Path Summary

```
DAW/Hardware MIDI In
    → [MIDI Pipeline]     Scale Lock, Chord Mode, Arpeggiator
    → [Theory Engine]     Chord detection, scale context, voice leading
    → [Synthesis Engine]  4 layers × (Osc → Filter → Envelope → LFO)
    → [Modulation Matrix] Cross-modulation, LFO/ENV → any parameter
    → [FX Engine]         Layer FX → Common FX → Master FX
    → DAW Audio Out
```

---

# 2. FULL COMPONENT LIST & RESPONSIBILITIES

## 2.1 PluginProcessor (AudioProcessor)
**File:** `Source/PluginProcessor.h/.cpp`
**Thread:** DSP (audio) thread — processBlock()
**Responsibilities:**
- Top-level owner of all engine instances (MidiPipeline, TheoryEngine, SynthEngine, FxEngine)
- Receives MIDI buffer + audio buffer from DAW each block
- Routes MIDI through pipeline in sequence per block
- Calls SynthEngine to generate audio samples
- Calls FxEngine to process audio output
- Owns JUCE `AudioProcessorValueTreeState` (APVTS) — all automatable parameters live here
- Implements `getStateInformation()` / `setStateInformation()` for full state serialization
- Runs at audio-thread priority; zero allocations, zero locks in processBlock()

**Justification:** JUCE's AudioProcessor is the mandatory entry point for all DAW-plugin communication. Centralizing engine ownership here ensures clean lifetime management and deterministic teardown.

---

## 2.2 PluginEditor (AudioProcessorEditor)
**File:** `Source/PluginEditor.h/.cpp`
**Thread:** UI (message) thread
**Responsibilities:**
- Creates and owns MainComponent (root UI)
- Sets initial plugin window size (default: 1200×800)
- Resizable with min/max bounds enforcement
- Bridges PluginProcessor ↔ UI via APVTS parameter attachments
- 60fps repaint cycle via `juce::Timer`

**Justification:** JUCE's component/editor separation is the standard pattern. The editor is optional (headless operation remains valid), keeping DSP unblocked if the UI is closed.

---

## 2.3 MainComponent
**File:** `Source/ui/MainComponent.h/.cpp`
**Thread:** UI thread
**Responsibilities:**
- Root layout container — owns all persistent chrome and the swappable ActivePageView
- Renders: TopNavBar, ActivePageView (page swap zone), PianoKeyboard strip, TransportBar, StatusBar
- Manages page navigation: tab click → destroys old page component, instantiates new one
- Scales all child components proportionally on window resize via `AffineTransform::scale()`
- Passes processor reference down to each page for data access

---

## 2.4 MIDI Pipeline (MidiPipeline)
**File:** `Source/engine/MidiPipeline.h/.cpp`
**Thread:** DSP thread (called from processBlock)
**Sub-modules:**

### 2.4.1 ScaleLockEngine
- Stores 128-entry `int lookupTable[128]` — maps each MIDI note to its in-scale equivalent
- Scale database: 60+ scales stored as interval arrays (e.g., Major = {0,2,4,5,7,9,11})
- On scale/root change: rebuilds lookup table in microseconds (single array rewrite, lock-free)
- Modes: REMAP (off-scale → nearest in-scale), MUTE (off-scale → discard), CHORD_TONES (sparse mapping to active chord notes), CHORD_SCALES (dynamic per chord)
- Root note offset applied as full-table shift

### 2.4.2 ChordModeEngine
- Takes a single processed note and expands it to a full chord
- Chord interval library: 20+ chord types (triad, 7ths, 9ths, 11ths, sus, add, etc.)
- Inversion control: reorders interval stack for root/1st/2nd/3rd inversion
- Passes all generated notes through ScaleLock if Scale Constraint mode is active
- Outputs multi-note MIDI buffer for downstream processing

### 2.4.3 ArpEngine
- Chord register buffer: holds all currently held notes (up to 16 voices)
- Internal step clock synced to host BPM via `AudioPlayHead::CurrentPositionInfo`
- Rate: 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 (+ dotted and triplet variants)
- Play modes: Up, Down, Up+Down, Order (press order), Random, Chord (all simultaneously)
- Per-step controls: velocity, duration, transpose (±24 semitones), ratchet (subdivide step), tie (legato)
- Swing: percentage shift on every other step
- Latch: hold notes in register after key release
- Restart: pattern restarts on new key press vs. free-running

**Justification:** This mirrors Komplete Kontrol's Smart Play pipeline — each stage (Scale → Chord → Arp) is independent and stackable. Lookup table approach is O(1) and real-time safe.

---

## 2.5 Theory Engine (TheoryEngine)
**File:** `Source/engine/TheoryEngine.h/.cpp`
**Thread:** DSP thread for MIDI detection; background thread for audio FFT analysis
**Sub-modules:**

### 2.5.1 ChordDetector
- **MIDI Mode:** Collects Note On messages over a configurable window (default 500ms); captures pitch classes (0–11); compares against `chord_definitions` database using best-match scoring (most matching pitch classes, fewest extra notes); returns top match + alternates
- **Audio Mode:** FFT analysis (2048-point, Hann window) on sidechain input; extracts fundamental frequencies from spectral peaks; quantizes to chromatic pitch classes; runs same matching algorithm; sensitivity parameter controls FFT smoothing + match threshold
- Output: chord name, root note, quality, voicing, parent scale suggestions

### 2.5.2 ScaleAwarenessEngine
- Tracks the "active scale context" — root + scale type
- Updated by: user selection, chord detection result, progression position
- Provides scale membership queries: `isNoteInScale(int midiNote)`, `getNearestInScaleNote(int midiNote)`
- Used by ScaleLock and ChordModeEngine for real-time constraint

### 2.5.3 VoiceLeadingEngine
- Input: current chord voicing (set of MIDI notes) + target chord root+quality
- Generates all inversions of target chord (root, 1st, 2nd, 3rd inversion, open voicings)
- Scores each: sum of `|current_note[i] - target_note[i]|` for all voice pairs (minimum movement)
- Selects inversion with lowest score
- Voice assignment: bass = lowest note, soprano = highest note, inner voices fill between
- Optional voice grouping: assigns voices to MIDI channels 1–4 for multi-instrument setups

### 2.5.4 ChordProgressionModel
- Stores the user's active chord progression (Section C in Scaler terms)
- Array of `ChordEvent { chord_id, root, voicing, duration_beats, position_beats }`
- Used by the Arrange page and audio engine for progression playback
- Serialized as part of plugin state

**Justification:** Mirrors Scaler 3's chord detection and voice leading architecture. Audio FFT detection fills a gap that Komplete Kontrol lacks entirely.

---

## 2.6 Synthesis Engine (SynthEngine)
**File:** `Source/engine/SynthEngine.h/.cpp`
**Thread:** DSP thread
**Architecture:** 4 independent synthesis layers (A, B, C, D), each a full signal path.

### 2.6.1 SynthLayer (×4)
Each layer contains:

**OscillatorModule**
- Mode selector: WAVETABLE | GRANULAR | FM | SAMPLE
- **Wavetable mode:** Single-cycle waveform tables (Sine, Saw, Square, Triangle, noise variants); band-limited via pre-computed wavetable sets at each octave; wavetable position morphing
- **Granular mode:** Audio file decomposed into grains (1ms–500ms); parameters: Position, Size, Density, Pitch scatter, Pan scatter, Phase scatter; windowing: Hanning envelope per grain to prevent clicks
- **FM mode:** 2-operator FM (carrier + modulator); modulator frequency ratio, FM depth; harmonic/inharmonic ratio control
- **Sample mode:** Loads from asset library; streaming from disk via lock-free read-ahead buffer; loop points, pitch tracking, root note assignment
- Common: Coarse/Fine tune, octave shift, unison (up to 8 voices with detune + stereo spread), volume, pan

**FilterModule (Dual Filter per layer)**
- 20 filter types: LP 6/12/18/24dB, HP 6/12/18/24dB, BP, Notch, Comb, Formant/Vowel, State Variable (LP/BP/HP switchable), resonant specialty types
- Filter 1 + Filter 2: fully independent
- Routing: Series (F1→F2) or Parallel (F1+F2) with mix control
- Parameters: Cutoff, Resonance, Drive, Key Tracking (cutoff tracks MIDI pitch), Envelope Depth, LFO Depth

**EnvelopeModule (per layer: Amp Env + Filter Env; plus 4 global Mod Envelopes)**
- Multi-stage ADSR expandable to N-stage custom envelope
- Per-stage parameters: level, duration, curve shape (linear, exponential, S-curve, logarithmic)
- Tempo sync: any stage duration can lock to host BPM subdivision
- Loop points: designate stages to loop at tempo-synced rate
- Release curves: configurable per envelope

**LfoModule (2 LFOs per layer + 2 global LFOs)**
- Waveforms: Sine, Triangle, Saw, Square, Sample+Hold, Random, Noise, user-drawn (16-point custom shape)
- Rate: free Hz or tempo-synced (1/32 to 4 bars)
- Delay: time before LFO onset after note trigger
- Fade-in: attack ramp for smooth LFO entry
- Phase offset: start phase per voice
- Per-voice vs. shared mode: per-voice creates stereo drift; shared = phase-locked

**LayerMixer**
- Layer volume, pan, output routing
- Independent / Shared mode toggle (shared = layers B/C/D use layer A's filter+envelope signal path — CPU efficient)
- Pre/Post-fader aux sends to FX engine

### 2.6.2 ModulationMatrix
- Sources: LFO 1/2 (per layer), LFO 3/4 (global), Amp Env, Filter Env, Mod Env 1–4, MIDI Velocity, MIDI Aftertouch, MIDI Mod Wheel (CC1), MIDI Breath (CC2), Pitch Bend, XY Pad Angle, XY Pad Radius, MIDI CC (any CC#), Random, Alternating, Constant
- Targets: Any parameter in SynthEngine or FxEngine (cutoff, pitch, volume, pan, osc position, grain density, FX rate, FX depth, etc.)
- Per-routing controls: Depth (amount), Polarity (+/−), Invert toggle, Smooth (lag filter on modulation signal), Mute
- Maximum 64 simultaneous routings
- Implemented as: array of `ModRouting { source_id, target_id, depth, flags }` iterated per audio block — O(n) per sample, all inline

**Justification:** Mirrors Omnisphere's per-layer architecture and Flex-Mod routing. 4 layers provides Omnisphere-level depth. Lookup tables + pre-computed wavetables ensure DSP-safe operation.

---

## 2.7 FX Engine (FxEngine)
**File:** `Source/engine/FxEngine.h/.cpp`
**Thread:** DSP thread

### Signal Path
```
Layer A audio → [Layer A FX Rack — 4 slots]
Layer B audio → [Layer B FX Rack — 4 slots]
Layer C audio → [Layer C FX Rack — 4 slots]
Layer D audio → [Layer D FX Rack — 4 slots]
                         ↓ (summed at CommonFX input)
              [Common FX Rack — 6 slots]
                         ↓
              [Master FX Rack — 4 slots]
                         ↓
                     Audio Out
```

### FX Units (30+ types)

| Category | Units |
|---|---|
| Dynamics | Compressor, Limiter, Gate, Transient Shaper |
| EQ | Parametric EQ (4-band), Graphic EQ (10-band), Lo-Fi |
| Filters | LP/HP/BP (as effects) |
| Distortion | Overdrive, Waveshaper, Bit Crusher, Amp Sim, Fuzz |
| Chorus/Flange/Phase | Stereo Chorus, Flanger, Multi-voice Phaser |
| Delay | Stereo Delay, Ping Pong, Multi-tap, Tape Delay |
| Reverb | Hall, Room, Plate, Spring, Convolution |
| Modulation | Tremolo, Auto-Pan, Ring Mod, Vibrato |
| Spatial | Stereo Width, Frequency Shifter, Harmonizer |

Each FX unit:
- Instantiated from a factory via string type ID (`"reverb_hall"`, `"delay_stereo"`, etc.)
- All parameters exposed as APVTS parameters with unique IDs incorporating slot index
- Fully modulatable: any Mod Matrix source can target any FX parameter
- Bypass toggle per slot
- Drag-and-drop reordering within rack (swap at block boundary, lock-free)

**Justification:** Mirrors Omnisphere's layered FX architecture. Integrating FX into the modulation matrix is a gap in all three reference plugins that Wolf's Den fills.

---

## 2.8 Preset System
**File:** `Source/engine/PresetSystem.h/.cpp`
**Thread:** Background (loading); UI thread (browsing)
**Responsibilities:**
- SQLite-backed database for preset metadata, tags, and chord sets
- Tag-based browser: filter by Sound Type, Character, Genre, Mood, Instrument
- Favorites system, recently-used history
- Factory presets embedded as compressed binary resources (JUCE BinaryData)
- User preset save: serialize full state → store in SQLite `presets` table
- Preset load: read state blob → call `setStateInformation()` on processor (thread-safe via JUCE message thread post)
- Sound Lock: per-module immutability flags prevent specific sections from being overwritten on preset load

---

# 3. THREAD MODEL

Wolf's Den operates on three primary threads and two secondary threads.

## Primary Threads

### 3.1 DSP Thread (Audio Thread)
- Managed by the DAW's audio engine
- Calls `PluginProcessor::processBlock(AudioBuffer<float>&, MidiBuffer&)` at fixed intervals
- Buffer size: 32–2048 samples (configurable in DAW)
- **HARD RULES — no exceptions:**
  - Zero memory allocation (`new`, `malloc`, `std::vector::push_back` with reallocation)
  - Zero OS locks (`std::mutex::lock`, `std::lock_guard`)
  - Zero blocking I/O (no file reads, no SQLite queries)
  - Zero throwing exceptions
- All data structures used here are pre-allocated at initialization time
- Communication with other threads via lock-free ring buffers only (JUCE `AbstractFifo`)

### 3.2 UI Thread (Message Thread)
- JUCE's main message loop
- Handles: component painting (60fps via `juce::Timer`), mouse events, keyboard events
- Reads parameter values from APVTS for display (APVTS provides thread-safe atomic reads)
- Writes parameter values via APVTS parameter attachments (slider, button, combobox) — APVTS handles thread safety
- Sends non-parameter state changes (e.g., chord progression updates) to processor via `juce::AsyncUpdater` or message-thread-safe queue

### 3.3 MIDI Input Thread (External Hardware)
- JUCE `MidiInput` callback fires on a separate OS thread for hardware MIDI devices
- All received MIDI is pushed into a lock-free FIFO and consumed on the DSP thread in processBlock()
- DAW-routed MIDI arrives already in the MidiBuffer passed to processBlock() — no separate thread

## Secondary Threads

### 3.4 Background / Loading Thread
- `juce::Thread` subclass, low priority
- Handles: preset loading from SQLite, sample file streaming read-ahead, FFT analysis for audio chord detection
- Communicates results to DSP thread via lock-free ring buffers
- Communicates UI updates via `juce::MessageManager::callAsync()`

### 3.5 Rendering Thread (Optional — OpenGL visualizers)
- Activated only when granular visualizer or spectrum analyzer is visible
- `juce::OpenGLContext` with attached `juce::OpenGLRenderer`
- Reads visualization data (grain positions, FFT magnitude array) from a lock-free double buffer written by the DSP thread
- Strictly read-only from the DSP perspective; no DSP state is written from this thread

## Thread Communication Summary

| From | To | Mechanism |
|---|---|---|
| UI → DSP | Parameter changes | APVTS atomic parameter values |
| UI → DSP | Non-parameter state | Lock-free FIFO (AbstractFifo) |
| DSP → UI | Meter / visualization data | Lock-free double buffer (flip on block boundary) |
| MIDI HW → DSP | MIDI events | Lock-free FIFO |
| Background → DSP | Loaded samples / presets | Lock-free FIFO |
| Background → UI | Database results | `MessageManager::callAsync()` |

---

# 4. DATA FLOW: MIDI IN → AUDIO OUT

The following traces a single note event from DAW input to audio output, per processBlock() call.

```
┌─────────────────────────────────────────────────────────────┐
│                      processBlock()                         │
│                                                             │
│  1. MidiBuffer received from DAW                            │
│     └── iterate MIDI events                                 │
│                                                             │
│  2. MidiPipeline::process(MidiBuffer& in, MidiBuffer& out)  │
│     │                                                       │
│     ├── ScaleLockEngine::process(note)                      │
│     │   └── note = lookupTable[note]  [O(1)]                │
│     │                                                       │
│     ├── ChordModeEngine::process(note) [if enabled]         │
│     │   └── expand note → [note, note+interval, ...]        │
│     │   └── each note re-passed through ScaleLock           │
│     │                                                       │
│     └── ArpEngine::process(notes, hostBpm, blockPosition)   │
│         └── advance step clock, emit queued note on/offs    │
│         └── output: final MIDI events with timing offsets   │
│                                                             │
│  3. TheoryEngine::update(MidiBuffer& finalMidi)             │
│     ├── ChordDetector::update(notes)  → active chord state  │
│     ├── ScaleAwarenessEngine::update() → active scale state │
│     └── VoiceLeadingEngine::apply()   → adjust voicings     │
│                                                             │
│  4. SynthEngine::process(MidiBuffer& midi, AudioBuffer& out)│
│     ├── For each MIDI event in buffer:                      │
│     │   └── dispatch to VoiceAllocator                      │
│     │       └── assign voice to Layer A/B/C/D (per config)  │
│     │                                                       │
│     ├── For each active voice × each layer:                 │
│     │   ├── OscillatorModule::process(samples)              │
│     │   │   └── render wavetable/granular/FM/sample audio   │
│     │   ├── FilterModule::process(buffer)                   │
│     │   │   └── apply Filter1 → Filter2 (series) or mix     │
│     │   └── EnvelopeModule::applyAmp(buffer)                │
│     │       └── multiply by amplitude envelope              │
│     │                                                       │
│     ├── ModulationMatrix::apply()                           │
│     │   └── for each routing: compute source → scale → add  │
│     │       to target parameter (inline, pre-allocated)     │
│     │                                                       │
│     └── mix all layers → stereo AudioBuffer                 │
│                                                             │
│  5. FxEngine::process(AudioBuffer& in, AudioBuffer& out)    │
│     ├── route layer stems through LayerFX racks             │
│     ├── sum → CommonFX rack                                 │
│     └── → MasterFX rack → output buffer                     │
│                                                             │
│  6. Output buffer returned to DAW                           │
└─────────────────────────────────────────────────────────────┘
```

### Voice Allocation Model
- Maximum 32 voices total across all layers
- Each MIDI Note On → allocated to one voice slot per active layer
- Steal policy (when at max voices): steal oldest note, then quietest note
- Each voice tracks: note, velocity, age, envelope state, oscillator phase state

### Sample-Accurate MIDI Event Timing
- JUCE MidiBuffer stores `(MidiMessage, sampleOffset)` pairs
- processBlock() iterates events at their precise sample offset within the block
- OscillatorModule, FilterModule, EnvelopeModule render in sub-block segments between events
- Result: note onset and release are sample-accurate, not block-quantized

---

# 5. PARAMETER SYSTEM

## 5.1 JUCE AudioProcessorValueTreeState (APVTS)

All automatable parameters are registered in the APVTS at plugin initialization:

```cpp
AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    // Example parameters — full list covers all engines
    return {
        // MIDI Pipeline
        std::make_unique<AudioParameterBool>("scaleLock_enabled", "Scale Lock", false),
        std::make_unique<AudioParameterInt>("scaleLock_rootNote", "Root Note", 0, 11, 0),
        std::make_unique<AudioParameterInt>("scaleLock_scaleType", "Scale Type", 0, 59, 0),
        std::make_unique<AudioParameterBool>("chordMode_enabled", "Chord Mode", false),
        std::make_unique<AudioParameterInt>("chordMode_type", "Chord Type", 0, 19, 0),
        std::make_unique<AudioParameterInt>("chordMode_inversion", "Inversion", 0, 3, 0),
        std::make_unique<AudioParameterBool>("arp_enabled", "Arpeggiator", false),
        std::make_unique<AudioParameterFloat>("arp_rate", "Arp Rate", ...),

        // Synthesis — Layer A (repeated pattern for B, C, D with prefix change)
        std::make_unique<AudioParameterInt>("layerA_osc_mode", "Osc Mode", 0, 3, 0),
        std::make_unique<AudioParameterFloat>("layerA_osc_tune", "Tune", -24.0f, 24.0f, 0.0f),
        std::make_unique<AudioParameterFloat>("layerA_filter1_cutoff", "Filter1 Cutoff", 20.0f, 20000.0f, 5000.0f),
        std::make_unique<AudioParameterFloat>("layerA_filter1_resonance", "Filter1 Resonance", 0.0f, 1.0f, 0.0f),
        std::make_unique<AudioParameterFloat>("layerA_ampEnv_attack", "Amp Attack", 0.001f, 10.0f, 0.01f),
        // ... (full parameter list per deliverable scope)

        // FX — slot-indexed IDs
        std::make_unique<AudioParameterBool>("layerFxA_slot0_bypass", "Layer A FX0 Bypass", false),
        // ...
    };
}
```

## 5.2 Parameter ID Convention

All parameter IDs follow a strict hierarchical naming convention for unambiguous DAW automation:

```
{module}_{component}_{parameter}

Examples:
  scaleLock_rootNote        → Scale Lock root note
  layerA_filter1_cutoff     → Layer A, Filter 1, cutoff
  layerB_lfo2_rate          → Layer B, LFO 2, rate
  commonFx_slot2_depth      → Common FX rack, slot 2, depth
  modMatrix_route3_depth    → Mod Matrix routing 3, depth
```

## 5.3 Parameter Types

| Type | JUCE Class | Usage |
|---|---|---|
| Float | AudioParameterFloat | Cutoff, resonance, attack, depth, volume, pan |
| Int | AudioParameterInt | Chord type, scale type, root note, step count, voice count |
| Bool | AudioParameterBool | Enable/disable toggles, bypass switches, latch |
| Choice | AudioParameterChoice | Filter type, LFO waveform, arp mode (string list) |

## 5.4 MIDI Learn
- Any APVTS parameter can be bound to a MIDI CC number
- MIDI Learn mode: right-click parameter → "MIDI Learn" → move CC on hardware → binding saved
- Bindings stored in SQLite `user_midi_maps` table and serialized in plugin state
- On MIDI CC received: look up bound parameter ID → call `apvts.getRawParameterValue(id)->store(mappedValue)`

## 5.5 Parameter Smoothing
- All audio-rate parameters (cutoff, volume, pitch) pass through `juce::SmoothedValue<float>` with configurable ramp time (default: 5ms)
- Prevents zipper noise on automation or MIDI CC changes

---

# 6. STATE MANAGEMENT & PRESETS

## 6.1 Complete State Serialization

Wolf's Den serializes two layers of state:

**Layer 1 — APVTS (all parameter values)**
JUCE handles this automatically. `apvts.copyState()` returns a `juce::ValueTree` of all parameter values. Stored as a child node inside the full state XML.

**Layer 2 — Complex non-parameter state**
Items that cannot be expressed as simple JUCE parameters:
- Active chord progression (array of ChordEvents)
- Arp step data (velocity, duration, transpose per step)
- Mod Matrix routing table (source/target/depth arrays)
- Preset browser state (active filters, scroll position)
- MIDI Learn bindings
- User wavetables and granular source files (file paths + embedded small audio)
- Sound Lock flags

These are serialized as a custom XML tree or binary chunk alongside APVTS state.

```cpp
void getStateInformation(juce::MemoryBlock& destData) override
{
    auto state = apvts.copyState();
    // Append custom state as child node
    auto customState = juce::ValueTree("CustomState");
    customState.appendChild(chordProgression.toValueTree(), nullptr);
    customState.appendChild(modMatrix.toValueTree(), nullptr);
    customState.appendChild(midiLearnMap.toValueTree(), nullptr);
    // ...
    state.appendChild(customState, nullptr);

    juce::MemoryOutputStream stream(destData, true);
    state.writeToStream(stream);
}

void setStateInformation(const void* data, int sizeInBytes) override
{
    auto state = juce::ValueTree::readFromData(data, sizeInBytes);
    if (state.isValid())
    {
        apvts.replaceState(state);
        auto customState = state.getChildWithName("CustomState");
        chordProgression.fromValueTree(customState.getChildWithName("ChordProgression"));
        modMatrix.fromValueTree(customState.getChildWithName("ModMatrix"));
        // ...
    }
}
```

## 6.2 Preset Workflow

```
SAVE PRESET:
  User clicks Save → PresetSaveDialog → enter name, assign tags
  → PluginProcessor::getStateInformation() → binary blob
  → INSERT INTO presets (name, state_blob, ...) + INSERT INTO preset_tags

LOAD PRESET:
  User selects preset in browser → PresetSystem::loadPreset(id)
  → SELECT state_blob FROM presets WHERE id = ?
  → posted to message thread → PluginProcessor::setStateInformation()
  → SoundLock check: skip immutable modules during restoration

FACTORY PRESETS:
  Embedded at compile time via JUCE BinaryData
  → on first launch: INSERT into presets table (is_factory = 1)
  → never overwritten by user save
```

## 6.3 Sound Lock System

Each engine module (SynthEngine, FxEngine, ArpEngine, ModMatrix) exposes a `soundLockFlags` bitmask:

- Bit 0: Oscillator locked
- Bit 1: Filter locked
- Bit 2: Envelope locked
- Bit 3: LFO locked
- Bit 4: Mod Matrix locked
- Bit 5: FX locked
- Bit 6: Arp locked

During `setStateInformation()`, locked modules skip their state restore block. This enables Omnisphere-style browsing where FX or envelope settings are preserved across preset changes.

---

# 7. DATABASE SCHEMA

SQLite3 database file: `/Resources/Database/apex.db`
Accessed from background thread only; never on DSP thread.

```sql
-- ─────────────────────────────────────────────────────────
-- CHORD & SCALE THEORY TABLES
-- ─────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS scale_definitions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,                      -- "Dorian", "Major Pentatonic"
    intervals   TEXT    NOT NULL,                      -- JSON: [0,2,3,5,7,9,10]
    mode_family TEXT,                                  -- "major", "minor", "pentatonic", "exotic"
    mood_tags   TEXT,                                  -- JSON: ["dark", "introspective"]
    is_common   INTEGER NOT NULL DEFAULT 1             -- 1 = displayed prominently
);

CREATE TABLE IF NOT EXISTS chord_definitions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,                      -- "Major 7th"
    symbol      TEXT,                                  -- "maj7"
    intervals   TEXT    NOT NULL,                      -- JSON: [0,4,7,11]
    category    TEXT    NOT NULL,                      -- "triad", "seventh", "ninth", "extended"
    quality     TEXT    NOT NULL                       -- "major", "minor", "dominant", "diminished"
);

CREATE TABLE IF NOT EXISTS chord_sets (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL,
    root_note       INTEGER,                           -- 0=C, 1=C#, ... 11=B
    scale_id        INTEGER REFERENCES scale_definitions(id),
    chords          TEXT    NOT NULL,                  -- JSON: [{chord_id, inversion, octave, voicing_notes}]
    genre_tags      TEXT,                              -- JSON: ["trap", "jazz", "cinematic"]
    mood_tags       TEXT,                              -- JSON: ["dark", "bright", "tense"]
    energy_tag      TEXT,                              -- "low", "medium", "high"
    is_user_created INTEGER NOT NULL DEFAULT 0,
    is_favorite     INTEGER NOT NULL DEFAULT 0,
    created_at      TEXT    NOT NULL DEFAULT (datetime('now'))
);

-- ─────────────────────────────────────────────────────────
-- PRESET TABLES
-- ─────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS presets (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,
    author      TEXT,
    category    TEXT,                                  -- "Pad", "Lead", "Bass", "Pluck", "FX"
    state_blob  BLOB    NOT NULL,                      -- full serialized plugin state
    is_factory  INTEGER NOT NULL DEFAULT 0,
    is_favorite INTEGER NOT NULL DEFAULT 0,
    play_count  INTEGER NOT NULL DEFAULT 0,
    last_used   TEXT,
    created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS preset_tags (
    preset_id   INTEGER NOT NULL REFERENCES presets(id) ON DELETE CASCADE,
    tag_type    TEXT    NOT NULL,                      -- "sound_type", "character", "genre", "mood", "instrument"
    tag_value   TEXT    NOT NULL,
    PRIMARY KEY (preset_id, tag_type, tag_value)
);

-- ─────────────────────────────────────────────────────────
-- USER CONFIGURATION TABLES
-- ─────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS user_midi_maps (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    parameter_id    TEXT    NOT NULL UNIQUE,           -- APVTS parameter ID string
    midi_cc         INTEGER NOT NULL,                  -- 0-127
    midi_channel    INTEGER NOT NULL DEFAULT 0         -- 0 = any channel
);

CREATE TABLE IF NOT EXISTS user_wavetables (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,
    file_path   TEXT    NOT NULL,                      -- absolute path on user's system
    sample_rate INTEGER,
    created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS user_granular_sources (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,
    file_path   TEXT    NOT NULL,
    duration_ms REAL,
    created_at  TEXT    NOT NULL DEFAULT (datetime('now'))
);

-- ─────────────────────────────────────────────────────────
-- INDEXES FOR QUERY PERFORMANCE
-- ─────────────────────────────────────────────────────────

CREATE INDEX IF NOT EXISTS idx_preset_tags_type_value ON preset_tags(tag_type, tag_value);
CREATE INDEX IF NOT EXISTS idx_preset_tags_preset ON preset_tags(preset_id);
CREATE INDEX IF NOT EXISTS idx_chord_sets_root ON chord_sets(root_note);
CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category);
CREATE INDEX IF NOT EXISTS idx_presets_favorite ON presets(is_favorite);
```

---

# 8. FULL UI PAGE MAP

## 8.1 Global Layout (Persistent Chrome)

```
┌───────────────────────────────────────────────────────────────────────┐
│  WOLF'S DEN                                                           │
│  TOP NAV BAR (height: 48px)                                           │
│  [Browse] [Harmony] [Synth] [Mod] [FX] [Arrange] [Perform]           │
│                                      [Vol ▐▐▐▐░] [CPU 12%] [⚙ Settings]│
├───────────────────────────────────────────────────────────────────────┤
│                                                                       │
│                    ACTIVE PAGE VIEW                                   │
│               (content swapped per nav tab)                           │
│                                                                       │
│                                                                       │
├───────────────────────────────────────────────────────────────────────┤
│  TRANSPORT BAR (height: 32px)                                         │
│  [▶ Play] [■ Stop] [⬤ Record] [Loop ↺] BPM: 120.0  4/4              │
│  Preset: [< Dark Strings Pad >]  [★ Fav]  [Save]                     │
├───────────────────────────────────────────────────────────────────────┤
│  PIANO KEYBOARD STRIP (height: 72px)                                  │
│  C2────────────────────────────────C4 (colored by Scale Lock state)  │
│  [Octave -] [Octave +]   [MIDI In indicator]   [L ▐▐░░ R] level meter│
└───────────────────────────────────────────────────────────────────────┘
  Default window: 1200 × 800px  |  Minimum: 900 × 600px  |  Resizable: yes
```

**Piano Keyboard Strip behavior:**
- Root note keys = high brightness
- In-scale keys = medium brightness
- Out-of-scale keys = dark/off
- Incoming MIDI notes = highlighted in real-time
- Click to audition individual notes

---

## 8.2 PAGE 1: BROWSE PAGE

**Purpose:** Discover and load presets and chord sets.

```
┌─ BROWSE ─────────────────────────────────────────────────────────────┐
│ [Presets] [Chord Sets]  ← sub-tabs                                   │
│ ┌──────────────────────────────────────────────────────────────────┐ │
│ │ 🔍 Search: [___________________________]                         │ │
│ └──────────────────────────────────────────────────────────────────┘ │
│ ┌── FILTER SIDEBAR (240px) ──┐  ┌── RESULTS GRID / LIST ───────────┐ │
│ │ Sound Type:                │  │                                   │ │
│ │  □ Pad  □ Lead  □ Bass     │  │  ┌────────┐ ┌────────┐ ┌───────┐ │ │
│ │  □ Pluck □ Keys □ FX       │  │  │ Dark   │ │ Tape   │ │ Lush  │ │ │
│ │                            │  │  │ Strings│ │ Drive  │ │ Pad   │ │ │
│ │ Character:                 │  │  │ ★      │ │        │ │ ★     │ │ │
│ │  □ Warm □ Bright □ Dark    │  │  └────────┘ └────────┘ └───────┘ │ │
│ │  □ Aggressive □ Soft       │  │  ┌────────┐ ┌────────┐ ...       │ │
│ │                            │  │  │ FM Bell│ │ Trap   │           │ │
│ │ Genre:                     │  │  │ Lead   │ │ 808    │           │ │
│ │  □ Cinematic □ Electronic  │  │  └────────┘ └────────┘           │ │
│ │  □ Jazz □ Hip-Hop □ Ambient│  │                                   │ │
│ │                            │  │  [List View toggle]               │ │
│ │ Mood:                      │  └───────────────────────────────────┘ │
│ │  □ Dark □ Bright □ Tense   │  ┌── DETAIL PANE ────────────────────┐ │
│ │  □ Cinematic □ Peaceful    │  │ Name: Dark Strings Pad             │ │
│ │                            │  │ Author: Wolf's Den Factory         │ │
│ │ [Reset Filters]            │  │ Tags: Pad • Warm • Cinematic       │ │
│ │                            │  │ [▶ Preview]  [Load Preset]         │ │
│ └────────────────────────────┘  └───────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

**Key behaviors:**
- Filter sidebar: multi-select checkboxes; results grid updates in real-time via SQLite query
- Grid cards: show preset name, primary tag, star/favorite state
- Click card → populates Detail Pane; double-click → loads preset immediately
- Preview button → loads preset temporarily for auditioning; reverts on [X]
- Chord Sets sub-tab: same layout but browsing `chord_sets` table; "Load to Harmony Page" action

---

## 8.3 PAGE 2: HARMONY PAGE

**Purpose:** Chord detection, progression building, scale/voice leading control.

```
┌─ HARMONY ────────────────────────────────────────────────────────────┐
│ [Detect] [Scales] [Circle] [Voicing]  ← sub-tabs                    │
│                                                                      │
│ ┌── CHORD DETECT (sub-tab 1) ────────────────────────────────────────┐
│ │  Input Mode: [● MIDI] [○ Audio]     Sensitivity: [▐▐▐░░]          │
│ │                                                                    │
│ │  Detected Chord: Cmaj7          Parent Scale: C Major / A Natural Minor│
│ │  Alternates: Am/C | Em7add11                                       │
│ │                                                                    │
│ │  CHORD PALETTE (Section A — discovered chords)                     │
│ │  [Cmaj7] [Am7] [Fmaj7] [G7] [Em7] [Dm7] [+Add]                   │
│ │                                                                    │
│ │  PROGRESSION (Section C — active arrangement)                      │
│ │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                             │
│ │  │ Cmaj7│ │ Am7  │ │ Fmaj7│ │ G7   │                             │
│ │  │  2♩  │ │  2♩  │ │  4♩  │ │  4♩  │ [+]                         │
│ │  └──────┘ └──────┘ └──────┘ └──────┘                             │
│ │  [▶ Play]  [Loop: ON]  [Voice Leading: ON]  [Export MIDI]         │
│ └────────────────────────────────────────────────────────────────────┘
│
│ ┌── SCALES (sub-tab 2) ──────────────────────────────────────────────┐
│ │  Root: [C ▾]   Scale: [Major ▾]   Mode Family: [Diatonic ▾]       │
│ │  Notes: C D E F G A B  (shown on mini keyboard)                   │
│ │  Chords from Scale: [Cmaj] [Dm] [Em] [Fmaj] [G7] [Am] [Bdim]     │
│ │  Click any chord → add to Palette                                  │
│ └────────────────────────────────────────────────────────────────────┘
│
│ ┌── CIRCLE OF FIFTHS (sub-tab 3) ────────────────────────────────────┐
│ │  [SVG/drawn circle — outer: Major, inner: Relative Minor]          │
│ │  Click segment → updates active scale in Scales tab                │
│ │  Highlighted: current scale + adjacent (closely related)           │
│ └────────────────────────────────────────────────────────────────────┘
│
│ ┌── VOICING (sub-tab 4) ─────────────────────────────────────────────┐
│ │  Active Chord: Cmaj7                                               │
│ │  Extensions Grid: [triad] [maj7] [9th] [11th] [13th] [alt] [sus] │
│ │  Color: blue = in scale | gray = chromatic outside scale          │
│ │  Inversion: [Root ▾]   Density: [▐▐▐░░]   Octave: [- +]          │
│ │  Voice Grouping: □ Split voices to MIDI channels 1–4              │
│ └────────────────────────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────────────────────┘
```

---

## 8.4 PAGE 3: SYNTH PAGE

**Purpose:** Per-layer synthesis editing.

```
┌─ SYNTH ──────────────────────────────────────────────────────────────┐
│ [Layer A] [Layer B] [Layer C] [Layer D]   Layer A: [● ON]  Vol: ▐▐▐│
│                                                                      │
│ ┌── OSCILLATOR ───────────────────┐ ┌── FILTER 1 ─────────────────┐ │
│ │ Mode: [Wavetable ▾]             │ │ Type: [LP 24dB ▾]           │ │
│ │ Waveform: [Saw ▾]               │ │ Cutoff:  [▐▐▐▐░] 2800Hz     │ │
│ │ Position: [▐▐░░░]               │ │ Res:     [▐▐░░░]            │ │
│ │ Tune: [-0.00] Octave: [0 ▾]     │ │ Drive:   [▐░░░░]            │ │
│ │ Fine: [+0.00]                   │ │ Key Trk: [░░░░░] 0%         │ │
│ │ Unison: [3 voices] Det: [0.15]  │ └─────────────────────────────┘ │
│ │ Spread: [▐▐░░░]                 │ ┌── FILTER 2 ─────────────────┐ │
│ └─────────────────────────────────┘ │ Type: [HP 12dB ▾]           │ │
│                                     │ Cutoff:  [▐░░░░] 80Hz       │ │
│ ┌── AMP ENVELOPE ─────────────────┐ │ [Routing: Series ▾]         │ │
│ │ A: [▐░░░░] D: [▐▐░░░]          │ └─────────────────────────────┘ │
│ │ S: [▐▐▐░░] R: [▐▐░░░]          │                                  │
│ │ [🔍 Full Editor]                │ ┌── LFO 1 ──────────────────┐  │
│ └─────────────────────────────────┘ │ Wave: [Sine ▾] Rate: 0.5Hz │  │
│                                     │ Depth: [▐▐░░░] Fade: [▐░░] │  │
│ ┌── FILTER ENVELOPE ──────────────┐ │ [Sync to BPM: □]           │  │
│ │ A: [▐░░░░] D: [▐▐▐░░]          │ └────────────────────────────┘  │
│ │ S: [░░░░░] R: [▐▐░░░]          │ ┌── LFO 2 ──────────────────┐  │
│ │ Depth → Filter1: [▐▐▐░░]       │ │ Wave: [Rand ▾] Rate: 2.1Hz │  │
│ │ [🔍 Full Editor]                │ │ Depth: [▐░░░░]             │  │
│ └─────────────────────────────────┘ └────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
  🔍 Full Envelope Editor → opens modal overlay with multi-stage editor
  🔍 Full Filter Editor → opens modal overlay with additional filter options
```

---

## 8.5 PAGE 4: MOD PAGE (Modulation Matrix)

**Purpose:** Connect LFOs, envelopes, and MIDI sources to synthesis targets.

```
┌─ MODULATION ─────────────────────────────────────────────────────────┐
│ [Mod Matrix] [Envelopes] [LFOs]  ← sub-tabs                         │
│                                                                      │
│ ┌── MOD MATRIX ──────────────────────────────────────────────────────┐
│ │  [+ Add Routing]                           Active Routes: 8 / 64   │
│ │                                                                    │
│ │  #  SOURCE              TARGET                DEPTH  POL  SMOOTH   │
│ │  1  LFO 1 (Layer A)  → Filter1 Cutoff         +60%   +   [▐░░░]   │
│ │  2  Mod Env 1        → Osc Wavetable Pos       +40%   +   [░░░░]   │
│ │  3  MIDI Velocity    → Amp Volume              +80%   +   [░░░░]   │
│ │  4  XY Pad Radius    → Reverb Wet Mix          +50%   +   [▐▐░░]   │
│ │  5  Aftertouch       → Filter2 Cutoff          +30%   +   [▐░░░]   │
│ │  6  LFO 2 (Layer A)  → Granular Density        +25%   −   [░░░░]   │
│ │  7  Mod Wheel CC1    → Master Volume           +100%  +   [▐▐▐░]   │
│ │  8  Pitch Bend       → Osc Fine Tune           +200c  +   [░░░░]   │
│ │                                                                    │
│ │  [Mute ●] [Delete 🗑] per row | Drag row to reorder               │
│ └────────────────────────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────────────────────┘
```

**Envelopes sub-tab:** Full multi-stage envelope editor for all Mod Envelopes 1–4 (visual graph with draggable nodes per stage, loop bracket selector).

**LFOs sub-tab:** Detailed view of all 6 LFOs (LFO1/2 per layer A/B/C/D + 2 global LFOs) with waveform preview display, all parameters, per-voice toggle.

---

## 8.6 PAGE 5: FX PAGE

**Purpose:** Build and manage FX signal chains.

```
┌─ FX ─────────────────────────────────────────────────────────────────┐
│  [Layer A FX] [Layer B FX] [Layer C FX] [Layer D FX] [Common] [Master]│
│                                                                      │
│  LAYER A FX RACK                                                     │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐│
│  │ SLOT 0       │ │ SLOT 1       │ │ SLOT 2       │ │ SLOT 3       ││
│  │ Chorus       │ │ Reverb Hall  │ │ [+ Empty]    │ │ [+ Empty]    ││
│  │ ● ON         │ │ ● ON         │ │              │ │              ││
│  │ Rate: [▐▐░░] │ │ Size: [▐▐▐░] │ │              │ │              ││
│  │ Depth:[▐░░░] │ │ Wet:  [▐▐░░] │ │              │ │              ││
│  │ Mix:  [▐▐▐░] │ │ Damp: [▐▐░░] │ │              │ │              ││
│  │ [🔍] [🗑]   │ │ [🔍] [🗑]   │ │              │ │              ││
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘│
│                                                                      │
│  [+ Add FX] → picker modal: category tiles → unit list → insert     │
│  Drag slots to reorder | 🔍 = expanded parameter view                │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 8.7 PAGE 6: ARRANGE PAGE

**Purpose:** DAW-style progression and arrangement within the plugin.

```
┌─ ARRANGE ────────────────────────────────────────────────────────────┐
│  [▶ Play] [■ Stop] [⬤ Rec]  Loop: [|←──────→|]  BPM sync: ● Host   │
│                                                                      │
│  Bars:  1    2    3    4    5    6    7    8                          │
│         ─────────────────────────────────────                        │
│  Chords ██████ Cmaj7 ██████████ Am7 ████████ Fmaj7 ██████ G7 ███    │
│  Melody  ▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪                   │
│  Bass    ▪▪▪▪▪▪▪▪   ▪▪▪▪▪▪▪▪▪▪   ▪▪▪▪▪▪▪▪▪   ▪▪▪▪▪▪                │
│  Phrase1 ▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪▪            │
│                                                                      │
│  [Export MIDI: All Lanes] [Export MIDI: Per Lane]                    │
│                                                                      │
│  Click clip → select | double-click → MIDI piano roll editor         │
│  Right-click → Cut / Copy / Delete / Change chord                    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 8.8 PAGE 7: PERFORM PAGE

**Purpose:** Live performance tools — Scale Lock, Chord Mode, Arpeggiator, XY Pad.

```
┌─ PERFORM ────────────────────────────────────────────────────────────┐
│                                                                      │
│  ┌── SCALE LOCK ─────────────────┐ ┌── CHORD MODE ─────────────────┐│
│  │ [● ENABLED]                   │ │ [● ENABLED]                   ││
│  │ Scale: [Dorian ▾]             │ │ Type: [Major 7th ▾]           ││
│  │ Root: [C ▾]                   │ │ Inversion: [Root ▾]           ││
│  │ Mode: [Remap ▾]               │ │ Spread: [Close ▾]             ││
│  │ (Remap|Mute|Chord|ChordScale) │ │ Scale Constrain: [● ON]       ││
│  └───────────────────────────────┘ └───────────────────────────────┘│
│                                                                      │
│  ┌── ARPEGGIATOR ──────────────────────────────────────────────────┐ │
│  │ [● ENABLED]  Mode: [Up ▾]  Rate: [1/16 ▾]  Oct: [2 ▾]         │ │
│  │ Swing: [▐▐░░░] Latch: [○]  Restart: [●]                        │ │
│  │                                                                 │ │
│  │ STEP GRID (16 steps)                                            │ │
│  │ ┌─┐┌─┐┌─┐┌─┐ ┌─┐┌─┐┌─┐┌─┐ ┌─┐┌─┐┌─┐┌─┐ ┌─┐┌─┐┌─┐┌─┐         │ │
│  │ │●││●││●││●│ │●││●││○││●│ │●││●││●││●│ │●││○││●││●│  ← ON/OFF│ │
│  │ └─┘└─┘└─┘└─┘ └─┘└─┘└─┘└─┘ └─┘└─┘└─┘└─┘ └─┘└─┘└─┘└─┘         │ │
│  │ vel ████▐▐▐▐ █▐▐▐ ████ ██   ← velocity bars per step           │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│  ┌── XY MOD PAD ─────────────────────────────────────────────────┐  │
│  │  ┌─────────────────────────────────────────┐                  │  │
│  │  │                                          │                  │  │
│  │  │              ·  (cursor)                 │                  │  │
│  │  │                                          │                  │  │
│  │  └─────────────────────────────────────────┘                  │  │
│  │  X → [Filter Cutoff ▾]   Y → [Reverb Mix ▾]                   │  │
│  │  Physics: [Free ▾] (Free|Inertia|Attractor)                    │  │
│  │  [⬤ Record 2 bars] [MIDI Learn]                                │  │
│  └───────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 8.9 SETTINGS PAGE (Modal/Overlay)

**Purpose:** Global configuration accessed from ⚙ Settings button in nav bar.

```
┌─ SETTINGS ───────────────────────────────────────────────────────────┐
│ [Audio] [MIDI] [Display] [Paths] [About]                             │
│                                                                      │
│ AUDIO:   Buffer Size [512 ▾]  Sample Rate [48000 ▾]                 │
│ MIDI:    MIDI Input Device [All ▾]  MIDI Channel [All ▾]            │
│          MIDI CC Learn: [open MIDI map table]                        │
│ DISPLAY: UI Scale [100% ▾]  Frame Rate [60fps ▾]  OpenGL: [● ON]    │
│ PATHS:   Sample Library: [Browse...]                                 │
│          User Presets: [Browse...]                                   │
│ ABOUT:   Wolf's Den v1.0.0  |  Powered by JUCE 7+                   │
└──────────────────────────────────────────────────────────────────────┘
```

---

# 9. BUILD SYSTEM NOTES

## 9.1 Requirements

| Requirement | Version |
|---|---|
| CMake | 3.21+ |
| JUCE | 7.0.9+ (fetched via CMake FetchContent) |
| C++ Standard | C++17 |
| macOS SDK | 13.0+ (for Apple Silicon native + Intel via universal binary) |
| Windows SDK | 10.0.19041.0+ (Windows 10/11) |
| Xcode | 14+ (macOS builds) |
| Visual Studio | 2022 (Windows builds) |
| AAX SDK | Avid AAX SDK 2.x (for Pro Tools; must be licensed separately) |

## 9.2 CMakeLists.txt Structure

```cmake
cmake_minimum_required(VERSION 3.21)
project(WolfsDen VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(WolfsDen
    COMPANY_NAME              "Wolf Productions"
    PLUGIN_MANUFACTURER_CODE  "WLFD"
    PLUGIN_CODE               "WfDn"
    FORMATS                   VST3 AU AAX Standalone
    PRODUCT_NAME              "Wolf's Den"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    VST3_CATEGORIES           Instrument Synth
    AU_MAIN_TYPE              kAudioUnitType_MusicDevice
)

# Source files
target_sources(WolfsDen PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/engine/SynthEngine.cpp
    Source/engine/TheoryEngine.cpp
    Source/engine/MidiPipeline.cpp
    Source/engine/FxEngine.cpp
    Source/engine/PresetSystem.cpp
    Source/ui/MainComponent.cpp
    # ... all page components
)

# SQLite3 (bundled as source)
target_sources(WolfsDen PRIVATE
    ThirdParty/sqlite3/sqlite3.c
)

# JUCE modules
target_compile_definitions(WolfsDen PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(WolfsDen PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Tests
add_subdirectory(Tests)
```

## 9.3 Codebase Folder Structure

This is the target structure Cursor will scaffold in TASK_002:

```
/05_ASSETS/apex_project/
├── CMakeLists.txt
├── ThirdParty/
│   └── sqlite3/
│       ├── sqlite3.h
│       └── sqlite3.c               ← single-file amalgamation
├── Source/
│   ├── PluginProcessor.h
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h
│   ├── PluginEditor.cpp
│   ├── engine/
│   │   ├── MidiPipeline.h          ← ScaleLock, ChordMode, Arp
│   │   ├── MidiPipeline.cpp
│   │   ├── TheoryEngine.h          ← ChordDetect, VoiceLead, ScaleAware
│   │   ├── TheoryEngine.cpp
│   │   ├── SynthEngine.h           ← 4-layer synth, Mod Matrix
│   │   ├── SynthEngine.cpp
│   │   ├── FxEngine.h              ← FX racks, all FX unit classes
│   │   ├── FxEngine.cpp
│   │   ├── PresetSystem.h          ← SQLite preset browser
│   │   └── PresetSystem.cpp
│   └── ui/
│       ├── MainComponent.h
│       ├── MainComponent.cpp
│       └── pages/
│           ├── BrowsePage.h / .cpp
│           ├── HarmonyPage.h / .cpp
│           ├── SynthPage.h / .cpp
│           ├── ModPage.h / .cpp
│           ├── FxPage.h / .cpp
│           ├── ArrangePage.h / .cpp
│           └── PerformPage.h / .cpp
├── Resources/
│   ├── Database/
│   │   └── apex.db                 ← SQLite schema + factory chord/scale data
│   └── BinaryData/                 ← factory presets, wavetables (via JUCE BinaryData)
└── Tests/
    ├── CMakeLists.txt
    ├── TheoryEngineTests.cpp
    ├── MidiPipelineTests.cpp
    └── SynthEngineTests.cpp
```

## 9.4 Platform-Specific Notes

**macOS:**
- Universal binary: `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` in CMake invocation
- Code signing: requires valid Apple Developer certificate for DAW load testing
- AU validation: `auval -v aumu WfDn WLFD` to validate AU component post-build
- AU component installs to `~/Library/Audio/Plug-Ins/Components/`
- VST3 installs to `~/Library/Audio/Plug-Ins/VST3/`

**Windows:**
- MSVC 2022 required; MinGW not supported (JUCE limitation for AAX)
- AAX requires Avid's signing tool (pace anti-piracy) — development builds use a dev license
- VST3 installs to `C:\Program Files\Common Files\VST3\`

## 9.5 Build Targets

| Target | Description |
|---|---|
| `WolfsDen_VST3` | VST3 plugin binary |
| `WolfsDen_AU` | Audio Unit (macOS only) |
| `WolfsDen_AAX` | Pro Tools AAX (requires AAX SDK) |
| `WolfsDen_Standalone` | Standalone app for testing without DAW |
| `WolfsDenTests` | CTest-based unit test runner |

---

# 10. TASK_001 CLOSURE — DELIVERABLE CROSSWALK & SCOPE

This section closes **TASK_001** against `/02_TASKS/TASK_001.md`: every required deliverable is present in this document at the section indicated. Subsequent tasks (002–010) implement or verify the design; **TASK_001 is complete when the blueprint exists**, not when every subsystem matches production fidelity.

| TASK_001 deliverable | Location in this ADD |
|---|---|
| System block diagram (text-based) | §1 — Top-Level Architecture diagram; Signal Path Summary |
| Full component list with responsibility of each | §2 — PluginProcessor through Preset System |
| Thread model: UI vs DSP vs MIDI | §3 — Primary/secondary threads; Thread Communication Summary |
| Data flow: MIDI in → Theory → Synth → FX → audio out | §4 — full pipeline; §1 signal path |
| Parameter system: definition, typing, automation | §5 — APVTS, IDs, types, MIDI learn, smoothing |
| State management: presets saved/recalled | §6 — serialization, preset workflow, Sound Lock |
| Database schema: chord sets, scales, presets, tags | §7 — tables, indexes, relationships |
| Full UI page map: every page, panel, key controls | §8 — Browse through Settings |
| Build system notes: JUCE + CMake | §9 — requirements, CMake structure, folders, platforms, targets |

**Scope boundary:** Conformance of the running plugin to this ADD (feature parity, deferred UI flows, host QA) is owned by **implementation tasks 002–009** and **verification in TASK_010**. TASK_001 does not re-open when implementation lags the design; gaps are tracked in those tasks’ reports and closed there in order.

---

# DECISIONS LOG

All architectural decisions are justified against the reference plugin research in `/01_BRIEF/vst_research.md`.

| Decision | Rationale |
|---|---|
| JUCE over Qt | Scaler 3 uses JUCE; JUCE is purpose-built for audio plugins with processBlock(), MidiBuffer, APVTS. Qt is a general UI framework (KK uses it) lacking audio primitives. |
| APVTS for parameters | Industry standard for JUCE VST; provides automation, undo, state serialization and thread-safe parameter access out of the box. |
| 4 synthesis layers | Mirrors Omnisphere's A/B/C/D layer architecture — sufficient for rich layered sounds. Beyond 4 layers yields diminishing returns at high CPU cost. |
| SQLite for presets/DB | Lightweight, single-file, no server required. Scaler 3 uses embedded DB for chord data. Supports tag-based queries efficiently via indexes. |
| Lookup table for Scale Lock | O(1) real-time performance; matches KK's proven implementation. 128-entry array trivially rebuilt on scale change in microseconds. |
| Lock-free inter-thread comm | DSP thread cannot block. JUCE AbstractFifo provides the single-producer/single-consumer lock-free pattern. Zero OS lock contention. |
| OpenGL for visualizers | Optional — granular particle display and spectrum analyzer benefit from GPU. Disabled by default (software renderer fallback). Mirrors Omnisphere's granular 3D visualizer. |
| 7-page UI model | Each pillar gets dedicated space (Harmony, Synth, Mod, FX) plus workflow pages (Browse, Arrange, Perform). Avoids Omnisphere's deep nesting while preserving discoverability. |

---

*Document Version: 1.1 | Updated: 2026-04-12 | §10 closure crosswalk added | Task: TASK_001*
