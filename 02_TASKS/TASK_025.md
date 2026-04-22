TASK ID: 025

PHASE: 2

STATUS: DONE

GOAL:
Replace ALL 88 existing progression rows seeded in TheoryEngine.cpp with 88 musically researched
progressions (8 per genre × 11 genres). Bump kProgSeedVersion from 4 to 5 and update the version
stamp write to 5 so the reseed fires on next launch regardless of DB state.

ASSIGNED TO:
Cursor

BUG REFERENCE:
Persistent follow-up to TASK_024. Chord names still do not appear on audition pads.
The version-stamp mechanism is now correct; this task replaces the seed data itself with
verified, musically accurate progressions that use correct chord_sequence and root_sequence values.

INPUTS:
/05_ASSETS/apex_project/Source/engine/TheoryEngine.cpp

OUTPUT:
/03_OUTPUTS/025_progression_reseed_report.md

---

CHORD TYPE ID REFERENCE (auto-assigned in seedDatabase(), do not change)

1=maj   2=m    3=dim   4=aug   5=sus2   6=sus4   7=pow
8=maj6  9=m6  10=dom7 11=maj7 12=m7   13=dim7  14=m7b5
15=mMaj7 16=augMaj7 17=dom7sus4 18=add9 19=madd9 20=dom9
21=maj9  22=m9  23=dom11 24=maj11 25=m11 26=dom13 27=maj13
28=m13  29=dom7b9 30=dom7#9 31=dom7b5 32=dom7#5 33=dom7#11
34=maj7#11 35=mMaj9 36=aug7 37=qrt 38=qnt 39=N
40=dom7b9b13 41=6/9 42=m6/9

ROOT SEMITONE OFFSETS (from C=0, used in root_sequence JSON arrays)
C=0  Db=1  D=2  Eb=3  E=4  F=5  F#=6  G=7  Ab=8  A=9  Bb=10  B=11

---

THE FIX — ONE FILE, THREE CHANGES

File: TheoryEngine.cpp

CHANGE 1 — Bump kProgSeedVersion from 4 to 5.

Line ~1086:
OLD:
    constexpr int kProgSeedVersion = 4;  // bumped: forces reseed after root_sequence migration

NEW:
    constexpr int kProgSeedVersion = 5;  // bumped: replaces seed data with researched progressions

CHANGE 2 — Replace ALL existing ins() calls (lines ~1122–1230) with the 88 calls below.
Remove every existing ins() call in the block between BEGIN TRANSACTION and COMMIT and replace
them with exactly the calls listed in the SEED DATA section below. Do not add, remove, or reorder
genres. Do not alter the ins() lambda definition or the surrounding transaction/COMMIT code.

CHANGE 3 — Update the version stamp INSERT from 4 to 5.

Lines ~1250–1252:
OLD:
    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 4);",
        nullptr, nullptr, nullptr);

NEW:
    sqlite3_exec(db,
        "INSERT OR REPLACE INTO prog_seed_meta (key, val) VALUES ('version', 5);",
        nullptr, nullptr, nullptr);

IMPORTANT: Leave the defensive COUNT check from TASK_024 fully intact. Only the literal '4'
inside the INSERT string changes to '5'.

---

SEED DATA — 88 ins() CALLS (replace lines ~1122–1230 entirely)

Each call: ins(name, genre, mood, energy, chord_sequence_json, root_sequence_json)
  - chord_sequence_json : JSON int array of chord type IDs (see reference above)
  - root_sequence_json  : JSON int array of semitone offsets from C=0 (see reference above)
  - energy              : 1 (low), 2 (mid), 3 (high)

