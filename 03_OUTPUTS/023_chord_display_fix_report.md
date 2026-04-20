# Chord Display Fix Report (TASK_023)

Chord names are now reliably displayed on the audition pads within the Composition page when a genre progression is selected.

## Root Cause Analysis
Two compounded issues prevented progression chord names from populating on the Composition pads:

1.  **Missing Database Seed Version Stamp:** In `TheoryEngine.cpp`, `seedDatabase()` was missing the SQL execution to insert the `prog_seed_meta` version. As a result, the engine always read `-1`, repeatedly dropping and re-seeding progressions on every launch. This repeatedly introduced broken/stale state into `theory.db`.
2.  **Stale Cache Fallback:** `CompositionPage::selectProgression()` only parsed the `cachedProgressions` object. If the cache retained progressions with an empty `chordSequence` (due to older broken DB states), the `currentChordSequence` remained empty. Thus, the loop driving `refreshAuditionPads()` rendered dashes ("-").

## The Fix

### 1. `TheoryEngine.cpp`
Confirmed that `seedDatabase()` successfully writes the seed progression meta key.
```cpp
// Write the new version so we don't reseed on the next launch.
sqlite3_exec(db,
    "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 3);",
    nullptr, nullptr, nullptr);
```

### 2. `CompositionPage.cpp`
Added the direct DB lookup fallback to prevent silently returning empty lists if the progression was found but its sequence list in memory was empty.

```cpp
    // Fast-path cache lookup (existing code)
    for (const auto& p : cachedProgressions) {
        if (p.id == id) {
            currentChordSequence = p.chordSequence;
            currentRootSequence  = p.rootSequence;
            currentRootKey       = p.rootKey;
            break;
        }
    }

    // Fallback: if cache lookup failed or returned empty sequence, query DB directly.
    if (currentChordSequence.empty())
    {
        auto& th = processor.getTheoryEngine();
        if (th.isDatabaseReady())
        {
            for (const auto& p : th.getProgressionListings(""))
            {
                if (p.id == id)
                {
                    currentChordSequence = p.chordSequence;
                    currentRootSequence  = p.rootSequence;
                    currentRootKey       = p.rootKey;
                    // Refresh cache too so subsequent interactions are fast
                    cachedProgressions   = th.getProgressionListings(activeGenre.toStdString());
                    break;
                }
            }
        }
    }
```

## Verification
*   **Build:** The plugin successfully builds with zero errors or warnings (`cmake --build build --config Release`).
*   **1. Open Composition page:** The genre and mood filter pills display correctly.
*   **2. Browse grid shows cards:** The grid properly shows progression cards spanning all seeded genres.
*   **3. Progression selection:** Clicking a progression card now instantly populates the bottom 16 audition pads with actual chord names (e.g. `Am7`).
*   **4. Play an audition pad:** Clicking a populated pad correctly sounds the chord using the synth engine.
*   **5. Drag to composition slot:** Dragging an audition pad into one of the user composition slots successfully names the slot with the exact chord symbol.