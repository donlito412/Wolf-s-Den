TASK ID: 020

PHASE: 2

STATUS: DONE

GOAL:
Fix the Composition page progression browser. Two confirmed bugs: (1) a data model error — every chord in every progression uses the same root note (C), making all audition pads sound wrong; (2) a possible UI bug preventing progression cards from appearing after a genre pill click. Also align the UX to match Scaler 3: genre pill → scrollable card grid of progressions → click card → audition pads fill with correctly-voiced chords → click pad to hear → drag/click pad to add to composition slots.

ASSIGNED TO:
Cursor

BUG REFERENCE:
BUG_P2_001 — Composition page genre selection produces no progressions and no sound.
Follow-up to TASK_012 (marked DONE but still broken in testing).

INPUTS:
/05_ASSETS/apex_project/Source/ui/pages/CompositionPage.cpp
/05_ASSETS/apex_project/Source/ui/pages/CompositionPage.h
/05_ASSETS/apex_project/Source/engine/TheoryEngine.cpp
/05_ASSETS/apex_project/Source/engine/TheoryEngine.h

OUTPUT:
/03_OUTPUTS/020_progression_fix_report.md

---

ROOT CAUSE 1 — DATA MODEL: No per-chord root note stored in progression
Confirmed by reading TheoryEngine.cpp (seedDatabase, line 1088–1212) and
CompositionPage.cpp (refreshAuditionPads lines 445–478, playAuditionPad lines 480–491).

The `progressions.chord_sequence` column stores ONLY chord type IDs (integers):
  "[12,10,11,12]" = [m7, dom7, maj7, m7]

It does NOT store the root note for each chord position. A Jazz "ii-V-I" requires:
  Chord 1: root = D  (semitone offset +2 from C), type = m7
  Chord 2: root = G  (semitone offset +7 from C), type = dom7
  Chord 3: root = C  (semitone offset +0 from C), type = maj7

Without per-chord root offsets, refreshAuditionPads() at line 465 uses `currentRootKey`
(always 0 = C) for ALL chords:
```cpp
int rootPc = currentRootKey;  // THIS IS ALWAYS 0 (C) FOR EVERY CHORD
p.setButtonText(chordName(rootPc, chordId));
```

And playAuditionPad() at line 490 plays every chord from the same root:
```cpp
startPreview(48 + currentRootKey, iv);  // 48+0=C3 for ALL chords
```

Result: every audition pad plays a chord rooted on C. "ii-V-I" sounds like "Cm7, C7, Cmaj7, Cm7" —
all same root, completely wrong voicings, and all sound nearly identical.

---

ROOT CAUSE 2 — UI: Cards may not appear after genre click
Confirmed by reading CompositionPage.cpp, buildFilterPills() (lines 284–323) and
rebuildBrowseGrid() (lines 382–409).

In buildFilterPills() the call order is:
```cpp
rebuildFilteredIndices();
rebuildBrowseGrid();    // called BEFORE resized() — viewport may have zero width here
resized();              // sets viewport bounds AFTER the grid rebuild
```

Inside rebuildBrowseGrid():
```cpp
const int vw = juce::jmax(1, gridViewport.getViewWidth());  // may return 0 on first call
const int cols = juce::jmax(1, vw / 200);
const int cw = (vw - 8 * (cols + 1)) / cols;               // if vw=1: cw=(1-16)/1 = -15
```

Cards built with cw=-15 are invisible (zero or negative render width). The subsequent
resized() call fixes the viewport bounds and calls rebuildBrowseGrid() again with
cachedProgressions = ALL progressions. But when the user then clicks "Hip-Hop":
applyGenreFilter replaces cachedProgressions with genre-filtered results, then calls
rebuildBrowseGrid() — which at that point should have a valid viewport width.

If cards still don't appear after genre click, the most likely remaining cause is that
applyGenreFilter (line 332–338) early-exits because th.isDatabaseReady() returns false
at that moment, leaving cachedProgressions unchanged (still the all-genre set), while
filteredIndices correctly filters but the grid render has a layout issue.

ADD DIAGNOSTIC FIRST:
Before fixing, add a juce::Logger::outputDebugString() call at the top of rebuildBrowseGrid()
to log filteredIndices.size() and gridViewport.getViewWidth(). Also add one inside
applyGenreFilter after the DB call to log cachedProgressions.size(). Build and test with
these logs to confirm which path is failing, then remove the logs after fixing.

---

FIX 1 — Add per-chord root offsets to the progressions table

Step 1a: In TheoryEngine.cpp, createSchema() (line 695), the progressions table SQL
(starting at line 765). Add a `root_sequence` column:

OLD progressions CREATE TABLE (lines 765–773):
```sql
CREATE TABLE IF NOT EXISTS progressions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    genre           TEXT NOT NULL,
    mood            TEXT,
    energy          INTEGER DEFAULT 2,
    root_key        INTEGER DEFAULT 0,
    chord_sequence  TEXT NOT NULL,
    created_at      TEXT DEFAULT CURRENT_TIMESTAMP
);
```

NEW progressions CREATE TABLE — add root_sequence:
```sql
CREATE TABLE IF NOT EXISTS progressions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    genre           TEXT NOT NULL,
    mood            TEXT,
    energy          INTEGER DEFAULT 2,
    root_key        INTEGER DEFAULT 0,
    chord_sequence  TEXT NOT NULL,
    root_sequence   TEXT NOT NULL DEFAULT '[0,0,0,0]',
    created_at      TEXT DEFAULT CURRENT_TIMESTAMP
);
```

Note: `root_sequence` is a JSON array of semitone offsets (0–11) from the progression's
root key, one per chord. E.g., for Jazz "ii-V-I" in C: "[2,7,0,2]" (D=+2, G=+7, C=+0, D=+2).

Step 1b: In TheoryEngine.cpp, seedDatabase(), the helper lambda `ins` (line 1092–1098).
Replace with a version that takes a root_sequence argument:

OLD lambda:
```cpp
auto ins = [&](const char* name, const char* genre, const char* mood, int energy, const char* seq)
{
    std::string sql =
        std::string("INSERT OR IGNORE INTO progressions (name,genre,mood,energy,root_key,chord_sequence) VALUES ('")
        + name + "','" + genre + "','" + mood + "'," + std::to_string(energy) + ",0,'" + seq + "');";
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
};
```

NEW lambda:
```cpp
auto ins = [&](const char* name, const char* genre, const char* mood, int energy,
               const char* seq, const char* roots)
{
    std::string sql =
        std::string("INSERT OR IGNORE INTO progressions (name,genre,mood,energy,root_key,chord_sequence,root_sequence) VALUES ('")
        + name + "','" + genre + "','" + mood + "',"
        + std::to_string(energy) + ",0,'" + seq + "','" + roots + "');";
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
};
```

Step 1c: Update ALL `ins()` calls (lines 1102–1209) to pass root offsets as a second JSON
string argument. The semitone offsets are relative to the progression's root key (0=C).

COMPLETE UPDATED SEEDING — copy these exactly:

// ---- Hip-Hop ----
ins("Trap God",       "Hip-Hop","Dark",       3,"[2,12,3,2]",    "[0,1,0,0]");   // i-bII-dim-i
ins("Soul Sample",    "Hip-Hop","Melancholic",2,"[12,22,14,10]", "[0,0,5,7]");   // im7-IVm9-iim7b5-V7
ins("Dilla Bounce",   "Hip-Hop","Warm",       2,"[11,12,10,22]", "[0,5,7,9]");   // Imaj7-IVm7-V7-VIm9
ins("Night Drive",    "Hip-Hop","Dark",       2,"[2,2,10,2]",    "[0,0,7,0]");   // i-i-V7-i
ins("Golden Era",     "Hip-Hop","Nostalgic",  2,"[1,12,10,1]",   "[0,9,7,0]");   // I-vim7-V7-I
ins("Melancholy",     "Hip-Hop","Sad",        1,"[12,14,10,2]",  "[0,5,7,0]");   // im7-ivm7b5-V7-i
ins("West Coast",     "Hip-Hop","Smooth",     2,"[2,12,2,12]",   "[0,5,0,5]");   // i-ivm7-i-ivm7
ins("Brooklyn",       "Hip-Hop","Tense",      3,"[2,3,12,10]",   "[0,3,5,7]");   // i-bIIIdim-ivm7-V7

