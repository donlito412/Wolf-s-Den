TASK ID: 022

PHASE: 2

STATUS: DONE

GOAL:
Fix the brief static/noise burst that occurs when the arpeggiator is enabled while chord mode
is active and notes are held. After this fix, toggling the arp on/off must be completely clean
with no audio artifact.

ASSIGNED TO:
Cursor

BUG REFERENCE:
Follow-up to TASK_019. Arp octave cycling is now correct, but a static burst occurs on toggle-on.

INPUTS:
/05_ASSETS/apex_project/Source/engine/MidiPipeline.cpp

OUTPUT:
/03_OUTPUTS/022_arp_static_fix_report.md

---

ROOT CAUSE — CONFIRMED BY SOURCE CODE INSPECTION

File: MidiPipeline.cpp, the `arpWasOff && arpOn` transition block (~line 816) and
`allNotesOffOutput()` (line 524).

The static happens because:

1. When the arp is OFF and chord mode is ON, pressing a key sends noteOn events DIRECTLY to the
   synth output (line 932–933). These notes are tracked in `chordMaps[]` (`used=true`) but NOT
   in `arpNoteActive` or `arpPolyHeld`.

2. When the arp is OFF and chord mode is OFF, pressing a key sends a single noteOn DIRECTLY to
   the synth (line 942). This note is NOT tracked by any arp state variable — only by
   `physicalHeld[raw]` (the raw MIDI note, which may differ from the locked/output note).

3. When the user enables the arp (arpWasOff → arpOn, line 816), `allNotesOffOutput()` is called.
   That function (line 524) only sends noteOff for `arpNoteActive` and `arpPolyHeld` — NEITHER
   of the cases above.

4. Result: the pre-existing chord tones or single notes stay sounding in the synth. The arp then
   immediately fires new notes on top. The voice overlap causes a brief clip/saturation — the
   "static" the user hears — which clears once the lingering voices reach the end of their
   release tails.

The same issue occurs in reverse: when the arp is disabled (lastProcessArpOn → !arpOn, line 828),
`allNotesOffOutput()` kills arp-tracked notes but NOT notes that were playing before the arp was
first enabled (if any survived).

---

THE FIX — ONE BLOCK

File: MidiPipeline.cpp

Find the arp toggle-ON transition block:

CURRENT (approximately line 816–826):
```cpp
const bool arpWasOff = !lastProcessArpOn;
if (arpOn && arpWasOff)
{
    allNotesOffOutput(out, 0);
    arpTimeInStep = 0;
    arpNoteActive = false;
    arpGateSamplesLeft = 0.0;
    arpPoolCount = 0;
    pressCount = 0;
    arpPatternIndex = 0;
    arpNoteWalk = 0;
}
```

NEW:
```cpp
const bool arpWasOff = !lastProcessArpOn;
if (arpOn && arpWasOff)
{
    // Kill chord-mode notes that were sent directly (not via arp) before arp was enabled.
    // These are tracked in chordMaps[] but NOT in arpNoteActive/arpPolyHeld, so
    // allNotesOffOutput() misses them. Without these explicit note-offs the synth
    // keeps playing them, causing a static burst when the arp fires its first note on top.
    for (int ci = 0; ci < kMaxMap; ++ci)
    {
        if (chordMaps[(size_t)ci].used)
        {
            for (uint8_t k = 0; k < chordMaps[(size_t)ci].nOut; ++k)
                out.addEvent(juce::MidiMessage::noteOff(1, (int)chordMaps[(size_t)ci].outs[(size_t)k]), 0);
            chordMaps[(size_t)ci] = {};   // clear the slot
        }
    }
    // Kill any single (non-chord) direct noteOns: send noteOff for all held physical notes.
    // physicalHeld tracks the raw MIDI note number, which is close enough for cleanup.
    for (int n = 0; n < 128; ++n)
    {
        if (physicalHeld[(size_t)n])
            out.addEvent(juce::MidiMessage::noteOff(1, n), 0);
    }
    allNotesOffOutput(out, 0);    // handles arpNoteActive + arpPolyHeld (arp-tracked notes)
    arpTimeInStep = 0;
    arpNoteActive = false;
    arpGateSamplesLeft = 0.0;
    arpPoolCount = 0;
    pressCount = 0;
    arpPatternIndex = 0;
    arpNoteWalk = 0;
}
```

Also apply the same chord-map cleanup to the toggle-OFF transition (arpOn=false, lastProcessArpOn=true),
found at approximately line 828–833:

CURRENT:
```cpp
if (!arpOn && lastProcessArpOn)
{
    allNotesOffOutput(out, 0);
    arpNoteActive = false;
    arpGateSamplesLeft = 0.0;
}
```

NEW:
```cpp
if (!arpOn && lastProcessArpOn)
{
    // Same cleanup as toggle-on: kill any chord-map notes that may have outlasted the arp.
    for (int ci = 0; ci < kMaxMap; ++ci)
    {
        if (chordMaps[(size_t)ci].used)
        {
            for (uint8_t k = 0; k < chordMaps[(size_t)ci].nOut; ++k)
                out.addEvent(juce::MidiMessage::noteOff(1, (int)chordMaps[(size_t)ci].outs[(size_t)k]), 0);
            chordMaps[(size_t)ci] = {};
        }
    }
    allNotesOffOutput(out, 0);
    arpNoteActive = false;
    arpGateSamplesLeft = 0.0;
}
```

---

IMPORTANT: `kMaxMap` is likely defined in MidiPipeline.h or at the top of MidiPipeline.cpp. Use
whatever constant is already used in the existing `for (int i = 0; i < kMaxMap; ++i)` loops in
the file — do not introduce a new magic number.

---

VERIFY

After the fix, test:
1. Chord mode ON + hold a chord + toggle arp ON → no static, arp plays cleanly from first note
2. Chord mode ON + arp playing + toggle arp OFF → clean cutoff, no burst
3. Chord mode OFF + hold a note + toggle arp ON → clean transition
4. Chord mode OFF + arp playing + toggle arp OFF → clean cutoff
5. Arp toggle with no notes held → no change in behavior (should be same as before)
6. Confirm octave cycling fix from TASK_019 is still intact (patterns cycle correctly)

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Exact diff (old → new) for both transition blocks
- Confirmation of test cases 1–6 above
- Build: cmake --build build --config Release — zero errors, zero new warnings

CONSTRAINTS:
- Only modify the two arp toggle transition blocks (arpWasOff && arpOn, and !arpOn && lastProcessArpOn)
- Do not change any arp pool logic, fireArpStep(), or pattern sequencing
- Do not change any APVTS parameter IDs
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE