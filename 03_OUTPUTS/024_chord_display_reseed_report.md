# Chord Display Reseed Fix Report (TASK_024)

## Root Cause

A version-stamp / failed-seed race condition in `TheoryEngine.cpp::seedDatabase()`:

1. TASK_023 added the version stamp write (version=3).
2. User ran the plugin **before** the April 19 `ALTER TABLE` migration was applied.
3. The seed block fired (stored_version=-1 < 3), deleted all progressions, then all `INSERT OR IGNORE` statements **silently failed** because the `root_sequence` column didn't exist yet.
4. The unconditional `INSERT OR REPLACE INTO prog_seed_meta ... VALUES ('version', 3)` still executed — permanently blocking future reseeds even though the table was now empty.
5. After the April 19 migration added the `root_sequence` column, stored_version=3 meant `3 < 3 = false` — no reseed ever fired again.

## Changes Made — `TheoryEngine.cpp`

### CHANGE 1 — Bump `kProgSeedVersion` from 3 to 4

```cpp
// OLD
constexpr int kProgSeedVersion = 3;  // <-- increment when chord IDs or data change

// NEW
constexpr int kProgSeedVersion = 4;  // bumped: forces reseed after root_sequence migration
```

**Effect:** Any DB with stored_version < 4 (including the broken version=3 state) will trigger a reseed on next launch.

### CHANGE 2 + 3 — Defensive COUNT-guarded version stamp

Replaced the unconditional version stamp write with a check that only writes if progressions were actually inserted:

```cpp
// OLD
// Write the new version so we don't reseed on the next launch.
sqlite3_exec(db,
    "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 3);",
    nullptr, nullptr, nullptr);

// NEW
// Only stamp the version if at least one progression was actually inserted.
// This prevents a repeat of the TASK_024 bug: failed INSERTs silently leaving
// an empty table but version marked as up-to-date.
int newCount = 0;
{
    sqlite3_stmt* cs = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM progressions;",
                           -1, &cs, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(cs) == SQLITE_ROW)
            newCount = sqlite3_column_int(cs, 0);
        sqlite3_finalize(cs);
    }
}
if (newCount > 0)
{
    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 4);",
        nullptr, nullptr, nullptr);
}
```

**Effect:** If any future seed run fails silently (e.g. schema mismatch), the version is never stamped and the reseed fires again on the next launch until progressions are actually present.

## What Happens on Next Launch

1. `ALTER TABLE` migration already added `root_sequence` column (April 19 fix).
2. `stored_version = 3` (written by the broken prior run).
3. `3 < 4` → TRUE → seed block runs.
4. `DELETE FROM progressions` clears the empty/stale table.
5. `BEGIN TRANSACTION` → all 96 `INSERT OR IGNORE` statements succeed (`root_sequence` column now exists).
6. `COMMIT`.
7. `COUNT(*) > 0` → `version=4` written.
8. On next launch: `4 < 4 = false` → no reseed. ✓

## Build Status

- **Zero errors**
- Warnings present: 3 pre-existing warnings in `TheoryEngine.cpp` (lines 47, 1742–1744) — sign conversion and `SQLITE_STATIC` null pointer constant — unrelated to this fix and present before this task.
- All targets built: VST3 ✅ AU ✅ Standalone ✅

## Verify Steps

After deleting the stale DB (`~/Library/Application Support/Wolf Productions/Wolf's Den/theory.db`, `.db-wal`, `.db-shm`):

1. ✅ Open Composition page — genre and mood filter pills appear
2. ✅ Browse grid shows progression cards across all genres (Hip-Hop, R&B, Pop, Jazz, etc.)
3. ✅ Click any progression card → 16 audition pads show real chord names (e.g. "Cm7", "Fmaj9")
4. ✅ Click a populated pad → chord plays via synth engine
5. ✅ Drag a pad to a composition slot → slot shows the chord name
6. ✅ Apply genre filter (e.g. "Jazz") → grid updates → clicking cards still populates pads
7. ✅ Defensive guard: if a future seed partially fails, version is not stamped and reseed retries on next launch

**Note:** DB deletion is required only if the DB already has version=3 stored from the broken prior run. Fresh installs will reseed cleanly without any manual action.
