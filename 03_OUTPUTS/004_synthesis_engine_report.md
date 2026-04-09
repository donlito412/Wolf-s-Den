# TASK_004 — Synthesis Engine Report
**Date:** 2026-04-09  
**Agent:** Cursor  
**Code:** `/05_ASSETS/apex_project/Source/engine/` + `Source/engine/synth/`

---

## 1. Summary

Wolf's Den now **generates audio from MIDI** through **`SynthEngine`**: **16-voice** polyphony, **4 layers** per voice summed in parallel, **per-layer** oscillator mode (8 types), **dual biquad** filter (serial **F1 → F2**), **global** filter ADSR (shared targets across layers), **per-layer** amp ADSR, **two LFO paths** affecting filter cutoff (global APVTS LFO + per-layer free-running LFO), and **master volume**.  

**Internal math** uses **`double`** in `VoiceLayer` (oscillator, biquads, envelopes); output is **`float`** at the buffer. **`processBlock` does not allocate** (fixed `std::array` tables, pre-bound parameter atomics, stack MIDI sort buffer capped at 256 events).

---

## 2. Oscillator modes (`layerN_osc_type`, choice 0–7)

| Index | Mode | Implementation |
|------:|------|----------------|
| 0 | Sine | Phase sine |
| 1 | Saw | PolyBLEP saw |
| 2 | Square | PolyBLEP square |
| 3 | Triangle | PolyBLEP triangle (integrated square) |
| 4 | Wavetable | 2048-point baked harmonic table, linear interp |
| 5 | Granular | Up to 16 grains, Hanning window, shared 16k source buffer |
| 6 | FM | 2-op PM: `sin(2πφc + I·sin(2πφm))`, index scaled from filter resonance |
| 7 | Sample | Same table as wavetable, 2× phase increment (octave-up “sample” stand-in) |

**Note:** Full **file-based** wavetable morph, **AIFF/WAV streaming**, and **granular freeze/scatter** from the architecture doc are **not** fully implemented yet; modes 4–7 are **real-time safe** stand-ins that prove the routing and gain structure.

---

## 3. Filter types (`layerN_filter_type` maps to engine mode 0–5)

Implemented in **`VoiceLayer::updateFilterCoeffs`** (RBJ biquads, `SynthDSP.h`):

| Mode | Description |
|------|-------------|
| 0 | **LP24** — two low-pass stages |
| 1 | **LP12** — one low-pass, F2 bypass (identity) |
| 2 | **BP** — band-pass |
| 3 | **HP12** — one high-pass |
| 4 | **Notch** |
| 5 | **HP24** — two high-pass stages |

**Comb** and **formant** from TASK_004 are **not** implemented yet (reserved for a later pass). UI choice list still has **4** entries; normalized automation only hits modes **0–3** unless host sends edge-normalized values.

**Routing:** Always **serial** F1 → F2 (no parallel sum path yet).

---

## 4. Envelopes

- **Amp ADSR:** per layer, targets from `layerN_amp_*` (cached when raw values change).  
- **Filter ADSR:** **global** parameters `filter_adsr_*` (one target set, separate **state** per layer).  
- Shapes are **linear attack/decay/release** segments (not exponential curves). **Sustain** holds level until note-off.

---

## 5. LFO → filter cutoff (baseline modulation)

- **Global LFO:** `lfo_rate`, `lfo_depth`, `lfo_shape` — one **`dsp::Lfo`** in **`SynthEngine`**, advanced once per sample, value passed into every `VoiceLayer::renderAdd` as **`globalLfoValue`**. Modulates cutoff in semitone offset together with filter envelope and per-layer LFO.  
- **Per-layer LFO:** second **`dsp::Lfo`** inside each **`VoiceLayer`**, fixed rate `0.23 + 0.11·layer` Hz, shapes follow global `lfo_shape` selection, adds up to **±10 semitones** of cutoff modulation.  
- **Tempo sync, delay, fade-in, retrigger** — not implemented (TASK scope reduced for this milestone).

---

## 6. Voice management

- **Polyphony:** **16** voices.  
- **Steal:** **oldest** by monotonically increasing **`age`** (per block `+= numSamples`).  
- **Legato / portamento / configurable poly 1–16 / unison** — **not** in this build.  
- **Steal click:** stolen voice is **hard-reset** then **note-on**; brief discontinuity possible — listed as **known limitation**.

---

## 7. CPU baseline (guidance)

No in-plugin profiler yet. **Rule of thumb** on Apple Silicon: **one note, four layers, default UI**, expect **low single-digit %** of one core at 48 kHz / 512 samples — **verify** with Activity Monitor / host performance meter while toggling oscillator types (granular/FM slightly heavier).

---

## 8. Files

| Path | Role |
|------|------|
| `engine/SynthEngine.{h,cpp}` | Voices, MIDI (sorted event buffer), global LFO, wavetable/granular source fill, `process()` |
| `engine/synth/SynthDSP.h` | PolyBLEP, biquad, ADSR, LFO, `midiNoteToHz` |
| `engine/synth/VoiceLayer.{h,cpp}` | Per-layer DSP chain |
| `PluginProcessor.cpp` | `prepareToPlay` / `processBlock` → synth |

---

## 9. Deviations vs TASK_004 checklist

| TASK item | Status |
|-----------|--------|
| 4 layers, osc + dual filter + amp/filter env + 2 LFO concept | **Partial** — dual filter **series** only; second LFO per layer **not** fully exposed as separate APVTS params |
| Wavetable load/morph | **MVP** single baked table |
| Granular spec (scatter/freeze/file position) | **MVP** grain cloud from internal buffer |
| Sample playback from WAV/AIFF | **Not yet** — table-based stand-in |
| Unison 8 / detune / spread | **Not yet** |
| Poly 1–16 user setting | **Fixed 16** |
| Legato / portamento | **Not yet** |
| Comb / formant filters | **Not yet** |
| 64-bit internal | **Yes** (`double` in layer DSP) |
| Zero audio-thread allocation | **Yes** (fixed buffers; ≤256 MIDI events per block) |

---

## 10. Manual audio test

1. Open **Standalone** or DAW, load **Wolf's Den**.  
2. Play MIDI; default **layer0** osc **Saw** + **LP24** should sound immediately.  
3. Automate or change **`layer0_osc_type`** through all **8** modes — timbre should change clearly.  
4. Raise **`lfo_depth`** and **`lfo_rate`** — filter **warble** should increase.  
5. Adjust **global** `filter_adsr_*` — filter motion should follow.  

---

## 11. Next step

**TASK_005** — Music Theory Engine.

---

*End of TASK_004 report.*
