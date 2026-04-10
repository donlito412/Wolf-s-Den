TASK ID: 009

STATUS: DONE

GOAL:
Build the full custom UI for Wolf's Den using JUCE's Component system. This is a completely custom-rendered interface — no OS-native widgets. The UI must be visually professional, responsive at 60fps, resizable, and organized into logical pages that match the engine architecture. This is the face of the product.

ASSIGNED TO:
Cursor

INPUTS:
/03_OUTPUTS/001_architecture_design_document.md
/03_OUTPUTS/004_synthesis_engine_report.md
/03_OUTPUTS/005_theory_engine_report.md
/03_OUTPUTS/006_midi_pipeline_report.md
/03_OUTPUTS/007_modulation_system_report.md  ← READ THIS — target list updated to 26 targets after gap fix
/03_OUTPUTS/008_fx_engine_report.md
/03_OUTPUTS/task_gap_fixes.md  ← READ THIS — documents all confirmed-complete features + the ModMatrix gap fix
/01_BRIEF/vst_research.md

OUTPUT:
/03_OUTPUTS/009_ui_report.md
(Code in /05_ASSETS/apex_project/Source/ui/)

DELIVERABLES INSIDE OUTPUT:
- All pages rendering at 60fps confirmed
- All controls connected to AudioProcessorValueTreeState (live parameter changes work)
- Resize test: plugin resizes from 800×600 to 1600×1000 without breaking layout
- Dark mode UI screenshots per page

---

DESIGN SYSTEM:

Color Palette:
- Background Dark: #0D0D0F
- Background Mid: #1A1A22
- Panel Surface: #22222E
- Accent Primary: #7B5EA7 (muted purple — premium)
- Accent Hot: #C084FC (bright purple — active states)
- Accent Alt: #38BDF8 (sky blue — secondary accents)
- Text Primary: #F0F0F5
- Text Secondary: #8888A8
- Text Disabled: #444460
- Success: #4ADE80
- Warning: #FBBF24
- Error: #F87171

Typography:
- Font: Inter (embed in resources) — all UI text
- Sizes: 10px labels, 12px values, 14px panel headers, 18px page headers

Controls: all custom-painted in JUCE
- Knob: circular arc indicator, value ring, center dot, glow effect on hover
- Slider: horizontal or vertical — filled track + thumb + value tooltip
- Button: pill-shaped, filled when active, subtle glow when active
- Toggle: LED-style indicator (LED on = colored glow, off = dim)
- Tab: underline indicator, not full background change

Animations:
- Page transitions: 150ms fade
- Hover: 80ms fill/glow transition
- Active state: immediate color change (no delay on press)

---

UI STRUCTURE & PAGES:

TOP BAR (persists across all pages):
```
[Wolf's Den logo] [BROWSE] [SYNTH] [THEORY] [PERFORM] [FX] [MOD]   [Preset Name ▼] [< >]  [Settings] [CPU%]
```
- Left: plugin logo + page tab bar
- Center: current preset name + prev/next arrows
- Right: settings button, CPU meter

BOTTOM BAR (persists):
- MIDI activity indicator (flashes on MIDI in)
- Current detected chord display (Theory Engine output)
- Current scale display
- Polyphony counter (X/16 voices active)
- Plugin output level meter (L/R)
- Live Sync status indicator

---

PAGE: BROWSE
Layout:
- Left 30%: Filter sidebar
  - Search field (text input)
  - Genre filter pills (Trap, Lo-Fi, R&B, Jazz, EDM, Cinematic, Classical, World, etc.)
  - Mood filter pills (Dark, Bright, Tense, Dreamy, Aggressive, Calm, etc.)
  - Energy: Low | Medium | High toggle
  - Scale selector filter
- Center 70%: Scrollable preset card grid
  - Card: name, genre tag, mood badge, star favourite, play preview button
  - Color accent based on mood (Dark=blue, Bright=gold, Tense=red, etc.)
- Bottom: selected preset info panel (name, author, tags, description)

---

PAGE: SYNTH (per-Layer view)
Layout:
- Layer selector tabs: [A] [B] [C] [D] — each has indicator dot (colored = active)
- For active layer:
  Left column: OSCILLATOR
  - Type selector: [Wavetable] [Analogue] [FM] [Granular] [Sample]
  - Type-specific controls (e.g., Granular: Position, Size, Density, Scatter sliders)
  - Waveform display (animates in real-time for Granular — particle dots)
  - Octave, Semitone, Fine tune knobs

  Center column: FILTER
  - Filter 1: Type dropdown (34 types), Cutoff, Resonance, Drive
  - Filter 2: same controls
  - Routing: [Series] [Parallel] toggle
  - Filter envelope: ADSR mini sliders with shape display

  Right column: AMPLIFIER
  - Amp Envelope: full ADSR graphic + sliders
  - Volume fader
  - Pan knob
  - Unison: count (1-8), detune, spread

  Bottom: LFO 1 + LFO 2 strips
  - Rate, Depth, Shape selector, Delay, Retrigger

---

PAGE: THEORY (Chord Intelligence)
Layout:
- Top bar: Current Key + Scale display | Detect MIDI button | Detect Audio button
Sub-tabs: [BROWSE CHORDS] [CIRCLE OF FIFTHS] [EXPLORE] [COLORS]

BROWSE CHORDS sub-tab:
- Section A: 8 chord pads (the current chord set)
- Section B: Diatonic chords of active scale (all 7 chords shown as pads)
- Section C: Main track — drag-arrange chord pads into progression
- Modifier sidebar: Inversion, Density, Octave, Voice Grouping

CIRCLE OF FIFTHS sub-tab:
- Interactive wheel (SVG-style custom drawn)
- Click segment = select scale
- Adjacent = related, popup shows shared notes

EXPLORE sub-tab:
- Card grid — mood-tagged, not scale-locked
- Constellation layout option (radial from root)

COLORS sub-tab:
- Grid: rows = extension type (triad, 7th, 9th, 11th, sus, altered)
- Columns = each chord in current set
- Each cell = a specific voicing — click to audition, drag to Section C
- Color-coded: blue=diatonic, gold=modal, gray=outside scale

---

PAGE: PERFORM (MIDI Pipeline)
Layout:
- Left section: KEYS LOCK
  - Mode selector: [OFF] [SCALE MAP] [SCALE ONLY] [CHORD NOTES] [CHORD SCALES]
  - Root note picker (circle of 12 notes)
  - Scale picker dropdown
  - Mini keyboard display (shows locked notes highlighted)

- Center section: CHORD MODE
  - On/Off toggle
  - Chord type dropdown (15+ types)
  - Inversion selector
  - Scale constraint toggle

- Right section: ARPEGGIATOR
  - On/Off toggle
  - Rate (knob + tempo-sync label)
  - Play Mode (Up/Down/Up-Down/Order/Random/Chord)
  - Octaves (1-4)
  - Swing knob
  - Latch + Restart toggles
  - 32-step grid (mini step display — clickable velocity bars)
  - MIDI Capture button (record → export)

---

PAGE: FX
Layout:
- Layer selector: [A] [B] [C] [D] [MASTER]
- FX chain: 4 horizontal slots
  - Each slot: FX name, On/Off LED, wet/dry knob, expand button (→ shows full params)
  - Drag to reorder slots
  - Click to open full FX parameter panel (expands below)
- Add FX button → popup browser (categories → effect types)

---

PAGE: MOD (Modulation Matrix + XY Pad)
Layout:
- Left 40%: MOD MATRIX table
  - 32 rows (slots)
  - Columns: Source | → | Target | Amount | Inv | Smooth | Mute
  - Each cell: dropdown selector for source/target
  - Amount: small horizontal slider in-row
  - Color: row highlight based on source type (LFO=purple, Envelope=teal, MIDI=gold)
  - Red dot indicator on target column when out-of-range
  - Add/Remove slot buttons

- Right 60%: XY PAD
  - 400×400px interactive square
  - X and Y axis labels (showing mapped targets from Mod Matrix)
  - Physics mode selector: [DIRECT] [INERTIA] [CHAOS]
  - Depth control: overall intensity slider
  - Dice button: randomize assignments
  - Record button: capture movement as DAW automation

---

UI COMPONENT HIERARCHY (JUCE):
```
AudioProcessorEditor
└── MainContentComponent (scales to window size)
    ├── TopBar
    ├── PageContainer (swaps component by tab)
    │   ├── BrowsePage
    │   ├── SynthPage
    │   │   └── LayerView (A/B/C/D — swapped by layer tab)
    │   ├── TheoryPage
    │   │   └── SubTabView (BrowseChords/CoF/Explore/Colors)
    │   ├── PerformPage
    │   ├── FxPage
    │   └── ModPage
    └── BottomBar
```

RESIZING:
- setResizable(true, false) — corner resizable by user
- setResizeLimits(800, 550, 2560, 1600)
- All layout via proportional geometry (getLocalBounds().proportionOfWidth() etc.)
- UI elements scale via AffineTransform if window too small for reflow

CONSTRAINTS:
- 60fps target: no heavy computation in paint() — all data pre-computed, paint only reads
- No OS-native widgets — all JUCE custom-painted
- All knobs + sliders: attached to AudioProcessorValueTreeState via attachments
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
