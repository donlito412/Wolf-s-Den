# 010 — Integration Test Report

**Task:** TASK_010  
**Date:** 2026-04-11  
**Agent:** Cursor (automated verification + static code review)  
**Rule:** No fabricated host tests, benchmarks, or listening results.

---

## Overall

| Gate | Result |
|------|--------|
| Release build, zero **errors** | **PASS** (`cmake --build build3 --config Release --target WolfsDen_ship`, exit 0; compiler warnings only) |
| All TASK_010 checklist items PASS | **NO** — see below |
| TASK_010 file may be marked DONE | **NO** (per TASK_010 constraints) |

**Build note:** Configure/build directory used: `05_ASSETS/apex_project/build3` (repo folder name includes a trailing space; stale `build/` can confuse CMake).

---

## Integration checklist (TASK_010 wording)

Legend: **PASS** = verified here · **CODE** = implementation present, not exercised in host · **NOT RUN** = needs your machine/DAW · **FAIL** = requirement not met in code or not verified

### BUILD

| Item | Status | Evidence |
|------|--------|----------|
| Release build — zero errors | **PASS** | `WolfsDen_ship` completed successfully 2026-04-11 |
| Preset-system compile regressions | **PASS** | Same build includes `TheoryEngine`, `BrowsePage`, `PluginProcessor` preset APIs |

### ENGINE INTEGRATION

| Item | Status | Notes |
|------|--------|-------|
| MIDI → Keys Lock → Chord Mode → Arp → SynthEngine → out | **NOT RUN** | End-to-end host + listening |
| Theory chord detection → bottom bar | **NOT RUN** | |
| Voice Leading, Section C | **NOT RUN** | |
| Mod Matrix LFO → filter cutoff audible | **NOT RUN** | |
| XY pad → mapped params | **NOT RUN** | |
| FX post-synth, reverb tail | **NOT RUN** | |
| Layer FX + master FX routing / bypass | **NOT RUN** | |

### STATE & PRESET

| Item | Status | Notes |
|------|--------|-------|
| Ableton save → reopen → exact restore | **NOT RUN** | Ableton not executed in this pass |
| Preset save DB → Browse load → full state | **CODE** | `saveCurrentAsPreset` → blob; user presets `loadPresetById` → `setStateInformation` |
| Undo/Redo (UndoManager) | **FAIL** | **No `UndoManager` references under `Source/`** — checklist item not implemented |
| Window size saved and restored | **FAIL** | `editorWidth` / `editorHeight` serialized in `CustomState`, but `PluginEditor` ctor always **`setSize(1200, 780)`** — stored size not applied on editor open |

### PRESET SYSTEM (2026-04-10 implementation)

| Item | Status | Notes |
|------|--------|-------|
| Browse opens PATCHES by default; 41 factory cards | **CODE** | `switchMode(BrowseMode::Patches)` in ctor; `kFactory` has **41** entries in `TheoryEngine::seedFactoryPresets` |
| Rhodes card → layer 0 WAV, audible change | **CODE** | `selectPresetForCard` → `loadPresetById` → `applyFactoryPreset` + `Keys/Rhodes.wav`; **NOT RUN** listening |
| Categories → distinct timbre | **CODE** | Different `relPath` + envelopes per row; **NOT RUN** listening |
| `<` `>` step presets + label updates | **CODE** | `TopBar` → `cyclePreset`; label via `getPresetDisplayName` / `onPresetNavigate` **Note:** `cyclePreset` iterates **entire** `getPresetListings()` (factory + user), not “factory only” |
| Save Preset → dialog → new card | **CODE** | `onSavePresetClicked` + `saveCurrentAsPreset`; **NOT RUN** UI click test |
| User preset survives DAW project reopen | **NOT RUN** | |
| CHORD SETS tab unchanged | **CODE** | `BrowseMode::ChordSets` + `BrowsePresetCard` path retained; **NOT RUN** regression pass |
| Factory cards: no Delete | **PASS** | `BrowsePatchCard`: `delBtn` only if `!isFactory` |
| User cards: Delete removes from DB | **CODE** | `deletePresetForCard` + `TheoryEngine::deletePreset`; factory guard SQL `state_blob IS NULL` |

### MIDI PIPELINE

| Item | Status |
|------|--------|
| Keys Lock / Chord Mode / Arp / Latch / MIDI Capture | **NOT RUN** |

### AUDIO QUALITY

| Item | Status |
|------|--------|
| Aliasing A0–C8 / voice-steal clicks / reverb tail / DC / gain staging | **NOT RUN** (no analyzer session) |

### PERFORMANCE

| Item | Status | Notes |
|------|--------|--------|
| M1 CPU &lt; 20% (16 voice, 4 layers, full FX, mod) | **NOT RUN** | No benchmark captured |
| Open &lt; 1 s | **NOT RUN** | |
| Preset load &lt; 200 ms | **NOT RUN** | |
| 1 h playback — zero crashes | **NOT RUN** | |
| Memory leaks — Instruments 1 h | **NOT RUN** | |

---

## CPU benchmarks

**Not measured.** TASK_010 target (&lt;20% on Apple M1 under full load) requires a host run on your hardware. Record results here when available.

---

## Crash log (1-hour test)

**Not executed.** No session performed in this pass.

---

## DAW compatibility (Ableton Live, Logic Pro, FL Studio)

**Not executed** in this pass. VST3/AU were deployed to `~/Library/Audio/Plug-Ins/` by `WolfsDen_ship` build on the build machine; host validation is still required.

---

## Bug log

| ID | Severity | Description | Status |
|----|----------|-------------|--------|
| BUG_INT_001 | **High** | TASK_010 requires JUCE **UndoManager** for undo/redo; **not present** in codebase | **Open** |
| BUG_INT_002 | **Medium** | Editor **window size** persisted in state but **not restored** (`PluginEditor` fixed initial size) | **Open** |
| BUG_INT_003 | **Low** | Top-bar `<` `>` cycles **all** DB presets, not “41 factory only” if user presets exist | **Open** (spec clarity vs implementation) |
| BUG_HIST_01 | — | Prior log: AU/host crash around `getPlayHead()` — cited as fixed in earlier draft | **Unverified** in this pass |
| BUG_INT_004 | **High** | `FxEngine` read layer/common/master **mix** and **EQ band dB** from raw APVTS atomics (normalized 0–1) instead of physical values — FX rack ineffective | **Fixed** 2026-04-10: `readFloatAP` + `AudioParameterFloat*` cache in `FxEngine.{h,cpp}`; rebuild `WolfsDen_ship` |

*(Prior TASK_010 run: no code changes. Subsequent integration fix: BUG_INT_004.)*

---

## Inputs reviewed

- `04_LOGS/project_log.md` — 2026-04-10 preset system entry  
- `02_TASKS/TASK_010.md`  
- Engine/UI reports 004–009: not re-summarized here; checklist derived from TASK_010 only  
- Source spot-check: `BrowsePage.cpp`, `PluginProcessor.cpp`, `TheoryEngine.cpp` (factory seed + delete guard), `PluginEditor.cpp`, `TopBar.cpp`

---

## Next step

Complete **NOT RUN** / **FAIL** rows in a DAW on M1; fix **BUG_INT_001** / **BUG_INT_002** if TASK_010 full PASS is required; then set `TASK_010.md` **STATUS: DONE** and extend this report with measured CPU numbers and host names/versions.