// ---- Hip-Hop (8) ----
ins("Classic Boom Bap",    "Hip-Hop","Dark",       1,"[12,12,10,11]","[9,2,7,0]");  // Am7-Dm7-G7-Cmaj7
ins("Soul Sample",         "Hip-Hop","Smooth",     1,"[11,12,12,11]","[5,4,2,0]");  // Fmaj7-Em7-Dm7-Cmaj7
ins("East Coast Grit",     "Hip-Hop","Dark",       2,"[12,12,10,11]","[0,5,10,3]"); // Cm7-Fm7-Bb7-Ebmaj7
ins("West Coast Slide",    "Hip-Hop","Smooth",     2,"[12,12,10,11]","[7,0,5,10]"); // Gm7-Cm7-F7-Bbmaj7
ins("Boom Bap ii-V-I",     "Hip-Hop","Chill",      1,"[12,10,11,12]","[2,7,0,9]");  // Dm7-G7-Cmaj7-Am7
ins("Dusty Minor",         "Hip-Hop","Nostalgic",  1,"[12,10,11,12]","[9,7,5,4]");  // Am7-G7-Fmaj7-Em7
ins("Underground Groove",  "Hip-Hop","Dark",       2,"[11,12,12,11]","[8,7,5,3]");  // Abmaj7-Gm7-Fm7-Ebmaj7
ins("Chill Wave",          "Hip-Hop","Chill",      1,"[12,12,12,10]","[4,9,2,7]");  // Em7-Am7-Dm7-G7

// ---- Trap (8) ----
ins("Dark Trap",           "Trap","Dark",          3,"[2,1,1,1]",    "[0,10,8,7]"); // Cm-Bb-Ab-G
ins("Ominous Bells",       "Trap","Dark",          2,"[2,1,1,1]",    "[9,5,7,4]");  // Am-F-G-E
ins("Street Menace",       "Trap","Dark",          3,"[2,1,1,1]",    "[0,8,10,7]"); // Cm-Ab-Bb-G
ins("Night Rider",         "Trap","Dark",          2,"[2,1,1,1]",    "[4,0,2,11]"); // Em-C-D-B
ins("808 Slide",           "Trap","Dark",          3,"[2,1,1,1]",    "[11,7,9,6]"); // Bm-G-A-F#
ins("Cold World",          "Trap","Dark",          2,"[2,1,1,1]",    "[2,10,0,9]"); // Dm-Bb-C-A
ins("Trap Goth",           "Trap","Dark",          3,"[2,1,1,1]",    "[5,1,3,0]");  // Fm-Db-Eb-C
ins("Melodic Drill",       "Trap","Dark",          2,"[2,1,1,1]",    "[7,3,5,2]");  // Gm-Eb-F-D

// ---- R&B (8) ----
ins("Neo Soul Classic",    "R&B","Smooth",         2,"[21,22,21,26]","[0,9,5,7]");  // Cmaj9-Am9-Fmaj9-G13
ins("Silk Road",           "R&B","Smooth",         2,"[11,12,12,20]","[5,4,2,7]");  // Fmaj7-Em7-Dm7-G9
ins("Late Night RnB",      "R&B","Dark",           1,"[11,12,11,20]","[8,5,1,3]");  // Abmaj7-Fm7-Dbmaj7-Eb9
ins("Velvet Groove",       "R&B","Smooth",         2,"[11,12,11,20]","[7,4,0,2]");  // Gmaj7-Em7-Cmaj7-D9
ins("Soul Glow",           "R&B","Uplifting",      2,"[11,12,11,20]","[3,0,8,10]"); // Ebmaj7-Cm7-Abmaj7-Bb9
ins("Midnight Drive",      "R&B","Smooth",         1,"[11,12,11,20]","[10,7,3,5]"); // Bbmaj7-Gm7-Ebmaj7-F9
ins("Desire",              "R&B","Smooth",         2,"[22,21,21,22]","[2,7,0,9]");  // Dm9-Gmaj9-Cmaj9-Am9
ins("Golden Hour",         "R&B","Uplifting",      2,"[22,22,21,21]","[9,2,7,0]");  // Am9-Dm9-Gmaj9-Cmaj9

// ---- Pop (8) ----
ins("Radio One",           "Pop","Happy",          2,"[1,1,2,1]",    "[0,7,9,5]");  // C-G-Am-F
ins("Minor Pop",           "Pop","Happy",          2,"[2,1,1,1]",    "[9,5,0,7]");  // Am-F-C-G
ins("Classic Pop",         "Pop","Happy",          2,"[1,2,1,1]",    "[0,9,5,7]");  // C-Am-F-G
ins("Four Chord Pop",      "Pop","Happy",          2,"[1,1,1,1]",    "[0,5,7,5]");  // C-F-G-F
ins("Stadium Anthem",      "Pop","Uplifting",      3,"[1,1,2,1]",    "[0,5,9,7]");  // C-F-Am-G
ins("G-Pop Bounce",        "Pop","Happy",          2,"[1,1,2,1]",    "[7,2,4,0]");  // G-D-Em-C
ins("Indie Folk",          "Pop","Chill",          2,"[2,2,1,1]",    "[9,4,7,2]");  // Am-Em-G-D
ins("Summer Pop",          "Pop","Happy",          3,"[1,1,2,1]",    "[2,9,11,7]"); // D-A-Bm-G

// ---- Jazz (8) ----
ins("ii-V-I Jazz",         "Jazz","Smooth",        2,"[12,10,11,11]","[2,7,0,0]");  // Dm7-G7-Cmaj7-Cmaj7
ins("Rhythm Changes",      "Jazz","Uplifting",     2,"[11,12,12,10]","[0,9,2,7]");  // Cmaj7-Am7-Dm7-G7
ins("Minor ii-V-i",        "Jazz","Dark",          2,"[14,29,12,12]","[2,7,0,0]");  // Dm7b5-G7b9-Cm7-Cm7
ins("Jazz Ballad",         "Jazz","Smooth",        1,"[21,22,22,26]","[0,9,2,7]");  // Cmaj9-Am9-Dm9-G13
ins("Coltrane Cycle",      "Jazz","Intense",       2,"[11,11,11,11]","[0,3,8,11]"); // Cmaj7-Ebmaj7-Abmaj7-Bmaj7
ins("Bebop Groove",        "Jazz","Smooth",        2,"[22,26,21,22]","[2,7,0,9]");  // Dm9-G13-Cmaj9-Am9
ins("Gypsy Jazz",          "Jazz","Dark",          2,"[2,10,2,10]",  "[9,4,9,4]");  // Am-E7-Am-E7
ins("Modal Vamp",          "Jazz","Chill",         1,"[12,12,26,26]","[2,2,7,7]");  // Dm7-Dm7-G13-G13

// ---- Lo-Fi (8) ----
ins("Rainy Afternoon",     "Lo-Fi","Nostalgic",    1,"[11,12,11,10]","[0,9,5,7]");  // Cmaj7-Am7-Fmaj7-G7
ins("Dusty Records",       "Lo-Fi","Nostalgic",    1,"[11,12,12,12]","[5,2,9,4]");  // Fmaj7-Dm7-Am7-Em7
ins("Hazy Summer",         "Lo-Fi","Dreamy",       1,"[11,12,11,10]","[8,5,1,10]"); // Abmaj7-Fm7-Dbmaj7-Bb7
ins("Slow Motion",         "Lo-Fi","Chill",        1,"[11,12,11,10]","[7,4,0,2]");  // Gmaj7-Em7-Cmaj7-D7
ins("Tape Hiss",           "Lo-Fi","Dreamy",       1,"[11,12,11,12]","[10,7,3,0]"); // Bbmaj7-Gm7-Ebmaj7-Cm7
ins("Warm Vinyl",          "Lo-Fi","Nostalgic",    1,"[12,12,11,10]","[2,9,10,0]"); // Dm7-Am7-Bbmaj7-C7
ins("Saturday Morning",    "Lo-Fi","Chill",        1,"[12,11,11,10]","[9,5,0,7]");  // Am7-Fmaj7-Cmaj7-G7
ins("Late Study",          "Lo-Fi","Dreamy",       1,"[11,12,11,10]","[4,1,9,11]"); // Emaj7-C#m7-Amaj7-B7

// ---- Afrobeats (8) ----
ins("Lagos Nights",        "Afrobeats","Groovy",   3,"[2,1,1,2]",   "[9,7,5,4]");  // Am-G-F-Em
ins("Highlife Groove",     "Afrobeats","Groovy",   2,"[11,12,11,10]","[5,2,10,0]"); // Fmaj7-Dm7-Bbmaj7-C7
ins("Amapiano Flow",       "Afrobeats","Groovy",   3,"[11,12,12,10]","[1,10,3,8]"); // Dbmaj7-Bbm7-Ebm7-Ab7
ins("Afropop Summer",      "Afrobeats","Happy",    3,"[1,2,1,1]",   "[7,4,0,2]");  // G-Em-C-D
ins("Naija Groove",        "Afrobeats","Groovy",   2,"[11,12,11,10]","[3,0,8,10]"); // Ebmaj7-Cm7-Abmaj7-Bb7
ins("Afroswing",           "Afrobeats","Smooth",   2,"[12,12,11,12]","[4,9,2,11]"); // Em7-Am7-Dmaj7-Bm7
ins("West African Pull",   "Afrobeats","Groovy",   3,"[2,1,1,2]",   "[2,0,10,9]"); // Dm-C-Bb-Am
ins("Zanku Energy",        "Afrobeats","Groovy",   3,"[2,1,1,1]",   "[0,10,8,7]"); // Cm-Bb-Ab-G

