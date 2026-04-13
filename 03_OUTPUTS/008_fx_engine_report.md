# TASK 008 — FX Engine Report

**Date:** 2026-04-09  
**Code:** `Source/engine/FxEngine.{h,cpp}`, `SynthEngine.{h,cpp}`, `PluginProcessor.{h,cpp}`, `WolfsDenParameterLayout.cpp`, `ModMatrix.{h,cpp}`, `MainComponent.cpp` (flex targets)

## Signal path (implemented)

1. **Synth** writes **8 channels** per block: stereo per layer (`L0,R0 … L3,R3`) after per-voice `renderAdd` and **master volume / mod-matrix master mul**.
2. **Per-layer rack** (4 slots × 4 layers): each layer’s stereo bus runs up to four effects in series (`fx_l{L}_s{S}_type`, `fx_l{L}_s{S}_mix`). Defaults: all **Off**.
3. **Layer buses are summed** to a stereo mix.
4. **Common rack** (4 slots): types `fx_c0_type`…`fx_c3_type` with mixes **`fx_reverb_mix`**, **`fx_delay_mix`**, **`fx_chorus_mix`**, **`fx_compressor`** (defaults: Reverb, Delay, Chorus, Compressor).
5. **Master rack** (4 slots): `fx_m{S}_type` + `fx_m{S}_mix` (defaults: Limiter @ full wet; empty; empty; Stereo Width @ 0.35 mix).

**Mono output:** final stereo mix is **L+R average** into one channel when the bus is mono.

## FX units (`FxUnitType`, 23 algorithms + Off)

All names appear in APVTS choice lists. Implementations are **stereo**, using `juce::dsp` where noted. 

**Update (2026-04-13):** All FX units now support four generic modulatable parameters (**`fx_sNN_p0`…`p3`**) in addition to the Mix control.

| Type | Param 0 (p0) | Param 1 (p1) | Param 2 (p2) | Param 3 (p3) |
| :--- | :--- | :--- | :--- | :--- |
| **Compressor** | Attack | Release | Threshold | Ratio |
| **Limiter** | Ceiling | Drive | - | - |
| **Gate** | Threshold | Attack | Release | - |
| **HPF / LPF** | Frequency | Resonance | - | - |
| **Soft / Hard Clip**| Drive | Ceiling (Hard) | - | - |
| **Bit Crusher** | Bits | Sample Hold | - | - |
| **Waveshaper** | Drive | - | - | - |
| **Chorus / Vibrato**| Rate | Depth | - | - |
| **Flanger** | Rate | Depth | Feedback | - |
| **Phaser** | Rate | Center Freq | - | - |
| **Tremolo** | Rate | Depth | - | - |
| **Auto-Pan** | Rate | Depth | - | - |
| **Delay (Both)** | Time | Feedback | - | - |
| **Reverb (All)** | Room Size | Damping | Pre-delay | - |
| **Stereo Width** | Width | - | - | - |
| **Mono Blend** | Frequency | Side Mix | - | - |

**Parametric EQ (4-band):** Uses dedicated band gain parameters **`fx_sNN_eq0`…`eq3`** (-18…+18 dB).

**Per-slot DSP state:** 24 `SlotDSP` objects (16 layer + 4 common + 4 master), each with reverb, dual delay lines, filters, and modulation state as needed.

## Mod matrix

New targets: **`Fx_ReverbMixAdd`**, **`Fx_DelayMixAdd`**, **`Fx_ChorusMixAdd`** — additive (clamped) on top of APVTS mix before the common rack runs. **SynthEngine** stores last-sample offsets for the block (`getLastFx*`), so modulation tracks the **end** of each audio block.

**UI:** Right-click **Reverb / Delay / Chorus** mix sliders (where exposed) can flex-assign sources to these targets (`MainComponent` mapping).

## Parameters

- **Legacy / macro:** `fx_reverb_mix`, `fx_delay_mix`, `fx_chorus_mix`, `fx_compressor` drive common rack slot 0–3 **wet** amounts.
- **Racks:** `fx_c0_type`…`fx_c3_type`, all `fx_l{L}_s{S}_type|mix`, all `fx_m{S}_type|mix`.
- **Per-slot EQ (96):** `fx_s00_eq0` … `fx_s23_eq3` — when that slot’s type is Parametric EQ, these are the four band gains in **dB**.

Fixed **registry enum** entries were **not** added for every new string ID (existing pattern: string `ParameterID` in layout).

## CPU / testing

- **Build:** Release **VST3 / AU / Standalone** link OK after integration.
- **Benchmark:** Not instrumented in CI; expect cost to scale with **enabled** slots (each **Reverb** is a full `dsp::Reverb`). Recommendation: use **Off** on unused layer slots in heavy sessions.

## Gaps vs TASK_008 long-form spec

- Not every unit matches the **full** parameter count (e.g. EQ bands, spring topology, drawn waveshaper).
- **Mod matrix** does not yet expose **every** FX parameter as a target — only the three common-send adds plus existing synth targets.
- **Preset serialization** of new params is automatic via APVTS XML; no custom chunk.

## Host tail

`getTailLengthSeconds()` returns **2.0** so hosts may keep processing after note-off while reverb decays.
