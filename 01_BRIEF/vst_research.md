# VST DEEP DECONSTRUCTION — MASTER DOCUMENT
## Scaler 3 | Omnisphere | Komplete Kontrol
## For Agent Use: VST Build Reference

---

# ═══════════════════════════════════════
# SCALER 3
# ═══════════════════════════════════════

## BUILD STACK
- **Framework:** JUCE (C++) — confirmed via JUCE case study and Plugin Boutique dev stack
- **Plugin Formats:** VST3, AU, AAX + Standalone App (JUCE handles all formats via single codebase)
- **Rendering Engine:** JUCE software renderer (custom-painted components via JUCE Graphics class)
- **UI Scaling:** JUCE `setResizable()` + `setResizeLimits()` + `AffineTransform::scale()` for HiDPI
- **Database:** Internal chord/scale database (likely SQLite embedded)
- **MIDI Processing:** JUCE MIDI buffer manipulation (MidiBuffer, MidiMessage)
- **Plugin Hosting:** JUCE AudioPluginHost infrastructure for hosting 3rd-party VST/AU inside Scaler 3

---

## UI ARCHITECTURE — FULL BREAKDOWN

### Top-Level Structure
```
AudioProcessorEditor (root — DAW-facing)
└── MainContentComponent (scales, contains all panels)
    ├── TopNav Bar (Browse | Create | Arrange tabs + Transport)
    ├── ActivePageView (swapped component for each page)
    │   ├── BrowsePage
    │   ├── CreatePage
    │   └── ArrangePage
    ├── PianoKeyboard (persistent bottom - MIDI display + audition)
    └── StatusBar / Output Meter
```

### Navigation Model
- 3 page tabs top-left: **Browse → Create → Arrange**
- Clicking a tab swaps the `ActivePageView` component — pages are NOT separate windows, they are component-swap views
- State persists across page switches (chord data model lives in the AudioProcessor, not the UI)
- Transport controls top-right: Play, Stop, Record, Loop — persistent across all pages

---

## PAGE-BY-PAGE UI BREAKDOWN

### BROWSE PAGE
**Purpose:** Entry point for content discovery
**Layout:**
- Left panel: Filter sidebar (Genre, Mood, Energy, Scale, Instrument)
- Center: Scrollable grid of chord set cards — each card shows name, root key, mood tags
- Right panel: Preview/details pane when card selected
- Search bar at top
- Favorites system (star icon on cards)
**Content:**
- 860+ chord sets tagged by genre (Trap, Lo-Fi, R&B, Jazz, Classical, etc.) and mood (Dark, Bright, Tense, Emotive)
- Common vs. Uncommon progressions categories
- Custom user-uploaded sets in "My Sets"
- Filter by root note: click keyboard at bottom → shows only scales containing that note

**Interaction Flow:**
1. User selects filter tags (genre + mood)
2. Grid dynamically filters cards
3. Click card → loaded into Section A (left chord slots)
4. Drag to Section C (Main Track) to use in progression

---

### CREATE PAGE — Sub-Pages

#### Sub-tab: SCALES
- Full scale browser
- "Mother scale" shown for each chord subset (e.g., C Dorian within C Natural Minor family)
- Notes of scale visualized on mini keyboard
- Select scale → chords generated from it populate Section B

#### Sub-tab: CIRCLE OF FIFTHS
- Visual wheel: major scales outer ring, minor scales inner ring
- Adjacent = closely related (shared notes), opposite = maximally distinct
- Click a scale segment → updates chord suggestions
- Used for harmonic modulation planning

#### Sub-tab: EXPLORE ("Harmonic Universe")
- Cards organized by mood (Bright, Dark, Neutral, Dreamy, etc.) NOT by scale
- Chords untied to a specific scale — cross-scale exploration
- "Constellations" layout:
  - Root chord = bottom center
  - Relative major/minor = top center
  - Inner circle = closely related chords
  - Outer circle = experimental/adventurous chords
- Used for bypassing theory constraints

#### Sub-tab: COLORS (Voicing / Extensions Hub)
- Grid of harmonic alternatives for each chord in current set
- Each row = a different voicing/extension class:
  - Basic triads
  - 7th chords (maj7, min7, dom7, half-dim, dim7)
  - 9th extensions
  - 11th/13th extensions
  - Altered chords
  - Suspended chords (sus2, sus4)
  - Chord substitutions (tritone sub, neapolitan, etc.)
