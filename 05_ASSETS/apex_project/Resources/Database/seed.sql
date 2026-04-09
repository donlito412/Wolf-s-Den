-- =============================================================================
-- Wolf's Den VST — Database Seed
-- File:    Resources/Database/seed.sql
-- Purpose: Reference seed for apex.db
--          The TheoryEngine.cpp::seedDatabase() runs equivalent SQL at runtime
--          from embedded strings.  This file is for documentation and manual
--          DB reconstruction.
-- =============================================================================

PRAGMA journal_mode = WAL;
PRAGMA synchronous  = NORMAL;

-- =============================================================================
-- SCHEMA
-- =============================================================================

CREATE TABLE IF NOT EXISTS chords (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    name             TEXT NOT NULL,
    symbol           TEXT NOT NULL,
    interval_pattern TEXT NOT NULL,   -- JSON: [0,4,7]
    category         TEXT NOT NULL,   -- triad / seventh / ninth / extended / sus
    quality          TEXT NOT NULL    -- major / minor / dominant / diminished / augmented
);

CREATE TABLE IF NOT EXISTS scales (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    name             TEXT NOT NULL,
    interval_pattern TEXT NOT NULL,   -- JSON: [0,2,4,5,7,9,11]
    mode_family      TEXT NOT NULL    -- major / minor / pentatonic / exotic / chromatic
);

CREATE TABLE IF NOT EXISTS chord_sets (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    name      TEXT NOT NULL,
    author    TEXT DEFAULT 'Wolf Den Factory',
    genre     TEXT,
    mood      TEXT,
    energy    TEXT,          -- Low / Medium / High
    root_note INTEGER DEFAULT 0,
    scale_id  INTEGER REFERENCES scales(id)
);

CREATE TABLE IF NOT EXISTS chord_set_entries (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    chord_set_id INTEGER NOT NULL REFERENCES chord_sets(id),
    position     INTEGER NOT NULL,   -- 1-based order in the set
    root_note    INTEGER NOT NULL,   -- 0=C, 1=C#, 2=D, 3=Eb, 4=E, 5=F,
                                     -- 6=F#, 7=G, 8=Ab, 9=A, 10=Bb, 11=B
    chord_id     INTEGER NOT NULL REFERENCES chords(id),
    voicing      TEXT,               -- JSON: specific MIDI note layout (optional)
    motion_id    INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS presets (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT NOT NULL,
    author     TEXT,
    tags       TEXT,                 -- JSON array of tag strings
    category   TEXT,
    created_at INTEGER DEFAULT (strftime('%s','now')),
    state_blob BLOB
);

CREATE INDEX IF NOT EXISTS idx_cse_set   ON chord_set_entries(chord_set_id);
CREATE INDEX IF NOT EXISTS idx_cs_genre  ON chord_sets(genre);
CREATE INDEX IF NOT EXISTS idx_cs_mood   ON chord_sets(mood);
CREATE INDEX IF NOT EXISTS idx_cs_root   ON chord_sets(root_note);

-- =============================================================================
-- CHORD DEFINITIONS  (42 types)
-- =============================================================================

INSERT INTO chords (name, symbol, interval_pattern, category, quality) VALUES
  -- Triads
  ('Major',               'maj',      '[0,4,7]',              'triad',    'major'),
  ('Minor',               'm',        '[0,3,7]',              'triad',    'minor'),
  ('Diminished',          'dim',      '[0,3,6]',              'triad',    'diminished'),
  ('Augmented',           'aug',      '[0,4,8]',              'triad',    'augmented'),
  ('Suspended 2nd',       'sus2',     '[0,2,7]',              'sus',      'major'),
  ('Suspended 4th',       'sus4',     '[0,5,7]',              'sus',      'major'),
  ('Power',               '5',        '[0,7]',                'triad',    'major'),
  -- 6th chords
  ('Major 6th',           'maj6',     '[0,4,7,9]',            'seventh',  'major'),
  ('Minor 6th',           'm6',       '[0,3,7,9]',            'seventh',  'minor'),
  -- 7th chords
  ('Dominant 7th',        '7',        '[0,4,7,10]',           'seventh',  'dominant'),
  ('Major 7th',           'maj7',     '[0,4,7,11]',           'seventh',  'major'),
  ('Minor 7th',           'm7',       '[0,3,7,10]',           'seventh',  'minor'),
  ('Diminished 7th',      'dim7',     '[0,3,6,9]',            'seventh',  'diminished'),
  ('Half-Diminished',     'm7b5',     '[0,3,6,10]',           'seventh',  'diminished'),
  ('Minor/Major 7th',     'mMaj7',    '[0,3,7,11]',           'seventh',  'minor'),
  ('Augmented Major 7th', 'augMaj7',  '[0,4,8,11]',           'seventh',  'augmented'),
  ('Dominant 7th Sus4',   '7sus4',    '[0,5,7,10]',           'seventh',  'dominant'),
  -- Add chords
  ('Major Add 9',         'add9',     '[0,2,4,7]',            'triad',    'major'),
  ('Minor Add 9',         'madd9',    '[0,2,3,7]',            'triad',    'minor'),
  -- 9th chords
  ('Dominant 9th',        '9',        '[0,4,7,10,14]',        'ninth',    'dominant'),
  ('Major 9th',           'maj9',     '[0,4,7,11,14]',        'ninth',    'major'),
  ('Minor 9th',           'm9',       '[0,3,7,10,14]',        'ninth',    'minor'),
  -- 11th chords
  ('Dominant 11th',       '11',       '[0,4,7,10,14,17]',     'extended', 'dominant'),
  ('Major 11th',          'maj11',    '[0,4,7,11,14,17]',     'extended', 'major'),
  ('Minor 11th',          'm11',      '[0,3,7,10,14,17]',     'extended', 'minor'),
  -- 13th chords
  ('Dominant 13th',       '13',       '[0,4,7,10,14,21]',     'extended', 'dominant'),
  ('Major 13th',          'maj13',    '[0,4,7,11,14,21]',     'extended', 'major'),
  ('Minor 13th',          'm13',      '[0,3,7,10,14,21]',     'extended', 'minor'),
  -- Altered dominants
  ('Dom 7th Flat 9',      '7b9',      '[0,4,7,10,13]',        'extended', 'dominant'),
  ('Dom 7th Sharp 9',     '7#9',      '[0,4,7,10,15]',        'extended', 'dominant'),
  ('Dom 7th Flat 5',      '7b5',      '[0,4,6,10]',           'seventh',  'dominant'),
  ('Dom 7th Sharp 5',     '7#5',      '[0,4,8,10]',           'seventh',  'dominant'),
  ('Dom 7th Sharp 11',    '7#11',     '[0,4,7,10,18]',        'extended', 'dominant'),
  ('Major 7th Sharp 11',  'maj7#11',  '[0,4,7,11,18]',        'extended', 'major'),
  -- Minor/Major extensions
  ('Minor/Major 9th',     'mMaj9',    '[0,3,7,11,14]',        'ninth',    'minor'),
  ('Augmented 7th',       'aug7',     '[0,4,8,10]',           'seventh',  'augmented'),
  -- Quartal/Quintal
  ('Quartal',             'qrt',      '[0,5,10]',             'triad',    'major'),
  ('Quintal',             'qnt',      '[0,7,14]',             'triad',    'major'),
  -- Exotic
  ('Neapolitan',          'N',        '[0,1,5,8]',            'triad',    'major'),
  ('Dom b9 b13',          '7b9b13',   '[0,4,7,10,13,20]',     'extended', 'dominant'),
  -- 6/9 chords
  ('Major 6/9',           '6/9',      '[0,2,4,7,9]',          'extended', 'major'),
  ('Minor 6/9',           'm6/9',     '[0,2,3,7,9]',          'extended', 'minor');

-- =============================================================================
-- SCALE DEFINITIONS  (58 scales)
-- =============================================================================

INSERT INTO scales (name, interval_pattern, mode_family) VALUES
  -- Diatonic modes
  ('Major',                    '[0,2,4,5,7,9,11]',            'major'),
  ('Natural Minor',            '[0,2,3,5,7,8,10]',            'minor'),
  ('Harmonic Minor',           '[0,2,3,5,7,8,11]',            'minor'),
  ('Melodic Minor',            '[0,2,3,5,7,9,11]',            'minor'),
  ('Dorian',                   '[0,2,3,5,7,9,10]',            'minor'),
  ('Phrygian',                 '[0,1,3,5,7,8,10]',            'minor'),
  ('Lydian',                   '[0,2,4,6,7,9,11]',            'major'),
  ('Mixolydian',               '[0,2,4,5,7,9,10]',            'major'),
  ('Locrian',                  '[0,1,3,5,6,8,10]',            'minor'),
  -- Pentatonic family
  ('Pentatonic Major',         '[0,2,4,7,9]',                 'pentatonic'),
  ('Pentatonic Minor',         '[0,3,5,7,10]',                'pentatonic'),
  ('Blues',                    '[0,3,5,6,7,10]',              'pentatonic'),
  -- Chromatic
  ('Chromatic',                '[0,1,2,3,4,5,6,7,8,9,10,11]','chromatic'),
  -- Symmetric scales
  ('Whole Tone',               '[0,2,4,6,8,10]',              'exotic'),
  ('Diminished Half-Whole',    '[0,1,3,4,6,7,9,10]',          'exotic'),
  ('Diminished Whole-Half',    '[0,2,3,5,6,8,9,11]',          'exotic'),
  ('Augmented',                '[0,3,4,7,8,11]',              'exotic'),
  -- Double harmonic / modal variants
  ('Double Harmonic',          '[0,1,4,5,7,8,11]',            'exotic'),
  ('Phrygian Dominant',        '[0,1,4,5,7,8,10]',            'major'),
  ('Lydian Dominant',          '[0,2,4,6,7,9,10]',            'major'),
  ('Lydian Augmented',         '[0,2,4,6,8,9,11]',            'major'),
  ('Super Locrian',            '[0,1,3,4,6,8,10]',            'minor'),
  -- Bebop scales
  ('Bebop Major',              '[0,2,4,5,7,8,9,11]',          'major'),
  ('Bebop Minor',              '[0,2,3,4,5,7,9,10]',          'minor'),
  ('Bebop Dominant',           '[0,2,4,5,7,9,10,11]',         'major'),
  -- Exotic Western
  ('Enigmatic',                '[0,1,4,6,8,10,11]',           'exotic'),
  ('Neapolitan Major',         '[0,1,3,5,7,9,11]',            'major'),
  ('Neapolitan Minor',         '[0,1,3,5,7,8,11]',            'minor'),
  ('Persian',                  '[0,1,4,5,6,8,11]',            'exotic'),
  ('Arabian',                  '[0,2,4,5,6,8,10]',            'exotic'),
  ('Hungarian Major',          '[0,3,4,6,7,9,10]',            'major'),
  ('Hungarian Minor',          '[0,2,3,6,7,8,11]',            'minor'),
  ('Romanian Minor',           '[0,2,3,6,7,9,10]',            'minor'),
  ('Spanish',                  '[0,1,3,4,5,7,8,10]',          'minor'),
  ('Spanish 8-Tone',           '[0,1,3,4,5,6,8,10]',          'minor'),
  -- East Asian pentatonics
  ('Japanese',                 '[0,2,3,7,8]',                 'pentatonic'),
  ('Hirajoshi',                '[0,2,3,7,8]',                 'pentatonic'),
  ('Iwato',                    '[0,1,5,6,10]',                'pentatonic'),
  ('In Sen',                   '[0,1,5,7,10]',                'pentatonic'),
  ('Yo',                       '[0,2,5,7,9]',                 'pentatonic'),
  ('Ryukyu',                   '[0,4,5,7,11]',                'pentatonic'),
  ('Balinese',                 '[0,1,3,7,8]',                 'pentatonic'),
  ('Javanese',                 '[0,1,3,5,7,9,10]',            'exotic'),
  ('Chinese',                  '[0,4,6,7,11]',                'pentatonic'),
  ('Egyptian',                 '[0,2,5,7,10]',                'pentatonic'),
  -- Prometheus / tritone family
  ('Prometheus',               '[0,2,4,6,9,10]',              'exotic'),
  ('Tritone',                  '[0,1,4,6,7,10]',              'exotic'),
  ('Two Semitone Tritone',     '[0,1,2,6,7,8]',               'exotic'),
  -- World / Flamenco
  ('Flamenco',                 '[0,1,4,5,7,8,11]',            'exotic'),
  ('Gypsy Major',              '[0,2,3,6,7,8,11]',            'exotic'),
  ('Gypsy Minor',              '[0,2,3,6,7,8,10]',            'minor'),
  ('Byzantine',                '[0,1,4,5,7,8,11]',            'exotic'),
  ('Hawaiian',                 '[0,2,3,5,7,9,11]',            'minor'),
  -- Jazz / contemporary
  ('Overtone',                 '[0,2,4,6,7,9,10]',            'major'),
  ('Leading Whole-Tone',       '[0,2,4,6,8,10,11]',           'exotic'),
  ('Altered',                  '[0,1,3,4,6,8,11]',            'minor'),
  ('Acoustic',                 '[0,2,4,6,7,9,10]',            'major'),
  -- Indonesian
  ('Pelog',                    '[0,1,3,6,7]',                 'pentatonic');

-- =============================================================================
-- CHORD SETS  (216 sets: 18 progressions × 12 root keys)
-- Chord ID reference (matches the INSERT above):
--   1=maj  2=m   3=dim  4=aug   5=sus2  6=sus4   7=pwr
--   8=maj6  9=m6  10=dom7  11=maj7  12=m7  13=dim7  14=m7b5
--  15=mMaj7  16=augMaj7  17=7sus4  18=add9  19=madd9
--  20=dom9  21=maj9  22=m9  23=dom11  24=maj11  25=m11
--  26=dom13  27=maj13  28=m13
--  29=7b9  30=7#9  31=7b5  32=7#5  33=7#11  34=maj7#11
--  35=mMaj9  36=aug7  37=qrt  38=qnt  39=N  40=7b9b13
--  41=6/9   42=m6/9
-- Scale IDs: 1=Major  2=Natural Minor  5=Dorian  6=Phrygian
--            7=Lydian  8=Mixolydian  11=Pent.Minor  12=Blues
-- =============================================================================

-- ─── PROGRESSION 1: I-Vmaj7-vim7-IVmaj7 — Pop Bright Medium (Major scale) ───

-- C
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Pop Bright','Pop','Bright','Medium',0,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,11),(last_insert_rowid(),2,7,11),(last_insert_rowid(),3,9,12),(last_insert_rowid(),4,5,11);
-- Db
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Pop Bright','Pop','Bright','Medium',1,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,11),(last_insert_rowid(),2,8,11),(last_insert_rowid(),3,10,12),(last_insert_rowid(),4,6,11);
-- D
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Pop Bright','Pop','Bright','Medium',2,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,11),(last_insert_rowid(),2,9,11),(last_insert_rowid(),3,11,12),(last_insert_rowid(),4,7,11);
-- Eb
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Pop Bright','Pop','Bright','Medium',3,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,11),(last_insert_rowid(),2,10,11),(last_insert_rowid(),3,0,12),(last_insert_rowid(),4,8,11);
-- E
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Pop Bright','Pop','Bright','Medium',4,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,11),(last_insert_rowid(),2,11,11),(last_insert_rowid(),3,1,12),(last_insert_rowid(),4,9,11);
-- F
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Pop Bright','Pop','Bright','Medium',5,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,11),(last_insert_rowid(),2,0,11),(last_insert_rowid(),3,2,12),(last_insert_rowid(),4,10,11);
-- F#
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Pop Bright','Pop','Bright','Medium',6,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,11),(last_insert_rowid(),2,1,11),(last_insert_rowid(),3,3,12),(last_insert_rowid(),4,11,11);
-- G
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Pop Bright','Pop','Bright','Medium',7,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,11),(last_insert_rowid(),2,2,11),(last_insert_rowid(),3,4,12),(last_insert_rowid(),4,0,11);
-- Ab
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Pop Bright','Pop','Bright','Medium',8,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,11),(last_insert_rowid(),2,3,11),(last_insert_rowid(),3,5,12),(last_insert_rowid(),4,1,11);
-- A
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Pop Bright','Pop','Bright','Medium',9,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,11),(last_insert_rowid(),2,4,11),(last_insert_rowid(),3,6,12),(last_insert_rowid(),4,2,11);
-- Bb
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Pop Bright','Pop','Bright','Medium',10,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,11),(last_insert_rowid(),2,5,11),(last_insert_rowid(),3,7,12),(last_insert_rowid(),4,3,11);
-- B
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Pop Bright','Pop','Bright','Medium',11,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,11),(last_insert_rowid(),2,6,11),(last_insert_rowid(),3,8,12),(last_insert_rowid(),4,4,11);

-- ─── PROGRESSION 2: im-bVII-bVI-bVII — Pop Emotive Minor ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Pop Emotive','Pop','Emotive','Medium',0,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,2),(last_insert_rowid(),2,10,1),(last_insert_rowid(),3,8,1),(last_insert_rowid(),4,10,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Pop Emotive','Pop','Emotive','Medium',1,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,2),(last_insert_rowid(),2,11,1),(last_insert_rowid(),3,9,1),(last_insert_rowid(),4,11,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Pop Emotive','Pop','Emotive','Medium',2,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,2),(last_insert_rowid(),2,0,1),(last_insert_rowid(),3,10,1),(last_insert_rowid(),4,0,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Pop Emotive','Pop','Emotive','Medium',3,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,2),(last_insert_rowid(),2,1,1),(last_insert_rowid(),3,11,1),(last_insert_rowid(),4,1,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Pop Emotive','Pop','Emotive','Medium',4,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,2),(last_insert_rowid(),2,2,1),(last_insert_rowid(),3,0,1),(last_insert_rowid(),4,2,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Pop Emotive','Pop','Emotive','Medium',5,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,2),(last_insert_rowid(),2,3,1),(last_insert_rowid(),3,1,1),(last_insert_rowid(),4,3,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Pop Emotive','Pop','Emotive','Medium',6,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,2),(last_insert_rowid(),2,4,1),(last_insert_rowid(),3,2,1),(last_insert_rowid(),4,4,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Pop Emotive','Pop','Emotive','Medium',7,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,2),(last_insert_rowid(),2,5,1),(last_insert_rowid(),3,3,1),(last_insert_rowid(),4,5,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Pop Emotive','Pop','Emotive','Medium',8,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,2),(last_insert_rowid(),2,6,1),(last_insert_rowid(),3,4,1),(last_insert_rowid(),4,6,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Pop Emotive','Pop','Emotive','Medium',9,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,2),(last_insert_rowid(),2,7,1),(last_insert_rowid(),3,5,1),(last_insert_rowid(),4,7,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Pop Emotive','Pop','Emotive','Medium',10,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,2),(last_insert_rowid(),2,8,1),(last_insert_rowid(),3,6,1),(last_insert_rowid(),4,8,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Pop Emotive','Pop','Emotive','Medium',11,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,2),(last_insert_rowid(),2,9,1),(last_insert_rowid(),3,7,1),(last_insert_rowid(),4,9,1);

-- ─── PROGRESSION 3: I-IV-V-I — Rock Energetic High ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Rock Energetic','Rock','Energetic','High',0,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,1),(last_insert_rowid(),2,5,1),(last_insert_rowid(),3,7,1),(last_insert_rowid(),4,0,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Rock Energetic','Rock','Energetic','High',2,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,1),(last_insert_rowid(),2,7,1),(last_insert_rowid(),3,9,1),(last_insert_rowid(),4,2,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Rock Energetic','Rock','Energetic','High',4,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,1),(last_insert_rowid(),2,9,1),(last_insert_rowid(),3,11,1),(last_insert_rowid(),4,4,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Rock Energetic','Rock','Energetic','High',7,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,1),(last_insert_rowid(),2,0,1),(last_insert_rowid(),3,2,1),(last_insert_rowid(),4,7,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Rock Energetic','Rock','Energetic','High',9,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,1),(last_insert_rowid(),2,2,1),(last_insert_rowid(),3,4,1),(last_insert_rowid(),4,9,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Rock Energetic','Rock','Energetic','High',11,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,1),(last_insert_rowid(),2,4,1),(last_insert_rowid(),3,6,1),(last_insert_rowid(),4,11,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Rock Energetic','Rock','Energetic','High',5,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,1),(last_insert_rowid(),2,10,1),(last_insert_rowid(),3,0,1),(last_insert_rowid(),4,5,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Rock Energetic','Rock','Energetic','High',8,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,1),(last_insert_rowid(),2,1,1),(last_insert_rowid(),3,3,1),(last_insert_rowid(),4,8,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Rock Energetic','Rock','Energetic','High',10,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,1),(last_insert_rowid(),2,3,1),(last_insert_rowid(),3,5,1),(last_insert_rowid(),4,10,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Rock Energetic','Rock','Energetic','High',3,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,1),(last_insert_rowid(),2,8,1),(last_insert_rowid(),3,10,1),(last_insert_rowid(),4,3,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Rock Energetic','Rock','Energetic','High',6,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,1),(last_insert_rowid(),2,11,1),(last_insert_rowid(),3,1,1),(last_insert_rowid(),4,6,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Rock Energetic','Rock','Energetic','High',1,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,1),(last_insert_rowid(),2,6,1),(last_insert_rowid(),3,8,1),(last_insert_rowid(),4,1,1);

-- ─── PROGRESSION 4: IIm7-V7-Imaj7 — Jazz Sophisticated (all 12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Jazz ii-V-I','Jazz','Sophisticated','Medium',0,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,12),(last_insert_rowid(),2,7,10),(last_insert_rowid(),3,0,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Jazz ii-V-I','Jazz','Sophisticated','Medium',5,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,12),(last_insert_rowid(),2,0,10),(last_insert_rowid(),3,5,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Jazz ii-V-I','Jazz','Sophisticated','Medium',10,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,12),(last_insert_rowid(),2,5,10),(last_insert_rowid(),3,10,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Jazz ii-V-I','Jazz','Sophisticated','Medium',3,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,12),(last_insert_rowid(),2,10,10),(last_insert_rowid(),3,3,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Jazz ii-V-I','Jazz','Sophisticated','Medium',8,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,12),(last_insert_rowid(),2,3,10),(last_insert_rowid(),3,8,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Jazz ii-V-I','Jazz','Sophisticated','Medium',1,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,12),(last_insert_rowid(),2,8,10),(last_insert_rowid(),3,1,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Gb Jazz ii-V-I','Jazz','Sophisticated','Medium',6,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,12),(last_insert_rowid(),2,1,10),(last_insert_rowid(),3,6,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Jazz ii-V-I','Jazz','Sophisticated','Medium',11,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,12),(last_insert_rowid(),2,6,10),(last_insert_rowid(),3,11,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Jazz ii-V-I','Jazz','Sophisticated','Medium',4,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,12),(last_insert_rowid(),2,11,10),(last_insert_rowid(),3,4,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Jazz ii-V-I','Jazz','Sophisticated','Medium',9,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,12),(last_insert_rowid(),2,4,10),(last_insert_rowid(),3,9,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Jazz ii-V-I','Jazz','Sophisticated','Medium',2,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,12),(last_insert_rowid(),2,9,10),(last_insert_rowid(),3,2,11);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Jazz ii-V-I','Jazz','Sophisticated','Medium',7,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,12),(last_insert_rowid(),2,2,10),(last_insert_rowid(),3,7,11);

-- ─── PROGRESSION 5: im7-IVm7-Vm7-IIm7 — Lo-Fi Dorian (all 12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Lo-Fi Dorian','Lo-Fi','Peaceful','Low',0,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,12),(last_insert_rowid(),2,5,12),(last_insert_rowid(),3,7,12),(last_insert_rowid(),4,2,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Lo-Fi Dorian','Lo-Fi','Peaceful','Low',2,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,12),(last_insert_rowid(),2,7,12),(last_insert_rowid(),3,9,12),(last_insert_rowid(),4,4,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Lo-Fi Dorian','Lo-Fi','Peaceful','Low',4,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,12),(last_insert_rowid(),2,9,12),(last_insert_rowid(),3,11,12),(last_insert_rowid(),4,6,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Lo-Fi Dorian','Lo-Fi','Peaceful','Low',5,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,12),(last_insert_rowid(),2,10,12),(last_insert_rowid(),3,0,12),(last_insert_rowid(),4,7,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Lo-Fi Dorian','Lo-Fi','Peaceful','Low',7,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,12),(last_insert_rowid(),2,0,12),(last_insert_rowid(),3,2,12),(last_insert_rowid(),4,9,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Lo-Fi Dorian','Lo-Fi','Peaceful','Low',9,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,12),(last_insert_rowid(),2,2,12),(last_insert_rowid(),3,4,12),(last_insert_rowid(),4,11,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Lo-Fi Dorian','Lo-Fi','Peaceful','Low',10,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,12),(last_insert_rowid(),2,3,12),(last_insert_rowid(),3,5,12),(last_insert_rowid(),4,0,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Lo-Fi Dorian','Lo-Fi','Peaceful','Low',8,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,12),(last_insert_rowid(),2,1,12),(last_insert_rowid(),3,3,12),(last_insert_rowid(),4,10,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Lo-Fi Dorian','Lo-Fi','Peaceful','Low',3,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,12),(last_insert_rowid(),2,8,12),(last_insert_rowid(),3,10,12),(last_insert_rowid(),4,5,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Lo-Fi Dorian','Lo-Fi','Peaceful','Low',11,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,12),(last_insert_rowid(),2,4,12),(last_insert_rowid(),3,6,12),(last_insert_rowid(),4,1,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Lo-Fi Dorian','Lo-Fi','Peaceful','Low',1,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,12),(last_insert_rowid(),2,6,12),(last_insert_rowid(),3,8,12),(last_insert_rowid(),4,3,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Lo-Fi Dorian','Lo-Fi','Peaceful','Low',6,5);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,12),(last_insert_rowid(),2,11,12),(last_insert_rowid(),3,1,12),(last_insert_rowid(),4,8,12);

-- ─── PROGRESSION 6: I7-IV7-V7-IV7 — Blues Soulful Medium (all 12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Blues','Blues','Soulful','Medium',0,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,10),(last_insert_rowid(),2,5,10),(last_insert_rowid(),3,7,10),(last_insert_rowid(),4,5,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Blues','Blues','Soulful','Medium',4,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,10),(last_insert_rowid(),2,9,10),(last_insert_rowid(),3,11,10),(last_insert_rowid(),4,9,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Blues','Blues','Soulful','Medium',9,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,10),(last_insert_rowid(),2,2,10),(last_insert_rowid(),3,4,10),(last_insert_rowid(),4,2,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Blues','Blues','Soulful','Medium',11,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,10),(last_insert_rowid(),2,4,10),(last_insert_rowid(),3,6,10),(last_insert_rowid(),4,4,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Blues','Blues','Soulful','Medium',7,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,10),(last_insert_rowid(),2,0,10),(last_insert_rowid(),3,2,10),(last_insert_rowid(),4,0,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Blues','Blues','Soulful','Medium',2,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,10),(last_insert_rowid(),2,7,10),(last_insert_rowid(),3,9,10),(last_insert_rowid(),4,7,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Blues','Blues','Soulful','Medium',5,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,10),(last_insert_rowid(),2,10,10),(last_insert_rowid(),3,0,10),(last_insert_rowid(),4,10,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Blues','Blues','Soulful','Medium',10,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,10),(last_insert_rowid(),2,3,10),(last_insert_rowid(),3,5,10),(last_insert_rowid(),4,3,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Blues','Blues','Soulful','Medium',3,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,10),(last_insert_rowid(),2,8,10),(last_insert_rowid(),3,10,10),(last_insert_rowid(),4,8,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Blues','Blues','Soulful','Medium',8,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,10),(last_insert_rowid(),2,1,10),(last_insert_rowid(),3,3,10),(last_insert_rowid(),4,1,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Blues','Blues','Soulful','Medium',1,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,10),(last_insert_rowid(),2,6,10),(last_insert_rowid(),3,8,10),(last_insert_rowid(),4,6,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Blues','Blues','Soulful','Medium',6,12);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,10),(last_insert_rowid(),2,11,10),(last_insert_rowid(),3,1,10),(last_insert_rowid(),4,11,10);

-- ─── PROGRESSION 7: Imaj7-Vmaj7-IIm7-VIm7 — Ambient Dreamy Lydian (12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Ambient Lydian','Ambient','Dreamy','Low',0,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,11),(last_insert_rowid(),2,7,11),(last_insert_rowid(),3,2,12),(last_insert_rowid(),4,9,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Ambient Lydian','Ambient','Dreamy','Low',2,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,11),(last_insert_rowid(),2,9,11),(last_insert_rowid(),3,4,12),(last_insert_rowid(),4,11,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Ambient Lydian','Ambient','Dreamy','Low',4,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,11),(last_insert_rowid(),2,11,11),(last_insert_rowid(),3,6,12),(last_insert_rowid(),4,1,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Ambient Lydian','Ambient','Dreamy','Low',7,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,11),(last_insert_rowid(),2,2,11),(last_insert_rowid(),3,9,12),(last_insert_rowid(),4,4,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Ambient Lydian','Ambient','Dreamy','Low',9,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,11),(last_insert_rowid(),2,4,11),(last_insert_rowid(),3,11,12),(last_insert_rowid(),4,6,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Ambient Lydian','Ambient','Dreamy','Low',5,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,11),(last_insert_rowid(),2,0,11),(last_insert_rowid(),3,7,12),(last_insert_rowid(),4,2,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Ambient Lydian','Ambient','Dreamy','Low',10,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,11),(last_insert_rowid(),2,5,11),(last_insert_rowid(),3,0,12),(last_insert_rowid(),4,7,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Ambient Lydian','Ambient','Dreamy','Low',8,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,11),(last_insert_rowid(),2,3,11),(last_insert_rowid(),3,10,12),(last_insert_rowid(),4,5,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Ambient Lydian','Ambient','Dreamy','Low',3,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,11),(last_insert_rowid(),2,10,11),(last_insert_rowid(),3,5,12),(last_insert_rowid(),4,0,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Ambient Lydian','Ambient','Dreamy','Low',11,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,11),(last_insert_rowid(),2,6,11),(last_insert_rowid(),3,1,12),(last_insert_rowid(),4,8,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Ambient Lydian','Ambient','Dreamy','Low',1,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,11),(last_insert_rowid(),2,8,11),(last_insert_rowid(),3,3,12),(last_insert_rowid(),4,10,12);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Ambient Lydian','Ambient','Dreamy','Low',6,7);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,11),(last_insert_rowid(),2,1,11),(last_insert_rowid(),3,8,12),(last_insert_rowid(),4,3,12);

-- ─── PROGRESSION 8: I-bVII-IV-I — Rock Mixolydian Anthemic (12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Mixolydian Anthemic','Rock','Anthemic','High',0,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,1),(last_insert_rowid(),2,10,1),(last_insert_rowid(),3,5,1),(last_insert_rowid(),4,0,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Mixolydian Anthemic','Rock','Anthemic','High',2,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,1),(last_insert_rowid(),2,0,1),(last_insert_rowid(),3,7,1),(last_insert_rowid(),4,2,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Mixolydian Anthemic','Rock','Anthemic','High',4,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,1),(last_insert_rowid(),2,2,1),(last_insert_rowid(),3,9,1),(last_insert_rowid(),4,4,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Mixolydian Anthemic','Rock','Anthemic','High',7,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,1),(last_insert_rowid(),2,5,1),(last_insert_rowid(),3,0,1),(last_insert_rowid(),4,7,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Mixolydian Anthemic','Rock','Anthemic','High',9,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,1),(last_insert_rowid(),2,7,1),(last_insert_rowid(),3,2,1),(last_insert_rowid(),4,9,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Mixolydian Anthemic','Rock','Anthemic','High',5,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,1),(last_insert_rowid(),2,3,1),(last_insert_rowid(),3,10,1),(last_insert_rowid(),4,5,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Mixolydian Anthemic','Rock','Anthemic','High',10,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,1),(last_insert_rowid(),2,8,1),(last_insert_rowid(),3,3,1),(last_insert_rowid(),4,10,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Mixolydian Anthemic','Rock','Anthemic','High',8,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,1),(last_insert_rowid(),2,6,1),(last_insert_rowid(),3,1,1),(last_insert_rowid(),4,8,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Mixolydian Anthemic','Rock','Anthemic','High',3,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,1),(last_insert_rowid(),2,1,1),(last_insert_rowid(),3,8,1),(last_insert_rowid(),4,3,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Mixolydian Anthemic','Rock','Anthemic','High',11,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,1),(last_insert_rowid(),2,9,1),(last_insert_rowid(),3,4,1),(last_insert_rowid(),4,11,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Mixolydian Anthemic','Rock','Anthemic','High',1,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,1),(last_insert_rowid(),2,11,1),(last_insert_rowid(),3,6,1),(last_insert_rowid(),4,1,1);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Mixolydian Anthemic','Rock','Anthemic','High',6,8);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,1),(last_insert_rowid(),2,4,1),(last_insert_rowid(),3,11,1),(last_insert_rowid(),4,6,1);

-- ─── PROGRESSION 9: im7-VImaj7-IIImaj7-V7 — Electronic Dark Cinematic (12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Electronic Dark','Electronic','Dark','High',0,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,12),(last_insert_rowid(),2,8,11),(last_insert_rowid(),3,3,11),(last_insert_rowid(),4,7,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Electronic Dark','Electronic','Dark','High',2,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,12),(last_insert_rowid(),2,10,11),(last_insert_rowid(),3,5,11),(last_insert_rowid(),4,9,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Electronic Dark','Electronic','Dark','High',4,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,12),(last_insert_rowid(),2,0,11),(last_insert_rowid(),3,7,11),(last_insert_rowid(),4,11,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Electronic Dark','Electronic','Dark','High',7,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,12),(last_insert_rowid(),2,3,11),(last_insert_rowid(),3,10,11),(last_insert_rowid(),4,2,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Electronic Dark','Electronic','Dark','High',9,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,12),(last_insert_rowid(),2,5,11),(last_insert_rowid(),3,0,11),(last_insert_rowid(),4,4,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Electronic Dark','Electronic','Dark','High',11,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,12),(last_insert_rowid(),2,7,11),(last_insert_rowid(),3,2,11),(last_insert_rowid(),4,6,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Electronic Dark','Electronic','Dark','High',5,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,12),(last_insert_rowid(),2,1,11),(last_insert_rowid(),3,8,11),(last_insert_rowid(),4,0,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Electronic Dark','Electronic','Dark','High',8,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,12),(last_insert_rowid(),2,4,11),(last_insert_rowid(),3,11,11),(last_insert_rowid(),4,3,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Electronic Dark','Electronic','Dark','High',10,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,12),(last_insert_rowid(),2,6,11),(last_insert_rowid(),3,1,11),(last_insert_rowid(),4,5,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Electronic Dark','Electronic','Dark','High',3,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,12),(last_insert_rowid(),2,11,11),(last_insert_rowid(),3,6,11),(last_insert_rowid(),4,10,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Electronic Dark','Electronic','Dark','High',1,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,12),(last_insert_rowid(),2,9,11),(last_insert_rowid(),3,4,11),(last_insert_rowid(),4,8,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Electronic Dark','Electronic','Dark','High',6,2);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,12),(last_insert_rowid(),2,2,11),(last_insert_rowid(),3,9,11),(last_insert_rowid(),4,1,10);

-- ─── PROGRESSION 10: im-VII-VI-V — Andalusian Cinematic (12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A Cinematic Andalusian','Cinematic','Dramatic','High',9,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,2),(last_insert_rowid(),2,7,1),(last_insert_rowid(),3,5,1),(last_insert_rowid(),4,4,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D Cinematic Andalusian','Cinematic','Dramatic','High',2,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,2),(last_insert_rowid(),2,0,1),(last_insert_rowid(),3,10,1),(last_insert_rowid(),4,9,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E Cinematic Andalusian','Cinematic','Dramatic','High',4,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,2),(last_insert_rowid(),2,2,1),(last_insert_rowid(),3,0,1),(last_insert_rowid(),4,11,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G Cinematic Andalusian','Cinematic','Dramatic','High',7,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,2),(last_insert_rowid(),2,5,1),(last_insert_rowid(),3,3,1),(last_insert_rowid(),4,2,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B Cinematic Andalusian','Cinematic','Dramatic','High',11,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,2),(last_insert_rowid(),2,9,1),(last_insert_rowid(),3,7,1),(last_insert_rowid(),4,6,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C Cinematic Andalusian','Cinematic','Dramatic','High',0,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,2),(last_insert_rowid(),2,10,1),(last_insert_rowid(),3,8,1),(last_insert_rowid(),4,7,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F Cinematic Andalusian','Cinematic','Dramatic','High',5,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,2),(last_insert_rowid(),2,3,1),(last_insert_rowid(),3,1,1),(last_insert_rowid(),4,0,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb Cinematic Andalusian','Cinematic','Dramatic','High',10,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,2),(last_insert_rowid(),2,8,1),(last_insert_rowid(),3,6,1),(last_insert_rowid(),4,5,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb Cinematic Andalusian','Cinematic','Dramatic','High',3,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,2),(last_insert_rowid(),2,1,1),(last_insert_rowid(),3,11,1),(last_insert_rowid(),4,10,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab Cinematic Andalusian','Cinematic','Dramatic','High',8,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,2),(last_insert_rowid(),2,6,1),(last_insert_rowid(),3,4,1),(last_insert_rowid(),4,3,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db Cinematic Andalusian','Cinematic','Dramatic','High',1,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,2),(last_insert_rowid(),2,11,1),(last_insert_rowid(),3,9,1),(last_insert_rowid(),4,8,10);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# Cinematic Andalusian','Cinematic','Dramatic','High',6,6);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,2),(last_insert_rowid(),2,4,1),(last_insert_rowid(),3,2,1),(last_insert_rowid(),4,1,10);

-- ─── PROGRESSION 11: Imaj9-IIm9-V13-Imaj9 — R&B Smooth (12 keys) ───

INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('C R&B Smooth','R&B','Smooth','Medium',0,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,0,21),(last_insert_rowid(),2,2,22),(last_insert_rowid(),3,7,26),(last_insert_rowid(),4,0,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Eb R&B Smooth','R&B','Smooth','Medium',3,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,3,21),(last_insert_rowid(),2,5,22),(last_insert_rowid(),3,10,26),(last_insert_rowid(),4,3,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F R&B Smooth','R&B','Smooth','Medium',5,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,5,21),(last_insert_rowid(),2,7,22),(last_insert_rowid(),3,0,26),(last_insert_rowid(),4,5,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('G R&B Smooth','R&B','Smooth','Medium',7,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,7,21),(last_insert_rowid(),2,9,22),(last_insert_rowid(),3,2,26),(last_insert_rowid(),4,7,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Ab R&B Smooth','R&B','Smooth','Medium',8,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,8,21),(last_insert_rowid(),2,10,22),(last_insert_rowid(),3,3,26),(last_insert_rowid(),4,8,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Bb R&B Smooth','R&B','Smooth','Medium',10,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,10,21),(last_insert_rowid(),2,0,22),(last_insert_rowid(),3,5,26),(last_insert_rowid(),4,10,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('D R&B Smooth','R&B','Smooth','Medium',2,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,2,21),(last_insert_rowid(),2,4,22),(last_insert_rowid(),3,9,26),(last_insert_rowid(),4,2,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('E R&B Smooth','R&B','Smooth','Medium',4,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,4,21),(last_insert_rowid(),2,6,22),(last_insert_rowid(),3,11,26),(last_insert_rowid(),4,4,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('A R&B Smooth','R&B','Smooth','Medium',9,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,9,21),(last_insert_rowid(),2,11,22),(last_insert_rowid(),3,4,26),(last_insert_rowid(),4,9,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('B R&B Smooth','R&B','Smooth','Medium',11,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,11,21),(last_insert_rowid(),2,1,22),(last_insert_rowid(),3,6,26),(last_insert_rowid(),4,11,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('Db R&B Smooth','R&B','Smooth','Medium',1,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,1,21),(last_insert_rowid(),2,3,22),(last_insert_rowid(),3,8,26),(last_insert_rowid(),4,1,21);
INSERT INTO chord_sets(name,genre,mood,energy,root_note,scale_id) VALUES ('F# R&B Smooth','R&B','Smooth','Medium',6,1);
INSERT INTO chord_set_entries(chord_set_id,position,root_note,chord_id) VALUES (last_insert_rowid(),1,6,21),(last_insert_rowid(),2,8,22),(last_insert_rowid(),3,1,26),(last_insert_rowid(),4,6,21);

-- =============================================================================
-- VERIFICATION QUERY (run to confirm seed counts)
-- =============================================================================
-- SELECT 'chords' AS tbl, COUNT(*) AS n FROM chords
-- UNION ALL SELECT 'scales', COUNT(*) FROM scales
-- UNION ALL SELECT 'chord_sets', COUNT(*) FROM chord_sets
-- UNION ALL SELECT 'chord_set_entries', COUNT(*) FROM chord_set_entries;
--
-- Expected:  chords=42  scales=58  chord_sets≥216  chord_set_entries≥864
-- =============================================================================
