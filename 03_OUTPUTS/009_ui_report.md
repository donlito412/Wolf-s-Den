# TASK_009 — Full custom UI (implementation report)

**Date:** 2026-04-09  
**Code:** `05_ASSETS/apex_project/Source/ui/` (theme, chrome, pages, `MainComponent`, `PerfXyPad`)  
**Build:** Release — VST3, AU, Standalone link successfully (JUCE 8.0.6).

---

## Summary

The plugin editor hosts a **custom LAF + themed layout**: persistent **TopBar** (page tabs, preset stub, settings alert, CPU @ ~8 Hz), **stacked pages** (Browse, Synth, Theory, Perform, FX, Mod), and **BottomBar** (MIDI LED, chord/scale from TheoryEngine, poly count, L/R meters, host sync @ ~30 Hz). **APVTS** uses JUCE 8’s `SliderAttachment`, `ComboBoxAttachment`, and `ButtonAttachment` with `(apvts, parameterID, widget)` signatures.

---

## Design system (TASK_009 palette)

Implemented in `Source/ui/theme/UITheme.{h,cpp}` and `WolfsDenLookAndFeel.{h,cpp}` — colors match the brief (e.g. background `#0D0D0F`, accent purples, text tiers). **Typography:** embedded **Inter** variable font (see UI polish section) at 10 / 12 / 14 / 18 pt for labels, values, panel headers, page headers.

---

## Resize / editor

- `PluginEditor.cpp`: `setSize(1200, 780)`, `setResizable(true, false)`, `setResizeLimits(800, 550, 2560, 1600)`.
- Layout uses `getLocalBounds()` splits and viewports where needed. **Manual resize test** in a host (800×600–1600×1000 window) is recommended; automated UI layout tests were not run.

---

## Pages vs brief (honest scope)

| Page    | Status |
|--------|--------|
| **Browse** | ~30% left filter sidebar (search, genre + mood toggle pills, Low/Med/High energy, scale combo) + ~70% scrollable **preset cards** from SQLite `chord_sets` (name, genre/mood tags, mood accent stripe, ★ favourite, Play). Bottom **detail strip**. **No audio preview engine** — Play loads selection to the bar only (documented future work). |
| **Synth** | Per-layer tabbed 3-column layout (Osc / Filter / Amp) ✅. **Octave, Semitone, Fine** tune knobs ✅. **Filter 1: Type, Cutoff, Resonance, Drive** ✅. **Filter 2: Type, Cutoff, Resonance, Drive** ✅. **Granular controls: Position, Size, Density, Scatter** (visible only in Granular mode) ✅. All APVTS-bound per active layer. |
| **Theory** | Sub-tabs: BrowseChords (8 chord pads, 7 degree pads, progression track, modifier sidebar) ✅; Circle of Fifths (interactive click-to-set) ✅; Explore (scale list) ✅; Colors grid (diatonic / modal / outside colour-coding) ✅. Scale root/type/voice leading + detect MIDI/audio ✅. |
| **Perform** | Keys lock, chord mode/type/inversion, arp controls, **step selector 1–32** with per-step attachments ✅; MIDI capture = documented stub (future pipeline work). |
| **FX** | Rack selector (common / layers A–D / master), **4 slots** type + mix + On/Off ✅. **Expand button → full FX parameter panel** (4 context-sensitive sliders, APVTS-bound, labels match FX type) ✅. **Add FX button → categorised popup browser** (Dynamics, EQ & Filter, Distortion, Modulation, Delay & Reverb, Stereo Utilities) ✅. Drag-reorder = documented future work. |
| **Mod** | **32** matrix slots via 4×8 paging; src/tgt/amt/inv/mute/scope; Dice; Record stub; **PerfXyPad** + `perf_xy_physics` attachment ✅. |

---

## APVTS / attachments

- Synth, Perform, FX slot rows, Theory (scale root/type, voice leading), Mod (XY physics combo) attach to **real parameter IDs** via JUCE 8 attachment helpers.
- Mod matrix rows **do not** use APVTS per slot (by design in current code): they read/write `ModMatrix` directly.

---

## Performance (60 fps)

- **No profiler run** in this task. `paint()` paths are mostly fills + text + meters; heavy work is on timers (CPU label, bottom bar sync, theory match labels), not per-frame DSP. Treat **60 fps as a target**, not a measured guarantee, until profiled in a host.
- Granular controls, Filter 2, and FX expanded panels only add widgets, no per-frame DSP in `paint()` — no additional 60fps risk introduced.

