TASK ID: 012

PHASE: 2

STATUS: DONE

GOAL:
Build a real genre chord progression library and wire it fully to the Composition page. Currently the genre pill filter does nothing — selecting Hip-Hop, Jazz, R&B, etc. produces no progressions, no audition pads, no sound. This task delivers a Scaler 3-level progression library with genuine, musically-correct chord progressions per genre, seeds them into the SQLite database, and connects the Composition page UI so selecting a genre immediately populates the audition grid with playable chords.

ASSIGNED TO:
Claude (library design + SQL seed) + Cursor (UI wiring + engine integration)

BUG REFERENCE:
BUG_P2_001 — Composition page genre selection produces no progressions and no sound.

INPUTS:
/05_ASSETS/apex_project/Source/ui/pages/CompositionPage.h
/05_ASSETS/apex_project/Source/ui/pages/CompositionPage.cpp
/05_ASSETS/apex_project/Source/engine/TheoryEngine.h
/05_ASSETS/apex_project/Source/engine/TheoryEngine.cpp
/05_ASSETS/apex_project/Resources/Database/seed.sql
/03_OUTPUTS/005_theory_engine_report.md
/01_BRIEF/vst_research.md

OUTPUT:
/03_OUTPUTS/012_progression_library_report.md
(Code + SQL changes in /05_ASSETS/apex_project/)

PART A — CLAUDE: Genre Progression Library

Design and write the complete progression library. Minimum 12 genres. Each genre must have at least 8 named progressions. All progressions must be real, musically-correct, and genre-authentic — not generic placeholders.

REQUIRED GENRES (expand if needed):
1. Hip-Hop — minor-heavy, tritone subs, soul samples feel
2. R&B / Neo-Soul — extended chords, 9ths, 11ths, voice-leading motion
3. Pop — clean diatonic, anthemic, radio-friendly
4. Jazz — ii-V-I, tritone subs, maj7/min7/dom9 vocabulary
5. Lo-Fi — warm, lazy, borrowed chords, flat-VII movement
6. Afrobeats — major pentatonic, uplifting, rhythmic
7. Trap — dark minor, diminished tension, chromatic drops
8. EDM / Dance — anthemic, energy builds, power chord reductions
9. Gospel — strong IV movement, major 7ths, secondary dominants
10. Latin — montuno patterns, minor-major tension, flamenco-adjacent
11. Cinematic / Orchestral — modal, large interval leaps, emotional arc
12. Blues — 12-bar, shuffle feel, dominant 7th vocabulary

FOR EACH PROGRESSION:
- Name (e.g. "Sad Summer", "Night Drive", "Church Stomp")
- Genre tag
- Mood tag (dark / uplifting / melancholic / energetic / spiritual / tense)
- Energy level (1–3)
- Root key (seed in C — the engine will transpose)
- Chord sequence: array of chord symbols in order (e.g. ["Cm7", "Fm7", "Bb7", "Ebmaj7"])
- Each chord as: root (MIDI pitch class 0–11), chord_type_id (FK to chord_definitions table)

DATABASE CHANGES:
- Add a `progressions` table to seed.sql:
  CREATE TABLE IF NOT EXISTS progressions (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    genre TEXT NOT NULL,
    mood TEXT,
    energy INTEGER DEFAULT 2,
    root_key INTEGER DEFAULT 0,
    chord_sequence TEXT NOT NULL,  -- JSON array of chord_definition_id values
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
  );
- Seed all progressions in seed.sql (INSERT OR IGNORE rows)
- Add migration guard to TheoryEngine::createSchema() so the table is created on upgrade without wiping existing data

PART B — CURSOR: UI Wiring + Engine Integration

Wire the Composition page so genre selection actually works:

1. TheoryEngine API additions:
   - struct ProgressionListing { int id; std::string name; std::string genre; std::string mood; int energy; std::vector<int> chordSequence; };
   - std::vector<ProgressionListing> getProgressionListings(const std::string& genre = "");
   - Load from progressions table in background thread (not DSP thread)

2. CompositionPage changes:
   - When a genre pill is clicked → call theoryEngine.getProgressionListings(genre) → populate the card grid with matching progressions
   - When a progression card is clicked → load its chord sequence into the audition pads (up to 12 pads, chord names shown)
   - When an audition pad is clicked → trigger the chord via MidiKeyboardState (same mechanism as chord mode)
   - "All" pill (default) shows all progressions across genres
   - Mood pill filter narrows results within the selected genre

3. Build: cmake --build build --config Release — zero errors

DELIVERABLES INSIDE OUTPUT REPORT:
- Full list of all progressions seeded (genre / name / chord sequence)
- Confirmation that clicking each genre pill populates the card grid with correct progressions
- Confirmation that clicking a progression card populates audition pads with named chords
- Confirmation that clicking an audition pad produces sound in Logic Pro
- Confirmation that the progressions table persists across plugin reopen
- Build confirmation

CONSTRAINTS:
- All chord sequences must be musically correct for the genre — no generic I-IV-V-I for Jazz
- No allocations on the DSP thread for progression loading
- Do not break existing chord_sets functionality
- Follow system rules

AFTER COMPLETION:
- Update /04_LOGS/project_log.md
- Change STATUS to DONE