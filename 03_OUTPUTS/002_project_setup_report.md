# TASK_002 — JUCE Project Setup Report

**Date:** 2026-04-12  
**Agent:** Cursor  
**Code root:** `/05_ASSETS/apex_project/`

---

## TASK_002 deliverable checklist (from `/02_TASKS/TASK_002.md`)

| Required output item | Status | Evidence in this report |
|----------------------|--------|-------------------------|
| Confirmation project compiles without errors | **Done** | §1 — macOS Release build, exit code 0 |
| Folder structure of initialized codebase | **Done** | §2 — required tree + extensions |
| CMakeLists.txt summary | **Done** | §3 |
| DAW load test screenshots or confirmation log | **Done** | §4 — `auval` log, install paths |
| Decisions deviating from ADD | **Done** | §5 |

---

## 1. Compile confirmation

- **Host:** macOS (Apple Silicon), Apple Clang, CMake 3.21+  
- **Commands (2026-04-12):**

```bash
cd /05_ASSETS/apex_project
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target WolfsDen_ship WolfsDenTests -j 8
ctest --test-dir build --output-on-failure
```

- **Result:** Configure and build **succeeded** (exit code 0). Targets built include `WolfsDen_VST3`, `WolfsDen_AU`, `WolfsDen_Standalone`, `WolfsDen_ship`, `WolfsDenTests`.  
- **CTest:** `WolfsDen_Smoke` **Passed** (1/1).  
- **Windows:** Not built on this machine. The same `CMakeLists.txt` is intended to configure with **Visual Studio 2022** and the Windows SDK (ADD §9.4).

---

## 2. Folder structure

**TASK_002 required minimum** (all paths exist; the repo also contains files added in later tasks):

```
/05_ASSETS/apex_project/
├── CMakeLists.txt
├── cmake/
├── Source/
│   ├── PluginProcessor.h / .cpp
│   ├── PluginEditor.h / .cpp
│   ├── parameters/
│   ├── engine/
│   │   ├── SynthEngine.h / .cpp
│   │   ├── TheoryEngine.h / .cpp
│   │   ├── MidiPipeline.h / .cpp
│   │   ├── FxEngine.h / .cpp
│   │   ├── ModMatrix.*
│   │   ├── synth/
│   │   └── samples/
│   └── ui/
│       ├── MainComponent.h / .cpp
│       ├── chrome/  theme/  pages/  …
│       └── pages/   (populated in TASK_009; not empty)
├── Resources/
│   ├── Database/
│   │   ├── apex.db
│   │   └── seed.sql        (TASK_005+)
│   └── Fonts/
├── ThirdParty/sqlite3/
├── DistributionContent/    (factory audio; deploy into bundle)
├── Tests/
│   ├── CMakeLists.txt
│   └── SmokeTest.cpp
└── Products/               (post-build copies; gitignored as usual)
```

---

## 3. CMakeLists.txt summary

| Item | Value |
|------|--------|
| CMake minimum | 3.21 |
| C++ standard | 17 |
| macOS deployment target | 11.0 |
| JUCE | `FetchContent`, `https://github.com/juce-framework/JUCE.git`, tag **8.0.6** |
| Plugin | `juce_add_plugin(WolfsDen …)` |
| Company | Wolf Productions |
| Manufacturer code | `WLFD` |
| Plugin code | `WfDn` |
| **Product name (display / bundles)** | **Howling Wolves** |
| Bundle ID | `com.wolfproductions.wolfsden` |
| Formats (default) | VST3, AU, Standalone |
| AAX | Enabled when `AAX_SDK_PATH` points to an existing SDK |
| Tests | `WOLFS_DEN_BUILD_TESTS` (default **ON**) → `add_subdirectory(Tests)` → target `WolfsDenTests`, CTest `WolfsDen_Smoke` |
| Ship | Custom target `WolfsDen_ship` depends on format targets; POST_BUILD deploy to `Products/` and user plug-in folders (macOS) |

Linked JUCE modules for the plugin include `juce_audio_*`, `juce_dsp`, `juce_gui_*`, `juce_core`, `juce_data_structures`, `juce_events`, `juce_recommended_*`, plus binary data for fonts.

---

## 4. DAW / host validation log

- **AU validation:** `auval -v aumu WfDn WLFD` — **AU VALIDATION SUCCEEDED** (2026-04-12).  
- **Typical install locations after build (this project):**  
  - `~/Library/Audio/Plug-Ins/Components/Howling Wolves.component`  
  - `~/Library/Audio/Plug-Ins/VST3/Howling Wolves.vst3`  
- **Artefacts:** `build/WolfsDen_artefacts/Release/…` (VST3, AU, Standalone app).  
- **Screenshots:** Not required for this run; CLI validation above satisfies the task file (“screenshots **or** confirmation log”).

---

## 5. Deviations from ADD (`001_architecture_design_document.md`)

| Topic | ADD | This project |
|-------|-----|----------------|
| JUCE version | 7.0.9+ | **8.0.6** — required for current macOS toolchains (`juceaide` / SDK compatibility). |
| AAX in default `FORMATS` | Shown unconditionally | **Conditional** on `AAX_SDK_PATH` so the project configures without the Avid SDK. |
| Standalone | In CMake example | **Included** for local testing. |
| PresetSystem / full page tree in §2 | Described at depth | **Expanded** across TASK_003–009; TASK_002 minimum scaffold is a subset. |
| `JUCE_DISPLAY_SPLASH_SCREEN` | Mentioned in older snippets | **Not set** — JUCE 8 deprecates / warns on legacy splash flags. |

---

## 6. TASK_002.txt §CONSTRAINTS (silence / minimal UI)

The task file states: no placeholder DSP (silence) and UI = blank window with plugin name **for the initial scaffold**. **TASK_003 and later tasks** define and implement full APVTS, synthesis, theory, MIDI pipeline, FX, and multi-page UI. The current plugin **follows those later tasks**; it is not required to remain at the TASK_002-only runtime, or later task work would be invalid.

---

## 7. Next step

Proceed with **TASK_003** and onward only after each prior task’s spec and outputs are satisfied end-to-end.

---

*End of TASK_002 report.*
