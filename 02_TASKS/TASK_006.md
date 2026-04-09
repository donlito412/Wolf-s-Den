TASK ID: 006

STATUS: DONE

GOAL:
Build the MIDI Transformation Pipeline — the Performance Engine layer that sits between MIDI input and the Synthesis Engine. This includes Keys Lock (scale remapping), Chord Mode (interval-based chord generation), and the Arpeggiator (step-sequenced note playback). All three systems must be independently toggle-able and chain correctly in sequence.

ASSIGNED TO:
Cursor

INPUTS:
/03_OUTPUTS/001_architecture_design_document.md
/03_OUTPUTS/003_core_processor_report.md
/03_OUTPUTS/005_theory_engine_report.md
/01_BRIEF/vst_research.md

OUTPUT:
/03_OUTPUTS/006_midi_pipeline_report.md
(Code in /05_ASSETS/apex_project/Source/engine/MidiPipeline.h/.cpp)

DELIVERABLES INSIDE OUTPUT:
- Keys Lock: all 4 modes tested and confirmed working
- Chord Mode: all 15+ chord types tested — correct intervals generated
- Arpeggiator: Up/Down/Up-Down/Random/Order modes tested at multiple rates
- Per-step controls: velocity and duration per step confirmed working
- Full chain test: Keys Lock → Chord Mode → Arp → Synth engine producing audio

MIDI PIPELINE — SIGNAL CHAIN TO BUILD:
```
Incoming MIDI (processBlock MidiBuffer)
→ [1. KEYS LOCK ENGINE]
→ [2. CHORD MODE ENGINE]
→ [3. ARPEGGIATOR ENGINE]
→ Output MIDI → SynthEngine
```

Each component: can be enabled/disabled independently

---

1. KEYS LOCK ENGINE:

Data structure: uint8_t lookupTable[128] (precomputed, 128 MIDI notes)
Modes:
- OFF: pass all MIDI through unchanged
- SCALE MAPPED: remap off-scale notes to nearest in-scale note (look up table)
- SCALE ONLY: gate — discard note if not in scale
- CHORD NOTES: remap all keys to only chord tones (spread across 128 notes)
- CHORD SCALES (dynamic): on each chord change, recalculate scale for that chord, rebuild lookup table

Build table: given root (0-11) + scale intervals[]
  for note in 0..127:
    pitchClass = note % 12
    if pitchClass in scaleSet: lookupTable[note] = note
    else: find nearest pitch in scaleSet (above or below), map to it (same octave)

On chord change: recalculate in <1ms (single array traversal)

---

2. CHORD MODE ENGINE:

Input: single MIDI note (post Keys Lock)
Output: multiple MIDI Note On messages (root + chord tones)

Chord Type interval arrays (store in TheoryEngine DB):
- Major: [0, 4, 7]
- Minor: [0, 3, 7]
- Dominant 7: [0, 4, 7, 10]
- Major 7: [0, 4, 7, 11]
- Minor 7: [0, 3, 7, 10]
- Minor Maj7: [0, 3, 7, 11]
- Diminished: [0, 3, 6]
- Diminished 7: [0, 3, 6, 9]
- Half-Diminished: [0, 3, 6, 10]
- Augmented: [0, 4, 8]
- Sus2: [0, 2, 7]
- Sus4: [0, 5, 7]
- Add9: [0, 4, 7, 14]
- Major 9: [0, 4, 7, 11, 14]
- Minor 9: [0, 3, 7, 10, 14]

Inversion: shift intervals (1st inv: remove lowest, add at top)
Scale constraint: each generated chord note also passed through Keys Lock if active

---

3. ARPEGGIATOR ENGINE:

Note buffer: holds currently pressed notes (max 16)
Clock: subscribes to host BPM (processBlock provides HostInfo)
Rate: host_bpm / (subdivisions per beat) → events per sample calculated

Play Modes:
- UP: sort buffer ascending, cycle upward
- DOWN: sort buffer descending, cycle downward
- UP_DOWN: go up then reverse, no repeated endpoints
- ORDER: cycle in order keys were pressed (FIFO tracking)
- CHORD: all notes played simultaneously (chord strum, not arp)
- RANDOM: random pick from buffer each step (with no-repeat option)

Per-step controls (32-step grid):
- Enabled/Disabled: bool stepEnabled[32]
- Velocity: uint8_t stepVelocity[32] (0-127)
- Duration: float stepDuration[32] (0.1 - 2.0 × step length)
- Modifier: enum {NONE, TRANSPOSE, SLIDE, RATCHET, CHORD} stepModifier[32]
- Transpose: int8_t stepTranspose[32] (±24 semitones)
- Ratchet: uint8_t stepRatchet[32] (1-4 triggers per step)

Additional:
- Latch: notes held in buffer after key release
- Restart: pattern resets to step 1 on new key press
- Swing: float swingAmount (0.0-0.5) — shifts even steps later
- Octave range: 1-4 octaves (pattern cycles through octave range)
- MIDI Capture: record generated MIDI → output as draggable MIDI clip

CONSTRAINTS:
- All MIDI processing runs in processBlock() — no allocation
- Clock must be sample-accurate (use AudioPlayHead to get exact position)
- Note Off messages generated correctly (no hanging notes on Arp mode changes)
- Latch: held notes released when Latch toggled off
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