// ---- EDM (8) ----
ins("Euphoric Drop",       "EDM","Uplifting",      3,"[2,1,1,1]",   "[9,5,0,7]");  // Am-F-C-G
ins("Trance Build",        "EDM","Uplifting",      3,"[2,2,1,1]",   "[9,4,5,7]");  // Am-Em-F-G
ins("House Classic",       "EDM","Groovy",         3,"[2,1,1,2]",   "[2,10,0,9]"); // Dm-Bb-C-Am
ins("Deep House",          "EDM","Dark",           2,"[2,2,1,1]",   "[5,0,8,3]");  // Fm-Cm-Ab-Eb
ins("Progressive Lift",    "EDM","Uplifting",      3,"[2,1,1,2]",   "[9,5,7,4]");  // Am-F-G-Em
ins("Melodic Drop",        "EDM","Dark",           3,"[2,1,1,1]",   "[0,8,3,10]"); // Cm-Ab-Eb-Bb
ins("Festival Anthem",     "EDM","Uplifting",      3,"[2,1,1,2]",   "[9,5,0,4]");  // Am-F-C-Em
ins("Tech House",          "EDM","Groovy",         2,"[2,1,1,2]",   "[9,7,5,4]");  // Am-G-F-Em

// ---- Gospel (8) ----
ins("Sunday Morning",      "Gospel","Uplifting",   2,"[11,12,11,10]","[0,9,5,7]"); // Cmaj7-Am7-Fmaj7-G7
ins("Praise Break",        "Gospel","Uplifting",   3,"[11,12,11,10]","[3,0,8,10]");// Ebmaj7-Cm7-Abmaj7-Bb7
ins("Worship Anthem",      "Gospel","Uplifting",   3,"[1,2,1,1]",   "[7,4,0,2]");  // G-Em-C-D
ins("Old Time Gospel",     "Gospel","Uplifting",   2,"[1,1,1,1]",   "[5,10,0,5]"); // F-Bb-C-F
ins("Contemporary Praise", "Gospel","Uplifting",   3,"[1,1,2,1]",   "[2,9,11,7]"); // D-A-Bm-G
ins("Choir Swell",         "Gospel","Uplifting",   2,"[11,12,11,20]","[10,7,3,5]");// Bbmaj7-Gm7-Ebmaj7-F9
ins("Spirit Move",         "Gospel","Uplifting",   3,"[1,2,1,1]",   "[0,9,5,7]");  // C-Am-F-G
ins("Joyful Noise",        "Gospel","Uplifting",   3,"[1,1,1,1]",   "[8,3,10,5]"); // Ab-Eb-Bb-F

// ---- Cinematic (8) ----
ins("Epic Rise",           "Cinematic","Intense",  3,"[2,1,1,1]",   "[9,5,0,7]");  // Am-F-C-G
ins("Dark Tension",        "Cinematic","Dark",     2,"[2,1,1,1]",   "[0,8,10,7]"); // Cm-Ab-Bb-G
ins("Hero Theme",          "Cinematic","Uplifting",3,"[1,1,2,1]",   "[0,7,9,5]");  // C-G-Am-F
ins("Emotional Score",     "Cinematic","Melancholic",2,"[21,22,21,20]","[0,9,5,7]");// Cmaj9-Am9-Fmaj9-G9
ins("Suspense",            "Cinematic","Dark",     2,"[14,29,12,12]","[2,7,0,0]"); // Dm7b5-G7b9-Cm7-Cm7
ins("Triumph",             "Cinematic","Uplifting",3,"[1,1,2,1]",   "[3,10,0,8]"); // Eb-Bb-Cm-Ab
ins("Melancholy",          "Cinematic","Melancholic",1,"[2,2,1,1]", "[9,4,5,7]");  // Am-Em-F-G
ins("Adventure",           "Cinematic","Uplifting",3,"[1,1,2,1]",   "[7,2,4,0]");  // G-D-Em-C

// ---- Blues (8) ----
ins("Blues in C",          "Blues","Gritty",       2,"[10,10,10,10]","[0,5,7,0]"); // C7-F7-G7-C7
ins("Minor Blues",         "Blues","Dark",         2,"[12,12,10,12]","[0,5,7,0]"); // Cm7-Fm7-G7-Cm7
ins("Texas Shuffle",       "Blues","Gritty",       3,"[10,10,10,10]","[9,2,4,9]"); // A7-D7-E7-A7
ins("Slow Blues",          "Blues","Melancholic",  1,"[10,10,10,10]","[4,9,11,4]");// E7-A7-B7-E7
ins("Jump Blues",          "Blues","Groovy",       3,"[10,10,10,10]","[5,10,0,5]");// F7-Bb7-C7-F7
ins("Jazz Blues",          "Blues","Smooth",       2,"[11,20,12,26]","[0,5,2,7]"); // Cmaj7-F9-Dm7-G13
ins("Blues Ballad",        "Blues","Melancholic",  1,"[12,12,10,11]","[9,2,7,0]"); // Am7-Dm7-G7-Cmaj7
ins("Chicago Blues",       "Blues","Gritty",       3,"[10,10,10,10]","[7,0,2,7]"); // G7-C7-D7-G7

---

WHAT THIS DOES ON NEXT LAUNCH

1. kProgSeedVersion = 5.
2. stored_version will be 3 or 4 (from prior runs) → 3 < 5 or 4 < 5 = TRUE → reseed fires.
3. DELETE FROM progressions → clean slate.
4. BEGIN TRANSACTION.
5. All 88 ins() calls succeed — root_sequence column exists (April 19 migration), chord IDs are
   valid (1–26 all defined in the chords table).
6. COMMIT.
7. newCount = 88 > 0 → version=5 written.
8. Next launch: stored_version=5, 5 < 5 = false → no reseed. ✓

Chord names appear because:
- chordSequence[i] is a valid chord type ID (1–26 in this set, all in chordDefs).
- rootSequence[i] is a valid semitone offset 0–11.
- chordName(rootPc, chordId) finds the chord in chordDefs and returns e.g. "Am7", "Cmaj7".

---

REQUIRED USER ACTION BEFORE TESTING

Delete the stale DB file so kProgSeedVersion=5 forces a clean reseed:

    ~/Library/Application Support/Wolf Productions/Wolf's Den/theory.db
    (also delete theory.db-wal and theory.db-shm if present)

In Finder: Go menu → Go to Folder → paste the path above → delete the files.

---

VERIFY

After deleting DB + clean build:
1. Open plugin → Composition page loads without crash.
2. Genre filter pills appear: Hip-Hop, Trap, R&B, Pop, Jazz, Lo-Fi, Afrobeats, EDM, Gospel,
   Cinematic, Blues.
3. Browse grid shows 8 cards per genre (88 total when "All" is selected).
4. Click any progression card → 4 audition pads (or more) show real chord names
   e.g. "Am7", "Cmaj7", "G7", "Fmaj7" — NOT dashes.
5. Click a populated pad → chord sounds via synth.
6. Drag a pad to a composition slot → slot shows the chord name.
7. Apply a genre filter (e.g. "Jazz") → grid shows 8 cards → click one → pads populate.
8. Verify a specific progression:
   "Classic Boom Bap" → pads should read: Am7 | Dm7 | G7 | Cmaj7
   "ii-V-I Jazz"      → pads should read: Dm7 | G7 | Cmaj7 | Cmaj7
   "Blues in C"       → pads should read: C7  | F7  | G7   | C7

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Diff showing kProgSeedVersion 4→5
- Diff showing all 88 new ins() calls replacing the old block
- Diff showing version stamp string 4→5
- Confirmation the build compiles with zero errors and zero warnings
- Confirmation of verify steps 1–8 above, including the three specific progressions in step 8

CONSTRAINTS:
- Only modify TheoryEngine.cpp (the three changes above)
- Do not change the ins() lambda definition
- Do not change the CREATE TABLE, ALTER TABLE migration, or COUNT check logic
- Do not change any APVTS parameter IDs
- Do not modify chord type definitions (the 42 chord INSERT calls before the progression block)
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
