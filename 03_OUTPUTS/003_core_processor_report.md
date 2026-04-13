# TASK_003 — Core Processor & Parameter System Report

**Date:** 2026-04-12 (documentation refresh; code as in repo)  
**Agent:** Cursor  
**Code:** `/05_ASSETS/apex_project/Source/`

---

## 1. Overview

- **`AudioProcessorValueTreeState`** holds all automatable parameters. **`juce::ParameterID`** uses **version `2`** (`kParamVersion` in `WolfsDenParameterLayout.cpp`). Host-visible automation keys are the **string IDs** from `makeParameterLayout()`.
- **Fixed `uint32` registry** for the **core** parameter set: `Source/parameters/WolfsDenParameterRegistry.h` (`wolfsden::pid::Registry`). **Do not renumber or reuse** those values after v1.0. Many **additional** parameters (arp step rows, FX rack slots, EQ bands, per-slot FX macros) exist **only** as string IDs in the layout and are **not** duplicated in the enum—treat **string ID** as the stable automation identity for those.
- **Parameter count (current):** **544** `RangedAudioParameter` instances created in `makeParameterLayout()` (includes 4 layers × 29 layer params, 32×5 arpeggiator step params, common/layer/master FX slots, 24×4-band EQ gains, 24×4 generic FX params, theory/MIDI/global/LFO/FX mix blocks, etc.). *Source of truth:* `WolfsDenParameterLayout.cpp`.
- **UI ↔ DSP bridge:** `juce::AbstractFifo` (capacity **512**) + `std::array<UiToDspMessage, 512>`. **No mutex** in `drainUiToDspFifo()` or the FIFO drain path inside `processBlock()`.
- **Producer:** `AudioProcessorValueTreeState::Listener::parameterChanged` pushes messages **only** for five whitelisted IDs (automation smoke list; see §4). **Other** parameters update via APVTS atomics / attachments as usual and **do not** go through this FIFO unless extended.
- **Consumer:** `drainUiToDspFifo()` at the **start** of `processBlock()` reads batches (up to 256 per slice), and for each message finds the matching parameter by **hash of string ID** and **`store`s** the normalized value into `apvts.getRawParameterValue(...)`. Messages are **applied**, not discarded.
- **State:** Binary XML via `copyXmlToBinary` / `getXmlFromBinary`. Root tag **`WolfsDenState`**: child matching APVTS state type, **`CustomState`** (preset/browse/preset-id/chord blob + editor dimensions), **`ModMatrix`** (`synthEngine.getModMatrix().toValueTree()`). Legacy sessions that only saved the APVTS root still load (`setStateInformation` fallback).

---

## 2. Lock-free FIFO (UI → DSP)

| Item | Detail |
|------|--------|
| Type | `juce::AbstractFifo` + fixed `std::array<UiToDspMessage, 512>` |
| Message | `uint32_t paramIdHash` (`juce::String::hashCode()` of parameter ID), `float value` (normalized 0–1 from listener) |
| Producer | `parameterChanged()` on message thread — **only** for: `master_volume`, `master_pan`, `layer0_volume`, `lfo_rate`, `fx_reverb_mix` |
| Consumer | `drainUiToDspFifo()` — applies each message to the matching raw parameter value; no heap allocs in the drain loop |
| `processBlock()` | Calls `drainUiToDspFifo()` first; FIFO path has **no locks** |

---

## 3. State save / recall

- **Save:** `getStateInformation` builds `WolfsDenState` → APVTS copy + `CustomState` + ModMatrix `ValueTree` → binary XML.
- **`CustomState` properties (current):** `presetName`, `browseChordSetId`, `chordData`, `currentPresetId`, plus `editorWidth` / `editorHeight` (from atomics; set via editor).
- **Restore:** `setStateInformation` restores APVTS, custom fields under lock, ModMatrix from child `ModMatrix` or reset if absent; `syncTheoryParamsFromApvts()` after param replace.
- **DAW project test:** Save project with plugin, reload—params and custom chunk should restore. Resize editor; save/reload—dimensions should persist (subject to host behavior).

---

## 4. Automation test (≥ 5 parameters)

