TASK ID: 019

PHASE: 2

STATUS: DONE

GOAL:
Fix the arpeggiator chord mode pattern bug. When chord mode and the arp are both active, the arp should cycle through the chord tones in musical order — C, E, G, Bb, C, E, G, Bb — before stepping up to the next octave (if multi-octave is selected). Instead, the current code interleaves the octave offset with the note walk index, producing extreme pitch jumps (C4→E5→G6→Bb7) that are unmusical and which the user hears as "plays a note four times then changes to the next note." Fix the octave cycling formula so the arp sounds like every other professional arpeggiator.

ASSIGNED TO:
Cursor

BUG REFERENCE:
BUG_P2_002 — Arp pattern broken with chord mode (follow-up to TASK_011).

INPUTS:
/05_ASSETS/apex_project/Source/engine/MidiPipeline.cpp

OUTPUT:
/03_OUTPUTS/019_arp_octave_fix_report.md

---

ROOT CAUSE — CONFIRMED BY SOURCE CODE INSPECTION

File: MidiPipeline.cpp, line 577 (inside fireArpStep)

CURRENT BROKEN CODE:
```cpp
const int nOct = juce::jmax(1, readArpOctaves());
const int octShift = (int)(((uint32_t)arpNoteWalk % (uint32_t)nOct) * 12u);
```

This formula cycles the OCTAVE on the SAME period as the note walk. With nNotes=4 chord tones (C, E, G, Bb) and nOct=4:
- Walk 0: noteIdx=0%4=0 → C,  octShift=(0%4)*12=0  → C4
- Walk 1: noteIdx=1%4=1 → E,  octShift=(1%4)*12=12 → E5
- Walk 2: noteIdx=2%4=2 → G,  octShift=(2%4)*12=24 → G6
- Walk 3: noteIdx=3%4=3 → Bb, octShift=(3%4)*12=36 → Bb7 (clamped to 127!)

The user hears: C4, E5, G6, Bb9 (clamped) — a different octave for every single note. With only 4 steps per cycle, it restarts immediately. This is what the user hears as "a note four times in a row then changes to the next note." The extreme pitch jumps make it sound like the same pitch class hammering at different heights, then restarting.

CORRECT STANDARD BEHAVIOR (every major DAW arp):
Complete ALL chord tones at the current octave, THEN advance to the next octave.

FIXED FORMULA:
```cpp
const int nOct = juce::jmax(1, readArpOctaves());
const int octShift = (int)(((uint32_t)arpNoteWalk / (uint32_t)juce::jmax(1, nNotes) % (uint32_t)nOct) * 12u);
```

With this fix and nNotes=4, nOct=2:
- Walk 0: octShift=(0/4)%2*12=0 → C4
- Walk 1: octShift=(1/4)%2*12=0 → E4
- Walk 2: octShift=(2/4)%2*12=0 → G4
- Walk 3: octShift=(3/4)%2*12=0 → Bb4
- Walk 4: octShift=(4/4)%2*12=12 → C5
- Walk 5: octShift=(5/4)%2*12=12 → E5
...
→ All 4 chord tones at octave 0, then all 4 at octave 1. Musical. ✓

With nOct=1 (default): octShift=(walk/nNotes)%1*12=0 always. No change from current behavior at default settings. ✓

---

THE FIX — ONE LINE

File: MidiPipeline.cpp, line 577

OLD:
```cpp
const int octShift = (int)(((uint32_t)arpNoteWalk % (uint32_t)nOct) * 12u);
```

NEW:
```cpp
const int octShift = (int)(((uint32_t)arpNoteWalk / (uint32_t)juce::jmax(1, nNotes) % (uint32_t)nOct) * 12u);
```

Note: `nNotes` is declared at line 570 (`const int nNotes = (int)arpSortedCount;`) — it is already in scope at line 577. No additional changes needed.

---

ALSO VERIFY — Chord pattern behavior

With ArpPattern::Chord (line 582–596), ALL notes in the pool play simultaneously every arp step. This is correct behavior (same as Ableton's "chord" arp mode). No change needed.

The user's main complaint is about the UP/DOWN/UPDOWN patterns with chord mode on. After this fix, those patterns should cycle musically through all chord tones before rising to the next octave.

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Old line 577 vs new line 577 (exact diff)
- Trace of expected output for Up pattern, nNotes=4 (Cm7 chord), nOct=1: C4→Eb4→G4→Bb4→C4→...
- Trace of expected output for Up pattern, nNotes=4 (Cm7 chord), nOct=2: C4→Eb4→G4→Bb4→C5→Eb5→G5→Bb5→C4→...
- Confirmation that with nOct=1 (default), behavior is identical to before
- Build: cmake --build build --config Release — zero errors, zero new warnings

CONSTRAINTS:
- Only change line 577
- Do not modify any other arp logic
- Do not change any APVTS parameter IDs
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
