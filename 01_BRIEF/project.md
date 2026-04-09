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

# CURRENT STATUS
Active development — core plugin building in JUCE (synth, MIDI pipeline, modulation, FX rack). All shipped features are held to **release quality**: no “placeholder” behavior marketed as finished product; names match what the DSP actually does.

# NEXT MILESTONE
Continue TASK_009+ UI and integration; parallel **quality hardening** of synthesis, FX, and modulation to reference-plugin depth (see engineering backlog / reports).

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
