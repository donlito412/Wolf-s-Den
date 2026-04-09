# TASK 007 — Modulation System Report

**Date:** 2026-04-09  
**Code:** `05_ASSETS/apex_project/Source/engine/ModMatrix.{h,cpp}`, `SynthEngine.{h,cpp}`, `synth/VoiceLayer.{h,cpp}`, `PluginProcessor.h`, `Source/ui/PerfXyPad.{h,cpp}`, `Source/ui/MainComponent.{h,cpp}`, `CMakeLists.txt`

## Summary

Implemented a **32-slot modulation matrix** with per-slot atomics, per-sample `evaluate()` on the audio thread, **MIDI-driven** mod wheel / channel & poly aftertouch / pitch bend into the same atomics, and **UI-driven** slot configuration plus a **300×300 XY performance pad** with **Direct**, **Inertia**, and **Chaos (Lorenz)** modes (`perf_xy_physics` APVTS). **Flex-Assign**: right-click mapped sliders to add a routing to the next free slot (from slot 2 upward, preserving defaults on 0–1); **Shift+right-click** lists existing routings for that parameter’s matrix target.

## Mod Matrix

- **Slots:** 32. Each slot: `slotSrc`, `slotTgt`, `slotAmt`, `slotSmooth`, `slotFlags` (mute, invert, layer scope in upper bits).
- **Defaults:** Slot 0 — `XY_X` → `Layer0_FilterCutoffSemi`, amount **0.6**; Slot 1 — `XY_Y` → `LFO_RateMul`, amount **0.4**.
- **Smoothing:** Per-slot one-pole on the product `v * amount` before applying to targets (`smoothZ[]`).
- **Synth integration:** `SynthEngine::process` calls `evaluate`, then scales LFO rate / depth / master gain and passes all per-layer offsets into `VoiceLayer::renderAdd`. `VoiceLayer` exposes `getLastAmpEnv` / `getLastFiltEnv` for the matrix.

### Sources (10 total) — `ModMatrix::Source` enum

| Index | Name | Signal |
|---|---|---|
| 0 | `GlobalLFO` | Global LFO bipolar, previous sample |
| 1 | `FilterEnv` | Filter envelope 0–1 (max across active voices) |
| 2 | `AmpEnv` | Amp envelope 0–1 (max across active voices) |
| 3 | `ModWheel` | MIDI CC1, 0–1 |
| 4 | `Aftertouch` | Channel + poly pressure, 0–1 |
| 5 | `Velocity` | Note velocity, 0–1 |
| 6 | `PitchBend` | Pitch wheel, bipolar ±1 |
| 7 | `XY_X` | XY pad X axis, 0–1 |
| 8 | `XY_Y` | XY pad Y axis, 0–1 |
| 9 | `Random` | Per-sample LCG noise, bipolar ±1 |

### Targets (26 total) — `ModMatrix::Target` enum

| Index | Name | Effect | Scale |
|---|---|---|---|
| 0–3 | `Layer0–3_FilterCutoffSemi` | Filter cutoff shift per layer | ±48 semitones |
| 4–7 | `Layer0–3_FilterRes` | Filter resonance add per layer | normalised |
| 8 | `LFO_RateMul` | Global LFO rate multiplier | additive (clamped ≥ 0.1) |
| 9 | `LFO_DepthAdd` | Global LFO depth add | normalised |
| 10 | `MasterVolumeMul` | Master output level mul | ±0.35 range |
| 11 | `Fx_ReverbMixAdd` | Reverb wet mix add | ±0.45 |
| 12 | `Fx_DelayMixAdd` | Delay wet mix add | ±0.45 |
| 13 | `Fx_ChorusMixAdd` | Chorus wet mix add | ±0.45 |
| 14–17 | `Layer0–3_PitchSemi` | Pitch shift per layer — **vibrato** | ±24 semitones |
| 18–21 | `Layer0–3_AmpMul` | Amplitude scale per layer — **tremolo** | mul, clamped [0, 2] |
| 22–25 | `Layer0–3_PanAdd` | Stereo pan offset per layer — **auto-pan** | bipolar ±1, additive |

> **Note — gap fix 2026-04-09:** Targets 14–25 (pitch, amp, pan per layer) were added in the post-audit gap fix pass. The original Cursor build contained only targets 0–13. The `evaluate()` signature and `VoiceLayer::renderAdd()` signature were extended accordingly. See `/03_OUTPUTS/task_gap_fixes.md` for full details.

## Threading note (TASK constraint)

TASK_007 text suggested matrix *configuration* from the UI only. In this build, **performance MIDI** (CC1, pitch bend, aftertouch) is written from the **audio thread** into dedicated atomics (`modWheel01`, `pitchBendBipolar`, `aftertouch01`), while **slot definitions** are written from the **UI** (`setSlot`, Flex-Assign). This matches a common “UI edits routing, MIDI streams live controllers” split.

## XY pad

- **Size:** 300×300 (`PerfXyPad`).
- **Physics:** Direct (drag snaps dot), Inertia (spring + damping toward last drag target while timer runs), Chaos (Lorenz integration with light bias toward drag target).
- **APVTS:** `perf_xy_physics` bound with `AudioProcessorValueTreeState::ComboBoxAttachment`.

## Flex-Assign

- Sliders registered: master volume/pan, layer 0 volume/cutoff, LFO rate/depth, reverb mix.
- **Mapped targets:** `master_volume` → `MasterVolumeMul`; `lfo_rate` → `LFO_RateMul`; `lfo_depth` → `LFO_DepthAdd`; `layer0_filter_cutoff` → `Layer0_FilterCutoffSemi`. Others show “no target mapped” until the target enum grows.

## Not in this pass

- Dice / XY automation record to DAW.
- Full param registry → `ModTarget` for every APVTS ID.
- Per-slot smoothing time constant from sample rate (fixed `smooth01` as in task sketch).

## Build

Release configure/build for WolfsDen (VST3/AU/Standalone) succeeded after wiring `ModMatrix.cpp` and `PerfXyPad.cpp`.
