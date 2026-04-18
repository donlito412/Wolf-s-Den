TASK ID: 018

PHASE: 2

STATUS: PENDING

GOAL:
Produce a verified, clean Release build of Wolf's Den on Windows 10/11 (VST3 + Standalone). All development and testing has been macOS-only. The CMakeLists.txt has macOS-specific deployment target settings and macOS-specific POST_BUILD deploy scripts. The Windows build has never been attempted. This task identifies and fixes every Windows-specific build issue, removes macOS-only assumptions from shared code, and confirms the plugin loads in a Windows DAW.

ASSIGNED TO:
Cursor (Windows machine or CI environment required)

INPUTS:
/05_ASSETS/apex_project/CMakeLists.txt
/05_ASSETS/apex_project/Source/ (all files)

OUTPUT:
/03_OUTPUTS/018_windows_build_report.md

---

CURRENT STATE — CONFIRMED BY READING CMakeLists.txt

1. Line 13: `set(WOLFS_DEN_PLUGIN_FORMATS VST3 AU Standalone)` — AU is macOS-only format. On Windows, AU must be excluded or the build will fail. JUCE silently skips unsupported formats but this should be explicit.

2. Lines 9–11: `if(APPLE) set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0") endif()` — This is guarded correctly.

3. Lines 70–130: ALL POST_BUILD deploy scripts (copy to ~/Library/Audio/Plug-Ins/) are macOS-only paths. These will not exist on Windows. Must be wrapped in `if(APPLE)` or the equivalent Windows path logic added.

4. JUCE FetchContent (line 23): JUCE 8.0.6 is cross-platform. FetchContent will work on Windows as long as git is available. No change needed.

5. SQLite3 source (ThirdParty/sqlite3/sqlite3.c): SQLite is cross-platform but may have Windows-specific warnings. Already partially guarded in earlier passes — verify.

6. Source code: search for any `#include <CoreAudio/>`, `#include <AudioToolbox/>`, `#include <mach/>`, or macOS SDK headers included without `#if JUCE_MAC` guards.

---

FIXES — IMPLEMENT ALL

FIX 1 — EXCLUDE AU FORMAT ON WINDOWS
File: CMakeLists.txt, line 13

```cmake
# Old:
set(WOLFS_DEN_PLUGIN_FORMATS VST3 AU Standalone)

# New:
if(APPLE)
    set(WOLFS_DEN_PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(WOLFS_DEN_PLUGIN_FORMATS VST3 Standalone)
endif()
```

FIX 2 — WRAP ALL POST_BUILD MACROS IN if(APPLE)
File: CMakeLists.txt, lines 70–130+

All POST_BUILD commands that reference `~/Library`, `cp -r`, `rsync`, `ditto`, `.component`, or macOS bundle paths must be inside `if(APPLE)` blocks.

Add a Windows POST_BUILD for VST3 that copies to the standard Windows VST3 location:
```cmake
if(WIN32)
    if(TARGET WolfsDen_VST3)
        add_custom_command(TARGET WolfsDen_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:WolfsDen_VST3>"
                "$ENV{PROGRAMFILES}/Common Files/VST3/Howling Wolves 2.0.vst3"
            COMMENT "Wolf's Den: VST3 -> C:/Program Files/Common Files/VST3/"
        )
    endif()
endif()
```

FIX 3 — AUDIT SOURCE FILES FOR PLATFORM-SPECIFIC INCLUDES
Search all .h and .cpp files under Source/ for:
- `#include <CoreAudio/` → must be inside `#if JUCE_MAC`
- `#include <AudioToolbox/` → same
- `#include <mach/` → same
- `#include <sys/sysctl.h>` → Linux/Mac only
- Any `__attribute__((visibility` → GCC/Clang only, wrap in `#ifndef _MSC_VER`
- Any `#pragma clang` → wrap in `#ifdef __clang__`

Fix any found by wrapping in the appropriate platform guard.

FIX 4 — WINDOWS COMPILER COMPATIBILITY
MSVC (the Windows C++ compiler) has stricter conformance than Clang/GCC in several areas that have caught JUCE projects:

a) Designated initializers: C++20 only in MSVC. Confirm CMakeLists uses C++17 (`CMAKE_CXX_STANDARD 17` — already set on line 4). If any Source/ file uses `{ .field = value }` C99 designated initializers, replace with standard C++ initialization.

b) `[[maybe_unused]]` — confirm all uses are C++17 attribute syntax, not GCC extension syntax.

c) `constexpr` arrays larger than 1MB: MSVC has stack size limits. The `WavetableBank.h` from TASK_015 will have 20 × 256 × 8 bytes = 40KB — fine. Confirm no other large constexpr arrays exist.

d) Integer narrowing warnings: MSVC warns on implicit int→float casts that Clang silently accepts. Audit the most common patterns in VoiceLayer.cpp and FxEngine.cpp and add explicit casts where needed.

e) `std::min` / `std::max` with mixed types: MSVC errors on these. Confirm all min/max calls use same types or explicit casts.

FIX 5 — SQLITE3 WINDOWS BUILD
File: ThirdParty/sqlite3/sqlite3.c
SQLite on Windows needs:
```cmake
if(MSVC)
    target_compile_options(sqlite3_lib PRIVATE /W0)  # suppress all warnings in sqlite3
endif()
```
Also confirm `sqlite3.c` compiles as C (not C++) — CMakeLists must use `LANGUAGES C CXX` (already set on line 2).

FIX 6 — APPLICATION SUPPORT PATH ON WINDOWS
File: PluginProcessor.cpp — search for `Application Support` or `~/Library`

JUCE provides `juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)` which returns the correct path on both macOS and Windows. Any hardcoded macOS paths (`~/Library/Application Support/Wolf Productions/`) must be replaced with the JUCE cross-platform API.

Search for and replace all occurrences:
```cpp
// Bad (macOS only):
juce::File dbPath = juce::File("~/Library/Application Support/Wolf Productions/...");

// Good (cross-platform):
juce::File dbPath = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Wolf Productions/Wolf's Den/...");
```

---

BUILD AND TEST PROCEDURE

1. On a Windows 10 or 11 machine with Visual Studio 2022 (MSVC) or MinGW installed:
```
cd 05_ASSETS/apex_project
mkdir build_win && cd build_win
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release --target WolfsDen_ship
```

2. Confirm: zero errors, warnings acceptable (document any non-trivial ones)

3. Load `WolfsDen_artefacts/Release/VST3/Wolf's Den.vst3` into one of:
   - JUCE AudioPluginHost (included with JUCE source — cross-platform, easiest to test)
   - Ableton Live (Windows trial is free)
   - FL Studio (has a free trial)

4. Confirm: plugin loads without crash, produces audio, UI renders

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Full list of every change made to CMakeLists.txt with old → new
- Full list of every source file change for platform guards
- Build log excerpt showing zero errors (paste last 20 lines)
- Screenshot or text confirmation that plugin loads in AudioPluginHost or DAW on Windows
- Any warnings left unresolved, with justification

CONSTRAINTS:
- Do not break the macOS build — all changes must be additive guards, not replacements
- Do not change any APVTS parameter IDs or DSP behavior
- AU format must still build on macOS
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
