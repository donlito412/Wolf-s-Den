# PROJECT NAME
Wolf's Den — All-in-One Music Production VST

# GOAL
Build a professional VST instrument combining the music theory intelligence of Scaler 3, the synthesis depth of Omnisphere, and the MIDI transformation pipeline of Komplete Kontrol — resulting in a single plugin that handles chord intelligence, deep synthesis, real audio output, and performance tools simultaneously.

# PRODUCT / SYSTEM DESCRIPTION
Wolf's Den is a multi-layer hybrid synthesizer with an embedded music theory engine. It generates real audio (not just MIDI) while understanding harmony. A user can detect chords from audio, build progressions, apply voice leading, trigger synthesized sounds in real-time, apply granular/FM/wavetable synthesis per layer, route through a full FX chain, and perform live using a MIDI transformation pipeline — all inside one plugin.

**Three Core Pillars:**
1. HARMONY ENGINE — Chord detection, scale awareness, voice leading, MIDI generation (Scaler 3-level)
2. SYNTHESIS ENGINE — Multi-layer oscillators (wavetable, granular, FM, sample), dual filters, multi-stage envelopes, LFO matrix (Omnisphere-level)
3. PERFORMANCE ENGINE — Real-time MIDI transformation: Scale Lock, Chord Mode, Arpeggiator, XY Mod Pad (Komplete Kontrol-level)

# TARGET USER
- Professional music producers
- Composers (film, TV, games)
- Live performers
- Beat makers needing both sound design + harmonic intelligence

# SUCCESS CRITERIA
- Plugin loads in all major DAWs: Ableton Live, Logic Pro, FL Studio, Pro Tools, Cubase
- VST3 + AU + AAX format support
- Synthesis engine produces professional-quality audio
- Chord detection works on both MIDI and audio input
- Keys Lock, Chord Mode, Arpeggiator all function correctly
- Full FX chain operational
- Preset browser functional with tag-based search
- Full custom UI rendered at 60fps
- Plugin size under 2GB (base install without sample libraries)
- Stable: zero crashes in 4-hour session tests

# PHASE 1 — COMPLETE [2026-04-18]
Tasks 001–010 executed. Full JUCE plugin built: synthesis engine, theory engine, MIDI pipeline, modulation system, FX rack, full custom UI, preset system (41 factory instruments), SQLite database. Plugin loads and produces audio in Logic Pro. Phase 1 is closed — no further work under Tasks 001–010.

# CURRENT STATUS — PHASE 2
Real-world DAW testing (Logic Pro) revealed three critical failures that block release. Phase 2 addresses these exclusively before any scope expansion.

**Open bugs from Logic Pro testing [2026-04-18]:**
- BUG_P2_001 (CRITICAL): Genre selection on Composition page does nothing — no progressions, no sound. No real genre→chord progression library exists. → TASK_012
- BUG_P2_002 (HIGH): Arpeggiator at fast BPM settings corrupts the preset sound — voice accumulation / note-off failure causes timbral drift. Slow arp works. → TASK_011
- BUG_P2_003 (HIGH): CPU 45–55% on Apple M1 under normal playing. Phase 1 target was <20%. Profiling and optimization required. → TASK_013

**Additional Phase 2 work:**
- Dead code and unused stubs removed from the codebase → TASK_014
- Wavetable oscillator: factory library (20 tables) + file I/O + morph UI → TASK_015
- Granular oscillator: pitch tracking fixed, scatter musically calibrated → TASK_016
- Sample playback: multi-sample keymaps, velocity layers, loop point editing → TASK_017
- Windows build: VST3 + Standalone verified clean on Windows 10/11 → TASK_018

# PHASE 2 TASK ORDER
Priority order (bugs block release, DSP depth and Windows are parallel):
1. TASK_011 — Arp fix (DONE)
2. TASK_013 — CPU optimization (blocks all real-world use)
3. TASK_012 — Genre progressions (core feature gap)
4. TASK_014 — Dead code removal (clean codebase before DSP work)
5. TASK_015 — Wavetable I/O
6. TASK_016 — Granular depth
7. TASK_017 — Sample playback depth
8. TASK_018 — Windows build (can run in parallel with 015–017)

# NEXT MILESTONE
Complete TASK_011–018 (Phase 2). Plugin is not release-ready until all three DAW-confirmed bugs are resolved (TASK_011 done, TASK_012 and TASK_013 remaining).

# QUALITY BAR (NON-NEGOTIABLE)
- If it’s in the build under a given name, it must **work properly** at a level fit for professional use.
- **Honest naming:** algorithm variants are labeled accurately (e.g. plate/spring-style vs physical modeling).
- **No silent shortcuts** for brand-facing releases: improve the DSP or narrow the claim — never the reverse.

# TECH STACK
- Framework: JUCE 7+ (C++)
- Build System: CMake
- Plugin Formats: VST3, AU, AAX (via JUCE)
- UI Rendering: JUCE software renderer + optional OpenGL for visualizers
- Database: SQLite3 (embedded chord/preset/tag database)
- Audio Engine: Custom C++ DSP (oscillators, filters, FX)
- Theory Engine: Custom C++ (chord matching, scale logic, voice leading)
- Platforms: macOS (Apple Silicon + Intel), Windows 10/11

# REFERENCE PLUGINS
- Scaler 3 (Plugin Boutique) — theory engine, UI workflow
- Omnisphere 2 (Spectrasonics) — synthesis architecture, modulation, FX
- Komplete Kontrol 3 (Native Instruments) — MIDI pipeline, hardware integration
