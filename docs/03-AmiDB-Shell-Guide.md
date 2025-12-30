# AmiDB Shell User Guide

A comprehensive guide to the AmiDB interactive SQL shell, covering all commands, syntax, and features with detailed examples.

## Table of Contents

1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Meta-Commands](#meta-commands)
4. [SQL Commands](#sql-commands)
5. [Data Types](#data-types)
6. [Query Clauses](#query-clauses)
7. [Aggregate Functions](#aggregate-functions)
8. [Script Execution](#script-execution)
9. [Practical Examples](#practical-examples)
10. [Tips and Tricks](#tips-and-tricks)
11. [Troubleshooting](#troubleshooting)

---

## Introduction

The AmiDB Shell (`amidb_shell`) is an interactive command-line interface for working with AmiDB databases. It provides a SQLite-like experience tailored for AmigaOS 3.1 systems with 68000 CPUs.

### Features

- Interactive SQL command execution
- SQL script file execution
- Table listing and schema inspection
- Support for INTEGER, TEXT, and BLOB data types
- WHERE clause filtering with comparison operators
- ORDER BY sorting (ASC/DESC)
- LIMIT clause for result pagination
- Aggregate functions (COUNT, SUM, AVG, MIN, MAX)
- Multi-line SQL statement support in scripts
- Comment support (`--` and `#`)

### Constraints

- Maximum 32 columns per table
- Maximum 100 rows returned per SELECT
- Maximum 512 bytes per SQL statement
- INTEGER PRIMARY KEY required (explicit or implicit rowid)
- No JOIN operations (yet)
- No floating-point numbers (INTEGER only)

---

## Getting Started

### Starting the Shell

```bash
# Interactive mode with default database (RAM:amidb.db)
./amidb_shell

# Interactive mode with specific database
./amidb_shell mydata.db

# Execute script then enter interactive mode
./amidb_shell mydata.db myscript.sql
```

### Command-Line Arguments

```
Usage: amidb_shell [database_file] [script_file]

Arguments:
  database_file  Database file path (default: RAM:amidb.db)
  script_file    Optional SQL script to execute on startup
```

### The Welcome Banner

When you start the shell, you'll see:

```
================================================
AmiDB SQL Shell v1.0
================================================
AmigaOS 3.1 - 68000 CPU - SQLite-like Database

Type .help for help, .quit to exit
================================================

amidb>
```

### Exiting the Shell

Type `.quit` or `.exit` to leave the shell:

```
amidb> .quit
Goodbye!
```

Or press Ctrl+D (EOF) to exit.

---

## Meta-Commands

Meta-commands start with a dot (`.`) and provide shell functionality.

### .help

Displays help information about available commands.

```
amidb> .help

AmiDB SQL Shell - Help
======================

Meta-commands:
  .help              Show this help
  .quit              Exit the shell
  .tables            List all tables
  .schema <table>    Show table schema

SQL commands:
  CREATE TABLE <name> (columns...)
  INSERT INTO <table> VALUES (...)
  SELECT * FROM <table> [WHERE ...] [ORDER BY ...] [LIMIT n]
  UPDATE <table> SET ... WHERE ...
  DELETE FROM <table> WHERE ...

Example:
  CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT);
  INSERT INTO users VALUES (1, 'Alice');
  SELECT * FROM users;
```

### .tables

Lists all tables in the current database.

```
amidb> .tables

Tables:
-------
  products
  customers
  orders

amidb>
```

If no tables exist:

```
amidb> .tables
No tables found.
```

### .schema

Shows detailed schema information for a specific table.

**Syntax:**
```
.schema <table_name>
```

**Example:**

```
amidb> .schema products

Table: products
=====================================
Columns:
  id INTEGER PRIMARY KEY
  name TEXT
  price INTEGER
  category TEXT

Row count: 15

amidb>
```

For tables with implicit rowid:

```
amidb> .schema logs

Table: logs
=====================================
Columns:
  message TEXT
  timestamp INTEGER

Implicit rowid: yes (next=5)
Row count: 4

amidb>
```

### .quit / .exit

Exits the shell gracefully.

```
amidb> .quit
Goodbye!
```

---

## SQL Commands

### CREATE TABLE

Creates a new table in the database.

**Syntax:**
```sql
CREATE TABLE table_name (
    column1 TYPE [PRIMARY KEY],
    column2 TYPE,
    ...
)
```

**Column Types:**
- `INTEGER` - 32-bit signed integer
- `TEXT` - Variable-length string
- `BLOB` - Binary data

**Examples:**

```sql
-- Table with explicit primary key
amidb> CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)
Table created successfully.

-- Table with implicit rowid (auto-generated primary key)
amidb> CREATE TABLE logs (message TEXT, level INTEGER)
Table created successfully.

-- Multi-column table
amidb> CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER, stock INTEGER, category TEXT)
Table created successfully.
```

**Rules:**
- Only one PRIMARY KEY allowed per table
- PRIMARY KEY must be INTEGER type
- If no PRIMARY KEY specified, an implicit rowid is created
- Maximum 32 columns per table
- Table names are case-sensitive

### DROP TABLE

Removes a table from the database.

**Syntax:**
```sql
DROP TABLE table_name
```

**Examples:**

```sql
amidb> DROP TABLE users
Table dropped successfully.

amidb> DROP TABLE nonexistent
Error: Table 'nonexistent' does not exist
```

**Notes:**
- This is permanent - all data is lost
- Pages are not reclaimed (file size doesn't shrink)
- Cannot be rolled back

### INSERT

Inserts a new row into a table.

**Syntax:**
```sql
INSERT INTO table_name VALUES (value1, value2, ...)
```

**Examples:**

```sql
-- Insert integers and text
amidb> INSERT INTO users VALUES (1, 'Alice', 30)
Row inserted successfully.

-- Text values must use single quotes
amidb> INSERT INTO products VALUES (1, 'Amiga 500', 299, 10, 'Computer')
Row inserted successfully.

-- For implicit rowid tables, omit the rowid
amidb> INSERT INTO logs VALUES ('System started', 1)
Row inserted successfully.

-- Negative integers are supported
amidb> INSERT INTO temperatures VALUES (1, 'January', -5)
Row inserted successfully.
```

**Rules:**
- Values must match column order exactly
- Value count must match column count
- TEXT values must be single-quoted (`'text'`)
- INTEGER values are unquoted
- NULL values are not supported in INSERT (yet)
- Duplicate PRIMARY KEY causes error

### SELECT

Retrieves rows from a table.

**Basic Syntax:**
```sql
SELECT * FROM table_name
SELECT * FROM table_name WHERE condition
SELECT * FROM table_name ORDER BY column [ASC|DESC]
SELECT * FROM table_name LIMIT n
```

**Examples:**

```sql
-- Select all rows
amidb> SELECT * FROM users

Row 1: 1, 'Alice', 30
Row 2: 2, 'Bob', 25
Row 3: 3, 'Carol', 35

3 rows returned.

-- With WHERE clause
amidb> SELECT * FROM users WHERE age > 28

Row 1: 1, 'Alice', 30
Row 2: 3, 'Carol', 35

2 rows returned.

-- With ORDER BY (ascending by default)
amidb> SELECT * FROM products ORDER BY price

Row 1: 3, 'Joystick', 15
Row 2: 2, 'Mouse', 25
Row 3: 1, 'Amiga 500', 299

3 rows returned.

-- Descending order
amidb> SELECT * FROM products ORDER BY price DESC

Row 1: 1, 'Amiga 500', 299
Row 2: 2, 'Mouse', 25
Row 3: 3, 'Joystick', 15

3 rows returned.

-- With LIMIT
amidb> SELECT * FROM products LIMIT 2

Row 1: 1, 'Amiga 500', 299
Row 2: 2, 'Mouse', 25

2 rows returned.

-- Combining clauses
amidb> SELECT * FROM products WHERE price > 20 ORDER BY price DESC LIMIT 5

Row 1: 1, 'Amiga 500', 299
Row 2: 2, 'Mouse', 25

2 rows returned.
```

**Output Format:**
- Each row is numbered starting from 1
- INTEGER values are displayed as numbers
- TEXT values are displayed in single quotes
- BLOB values show size: `[BLOB 128 bytes]`
- NULL values display as `NULL`

### UPDATE

Modifies existing rows in a table.

**Syntax:**
```sql
UPDATE table_name SET column = value WHERE condition
```

**Examples:**

```sql
-- Update by primary key (fast path)
amidb> UPDATE users SET age = 31 WHERE id = 1
Rows updated successfully.

-- Update by non-primary key (full scan)
amidb> UPDATE products SET price = 349 WHERE name = 'Amiga 500'
Rows updated successfully.

-- Update multiple rows
amidb> UPDATE products SET stock = 0 WHERE price > 1000
Rows updated successfully.
```

**Rules:**
- Only one SET clause is supported
- WHERE clause is required (no UPDATE without WHERE)
- Cannot update PRIMARY KEY column
- WHERE on PRIMARY KEY is fastest (direct B+Tree lookup)

### DELETE

Removes rows from a table.

**Syntax:**
```sql
DELETE FROM table_name WHERE condition
```

**Examples:**

```sql
-- Delete by primary key (fast path)
amidb> DELETE FROM users WHERE id = 3
Rows deleted successfully.

-- Delete by non-primary key (full scan)
amidb> DELETE FROM products WHERE price < 20
Rows deleted successfully.

-- Delete by text match
amidb> DELETE FROM logs WHERE message = 'debug'
Rows deleted successfully.
```

**Rules:**
- WHERE clause is required
- WHERE on PRIMARY KEY is fastest
- Maximum 100 rows can be deleted per operation
- Deletion is permanent

---

## Data Types

### INTEGER

32-bit signed integer values.

**Range:** -2,147,483,648 to 2,147,483,647

**Examples:**
```sql
INSERT INTO scores VALUES (1, 'Alice', 95)
INSERT INTO temps VALUES (1, 'North Pole', -40)
SELECT * FROM scores WHERE score >= 90
```

### TEXT

Variable-length character strings.

**Notes:**
- Stored as length-prefixed blobs (not null-terminated)
- Must be enclosed in single quotes (`'text'`)
- No escape sequences for quotes (yet)
- UTF-8 compatible

**Examples:**
```sql
INSERT INTO users VALUES (1, 'Alice Smith')
INSERT INTO products VALUES (1, 'Amiga 500 Plus')
SELECT * FROM users WHERE name = 'Alice Smith'
```

### BLOB

Binary large objects (not commonly used in shell).

**Notes:**
- Displayed as `[BLOB n bytes]` in SELECT output
- Cannot be inserted via SQL shell (use library API)
- Useful for storing binary data programmatically

### NULL

NULL values indicate missing or unknown data.

**Notes:**
- Displayed as `NULL` in output
- Cannot insert NULL via SQL (yet)
- COUNT(column) excludes NULL values

---

## Query Clauses

### WHERE Clause

Filters rows based on a condition.

**Supported Operators:**

| Operator | Meaning | Example |
|----------|---------|---------|
| `=` | Equal | `WHERE id = 1` |
| `!=` or `<>` | Not equal | `WHERE status != 'deleted'` |
| `<` | Less than | `WHERE price < 100` |
| `<=` | Less than or equal | `WHERE age <= 18` |
| `>` | Greater than | `WHERE score > 90` |
| `>=` | Greater than or equal | `WHERE stock >= 10` |

**Examples:**

```sql
-- Equality (INTEGER)
SELECT * FROM users WHERE id = 1

-- Equality (TEXT)
SELECT * FROM products WHERE category = 'Computer'

-- Inequality
SELECT * FROM users WHERE status != 'inactive'

-- Range comparisons
SELECT * FROM products WHERE price >= 100
SELECT * FROM products WHERE price < 500
SELECT * FROM users WHERE age > 21

-- Combined with ORDER BY
SELECT * FROM products WHERE price > 50 ORDER BY price ASC
```

**Optimization:**
- WHERE on PRIMARY KEY with `=` uses B+Tree search (O(log n))
- WHERE on other columns uses full table scan (O(n))

### ORDER BY Clause

Sorts results by a column.

**Syntax:**
```sql
ORDER BY column_name [ASC|DESC]
```

**Examples:**

```sql
-- Ascending (default)
SELECT * FROM products ORDER BY price
SELECT * FROM products ORDER BY price ASC

-- Descending
SELECT * FROM users ORDER BY age DESC
SELECT * FROM products ORDER BY name DESC
```

**Notes:**
- Default order is ASC (ascending)
- For TEXT columns, uses lexicographic ordering
- Limited to one ORDER BY column
- Maximum 100 rows for in-memory sorting

### LIMIT Clause

Restricts the number of rows returned.

**Syntax:**
```sql
LIMIT n
```

**Examples:**

```sql
-- Get first 5 products
SELECT * FROM products LIMIT 5

-- Top 10 most expensive
SELECT * FROM products ORDER BY price DESC LIMIT 10

-- Combined with WHERE
SELECT * FROM users WHERE age > 18 ORDER BY name LIMIT 20
```

**Notes:**
- Provides early termination for large result sets
- Maximum is 100 rows regardless of LIMIT value

---

## Aggregate Functions

### COUNT

Counts rows or non-NULL values.

**Syntax:**
```sql
SELECT COUNT(*) FROM table_name
SELECT COUNT(column) FROM table_name
```

**Examples:**

```sql
-- Count all rows
amidb> SELECT COUNT(*) FROM users

Row 1: 15

1 row returned.

-- Count with WHERE
amidb> SELECT COUNT(*) FROM users WHERE age >= 21

Row 1: 12

1 row returned.

-- Count non-NULL values in a column
amidb> SELECT COUNT(email) FROM users

Row 1: 13

1 row returned.
```

### SUM

Calculates the sum of INTEGER values.

**Syntax:**
```sql
SELECT SUM(column) FROM table_name
```

**Examples:**

```sql
-- Sum all prices
amidb> SELECT SUM(price) FROM products

Row 1: 2397

1 row returned.

-- Sum with WHERE clause
amidb> SELECT SUM(amount) FROM orders WHERE customer_id = 5

Row 1: 750

1 row returned.

-- Sum on empty result returns 0
amidb> SELECT SUM(amount) FROM orders WHERE customer_id = 999

Row 1: 0

1 row returned.
```

**Notes:**
- Only works on INTEGER columns
- NULL values are ignored
- Returns 0 for empty tables or no matches

### AVG

Calculates the average of INTEGER values.

**Syntax:**
```sql
SELECT AVG(column) FROM table_name
```

**Examples:**

```sql
-- Average price
amidb> SELECT AVG(price) FROM products

Row 1: 159

1 row returned.

-- Average with WHERE
amidb> SELECT AVG(score) FROM grades WHERE subject = 'Math'

Row 1: 82

1 row returned.
```

**Notes:**
- Only works on INTEGER columns
- Uses integer division (truncates toward zero)
- NULL values are ignored
- Returns 0 for empty tables

### MIN

Finds the minimum INTEGER value.

**Syntax:**
```sql
SELECT MIN(column) FROM table_name
```

**Examples:**

```sql
-- Minimum price
amidb> SELECT MIN(price) FROM products

Row 1: 15

1 row returned.

-- Minimum with WHERE
amidb> SELECT MIN(price) FROM products WHERE category = 'Computer'

Row 1: 299

1 row returned.
```

**Notes:**
- Only works on INTEGER columns
- NULL values are ignored
- Returns 0 for empty tables

### MAX

Finds the maximum INTEGER value.

**Syntax:**
```sql
SELECT MAX(column) FROM table_name
```

**Examples:**

```sql
-- Maximum price
amidb> SELECT MAX(price) FROM products

Row 1: 1299

1 row returned.

-- Maximum with WHERE
amidb> SELECT MAX(age) FROM users WHERE country = 'UK'

Row 1: 67

1 row returned.
```

**Notes:**
- Only works on INTEGER columns
- NULL values are ignored
- Returns 0 for empty tables

---

## Script Execution

### Running SQL Scripts

```bash
./amidb_shell database.db script.sql
```

This:
1. Opens/creates the database
2. Executes all statements in the script
3. Flushes changes to disk
4. Enters interactive mode

### Script Format

```sql
-- This is a comment
# This is also a comment

-- CREATE statements
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name TEXT,
    price INTEGER
);

-- INSERT statements
INSERT INTO products VALUES (1, 'Amiga 500', 299);
INSERT INTO products VALUES (2, 'Amiga 1200', 499);

-- Queries
SELECT * FROM products;

-- Comments can be inline
INSERT INTO products VALUES (3, 'Mouse', 25);  -- peripherals
```

### Script Rules

1. **Semicolons required** - Each statement must end with `;`
2. **Multi-line OK** - Statements can span multiple lines
3. **Comments** - Use `--` or `#` (everything after is ignored)
4. **Empty lines** - Ignored
5. **Error handling** - Errors are reported but execution continues

### Example Script: showcase.sql

```sql
-- AmiDB Showcase Database
-- A retro computer store inventory system

-- Products table
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name TEXT,
    price INTEGER,
    category TEXT,
    stock INTEGER
);

-- Sample data
INSERT INTO products VALUES (1, 'Amiga 500', 299, 'Computer', 5);
INSERT INTO products VALUES (2, 'Amiga 1200', 499, 'Computer', 3);
INSERT INTO products VALUES (3, 'Amiga 4000', 1299, 'Computer', 1);
INSERT INTO products VALUES (4, 'Competition Pro', 25, 'Peripheral', 20);
INSERT INTO products VALUES (5, 'Tank Mouse', 35, 'Peripheral', 15);

-- Customers table
CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    name TEXT,
    country TEXT
);

INSERT INTO customers VALUES (1, 'John Smith', 'UK');
INSERT INTO customers VALUES (2, 'Hans Mueller', 'Germany');
INSERT INTO customers VALUES (3, 'Pierre Dubois', 'France');

-- Verify data
SELECT * FROM products;
SELECT * FROM customers;
```

### Script Output

```
$ ./amidb_shell RAM:store.db showcase.sql
Executing script: showcase.sql

>> CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER, category TEXT, stock INTEGER)
Table created successfully.
>> INSERT INTO products VALUES (1, 'Amiga 500', 299, 'Computer', 5)
Row inserted successfully.
>> INSERT INTO products VALUES (2, 'Amiga 1200', 499, 'Computer', 3)
Row inserted successfully.
...
>> SELECT * FROM products

Row 1: 1, 'Amiga 500', 299, 'Computer', 5
Row 2: 2, 'Amiga 1200', 499, 'Computer', 3
...

Script executed successfully!

amidb>
```

---

## Practical Examples

### Example 1: User Management System

```sql
-- Create users table
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username TEXT,
    email TEXT,
    age INTEGER,
    active INTEGER
);

-- Add users
INSERT INTO users VALUES (1, 'alice', 'alice@example.com', 28, 1);
INSERT INTO users VALUES (2, 'bob', 'bob@example.com', 34, 1);
INSERT INTO users VALUES (3, 'carol', 'carol@example.com', 22, 1);
INSERT INTO users VALUES (4, 'dave', 'dave@example.com', 45, 0);
INSERT INTO users VALUES (5, 'eve', 'eve@example.com', 31, 1);

-- List all active users
SELECT * FROM users WHERE active = 1;

-- Find users over 30
SELECT * FROM users WHERE age > 30;

-- Count active users
SELECT COUNT(*) FROM users WHERE active = 1;

-- Deactivate a user
UPDATE users SET active = 0 WHERE id = 2;

-- Delete inactive users
DELETE FROM users WHERE active = 0;
```

### Example 2: Inventory Tracking

```sql
-- Create inventory table
CREATE TABLE inventory (
    id INTEGER PRIMARY KEY,
    product TEXT,
    quantity INTEGER,
    min_stock INTEGER,
    price INTEGER
);

-- Add items
INSERT INTO inventory VALUES (1, 'Amiga 500', 10, 2, 299);
INSERT INTO inventory VALUES (2, 'Amiga 1200', 5, 1, 499);
INSERT INTO inventory VALUES (3, 'Mouse', 50, 10, 25);
INSERT INTO inventory VALUES (4, 'Joystick', 30, 5, 15);
INSERT INTO inventory VALUES (5, 'Monitor', 3, 1, 350);

-- Check low stock items
SELECT * FROM inventory WHERE quantity <= min_stock;

-- Calculate total inventory value
SELECT SUM(quantity) FROM inventory;

-- Find most expensive item
SELECT MAX(price) FROM inventory;

-- Find cheapest item
SELECT MIN(price) FROM inventory;

-- Average price
SELECT AVG(price) FROM inventory;

-- Update stock after sale
UPDATE inventory SET quantity = 9 WHERE id = 1;

-- Delete discontinued item
DELETE FROM inventory WHERE id = 5;
```

### Example 3: High Score System

```sql
-- Create scores table
CREATE TABLE highscores (
    id INTEGER PRIMARY KEY,
    player TEXT,
    game TEXT,
    score INTEGER,
    level INTEGER
);

-- Add scores
INSERT INTO highscores VALUES (1, 'ACE', 'Turrican', 125000, 5);
INSERT INTO highscores VALUES (2, 'BOB', 'Turrican', 98500, 4);
INSERT INTO highscores VALUES (3, 'ACE', 'Shadow of the Beast', 45000, 3);
INSERT INTO highscores VALUES (4, 'CAT', 'Turrican', 156000, 6);
INSERT INTO highscores VALUES (5, 'BOB', 'Shadow of the Beast', 62000, 4);

-- Top scores for Turrican
SELECT * FROM highscores WHERE game = 'Turrican' ORDER BY score DESC;

-- All scores by player ACE
SELECT * FROM highscores WHERE player = 'ACE';

-- Highest score overall
SELECT MAX(score) FROM highscores;

-- Average score
SELECT AVG(score) FROM highscores;

-- Number of entries per game (approximate - use multiple queries)
SELECT COUNT(*) FROM highscores WHERE game = 'Turrican';
SELECT COUNT(*) FROM highscores WHERE game = 'Shadow of the Beast';
```

### Example 4: Event Logger

```sql
-- Create log table (with implicit rowid)
CREATE TABLE events (
    timestamp INTEGER,
    level INTEGER,
    message TEXT
);

-- Log some events (timestamp is Unix-style)
INSERT INTO events VALUES (1703800000, 1, 'System started');
INSERT INTO events VALUES (1703800100, 1, 'User login: alice');
INSERT INTO events VALUES (1703800200, 2, 'Warning: low memory');
INSERT INTO events VALUES (1703800300, 3, 'Error: disk full');
INSERT INTO events VALUES (1703800400, 1, 'User logout: alice');

-- View all events
SELECT * FROM events ORDER BY timestamp;

-- View errors only (level 3)
SELECT * FROM events WHERE level = 3;

-- View warnings and errors
SELECT * FROM events WHERE level >= 2;

-- Count by level
SELECT COUNT(*) FROM events WHERE level = 1;
SELECT COUNT(*) FROM events WHERE level = 2;
SELECT COUNT(*) FROM events WHERE level = 3;

-- Delete old logs
DELETE FROM events WHERE timestamp < 1703800200;
```

---

## Tips and Tricks

### 1. Use Primary Key for Fast Lookups

```sql
-- Fast (uses B+Tree index)
SELECT * FROM users WHERE id = 42

-- Slow (full table scan)
SELECT * FROM users WHERE name = 'Alice'
```

### 2. Check Schema Before Queries

```
amidb> .schema users
```

This shows column names and types, helping you write correct queries.

### 3. Use LIMIT for Large Tables

```sql
-- Prevent overwhelming output
SELECT * FROM logs LIMIT 20
```

### 4. Combine Clauses Wisely

```sql
-- Filter, sort, then limit
SELECT * FROM products WHERE price > 50 ORDER BY price DESC LIMIT 10
```

### 5. Scripts for Repeatable Setup

Create a `setup.sql` file:
```sql
CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price INTEGER);
CREATE TABLE orders (id INTEGER PRIMARY KEY, product_id INTEGER, quantity INTEGER);
```

Run it:
```bash
./amidb_shell mydb.db setup.sql
```

### 6. Use RAM: for Testing

```bash
./amidb_shell RAM:test.db
```

Faster I/O and automatically cleaned up on reboot.

### 7. Check Row Counts with COUNT

```sql
-- Before bulk delete
SELECT COUNT(*) FROM logs WHERE level < 2

-- After delete
DELETE FROM logs WHERE level < 2
SELECT COUNT(*) FROM logs
```

### 8. Verify Updates with SELECT

```sql
-- Before
SELECT * FROM users WHERE id = 1

-- Update
UPDATE users SET age = 31 WHERE id = 1

-- After
SELECT * FROM users WHERE id = 1
```

---

## Troubleshooting

### "Parse error: Invalid SQL syntax"

**Causes:**
- Missing semicolon in scripts
- Misspelled keyword
- Wrong quote type (use single quotes `'text'`)
- Missing comma in column list

**Solutions:**
```sql
-- Wrong
INSERT INTO users VALUES (1, "Alice")  -- Double quotes
SELECT * FROM users  -- Missing semicolon in script

-- Right
INSERT INTO users VALUES (1, 'Alice')  -- Single quotes
SELECT * FROM users;  -- Semicolon for scripts
```

### "Error: Table 'xxx' not found"

**Causes:**
- Table doesn't exist
- Case-sensitive table name
- Database file changed

**Solutions:**
```sql
-- Check existing tables
.tables

-- Verify table name case
.schema MyTable  -- Exact case matters
```

### "Error: Table 'xxx' already exists"

**Cause:** Trying to create a table that exists.

**Solution:**
```sql
-- Drop first if needed
DROP TABLE users
CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)
```

### "No rows returned"

**Causes:**
- Table is empty
- WHERE condition matches nothing
- Wrong column name in WHERE

**Solutions:**
```sql
-- Check table contents
SELECT * FROM users

-- Check without WHERE
SELECT * FROM users  -- Then add WHERE

-- Verify column names
.schema users
```

### "Error: Column 'xxx' not found"

**Cause:** Referencing non-existent column.

**Solution:**
```sql
-- Check schema
.schema products

-- Use correct column name
SELECT * FROM products WHERE name = 'Amiga'  -- Not 'title'
```

### Empty TEXT Fields

**Cause:** Inserting empty string.

**Solution:**
```sql
-- Provide actual text
INSERT INTO users VALUES (1, 'Name Here')
```

### Garbage in TEXT Output

**Cause:** This was a bug (fixed). If you see garbage characters after TEXT values, you may have an old version.

**Solution:** Rebuild from latest source.

### Script Errors

**Symptoms:**
- `[Error at line X]`
- Incomplete command warnings

**Solutions:**
1. Ensure all statements end with `;`
2. Check for unclosed quotes
3. Verify syntax of each statement
4. Check that multi-line statements are properly continued

### Memory Issues

**Symptoms:**
- Crashes on large operations
- Slow performance

**Solutions:**
1. Use smaller LIMIT values
2. Delete unneeded rows
3. Restart shell periodically
4. Use RAM: for temporary databases

---

## Quick Reference Card

### Meta-Commands

| Command | Description |
|---------|-------------|
| `.help` | Show help |
| `.quit` | Exit shell |
| `.exit` | Exit shell |
| `.tables` | List tables |
| `.schema <table>` | Show schema |

### SQL Statements

| Statement | Example |
|-----------|---------|
| CREATE TABLE | `CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)` |
| DROP TABLE | `DROP TABLE t` |
| INSERT | `INSERT INTO t VALUES (1, 'Alice')` |
| SELECT | `SELECT * FROM t WHERE id > 5` |
| UPDATE | `UPDATE t SET name = 'Bob' WHERE id = 1` |
| DELETE | `DELETE FROM t WHERE id = 1` |

### Aggregate Functions

| Function | Example |
|----------|---------|
| COUNT(*) | `SELECT COUNT(*) FROM t` |
| COUNT(col) | `SELECT COUNT(name) FROM t` |
| SUM | `SELECT SUM(price) FROM t` |
| AVG | `SELECT AVG(price) FROM t` |
| MIN | `SELECT MIN(price) FROM t` |
| MAX | `SELECT MAX(price) FROM t` |

### WHERE Operators

| Operator | Example |
|----------|---------|
| = | `WHERE id = 1` |
| != | `WHERE status != 'deleted'` |
| <> | `WHERE status <> 'deleted'` |
| < | `WHERE price < 100` |
| <= | `WHERE price <= 100` |
| > | `WHERE price > 100` |
| >= | `WHERE price >= 100` |

---

## Next Steps

1. Try the [Cross-Compile Setup Guide](01-Cross-Compile-Setup.md) to build AmiDB
2. Read [Using AmiDB as a Library](02-AmiDB-Library-Usage.md) for programmatic access
3. Explore the `showcase.sql` demo script included with AmiDB
4. Build your own Amiga applications with embedded AmiDB!

Happy database hacking on your Amiga!
