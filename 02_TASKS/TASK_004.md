TASK ID: 004

STATUS: PENDING

GOAL:
Build the Synthesis Engine — the audio-generating core of Wolf's Den. This includes 4 independent layers per patch, each with a full oscillator, dual filters, amp & filter envelopes, and 2 LFOs. A completed task means Wolf's Den produces real, audible audio from MIDI input across all oscillator types.

ASSIGNED TO:
Cursor

INPUTS:
/03_OUTPUTS/001_architecture_design_document.md
/03_OUTPUTS/003_core_processor_report.md

OUTPUT:
/03_OUTPUTS/004_synthesis_engine_report.md
(Code in /05_ASSETS/apex_project/Source/engine/SynthEngine.h/.cpp + subfiles)

DELIVERABLES INSIDE OUTPUT:
- All oscillator types confirmed working with audio output test
- Filter types implemented and listed
- Envelope shapes confirmed (ADSR correctly shapes amplitude)
- LFO targets working (LFO modulating filter cutoff as baseline test)
- CPU usage baseline: single layer, single note, all osc types documented

SYNTHESIS ENGINE ARCHITECTURE TO BUILD:

LAYER STRUCTURE (4 per patch):
```
Layer (A/B/C/D)
├── Oscillator
│   ├── Wavetable mode (load/morph between wavetables)
│   ├── Analogue mode (Sine, Saw, Square, Triangle, Noise — band-limited)
│   ├── FM mode (2-op FM: carrier + modulator ratio, FM amount)
│   ├── Granular mode (grain size, density, position, pitch scatter, pan scatter)
│   └── Sample mode (load .wav/.aiff — pitch-shifted playback)
├── Unison (up to 8 voices, detune, stereo spread)
├── Dual Filter (Filter 1 + Filter 2 — series or parallel)
│   ├── Types: LP 12dB, LP 24dB, HP 12dB, HP 24dB, BP, Notch, Comb, Formant (vowel)
│   ├── Cutoff, Resonance, Drive, Key Tracking, Env Depth, LFO Depth
│   └── Routing: Serial (F1→F2) or Parallel (F1+F2 summed)
├── Amp Envelope (ADSR — shapes volume)
├── Filter Envelope (ADSR — modulates filter cutoff)
├── LFO 1 + LFO 2
│   ├── Shapes: Sine, Triangle, Saw Up, Saw Down, Square, S&H, Random, Noise
│   ├── Rate: Hz free or tempo-synced (1/32 to 8 bars)
│   ├── Delay, Fade-in (slow ramp into effect)
│   └── Retrigger on new note vs. free-running
└── Level + Pan fader
```

VOICE MANAGEMENT:
- Polyphony: 1–16 voices configurable
- Voice stealing: steal oldest note when at polyphony limit
- Legato mode: single active voice, pitch slides between notes
- Portamento rate (glide speed)

GRANULAR ENGINE SPECIFICS:
- Grains: 1ms - 500ms
- Windowing: Hanning envelope per grain (fade in + fade out)
- Scatter: random pitch deviation ±cents, random pan ±degrees, random timing ±ms
- Position: playhead through source file
- Freeze button: locks position, creates infinite grain wash

CONSTRAINTS:
- Band-limited oscillators (no aliasing — use PolyBLEP or wavetable method)
- All audio processing: 64-bit double precision internally, 32-bit float at plugin I/O
- No audio artifacts at voice steal boundary
- Zero memory allocation inside audio callback
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
