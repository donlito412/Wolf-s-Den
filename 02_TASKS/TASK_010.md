TASK ID: 010

STATUS: DONE

GOAL:
Full system integration and testing. Connect all engines (Synthesis, Theory, MIDI Pipeline, FX, Modulation) into a unified, functioning plugin. Validate every system works together end-to-end. Fix all integration bugs. Deliver a stable, testable build.

ASSIGNED TO:
Cursor + Claude

INPUTS:
/03_OUTPUTS/004_synthesis_engine_report.md
/03_OUTPUTS/005_theory_engine_report.md
/03_OUTPUTS/006_midi_pipeline_report.md
/03_OUTPUTS/007_modulation_system_report.md
/03_OUTPUTS/008_fx_engine_report.md
/03_OUTPUTS/009_ui_report.md
/04_LOGS/project_log.md (read the 2026-04-10 preset system entry for full context on recent changes)

OUTPUT:
/03_OUTPUTS/010_integration_test_report.md

DELIVERABLES INSIDE OUTPUT:
- Integration test checklist (all items must be PASS before task marks DONE)
- Bug log: all issues found, status (fixed/deferred)
- CPU benchmarks: full polyphony + all FX + Mod Matrix active — must stay under 20% on Apple M1
- Crash log: zero crashes during 1-hour playback test
- DAW compatibility: tested in Ableton Live, Logic Pro, FL Studio

INTEGRATION TEST CHECKLIST:

ENGINE INTEGRATION:
- [ ] MIDI input → Keys Lock → Chord Mode → Arp → SynthEngine → Audio output (full chain)
- [ ] Theory Engine chord detection updates bottom bar display in real-time
- [ ] Voice Leading fires correctly on chord change in Section C
- [ ] Mod Matrix LFO → Filter Cutoff produces correct audible modulation
- [ ] XY Pad position changes affect mapped parameters in real-time
- [ ] FX chain processes audio post-synthesis (reverb tail heard after note release)
- [ ] All Layer FX + Master FX route correctly (no bypass leaking)

BUILD — DO THIS FIRST (three bug fixes were applied, plugin must be rebuilt):
Files changed since last build:
  Source/engine/SynthEngine.h          — added public allNotesOff() declaration
  Source/engine/SynthEngine.cpp        — implemented allNotesOff() method
  Source/PluginProcessor.h             — added std::atomic<bool> pendingAllNotesOff
  Source/PluginProcessor.cpp           — set pendingAllNotesOff in loadPresetById(); consume it in processBlock()
  Source/parameters/WolfsDenParameterLayout.cpp — midi_arp_sync_ppq default changed true → false

Build steps:
- [ ] cd into the apex_project directory
- [ ] cmake --build build --config Release
- [ ] Confirm zero compiler errors and zero linker errors
- [ ] Fix any errors introduced by preset system or bug-fix changes before proceeding to tests

STATE & PRESET:
- [ ] Save project in Ableton → close and reopen → all parameters restored exactly
- [ ] Preset save to DB → load from browse page → full state restored
- [ ] Undo/Redo works for parameter changes (JUCE UndoManager integration)
- [ ] Window size saved and restored

PRESET SYSTEM (new — verify the 2026-04-10 implementation):
- [ ] Browse page opens in PATCHES mode by default — 41 factory instrument cards visible
- [ ] Selecting "Rhodes" card loads the Rhodes WAV into layer 0 and the sound changes audibly
- [ ] Each category (Bass, Keys, Leads, Pads, Plucks, Strings, Horns, Woodwinds) loads a distinctly different timbre
- [ ] < > arrows in top bar step through all 41 factory instruments and the preset label updates
- [ ] Save Preset button → name dialog → saved preset appears as a new card in PATCHES mode
- [ ] User preset save → DAW project close/reopen → user preset still loads correctly from DB
- [ ] CHORD SETS tab still works exactly as before (existing chord-set cards, no regression)
- [ ] Factory preset cards cannot be deleted (Delete button absent on factory cards)
- [ ] User preset cards show a Delete button that removes the preset from the list

BUG FIXES (applied 2026-04-10 — verify these are resolved):
- [ ] Preset stuck on one sound: loading a factory preset now calls allNotesOff() (via atomic pendingAllNotesOff flag consumed in processBlock) so the new sample is clean on next key press — confirm sound changes immediately after selecting a new preset card
- [ ] Arp static / erratic: midi_arp_sync_ppq default changed from true → false so arp runs free-tempo by default; PPQ sync can still be enabled per-session — confirm arp plays at correct tempo without transport running
- [ ] Voice accumulation static: the allNotesOff() fix also clears hung voices from the previous preset, eliminating the clipping noise — confirm no static when switching presets rapidly

MIDI PIPELINE:
- [ ] Keys Lock: play chromatic scale → only in-key notes heard
- [ ] Chord Mode: single note press → full chord generated
- [ ] Arp: hold chord → arpeggio starts; release → stops (no hanging notes)
- [ ] Latch: hold notes → toggle latch → release → arp continues
- [ ] MIDI Capture: record arp → drag into DAW → MIDI matches what was heard

AUDIO QUALITY:
- [ ] No aliasing on all analogue oscillator types at all pitches (A0 to C8)
- [ ] No clicks at voice steal boundary
- [ ] Reverb tail plays out completely after note release (not cut off)
- [ ] No DC offset on output (DC blocker confirmation)
- [ ] Gain staging: no internal clipping at unity gain settings

PERFORMANCE:
- [ ] 16-voice polyphony + all 4 layers + full FX: <20% CPU (Apple M1)
- [ ] Plugin opens in under 1 second
- [ ] Preset load time: <200ms
- [ ] No memory leaks after 1 hour operation (Instruments: monitor in Xcode)

CONSTRAINTS:
- All PASS items must be green before marking DONE
- Deferred bugs must be logged with priority (High/Medium/Low)
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