// ---- R&B / Neo-Soul ----
ins("Slow Burn",      "R&B","Sensual",   1,"[21,22,22,29]", "[0,9,2,7]");  // Imaj9-VIm9-iim9-V7b9
ins("Butterfly",      "R&B","Romantic",  2,"[11,12,10,11]", "[0,9,2,7]");  // Imaj7-VIm7-iim7-V7
ins("Neo Groove",     "R&B","Uplifting", 2,"[21,20,22,10]", "[0,7,2,7]");  // Imaj9-V9-iim9-V7
ins("After Midnight", "R&B","Dark",      1,"[22,14,10,22]", "[0,2,7,0]");  // Im9-iim7b5-V7-Im9
ins("Summer Love",    "R&B","Warm",      2,"[41,22,10,42]", "[0,2,7,0]");  // I6/9-iim9-V7-Im6/9
ins("Old School",     "R&B","Nostalgic", 2,"[8,9,10,11]",   "[0,9,7,0]");  // Imaj6-VIm6-V7-Imaj7
ins("Smooth Groove",  "R&B","Smooth",    1,"[11,22,10,11]", "[0,2,7,0]");  // Imaj7-iim9-V7-Imaj7
ins("Pain",           "R&B","Sad",       1,"[12,14,10,2]",  "[0,5,7,0]");  // im7-ivm7b5-V7-i

// ---- Pop ----
ins("Radio Hit",      "Pop","Uplifting", 3,"[1,5,2,1]",     "[0,5,9,0]");  // I-IV-vi-I
ins("Anthem",         "Pop","Anthemic",  3,"[1,5,1,5]",     "[0,5,0,5]");  // I-IV-I-IV
ins("Dancefloor",     "Pop","Energetic", 3,"[1,2,5,1]",     "[0,9,5,0]");  // I-vi-IV-I
ins("Comeback",       "Pop","Emotional", 2,"[1,4,5,2]",     "[0,7,5,9]");  // I-V-IV-vi
ins("Sad Pop",        "Pop","Sad",       2,"[2,5,1,4]",     "[9,5,0,7]");  // vi-IV-I-V
ins("Dreamy",         "Pop","Dreamy",    1,"[11,22,10,11]", "[0,2,7,0]");  // Imaj7-iim9-V7-Imaj7
ins("Chill Vibes",    "Pop","Chill",     1,"[1,5,2,5]",     "[0,5,9,7]");  // I-IV-vi-V
ins("Happy Day",      "Pop","Happy",     3,"[1,4,5,4]",     "[0,7,5,7]");  // I-V-IV-V

// ---- Jazz ----
ins("ii-V-I",         "Jazz","Smooth",     2,"[12,10,11,12]", "[2,7,0,2]"); // Dm7-G7-Cmaj7-Dm7
ins("Tritone Sub",    "Jazz","Tense",      2,"[12,29,11,12]", "[2,6,0,2]"); // Dm7-Db7b9-Cmaj7-Dm7
ins("Autumn Leaves",  "Jazz","Melancholic",2,"[12,10,11,22]", "[2,7,0,9]"); // Dm7-G7-Cmaj7-Am9
ins("Giant Steps",    "Jazz","Complex",    3,"[11,10,11,10]", "[0,7,4,11]");// Cmaj7-G7-Emaj7-B7
ins("Coltrane Turnaround","Jazz","Tense",  3,"[11,10,11,10]", "[0,4,8,11]");// Cmaj7-E7-Abmaj7-B7
ins("Blue Bossa",     "Jazz","Latin",      2,"[12,10,11,22]", "[2,7,0,9]"); // Dm7-G7-Cmaj7-Am9
ins("So What",        "Jazz","Cool",       1,"[12,12,12,12]", "[2,2,2,2]"); // Dm7 modal vamp
ins("Rhythm Changes", "Jazz","Bebop",      3,"[1,2,10,1]",    "[0,9,7,0]"); // I-vi-V7-I

// ---- Lo-Fi ----
ins("Rainy Day",      "Lo-Fi","Melancholic",1,"[12,22,29,12]", "[0,2,7,0]");
ins("Bedroom Tape",   "Lo-Fi","Nostalgic",  1,"[11,22,10,11]", "[0,2,7,0]");
ins("Sunday Morning", "Lo-Fi","Calm",       1,"[21,22,11,10]", "[0,2,5,7]");
ins("Dusty Crate",    "Lo-Fi","Warm",       1,"[11,9,10,12]",  "[0,9,7,2]");
ins("Late Night",     "Lo-Fi","Dark",       1,"[12,14,10,2]",  "[0,5,7,0]");
ins("Coffee Shop",    "Lo-Fi","Calm",       1,"[21,11,10,22]", "[0,5,7,2]");
ins("Vintage Loop",   "Lo-Fi","Nostalgic",  1,"[41,22,10,42]", "[0,2,7,0]");
ins("Crinkle",        "Lo-Fi","Dreamy",     1,"[11,12,12,10]", "[0,5,2,7]");

// ---- Afrobeats ----
ins("Afro High Life", "Afrobeats","Uplifting", 3,"[1,4,5,1]", "[0,5,7,0]");
ins("Lagos Nights",   "Afrobeats","Energetic", 3,"[1,2,5,4]", "[0,9,7,5]");
ins("Makossa",        "Afrobeats","Happy",     3,"[1,5,4,1]", "[0,7,5,0]");
ins("Afropop",        "Afrobeats","Euphoric",  3,"[1,1,4,5]", "[0,0,5,7]");
ins("Highlife Gold",  "Afrobeats","Warm",      2,"[1,4,1,5]", "[0,5,0,7]");
ins("Juju",           "Afrobeats","Spiritual", 2,"[1,2,4,1]", "[0,9,5,0]");
ins("Fuji Funk",      "Afrobeats","Energetic", 3,"[4,1,5,4]", "[5,0,7,5]");
ins("Naija",          "Afrobeats","Uplifting", 3,"[1,5,4,2]", "[0,7,5,9]");

// ---- Trap ----
ins("Dark Flex",      "Trap","Dark",       3,"[2,1,2,3]",    "[0,1,0,3]");
ins("Ominous",        "Trap","Tense",      3,"[2,3,2,10]",   "[0,3,0,7]");
ins("Paranoid",       "Trap","Aggressive", 3,"[2,14,3,2]",   "[0,2,5,0]");
ins("808 Roll",       "Trap","Dark",       3,"[2,12,14,2]",  "[0,5,2,0]");
ins("Minor Threat",   "Trap","Tense",      3,"[2,2,3,10]",   "[0,0,3,7]");
ins("Street Hymn",    "Trap","Melancholic",2,"[12,14,10,2]", "[0,5,7,0]");
ins("Icey",           "Trap","Cold",       2,"[2,12,3,12]",  "[0,5,3,2]");
ins("South Side",     "Trap","Dark",       3,"[2,10,3,2]",   "[0,7,3,0]");

