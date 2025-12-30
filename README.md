# AmiDB

**A SQLite-like Embedded Database for AmigaOS 3.1**

```
     _    __  __ ___ ____  ____
    / \  |  \/  |_ _|  _ \| __ )
   / _ \ | |\/| || || | | |  _ \
  / ___ \| |  | || || |_| | |_) |
 /_/   \_\_|  |_|___|____/|____/

 Embedded SQL Database for Amiga
```

AmiDB is a complete embedded database management system designed from the ground up for AmigaOS 3.1 running on Motorola 68000 processors. It brings modern database capabilities to classic Amiga hardware with full ACID transactions, B+Tree indexing, and SQL support.

---

## Motivation

The Amiga platform, despite its age, continues to have an active community of enthusiasts, developers, and retro computing hobbyists. However, there has never been a native, lightweight embedded database solution for AmigaOS 3.1 that:

- Works within the **4KB stack limit** of AmigaOS
- Runs efficiently on **2MB RAM** systems
- Provides **ACID transactions** for data integrity
- Supports a familiar **SQL interface**
- Uses **single-file databases** for easy management

AmiDB fills this gap by implementing a SQLite-inspired database engine specifically optimized for the constraints of the 68000 platform.

---

## Features

### Core Database Engine
- **Page-based storage** - 4KB pages with CRC32 checksums
- **B+Tree indexing** - O(log n) lookups on INTEGER PRIMARY KEY
- **LRU page cache** - 128-page cache (512KB) for performance
- **Row serialization** - Efficient binary format for records

### Transaction Support
- **Write-Ahead Logging (WAL)** - Atomic writes and crash recovery
- **Full ACID compliance** - Atomicity, Consistency, Isolation, Durability
- **Rollback on abort** - Clean transaction cancellation
- **Automatic recovery** - Recovers from crashes on database open

### SQL Support
- **CREATE TABLE** - With explicit or implicit PRIMARY KEY
- **DROP TABLE** - Remove tables from database
- **INSERT INTO** - Add records with VALUES clause
- **SELECT** - With WHERE, ORDER BY, LIMIT clauses
- **UPDATE** - Modify records with SET and WHERE
- **DELETE** - Remove records with WHERE clause

### Aggregate Functions
- **COUNT(*)** - Count all rows
- **COUNT(column)** - Count non-NULL values
- **SUM(column)** - Sum of INTEGER values
- **AVG(column)** - Average of INTEGER values
- **MIN(column)** - Minimum INTEGER value
- **MAX(column)** - Maximum INTEGER value

### Interactive Shell
- **REPL interface** - Interactive SQL command entry
- **Script execution** - Run .sql files automatically
- **Meta-commands** - .tables, .schema, .help, .quit
- **Result formatting** - Clean tabular output

### Data Types
- **INTEGER** - 32-bit signed integers
- **TEXT** - Variable-length strings
- **BLOB** - Binary data
- **NULL** - Missing values

---

## Requirements

### Target Platform
| Requirement | Specification |
|-------------|---------------|
| Operating System | AmigaOS 3.1 |
| Processor | Motorola 68000 or higher |
| Minimum RAM | 512KB |
| Recommended RAM | 2MB |
| Stack Size | 4KB (system default) |

### Development Platform
| Requirement | Specification |
|-------------|---------------|
| Cross-compiler | m68k-amigaos-gcc 6.5.0b |
| Build system | GNU Make |
| Testing | Amiberry emulator or real hardware |

---

## Quick Start

### Running the Shell

```bash
# Start with a new database in RAM
./amidb_shell RAM:mydb.db

# Start with an existing database
./amidb_shell DH0:data/store.db

# Run a SQL script then enter interactive mode
./amidb_shell RAM:gamedb.db sqlscripts/01_retro_game_history.sql
```

### Basic Usage

