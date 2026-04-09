# TASK 007 — Modulation System Report

**Date:** 2026-04-09  
**Code:** `05_ASSETS/apex_project/Source/engine/ModMatrix.{h,cpp}`, `SynthEngine.{h,cpp}`, `synth/VoiceLayer.{h,cpp}`, `PluginProcessor.h`, `Source/ui/PerfXyPad.{h,cpp}`, `Source/ui/MainComponent.{h,cpp}`, `CMakeLists.txt`

## Summary

Implemented a **32-slot modulation matrix** with per-slot atomics, per-sample `evaluate()` on the audio thread, **MIDI-driven** mod wheel / channel & poly aftertouch / pitch bend into the same atomics, and **UI-driven** slot configuration plus a **300×300 XY performance pad** with **Direct**, **Inertia**, and **Chaos (Lorenz)** modes (`perf_xy_physics` APVTS). **Flex-Assign**: right-click mapped sliders to add a routing to the next free slot (from slot 2 upward, preserving defaults on 0–1); **Shift+right-click** lists existing routings for that parameter’s matrix target.

## Mod Matrix

- **Slots:** 32. Each slot: `slotSrc`, `slotTgt`, `slotAmt`, `slotSmooth`, `slotFlags` (mute, invert, layer scope in upper bits).
- **Defaults:** Slot 0 — `XY_X` → `Layer0_FilterCutoffSemi`, amount **0.6**; Slot 1 — `XY_Y` → `LFO_RateMul`, amount **0.4**.
- **Targets implemented in DSP:** per-layer cutoff semitone offset (layers 0–3), layer 0 resonance add, `LFO_RateMul`, `LFO_DepthAdd`, `MasterVolumeMul`.
- **Sources used in `evaluate`:** Global LFO (previous-sample bipolar), filter/amp envelope (max across active voices, layer 0), velocity (max active), pitch bend (argument + atomic from MIDI), mod wheel, aftertouch, XY X/Y, random.
- **Smoothing:** Per-slot one-pole on the product `v * amount` before applying to targets (`smoothZ[]`).
- **Synth integration:** `SynthEngine::process` calls `evaluate`, then scales LFO rate / depth / master gain and passes per-layer cutoff and resonance offsets into `VoiceLayer::renderAdd`. `VoiceLayer` exposes `getLastAmpEnv` / `getLastFiltEnv` for the matrix.

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