The processor still whitelists these five IDs for FIFO mirroring (`PluginProcessor.cpp`):

1. `master_volume`  
2. `master_pan`  
3. `layer0_volume`  
4. `lfo_rate`  
5. `fx_reverb_mix`  

**UI:** There is **no** dedicated “five slider” strip on `MainComponent` anymore; the product UI is the full multi-page editor (`Browse`, `Synth`, `Theory`, etc.). **Automation** is validated via the **host’s parameter list** / lanes for any published ID (including the five above and the other 539 parameters).

**Manual check:** In any DAW, expose plugin parameters, arm automation, and move or draw automation for at least five distinct IDs (e.g. the list above)—lanes should follow. *This report does not replace a recorded test log in TASK_010 if you require written proof.*

---

## 5. Registry vs layout (core `uint32` block)

JUCE **string ID** is the automation key for **all** parameters. The table below summarizes the **`WolfsDenParameterRegistry.h`** entries (core globals + per-layer blocks). **Per-layer** registry values follow the pattern: layer0 base `0x02000001`, layer1 `0x03000001`, layer2 `0x04000001`, layer3 `0x05000001`, with the same suffix ordering as layer0.

### Master, global filter ADSR, LFO, theory, MIDI (subset), performance, FX mix

| Registry `uint32` | String ID | Notes |
|-------------------|-----------|--------|
| 0x01000001 | master_volume | float |
| 0x01000002 | master_pitch | float |
| 0x01000003 | master_pan | float |
| 0x01000101–0x01000104 | filter_adsr_* | attack/decay/sustain/release |
| 0x01000201–0x01000203 | lfo_rate, lfo_depth, lfo_shape | |
| 0x01000301–0x01000307 | theory_scale_root … theory_chord_density | includes chord anchor/octave/density |
| 0x01000401–0x0100040A | midi_keys_lock_mode … midi_arp_sync_ppq | registry holds main MIDI pipeline IDs |
| 0x01000601 | perf_xy_physics | choice |
| 0x01000501–0x01000504 | fx_reverb_mix … fx_compressor | |

**Not in registry enum:** `midi_arp_s00_on` … `midi_arp_s31_rkt` (160 params), all `fx_c*`, `fx_l*`, `fx_m*`, `fx_s*_eq*`, `fx_s*_p*`, etc.—still fully real parameters with string IDs in `makeParameterLayout()`.

### Per-layer suffixes (repeat for `layer0_` … `layer3_`)

Each layer adds **29** parameters, including: `volume`, `pan`, `osc_type`, `tune_coarse`, `tune_fine`, `filter_cutoff`, `filter_resonance`, `filter_type`, `amp_*`, `lfo2_*`, `filter_routing`, `unison_*`, `tune_octave`, `filter_drive`, `filter2_*`, `gran_*`. See `addLayerParams()` for ranges and defaults.

---

## 6. TASK_003 constraint: zero allocations in `processBlock()`

Intent: no heap churn on the audio thread. **Current code:** `synthLayerBus.setSize(...)` in `processBlock` can **allocate or reallocate** when channel count or block size **grows** (there is a comment about avoiding per-block realloc for variable host sizes). For strict “never allocate in `processBlock`,” reserve the **maximum** channels and block size in `prepareToPlay` and avoid `setSize` on the audio thread—**not** addressed in this documentation-only update.

---

## 7. Files (primary)

| File | Role |
|------|------|
| `Source/parameters/WolfsDenParameterRegistry.h` | Fixed `uint32` enum for **core** PIDs |
| `Source/parameters/WolfsDenParameterLayout.{h,cpp}` | Full APVTS layout factory |
| `Source/PluginProcessor.{h,cpp}` | APVTS, FIFO, listener, state, `processBlock` orchestration |
| `Source/PluginEditor.{h,cpp}` | Reports editor size to processor |
| `Source/ui/MainComponent.{h,cpp}` | Main shell (pages); **not** the old five-slider automation strip |

---

## 8. Next step

**TASK_004** and beyond implement engines against this processor; **TASK_010** can capture formal automation/state test logs if required.

---

*End of TASK_003 report.*
