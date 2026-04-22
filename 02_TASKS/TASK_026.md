TASK ID: 026

PHASE: 2

STATUS: DONE

GOAL:
Find out why chords still show dashes after TASK_025. Add a small visible debug label to
CompositionPage that shows the live progression count from the DB. This makes it impossible to
miss whether the right binary and correct DB state are running. Also add a DBG() log line in
seedDatabase() that prints the reseed result so it shows in Xcode/standalone console.

ASSIGNED TO:
Cursor

BUG REFERENCE:
Follow-up to TASK_025. Code confirmed logically correct via simulation. Problem is environmental —
either the wrong binary is being loaded (stale AU cache) or the DB path resolves differently.
The debug label will expose which is happening.

INPUTS:
/05_ASSETS/apex_project/Source/ui/pages/CompositionPage.cpp
/05_ASSETS/apex_project/Source/ui/pages/CompositionPage.h
/05_ASSETS/apex_project/Source/engine/TheoryEngine.cpp

OUTPUT:
/03_OUTPUTS/026_chord_debug_report.md

---

CHANGE 1 — Add DBG() log to seedDatabase() so reseed result is visible in console

File: TheoryEngine.cpp

After the `if (newCount > 0)` block (around line 1248), add ONE line of DBG output.

Find this block:
```cpp
if (newCount > 0)
{
    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 5);",
        nullptr, nullptr, nullptr);
}
```

Change to:
```cpp
if (newCount > 0)
{
    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 5);",
        nullptr, nullptr, nullptr);
    DBG("WolfsDen seedDatabase: reseeded OK, count=" << newCount << ", version=5");
}
else
{
    DBG("WolfsDen seedDatabase: WARNING — 0 progressions inserted, version NOT written!");
}
```

Also add one DBG line at the top of the `if (stored_version < kProgSeedVersion)` block:

Find:
```cpp
if (stored_version < kProgSeedVersion)
{
    // Delete and re-seed with correct root_sequence data
    sqlite3_exec(db, "DELETE FROM progressions;", nullptr, nullptr, nullptr);
```

Change to:
```cpp
if (stored_version < kProgSeedVersion)
{
    DBG("WolfsDen seedDatabase: stored_version=" << stored_version << " < " << kProgSeedVersion << ", reseeding...");
    sqlite3_exec(db, "DELETE FROM progressions;", nullptr, nullptr, nullptr);
```

---

CHANGE 2 — Add a visible "DB: N progressions" debug label to CompositionPage

This label will appear in the top-right corner of the Composition page. It shows the ACTUAL
live count of progressions in the DB. If it shows "DB: 88" the seeding worked. If it shows
"DB: 0" the reseed failed. If the label doesn't appear at all, an old binary is running.

File: CompositionPage.h

In the Members section (after the `// --- Perform bar ---` section), add:
```cpp
// --- Debug label (temporary, shows DB progression count) ---
juce::Label dbgLabel;
```

File: CompositionPage.cpp

In the constructor (after `startTimerHz(10);`), add:
```cpp
dbgLabel.setFont(juce::Font(11.f));
dbgLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff8800));
dbgLabel.setText("DB: ...", juce::dontSendNotification);
addAndMakeVisible(dbgLabel);
```

In `resized()`, after the last existing layout line, add:
```cpp
// Debug label: top-right corner, small
dbgLabel.setBounds(getWidth() - 90, 4, 86, 16);
```

In `timerCallback()`, replace the existing body with:
```cpp
void CompositionPage::timerCallback()
{
    if (!pillsBuilt)
        buildFilterPills();

    if (previewTicksLeft_ > 0)
        if (--previewTicksLeft_ == 0) stopPreview();

    captureBtn.setButtonText(processor.isMidiCaptureActive()
                             ? "* STOP CAPTURE"
                             : "* MIDI CAPTURE");

    // Debug: update progression count label from live DB
    auto& th = processor.getTheoryEngine();
    if (th.isDatabaseReady())
    {
        auto all = th.getProgressionListings("");
        dbgLabel.setText("DB: " + juce::String((int)all.size()), juce::dontSendNotification);
    }
}
```

---

WHAT TO LOOK FOR AFTER BUILD + RUN

The orange "DB: N" label appears in the top-right corner of the Composition page.

Case A — Label shows "DB: 88":
  Seeding worked. The bug is in the chord lookup path. In this case, also add a DBG() line
  to selectProgression() (see CHANGE 3 below).

Case B — Label shows "DB: 0" or "DB: ...":
  Seeding failed. Check console output for the DBG() lines from CHANGE 1.

Case C — Label does not appear at all:
  The OLD binary is running (Logic Pro AU cache or wrong build deployed).

---

CHANGE 3 (ONLY if Case A above — label shows DB: 88)

File: CompositionPage.cpp, selectProgression()

After the `currentChordSequence.clear()` line, add:
```cpp
DBG("CompositionPage selectProgression: id=" << id
    << " cache_size=" << (int)cachedProgressions.size());
```

After the cache loop, add:
```cpp
DBG("CompositionPage selectProgression: after cache, seq_size=" << (int)currentChordSequence.size());
```

After the fallback block, add:
```cpp
DBG("CompositionPage selectProgression: final seq_size=" << (int)currentChordSequence.size());
```

---

VERIFY

1. Build: cmake --build build --config Release — zero errors, zero warnings
2. After build, confirm the Products/AU/VST3 timestamps are NEWER than before this build
3. Open Standalone (Howling Wolves 3.2.app from Products folder) — easiest to verify
4. Navigate to Composition page
5. Note what "DB: N" shows in the top-right corner
6. Click any progression card
7. Check console output (Xcode / stdout) for DBG() lines

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Confirmation of which Case (A, B, or C) was observed
- Screenshot or text showing the DB label value
- Full console output from the DBG() lines
- What selectProgression DBG lines showed (if Case A)
- Build: zero errors, zero warnings

CONSTRAINTS:
- The debug label and DBG() lines are TEMPORARY diagnostics — they will be removed in TASK_027
- Do not change APVTS parameters, seeding logic, or chord lookup logic
- Do not remove the TASK_024 defensive COUNT check
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE