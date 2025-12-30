-- ================================================
-- UPDATE and DELETE Demo
-- ================================================
-- Demonstrates UPDATE and DELETE operations
-- Run with: ./amidb_shell demo.db update_delete_demo.sql
-- ================================================

-- Create a simple inventory table
CREATE TABLE inventory (
    id INTEGER PRIMARY KEY,
    product TEXT,
    stock INTEGER,
    price INTEGER
);

-- Insert initial data
INSERT INTO inventory VALUES (1, 'Amiga 500', 10, 299);
INSERT INTO inventory VALUES (2, 'Amiga 1200', 5, 499);
INSERT INTO inventory VALUES (3, 'Mouse', 50, 25);
INSERT INTO inventory VALUES (4, 'Joystick', 30, 19);
INSERT INTO inventory VALUES (5, 'Monitor', 8, 249);

-- ================================================
-- UPDATE Examples
-- ================================================

-- Update stock for a specific product (WHERE on PRIMARY KEY - fast path)
UPDATE inventory SET stock = 12 WHERE id = 1;

-- Update price for all mice
UPDATE inventory SET price = 22 WHERE product = 'Mouse';

-- Update stock for expensive items
UPDATE inventory SET stock = 3 WHERE price > 200;

-- ================================================
-- DELETE Examples
-- ================================================

-- Delete a specific product by ID (WHERE on PRIMARY KEY - fast path)
DELETE FROM inventory WHERE id = 4;

-- Delete products with low stock
DELETE FROM inventory WHERE stock < 5;

-- ================================================
-- Verification Queries
-- ================================================
-- After running this script, try these in the REPL:
--
-- View remaining inventory:
--   SELECT * FROM inventory;
--
-- Check specific product:
--   SELECT * FROM inventory WHERE id = 1;
--
-- List all tables:
--   .tables
--
-- View schema:
--   .schema inventory
-- ================================================
