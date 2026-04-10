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
| **Browse** | **TASK_009 layout:** ~30% left filter sidebar (search, genre + mood toggle pills incl. spec labels + DB extras, Low/Med/High energy, scale combo) + ~70% scrollable **preset cards** from SQLite `chord_sets` (name, genre/mood tags, mood accent stripe, ★ favourite, Play). Bottom **detail strip** (title, author, tags, scale blurb). Selection/Play updates processor preset display name + TopBar. **Not in APVTS** (browse is pack metadata, not synth params). **No audio preview engine yet** — Play loads selection to the bar only. |
| **Synth** | Single scrollable page: global filter ADSR + LFO + all four layers’ major APVTS controls (float/int/choice) — not per-layer tabbed three-column oscillator/filter/amp layout. |
| **Theory** | Sub-tabs; scale root/type/voice leading; detect MIDI/audio; browse panel with **3 live match labels**; CoF + explore stubs; degree pads stub alert. |
| **Perform** | Keys lock, chord mode/type/inversion, arp controls, **step selector 1–32** with per-step attachments; MIDI capture = stub alert. |
| **FX** | Rack selector (common / layers / master), **4 slots** type + mix + legacy wet sliders — not drag-reorder or full per-FX expanded panels. |
| **Mod** | **32** matrix slots via 4×8 paging; src/tgt/amt/inv/mute/scope; Dice; Record stub; **PerfXyPad** + `perf_xy_physics` attachment. |

---

## APVTS / attachments

- Synth, Perform, FX slot rows, Theory (scale root/type, voice leading), Mod (XY physics combo) attach to **real parameter IDs** via JUCE 8 attachment helpers.
- Mod matrix rows **do not** use APVTS per slot (by design in current code): they read/write `ModMatrix` directly.

---

## Performance (60 fps)

- **No profiler run** in this task. `paint()` paths are mostly fills + text + meters; heavy work is on timers (CPU label, bottom bar sync, theory match labels), not per-frame DSP. Treat **60 fps as a target**, not a measured guarantee, until profiled in a host.

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

- **Browse:** optional audio preview; applying a chord set to Theory/Perform pipeline (beyond preset name); persist favourites.
- **Synth** layer tabs + full spec; Theory progression track; FX drag-reorder + full param sheets; MIDI capture export; XY→DAW record; **80 ms** animated hover on tabs/buttons (currently instant LAF highlight + slider glow).

---

## Next step

**TASK_010** — integration testing in target hosts, or continue UI polish against the full TASK_009 spec.
