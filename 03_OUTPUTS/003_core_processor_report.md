# TASK_003 — Core Processor & Parameter System Report
**Date:** 2026-04-09  
**Agent:** Cursor  
**Code:** `/05_ASSETS/apex_project/Source/`

---

## 1. Overview

- **APVTS** hosts **70** automatable parameters (`juce::ParameterID` string IDs + version hint **1**).  
- **Fixed public registry (uint32)** for every logical control: `Source/parameters/WolfsDenParameterRegistry.h` (`wolfsden::pid::Registry`). **Do not renumber or reuse values after v1.0.**  
- **UI ↔ DSP bridge:** `juce::AbstractFifo` (capacity **512**) + `std::array<UiToDspMessage,512>` — **single-producer / single-consumer**, **no mutex** in `processBlock()`. Producer: message thread via `AudioProcessorValueTreeState::Listener::parameterChanged` for five whitelisted automation-test IDs. Consumer: `drainUiToDspFifo()` at start of `processBlock()` (no heap allocations).  
- **State:** XML binary via `copyXmlToBinary` / `getXmlFromBinary`. Root tag **`WolfsDenState`**: child **`Parameters`** (APVTS) + **`CustomState`** (`presetName`, `chordData`, `editorWidth`, `editorHeight`). Legacy sessions that only saved the APVTS root are still loaded (`setStateInformation` fallback).  
- **Editor:** five horizontal sliders with `juce::SliderParameterAttachment` for DAW automation smoke test.

---

## 2. Lock-free FIFO (UI → DSP)

| Item | Detail |
|------|--------|
| Type | `juce::AbstractFifo` + fixed `std::array<UiToDspMessage, 512>` |
| Message | `uint32_t paramIdHash` (`juce::String::hashCode()` of parameter ID), `float value` (normalized 0–1 as delivered by APVTS listener) |
| Producer | `parameterChanged()` on message thread — only for: `master_volume`, `master_pan`, `layer0_volume`, `lfo_rate`, `fx_reverb_mix` |
| Consumer | `drainUiToDspFifo()` — discards payloads today; reserved for engines that need block-aligned UI hints without blocking APVTS atomics |
| `processBlock()` | No allocations; no locks |

---

## 3. State save / recall

- **Save:** `getStateInformation` builds `WolfsDenState` → serializes to binary XML.  
- **Restore:** `setStateInformation` restores APVTS + custom fields; chord blob is a string property (empty until progression UI exists).  
- **Preset / DAW test (manual):** Save project in a DAW with plugin on a track, reload project — parameters and custom chunk should restore. Resize editor; save/reload — `editorWidth` / `editorHeight` should persist.

---

## 4. Automation test (≥ 5 parameters)

Expose these in the editor (all attached to APVTS):

1. `master_volume`  
2. `master_pan`  
3. `layer0_volume`  
4. `lfo_rate`  
5. `fx_reverb_mix`  

**Manual check:** In Live / Logic / etc., arm automation, move each slider or draw lanes — meters/lanes should move. Same five IDs also mirror into the FIFO for future DSP-side consumption.

---

## 5. Parameter table (ID string ↔ Registry uint32)

JUCE **string ID** is the stable automation key. **Registry** value is the frozen `uint32` from `WolfsDenParameterRegistry.h`.  
Ranges: see `WolfsDenParameterLayout.cpp` (`NormalisableRange` / int min-max / choice indices).

### Master, global filter ADSR, LFO, theory, MIDI, FX

| Registry `uint32` | String ID | Type | Notes |
|-------------------|-----------|------|--------|
| 0x01000001 | master_volume | float | 0–1, skewed |
| 0x01000002 | master_pitch | float | −48…+48 semitones |
| 0x01000003 | master_pan | float | −1…1 |
| 0x01000101 | filter_adsr_attack | float | s |
| 0x01000102 | filter_adsr_decay | float | s |
| 0x01000103 | filter_adsr_sustain | float | 0–1 |
| 0x01000104 | filter_adsr_release | float | s |
| 0x01000201 | lfo_rate | float | Hz |
| 0x01000202 | lfo_depth | float | 0–1 |
| 0x01000203 | lfo_shape | choice | 6 waveforms |
| 0x01000301 | theory_scale_root | int | 0–11 |
| 0x01000302 | theory_scale_type | choice | 14 scales |
| 0x01000303 | theory_chord_type | choice | 11 types |
| 0x01000304 | theory_voice_leading | bool | |
| 0x01000401 | midi_keys_lock_mode | choice | 5 modes |
| 0x01000402 | midi_chord_mode | bool | |
| 0x01000403 | midi_arp_on | bool | |
| 0x01000404 | midi_arp_rate | float | rate |
| 0x01000501 | fx_reverb_mix | float | 0–1 |
| 0x01000502 | fx_delay_mix | float | 0–1 |
| 0x01000503 | fx_chorus_mix | float | 0–1 |
| 0x01000504 | fx_compressor | float | 0–1 |

### Per-layer (layers 0–3 = UI “Layer 1–4”)

Per layer, **12** parameters; registry base **0x02000001 + layer×0x01000000** (layer0 `0x02…`, layer1 `0x03…`, layer2 `0x04…`, layer3 `0x05…`). Suffix keys (same for each layer, with `layerN_` prefix):

| Suffix | Type |
|--------|------|
| volume | float 0–1 |
| pan | float −1…1 |
| osc_type | choice (8 engines) |
| tune_coarse | int −24…24 |
| tune_fine | float cents |
| filter_cutoff | float 20–20k log |
| filter_resonance | float |
| filter_type | choice (4) |
| amp_attack / amp_decay / amp_sustain / amp_release | float |

**Example:** Layer 2 volume → string `layer2_volume`, registry `0x04000001`.

**Total count:** 22 (global block) + 48 (4×12) = **70** parameters.

---

## 6. Files touched

| File | Role |
|------|------|
| `Source/parameters/WolfsDenParameterRegistry.h` | Fixed `uint32` enum |
| `Source/parameters/WolfsDenParameterLayout.{h,cpp}` | APVTS layout factory |
| `Source/PluginProcessor.{h,cpp}` | APVTS, FIFO, state, listener |
| `Source/ui/MainComponent.{h,cpp}` | Five automation sliders |
| `Source/PluginEditor.{h,cpp}` | Reports editor size to processor |

---

## 7. Next step

**TASK_004** — Synthesis engine implementation.

---

*End of TASK_003 report.*