```sql
-- Create a table
CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER);

-- Insert data
INSERT INTO products VALUES (1, 'Amiga 500', 29900);
INSERT INTO products VALUES (2, 'Amiga 1200', 49900);

-- Query data
SELECT * FROM products;
SELECT * FROM products WHERE price > 30000;
SELECT * FROM products ORDER BY name;
SELECT * FROM products LIMIT 5;

-- Aggregate queries
SELECT COUNT(*) FROM products;
SELECT AVG(price) FROM products;
SELECT MIN(price) FROM products;
SELECT MAX(price) FROM products;

-- Update records
UPDATE products SET price = 27900 WHERE id = 1;

-- Delete records
DELETE FROM products WHERE id = 2;

-- Drop table
DROP TABLE products;

-- Shell commands
.tables           -- List all tables
.schema products  -- Show table structure
.help             -- Show help
.quit             -- Exit shell
```

---

## Project Structure

```
AmiDB/
├── src/                    # Source code
│   ├── api/                # Error handling and public API
│   │   └── error.c/h
│   ├── os/                 # AmigaOS abstraction layer
│   │   ├── file.h          # File operations interface
│   │   ├── file_amiga.c    # AmigaOS implementation
│   │   ├── mem.h           # Memory operations interface
│   │   └── mem_amiga.c     # AmigaOS implementation
│   ├── util/               # Utility functions
│   │   ├── crc32.c/h       # CRC32 checksums
│   │   ├── hash.c/h        # String hashing
│   │   └── endian.c/h      # Byte order conversion
│   ├── storage/            # Storage engine
│   │   ├── pager.c/h       # Page-based file I/O
│   │   ├── cache.c/h       # LRU page cache
│   │   ├── row.c/h         # Row serialization
│   │   └── btree.c/h       # B+Tree implementation
│   ├── txn/                # Transaction management
│   │   ├── wal.c/h         # Write-Ahead Logging
│   │   └── txn.c/h         # Transaction context
│   ├── sql/                # SQL processing
│   │   ├── lexer.c/h       # SQL tokenization
│   │   ├── parser.c/h      # SQL parsing
│   │   ├── catalog.c/h     # Table metadata
│   │   ├── executor.c/h    # Query execution
│   │   └── repl.c/h        # Interactive shell
│   └── shell_main.c        # Shell entry point
│
├── tests/                  # Test suite (133 tests)
│   ├── test_main.c         # Test runner
│   ├── test_harness.h      # Test framework
│   ├── test_pager.c        # Pager tests
│   ├── test_cache.c        # Cache tests
│   ├── test_row.c          # Row format tests
│   ├── test_btree_*.c      # B+Tree tests
│   ├── test_wal.c          # WAL tests
│   ├── test_txn.c          # Transaction tests
│   ├── test_recovery.c     # Recovery tests
│   ├── test_sql_lexer.c    # Lexer tests
│   ├── test_sql_parser.c   # Parser tests
│   ├── test_sql_catalog.c  # Catalog tests
│   ├── test_sql_e2e.c      # End-to-end SQL tests
│   └── test_sql_phase5.c   # Phase 5 (aggregates) tests
│
├── examples/               # Example programs
│   ├── inventory_demo.c    # Comprehensive API demo
│   └── README.md           # Examples documentation
│
├── sqlscripts/             # SQL migration scripts
│   ├── 01_retro_game_history.sql  # Simple database (5 tables)
│   ├── 02_retro_game_store.sql    # Complex database (11 tables)
│   └── README.md           # Scripts documentation
│
├── docs/                   # Documentation
│   ├── 01-Cross-Compile-Setup.md   # Development environment
│   ├── 02-AmiDB-Library-Usage.md   # Library API guide
│   └── 03-AmiDB-Shell-Guide.md     # Shell user guide
│
├── obj/                    # Compiled object files
├── Makefile                # Build system
├── README.md               # This file
└── PROGRESS.md             # Development progress
```

---

## Binaries

### amidb_shell

The interactive SQL shell for AmiDB databases.

```bash
# Build
make amidb_shell

# Usage
./amidb_shell <database_file> [script.sql]
```

**Features:**
- Interactive REPL for SQL commands
- Script file execution
- Meta-commands for database inspection
- Clean result formatting
- Comment stripping in scripts

### amidb_tests

