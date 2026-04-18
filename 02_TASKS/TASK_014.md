TASK ID: 014

PHASE: 2

STATUS: PENDING

GOAL:
Audit the entire codebase and remove all dead code, unused stubs, unreachable branches, and placeholder implementations that are not connected to any real functionality. The codebase grew across multiple agents and remediation passes — it has accumulated orphaned files, stub oscillator modes that produce silence, FX type enum entries with no DSP, and disconnected APVTS parameters. Remove all of it. The result must be a clean, honest codebase where every file, class, function, and parameter does exactly what its name says.

ASSIGNED TO:
Cursor

INPUTS:
All files under /05_ASSETS/apex_project/Source/
/05_ASSETS/apex_project/CMakeLists.txt

OUTPUT:
/03_OUTPUTS/014_dead_code_removal_report.md
(Code changes in /05_ASSETS/apex_project/)

WHAT TO AUDIT AND REMOVE:

1. OSCILLATOR STUBS
   Wavetable, Granular, and FM oscillator modes were marked as "stand-ins" in the project log.
   - Inspect VoiceLayer.cpp renderAdd() for each osc type case
   - Any case that returns silence, a sine wave regardless of mode name, or a TODO comment is a stub
   - Either implement it properly (out of scope for this task) OR remove the enum value, the case branch, the APVTS parameter entry, and the UI control that selects it
   - Do not leave a named mode that does nothing — users selecting "Granular" must not get silence

2. FX ENGINE STUBS
   FxEngine has 23+ FX type enum entries. Inspect each one:
   - If the case branch has no DSP (passes audio through unmodified, has a TODO, or only adjusts gain), remove it from the enum, the switch, the APVTS type parameter string list, and the FX slot UI
   - Document each removed type in the report

3. UNUSED APVTS PARAMETERS
   The parameter layout grew across multiple passes and may contain parameters with no UI attachment and no DSP read.
   - For each parameter in WolfsDenParameterLayout.cpp: confirm it is (a) read in DSP code or (b) attached in UI code
   - Parameters that are neither: remove from layout + remove from registry

4. ORPHANED SOURCE FILES
   Check CMakeLists.txt target_sources list. For any .cpp file listed:
   - Confirm it is still needed (not superseded by a rewrite)
   - If a file exists but is empty, contains only stub classes, or is never #included: remove from CMakeLists.txt and delete the file

5. DEAD UI CONTROLS
   Controls in the UI that are attached to removed parameters or that have no effect:
   - Sliders, buttons, or combos with no APVTS attachment and no manual callback: remove

6. COMMENTED-OUT CODE BLOCKS
   Large blocks of commented-out code (not documentation comments): remove entirely.

WHAT NOT TO REMOVE:
- Any code that is confirmed working in Logic Pro testing
- Chord Mode, Keys Lock, slow-rate Arp, Theory Engine, FX types confirmed working
- The preset system (BrowsePage, TheoryEngine preset API, factory seeds)
- Any stub that is explicitly marked for Phase 2 implementation (wavetable / granular / FM may be removed OR retained with a clear TODO, at discretion — but must not be accessible to users under a non-functional name)

DELIVERABLES INSIDE OUTPUT REPORT:
- Complete list of everything removed: file / class / function / parameter / enum value
- For each removed item: why it was removed (stub / unreachable / no DSP / no UI attachment)
- Confirmation that the build compiles clean after removal
- Confirmation that confirmed-working features are unaffected
- Parameter count before and after removal
- Build: cmake --build build --config Release — zero errors

CONSTRAINTS:
- Do not remove anything you cannot confirm is dead — when in doubt, leave it and document the uncertainty
- Do not change any user-visible behavior of confirmed-working features
- APVTS parameter ID registry must remain internally consistent after removals (no dangling IDs)
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
