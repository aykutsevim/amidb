# AmiDB — Embedded Database Engine for Amiga Workbench 3.1

**Version:** 1.0  
**Target Platform:** AmigaOS 3.1 (Motorola 68000)  
**Document Revision:** December 2025

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Project Goals & Philosophy](#2-project-goals--philosophy)
3. [Target Platform Analysis](#3-target-platform-analysis)
4. [Feature Specification](#4-feature-specification)
5. [Data Model & Type System](#5-data-model--type-system)
6. [SQL Language Specification](#6-sql-language-specification)
7. [INNER JOIN Implementation](#7-inner-join-implementation)
8. [Architecture Design](#8-architecture-design)
9. [File Format Specification](#9-file-format-specification)
10. [Memory Management Strategy](#10-memory-management-strategy)
11. [B+Tree Engine Design](#11-btree-engine-design)
12. [Transaction & WAL System](#12-transaction--wal-system)
13. [Query Execution Engine](#13-query-execution-engine)
14. [C API Reference](#14-c-api-reference)
15. [Development Guidelines](#15-development-guidelines)
16. [Testing Strategy](#16-testing-strategy)
17. [AI Agent Implementation Plan](#17-ai-agent-implementation-plan)
18. [Appendices](#18-appendices)

---

## 1. Executive Summary

AmiDB is a lightweight, embedded database management system designed specifically for AmigaOS 3.1 running on Motorola 68000-series processors. The system prioritizes reliability, minimal resource consumption, and compatibility with the constraints of classic Amiga hardware.

### Key Characteristics

| Attribute | Specification |
|-----------|---------------|
| Database Model | Relational (simplified) |
| Storage | Single-file, page-based |
| Maximum DB Size | 2 GB (32-bit file offsets) |
| Minimum RAM | 512 KB |
| Recommended RAM | 2 MB+ |
| Concurrency | Single-writer, multi-reader |
| SQL Support | Subset (CREATE, INSERT, SELECT, DELETE, INNER JOIN) |
| Transaction Model | ACID-compliant with WAL |

---

## 2. Project Goals & Philosophy

### 2.1 Core Principles

1. **Simplicity Over Features**  
   Every feature must justify its memory and complexity cost. The Amiga's limited resources demand ruthless prioritization.

2. **Reliability First**  
   Data integrity is non-negotiable. The system must survive unexpected power loss, crashes, and disk errors gracefully.

3. **No External Dependencies**  
   AmiDB must compile and run with only the standard AmigaOS 3.1 ROM and dos.library. No third-party libraries, no C runtime beyond basic functions.

4. **Hackable Codebase**  
   The code should be readable by a competent C programmer in a weekend. No clever tricks that obscure intent.

5. **Big-Endian Native, Little-Endian Storage**  
   While the 68k is big-endian, disk format uses little-endian for cross-platform tool compatibility.

### 2.2 Non-Goals (Explicitly Out of Scope)

- Multi-table JOINs beyond INNER JOIN on two tables
- Subqueries
- Views, triggers, stored procedures
- User authentication / access control
- Network/client-server mode
- Full-text search
- JSON/XML support
- Query optimizer (beyond basic heuristics)
- Floating-point indexing

---

## 3. Target Platform Analysis

### 3.1 AmigaOS 3.1 Environment

**Hardware Constraints:**

| Component | Typical Specification | Design Impact |
|-----------|----------------------|---------------|
| CPU | 68000 @ 7 MHz (minimum) | No 64-bit atomics, slow division |
| RAM | 512 KB - 8 MB typical | Fixed buffer pools, no dynamic growth |
| Storage | Floppy (880 KB), HDD (20-500 MB) | Small page sizes, sequential I/O preferred |
| MMU | Optional (68030+) | No memory protection assumed |
| FPU | Optional (68881/68882) | Avoid floating-point in core paths |

**Software Environment:**

- **exec.library**: Memory allocation (AllocMem/FreeMem)
- **dos.library**: File I/O (Open/Close/Read/Write/Seek)
- **No POSIX**: No standard C library file I/O, no mmap(), no threads

### 3.2 Critical Platform Constraints

1. **Stack Limitations**  
   Default 4 KB stack on AmigaOS. Deep recursion is dangerous. B+Tree traversal must be iterative.

2. **Memory Fragmentation**  
   Long-running programs fragment chip/fast RAM. Pre-allocate all buffers at startup.

3. **No Memory Protection**  
   A wild pointer can corrupt the entire system. Defensive coding is essential.

4. **File System Quirks**  
   - FFS maximum file size: 2 GB (theoretical), practical limits lower
   - Seek() uses signed 32-bit offset
   - No sparse files
   - No file locking primitives (must implement in DB layer)

### 3.3 Compiler Considerations

**Recommended Compilers:**
- VBCC (modern, actively maintained)
- SAS/C 6.x (period-appropriate)
- GCC (cross-compilation from Linux)

**Required Compiler Flags:**
```
-c99        # or -ansi for stricter compatibility
-O2         # Optimize for size and speed
-m68000     # Generate pure 68000 code
-fno-builtin # Don't assume C library functions exist
```

---

## 4. Feature Specification

### 4.1 Storage Features

| Feature | Description | Priority |
|---------|-------------|----------|
| Single-file database | All data, indexes, WAL in one file | P0 |
| Page-based storage | Fixed 4096-byte pages | P0 |
| Page checksums | CRC32 per page for corruption detection | P0 |
| Automatic recovery | WAL replay on unclean shutdown | P0 |
| Compaction | Reclaim space from deleted records | P1 |
| Encryption | Not supported | N/A |

### 4.2 Data Features

| Feature | Description | Priority |
|---------|-------------|----------|
| Tables | Named collections with schema | P0 |
| Primary keys | Unique, indexed, required | P0 |
| INTEGER type | 32-bit signed | P0 |
| TEXT type | UTF-8, variable length | P0 |
| BLOB type | Binary, variable length | P0 |
| NULL values | Supported | P0 |
| FLOAT type | 32-bit IEEE 754 | P2 |
| Auto-increment | Integer primary keys | P1 |

### 4.3 Query Features

| Feature | Description | Priority |
|---------|-------------|----------|
| CREATE TABLE | Define table schema | P0 |
| DROP TABLE | Remove table | P0 |
| INSERT | Add rows | P0 |
| SELECT * | Retrieve all columns | P0 |
| SELECT columns | Retrieve specific columns | P0 |
| WHERE = | Equality filter | P0 |
| WHERE <, >, <=, >= | Range filters | P0 |
| WHERE AND/OR | Boolean combinations | P1 |
| INNER JOIN | Two-table join | P0 |
| DELETE | Remove rows | P0 |
| UPDATE | Modify rows | P1 |
| ORDER BY | Sort results | P1 |
| LIMIT | Restrict result count | P1 |

### 4.4 Transaction Features

| Feature | Description | Priority |
|---------|-------------|----------|
| BEGIN/COMMIT/ROLLBACK | Explicit transactions | P0 |
| Auto-commit | Single statements | P0 |
| Crash recovery | Automatic WAL replay | P0 |
| Savepoints | Not supported | N/A |
| Nested transactions | Not supported | N/A |

---

## 5. Data Model & Type System

### 5.1 Type Definitions

```c
typedef enum {
    AMIDB_TYPE_NULL    = 0,
    AMIDB_TYPE_INTEGER = 1,  /* 32-bit signed */
    AMIDB_TYPE_TEXT    = 2,  /* UTF-8, null-terminated */
    AMIDB_TYPE_BLOB    = 3,  /* raw bytes */
    AMIDB_TYPE_FLOAT   = 4   /* 32-bit IEEE 754 (phase 2) */
} amidb_type_t;
```

### 5.2 Value Storage Format

```c
struct amidb_value {
    uint8_t  type;       /* amidb_type_t */
    uint8_t  reserved;
    uint16_t length;     /* for TEXT/BLOB; 0 for INTEGER/FLOAT */
    union {
        int32_t  i32;
        float    f32;
        uint8_t  data[1]; /* variable length */
    } u;
};
```

### 5.3 Row Format

Each row is stored as:

```
+--------+--------+--------+--------+-----+--------+
| RowID  | Col1   | Col2   | Col3   | ... | ColN   |
| (4B)   | Value  | Value  | Value  |     | Value  |
+--------+--------+--------+--------+-----+--------+
```

- RowID: 32-bit auto-incrementing integer (primary key if not specified)
- Each column stored as amidb_value structure
- Variable-length fields (TEXT/BLOB) stored inline up to 256 bytes, overflow pages for larger

### 5.4 Table Schema Storage

```c
struct amidb_column_def {
    char     name[32];      /* column name */
    uint8_t  type;          /* amidb_type_t */
    uint8_t  flags;         /* PRIMARY_KEY, NOT_NULL, AUTO_INCREMENT */
    uint16_t max_length;    /* for TEXT/BLOB limits */
};

struct amidb_table_def {
    char     name[64];      /* table name */
    uint32_t column_count;
    uint32_t row_count;
    uint32_t btree_root;    /* page number of B+Tree root */
    struct amidb_column_def columns[16]; /* max 16 columns */
};
```

---

## 6. SQL Language Specification

### 6.1 Grammar (EBNF)

```ebnf
statement     = create_stmt | drop_stmt | insert_stmt | select_stmt | delete_stmt ;

create_stmt   = "CREATE" "TABLE" table_name "(" column_defs ")" ;
column_defs   = column_def { "," column_def } ;
column_def    = column_name type_name [ "PRIMARY" "KEY" ] [ "NOT" "NULL" ] ;
type_name     = "INTEGER" | "TEXT" [ "(" number ")" ] | "BLOB" [ "(" number ")" ] ;

drop_stmt     = "DROP" "TABLE" table_name ;

insert_stmt   = "INSERT" "INTO" table_name [ "(" column_list ")" ] 
                "VALUES" "(" value_list ")" ;
column_list   = column_name { "," column_name } ;
value_list    = value { "," value } ;
value         = number | string | "NULL" ;

select_stmt   = "SELECT" ( "*" | column_list ) 
                "FROM" table_ref
                [ join_clause ]
                [ "WHERE" condition ]
                [ "ORDER" "BY" column_name [ "ASC" | "DESC" ] ]
                [ "LIMIT" number ] ;

table_ref     = table_name [ alias ] ;
alias         = [ "AS" ] identifier ;

join_clause   = "INNER" "JOIN" table_ref "ON" join_condition ;
join_condition= column_ref "=" column_ref ;
column_ref    = [ table_name "." ] column_name ;

condition     = simple_cond { ( "AND" | "OR" ) simple_cond } ;
simple_cond   = column_ref compare_op value 
              | column_ref "IS" [ "NOT" ] "NULL" ;
compare_op    = "=" | "<>" | "<" | ">" | "<=" | ">=" ;

delete_stmt   = "DELETE" "FROM" table_name [ "WHERE" condition ] ;

table_name    = identifier ;
column_name   = identifier ;
identifier    = letter { letter | digit | "_" } ;
number        = [ "-" ] digit { digit } ;
string        = "'" { any_char } "'" ;
```

### 6.2 SQL Examples

```sql
-- Create tables
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name TEXT(64) NOT NULL,
    email TEXT(128)
);

CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    product TEXT(64),
    amount INTEGER
);

-- Insert data
INSERT INTO users (id, name, email) VALUES (1, 'Alice', 'alice@example.com');
INSERT INTO orders (id, user_id, product, amount) VALUES (100, 1, 'Widget', 5);

-- Simple select
SELECT * FROM users WHERE id = 1;
SELECT name, email FROM users;

-- INNER JOIN
SELECT users.name, orders.product, orders.amount 
FROM users 
INNER JOIN orders ON users.id = orders.user_id 
WHERE orders.amount > 3;

-- Delete
DELETE FROM orders WHERE id = 100;
```

### 6.3 Lexer Token Types

```c
typedef enum {
    TOK_EOF = 0,
    
    /* Keywords */
    TOK_SELECT, TOK_FROM, TOK_WHERE, TOK_INSERT, TOK_INTO,
    TOK_VALUES, TOK_DELETE, TOK_CREATE, TOK_DROP, TOK_TABLE,
    TOK_INNER, TOK_JOIN, TOK_ON, TOK_AND, TOK_OR, TOK_NOT,
    TOK_NULL, TOK_IS, TOK_PRIMARY, TOK_KEY, TOK_INTEGER,
    TOK_TEXT, TOK_BLOB, TOK_AS, TOK_ORDER, TOK_BY, TOK_ASC,
    TOK_DESC, TOK_LIMIT,
    
    /* Literals */
    TOK_IDENT,    /* identifier */
    TOK_NUMBER,   /* integer literal */
    TOK_STRING,   /* 'string literal' */
    
    /* Operators */
    TOK_EQ,       /* = */
    TOK_NE,       /* <> */
    TOK_LT,       /* < */
    TOK_GT,       /* > */
    TOK_LE,       /* <= */
    TOK_GE,       /* >= */
    TOK_STAR,     /* * */
    TOK_COMMA,    /* , */
    TOK_DOT,      /* . */
    TOK_LPAREN,   /* ( */
    TOK_RPAREN,   /* ) */
    
    TOK_ERROR
} token_type_t;
```

---

## 7. INNER JOIN Implementation

### 7.1 Join Strategy Overview

Given the memory constraints of the Amiga platform, AmiDB implements a **Nested Loop Join** with optional **Index Lookup** optimization when the join column is indexed.

**Algorithm Selection Matrix:**

| Left Table Size | Right Table Indexed | Strategy |
|-----------------|---------------------|----------|
| Any | Yes | Index Nested Loop |
| Small (< 1000 rows) | No | Simple Nested Loop |
| Large | No | Block Nested Loop |

### 7.2 Nested Loop Join Algorithm

```
NESTED_LOOP_JOIN(left_table, right_table, join_condition):
    result = []
    for each row L in left_table:
        for each row R in right_table:
            if join_condition(L, R):
                result.append(combine(L, R))
    return result
```

**Complexity:** O(n × m) where n, m are table sizes
**Memory:** O(page_size) — only need one row from each table at a time

### 7.3 Index Nested Loop Join Algorithm

When the join column on the right table has a B+Tree index:

```
INDEX_NESTED_LOOP_JOIN(left_table, right_table, join_col_left, join_col_right):
    result = []
    right_index = get_index(right_table, join_col_right)
    
    for each row L in left_table:
        key = L[join_col_left]
        matching_rows = right_index.lookup(key)
        for each row R in matching_rows:
            result.append(combine(L, R))
    
    return result
```

**Complexity:** O(n × log(m)) with index
**Memory:** O(page_size + tree_height × page_size) for index traversal stack

### 7.4 Block Nested Loop Join (Memory-Efficient)

For large tables without indexes, process in blocks:

```
BLOCK_NESTED_LOOP_JOIN(left_table, right_table, join_condition, block_size):
    result = []
    
    for each block B_left of block_size rows from left_table:
        for each row R in right_table:
            for each row L in B_left:
                if join_condition(L, R):
                    result.append(combine(L, R))
    
    return result
```

**Block Size Calculation:**
```c
#define JOIN_BUFFER_SIZE  (32 * 1024)  /* 32 KB join buffer */
block_size = JOIN_BUFFER_SIZE / avg_row_size;
```

### 7.5 Join Execution Plan

The query executor builds a plan for JOIN queries:

```c
struct join_plan {
    struct table_scan  *left_scan;    /* how to scan left table */
    struct table_scan  *right_scan;   /* how to scan right table */
    
    uint8_t   left_join_col;          /* column index for join */
    uint8_t   right_join_col;
    
    enum {
        JOIN_NESTED_LOOP,
        JOIN_INDEX_NESTED_LOOP,
        JOIN_BLOCK_NESTED_LOOP
    } strategy;
    
    uint32_t  right_index_root;       /* B+Tree root if indexed */
    uint32_t  block_size;             /* for block nested loop */
};
```

### 7.6 Result Materialization

JOIN results are materialized as temporary rows:

```c
struct join_result_row {
    uint32_t  left_rowid;
    uint32_t  right_rowid;
    uint8_t   data[1];  /* combined column values */
};
```

**Memory Management:**
- Results are yielded one at a time (iterator pattern)
- No full result materialization in memory
- Caller processes each row before requesting next

### 7.7 Join Column Resolution

Handling qualified column names (table.column):

```c
struct column_ref {
    char table_alias[32];   /* empty if unqualified */
    char column_name[32];
    
    /* Resolved during planning: */
    uint8_t table_index;    /* 0 = left, 1 = right */
    uint8_t column_index;   /* index in table schema */
};

int resolve_column_ref(
    struct column_ref *ref,
    struct table_def *left,
    const char *left_alias,
    struct table_def *right,
    const char *right_alias
);
```

### 7.8 Join Optimization Heuristics

Simple cost-based selection:

```c
enum join_strategy select_join_strategy(
    struct table_def *left,
    struct table_def *right,
    uint8_t right_join_col
) {
    /* Check if right join column is indexed */
    if (has_index(right, right_join_col)) {
        return JOIN_INDEX_NESTED_LOOP;
    }
    
    /* Small tables: simple nested loop */
    if (left->row_count < 1000 && right->row_count < 1000) {
        return JOIN_NESTED_LOOP;
    }
    
    /* Large tables: block nested loop */
    return JOIN_BLOCK_NESTED_LOOP;
}
```

---

## 8. Architecture Design

### 8.1 System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         APPLICATION                              │
│                    (Amiga Program using AmiDB)                   │
└─────────────────────────────────┬───────────────────────────────┘
                                  │ C API
                                  ▼
┌─────────────────────────────────────────────────────────────────┐
│                          PUBLIC API                              │
│              db_open, db_exec, db_prepare, db_step               │
│                     db_close, db_error                           │
└─────────────────────────────────┬───────────────────────────────┘
                                  │
          ┌───────────────────────┼───────────────────────┐
          ▼                       ▼                       ▼
┌─────────────────┐   ┌─────────────────────┐   ┌─────────────────┐
│   SQL PARSER    │   │  QUERY EXECUTOR     │   │   TRANSACTION   │
│                 │   │                     │   │    MANAGER      │
│ - Lexer         │   │ - Plan Generator    │   │                 │
│ - Parser        │   │ - Scan Operators    │   │ - Begin/Commit  │
│ - AST Builder   │   │ - Join Operators    │   │ - Rollback      │
│                 │   │ - Filter Operators  │   │ - Savepoints    │
└────────┬────────┘   └──────────┬──────────┘   └────────┬────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                        STORAGE ENGINE                            │
│                                                                  │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │   CATALOG       │  │   B+TREE        │  │   ROW STORE     │  │
│  │   MANAGER       │  │   ENGINE        │  │                 │  │
│  │                 │  │                 │  │ - Serialize     │  │
│  │ - Table defs    │  │ - Insert        │  │ - Deserialize   │  │
│  │ - Column defs   │  │ - Search        │  │ - Overflow      │  │
│  │ - Index defs    │  │ - Delete        │  │                 │  │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘  │
│           │                    │                    │           │
│           └────────────────────┼────────────────────┘           │
│                                ▼                                 │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                      PAGE CACHE                              ││
│  │                                                              ││
│  │  - LRU eviction          - Dirty page tracking              ││
│  │  - Pin/unpin pages       - Page reference counting          ││
│  │  - Fixed buffer pool     - Write-back on eviction           ││
│  └─────────────────────────────┬───────────────────────────────┘│
│                                │                                 │
│  ┌─────────────────────────────▼───────────────────────────────┐│
│  │                      PAGER                                   ││
│  │                                                              ││
│  │  - Page allocation       - Free list management             ││
│  │  - Page checksums        - Page number ↔ file offset        ││
│  └─────────────────────────────┬───────────────────────────────┘│
└────────────────────────────────┼────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                      WAL (Write-Ahead Log)                       │
│                                                                  │
│  - Append-only log          - Checkpoint management             │
│  - Redo records             - Crash recovery                    │
│  - Fsync on commit          - Log truncation                    │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                      FILE I/O LAYER                              │
│                                                                  │
│  - AmigaOS dos.library      - Buffered writes                   │
│  - Endian conversion        - Error handling                    │
│  - Portable abstraction     - File locking (advisory)           │
└─────────────────────────────────────────────────────────────────┘
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │   DATABASE FILE        │
                    │   (single .adb file)   │
                    └────────────────────────┘
```

### 8.2 Module Decomposition

```
src/
├── api/
│   ├── amidb.h           # Public API header
│   ├── amidb.c           # API implementation
│   └── error.c           # Error handling
│
├── sql/
│   ├── lexer.c           # SQL tokenizer
│   ├── lexer.h
│   ├── parser.c          # SQL parser
│   ├── parser.h
│   ├── ast.c             # AST node management
│   └── ast.h
│
├── exec/
│   ├── executor.c        # Query executor
│   ├── executor.h
│   ├── planner.c         # Query planner
│   ├── planner.h
│   ├── scan.c            # Table scan operators
│   ├── join.c            # Join operators (INNER JOIN)
│   └── filter.c          # WHERE clause evaluation
│
├── storage/
│   ├── pager.c           # Page management
│   ├── pager.h
│   ├── cache.c           # Page cache (LRU)
│   ├── cache.h
│   ├── btree.c           # B+Tree implementation
│   ├── btree.h
│   ├── row.c             # Row serialization
│   ├── row.h
│   ├── catalog.c         # System catalog
│   └── catalog.h
│
├── txn/
│   ├── wal.c             # Write-ahead log
│   ├── wal.h
│   ├── txn.c             # Transaction manager
│   └── txn.h
│
├── os/
│   ├── file_amiga.c      # AmigaOS file I/O
│   ├── file_posix.c      # POSIX file I/O (dev/test)
│   ├── file.h            # Portable file interface
│   ├── mem_amiga.c       # AmigaOS memory
│   ├── mem_posix.c       # POSIX memory
│   └── mem.h             # Portable memory interface
│
├── util/
│   ├── endian.c          # Endian conversion
│   ├── endian.h
│   ├── crc32.c           # CRC32 checksums
│   ├── crc32.h
│   ├── hash.c            # String hashing
│   └── hash.h
│
└── main.c                # CLI tool (optional)
```

### 8.3 Data Flow: SELECT with INNER JOIN

```
User Query:
  SELECT users.name, orders.amount 
  FROM users 
  INNER JOIN orders ON users.id = orders.user_id 
  WHERE orders.amount > 10

     │
     ▼
┌─────────────┐
│   LEXER     │ → Token stream: [SELECT, users, DOT, name, ...]
└─────────────┘
     │
     ▼
┌─────────────┐
│   PARSER    │ → AST: SelectStmt { 
└─────────────┘         columns: [users.name, orders.amount],
     │                   from: TableRef("users"),
     │                   join: JoinClause(INNER, "orders", 
     │                          Condition(users.id = orders.user_id)),
     │                   where: Condition(orders.amount > 10)
     ▼                 }
┌─────────────┐
│   PLANNER   │ → Plan: {
└─────────────┘     left_scan: SeqScan("users"),
     │              right_scan: IndexScan("orders", "user_id"),
     │              join_strategy: INDEX_NESTED_LOOP,
     │              filter: (orders.amount > 10),
     │              project: [0.name, 1.amount]
     ▼           }
┌─────────────┐
│  EXECUTOR   │ → For each row from left scan:
└─────────────┘       Lookup matching rows in right index
     │                 Apply WHERE filter
     │                 Project requested columns
     │                 Yield result row
     ▼
┌─────────────┐
│   B+TREE    │ → Navigate tree, return leaf entries
└─────────────┘
     │
     ▼
┌─────────────┐
│ PAGE CACHE  │ → Return cached pages or fetch from disk
└─────────────┘
     │
     ▼
┌─────────────┐
│   PAGER     │ → Convert page# to file offset, read page
└─────────────┘
     │
     ▼
┌─────────────┐
│  FILE I/O   │ → dos.library Read() call
└─────────────┘
```

---

## 9. File Format Specification

### 9.1 Database File Layout

```
Offset      Size        Description
──────────────────────────────────────────────────────────────
0x0000      4096        File Header (Page 0)
0x1000      4096        System Catalog Root (Page 1)
0x2000      4096        Free Page Bitmap (Page 2)
0x3000      ...         WAL Region (Pages 3-34, 128 KB)
0x23000     ...         Data/Index Pages (Page 35+)
```

### 9.2 File Header (Page 0)

```c
struct amidb_file_header {
    /* Offset 0x00 */
    char        magic[8];           /* "AMIDB01\0" */
    uint32_t    version;            /* Format version: 0x00010000 */
    uint32_t    page_size;          /* 4096 */
    
    /* Offset 0x10 */
    uint32_t    page_count;         /* Total pages in file */
    uint32_t    free_page_count;    /* Pages on free list */
    uint32_t    catalog_root;       /* Page# of catalog B+Tree */
    uint32_t    free_list_head;     /* First free page */
    
    /* Offset 0x20 */
    uint32_t    wal_start_page;     /* First WAL page */
    uint32_t    wal_page_count;     /* WAL region size */
    uint32_t    wal_head;           /* Current WAL write position */
    uint32_t    wal_tail;           /* Oldest un-checkpointed entry */
    
    /* Offset 0x30 */
    uint32_t    flags;              /* DB_FLAG_DIRTY, etc. */
    uint32_t    checksum;           /* CRC32 of header */
    uint64_t    last_txn_id;        /* Last committed transaction */
    
    /* Offset 0x40 */
    uint8_t     reserved[4016];     /* Pad to page size */
};

/* Flags */
#define DB_FLAG_DIRTY      0x0001   /* Unclean shutdown */
#define DB_FLAG_WAL_ACTIVE 0x0002   /* WAL has uncommitted data */
```

### 9.3 Page Header (All Data Pages)

```c
struct amidb_page_header {
    uint32_t    page_number;        /* Self-reference for validation */
    uint16_t    page_type;          /* PAGE_TYPE_* */
    uint16_t    flags;              /* Page-specific flags */
    uint32_t    checksum;           /* CRC32 of page content */
    uint32_t    lsn;                /* Log sequence number */
};

/* Page types */
#define PAGE_TYPE_FREE      0x0000
#define PAGE_TYPE_CATALOG   0x0001
#define PAGE_TYPE_BTREE_INT 0x0002  /* B+Tree internal node */
#define PAGE_TYPE_BTREE_LEAF 0x0003 /* B+Tree leaf node */
#define PAGE_TYPE_OVERFLOW  0x0004  /* Large value overflow */
#define PAGE_TYPE_FREEMAP   0x0005  /* Free page bitmap */
#define PAGE_TYPE_WAL       0x0006  /* WAL page */
```

### 9.4 B+Tree Node Format

**Internal Node:**
```
+──────────────────────────────────────────────────────────+
│ Page Header (16 bytes)                                   │
+──────────────────────────────────────────────────────────+
│ Node Header:                                             │
│   uint16_t  key_count                                    │
│   uint16_t  level           (0 = leaf)                   │
│   uint32_t  right_sibling   (page# or 0)                 │
+──────────────────────────────────────────────────────────+
│ Child Pointers: uint32_t[key_count + 1]                  │
+──────────────────────────────────────────────────────────+
│ Keys: { uint16_t len; uint8_t data[len]; }[key_count]    │
+──────────────────────────────────────────────────────────+
│ Free Space                                               │
+──────────────────────────────────────────────────────────+
```

**Leaf Node:**
```
+──────────────────────────────────────────────────────────+
│ Page Header (16 bytes)                                   │
+──────────────────────────────────────────────────────────+
│ Node Header:                                             │
│   uint16_t  entry_count                                  │
│   uint16_t  level = 0                                    │
│   uint32_t  next_leaf       (page# for range scan)       │
│   uint32_t  prev_leaf       (page# for reverse scan)     │
+──────────────────────────────────────────────────────────+
│ Cell Pointers: uint16_t[entry_count]  (offsets from end) │
+──────────────────────────────────────────────────────────+
│ Free Space                                               │
+──────────────────────────────────────────────────────────+
│ Cells (grow from end of page):                           │
│   { uint16_t key_len; uint8_t key[]; }                   │
│   { uint16_t val_len; uint8_t val[]; }                   │
│   OR                                                     │
│   { uint32_t overflow_page; } if val_len > threshold     │
+──────────────────────────────────────────────────────────+
```

### 9.5 Endian Encoding

All multi-byte integers on disk are **little-endian**:

```c
/* Write 32-bit integer to buffer (little-endian) */
static inline void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* Read 32-bit integer from buffer (little-endian) */
static inline uint32_t get_u32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
```

---

## 10. Memory Management Strategy

### 10.1 Memory Pools

AmiDB uses fixed-size memory pools to avoid fragmentation:

```c
struct amidb_mem_config {
    uint32_t  page_cache_size;    /* Number of cached pages */
    uint32_t  wal_buffer_size;    /* WAL buffer in bytes */
    uint32_t  sql_arena_size;     /* Parser/AST arena */
    uint32_t  result_buffer_size; /* Query result buffer */
};

/* Default configuration for 1 MB RAM budget */
static const struct amidb_mem_config default_config = {
    .page_cache_size    = 64,      /* 64 × 4KB = 256 KB */
    .wal_buffer_size    = 32768,   /* 32 KB */
    .sql_arena_size     = 16384,   /* 16 KB */
    .result_buffer_size = 65536    /* 64 KB */
};
```

### 10.2 Page Cache Implementation

```c
struct cache_entry {
    uint32_t  page_num;       /* 0 = unused slot */
    uint32_t  pin_count;      /* > 0 = pinned, cannot evict */
    uint32_t  last_access;    /* Tick counter for LRU */
    uint8_t   dirty;          /* Needs write-back */
    uint8_t   data[PAGE_SIZE];
};

struct page_cache {
    struct cache_entry *entries;  /* Fixed array */
    uint32_t  entry_count;
    uint32_t  clock;              /* LRU clock */
};
```

### 10.3 Arena Allocator for SQL Processing

```c
struct arena {
    uint8_t  *base;
    uint32_t  size;
    uint32_t  used;
};

/* Fast bump allocation - no individual free */
void *arena_alloc(struct arena *a, uint32_t size) {
    size = (size + 3) & ~3;  /* 4-byte alignment */
    if (a->used + size > a->size) return NULL;
    void *p = a->base + a->used;
    a->used += size;
    return p;
}

/* Reset arena for next query */
void arena_reset(struct arena *a) {
    a->used = 0;
}
```

### 10.4 Memory Budget Breakdown

For 2 MB total allocation:

| Component | Size | Notes |
|-----------|------|-------|
| Page Cache | 512 KB | 128 pages × 4 KB |
| WAL Buffer | 64 KB | Double-buffered |
| SQL Arena | 32 KB | Parser + AST |
| Result Buffer | 128 KB | Query results |
| Join Buffer | 64 KB | Block nested loop |
| Catalog Cache | 32 KB | Table definitions |
| Misc Buffers | 64 KB | I/O, temp storage |
| **Total** | **896 KB** | Plus overhead |

---

## 11. B+Tree Engine Design

### 11.1 Tree Parameters

```c
#define BTREE_PAGE_SIZE     4096
#define BTREE_HEADER_SIZE   32
#define BTREE_MAX_KEY_SIZE  256
#define BTREE_MIN_KEYS      2    /* Minimum keys per node */

/* Calculate max keys per internal node */
/* Each entry: 4-byte child ptr + 2-byte key len + key data */
/* Assuming average key size of 32 bytes: */
/* (4096 - 32) / (4 + 2 + 32) ≈ 106 keys max */
```

### 11.2 Iterative Traversal (No Recursion)

```c
struct btree_cursor {
    struct amidb   *db;
    uint32_t        root_page;
    
    /* Path from root to current leaf */
    struct {
        uint32_t  page_num;
        uint16_t  index;      /* Position within node */
    } path[16];               /* Max tree depth */
    uint8_t         depth;
    
    /* Current position */
    uint32_t        current_page;
    uint16_t        current_index;
    
    /* Cached current key/value */
    uint8_t         key[BTREE_MAX_KEY_SIZE];
    uint16_t        key_len;
    uint8_t        *value;
    uint32_t        value_len;
};

/* Move cursor to first entry >= key */
int btree_seek(struct btree_cursor *c, 
               const uint8_t *key, uint16_t key_len);

/* Move to next entry */
int btree_next(struct btree_cursor *c);

/* Move to previous entry */
int btree_prev(struct btree_cursor *c);

/* Get current key/value (valid after seek/next/prev) */
int btree_current(struct btree_cursor *c,
                  const uint8_t **key, uint16_t *key_len,
                  const uint8_t **value, uint32_t *value_len);
```

### 11.3 Insert Algorithm

```c
int btree_insert(struct amidb *db, uint32_t root,
                 const uint8_t *key, uint16_t key_len,
                 const uint8_t *value, uint32_t value_len)
{
    struct btree_cursor cursor;
    btree_cursor_init(&cursor, db, root);
    
    /* 1. Find insertion point */
    int rc = btree_seek(&cursor, key, key_len);
    if (rc == AMIDB_EXISTS) {
        return AMIDB_EXISTS;  /* Duplicate key */
    }
    
    /* 2. Insert into leaf */
    rc = leaf_insert(&cursor, key, key_len, value, value_len);
    if (rc != AMIDB_OVERFLOW) {
        return rc;  /* Success or error */
    }
    
    /* 3. Split overflowing leaf */
    uint8_t split_key[BTREE_MAX_KEY_SIZE];
    uint16_t split_key_len;
    uint32_t new_page;
    
    rc = leaf_split(&cursor, split_key, &split_key_len, &new_page);
    if (rc != AMIDB_OK) return rc;
    
    /* 4. Propagate split up the tree */
    while (cursor.depth > 0) {
        cursor.depth--;
        rc = internal_insert(&cursor, split_key, split_key_len, new_page);
        if (rc != AMIDB_OVERFLOW) break;
        
        rc = internal_split(&cursor, split_key, &split_key_len, &new_page);
        if (rc != AMIDB_OK) return rc;
    }
    
    /* 5. Create new root if needed */
    if (rc == AMIDB_OVERFLOW) {
        rc = create_new_root(db, &root, split_key, split_key_len, new_page);
    }
    
    return rc;
}
```

### 11.4 Node Layout Helpers

```c
/* Get pointer to child page at index i in internal node */
static inline uint32_t get_child(const uint8_t *page, int i) {
    const uint8_t *ptr = page + BTREE_HEADER_SIZE + i * 4;
    return get_u32(ptr);
}

/* Binary search for key in node */
static int node_search(const uint8_t *page, 
                       const uint8_t *key, uint16_t key_len,
                       int *found)
{
    int lo = 0, hi = get_key_count(page) - 1;
    *found = 0;
    
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *mid_key;
        uint16_t mid_len;
        get_key(page, mid, &mid_key, &mid_len);
        
        int cmp = key_compare(key, key_len, mid_key, mid_len);
        if (cmp < 0) {
            hi = mid - 1;
        } else if (cmp > 0) {
            lo = mid + 1;
        } else {
            *found = 1;
            return mid;
        }
    }
    
    return lo;
}
```

---

## 12. Transaction & WAL System

### 12.1 Transaction States

```c
typedef enum {
    TXN_IDLE = 0,      /* No active transaction */
    TXN_ACTIVE,        /* Transaction in progress */
    TXN_COMMITTING,    /* Writing to WAL */
    TXN_ABORTING       /* Rolling back */
} txn_state_t;

struct amidb_txn {
    struct amidb   *db;
    uint64_t        txn_id;
    txn_state_t     state;
    uint32_t        wal_start;     /* Start of this txn in WAL */
    uint32_t        dirty_pages;   /* Count of modified pages */
};
```

### 12.2 WAL Record Format

```c
struct wal_record_header {
    uint32_t  record_size;     /* Total size including header */
    uint32_t  record_type;     /* WAL_* type */
    uint64_t  txn_id;
    uint32_t  lsn;             /* Log sequence number */
    uint32_t  checksum;        /* CRC32 of record */
};

/* Record types */
#define WAL_BEGIN     0x0001
#define WAL_COMMIT    0x0002
#define WAL_ABORT     0x0003
#define WAL_PAGE      0x0010   /* Full page image */
#define WAL_REDO      0x0011   /* Redo-only record */

struct wal_page_record {
    struct wal_record_header header;
    uint32_t  page_num;
    uint8_t   page_data[PAGE_SIZE];
};
```

### 12.3 Commit Protocol

```c
int txn_commit(struct amidb_txn *txn)
{
    struct amidb *db = txn->db;
    
    /* 1. Write all dirty pages to WAL */
    for (int i = 0; i < db->cache->entry_count; i++) {
        struct cache_entry *e = &db->cache->entries[i];
        if (e->dirty && e->txn_id == txn->txn_id) {
            int rc = wal_write_page(db, e->page_num, e->data);
            if (rc != AMIDB_OK) {
                txn_abort(txn);
                return rc;
            }
        }
    }
    
    /* 2. Write COMMIT record */
    int rc = wal_write_commit(db, txn->txn_id);
    if (rc != AMIDB_OK) {
        txn_abort(txn);
        return rc;
    }
    
    /* 3. Fsync WAL file */
    rc = file_sync(db->file);
    if (rc != AMIDB_OK) {
        return rc;  /* Commit is uncertain */
    }
    
    /* 4. Mark transaction committed */
    txn->state = TXN_IDLE;
    
    /* 5. Write dirty pages to main file (checkpoint) */
    rc = checkpoint_dirty_pages(db, txn->txn_id);
    
    return AMIDB_OK;
}
```

### 12.4 Crash Recovery

```c
int db_recover(struct amidb *db)
{
    /* 1. Check dirty flag */
    if (!(db->header->flags & DB_FLAG_DIRTY)) {
        return AMIDB_OK;  /* Clean shutdown */
    }
    
    /* 2. Scan WAL for committed transactions */
    struct wal_scanner scanner;
    wal_scanner_init(&scanner, db);
    
    while (wal_scanner_next(&scanner)) {
        if (scanner.record->record_type == WAL_COMMIT) {
            /* Mark transaction as needing redo */
            bitmap_set(db->redo_txns, scanner.record->txn_id);
        }
    }
    
    /* 3. Replay committed transactions */
    wal_scanner_reset(&scanner);
    
    while (wal_scanner_next(&scanner)) {
        if (scanner.record->record_type == WAL_PAGE) {
            if (bitmap_get(db->redo_txns, scanner.record->txn_id)) {
                /* Apply page image */
                struct wal_page_record *pr = 
                    (struct wal_page_record *)scanner.record;
                pager_write(db, pr->page_num, pr->page_data);
            }
        }
    }
    
    /* 4. Sync and clear WAL */
    file_sync(db->file);
    wal_truncate(db);
    
    /* 5. Clear dirty flag */
    db->header->flags &= ~DB_FLAG_DIRTY;
    pager_write(db, 0, db->header);
    file_sync(db->file);
    
    return AMIDB_OK;
}
```

---

## 13. Query Execution Engine

### 13.1 Execution Plan Nodes

```c
typedef enum {
    PLAN_SEQSCAN,      /* Sequential table scan */
    PLAN_INDEXSCAN,    /* B+Tree index scan */
    PLAN_FILTER,       /* WHERE clause filter */
    PLAN_PROJECT,      /* Column projection */
    PLAN_NESTEDLOOP,   /* Nested loop join */
    PLAN_SORT,         /* ORDER BY */
    PLAN_LIMIT,        /* LIMIT clause */
    PLAN_INSERT,       /* INSERT statement */
    PLAN_DELETE        /* DELETE statement */
} plan_node_type_t;

struct plan_node {
    plan_node_type_t  type;
    struct plan_node *left;     /* Child node */
    struct plan_node *right;    /* Second child (for joins) */
    
    union {
        struct {
            uint32_t table_id;
        } scan;
        
        struct {
            uint32_t table_id;
            uint32_t index_root;
            uint8_t  start_key[256];
            uint8_t  end_key[256];
        } indexscan;
        
        struct {
            struct condition *cond;
        } filter;
        
        struct {
            uint8_t column_mask[16];  /* Bitmap of columns */
        } project;
        
        struct {
            uint8_t left_col;
            uint8_t right_col;
            enum join_strategy strategy;
        } join;
    } u;
};
```

### 13.2 Iterator Interface

```c
/* All plan nodes implement this interface */
struct plan_iterator {
    struct plan_node *node;
    void *state;               /* Node-specific state */
    
    /* Returns AMIDB_OK and fills row, or AMIDB_DONE */
    int (*next)(struct plan_iterator *it, struct row *row);
    
    /* Cleanup */
    void (*close)(struct plan_iterator *it);
};

/* Create iterator for plan node */
struct plan_iterator *plan_execute(struct amidb *db, 
                                   struct plan_node *plan);
```

### 13.3 Join Iterator Implementation

```c
struct join_state {
    struct plan_iterator *left_iter;
    struct plan_iterator *right_iter;
    struct row left_row;
    struct row right_row;
    uint8_t left_col;
    uint8_t right_col;
    int left_exhausted;
    
    /* For index nested loop */
    struct btree_cursor *index_cursor;
    
    /* For block nested loop */
    struct row *block;
    int block_count;
    int block_pos;
};

static int join_next(struct plan_iterator *it, struct row *out)
{
    struct join_state *js = (struct join_state *)it->state;
    
    while (1) {
        /* Get next from right (inner) side */
        if (js->index_cursor) {
            /* Index nested loop */
            int rc = btree_next(js->index_cursor);
            if (rc == AMIDB_OK) {
                /* Check join condition */
                if (values_equal(&js->left_row.cols[js->left_col],
                                &js->right_row.cols[js->right_col])) {
                    combine_rows(&js->left_row, &js->right_row, out);
                    return AMIDB_OK;
                }
                continue;
            }
        } else {
            /* Simple nested loop */
            int rc = js->right_iter->next(js->right_iter, &js->right_row);
            if (rc == AMIDB_OK) {
                if (values_equal(&js->left_row.cols[js->left_col],
                                &js->right_row.cols[js->right_col])) {
                    combine_rows(&js->left_row, &js->right_row, out);
                    return AMIDB_OK;
                }
                continue;
            }
        }
        
        /* Right exhausted, advance left */
        int rc = js->left_iter->next(js->left_iter, &js->left_row);
        if (rc != AMIDB_OK) {
            return AMIDB_DONE;
        }
        
        /* Reset right side */
        if (js->index_cursor) {
            uint8_t *key = js->left_row.cols[js->left_col].data;
            uint16_t key_len = js->left_row.cols[js->left_col].length;
            btree_seek(js->index_cursor, key, key_len);
        } else {
            js->right_iter->close(js->right_iter);
            js->right_iter = plan_execute(it->db, it->node->right);
        }
    }
}
```

---

## 14. C API Reference

### 14.1 Core API

```c
/*
 * AmiDB - Embedded Database for AmigaOS
 * Public API Header
 */

#ifndef AMIDB_H
#define AMIDB_H

#include <exec/types.h>

/* Opaque types */
typedef struct amidb      amidb_t;
typedef struct amidb_stmt amidb_stmt_t;

/* Error codes */
#define AMIDB_OK          0
#define AMIDB_ERROR      -1
#define AMIDB_BUSY       -2
#define AMIDB_NOTFOUND   -3
#define AMIDB_EXISTS     -4
#define AMIDB_CORRUPT    -5
#define AMIDB_NOMEM      -6
#define AMIDB_IOERR      -7
#define AMIDB_FULL       -8
#define AMIDB_SYNTAX     -9
#define AMIDB_DONE       -10
#define AMIDB_ROW        -11

/* Open flags */
#define AMIDB_OPEN_READONLY   0x0001
#define AMIDB_OPEN_CREATE     0x0002

/*
 * Database connection
 */

/* Open database file */
int amidb_open(const char *path, int flags, amidb_t **db);

/* Close database connection */
int amidb_close(amidb_t *db);

/* Get last error message */
const char *amidb_errmsg(amidb_t *db);

/* Get last error code */
int amidb_errcode(amidb_t *db);

/*
 * SQL execution
 */

/* Execute SQL directly (no result) */
int amidb_exec(amidb_t *db, const char *sql);

/* Prepare SQL statement */
int amidb_prepare(amidb_t *db, const char *sql, amidb_stmt_t **stmt);

/* Finalize (free) statement */
int amidb_finalize(amidb_stmt_t *stmt);

/* Reset statement for re-execution */
int amidb_reset(amidb_stmt_t *stmt);

/* Step to next result row */
int amidb_step(amidb_stmt_t *stmt);

/*
 * Parameter binding
 */

/* Bind integer parameter */
int amidb_bind_int(amidb_stmt_t *stmt, int index, LONG value);

/* Bind text parameter */
int amidb_bind_text(amidb_stmt_t *stmt, int index, 
                    const char *value, int length);

/* Bind blob parameter */
int amidb_bind_blob(amidb_stmt_t *stmt, int index,
                    const void *value, int length);

/* Bind NULL */
int amidb_bind_null(amidb_stmt_t *stmt, int index);

/*
 * Result column access
 */

/* Get number of columns in result */
int amidb_column_count(amidb_stmt_t *stmt);

/* Get column name */
const char *amidb_column_name(amidb_stmt_t *stmt, int col);

/* Get column type */
int amidb_column_type(amidb_stmt_t *stmt, int col);

/* Get integer value */
LONG amidb_column_int(amidb_stmt_t *stmt, int col);

/* Get text value */
const char *amidb_column_text(amidb_stmt_t *stmt, int col);

/* Get blob value */
const void *amidb_column_blob(amidb_stmt_t *stmt, int col);

/* Get value length (for text/blob) */
int amidb_column_bytes(amidb_stmt_t *stmt, int col);

/*
 * Transactions
 */

/* Begin transaction */
int amidb_begin(amidb_t *db);

/* Commit transaction */
int amidb_commit(amidb_t *db);

/* Rollback transaction */
int amidb_rollback(amidb_t *db);

/*
 * Low-level key-value API
 */

/* Put key-value pair */
int amidb_put(amidb_t *db, const char *table,
              const void *key, int key_len,
              const void *value, int value_len);

/* Get value by key */
int amidb_get(amidb_t *db, const char *table,
              const void *key, int key_len,
              void *value, int *value_len);

/* Delete by key */
int amidb_del(amidb_t *db, const char *table,
              const void *key, int key_len);

#endif /* AMIDB_H */
```

### 14.2 Usage Example

```c
#include <stdio.h>
#include "amidb.h"

int main(void)
{
    amidb_t *db;
    amidb_stmt_t *stmt;
    int rc;
    
    /* Open database */
    rc = amidb_open("RAM:mydata.adb", AMIDB_OPEN_CREATE, &db);
    if (rc != AMIDB_OK) {
        printf("Failed to open: %s\n", amidb_errmsg(db));
        return 1;
    }
    
    /* Create tables */
    amidb_exec(db, 
        "CREATE TABLE users ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT(64)"
        ")");
    
    amidb_exec(db,
        "CREATE TABLE orders ("
        "  id INTEGER PRIMARY KEY,"
        "  user_id INTEGER,"
        "  product TEXT(64)"
        ")");
    
    /* Insert data */
    amidb_exec(db, "INSERT INTO users VALUES (1, 'Alice')");
    amidb_exec(db, "INSERT INTO users VALUES (2, 'Bob')");
    amidb_exec(db, "INSERT INTO orders VALUES (100, 1, 'Widget')");
    amidb_exec(db, "INSERT INTO orders VALUES (101, 1, 'Gadget')");
    amidb_exec(db, "INSERT INTO orders VALUES (102, 2, 'Thing')");
    
    /* Query with INNER JOIN */
    rc = amidb_prepare(db,
        "SELECT users.name, orders.product "
        "FROM users "
        "INNER JOIN orders ON users.id = orders.user_id",
        &stmt);
    
    if (rc == AMIDB_OK) {
        while (amidb_step(stmt) == AMIDB_ROW) {
            printf("%s ordered %s\n",
                   amidb_column_text(stmt, 0),
                   amidb_column_text(stmt, 1));
        }
        amidb_finalize(stmt);
    }
    
    amidb_close(db);
    return 0;
}
```

---

## 15. Development Guidelines

### 15.1 Coding Standards

**File Organization:**
- One header per module
- Implementation files < 1000 lines
- All public symbols prefixed with `amidb_` or `AMIDB_`

**Naming Conventions:**
```c
/* Types: snake_case with _t suffix */
typedef struct amidb_page amidb_page_t;

/* Functions: snake_case */
int amidb_page_read(amidb_t *db, uint32_t page_num);

/* Constants: UPPER_SNAKE_CASE */
#define AMIDB_MAX_KEY_SIZE 256

/* Local variables: snake_case */
int page_count = 0;

/* Struct members: snake_case */
struct page { uint32_t page_num; };
```

**Error Handling:**
```c
/* Always check return codes */
int rc = some_function();
if (rc != AMIDB_OK) {
    /* Clean up and propagate error */
    return rc;
}

/* Use goto for cleanup in complex functions */
int complex_function(void)
{
    int rc = AMIDB_OK;
    void *buf1 = NULL, *buf2 = NULL;
    
    buf1 = mem_alloc(1024);
    if (!buf1) { rc = AMIDB_NOMEM; goto cleanup; }
    
    buf2 = mem_alloc(2048);
    if (!buf2) { rc = AMIDB_NOMEM; goto cleanup; }
    
    /* ... work ... */
    
cleanup:
    if (buf1) mem_free(buf1);
    if (buf2) mem_free(buf2);
    return rc;
}
```

### 15.2 68000-Specific Considerations

**Avoid:**
- 64-bit arithmetic (emulated, slow)
- Floating-point in hot paths
- Deep recursion
- Unaligned memory access
- Division (use shifts when possible)

**Prefer:**
- 32-bit operations
- Bit manipulation
- Lookup tables
- Iterative algorithms
- Word-aligned structures

**Structure Padding:**
```c
/* Bad: Unaligned access on 68000 */
struct bad {
    uint8_t  a;
    uint32_t b;  /* Misaligned! */
};

/* Good: Natural alignment */
struct good {
    uint32_t b;
    uint8_t  a;
    uint8_t  pad[3];
};
```

### 15.3 AmigaOS Programming Patterns

**Memory Allocation:**
```c
#include <exec/memory.h>
#include <proto/exec.h>

void *mem_alloc(uint32_t size)
{
    return AllocMem(size, MEMF_ANY | MEMF_CLEAR);
}

void mem_free(void *ptr, uint32_t size)
{
    if (ptr) FreeMem(ptr, size);
}
```

**File I/O:**
```c
#include <dos/dos.h>
#include <proto/dos.h>

BPTR file_open(const char *path, int mode)
{
    LONG amiga_mode = (mode & O_RDONLY) ? MODE_OLDFILE : MODE_READWRITE;
    return Open(path, amiga_mode);
}

int file_read(BPTR file, void *buf, uint32_t len)
{
    LONG result = Read(file, buf, len);
    return (result < 0) ? AMIDB_IOERR : (int)result;
}

int file_write(BPTR file, const void *buf, uint32_t len)
{
    LONG result = Write(file, (void *)buf, len);
    return (result != len) ? AMIDB_IOERR : AMIDB_OK;
}

int file_seek(BPTR file, int32_t offset, int whence)
{
    LONG mode = (whence == SEEK_SET) ? OFFSET_BEGINNING :
                (whence == SEEK_CUR) ? OFFSET_CURRENT :
                OFFSET_END;
    LONG result = Seek(file, offset, mode);
    return (result < 0) ? AMIDB_IOERR : AMIDB_OK;
}

void file_close(BPTR file)
{
    if (file) Close(file);
}
```

### 15.4 Build System

**Makefile for VBCC:**
```makefile
CC = vc
CFLAGS = -c99 -O2 -cpu=68000 -I/include
LDFLAGS = -lamiga

OBJS = api/amidb.o \
       sql/lexer.o \
       sql/parser.o \
       exec/executor.o \
       exec/join.o \
       storage/pager.o \
       storage/btree.o \
       txn/wal.o \
       os/file_amiga.o \
       util/endian.o \
       util/crc32.o

all: amidb.library amidb_cli

amidb.library: $(OBJS)
	$(CC) -nostdlib -o $@ $(OBJS) $(LDFLAGS)

amidb_cli: main.o $(OBJS)
	$(CC) -o $@ main.o $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) amidb.library amidb_cli
```

---

## 16. Testing Strategy

### 16.1 Test Categories

| Category | Description | Tools |
|----------|-------------|-------|
| Unit | Individual function tests | Custom harness |
| Integration | Module interaction | Shell scripts |
| Stress | Memory/disk pressure | Fuzzer |
| Regression | Known bug prevention | Test suite |
| Platform | Amiga-specific behavior | UAE + real hardware |

### 16.2 Unit Test Framework

```c
/* Minimal test framework for Amiga */

#define TEST(name) static int test_##name(void)
#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while(0)

#define RUN_TEST(name) do { \
    printf("Testing %s... ", #name); \
    if (test_##name() == 0) { \
        printf("OK\n"); \
        passed++; \
    } else { \
        failed++; \
    } \
} while(0)

/* Example tests */
TEST(btree_insert_single)
{
    amidb_t *db;
    ASSERT(amidb_open("RAM:test.adb", AMIDB_OPEN_CREATE, &db) == AMIDB_OK);
    ASSERT(amidb_exec(db, "CREATE TABLE t (k INTEGER PRIMARY KEY)") == AMIDB_OK);
    ASSERT(amidb_exec(db, "INSERT INTO t VALUES (42)") == AMIDB_OK);
    amidb_close(db);
    return 0;
}

TEST(join_basic)
{
    amidb_t *db;
    amidb_stmt_t *stmt;
    
    ASSERT(amidb_open("RAM:test.adb", AMIDB_OPEN_CREATE, &db) == AMIDB_OK);
    ASSERT(amidb_exec(db, "CREATE TABLE a (id INTEGER PRIMARY KEY, x INTEGER)") == AMIDB_OK);
    ASSERT(amidb_exec(db, "CREATE TABLE b (id INTEGER PRIMARY KEY, a_id INTEGER)") == AMIDB_OK);
    ASSERT(amidb_exec(db, "INSERT INTO a VALUES (1, 100)") == AMIDB_OK);
    ASSERT(amidb_exec(db, "INSERT INTO b VALUES (10, 1)") == AMIDB_OK);
    
    ASSERT(amidb_prepare(db, 
        "SELECT a.x FROM a INNER JOIN b ON a.id = b.a_id", 
        &stmt) == AMIDB_OK);
    ASSERT(amidb_step(stmt) == AMIDB_ROW);
    ASSERT(amidb_column_int(stmt, 0) == 100);
    ASSERT(amidb_step(stmt) == AMIDB_DONE);
    
    amidb_finalize(stmt);
    amidb_close(db);
    return 0;
}
```

### 16.3 Test Coverage Goals

| Module | Target Coverage |
|--------|-----------------|
| Pager | 90% |
| B+Tree | 95% |
| WAL | 90% |
| SQL Parser | 85% |
| Join Executor | 90% |
| API | 80% |

---

## 17. AI Agent Implementation Plan

This section provides a step-by-step guide for an AI coding agent to implement AmiDB from scratch.

### 17.1 Phase Overview

| Phase | Duration | Focus |
|-------|----------|-------|
| 1 | Week 1-2 | Foundation |
| 2 | Week 3-4 | Storage Engine |
| 3 | Week 5-6 | B+Tree & Transactions |
| 4 | Week 7-8 | SQL Parser |
| 5 | Week 9-10 | Query Executor & JOIN |
| 6 | Week 11-12 | Integration & Testing |

---

### 17.2 Phase 1: Foundation (Week 1-2)

#### Step 1.1: Project Setup
```
Actions:
1. Create directory structure as per section 8.2
2. Create Makefile for cross-compilation and native Amiga build
3. Set up version control with .gitignore

Deliverables:
- Makefile
- Directory tree
- README.md
```

#### Step 1.2: Platform Abstraction Layer
```
Actions:
1. Define os/file.h interface:
   - file_open(), file_close(), file_read(), file_write()
   - file_seek(), file_sync(), file_size()
   
2. Implement os/file_posix.c for development
3. Implement os/file_amiga.c for target platform

4. Define os/mem.h interface:
   - mem_alloc(), mem_free(), mem_realloc()
   
5. Implement os/mem_posix.c
6. Implement os/mem_amiga.c

Deliverables:
- file.h, file_posix.c, file_amiga.c
- mem.h, mem_posix.c, mem_amiga.c
- Unit tests for both modules
```

#### Step 1.3: Utility Functions
```
Actions:
1. Implement util/endian.c:
   - get_u16(), put_u16()
   - get_u32(), put_u32()
   - get_u64(), put_u64()
   
2. Implement util/crc32.c:
   - crc32_init()
   - crc32_update()
   - crc32_final()
   
3. Implement util/hash.c for string hashing

Deliverables:
- endian.h/c, crc32.h/c, hash.h/c
- Unit tests
```

---

### 17.3 Phase 2: Storage Engine (Week 3-4)

#### Step 2.1: Pager Module
```
Actions:
1. Define pager.h interface:
   - pager_open(), pager_close()
   - pager_read_page(), pager_write_page()
   - pager_alloc_page(), pager_free_page()
   
2. Implement page allocation bitmap
3. Implement page checksum verification
4. Implement file header read/write

Verification:
- Create database file
- Allocate pages
- Read/write pages with checksums
- Verify file format matches section 9

Deliverables:
- pager.h, pager.c
- Unit tests
```

#### Step 2.2: Page Cache
```
Actions:
1. Define cache.h interface:
   - cache_init(), cache_close()
   - cache_get(), cache_put()
   - cache_pin(), cache_unpin()
   - cache_flush()
   
2. Implement fixed-size buffer pool
3. Implement LRU eviction
4. Implement dirty page tracking

Verification:
- Cache hit/miss behavior
- LRU eviction order
- Dirty page write-back

Deliverables:
- cache.h, cache.c
- Unit tests
```

#### Step 2.3: Row Serialization
```
Actions:
1. Define row.h interface:
   - row_serialize(), row_deserialize()
   - value_compare()
   - row_size()
   
2. Implement type encoding per section 5.2
3. Implement overflow page handling for large values

Verification:
- Round-trip all data types
- Large value overflow

Deliverables:
- row.h, row.c
- Unit tests
```

---

### 17.4 Phase 3: B+Tree & Transactions (Week 5-6)

#### Step 3.1: B+Tree Core
```
Actions:
1. Define btree.h interface:
   - btree_create(), btree_open()
   - btree_insert(), btree_search(), btree_delete()
   - btree_cursor_*() functions
   
2. Implement node layout per section 9.4
3. Implement iterative search (no recursion!)
4. Implement leaf node linked list

Verification:
- Insert/search/delete 10,000 keys
- Range scans
- Tree depth stays logarithmic

Deliverables:
- btree.h, btree.c
- Unit tests
```

#### Step 3.2: B+Tree Split/Merge
```
Actions:
1. Implement leaf node split
2. Implement internal node split
3. Implement new root creation
4. Implement leaf merge on delete
5. Implement internal merge/rebalance

Verification:
- Insert until split occurs
- Delete until merge occurs
- Verify tree invariants maintained

Deliverables:
- Updated btree.c
- Stress tests
```

#### Step 3.3: Write-Ahead Log
```
Actions:
1. Define wal.h interface:
   - wal_init(), wal_close()
   - wal_begin(), wal_commit(), wal_abort()
   - wal_write_page()
   - wal_replay()
   
2. Implement WAL record format per section 12.2
3. Implement append-only writing
4. Implement checkpoint

Verification:
- Write records, verify on disk
- Simulate crash, replay WAL
- Verify data integrity after recovery

Deliverables:
- wal.h, wal.c
- Recovery tests
```

#### Step 3.4: Transaction Manager
```
Actions:
1. Define txn.h interface:
   - txn_begin(), txn_commit(), txn_rollback()
   
2. Integrate with pager and WAL
3. Implement commit protocol per section 12.3
4. Implement crash recovery per section 12.4

Verification:
- ACID compliance tests
- Crash recovery scenarios

Deliverables:
- txn.h, txn.c
- ACID tests
```

---

### 17.5 Phase 4: SQL Parser (Week 7-8)

#### Step 4.1: Lexer
```
Actions:
1. Define lexer.h interface:
   - lexer_init(), lexer_next()
   - token structure
   
2. Implement tokenizer per section 6.3
3. Handle string escaping
4. Handle numeric literals

Verification:
- Tokenize all SQL examples in section 6.2
- Error handling for invalid tokens

Deliverables:
- lexer.h, lexer.c
- Unit tests
```

#### Step 4.2: Parser
```
Actions:
1. Define ast.h with AST node types
2. Define parser.h interface:
   - parse_sql()
   
3. Implement recursive descent parser
4. Build AST for all statement types
5. Implement JOIN clause parsing (INNER JOIN ... ON)

Verification:
- Parse all SQL examples
- Error messages for syntax errors

Deliverables:
- ast.h, ast.c
- parser.h, parser.c
- Unit tests
```

#### Step 4.3: System Catalog
```
Actions:
1. Define catalog.h interface:
   - catalog_create_table()
   - catalog_drop_table()
   - catalog_get_table()
   
2. Store table definitions in B+Tree
3. Implement schema validation

Verification:
- Create/drop tables
- Persist across database close/open

Deliverables:
- catalog.h, catalog.c
- Integration tests
```

---

### 17.6 Phase 5: Query Executor & JOIN (Week 9-10)

#### Step 5.1: Basic Operators
```
Actions:
1. Define executor.h interface:
   - execute_statement()
   
2. Implement sequential scan operator
3. Implement filter operator (WHERE)
4. Implement projection operator (SELECT columns)

Verification:
- SELECT * FROM table
- SELECT cols FROM table WHERE condition

Deliverables:
- executor.h, executor.c
- scan.c, filter.c
```

#### Step 5.2: DML Operators
```
Actions:
1. Implement INSERT operator
2. Implement DELETE operator
3. Integrate with transactions

Verification:
- INSERT + SELECT roundtrip
- DELETE + verify gone
- Transaction rollback undoes changes

Deliverables:
- Updated executor.c
- Integration tests
```

#### Step 5.3: INNER JOIN Implementation
```
Actions:
1. Implement join planner per section 7.6
2. Implement nested loop join per section 7.2
3. Implement index nested loop join per section 7.3
4. Implement block nested loop join per section 7.4
5. Implement column resolution for qualified names (table.column)
6. Implement result row combination

Verification:
- Two-table join with small data
- Index join when right table has index
- Block join for large tables without index
- Correct result ordering

Deliverables:
- join.c, planner.c
- Join-specific tests
```

#### Step 5.4: Query Planner
```
Actions:
1. Implement plan node creation per section 13.1
2. Implement join strategy selection per section 7.8
3. Optimize for index usage when available

Verification:
- Check plan for various queries
- Verify index usage when expected

Deliverables:
- planner.c
- Plan explanation tests
```

---

### 17.7 Phase 6: Integration & Testing (Week 11-12)

#### Step 6.1: Public API
```
Actions:
1. Implement all functions in amidb.h per section 14.1
2. Add comprehensive error messages
3. Ensure thread-safety notes documented

Verification:
- Run usage example from section 14.2
- API consistency checks

Deliverables:
- api/amidb.c
- API tests
```

#### Step 6.2: CLI Tool (Optional)
```
Actions:
1. Implement interactive SQL shell
2. Add .dump, .schema, .tables commands
3. Add readline-like editing (if available)

Deliverables:
- main.c
- User documentation
```

#### Step 6.3: Platform Testing
```
Actions:
1. Build for AmigaOS 3.1 with VBCC
2. Test on UAE emulator
3. Test on real Amiga hardware (if available)
4. Profile memory usage
5. Benchmark performance

Verification:
- All tests pass on Amiga
- Memory usage within budget (section 10.4)
- JOIN performance acceptable

Deliverables:
- Test reports
- Performance benchmarks
```

#### Step 6.4: Final Integration
```
Actions:
1. Run full test suite
2. Fix remaining bugs
3. Write user documentation
4. Create release package

Deliverables:
- Release archive with binaries
- User manual
- API reference
```

---

### 17.8 Implementation Checklist

```
Foundation:
[ ] Project structure created
[ ] Makefile working (POSIX + Amiga)
[ ] file.h abstraction implemented
[ ] mem.h abstraction implemented  
[ ] endian.c helpers working
[ ] crc32.c verified
[ ] Platform builds verified

Storage:
[ ] Pager read/write working
[ ] Page checksum verified
[ ] Page allocation bitmap working
[ ] Page cache with LRU eviction
[ ] Dirty page tracking
[ ] Row serialization all types
[ ] Overflow page handling

B+Tree:
[ ] Single insert working
[ ] Search working
[ ] Cursor iteration working
[ ] Leaf split working
[ ] Internal split working
[ ] Delete working
[ ] Merge/rebalance working
[ ] 10K key stress test passing

Transactions:
[ ] WAL append working
[ ] WAL replay working
[ ] Commit protocol complete
[ ] Crash recovery verified
[ ] ACID tests passing

SQL Parser:
[ ] Lexer all tokens
[ ] CREATE TABLE parsing
[ ] INSERT parsing
[ ] SELECT parsing
[ ] DELETE parsing
[ ] WHERE clause parsing
[ ] INNER JOIN parsing
[ ] Error messages helpful

Executor:
[ ] SeqScan operator
[ ] Filter operator
[ ] Project operator
[ ] INSERT operator
[ ] DELETE operator
[ ] Nested loop join
[ ] Index nested loop join
[ ] Block nested loop join
[ ] Column resolution (table.column)

Integration:
[ ] Public API complete
[ ] All tests passing
[ ] Amiga build working
[ ] UAE testing complete
[ ] Real hardware testing (optional)
[ ] Documentation complete
[ ] Release package created
```

---

## 18. Appendices

### 18.1 Error Code Reference

| Code | Name | Description |
|------|------|-------------|
| 0 | AMIDB_OK | Success |
| -1 | AMIDB_ERROR | Generic error |
| -2 | AMIDB_BUSY | Database locked |
| -3 | AMIDB_NOTFOUND | Key/row not found |
| -4 | AMIDB_EXISTS | Duplicate key |
| -5 | AMIDB_CORRUPT | Data corruption detected |
| -6 | AMIDB_NOMEM | Out of memory |
| -7 | AMIDB_IOERR | I/O error |
| -8 | AMIDB_FULL | Database full |
| -9 | AMIDB_SYNTAX | SQL syntax error |
| -10 | AMIDB_DONE | No more rows |
| -11 | AMIDB_ROW | Row available |

### 18.2 SQL Reserved Words

```
AND, AS, ASC, BLOB, BY, CREATE, DELETE, DESC, DROP, FROM,
INNER, INSERT, INTEGER, INTO, IS, JOIN, KEY, LIMIT, NOT,
NULL, ON, OR, ORDER, PRIMARY, SELECT, TABLE, TEXT, VALUES,
WHERE
```

### 18.3 Glossary

| Term | Definition |
|------|------------|
| B+Tree | Self-balancing tree structure with keys in internal nodes and data in leaves |
| LSN | Log Sequence Number - monotonic identifier for WAL records |
| Page | Fixed-size unit of storage (4096 bytes) |
| Pager | Module responsible for page I/O and allocation |
| WAL | Write-Ahead Log - append-only log for crash recovery |
| Checkpoint | Process of writing WAL changes to main database file |

### 18.4 References

1. SQLite File Format Documentation
2. "Database Internals" by Alex Petrov
3. AmigaOS 3.x Developer Documentation
4. VBCC Compiler Manual
5. 68000 Programmer's Reference Manual

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | December 2025 | Initial specification with INNER JOIN support |

---

*End of Document*
