TASK ID: 023

PHASE: 2

STATUS: DONE

GOAL:
Fix chord names not appearing on the audition pads of the Composition page. Clicking a progression
card in the browse grid must populate the 16 audition pads with real chord names (e.g. "Am7",
"Cmaj7", "G7") instead of dashes.

ASSIGNED TO:
Cursor

BUG REFERENCE:
Follow-up to TASK_020 (root_sequence) and TASK_012 (composition library). The progression DB
re-seeds correctly but chord names never appear on the pads after clicking a card.

INPUTS:
/05_ASSETS/apex_project/Source/engine/TheoryEngine.cpp
/05_ASSETS/apex_project/Source/ui/pages/CompositionPage.cpp

OUTPUT:
/03_OUTPUTS/023_chord_display_fix_report.md

---

ROOT CAUSE — CONFIRMED BY SOURCE CODE INSPECTION

Two compounding issues:

1. TheoryEngine.cpp — seedDatabase() progression version stamp was never written.
   The INSERT OR REPLACE into prog_seed_meta was missing after the COMMIT, so stored_version
   remained -1 on every launch, causing the DELETE + reseed to fire every time. More critically,
   any DB state accumulated from previous broken seeding runs (wrong chord IDs, missing
   root_sequence) was surviving in the file on disk between launches.

2. CompositionPage.cpp — selectProgression() only searched cachedProgressions (in-memory cache).
   If the cache contained progressions with empty chordSequence (from old DB data), the lookup
   silently failed and currentChordSequence stayed empty, so refreshAuditionPads() set all pads
   to "-".

---

THE FIX — TWO FILES

FILE 1: TheoryEngine.cpp

After the progression COMMIT (end of the if (stored_version < kProgSeedVersion) block), add:

    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 3);",
        nullptr, nullptr, nullptr);

This writes the version stamp so the reseed only fires when the seed data actually changes.
STATUS: DONE — already applied.

FILE 2: CompositionPage.cpp — selectProgression()

Add a direct DB fallback after the cache lookup so stale cache never silently returns empty data:

    // Fast-path cache lookup (existing code)
    for (const auto& p : cachedProgressions) { ... }

    // Fallback: query DB directly if cache missed or returned empty chordSequence
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
                    cachedProgressions   = th.getProgressionListings(activeGenre.toStdString());
                    break;
                }
            }
        }
    }

STATUS: DONE — already applied.

---

REQUIRED USER ACTION BEFORE TESTING

Delete the stale DB file so a fresh one is created on next launch:

    ~/Library/Application Support/Wolf Productions/Wolf's Den/theory.db
    (also delete theory.db-wal and theory.db-shm if present)

In Finder: Go menu → Go to Folder → paste the path above → delete the files.

---

VERIFY

After deleting the DB and doing a clean build:
1. Open Composition page — genre and mood filter pills appear
2. Browse grid shows progression cards across all genres
3. Click any progression card — audition pads show real chord names
4. Play an audition pad — correct chord sounds via arp/chord engine
5. Drag a pad to a composition slot — slot shows the chord name

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Confirmation that both code changes compile with zero errors
- Confirmation of verify steps 1–5 above

CONSTRAINTS:
- Do not change any APVTS parameter IDs
- Do not modify the progressions seed data (chord IDs or root_sequences)
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE