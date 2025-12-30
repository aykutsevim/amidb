# AmiDB Examples

This directory contains comprehensive example programs demonstrating all AmiDB features for AmigaOS 3.1 development.

## inventory_demo.c - Complete AmiDB Showcase

A comprehensive example demonstrating **ALL AmiDB capabilities** through a retro computer store inventory management system.

### Features Demonstrated

The example is divided into two parts:

#### Part A: Direct C API (Examples 1-4)

| Example | Topic | Features |
|---------|-------|----------|
| 1 | B+Tree Basics | Pager, Cache, B+Tree insert/search/delete/cursor, statistics |
| 2 | Row Serialization | row_init, row_set_*, row_serialize, row_deserialize, data types |
| 3 | ACID Transactions | txn_begin, txn_commit, txn_abort, WAL, isolation |
| 4 | Crash Recovery | Unclean shutdown simulation, automatic recovery |

#### Part B: SQL Interface (Examples 5-10)

| Example | Topic | Features |
|---------|-------|----------|
| 5 | CREATE & INSERT | Schema creation, explicit/implicit PRIMARY KEY, data insertion |
| 6 | SELECT Queries | WHERE (=, !=, <, <=, >, >=), ORDER BY ASC/DESC, LIMIT |
| 7 | UPDATE & DELETE | UPDATE SET WHERE, DELETE WHERE, fast path vs full scan |
| 8 | Aggregate Functions | COUNT(*), COUNT(col), SUM, AVG, MIN, MAX |
| 9 | DROP TABLE | Table removal, recreation with different schema |
| 10 | Complete Workflow | Real-world inventory report scenario |

### Building

```bash
# Build all examples
make examples

# Or build just the inventory demo
m68k-amigaos-gcc -m68000 -O2 -noixemul -Isrc \
    examples/inventory_demo.c \
    obj/*.o \
    -o inventory_demo
```

### Running

#### On Amiga (Real Hardware or Emulator)

```bash
# Copy to your Amiga and run
inventory_demo
```

#### On Amiberry/UAE

```bash
# The executable will create test databases in RAM:
# - RAM:inventory_direct.db (Part A examples)
# - RAM:inventory_sql.db (Part B examples)
```

### Expected Output

The program runs 10 sequential examples with detailed output:

```
*****************************************************
*     AmiDB Comprehensive Example Program           *
*     Demonstrating ALL Database Capabilities       *
*****************************************************

Platform: AmigaOS 3.1 / 68000 CPU
Constraints: 2MB RAM, 4KB Stack

=====================================================
PART A: DIRECT C API
=====================================================

===============================================
Example 1: B+Tree Basics
===============================================

1. Creating database 'RAM:inventory_direct.db'...
   Database created (page size: 4096 bytes)

2. Creating page cache (64 pages = 256 KB)...

3. Creating B+Tree index...
   B+Tree created (root page: 2, order: 64)

--- Inserting Products ---
   Product 1001: Amiga 500    -> price 299
   Product 1002: Amiga 1200   -> price 499
   ...

[OK] Example 1 completed.

... (examples 2-9) ...

===============================================
Example 10: Complete Inventory Workflow
===============================================

=== DAILY INVENTORY REPORT ===

--- Inventory Summary ---
   Total Products:    6
   Total Value: $2712
   Average Price: $452
   Price Range: $79 - $1299

--- By Category ---
   Computers:    3
   Peripherals:  1
   Accessories:  1

--- High-Value Items (> $200) ---
   3, 'Amiga 4000', 1299, 'Computer', 2
   2, 'Amiga 1200', 499, 'Computer', 5
   ...

[OK] Example 10 completed.

===============================================
DEMONSTRATION COMPLETE
===============================================

All 10 examples completed successfully!

Capabilities Demonstrated:
  DIRECT API:
    [x] Pager - Page-based file I/O
    [x] Cache - LRU page caching
    [x] B+Tree - Indexed storage (insert/search/delete/cursor)
    [x] Row - Multi-column serialization
    [x] WAL - Write-Ahead Logging
    [x] Transactions - ACID guarantees
    [x] Recovery - Crash recovery

  SQL INTERFACE:
    [x] CREATE TABLE - Schema definition
    [x] DROP TABLE - Schema removal
    [x] INSERT - Data insertion
    [x] SELECT - Queries with WHERE/ORDER BY/LIMIT
    [x] UPDATE - Data modification
    [x] DELETE - Data removal
    [x] COUNT(*) - Row counting
    [x] SUM() - Numeric summation
    [x] AVG() - Numeric averaging
    [x] MIN() - Minimum value
    [x] MAX() - Maximum value

AmiDB is ready for your Amiga applications!
```

### Code Structure

```c
/* Configuration */
#define DB_PATH_DIRECT "RAM:inventory_direct.db"
#define DB_PATH_SQL    "RAM:inventory_sql.db"
#define CACHE_SIZE     64  /* 256KB */

/* Helper functions */
print_section()      - Section headers
print_subsection()   - Sub-section headers
print_line()         - Horizontal dividers
run_sql()            - Execute SQL statement
print_results()      - Display SELECT results

/* Part A: Direct API Examples */
example_btree_basics()        - Example 1
example_row_serialization()   - Example 2
example_transactions()        - Example 3
example_crash_recovery()      - Example 4

/* Part B: SQL Interface Examples */
example_sql_create_insert()   - Example 5
example_sql_select()          - Example 6
example_sql_update_delete()   - Example 7
example_sql_aggregates()      - Example 8
example_sql_drop_table()      - Example 9
example_complete_workflow()   - Example 10

/* Main entry point */
main()               - Runs all examples in sequence
```

### Key Concepts Demonstrated

#### Storage Engine

- **Pager**: Page-based file I/O with 4096-byte pages
- **Cache**: LRU caching with configurable size and pinning
- **B+Tree**: Order-64 tree for O(log n) operations
- **Row Format**: Length-prefixed columns with type tags

#### Transaction System

- **WAL**: Write-ahead logging for durability
- **ACID**: Full atomicity, consistency, isolation, durability
- **Recovery**: Automatic crash recovery on database open

#### SQL Engine

- **Lexer**: Tokenizes SQL into keywords, identifiers, literals
- **Parser**: Builds AST from token stream
- **Catalog**: Stores table schemas in B+Tree
- **Executor**: Executes parsed statements

#### Data Types

| Type | Size | Range |
|------|------|-------|
| INTEGER | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| TEXT | variable | Length-prefixed UTF-8 |
| BLOB | variable | Length-prefixed binary |
| NULL | 1 byte | Type tag only |

### Memory Usage

The example uses approximately:
- Cache: 256 KB (64 pages)
- WAL buffer: ~32 KB
- Stack: < 1 KB (uses static allocation)
- **Total: ~300 KB** (well within 2MB Amiga limit)

### Learning Path

1. **Start with Example 1** - Understand basic database operations
2. **Study Example 2** - Learn row serialization format
3. **Review Example 3** - See how ACID transactions work
4. **Examine Example 4** - Understand crash recovery
5. **Move to Example 5** - Learn SQL schema creation
6. **Try Example 6** - Master SQL queries
7. **Practice Example 7** - Understand UPDATE/DELETE
8. **Explore Example 8** - Use aggregate functions
9. **See Example 9** - Learn DROP TABLE
10. **Complete Example 10** - Real-world application

### Extending the Example

To build your own AmiDB application:

```c
#include "storage/pager.h"
#include "storage/cache.h"
#include "sql/catalog.h"
#include "sql/executor.h"
#include "sql/lexer.h"
#include "sql/parser.h"

int main(void) {
    struct amidb_pager *pager;
    struct page_cache *cache;
    struct catalog catalog;
    struct sql_executor exec;

    /* Initialize */
    pager_open("RAM:myapp.db", 0, &pager);
    cache = cache_create(64, pager);
    catalog_init(&catalog, pager, cache);
    executor_init(&exec, pager, cache, &catalog);

    /* Execute SQL */
    struct sql_lexer lexer;
    struct sql_parser parser;
    struct sql_statement stmt;

    lexer_init(&lexer, "CREATE TABLE mydata (id INTEGER PRIMARY KEY, value TEXT)");
    parser_init(&parser, &lexer);
    parser_parse_statement(&parser, &stmt);
    executor_execute(&exec, &stmt);

    /* Cleanup */
    executor_close(&exec);
    catalog_close(&catalog);
    cache_destroy(cache);
    pager_close(pager);

    return 0;
}
```

### API Reference

See header files for complete API documentation:

| File | Purpose |
|------|---------|
| `src/storage/pager.h` | Page-level file I/O |
| `src/storage/cache.h` | LRU page cache |
| `src/storage/btree.h` | B+Tree index operations |
| `src/storage/row.h` | Row serialization |
| `src/txn/wal.h` | Write-Ahead Logging |
| `src/txn/txn.h` | Transaction management |
| `src/sql/lexer.h` | SQL tokenization |
| `src/sql/parser.h` | SQL parsing |
| `src/sql/catalog.h` | Table schema storage |
| `src/sql/executor.h` | SQL execution |

### Troubleshooting

**"Failed to create database"**
- Ensure RAM: has enough space (1 MB recommended)
- Check file permissions on destination

**"Failed to create cache"**
- Reduce CACHE_SIZE (try 32 instead of 64)
- Free up system memory

**Parse errors in SQL**
- Use single quotes for strings: `'text'` not `"text"`
- End statements with semicolon in scripts
- Check keyword spelling (case-insensitive)

**Transaction errors**
- Ensure txn_begin() before operations
- Call txn_commit() or txn_abort() to end
- Don't nest transactions

### Performance Tips

1. **Increase cache size** for better hit rates
2. **Batch operations** in single transactions
3. **Use cursor iteration** for bulk reads
4. **WHERE on PRIMARY KEY** for O(log n) lookups
5. **Flush cache** after bulk inserts

### Further Reading

- `docs/01-Cross-Compile-Setup.md` - Development environment setup
- `docs/02-AmiDB-Library-Usage.md` - Complete API guide
- `docs/03-AmiDB-Shell-Guide.md` - Interactive shell reference
- `tests/` - Test suite with more usage examples

---

**Happy coding on your Amiga!**
