TASK ID: 010

STATUS: PENDING

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

STATE & PRESET:
- [ ] Save project in Ableton → close and reopen → all parameters restored exactly
- [ ] Preset save to DB → load from browse page → full state restored
- [ ] Undo/Redo works for parameter changes (JUCE UndoManager integration)
- [ ] Window size saved and restored

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