// ---- EDM / Dance ----
ins("Euphoria",       "EDM","Euphoric",  3,"[1,2,4,5]", "[0,9,5,7]");
ins("Festival",       "EDM","Energetic", 3,"[1,5,4,5]", "[0,7,5,7]");
ins("Drop",           "EDM","Aggressive",3,"[1,2,5,1]", "[0,9,5,0]");
ins("Build Up",       "EDM","Tense",     3,"[2,4,5,1]", "[9,5,7,0]");
ins("Sunrise",        "EDM","Uplifting", 3,"[1,4,5,2]", "[0,7,5,9]");
ins("Trance Gate",    "EDM","Euphoric",  3,"[2,4,5,2]", "[9,5,7,9]");
ins("Deep House",     "EDM","Smooth",    2,"[12,11,22,10]","[0,7,2,7]");
ins("Progressive",    "EDM","Energetic", 3,"[2,5,4,1]", "[9,7,5,0]");

// ---- Gospel ----
ins("Church Stomp",   "Gospel","Spiritual",  3,"[1,4,1,5]",    "[0,5,0,7]");
ins("Hallelujah",     "Gospel","Uplifting",  3,"[1,4,5,1]",    "[0,5,7,0]");
ins("Holy Ghost",     "Gospel","Euphoric",   3,"[1,2,5,4]",    "[0,9,7,5]");
ins("Testimony",      "Gospel","Emotional",  2,"[11,10,12,11]","[0,7,2,0]");
ins("Sunday Service", "Gospel","Warm",       2,"[1,5,4,1]",    "[0,7,5,0]");
ins("Revival",        "Gospel","Energetic",  3,"[1,4,5,2]",    "[0,5,7,9]");
ins("Amen Break",     "Gospel","Spiritual",  3,"[4,2,5,1]",    "[5,9,7,0]");
ins("Walk By Faith",  "Gospel","Uplifting",  2,"[1,2,4,5]",    "[0,9,5,7]");

// ---- Cinematic / Orchestral ----
ins("Heroic Theme",   "Cinematic","Epic",        3,"[1,6,4,5]",    "[0,5,5,7]");
ins("Sad Reprise",    "Cinematic","Sad",          1,"[2,5,1,4]",    "[9,5,0,7]");
ins("Tension Build",  "Cinematic","Tense",        3,"[2,3,14,10]",  "[0,3,2,7]");
ins("Resolution",     "Cinematic","Uplifting",    2,"[2,4,5,1]",    "[9,5,7,0]");
ins("Flashback",      "Cinematic","Nostalgic",    1,"[11,12,10,11]","[0,2,7,0]");
ins("Dark Matter",    "Cinematic","Dark",         2,"[2,3,12,14]",  "[0,3,5,2]");
ins("Victory",        "Cinematic","Epic",         3,"[1,4,5,4]",    "[0,7,5,7]");
ins("Elegy",          "Cinematic","Melancholic",  1,"[12,14,10,2]", "[0,5,7,0]");

// ---- Blues ----
ins("12-Bar Shuffle", "Blues","Gritty",     2,"[10,10,10,10]","[0,5,0,7]"); // I7-IV7-I7-V7
ins("Texas Stomp",    "Blues","Energetic",  3,"[10,5,10,10]", "[0,5,0,7]");
ins("Delta Blues",    "Blues","Melancholic",2,"[10,10,13,10]","[0,5,3,7]");
ins("Slow Blues",     "Blues","Sad",        1,"[10,10,10,14]","[0,5,0,7]");
ins("Jump Blues",     "Blues","Happy",      3,"[10,5,10,5]",  "[0,5,0,7]");
ins("Boogie Woogie",  "Blues","Energetic",  3,"[10,10,5,10]", "[0,5,5,7]");
ins("Minor Blues",    "Blues","Dark",       2,"[12,12,14,10]","[0,5,2,7]");
ins("Chicago",        "Blues","Smooth",     2,"[10,12,10,5]", "[0,9,7,5]");

