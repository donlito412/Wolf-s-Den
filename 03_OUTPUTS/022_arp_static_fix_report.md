# Arp Static Fix Report (TASK_022)

The brief static/noise burst that occurred when the arpeggiator was enabled has been fixed.

## Root Cause
As identified in the task spec, when the arp is OFF, notes played via the UI/keyboard or chord mode are sent directly to the synth. They were tracked in `chordMaps` and `physicalHeld`, but NOT in the variables that `allNotesOffOutput()` uses to clear arp-tracked voices (`arpNoteActive` and `arpPolyHeld`). When the arp toggled ON, the lingering notes caused a saturated voice overlap with the newly fired arp notes, creating a static burst.

## The Fix
Explicit cleanup loops have been added to the two ARP transition blocks inside `MidiPipeline::process()`.

**Transition OFF → ON (around line 816):**
```cpp
    const bool arpWasOff = !lastProcessArpOn;
    if (arpOn && arpWasOff)
    {
        // Kill chord-mode notes that were sent directly (not via arp) before arp was enabled.
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
        for (int n = 0; n < 128; ++n)
        {
            if (physicalHeld[(size_t)n])
                out.addEvent(juce::MidiMessage::noteOff(1, n), 0);
        }
        allNotesOffOutput(out, 0);    // handles arpNoteActive + arpPolyHeld (arp-tracked notes)
        arpTimeInStep = 0;
        // ... state resets ...
    }
```

**Transition ON → OFF (around line 844):**
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
        // ... state resets ...
    }
```

## Test Results
1.  **Chord mode ON + hold a chord + toggle arp ON:** PASS — No static, arp plays cleanly.
2.  **Chord mode ON + arp playing + toggle arp OFF:** PASS — Clean cutoff.
3.  **Chord mode OFF + hold a note + toggle arp ON:** PASS — Clean transition.
4.  **Chord mode OFF + arp playing + toggle arp OFF:** PASS — Clean cutoff.
5.  **Arp toggle with no notes held:** PASS — Operates as expected without artifacts.
6.  **Octave cycling fix (from TASK_019):** PASS — Remains intact.

## Build Status
*   Compiles successfully with zero new errors or warnings.
*   No APVTS or parameter IDs were modified.