---

## Screenshots

Per-page **screenshots were not captured** in this environment. Suggested location for manual captures: `03_OUTPUTS/screenshots/task_009/` (one PNG per page, dark UI).

---

## Build / CMake

- `CMakeLists.txt` includes: `juce_add_binary_data` target **WolfsDenFonts** (Inter), `UITheme.cpp`, `WolfsDenLookAndFeel.cpp`, `TopBar.cpp`, `BottomBar.cpp`, all page `.cpp` files.

---

## UI polish (2026-04-09 follow-up)

- **Inter** variable font embedded via `juce_add_binary_data` (`Resources/Fonts/InterVariable.ttf`, SIL OFL — see `Resources/Fonts/LICENSE.txt`); `Theme::font*()` and `WolfsDenLookAndFeel` default sans use it.
- **Page transition:** ~150 ms total (75 ms fade out + 75 ms fade in), smoothstep alpha; rapid tab clicks queue the latest target.
- **Chrome:** subtle vertical gradients on top/bottom bars; softer separator lines.
- **Sliders:** thumb hover glow (linear horizontal + vertical).
- **Transport:** BottomBar uses `AudioPlayHead::getPosition()` instead of deprecated `getCurrentPosition` (still only reliable on the audio thread per JUCE docs; UI thread read may vary by host).

## Browse implementation notes (2026-04-09)

- `TheoryEngine::loadChordSetListings()` + `ChordSetListing` / `getChordSetListings()` expose factory chord-set rows for the UI.
- `BrowsePage` + `BrowsePresetCard` in `Source/ui/pages/BrowsePage.{h,cpp}`.

## Deferred / follow-ups

- **Browse:** optional audio preview (no DSP engine yet); applying a chord set to Theory/Perform pipeline beyond preset name display; persist favourites to DB.
- **Theory:** CoF adjacent-segment popup showing shared notes (minor enhancement).
- **Perform:** MIDI capture clip export (requires arrange/preset pipeline work).
- **FX:** drag-to-reorder slots (complex UX, deferred).
- **Mod:** XY → DAW automation record (requires host parameter automation pass).

---

## UI polish completed (2026-04-09 — spec closure pass)

- **Synth Filter column:** Filter 1 + Filter 2 each have Type combo, Cutoff, Resonance, and Drive knobs (APVTS per-layer bindings: `filter_drive`, `filter2_type/cutoff/resonance/drive`).
- **Synth Oscillator column:** Added separate **Octave** knob (`tune_octave`, int -3 to +3) alongside existing Semitone/Fine. Knobs now labeled Oct / Semi / Fine.
- **Synth Granular mode:** Four dedicated sliders — Position, Size, Density, Scatter — appear/hide with their labels when osc mode is set to Granular.
- **New per-layer parameters added** to `WolfsDenParameterLayout.cpp` + `WolfsDenParameterRegistry.h`: `tune_octave`, `filter_drive`, `filter2_type/cutoff/resonance/drive`, `gran_pos/size/density/scatter` (×4 layers = 40 new params).
- **FX expanded panel:** Clicking `…` on any slot reveals a context-sensitive 4-parameter panel (`fx_s{idx}_p{0-3}`, or EQ band params for Parametric EQ). Labels (e.g. Decay / Pre-delay / Damping / Size for Reverb) update automatically when the FX type changes.
- **Add FX browser:** `+ Add FX` button appears when a slot is expanded; shows a `PopupMenu` with sub-categories (Dynamics, EQ & Filter, Distortion, Modulation, Delay & Reverb, Stereo Utilities) and sets the slot type parameter on selection.
- **80ms hover animation:** `WolfsDenLookAndFeel` now extends `juce::Timer` + `juce::ComponentListener`. `drawButtonBackground` drives an animated `hoverAlpha` per component (60 Hz timer, ~0.208 step/frame → 80ms full fade). Stale entries auto-removed via `componentBeingDeleted`.
- **Per-slot FX generic params** added: `fx_s{idx}_p{0-3}` for 24 slots (96 new global params).

## Next step

**TASK_010** — integration testing in target hosts (Ableton Live, Logic Pro, FL Studio).