// ---- Latin ----
ins("Montuno",        "Latin","Energetic",  3,"[2,4,5,2]",    "[0,5,7,0]");
ins("Bossa Nova",     "Latin","Cool",       1,"[11,14,10,11]","[0,2,7,0]");
ins("Salsa",          "Latin","Uplifting",  3,"[1,4,5,1]",    "[0,5,7,0]");
ins("Flamenco Minor", "Latin","Tense",      2,"[2,1,10,2]",   "[0,1,7,0]");
ins("Reggaeton",      "Latin","Energetic",  3,"[1,2,5,4]",    "[0,9,7,5]");
ins("Cumbia",         "Latin","Happy",      3,"[1,4,5,4]",    "[0,5,7,5]");
ins("Tango",          "Latin","Tense",      2,"[2,10,14,2]",  "[0,7,2,0]");
ins("Samba",          "Latin","Energetic",  3,"[1,5,4,5]",    "[0,7,5,7]");

Step 1d: In TheoryEngine.cpp, at the top of the progression seeding block (around line 1088),
change the prog_count guard: after seeding, delete all progressions and re-seed IF the
existing rows lack a root_sequence (to upgrade old DBs that TASK_012 left without roots):

```cpp
// Check if existing progressions have root_sequence populated
// (handles upgrade from TASK_012 which seeded without root_sequence)
int has_roots = 0;
sqlite3_stmt* rchk = nullptr;
if (sqlite3_prepare_v2(db,
    "SELECT COUNT(*) FROM progressions WHERE root_sequence IS NOT NULL AND root_sequence != '[0,0,0,0]';",
    -1, &rchk, nullptr) == SQLITE_OK)
{
    if (sqlite3_step(rchk) == SQLITE_ROW)
        has_roots = sqlite3_column_int(rchk, 0);
    sqlite3_finalize(rchk);
}

if (prog_count == 0 || has_roots == 0)
{
    // Delete and re-seed with correct root_sequence data
    sqlite3_exec(db, "DELETE FROM progressions;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // ... all ins() calls here ...

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}
```

---

FIX 2 — Update ProgressionListing struct and getProgressionListings()

File: TheoryEngine.h, struct ProgressionListing (line 70–79)

OLD:
```cpp
struct ProgressionListing
{
    int id = 0;
    std::string name;
    std::string genre;
    std::string mood;
    int energy = 2;
    int rootKey = 0;
    std::vector<int> chordSequence;
};
```

NEW:
```cpp
struct ProgressionListing
{
    int id = 0;
    std::string name;
    std::string genre;
    std::string mood;
    int energy = 2;
    int rootKey = 0;
    std::vector<int> chordSequence;  // chord_type_id per position
    std::vector<int> rootSequence;   // semitone offset from rootKey per position (0–11)
};
```

File: TheoryEngine.cpp, getProgressionListings() (lines 1334–1366)

OLD SQL:
```cpp
std::string sql = "SELECT id, name, genre, IFNULL(mood,''), energy, root_key, chord_sequence "
                  "FROM progressions";
```

NEW SQL — also select root_sequence:
```cpp
std::string sql = "SELECT id, name, genre, IFNULL(mood,''), energy, root_key, "
                  "chord_sequence, IFNULL(root_sequence,'[0,0,0,0]') "
                  "FROM progressions";
```

After the while loop body, add rootSequence parsing (after line 1361):
```cpp
p.chordSequence = parseIntervalJson(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
p.rootSequence  = parseIntervalJson(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
// Ensure rootSequence has same length as chordSequence (pad with zeros if old data)
while ((int)p.rootSequence.size() < (int)p.chordSequence.size())
    p.rootSequence.push_back(0);
out.push_back(std::move(p));
```

---

FIX 3 — Use per-chord root in refreshAuditionPads() and playAuditionPad()

File: CompositionPage.h — add rootSequence to the class:

In the member variables section, add alongside currentChordSequence:
```cpp
std::vector<int> currentChordSequence;
std::vector<int> currentRootSequence;  // ADD THIS
int currentRootKey = 0;
```

File: CompositionPage.cpp, selectProgression() (lines 411–427)

