# TASK_002 — JUCE Project Setup Report
**Date:** 2026-04-09  
**Agent:** Cursor  
**Code root:** `/05_ASSETS/apex_project/`

---

## 1. Compile confirmation

- **Host:** macOS (Apple Silicon toolchain, Apple Clang 17).  
- **Command:** `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`  
- **Result:** **Success** — targets `WolfsDen_VST3`, `WolfsDen_AU`, `WolfsDen_Standalone` built with **exit code 0**.  
- **Windows:** Not executed on this machine. Per ADD §9.4, **Visual Studio 2022** and the Windows SDK are required; the same `CMakeLists.txt` is intended to configure there.

---

## 2. Folder structure (initialized codebase)

```
/05_ASSETS/apex_project/
├── CMakeLists.txt
├── Source/
│   ├── PluginProcessor.h
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h
│   ├── PluginEditor.cpp
│   ├── engine/
│   │   ├── SynthEngine.h / .cpp
│   │   ├── TheoryEngine.h / .cpp
│   │   ├── MidiPipeline.h / .cpp
│   │   └── FxEngine.h / .cpp
│   └── ui/
│       ├── MainComponent.h / .cpp
│       └── pages/
│           └── .gitkeep
├── Resources/
│   └── Database/
│       └── apex.db          (empty valid SQLite file)
└── Tests/
    └── .gitkeep
```

**Note:** `Source/ui/pages/` is intentionally empty except `.gitkeep` (per TASK_002). No `PresetSystem` or `ThirdParty/sqlite3` yet — TASK_002 scaffold matches the task file; the ADD’s fuller tree (PresetSystem, page components, BinaryData) lands in later tasks.

---

## 3. CMakeLists.txt summary

| Item | Value |
|------|--------|
| CMake minimum | 3.21 |
| C++ standard | 17 |
| macOS deployment target | 11.0 |
| JUCE | `FetchContent` from `https://github.com/juce-framework/JUCE.git`, tag **8.0.6** |
| Plugin target | `juce_add_plugin(WolfsDen …)` |
| Company | Wolf Productions |
| Manufacturer code | `WLFD` |
| Plugin code | `WfDn` |
| Product name | Wolf's Den |
| Bundle ID | `com.wolfproductions.wolfsden` |
| Formats (default) | **VST3**, **AU**, **Standalone** |
| AAX | Added to `FORMATS` **only** when `AAX_SDK_PATH` is set and the path exists |
| Synth / MIDI | `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE` |
| Post-build | `COPY_PLUGIN_AFTER_BUILD TRUE` (installs to user plug-in folders on macOS) |

Linked JUCE modules: `juce_audio_*`, `juce_gui_*`, `juce_core`, `juce_data_structures`, `juce_events`, `juce_recommended_*` (no `juce_opengl` in this skeleton).

---

## 4. DAW load test / validation log

- **AU:** `auval -v aumu WfDn WLFD` — validation **PASS** (cold open ~347 ms; default stereo output bus; 0 input buses as expected for this synth config).  
- **Install paths (this build):**  
  - `~/Library/Audio/Plug-Ins/Components/Wolf's Den.component`  
  - `~/Library/Audio/Plug-Ins/VST3/Wolf's Den.vst3`  
- **Standalone:** `build/WolfsDen_artefacts/Release/Standalone/Wolf's Den.app`  
- **Screenshots:** Not attached; evidence above is CLI validation + successful link/install.

---

## 5. Deviations from ADD (`001_architecture_design_document.md`)

| Topic | ADD | This setup |
|-------|-----|------------|
| JUCE version | 7.0.9+ via FetchContent | **8.0.6** — JUCE 7.0.x **fails to build `juceaide` on macOS 15** (`CGWindowListCreateImage` removed). 8.x includes the platform fix while remaining CMake-based and API-compatible for this skeleton. |
| AAX | Always in `FORMATS` | **Conditional** on `AAX_SDK_PATH` so default clones compile without Avid’s SDK (ADD §9.1 already states AAX SDK is separate/licensed). |
| Standalone | Listed in ADD CMake example | **Included** for fast local testing (no conflict with TASK_002 deliverable). |
| Scaffold depth | ADD shows PresetSystem, pages, sqlite amalgamation | **TASK_002 spec** used: engine stubs + empty `pages/` + empty `apex.db` only. |
| `JUCE_DISPLAY_SPLASH_SCREEN` | ADD sets `0` | **Omitted** — JUCE 8 ignores this flag and warns if set. |
| `add_subdirectory(Tests)` | ADD shows CTest | **Not wired** — `Tests/` exists empty; tests come in later integration work. |

---

## 6. Runtime behavior (TASK_002 constraints)

- **Audio:** `processBlock` clears the output buffer → **silence** (no placeholder oscillators).  
- **UI:** Dark background + centered **“Wolf's Den”** label; resizable editor window.

---

## 7. Next step

Execute **TASK_003** (Core Processor) per `/02_TASKS/TASK_003.md`.

---

*End of TASK_002 report.*