- Color-coding: blue = diatonic to scale, gray = chromatic/outside scale
- Click any cell → auditioning that variant
- Drag-drop to replace chord in Main Track

**Voicing Modifier Sidebar (left panel):**
- Inversion: 1st / 2nd / 3rd inversion (which note is in bass)
- Density: number of notes (thin = 3 notes, thick = 5+ with doublings)
- Octave shift: whole chord up/down by octave
- Extract Voicing: captures specific note spacing of a chord for applying to others
- Voice Grouping: splits chord voices to different MIDI channels for multi-instrument setups

---

### ARRANGE PAGE
**Purpose:** DAW-style multi-lane timeline arrangement within the plugin
**Layout:**
```
[Transport bar - top]
[Master Chord Lane]     ← primary, controls others
[Melody Lane]
[Bass Lane]
[Phrase/Motion Lane 1]
[Phrase/Motion Lane 2]
[Plugin Host Lane]      ← hosted VST/AU instrument
```

**Mechanics:**
- Each lane = independent clip-based track
- Clips can be: moved (drag), resized (drag edges), cut (scissor tool), duplicated
- All lanes locked to master Chord Lane timing — chord change triggers re-harmonization of other lanes
- Bass Follow: bass lane auto-follows root note of chord track (isolated from chord stack)
- Piano Roll editor: lightweight embedded MIDI editor for Motions customization
- Clip colors: different Motion types have distinct colors for quick identification

**MIDI Export:**
- Drag-drop any clip/lane directly into DAW timeline
- Export button: exports entire arrangement or loop region as MIDI
- Options: single combined MIDI track OR separate tracks per lane
- Note: arrangement must start at Bar 1 to avoid timing offset bugs

---

## SECTION MODEL (Scaler's Internal Data Layout)

Scaler uses a 3-section internal model regardless of page:
- **Section A:** Chord discovery/bank — "palette" of available chords from a chord set
- **Section B:** Extended scale/theory view — all chords from parent scale
- **Section C (Main Track):** User's built progression — the active arrangement

This model persists across pages and powers everything.

---

## FEATURE DEEP DIVES

### CHORD DETECTION ENGINE
**MIDI Detection:**
1. Scaler opens a MIDI input listener (JUCE MidiInput)
2. Incoming Note On messages logged over a window (~500ms)
3. Active notes captured as a set of pitch classes (C=0, C#=1... B=11)
4. Pitch class set compared against chord definition database
5. Best match algorithm: greatest number of matching pitch classes with fewest extra notes
6. Returns: chord name + all possible interpretations (e.g., C-E-G could be Cmaj OR Am/C)
7. Scale context: matched chord tested against all known scales to show parent scale

**Audio Detection:**
1. Audio input analyzed via FFT (Fast Fourier Transform)
2. Fundamental frequencies extracted from FFT bins
3. Frequencies quantized to chromatic pitch classes (12-TET)
4. Same pitch class matching as MIDI detection
5. Sensitivity knob: adjusts FFT smoothing + matching threshold
6. Sidechain: plugin receives audio from DAW routing

---

### KEYS LOCK (MIDI Remapping Engine)
**Internal model:** Pre-output MIDI interceptor — sits between user input and instrument output

**Per-Mode Behavior:**
| Mode | Behavior | Algorithm |
|---|---|---|
| Scale Notes Mapped | Off-scale note → nearest in-scale note | Lookup table: each MIDI note [0-127] maps to closest scale pitch |
| Scale Notes Only | Off-scale note → muted | Gate filter: pass if note in scale set, else discard |
| Chord Notes | All keys remapped to chord tones only | Sparse lookup: N chord tones spread across 128 MIDI notes |
| Chord Scales | Scale remaps dynamically per active chord | On chord change: recalculate scale for that chord, regenerate lookup table in real-time |

**Implementation:**
- Lookup tables: array[128] of MIDI note values — precomputed, very fast
- On chord change (Chord Scales mode): table rebuilt in microseconds
- Output: remapped MIDI note passed through to instrument unchanged in velocity/timing

---

### VOICE LEADING ALGORITHM
**Goal:** Minimum total pitch movement between consecutive chords

**Process:**
1. Current chord = set of MIDI notes [n1, n2, n3, ...]
2. Next chord = root + quality (e.g., Fmaj = F A C)
3. Generate all inversions of next chord (root, 1st inv, 2nd inv, open voicing variants)
4. For each inversion: calculate sum of |current note - next note| for all voice pairs
5. Select inversion with lowest total distance score
6. Output that specific inversion

**Voice Grouping:**
- Voices assigned to MIDI channels: bass = Ch1, tenor = Ch2, alto = Ch3, soprano = Ch4
- Each channel routed to separate instrument track in DAW
- Enables realistic orchestral/choral voice distribution

---

### MOTIONS ENGINE
**What Motions are:** Pre-recorded MIDI performance sequences stored as relative pitch data (not fixed pitches)

**Content:** Arpeggios, strumming patterns, bass patterns, melodic phrases — all tagged:
- Energy: Low / Medium / High
- Mood: Bright, Dark, Tense, Neutral, Cinematic, etc.
- Style: Electronic, Acoustic, Orchestral, Urban, etc.
- Instrument suggestion: Piano, Guitar, Synth, Bass, etc.

**Playback Mechanics:**
1. Motion sequence = array of {relative pitch offset, timing, velocity, duration}
2. Root note = root of active chord
3. Scale = active scale for that chord
4. Each relative offset remapped to nearest in-scale note above root
5. Result: same phrase sounds musically correct over any chord

**Custom Editing:**
- Embedded MIDI piano roll in Arrange page
- Double-click Motion clip → opens editor
- Can edit individual notes, velocities, timing
- Save as user Motion

---

### LIVE SYNC
**Architecture:** Leader/Follower IPC model
- **Leader instance:** broadcasts Main Track chord state
- **Follower instances:** subscribe and mirror leader's chord state
- Communication: inter-plugin communication via shared plugin state (not MIDI routing)
  - Mechanism: DAW plugin host memory / JUCE inter-process shared memory
- New instance added to project auto-detects leader and sets itself to Follow
- Manual disconnect: edit follower's Main Track → prompted to break sync
- Rejoin: set back to Follow in Live Sync menu
- Works cross-format: instrument plugin instance can lead audio effect plugin instance

---

### PLUGIN HOSTING (Scaler as a mini-DAW)
**Implementation:** JUCE AudioPluginFormat + AudioPluginInstance
- Scaler scans for installed VST3/AU plugins
- User selects instrument from list inside Scaler
- Plugin loaded into JUCE AudioPluginHost container within Scaler's process
- Scaler's MIDI output routed to hosted plugin's MIDI input internally
- Hosted plugin's audio output piped to Scaler's audio output
- Real-time: no latency added beyond normal plugin processing chain

---

### STATE MANAGEMENT / PRESETS (VST Standard)
- All Scaler parameters exposed as JUCE AudioProcessorParameter objects (fixed IDs)
- Complex state (chord data, arrangements, content) saved as binary chunk via `getStateInformation()` / `setStateInformation()`
- DAW saves chunk in project file → full recall on load
- Automation: any exposed parameter gets a fixed 32-bit ID → DAW writes automation curves per ID
- Window size saved/restored in state chunk

---

# ═══════════════════════════════════════
# OMNISPHERE
# ═══════════════════════════════════════

## BUILD STACK
- **Core Engine:** STEAM (Spectrasonics Time-based Environment and Modulation) — 100% proprietary C++
- **UI Framework:** Custom proprietary C++ rendering pipeline (NOT JUCE, NOT Qt)
  - Texture-based compositing engine (pre-rendered bitmaps composited at runtime)
  - OpenGL / Metal (macOS) / DirectX (Windows) for GPU-accelerated rendering
  - Custom scene graph (C++ object tree mapping UI elements to DSP parameters)
- **Plugin Formats:** VST3, AU, AAX
- **Sample Streaming:** Custom sample streaming engine — library too large to load into RAM
- **Database:** Custom patch metadata database (proprietary format)
- **Thread Model:** UI thread strictly separated from DSP audio thread via lock-free ring buffers

---

## UI ARCHITECTURE — FULL BREAKDOWN

### Top-Level Structure
```
Plugin Window (OS-native window)
├── Persistent Header Bar
│   ├── Part Selector (1-8) + Multi button
│   ├── Master Volume
│   ├── Utility Menu (Save, Copy, MIDI Learn, etc.)
│   └── Browser Toggle Buttons (Patches, Multis, Soundsources)
├── Main View Area (swaps based on active tab)
│   ├── Main Page
│   ├── Layer Pages (A, B, C, D)
│   ├── FX Pages (Layer FX, Common FX, Part Aux, Multi Aux, Master FX)
│   ├── Arpeggiator Page
│   ├── Modulation / Mod Matrix Page
│   └── Hardware Page
└── Browser (floatable panel)
    ├── Search Field
    ├── Filter Tags (Type, Character, Attribute)
    ├── Results List (sorted by Sound Match score when active)
    └── Sound Lock Controls
```

### UI Rendering Model
- All UI elements are bitmap textures composited in layers at GPU level
- Controls (knobs, sliders, buttons) are NOT OS-native widgets — fully custom-drawn
- Custom scene graph: each control = C++ object with position, state, render callback
- Knob interaction: click + drag vertical → value change. Shift+drag = fine adjust
- Zoom icons (🔍 magnifying glass): open a new zoomed panel/window for detailed editing
- Context menus: right-click any parameter → contextual options (MIDI Learn, Modulate, Automation, etc.)
- Real-time visual feedback: LED-style indicators show module on/off state

### Navigation Model
- **Multi level:** up to 8 Parts — Part selector tabs across top
- **Part level:** per-Part tabs below header (Main, Edit, FX, Arp, Mod)
- **Layer level:** within Edit/Layer page — Layers A/B/C/D shown as sub-tabs or toggle lights
- **Zoom level:** clicking magnifying glass icon → opens zoom window for deep parameter editing
- All navigation = panel swaps, no separate windows (except Browser floats)

---

## PAGE-BY-PAGE UI BREAKDOWN

### MAIN PAGE (Per-Part Dashboard)
- Patch name display + browse buttons (Previous/Next patch)
- Part volume + pan fader
- Coarse/Fine tune
- Pitch Bend range
- Layer A/B/C/D: mini overview strip per layer (shows oscillator type, filter settings, on/off toggle)
- **The Orb:** Center-left — circular XY performance controller
- Envelope ADSR quick controls (attack, decay, sustain, release sliders)
- Basic filter quick controls (cutoff, resonance)
- Velocity/Aftertouch response curves
- Performance controls (legato, portamento, polyphony)

### LAYER / EDIT PAGES (Per-Layer — 4 available)
**Oscillator Section:**
- Source type selector: Synth (DSP) | Sample (Soundsource browser)
- Synth mode: waveform picker (Sine, Saw, Square, Triangle, Noise variants, FM, Ring Mod, Wavetable, etc.)
- Sample mode: Soundsource browser embedded, shows waveform display of loaded sample
- Octave, semitone, fine tune knobs
- Harmonia: number of additional oscillator harmonics + detune + pan spread
- Unison: number of voices + detune + stereo spread
- Granular: grain size, density, position, pitch scatter, pan scatter (when granular mode active)

**Filter Section (Dual Filter per Layer):**
- 34 filter types:
  - Low Pass: 6dB, 12dB, 18dB, 24dB, 36dB slopes
  - High Pass: same slope variants
  - Band Pass
  - Notch
  - Comb Filter
  - Formant/Vowel filters
  - State Variable (LP/BP/HP switchable)
  - Various resonant specialty types
- Filter 1 + Filter 2: each fully independent
- Routing: Series (F1→F2) or Parallel (F1+F2)
- Key Tracking: filter cutoff tracks MIDI pitch
- Cutoff, Resonance, Envelope Depth, LFO Depth quick controls
- Filter Zoom (🔍): detailed per-filter editing including Drive, Scale, additional routing

**Envelope Section:**
- Per-layer: dedicated Amp Envelope + Filter Envelope
- Per-Part (shared): 4 Modulation Envelopes
- Simple ADSR on main page
- Envelope Zoom (🔍): full multi-stage envelope editor
  - Add stages: standard ADSR upgrades to multi-segment (hundreds of stages)
  - Curve types per stage: linear, exponential, S-curve, logarithmic
  - Tempo sync: all stages can sync to host BPM
  - Loop: loop specific stages at tempo-synced rate

**LFO Section:**
- 2 LFOs per layer (+ global LFOs shared across layers)
- Waveforms: Sine, Tri, Saw, Square, S&H (Sample and Hold), Random, Noise, custom drawn
- Rate: Hz or tempo-synced (1/32 to multiple bars)
- Delay, Fade-in
- Phase offset
- Per-voice vs. shared mode

### FX PAGE (Signal Path Architecture)
```
Layer A → [Layer A FX Rack (4 slots)]
Layer B → [Layer B FX Rack (4 slots)]
Layer C → [Layer C FX Rack (4 slots)]
Layer D → [Layer D FX Rack (4 slots)]
          ↓ (summed)
     [Common FX Rack (4 slots)]
          ↓
     [Part Aux FX Rack (send)] ←── Aux send from any layer
          ↓
     [Multi Aux Racks 1-4] ←────── shared across all 8 Parts
          ↓
     [Master FX Rack] ←──────────── final output stage
```

**58 FX Units — Categories:**
- **Dynamics:** Compressor, Limiter, Gate, Transient Shaper
- **EQ:** Parametric EQ, Graphic EQ, Lo-Fi
- **Filters:** Lowpass, Highpass, Bandpass (as effects, additional to synth filters)
- **Distortion:** Amp Sim, Waveshaper, Bit Crusher, Overdrive, Fuzz
- **Chorus/Flanger/Phaser:** Multiple variants (stereo, multi-voice)
- **Delay:** Stereo Delay, Tape Delay, Multi-tap Delay, Ping Pong
- **Reverb:** Hall, Room, Spring, Plate, Convolution
- **Modulation FX:** Ring Mod, Tremolo, Auto-Pan, Vibrato
- **Spectral:** Resonators, Harmonizer, Frequency Shifter
- **Special:** PS-22 Spread (stereo widening), Retroplex (vintage tape sim)

Each FX unit: fully modulatable via Mod Matrix (any LFO/Envelope → any FX parameter)

### ARPEGGIATOR PAGE
**Layout:**
- Play/Stop button
- Latch toggle
- Clock rate (1/32, 1/16, 1/8, 1/4, etc. — host synced)
- Play Mode: Up, Down, Up+Down, As Played, Random, Chord
- Steps: horizontal grid of up to 32 steps

**Per-Step Controls:**
- On/Off toggle (click step button)
- Velocity bar (drag up/down per step)
- Duration bar (drag right edge to extend)
- Tie next step (double-click next step = legato connection)
- **Step Modifier menu (per step):**
  - Transpose: ±24 semitones
  - Slide (portamento to target pitch)
  - Ratchet/Divider (multiply triggers within single step)
  - Chord (play full chord from that step)

**Special Features:**
- Groove Lock: sync pattern rhythm to external MIDI groove file (Stylus RMX compatible)
- MIDI Capture: record generated arpeggio → drag as MIDI into DAW
- Save pattern as preset

### MODULATION PAGE (Flex-Mod + Mod Matrix)
**Flex-Mod (Quick Assign):**
- Right-click any knob/slider → "Modulate with LFO" or "Modulate with Envelope"
- System auto-assigns next available LFO/Envelope slot
- Routing appears instantly in Mod Matrix view

**Mod Matrix Zoom:**
- Grid view of all active Source → Target routings
- Per-routing controls:
  - Amount (depth)
  - Polarity (+/-)
  - Invert toggle
  - Smooth (soften harsh modulation transitions)
  - Mute (toggle off without deleting)
  - Red dot indicator: target parameter being pushed out of its valid range

**Sources:** LFO 1, LFO 2, Amp Envelope, Filter Envelope, Mod Envelope 1-4, MIDI Mod Wheel, Aftertouch, Velocity, Breath Controller, Pitch Bend, Orb Angle, Orb Radius, Random, Alternating, MIDI CC (any)
**Targets:** Any parameter in the synth + any FX parameter (cutoff, pitch, volume, pan, FX depth, FX rate, etc.)

---

## FEATURE DEEP DIVES

### SOUND MATCH (Tag-Based Similarity Engine)
1. Every patch has metadata record: Type, Character, Attributes, Sub-type tags
2. Each patch = tag vector (set of associated tags)
3. "Sound Match" query: compare selected patch's tag vector against entire database
4. Score = number of shared tags (possibly weighted by tag importance)
5. Results ranked by score — displayed as match strength bar
6. Dynamic: select result → run new match from that patch ("steer" search direction)
7. Browser filters constrain search pool before scoring runs

### SOUND LOCK (Module-Level State Protection)
**States:** Each sub-module (FX Chain, Arpeggiator, Mod Matrix, Envelopes, Filters, Tuning, Polyphony) = separate serializable object
**Lock mechanism:**
1. User locks specific module(s) via Sound Lock controls
2. Module flagged as "immutable" in state management layer
3. On patch load: engine loads oscillator/sample data of new patch
4. Immutable modules: their state data blocks skipped during patch load
5. Result: new oscillators/samples playing through old FX/Arp/Envelope settings
6. Multiple locks: stack freely — build entirely new hybrid patches by browsing

### THE ORB (Multi-Parameter Performance Controller)
**UI:** Circular XY pad — cursor moves within concentric circles
**Core Model:**
- **Angle** (0°-360°): selects which parameter "scenes" are affected
- **Radius** (0.0-1.0): intensity of modulations applied

**Modulation Mechanics:**
- Angle and Radius are exposed as sources in Mod Matrix
- Pre-assigned targets (patch-specific): generated by Dice randomization OR manually set
- "Dice" button: randomizes target assignments while selecting musically intelligent parameters for the current patch type

**Physics Modes:**
- **Free (default):** Direct cursor control — cursor stays where you leave it
- **Inertia:** Cursor has momentum physics — velocity + damping coefficient applied. Cursor orbits/bounces after release
- **Attractor:** Chaotic attractor math (Lorenz-style) — cursor follows non-linear path creating organic, unpredictable motion

**Recording:**
- Record Orb movement as loop (1, 2, 4 bars — tempo-synced) or free-form
- Saved within patch state
- Can assign Orb control to MIDI CC via MIDI Learn for live hardware control

### HARDWARE INTEGRATION
- 300+ supported hardware synth profiles (Moog, Roland, Korg, etc.)
- Each profile = MIDI mapping config + parameter groupings for that device's physical controls
- Setup: select hardware model in Hardware menu → integration active immediately
- "Link Omnisphere GUI": when hardware knob touched → Omnisphere UI auto-zooms to that parameter's page
- Hardware Library: patches designed specifically using each hardware profile — accessible to all users regardless of owning the hardware

### SIGNAL PATH MODES (Layer Architecture)
- **Independent Mode (default):** Each Layer (A/B/C/D) has fully separate oscillator → filter → envelope → FX chain → mixer signal path
- **Shared Mode:** Layers B, C, D share Layer A's signal path — CPU efficient, less flexibility
- Aux Sends: Pre-fader or Post-fader send to Part Aux Rack and Multi Aux Racks

### GRANULAR SYNTHESIS ENGINE (Deep)
- Audio file decomposed into overlapping grains (1ms - 500ms each)
- Parameters: Position (playhead), Size (grain length), Density (grains/sec), Pitch (grain pitch vs source), Scatter (random pitch deviation), Pan Scatter, Phase Scatter
- Windowing: each grain has amplitude envelope (Hanning, Gaussian, Rectangular, etc.) to prevent clicks at grain boundaries
- Real-time 3D visualization: each active grain = particle in GPU-rendered visualizer (OpenGL shader)
- Freeze function: locks playhead position, holds continuous granular wash

---

# ═══════════════════════════════════════
# KOMPLETE KONTROL
# ═══════════════════════════════════════

## BUILD STACK
- **UI Framework:** Qt / QML (C++) — migrated to Qt in v3.0 update
- **Rendering:** Qt's default rendering pipeline (backend: Metal/macOS, Direct2D/Windows)
- **Plugin Formats:** VST3 primary (VST2 dropped at 3.0)
- **Hardware Protocol:** USB HID + NKS 2 (Native Kontrol Standard) — bidirectional
- **Plugin Wrapper:** Proprietary wrapping engine scans + hosts instruments inside KK
- **State Persistence:** Qt state + standard VST3 preset/chunk system

---

## UI ARCHITECTURE — FULL BREAKDOWN

### Top-Level Layout
```
Application Window (Qt QMainWindow or QWidget)
├── Global Header (top bar — persistent)
│   ├── KK Logo button (About screen)
│   ├── Browser Toggle icon
│   ├── Preset name display + Prev/Next arrows
│   ├── Main Menu button
│   ├── Transport controls (Play, Record)
│   ├── Master Volume knob
│   └── View Selectors:
│       ├── Perform Panel toggle (Scale/Arp — lights blue when active)
│       ├── Plug-in Panel toggle (mapping view)
│       └── Plug-in Chain toggle (signal chain strip)
├── Browser Panel (side panel, togglable)
│   ├── Search field
│   ├── Product Type selector (Instruments | Effects | Loops | One-shots)
│   ├── Library Tile Grid (installed products as visual tiles)
│   ├── Tag Filters (Sound Type + Character — NKS metadata)
│   ├── User/Factory toggle
│   ├── Results list (scrollable)
│   └── Info Pane (metadata edit / tagging)
└── Main Center Area
    ├── Plug-in Chain Strip (series chain of loaded instruments/effects)
    ├── Instrument View (loaded plugin's own GUI embedded here)
    │   ├── Default View (streamlined)
    │   ├── Additional View (more controls)
    │   └── Edit View (full UI — Kontakt/Reaktor)
    └── Perform Panel (Scale/Chord/Arp controls — shown when toggled)
```

### Key UI Behavior (v3.0 Change)
- v2.x: Browser was persistent sidebar alongside plugin view (split screen)
- **v3.0:** Browser and Plugin View are MUTUALLY EXCLUSIVE — user toggles between them
- Intentional design: focuses user on one task (browse OR play). Controversial — many power users preferred split view
- HiDPI: Qt enables proper display scaling — fixed v2.x fixed-size limitation
- Plugin resizing: hosted plugin GUIs now resize properly within KK wrapper

---

## BROWSER SYSTEM — DETAILED

### Tag System (NKS Metadata)
- Every preset stores NKS metadata file alongside plugin preset
- Tags: Sound Type (Instrument category), Character (tonal qualities like Warm, Bright, Dark, Punchy, etc.), Author, BPM, Key
- Tag Filters = query against NKS metadata database
- Library Tile filter = narrows results to specific installed product
- Smart Folders: User can save filter combinations as saved searches

### Preset Display
- Shows preset name, author, product source, assigned tags
- Star (favorite) system
- Recently played history
- Missing presets detection — flags presets whose source library is not installed

---

## SMART PLAY — MIDI PIPELINE (Complete Detail)

### Full Signal Chain
```
Physical Key/Pad Press (S-Series Hardware)
→ USB HID: raw MIDI Note On + Velocity sent to laptop
→ KK Application receives MIDI
→ [SCALE ENGINE]
  - Lookup table: 128-entry array maps each MIDI note to in-scale equivalent
  - Scale database: 58+ scales (Major, Minor, Dorian, Phrygian, Locrian, Pentatonic, etc.)
  - Non-scale note → snapped to nearest in-scale note
  - Root note offset applied (transpose to any key)
→ [CHORD ENGINE]
  - Take processed note as root
  - Apply chord interval math: major = +4, +7; minor = +3, +7; maj7 = +4, +7, +11; etc.
  - All generated chord notes passed through Scale Engine (scale-constrained)
  - 15+ chord types available (triad, 7ths, 9ths, 6ths, sus, add, etc.)
  - Inversion selector: root, 1st, 2nd, 3rd
→ [ARPEGGIATOR ENGINE]
  - Input: single note OR full chord (from Chord Engine)
  - Notes held in chord register buffer
  - Internal clock divides host DAW tempo by rate setting
  - Rate options: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 (+ dotted and triplet variants)
  - Play Modes: Up, Down, Up+Down, Order (sequence of key press order), Chord, Random
  - Ratchet: steps can repeat within a single step for stutter effect
  - Swing: shifts every other note by swing percentage
  - Latch: notes held in register after key release (keys lock in pattern)
  - Restart: pattern restarts on new key press vs. free-running
→ [OUTPUT]
  → VST3 Plugin loaded in KK (internal routing)
  → OR DAW MIDI track (via KK's virtual MIDI port)
```

### Scale Engine — Implementation
- 128-entry lookup table pre-computed per scale selection
- Rebuild time: microseconds (single array rewrite)
- Root note setting: entire table shifted by root offset
- 58+ scales stored as interval arrays: e.g., Major = [0,2,4,5,7,9,11] (semitone offsets from root)
- Scale notes in table = exact pitch of first available in-scale note at or above input pitch

### Light Guide — Hardware LED Sync
- After Scale Engine computes lookup table:
  - Root notes → HIGH brightness LED
  - Other in-scale notes → LOW brightness LED  
  - Out-of-scale notes → LED OFF
- LED state array sent from KK software → S-Series keyboard via USB HID protocol
- Updates in real-time when scale or root note changes
- Keyswitch ranges: separate LED colors for instrument keyswitches (when NKS data provides ranges)

### NKS 2 Standard (Plugin Integration Protocol)
**Purpose:** Standardized way for any plugin to expose itself to KK hardware
**Consists of:**
1. **NKS metadata file:** JSON-like descriptor alongside plugin preset
   - Parameter names, IDs, ranges, units
   - Page groupings (8 params per page → maps to 8 hardware knobs)
   - Icons, colors, descriptions
2. **VST3 ParameterInfo:** Rich parameter data from VST3 API
   - KK reads this to auto-map params even without explicit NKS file
3. **Hardware display data:** Param names + values sent to S-Series screen in real-time
4. **Keyswitch data:** Note ranges for articulations → LED color mapped on keyboard
5. **Polyphonic Aftertouch:** Hardware sends per-note pressure data, KK routes to plugin via MPE

**Plugin View Modes (NKS-aware plugins):**
- Default View: 8-knob macro view (most accessible)
- Additional View: secondary parameter page
- Edit View (Kontakt/Reaktor): full native GUI

---

## PERFORM PANEL — UI DETAIL

### Scale Section (UI)
- Scale type dropdown (58+ scales)
- Root note selector (12 chromatic pitches)
- Chord selection within scale: highlights which chords of the scale are IN key
- Active/Inactive toggle

### Chord Section (UI)
- Chord type dropdown (15+ types)
- Inversion selector (Root, 1st, 2nd, 3rd)
- Chord mode: Single (one chord type), Scale (chord type from scale harmony)

### Arpeggiator Section (UI)
- Rate knob (with host-sync options)
- Play Mode selector
- Octaves range (1, 2, 3, 4)
- Swing knob
- Ratchet toggle + amount
- Latch button
- Restart toggle
- Sequence Step edit (mini step grid for custom note ordering)

---

# ═══════════════════════════════════════
# COMBINED INSIGHTS: HOW TO BUILD BETTER
# ═══════════════════════════════════════

## GAPS TO EXPLOIT (Where They Fall Short)

### Scaler 3 Gaps
- No real synthesis engine — chord/MIDI only, you still need another instrument
- Browser limited to pre-set content — no user-generated content AI
- Voice leading is basic (interval minimization) — no stylistic rules (genre-aware voicing)
- No audio generation — purely MIDI-output
- Live Sync limited to chord sharing — no shared modulation, dynamics, rhythm

### Omnisphere Gaps
- No music theory layer — no chord awareness, no scale constraints
- UI is extremely deep but also extremely complex — high learning curve
- Hardware integration excellent but software-only workflow lacking
- No cross-instance sync
- Sound Match is tag-only — not AI/audio similarity
- No generative composition tools

### Komplete Kontrol Gaps
- Just a wrapper — no synthesis engine of its own
- Smart Play is basic music theory: lookup tables only, no ML or context awareness
- Browser requires NKS metadata — third-party plugins underserved without NKS
- No arrangement / composition tools at all
- Arpeggiator is functional but no deep step modifiers like Omnisphere has
- No audio detection / key detection

## A MORE POWERFUL VST WOULD COMBINE:
1. **Scaler's music theory intelligence** — chord detection, scale awareness, voice leading
2. **Omnisphere's synthesis depth** — multi-layer engine, full modulation matrix, granular, FX routing
3. **KK's hardware-software pipeline** — standardized MIDI transformation, hardware display, LED feedback
4. **New: AI-driven content** — generative chord progressions, style-aware voice leading, audio-similarity browsing (not tag-matching)
5. **New: Cross-synthesis** — chord trigger → granular synthesis, chord change → modulation event in synth engine
6. **New: Real audio output** — synthesis + effects, not just MIDI generation

---

*Research compiled: 2026-04-08 | Version 2 — Full Detail*
