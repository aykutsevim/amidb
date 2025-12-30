# AmiDB SQL Scripts

This folder contains example SQL migration scripts that demonstrate AmiDB's capabilities and can be used to populate empty database files.

## Scripts

### 01_retro_game_history.sql - Simple Database

A database cataloging classic video games from the 8-bit and 16-bit era.

**Tables (5):**
| Table | Records | Description |
|-------|---------|-------------|
| manufacturers | 15 | Game publishers and developers |
| platforms | 10 | Gaming systems (Amiga, C64, NES, etc.) |
| genres | 10 | Game categories |
| games | 30 | The main game catalog |
| reviews | 20 | Game ratings from magazines |

**Usage:**
```bash
./amidb_shell RAM:gamehistory.db sqlscripts/01_retro_game_history.sql
```

**Sample Queries:**
```sql
-- All platformers
SELECT * FROM games WHERE genre_id = 1;

-- Games by Bitmap Brothers
SELECT * FROM games WHERE manufacturer_id = 7;

-- Average review score
SELECT AVG(score) FROM reviews;

-- Games released in 1991
SELECT * FROM games WHERE release_year = 1991;
```

### 02_retro_game_store.sql - Complex Database

A comprehensive e-commerce database for a retro gaming store, demonstrating complex relational data modeling.

**Tables (11):**
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

**Usage:**
```bash
./amidb_shell RAM:gamestore.db sqlscripts/02_retro_game_store.sql
```

**Sample Queries:**
```sql
-- Total revenue
SELECT SUM(total) FROM orders WHERE status != 'cancelled';

-- Average order value
SELECT AVG(total) FROM orders;

-- Low stock items
SELECT * FROM products WHERE stock < 3;

-- Pending orders
SELECT * FROM orders WHERE status = 'pending';

-- Products by category
SELECT * FROM products WHERE type_id = 20;

-- Customer order count
SELECT * FROM customers ORDER BY total_orders DESC;
```

## Database Schema

### 01_retro_game_history.sql

```
manufacturers ─────┐
                   │
platforms ─────────┼──► games ──► reviews
                   │
genres ────────────┘
```

### 02_retro_game_store.sql

```
product_types ──┐
                ├──► products ──► order_items ◄── orders ◄── customers
conditions ─────┘                     │              │            │
                                      │              │            │
                              inventory_log     payments      addresses
                                                    │             │
                                           payment_methods    countries
```

## Running Scripts

### Interactive Mode
```bash
# Start shell with database, run script, then stay in shell
./amidb_shell RAM:mydb.db myscript.sql

# After script runs, you can query the data
amidb> SELECT * FROM games LIMIT 5;
amidb> SELECT COUNT(*) FROM products;
```

### Script Format Notes

- Each statement ends with `;`
- Comments use `--` (SQL standard)
- Text values use single quotes: `'text'`
- INTEGER for all numbers (no floats)
- Dates stored as INTEGER (YYYYMMDD format)
- Prices stored in cents (e.g., 29900 = $299.00)

## Creating Your Own Scripts

Template:
```sql
-- ============================================================
-- My Database Name
-- ============================================================
-- Description of what this database stores
--
-- Tables:
--   - table1: Description
--   - table2: Description
--
-- Usage:
--   ./amidb_shell RAM:mydb.db my_script.sql
-- ============================================================

-- Table definitions
CREATE TABLE table1 (
    id INTEGER PRIMARY KEY,
    name TEXT,
    value INTEGER
);

-- Data population
INSERT INTO table1 VALUES (1, 'First', 100);
INSERT INTO table1 VALUES (2, 'Second', 200);

-- Verification
SELECT COUNT(*) FROM table1;
```

## Best Practices

1. **Use INTEGER PRIMARY KEY** - Required for efficient lookups
2. **Store dates as YYYYMMDD integers** - Easy to compare and sort
3. **Store money as cents** - Avoid floating point issues
4. **Use reference tables** - For status codes, types, etc.
5. **Add verification queries** - Confirm data loaded correctly
6. **Comment your schema** - Future you will thank you

## Tips

- Scripts run sequentially - tables must be created before inserting
- Errors are reported but script continues
- Use `SELECT COUNT(*)` to verify record counts
- Use `.tables` to see all tables after script runs
- Use `.schema tablename` to see column definitions
