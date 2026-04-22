# Progression Reseed Report (TASK_025)

## Summary

Replaced all 88 existing progression `ins()` calls in `TheoryEngine.cpp` with musically researched progressions (8 per genre × 11 genres), bumped `kProgSeedVersion` from 4 to 5, and updated the version stamp to force a clean reseed on next launch.

## Changes Made — `TheoryEngine.cpp` only

### CHANGE 1 — `kProgSeedVersion` 4 → 5

```cpp
// OLD
constexpr int kProgSeedVersion = 4;  // bumped: forces reseed after root_sequence migration

// NEW
constexpr int kProgSeedVersion = 5;  // bumped: replaces seed data with researched progressions
```

### CHANGE 2 — Replace all 88 `ins()` calls

Replaced lines 1122–1231 (110 lines, old mismatched/stub progressions) with exactly 88 new `ins()` calls — 8 per genre across 11 genres:

| Genre      | Count | Example                                          |
|------------|-------|--------------------------------------------------|
| Hip-Hop    | 8     | "Classic Boom Bap" → Am7-Dm7-G7-Cmaj7           |
| Trap       | 8     | "Dark Trap" → Am-F-G-Em                         |
| R&B        | 8     | "Neo Soul Classic" → Cmaj9-Am9-Fmaj9-G13        |
| Pop        | 8     | "Radio One" → C-G-Am-F                          |
| Jazz       | 8     | "ii-V-I Jazz" → Dm7-G7-Cmaj7-Cmaj7             |
| Lo-Fi      | 8     | "Rainy Afternoon" → Cmaj7-Am7-Fmaj7-G7         |
| Afrobeats  | 8     | "Lagos Nights" → Am-G-F-Em                     |
| EDM        | 8     | "Euphoric Drop" → Am-F-C-G                     |
| Gospel     | 8     | "Sunday Morning" → Cmaj7-Am7-Fmaj7-G7          |
| Cinematic  | 8     | "Hero Theme" → C-G-Am-F                        |
| Blues      | 8     | "Blues in C" → C7-F7-G7-C7                     |

All chord type IDs are within the valid range (1–29 used here, all defined in `chordDefs`). All root semitone offsets are 0–11.

### CHANGE 3 — Version stamp 4 → 5

```cpp
// OLD
"INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 4);"

// NEW
"INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 5);"
```

The defensive `COUNT(*)` guard from TASK_024 is left fully intact — version is only written if `newCount > 0`.

## What Happens on Next Launch

1. `ALTER TABLE` migration already added `root_sequence` column.
2. `stored_version` = 3, 4, or -1 from prior runs → all satisfy `< 5` → seed fires.
3. `DELETE FROM progressions` clears stale data.
4. `BEGIN TRANSACTION` → all 88 `ins()` calls succeed.
5. `COMMIT`.
6. `COUNT(*) = 88 > 0` → `version=5` written.
7. Next launch: `5 < 5 = false` → no reseed. ✓

## Verification

Three specific progressions to confirm chord name display:

| Progression      | Expected pad labels              |
|-----------------|----------------------------------|
| Classic Boom Bap | Am7 \| Dm7 \| G7 \| Cmaj7       |
| ii-V-I Jazz      | Dm7 \| G7 \| Cmaj7 \| Cmaj7    |
| Blues in C       | C7  \| F7  \| G7  \| C7        |

**Verify steps:**
1. Delete stale DB: `~/Library/Application Support/Wolf Productions/Wolf's Den/theory.db` (+ `.db-wal`, `.db-shm`)
2. Open plugin → Composition page loads without crash
3. Genre filter pills appear: Hip-Hop, Trap, R&B, Pop, Jazz, Lo-Fi, Afrobeats, EDM, Gospel, Cinematic, Blues
4. Browse grid shows 8 cards per genre (88 total on "All")
5. Click any progression card → audition pads show real chord names, not dashes
6. Click a populated pad → chord sounds via synth
7. Drag pad to composition slot → slot shows chord name
8. Genre filter (e.g. "Jazz") → 8 cards → click → pads populate

## Build Status

- **Zero errors, zero warnings**
- All targets: VST3 ✅ AU ✅ Standalone ✅
- `ins()` call count verified: 88 exactly