Comprehensive test suite with 133 tests covering all components.

```bash
# Build
make amidb_tests

# Run all tests
./amidb_tests
```

**Test Coverage:**
| Phase | Component | Tests |
|-------|-----------|-------|
| 1 | Storage Engine | 30 |
| 2 | Write-Ahead Logging | 12 |
| 3 | Transactions | 36 |
| 4 | SQL Parser/Executor | 36 |
| 5 | DROP TABLE/Aggregates | 19 |
| **Total** | | **133** |

---

## Examples

The `examples/` directory contains demonstration programs showing how to use AmiDB as an embedded library.

### inventory_demo.c

A comprehensive example demonstrating all AmiDB capabilities:

**Part A - Direct C API:**
1. Page allocation and I/O
2. Row serialization and deserialization
3. B+Tree operations (insert, search, delete, cursor)
4. Transactions (begin, commit, abort)

**Part B - SQL Interface:**
5. CREATE TABLE and INSERT
6. SELECT with WHERE clauses
7. Aggregate functions (COUNT, SUM, AVG, MIN, MAX)
8. UPDATE operations
9. DELETE operations
10. DROP TABLE

```bash
# Build the demo
make inventory_demo

# Run
./inventory_demo
```

See [examples/README.md](examples/README.md) for detailed documentation.

---

## SQL Scripts

The `sqlscripts/` directory contains ready-to-use SQL migration scripts that demonstrate AmiDB's capabilities and can populate empty databases.

### 01_retro_game_history.sql

A database cataloging classic video games from the 8-bit and 16-bit era.

| Table | Records | Description |
|-------|---------|-------------|
| manufacturers | 15 | Game publishers and developers |
| platforms | 10 | Gaming systems (Amiga, C64, NES, etc.) |
| genres | 10 | Game categories |
| games | 30 | The main game catalog |
| reviews | 20 | Game ratings from magazines |

```bash
./amidb_shell RAM:gamehistory.db sqlscripts/01_retro_game_history.sql
```

### 02_retro_game_store.sql

A comprehensive e-commerce database for a retro gaming store.

| Table | Records | Description |
|-------|---------|-------------|
| product_types | 17 | Categories and subcategories |
| conditions | 8 | Item condition grades |
| products | 30 | Inventory of items for sale |
| countries | 10 | Country reference |
| customers | 10 | Customer accounts |
| addresses | 13 | Shipping/billing addresses |
| orders | 12 | Customer orders |
| order_items | 33 | Line items within orders |
| payment_methods | 4 | Payment type reference |
| payments | 10 | Payment transactions |
| inventory_log | 16 | Stock movement tracking |

```bash
./amidb_shell RAM:gamestore.db sqlscripts/02_retro_game_store.sql
```

See [sqlscripts/README.md](sqlscripts/README.md) for sample queries and usage details.

---

## Documentation

Comprehensive documentation for developers is available in the `docs/` directory.

### 01-Cross-Compile-Setup.md

Complete guide to setting up a cross-compilation environment:
- Installing m68k-amigaos-gcc on Debian/Ubuntu
- Configuring Amiberry emulator for testing
- Development workflow and Makefile usage
- Debugging and troubleshooting

### 02-AmiDB-Library-Usage.md

Guide to using AmiDB as an embedded library:
- Direct C API usage (pager, cache, btree, row)
- SQL interface usage (lexer, parser, executor)
- Memory management patterns for 4KB stack
- Complete code examples

### 03-AmiDB-Shell-Guide.md

Detailed reference for the interactive shell:
- All meta-commands with examples
- Complete SQL syntax reference
- Aggregate functions documentation
- Troubleshooting common issues

---

## Building

### Prerequisites

Install the m68k-amigaos-gcc cross-compiler:

```bash
# Add Bebbo's repository (Debian/Ubuntu)
sudo add-apt-repository ppa:bebbo/amiga-gcc
sudo apt update
sudo apt install amiga-gcc

# Verify installation
m68k-amigaos-gcc --version
```

### Build Commands

```bash
# Clean build artifacts
make clean

# Build the SQL shell
make amidb_shell

# Build the test suite
make amidb_tests

# Build everything
make all

# Build the demo program
make inventory_demo
```

### Compiler Flags

The Makefile uses optimized settings for the 68000:

```makefile
CC = m68k-amigaos-gcc
CFLAGS = -m68000 -O2 -fomit-frame-pointer -fno-builtin -noixemul
```

| Flag | Purpose |
|------|---------|
| `-m68000` | Target base 68000 CPU |
| `-O2` | Optimization level 2 |
| `-fomit-frame-pointer` | Save stack space |
| `-fno-builtin` | No built-in functions |
| `-noixemul` | Native AmigaOS (no ixemul.library) |

---

## Technical Architecture

### Storage Engine

AmiDB uses a page-based storage model similar to SQLite:

```
┌─────────────────────────────────────────┐
│              Database File               │
├─────────────────────────────────────────┤
│  Page 0: File Header                     │
│  - Magic number                          │
│  - Page size (4096 bytes)                │
│  - Catalog B+Tree root                   │
├─────────────────────────────────────────┤
│  Page 1-N: B+Tree Nodes                  │
│  - Internal nodes (keys + child ptrs)    │
│  - Leaf nodes (keys + data page refs)    │
├─────────────────────────────────────────┤
│  Page N+1...: Data Pages                 │
│  - Serialized row data                   │
│  - CRC32 checksums                       │
└─────────────────────────────────────────┘
```

### B+Tree Structure

All data is indexed using B+Trees with INTEGER keys:

```
                    ┌─────────────┐
                    │  Root Node  │
                    │  [50, 100]  │
                    └─────┬───────┘
              ┌───────────┼───────────┐
              ▼           ▼           ▼
        ┌─────────┐ ┌─────────┐ ┌─────────┐
        │ Leaf 1  │ │ Leaf 2  │ │ Leaf 3  │
        │ 10→pg5  │ │ 50→pg8  │ │100→pg12 │
        │ 25→pg6  │ │ 75→pg9  │ │150→pg15 │
        │ 40→pg7  │ │ 90→pg11 │ │175→pg18 │
        └─────────┘ └─────────┘ └─────────┘
```

### Transaction Model

AmiDB implements Write-Ahead Logging for ACID compliance:

```
┌─────────────────────────────────────────────────┐
│                 Transaction Flow                 │
├─────────────────────────────────────────────────┤
│                                                  │
│  BEGIN ──► Operations ──► WAL Write ──► COMMIT   │
│                │                                 │
│                └──────────► ABORT ──► Rollback   │
│                                                  │
└─────────────────────────────────────────────────┘

Recovery on Crash:
1. Open database
2. Check WAL for uncommitted transactions
3. Rollback incomplete transactions
4. Apply committed transactions
5. Checkpoint WAL to main database
```

### Memory Strategy

Due to the 4KB stack limit, AmiDB uses static allocation:

```c
// WRONG - Will overflow 4KB stack
void query() {
    struct table_schema schema;  // 2300 bytes
    uint8_t buffer[4096];        // 4096 bytes - CRASH!
}

// CORRECT - Static allocation
void query() {
    static struct table_schema schema;
    static uint8_t buffer[4096];
    // Stack usage: only pointers
}
```

---

## SQL Reference

### Data Definition

```sql
-- Create table with explicit PRIMARY KEY
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name TEXT,
    email TEXT
);

-- Create table with implicit rowid
CREATE TABLE logs (
    message TEXT,
    timestamp INTEGER
);

-- Drop a table
DROP TABLE users;
```

### Data Manipulation

```sql
-- Insert a row
INSERT INTO users VALUES (1, 'Alice', 'alice@example.com');

-- Update rows
UPDATE users SET email = 'alice@new.com' WHERE id = 1;
UPDATE users SET name = 'Administrator' WHERE name = 'admin';

-- Delete rows
DELETE FROM users WHERE id = 1;
DELETE FROM users WHERE name = 'test';
```

### Queries