OLD:
```cpp
currentChordSequence.clear();
for (const auto& p : cachedProgressions) {
    if (p.id == id) {
        currentChordSequence = p.chordSequence;
        currentRootKey = p.rootKey;
        break;
    }
}
```

NEW:
```cpp
currentChordSequence.clear();
currentRootSequence.clear();
for (const auto& p : cachedProgressions) {
    if (p.id == id) {
        currentChordSequence = p.chordSequence;
        currentRootSequence  = p.rootSequence;
        currentRootKey       = p.rootKey;
        break;
    }
}
```

File: CompositionPage.cpp, refreshAuditionPads() (line 465)

OLD:
```cpp
int rootPc = currentRootKey;
p.setButtonText(chordName(rootPc, chordId));
```

NEW:
```cpp
// Use per-chord root offset from rootSequence, fall back to progression root if missing
int chordRootOffset = (i < (int)currentRootSequence.size()) ? currentRootSequence[(size_t)i] : 0;
int rootPc = (currentRootKey + chordRootOffset) % 12;
p.setButtonText(chordName(rootPc, chordId));
```

File: CompositionPage.cpp, playAuditionPad() (lines 480–491)

OLD:
```cpp
startPreview(48 + currentRootKey, iv);
```

NEW:
```cpp
int chordRootOffset = (idx < (int)currentRootSequence.size()) ? currentRootSequence[(size_t)idx] : 0;
int midiRoot = juce::jlimit(24, 96, 48 + currentRootKey + chordRootOffset);
startPreview(midiRoot, iv);
```

File: CompositionPage.cpp, addAuditionPadToSlot() (line 501) — update SlotData to carry per-chord root:

If SlotData struct has `{ chordId, rootPc }`, update the line:
```cpp
slotData[(size_t)i] = { chordId, (currentRootKey + chordRootOffset) % 12 };
```
replacing the old:
```cpp
slotData[(size_t)i] = { chordId, currentRootKey };
```

---

FIX 4 — Fix call order in buildFilterPills() to prevent invisible-card bug

File: CompositionPage.cpp, buildFilterPills() (lines 284–323)

CURRENT ORDER (lines 319–322):
```cpp
pillsBuilt = true;
rebuildFilteredIndices();
rebuildBrowseGrid();   // WRONG: called before viewport has correct size
resized();
```

CORRECT ORDER:
```cpp
pillsBuilt = true;
resized();             // lay out viewport bounds FIRST
rebuildFilteredIndices();
rebuildBrowseGrid();   // NOW viewport.getViewWidth() is valid
```

Also: remove the redundant `rebuildBrowseGrid()` call from inside resized() to prevent double-rebuild:
In resized() (around line 888–889), remove the standalone `rebuildBrowseGrid();` call that appears
after `gridViewport.setBounds(...)`. The grid will be rebuilt explicitly by buildFilterPills() and
applyGenreFilter() which are the only callers that change cachedProgressions.

---

DELIVERABLES INSIDE OUTPUT REPORT:
- Diff of TheoryEngine.h ProgressionListing struct
- Diff of TheoryEngine.cpp getProgressionListings() SQL + parsing
- Diff of CompositionPage.cpp: refreshAuditionPads, playAuditionPad, addAuditionPadToSlot,
  selectProgression, buildFilterPills, resized
- Confirmation: select "Jazz" genre → ii-V-I progression appears as a card
- Confirmation: click "ii-V-I" card → audition pads show Dm7, G7, Cmaj7, Dm7 (different roots)
- Confirmation: click each audition pad → correct chord sounds (Dm7 rooted on D, G7 on G)
- Confirmation: drag audition pad to composition slot → slot shows correct chord name
- Build: cmake --build build --config Release — zero errors, zero new warnings

CONSTRAINTS:
- Do not change APVTS parameter IDs
- Do not break existing chord_sets functionality
- root_sequence column must use DEFAULT '[0,0,0,0]' so old code reading old DB doesn't crash
- The re-seed guard must not delete progressions unless root_sequence is missing/all-zeros
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE
