TASK ID: 011

PHASE: 2

STATUS: DONE

GOAL:
Fix the arpeggiator so it does not corrupt the loaded preset's sound and plays cleanly at all BPM settings. Testing in Logic Pro confirmed: slow arp rates work, fast arp rates corrupt the timbre. The root causes have been identified by reading the actual source code. Implement every fix listed below. No guessing required.

ASSIGNED TO:
Cursor

BUG REFERENCE:
BUG_P2_002 — Arpeggiator corrupts preset sound at faster BPM settings.

INPUTS:
/05_ASSETS/apex_project/Source/engine/MidiPipeline.h
/05_ASSETS/apex_project/Source/engine/MidiPipeline.cpp
/05_ASSETS/apex_project/Source/engine/SynthEngine.h
/05_ASSETS/apex_project/Source/engine/SynthEngine.cpp
/05_ASSETS/apex_project/Source/PluginProcessor.h
/05_ASSETS/apex_project/Source/PluginProcessor.cpp

OUTPUT:
/03_OUTPUTS/011_arp_fix_report.md
(Code changes in /05_ASSETS/apex_project/Source/)

---

ROOT CAUSES — CONFIRMED BY SOURCE CODE INSPECTION

ROOT CAUSE 1 — ARP TOGGLE: VOICES NOT CLEARED ON ENABLE
File: MidiPipeline.cpp, around line 1040
The flag `lastProcessArpOn` is updated at the end of process(). When the arp transitions from off→on, the code detects this (checked around line 682 area), but no `allNotesOffOutput()` is fired at the start of that block. Any voices that were playing from a manually held chord before the arp was enabled remain alive AND the arp fires new notes on top of them. This is the "completely changes the sound" effect — it is NOT a fast-rate issue, it is a toggle issue that happens at any rate. The pre-existing held chord voices layer with arp voices, producing a different timbre than the preset alone.

ROOT CAUSE 2 — GATE FLOOR LONGER THAN STEP AT FAST RATES
File: MidiPipeline.cpp, line 594
`arpGateSamplesLeft = juce::jmax(64.0, gateSamples(gate01));`
The gate has a hard floor of 64 samples. At fast rates, `samplesPerArpStep` can be shorter than 64 samples. When this happens, the gate duration is LONGER than the step interval, so the note-off from the gate mechanism (lines 972–976) fires AFTER the next note-on has already been sent by `fireArpStep`. Although `allNotesOffOutput` is called inside `fireArpStep` before the new note-on (line 561), the synth engine may still be processing the old voice. The result is both notes sustaining simultaneously or producing an audio artifact at the step boundary.

ROOT CAUSE 3 — SAMPLE RATE CHANGE OF samplesPerArpStep MID-BLOCK
File: MidiPipeline.cpp, lines 683–686
`samplesPerArpStep` is recalculated from `readArpRate()` every time `process()` runs. If arp rate is automated or changed via UI mid-block, `samplesPerArpStep` changes while `arpTimeInStep` is partway through the old step length. If the new `samplesPerArpStep` is shorter than `arpTimeInStep`, the step boundary condition `arpTimeInStep >= stepLen - 1e-9` (line 1007) fires immediately for every remaining sample in the block — potentially triggering dozens of rapid note-on/note-off pairs in a single block, producing noise or distortion.

ROOT CAUSE 4 — RATE PARAMETER READ INCONSISTENCY
File: MidiPipeline.cpp, lines 337–367 (readArpRate)
`readArpRate()` reads from `arpRateParam` (AudioParameterChoice*) but falls back to `ptrs.arpRate` (raw atomic). If only the atomic is bound (not the AudioParameterChoice*), the rate value goes through a normalized float → integer conversion that can be imprecise. Confirm which path is active and consolidate to one.

---

FIXES — IMPLEMENT ALL OF THESE

FIX 1 — CLEAR VOICES ON ARP TOGGLE
Location: MidiPipeline::process(), near the arp-on detection block (where `lastProcessArpOn` is read before update at line 1040)

At the very start of the per-sample loop (or before it), detect the off→on transition:
```cpp
const bool arpWasOff = !lastProcessArpOn;
if (arpOn && arpWasOff)
{
    // Arp just enabled. Kill all held voices before arp starts firing.
    allNotesOffOutput(out, 0);  // sample offset 0 = start of block
    arpTimeInStep = 0;
    arpNoteActive = false;
    arpGateSamplesLeft = 0.0;
    arpPoolCount = 0;   // clear the pool so arp waits for new MIDI input
    pressCount = 0;
    arpPatternIndex = 0;
    arpNoteWalk = 0;
}
```
Also detect on→off transition and clear:
```cpp
if (!arpOn && lastProcessArpOn)
{
    allNotesOffOutput(out, 0);
    arpNoteActive = false;
    arpGateSamplesLeft = 0.0;
}
```
`lastProcessArpOn` is still updated at the end of process() (line 1040) — do not move it.

FIX 2 — GATE FLOOR MUST NOT EXCEED STEP LENGTH
Location: MidiPipeline.cpp, lines 554 and 594 (inside fireArpStep)

Replace the hard floor of 64 samples with a floor that is capped at 90% of the current step length:
```cpp
// Old (line 594):
arpGateSamplesLeft = juce::jmax(64.0, gateSamples(gate01));

// New:
const double maxGate = juce::jmax(1.0, samplesPerArpStep * 0.9);
arpGateSamplesLeft = juce::jmin(maxGate, juce::jmax(1.0, gateSamples(gate01)));
```
Apply the same cap at line 554 where `want` is computed:
```cpp
const double want = (double)juce::jlimit(0.1f, 2.f, g)
                    * samplesPerArpStep
                    * juce::jmax(1.0e-6, gateScaleMul);
// Add: cap want at 90% of step
const double maxWant = samplesPerArpStep * 0.9;
// Use: juce::jmin(maxWant, want) in the assignment below
```
This guarantees the gate always expires before the next step, regardless of rate.

FIX 3 — SNAPSHOT samplesPerArpStep AT BLOCK START
Location: MidiPipeline::process(), after line 686

Add a local `const double blockStepLen = samplesPerArpStep;` immediately after the `samplesPerArpStep = ...` assignment. Use `blockStepLen` everywhere in the per-sample loop (lines 983, 994, 995, 1000, 1007, 1009) instead of `samplesPerArpStep` directly. This prevents a UI parameter change mid-block from corrupting the timing loop. `samplesPerArpStep` itself (the member) is still updated each block — only the loop uses the snapshot.

FIX 4 — CONSOLIDATE RATE PARAMETER READ
Location: MidiPipeline.cpp, lines 337–367 (readArpRate)

The function has two paths: AudioParameterChoice* and raw atomic fallback. In practice, if `arpRateParam` is non-null (bound in prepare()), use it exclusively via `arpRateParam->getIndex()` cast to float for `stepsPerBeat`. Remove the raw atomic fallback path. If `arpRateParam` is null (was not found in APVTS), return a safe default of 2.0f (eighth note). Add a DBG/jassert in prepare() to catch a null `arpRateParam` at startup.

---

TEST PROTOCOL — CONFIRM THESE SPECIFIC SCENARIOS PASS

All tests in Logic Pro with a single factory preset loaded:

TEST 1 — Toggle safety (ROOT CAUSE 1)
- Hold a chord manually (no arp)
- Enable arp
- RESULT: The previously held chord stops immediately, arp plays cleanly with the preset's original timbre
- FAIL condition: old voices layer with arp, timbre sounds different from preset played without arp

TEST 2 — Fast rate no corruption (ROOT CAUSE 2)
- Set arp rate to 1/32 or faster
- Set BPM to 160
- Hold a chord for 8 bars
- RESULT: Clean, repeating arp pattern, no drift in timbre, no accumulated distortion over time
- FAIL condition: sound thickens, distorts, or changes character after 2–4 bars

TEST 3 — Preset change with arp running
- Start arp
- Load a different factory preset while arp is running
- RESULT: New preset sound plays cleanly through the arp immediately
- FAIL condition: old sound bleeds through, or a burst of noise occurs at preset change

TEST 4 — Toggle at fast rate
- Set arp rate to 1/16 at 140 BPM
- Enable arp → play 4 bars → disable arp → silence immediately
- FAIL condition: notes hang after arp disabled

TEST 5 — Slow rate still works
- Set arp rate to 1/4 at 80 BPM
- RESULT: Clean, correct timing as before

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Confirmation of each root cause identified (yes/no it matched code inspection)
- List of every line changed with old code → new code
- All 5 test results (PASS/FAIL)
- Build: cmake --build build --config Release — zero errors

CONSTRAINTS:
- Zero allocations in processBlock()
- No mutex in the audio thread
- Do not change any APVTS parameter IDs
- Do not change the arp UI or any user-visible parameter ranges
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE