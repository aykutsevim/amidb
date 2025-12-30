-- ============================================================
-- Retro Game History Database
-- ============================================================
-- A database cataloging classic video games from the 8-bit and
-- 16-bit era. Perfect for game collectors and historians.
--
-- Tables:
--   - manufacturers: Game publishers and developers
--   - platforms: Gaming systems (Amiga, C64, NES, etc.)
--   - genres: Game categories
--   - games: The main game catalog
--   - reviews: Game ratings and reviews
--
-- Usage:
--   ./amidb_shell RAM:gamehistory.db 01_retro_game_history.sql
-- ============================================================

-- ============================================================
-- MANUFACTURERS - Game publishers and developers
-- ============================================================
CREATE TABLE manufacturers (
    id INTEGER PRIMARY KEY,
    name TEXT,
    country TEXT,
    founded INTEGER,
    active INTEGER
);

-- Classic game companies
INSERT INTO manufacturers VALUES (1, 'Commodore', 'USA', 1954, 0);
INSERT INTO manufacturers VALUES (2, 'Team17', 'UK', 1990, 1);
INSERT INTO manufacturers VALUES (3, 'Cinemaware', 'USA', 1985, 0);
INSERT INTO manufacturers VALUES (4, 'Psygnosis', 'UK', 1984, 0);
INSERT INTO manufacturers VALUES (5, 'Electronic Arts', 'USA', 1982, 1);
INSERT INTO manufacturers VALUES (6, 'Ocean Software', 'UK', 1983, 0);
INSERT INTO manufacturers VALUES (7, 'Bitmap Brothers', 'UK', 1987, 0);
INSERT INTO manufacturers VALUES (8, 'Sensible Software', 'UK', 1986, 0);
INSERT INTO manufacturers VALUES (9, 'Factor 5', 'Germany', 1987, 0);
INSERT INTO manufacturers VALUES (10, 'DMA Design', 'UK', 1987, 1);
INSERT INTO manufacturers VALUES (11, 'Bullfrog', 'UK', 1987, 0);
INSERT INTO manufacturers VALUES (12, 'Gremlin', 'UK', 1984, 0);
INSERT INTO manufacturers VALUES (13, 'Thalion', 'Germany', 1988, 0);
INSERT INTO manufacturers VALUES (14, 'Rainbow Arts', 'Germany', 1984, 0);
INSERT INTO manufacturers VALUES (15, 'Konami', 'Japan', 1969, 1);

-- ============================================================
-- PLATFORMS - Gaming systems
-- ============================================================
CREATE TABLE platforms (
    id INTEGER PRIMARY KEY,
    name TEXT,
    manufacturer_id INTEGER,
    release_year INTEGER,
    cpu TEXT,
    bits INTEGER
);

-- Classic platforms
INSERT INTO platforms VALUES (1, 'Amiga 500', 1, 1987, '68000', 16);
INSERT INTO platforms VALUES (2, 'Amiga 1200', 1, 1992, '68020', 32);
INSERT INTO platforms VALUES (3, 'Commodore 64', 1, 1982, '6510', 8);
INSERT INTO platforms VALUES (4, 'Atari ST', 0, 1985, '68000', 16);
INSERT INTO platforms VALUES (5, 'NES', 0, 1983, '6502', 8);
INSERT INTO platforms VALUES (6, 'SNES', 0, 1990, '65816', 16);
INSERT INTO platforms VALUES (7, 'Sega Genesis', 0, 1988, '68000', 16);
INSERT INTO platforms VALUES (8, 'Amiga CD32', 1, 1993, '68020', 32);
INSERT INTO platforms VALUES (9, 'DOS PC', 0, 1981, '8086', 16);
INSERT INTO platforms VALUES (10, 'Arcade', 0, 1970, 'Various', 0);

-- ============================================================
-- GENRES - Game categories
-- ============================================================
CREATE TABLE genres (
    id INTEGER PRIMARY KEY,
    name TEXT,
    description TEXT
);

INSERT INTO genres VALUES (1, 'Platformer', 'Jump and run games');
INSERT INTO genres VALUES (2, 'Shoot-em-up', 'Scrolling shooter games');
INSERT INTO genres VALUES (3, 'RPG', 'Role-playing games');
INSERT INTO genres VALUES (4, 'Strategy', 'Turn-based or real-time strategy');
INSERT INTO genres VALUES (5, 'Adventure', 'Story-driven exploration');
INSERT INTO genres VALUES (6, 'Puzzle', 'Brain teasers and logic games');
INSERT INTO genres VALUES (7, 'Sports', 'Sports simulation games');
INSERT INTO genres VALUES (8, 'Racing', 'Vehicle racing games');
INSERT INTO genres VALUES (9, 'Fighting', 'Combat and beat-em-up');
INSERT INTO genres VALUES (10, 'Simulation', 'Life and world simulation');

-- ============================================================
-- GAMES - The main game catalog
-- ============================================================
CREATE TABLE games (
    id INTEGER PRIMARY KEY,
    title TEXT,
    manufacturer_id INTEGER,
    platform_id INTEGER,
    genre_id INTEGER,
    release_year INTEGER,
    disks INTEGER,
    multiplayer INTEGER
);

-- Classic Amiga games
INSERT INTO games VALUES (1, 'Turrican', 14, 1, 1, 1990, 1, 0);
INSERT INTO games VALUES (2, 'Turrican II', 14, 1, 1, 1991, 2, 0);
INSERT INTO games VALUES (3, 'Turrican 3', 9, 1, 1, 1993, 3, 0);
INSERT INTO games VALUES (4, 'Shadow of the Beast', 4, 1, 1, 1989, 1, 0);
INSERT INTO games VALUES (5, 'Shadow of the Beast II', 4, 1, 1, 1990, 2, 0);
INSERT INTO games VALUES (6, 'Lemmings', 10, 1, 6, 1991, 1, 0);
INSERT INTO games VALUES (7, 'Lemmings 2: The Tribes', 10, 1, 6, 1993, 2, 0);
INSERT INTO games VALUES (8, 'Worms', 2, 1, 4, 1995, 2, 1);
INSERT INTO games VALUES (9, 'Alien Breed', 2, 1, 2, 1991, 1, 1);
INSERT INTO games VALUES (10, 'Alien Breed II', 2, 1, 2, 1993, 2, 1);
INSERT INTO games VALUES (11, 'Speedball 2', 7, 1, 7, 1990, 1, 1);
INSERT INTO games VALUES (12, 'Chaos Engine', 7, 1, 2, 1993, 2, 1);
INSERT INTO games VALUES (13, 'Gods', 7, 1, 1, 1991, 2, 0);
INSERT INTO games VALUES (14, 'Sensible Soccer', 8, 1, 7, 1992, 1, 1);
INSERT INTO games VALUES (15, 'Cannon Fodder', 8, 1, 4, 1993, 2, 0);
INSERT INTO games VALUES (16, 'Defender of the Crown', 3, 1, 4, 1986, 1, 0);
INSERT INTO games VALUES (17, 'It Came from the Desert', 3, 1, 5, 1989, 4, 0);
INSERT INTO games VALUES (18, 'Wings', 3, 1, 8, 1990, 3, 0);
INSERT INTO games VALUES (19, 'Lotus Turbo Challenge', 12, 1, 8, 1990, 1, 1);
INSERT INTO games VALUES (20, 'Lotus II', 12, 1, 8, 1991, 1, 1);
INSERT INTO games VALUES (21, 'Populous', 11, 1, 4, 1989, 1, 0);
INSERT INTO games VALUES (22, 'Powermonger', 11, 1, 4, 1990, 2, 0);
INSERT INTO games VALUES (23, 'Theme Park', 11, 1, 10, 1994, 3, 0);
INSERT INTO games VALUES (24, 'Lionheart', 13, 1, 1, 1993, 4, 0);
INSERT INTO games VALUES (25, 'Ambermoon', 13, 1, 3, 1993, 4, 0);
INSERT INTO games VALUES (26, 'The Settlers', 0, 1, 4, 1993, 2, 0);
INSERT INTO games VALUES (27, 'Another World', 0, 1, 5, 1991, 1, 0);
INSERT INTO games VALUES (28, 'Flashback', 0, 1, 5, 1992, 3, 0);
INSERT INTO games VALUES (29, 'Prince of Persia', 0, 1, 1, 1990, 1, 0);
INSERT INTO games VALUES (30, 'Pinball Dreams', 0, 1, 10, 1992, 1, 0);

-- ============================================================
-- REVIEWS - Game ratings and reviews
-- ============================================================
CREATE TABLE reviews (
    id INTEGER PRIMARY KEY,
    game_id INTEGER,
    source TEXT,
    score INTEGER,
    review_year INTEGER
);

-- Magazine reviews (score out of 100)
INSERT INTO reviews VALUES (1, 1, 'Amiga Power', 90, 1990);
INSERT INTO reviews VALUES (2, 1, 'CU Amiga', 92, 1990);
INSERT INTO reviews VALUES (3, 2, 'Amiga Format', 95, 1991);
INSERT INTO reviews VALUES (4, 2, 'The One', 94, 1991);
INSERT INTO reviews VALUES (5, 4, 'ACE Magazine', 920, 1989);
INSERT INTO reviews VALUES (6, 6, 'Amiga Power', 93, 1991);
INSERT INTO reviews VALUES (7, 6, 'Zero', 96, 1991);
INSERT INTO reviews VALUES (8, 8, 'Amiga Format', 91, 1995);
INSERT INTO reviews VALUES (9, 11, 'CU Amiga', 94, 1990);
INSERT INTO reviews VALUES (10, 12, 'Amiga Power', 88, 1993);
INSERT INTO reviews VALUES (11, 14, 'Amiga Format', 95, 1992);
INSERT INTO reviews VALUES (12, 15, 'Amiga Power', 90, 1993);
INSERT INTO reviews VALUES (13, 16, 'Zzap64', 97, 1986);
INSERT INTO reviews VALUES (14, 21, 'ACE Magazine', 910, 1989);
INSERT INTO reviews VALUES (15, 27, 'Amiga Format', 93, 1991);
INSERT INTO reviews VALUES (16, 28, 'Amiga Power', 91, 1992);
INSERT INTO reviews VALUES (17, 25, 'Amiga Joker', 89, 1993);
INSERT INTO reviews VALUES (18, 24, 'Amiga Format', 85, 1993);
INSERT INTO reviews VALUES (19, 3, 'Amiga Format', 87, 1993);
INSERT INTO reviews VALUES (20, 23, 'CU Amiga', 92, 1994);

-- ============================================================
-- VERIFICATION QUERIES
-- ============================================================

-- Show all tables
-- .tables

-- Count records
SELECT COUNT(*) FROM manufacturers;
SELECT COUNT(*) FROM platforms;
SELECT COUNT(*) FROM genres;
SELECT COUNT(*) FROM games;
SELECT COUNT(*) FROM reviews;

-- Sample queries
SELECT * FROM games WHERE genre_id = 1 ORDER BY release_year;
SELECT * FROM games WHERE manufacturer_id = 7;
SELECT AVG(score) FROM reviews;
SELECT MAX(release_year) FROM games;
SELECT MIN(release_year) FROM games;
