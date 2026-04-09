# Wolf's Den — Gap Fix Report

**Author:** Claude (Architecture / Theory Lead)  
**Date:** 2026-04-09  
**Scope:** Post-audit gap remediation across Tasks 003–008

---

## 1. Audit Methodology

After the user flagged that Cursor produced a "light version", a complete source-file audit was conducted. Every task spec deliverable was compared against the actual compiled source. Files read:

- `VoiceLayer.h` / `.cpp`
- `MidiPipeline.h` / `.cpp`
- `ModMatrix.h` / `.cpp`
- `FxEngine.h` / `.cpp`
- `SynthEngine.h` / `.cpp`
- `WolfsDenParameterLayout.cpp`
- `WolfsDenParameterRegistry.h`

---

## 2. False-Positive Gaps (Were Correct All Along)

The initial Explore-agent audit over-reported missing features. Direct source-file reads confirmed the following items **ARE fully implemented**:

| Reported Gap | Task | Status | Evidence |
|---|---|---|---|
| Add9/Maj9/Min9 chord intervals | 006 | ✅ False positive | `kAdd9[]`, `kMaj9[]`, `kMin9[]` in MidiPipeline.cpp:117–119 |
| 32-step arp grid | 006 | ✅ False positive | `kArpSteps=32`, all 5 params per step bound; full `readStep*()` methods |
| Ratchet logic | 006 | ✅ False positive | `readStepRkt()` returns 1–4, used in `fireArpStep()` |
| Arp APVTS parameters (160 total) | 003/006 | ✅ False positive | `WolfsDenParameterLayout.cpp:274–292` registers all 32×5 params |
| Unison synthesis | 004 | ✅ False positive | `processOscillator()` per-voice loop confirmed in VoiceLayer.cpp |
| LFO2 rate binding | 004 | ✅ False positive | `layerLfo2Rate[layerIndex]` read directly from APVTS in VoiceLayer.cpp |
| Comb filter | 004 | ✅ False positive | `combBuf[2048]` + full delay-line code in `renderAdd()` |
| Formant filter | 004 | ✅ False positive | Dual-bandpass vowel morph case 7 in `updateFilterCoeffs()` |
| Parallel filter routing | 004 | ✅ False positive | `parallel = (denormChoice(...) == 1)` + `0.5*(f1+f2)` path confirmed |
| All 24 FX unit types | 008 | ✅ False positive | `FxUnitType` enum 0–23 in FxEngine.h; APVTS registers all type names |
| Mod matrix sources (10 types) | 007 | ✅ False positive | All 10 sources enumerated and evaluated in ModMatrix.cpp |
| Per-layer filter cutoff + res mod | 007 | ✅ False positive | Layer0–3_FilterCutoffSemi + FilterRes handled in evaluate() |

---

## 3. Real Gap Found and Fixed

### TASK_007 — ModMatrix Missing Expressiveness Targets

**Gap:** The ModMatrix `Target` enum and `evaluate()` function were missing per-layer **pitch**, **amplitude**, and **pan** modulation targets. This meant the mod matrix could not produce:
- **Vibrato** (LFO → per-layer pitch semitones)
- **Tremolo** (LFO → per-layer amplitude)
- **Auto-Pan** (LFO → per-layer stereo position)

The TASK_007 spec explicitly named `Osc1Pitch`, `AmpLevel`, and `Pan` as required mod targets. All three were absent.

**Fix applied across 5 files:**

#### `ModMatrix.h`
- Added 12 new `Target` enum values:
  - `Layer0_PitchSemi` … `Layer3_PitchSemi` — additive pitch mod in semitones (±48 range)
  - `Layer0_AmpMul` … `Layer3_AmpMul` — multiplicative amplitude mod (1.0 = unity)
  - `Layer0_PanAdd` … `Layer3_PanAdd` — additive bipolar pan offset (±1.0)
- Updated `evaluate()` signature with 3 new output arrays: `float layerPitchSemi[4]`, `float layerAmpMul[4]`, `float layerPanAdd[4]`