```sql
-- Select all
SELECT * FROM users;

-- Select with WHERE
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE name = 'Alice';
SELECT * FROM users WHERE id > 10;

-- Select with ORDER BY
SELECT * FROM users ORDER BY name;
SELECT * FROM users ORDER BY id DESC;

-- Select with LIMIT
SELECT * FROM users LIMIT 10;

-- Combined
SELECT * FROM users WHERE id > 5 ORDER BY name LIMIT 20;
```

### Aggregate Functions

```sql
-- Count all rows
SELECT COUNT(*) FROM users;

-- Count with WHERE
SELECT COUNT(*) FROM users WHERE id > 10;

-- Sum values
SELECT SUM(price) FROM products;

-- Average value
SELECT AVG(score) FROM grades;

-- Minimum/Maximum
SELECT MIN(price) FROM products;
SELECT MAX(score) FROM grades;
```

### Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `=` | Equal | `WHERE id = 5` |
| `!=` | Not equal | `WHERE status != 0` |
| `<` | Less than | `WHERE price < 100` |
| `>` | Greater than | `WHERE qty > 10` |
| `<=` | Less or equal | `WHERE age <= 30` |
| `>=` | Greater or equal | `WHERE score >= 75` |

---

## Performance

### Typical Operations (Amiga 500, 7MHz 68000)

| Operation | Time |
|-----------|------|
| INSERT (single row) | 10-50ms |
| SELECT by PRIMARY KEY | 5-20ms |
| SELECT full table scan | 2-5ms per row |
| UPDATE/DELETE | Similar to SELECT + write |
| Script (45 rows) | 2-3 seconds |

### B+Tree Complexity

| Operation | Complexity |
|-----------|------------|
| Search | O(log n) |
| Insert | O(log n) |
| Delete | O(log n) |
| Range scan | O(k) where k = result size |

---

## Limitations

### By Design

| Limitation | Value | Reason |
|------------|-------|--------|
| Primary key type | INTEGER only | B+Tree uses int32_t keys |
| Max columns | 32 per table | Row format limit |
| Max ORDER BY rows | 100 | In-memory sort buffer |
| Max SQL statement | 512 bytes | Input buffer size |
| Float/Real types | Not supported | 68000 has no FPU |
| JOIN operations | Not implemented | Future enhancement |

### Platform Constraints

| Constraint | Limit | Mitigation |
|------------|-------|------------|
| Stack size | 4KB | Static allocation patterns |
| RAM | 2MB | LRU cache, efficient storage |
| CPU | 68000 big-endian | Native byte order |

---

## Development Status

**Current Version:** Phase 5 In Progress

| Phase | Status | Description |
|-------|--------|-------------|
| Phase 1 | Complete | Storage engine, pager, cache, B+Tree |
| Phase 2 | Complete | Write-Ahead Logging |
| Phase 3 | Complete | ACID transactions |
| Phase 4 | Complete | SQL parser, executor, REPL |
| Phase 5 | In Progress | DROP TABLE, aggregates |
| Phase 6 | Planned | JOIN, GROUP BY, HAVING |
| Phase 7 | Planned | Optimization, secondary indexes |

**Tests:** 133/133 passing

See [PROGRESS.md](PROGRESS.md) for detailed development history.

---

## Contributing

Contributions are welcome! Areas of interest:

- **JOIN implementation** - INNER, LEFT, RIGHT joins
- **GROUP BY / HAVING** - Grouped aggregates
- **Secondary indexes** - CREATE INDEX support
- **Query optimizer** - Cost-based planning
- **Additional platforms** - Other 68k systems

### Code Style

- Use static allocation for large structures
- Keep functions small (stack usage awareness)
- Comment complex algorithms
- Add tests for new features
- Follow existing naming conventions

---

## License

[To be determined]

---

## Acknowledgments

- The Amiga community for keeping the platform alive
- Bebbo for the m68k-amigaos-gcc toolchain
- SQLite for architectural inspiration
- All retro computing enthusiasts

---

## Contact

For questions, bug reports, or contributions, please open an issue on GitHub.

---

*AmiDB - Bringing modern database capabilities to classic Amiga hardware.*
