# TASK 006 — MIDI Transformation Pipeline

**Output:** `05_ASSETS/apex_project/Source/engine/MidiPipeline.h`, `MidiPipeline.cpp`  
**Integration:** `PluginProcessor.cpp` (prepare / reset / `processBlock` order)

## Signal chain

Host `MidiBuffer` → `TheoryEngine::processMidi` (detection state) → `MidiPipeline::process` → `SynthEngine::process`.

Pipeline order inside `MidiPipeline`: **Keys Lock** → **Chord Mode** → **Arpeggiator** (arp consumes pooled notes; when arp is off, chord/note events pass straight through).

## Keys Lock (`midi_keys_lock_mode`)

| Mode | Behaviour |
|------|-----------|
| Off | No change |
| Remap | Uses `TheoryEngine::getScaleLookupTable()` (nearest in-scale note upward, same as theory engine) |
| Mute | Note on/off only if pitch class is in the active scale from the theory DB |
| Chord Tones | Maps note to nearest pitch class of the **detected** chord (`getBestMatch` + chord definition intervals); weak/no match passes note through |
| Chord Scales | Rebuilds a local 128-entry remap when the detected chord (id + root) changes: **major scale** on the chord root; falls back to global scale table when no confident chord |

## Chord Mode (`midi_chord_mode` + `theory_chord_type` + `midi_chord_inversion`)

- Interval sets for the 11 APVTS chord types (Major, Minor, Dim, Aug, Sus2, Sus4, Maj7, Min7, Dom7, Min7b5, Dim7) match the TASK_006 list (Add9 / Maj9 / Min9 reserved for future UI indices).
- Inversions rotate the sorted chord stack upward in octave.
- Each generated chord tone is run through **Keys Lock** again when that stage is active.
- Note-off uses per-source **ChordMapSlot** so releasing the triggering key releases all emitted notes (or removes them from the arp pool).

## Arpeggiator (`midi_arp_on`, `midi_arp_rate`, `midi_arp_pattern`, `midi_arp_latch`, `midi_arp_swing`, `midi_arp_octaves`)

- **Clock:** BPM from `AudioPlayHead::getPosition()->getBpm()` when present; else 120. Step length = `sampleRate / (bpm/60 * midi_arp_rate)` with `midi_arp_rate` = steps per beat (default 4).
- **Patterns:** Up, Down, Up-Down (ping-pong without duplicate endpoints), Order (FIFO press order), Chord (all pool notes per step), Random (xorshift pool, avoids immediate repeat when possible).
- **Swing:** Odd arp step indices lengthen the interval (even steps “pushed” later in time).
- **Latch:** Note-off does not remove notes from the arp pool; turning latch off drops non–physically-held notes from the pool.
- **Octaves:** Shifts output by 0‥(N−1) octaves using `(arpPatternIndex / 32) % N` (coarse cycling over the 32-step pattern index).
- **32-step grid:** `stepEnabled`, `stepVelocity`, `stepDuration`, `stepTranspose`, `stepRatchet` — defaults set in `prepare()` (all steps on, velocity 100, duration 1.0, transpose 0, ratchet 1). Per-step API for UI/automation can be added later without changing the DSP contract.

## Real-time constraints

- No heap allocation in `process()`; scale intervals use pointers into `TheoryEngine`’s loaded vectors (read-only after init).
- MIDI working set: 512 input events per block; 16-note arp pool; 16 chord-voice maps.

## Manual test checklist (DAW / Standalone)

1. **Keys Lock:** Off vs Remap vs Mute vs Chord Tones vs Chord Scales with scale + chord detection active.
2. **Chord Mode:** Single key → correct stack; inversion 0‥3; combined with Remap.
3. **Arp:** Each pattern at slow/fast `midi_arp_rate`; latch on/off; swing > 0; octaves 1‒4.
4. **Chain:** Remap + Chord + Arp → audible sequence at output.

## Follow-ups (not in this task)

- Expose 32-step grid and modifiers (SLIDE, MIDI capture) in APVTS/UI.
- Ratchet: interpret `stepRatchet` as multiple retriggers per step.
- `getPosition()` sample-accurate subdivision from `timeInSamples` / PPQ if host provides it.
