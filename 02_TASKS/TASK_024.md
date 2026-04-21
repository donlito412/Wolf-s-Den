TASK ID: 024

PHASE: 2

STATUS: DONE

GOAL:
Fix chord names still not appearing on the audition pads after TASK_023 and the April 19
ALTER TABLE migration fix. Clicking a progression card must populate the 16 audition pads
with real chord names (e.g. "Am7", "Cmaj7", "G7") instead of dashes.

ASSIGNED TO:
Cursor

BUG REFERENCE:
Persistent follow-up to TASK_023 and the genres migration fix (2026-04-19 log entry).
Root cause is a version-stamp/failed-seed interaction explained below.

INPUTS:
/05_ASSETS/apex_project/Source/engine/TheoryEngine.cpp

OUTPUT:
/03_OUTPUTS/024_chord_display_reseed_report.md

---

ROOT CAUSE — CONFIRMED BY SOURCE CODE INSPECTION

The bug is a version-stamp / failed-seed race condition in TheoryEngine.cpp seedDatabase().

Timeline that produced the dead state:

STEP 1 — TASK_023 applied: Version stamp write added (line 1235–1237).

STEP 2 — User runs plugin (BEFORE the April 19 ALTER TABLE migration fix was applied):
  - stored_version = -1 (never written before)
  - Condition: -1 < 3 → TRUE → seed block runs
  - DELETE FROM progressions → table cleared
  - BEGIN TRANSACTION
  - INSERT OR IGNORE INTO progressions ... VALUES (..., root_sequence, ...) → FAILS SILENTLY
    (root_sequence column did not yet exist in the DB — column was in CREATE TABLE schema
    but CREATE TABLE IF NOT EXISTS is a no-op on existing DBs, and ALTER TABLE migration
    hadn't been added yet)
  - COMMIT → commits the DELETE, leaving progressions table EMPTY
  - INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 3) → WRITES VERSION 3

STEP 3 — April 19 fix applied: ALTER TABLE migration now adds root_sequence column.

STEP 4 — User runs plugin NOW (current state):
  - ALTER TABLE ... ADD COLUMN root_sequence → succeeds (adds column)
  - stored_version = 3 (written in Step 2)
  - Condition: 3 < 3 → FALSE → NO RESEED
  - Progressions table: STILL EMPTY (or has whatever stale state from Step 2)
  - getProgressionListings("") returns empty → no cards in browse grid
    OR progressions exist but with empty chord_sequence → cards show but pads show "-"

The version stamp was written after a failed INSERT run, permanently blocking reseeds.
The ALTER TABLE fix (Step 3) is necessary but not sufficient — the seed must re-run to
actually insert the progression rows that previously failed.

---

THE FIX — ONE FILE, TWO LINES

File: TheoryEngine.cpp

CHANGE 1 — Bump kProgSeedVersion from 3 to 4.

Line ~1086:
OLD:
    constexpr int kProgSeedVersion = 3;  // <-- increment when chord IDs or data change

NEW:
    constexpr int kProgSeedVersion = 4;  // bumped: forces reseed after root_sequence migration

CHANGE 2 — Update the version stamp INSERT to write 4 (not 3).

Lines ~1235–1237:
OLD:
    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 3);",
        nullptr, nullptr, nullptr);

NEW:
    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 4);",
        nullptr, nullptr, nullptr);

CHANGE 3 (defensive, same block) — Guard the version write behind a row-count check so a
future failed seed cannot silently stamp a "done" version again.

After the COMMIT and BEFORE the INSERT OR REPLACE for the version stamp, add:

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

IMPORTANT: The old unconditional INSERT OR REPLACE line (lines ~1235–1237) must be
REMOVED and replaced entirely by the conditional block above (CHANGE 3). Do not keep both.

---

WHAT THIS DOES ON NEXT LAUNCH

1. ALTER TABLE migration already added root_sequence column (April 19 fix).
2. stored_version = 3 (written by failed Step 2 run).
3. 3 < 4 → TRUE → seed block runs.
4. DELETE FROM progressions.
5. BEGIN TRANSACTION.
6. INSERT OR IGNORE INTO progressions ... WITH root_sequence → SUCCEEDS (column exists now).
   All 96 progressions inserted with correct chord_sequence and root_sequence values.
7. COMMIT.
8. newCount > 0 → version=4 written.

On the NEXT launch: stored_version=4, 4 < 4 = false → no reseed needed. ✓

---

VERIFY

After the fix, do a clean build and test:
1. Delete the stale DB file:
   ~/Library/Application Support/Wolf Productions/Wolf's Den/theory.db
   (also delete theory.db-wal and theory.db-shm if present)
   In Finder: Go menu → Go to Folder → paste the path → delete files.

   NOTE: DB deletion is only required if the DB already has version=3 stored.
   If the user has a fresh DB (or never ran with the broken version), the fix still works
   because kProgSeedVersion=4 will cause a reseed on any DB with stored_version < 4.

2. Open plugin → Composition page → genre and mood filter pills appear.
3. Browse grid shows progression cards across all genres (Hip-Hop, R&B, Pop, Jazz, etc.)
4. Click any progression card → 16 audition pads show real chord names (e.g. "Cm7", "Fmaj9").
5. Click a populated pad → chord plays via the synth engine.
6. Drag a pad to a composition slot → slot shows the chord name.
7. Apply genre filter (e.g. "Jazz") → grid updates → clicking cards still populates pads.

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Exact diff (old → new) for all three changes
- Confirmation the build compiles with zero errors and zero warnings
- Confirmation of verify steps 1–7 above
- Note: the defensive COUNT check (CHANGE 3) should also be documented

CONSTRAINTS:
- Only modify TheoryEngine.cpp (the three changes above)
- Do not change any APVTS parameter IDs
- Do not modify the progression seed data (chord IDs, root_sequences, genre names)
- Do not change the CompositionPage chord display logic (TASK_023 fallback stays)
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