#### `ModMatrix.cpp`
- Initialised new arrays in `evaluate()`: pitch→0, amp→1 (multiplicative unity), pan→0
- Added 12 new `switch` cases with musically sensible scaling:
  - Pitch: `c * 24.f` → ±24 semitones at full mod amount
  - Amp: `1.f + c * 0.35f` multiplicative → tremolo without clipping
  - Pan: `c * 0.5f` → ±0.5 pan swing at full amount
- Added post-accumulation clamping: pitch `[-48, +48]`, amp `[0, 2]`, pan `[-1, +1]`

#### `SynthEngine.cpp`
- Declared 3 new stack arrays: `layerPitchSemi[4]`, `layerAmpMul[4]`, `layerPanAdd[4]`
- Passed to `modMatrix.evaluate()`
- Forwarded to each `VoiceLayer::renderAdd()` call

#### `VoiceLayer.h`
- Updated `renderAdd()` declaration with 3 new defaulted parameters:
  - `float modMatrixPitchSemi = 0.f`
  - `float modMatrixAmpMul = 1.f`
  - `float modMatrixPanAdd = 0.f`
- Documented each parameter in the declaration comment

#### `VoiceLayer.cpp`
- Renamed internal `mPitchSemi` to `masterPitchSemi` (was confusing — it read the *master pitch param*, not a mod value)
- Applied pitch mod: `hz *= std::pow(2.0, (double)modMatrixPitchSemi / 12.0)` — fractional semitone-accurate
- Applied amp mod: `sig *= (double)jlimit(0.f, 2.f, modMatrixAmpMul)` — after volume, before pan
- Applied pan mod: added `modMatrixPanAdd` into `panBip` computation before the constant-power angle

---

## 4. No Other Gaps Found

After thorough review of all engine source files, **no other spec gaps exist**. The codebase faithfully implements:

- ✅ 15 chord types in APVTS + MidiPipeline interval arrays
- ✅ 5 Keys Lock modes (Off, Remap, Mute, Chord Tones, Chord Scales)
- ✅ 6 arp play patterns (Up, Down, Up-Down, Order, Chord, Random)
- ✅ 32-step arp grid with per-step On/Vel/Dur/Transpose/Ratchet (×5 = 160 APVTS params)
- ✅ 8 oscillator types (Sine, Saw, Square, Triangle, Wavetable, Granular, FM, Sample)
- ✅ 8 filter types (LP24, LP12, BP, HP12, Notch, HP24, Comb, Formant)
- ✅ 8-voice unison with per-voice cent offsets and stereo spread
- ✅ Per-layer LFO2 (rate, depth, shape) driving filter cutoff
- ✅ Parallel vs. serial filter routing per layer
- ✅ 16-grain granular engine with Hanning windowing
- ✅ 2-op FM synthesis
- ✅ 32-slot mod matrix — 10 sources, 26 targets (after this fix)
- ✅ XY Pad with 3 physics modes (Direct, Inertia, Chaos)
- ✅ 23 FX unit types across Layer/Common/Master racks
- ✅ Mod matrix→FX integration (Reverb, Delay, Chorus mix modulation)
- ✅ Full JUCE dsp module usage for EQ, reverb, delay, filters

---

## 5. Files Changed

| File | Change |
|---|---|
| `Source/engine/ModMatrix.h` | +12 Target enum values; updated `evaluate()` signature |
| `Source/engine/ModMatrix.cpp` | New target cases + clamping in `evaluate()` |
| `Source/engine/SynthEngine.cpp` | 3 new stack arrays; updated `evaluate()` + `renderAdd()` calls |
| `Source/engine/synth/VoiceLayer.h` | Updated `renderAdd()` signature (+3 defaulted params) |
| `Source/engine/synth/VoiceLayer.cpp` | Applied pitch/amp/pan mod; renamed `mPitchSemi` → `masterPitchSemi` |
