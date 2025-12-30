-- ============================================================
-- Retro Game Store Database
-- ============================================================
-- A comprehensive e-commerce database for a retro gaming store.
-- Demonstrates complex relational data modeling with AmiDB.
--
-- Tables:
--   - product_types: Categories of products
--   - conditions: Item condition grades
--   - products: Inventory of items for sale
--   - countries: Country reference table
--   - customers: Customer accounts
--   - addresses: Customer shipping/billing addresses
--   - orders: Customer orders
--   - order_items: Line items within orders
--   - payment_methods: Payment type reference
--   - payments: Payment transactions
--   - inventory_log: Stock movement tracking
--
-- Usage:
--   ./amidb_shell RAM:gamestore.db 02_retro_game_store.sql
-- ============================================================

-- ============================================================
-- PRODUCT_TYPES - Categories of products
-- ============================================================
CREATE TABLE product_types (
    id INTEGER PRIMARY KEY,
    name TEXT,
    description TEXT,
    parent_id INTEGER
);

-- Main categories (parent_id = 0 means top-level)
INSERT INTO product_types VALUES (1, 'Hardware', 'Computer and console hardware', 0);
INSERT INTO product_types VALUES (2, 'Software', 'Games and applications', 0);
INSERT INTO product_types VALUES (3, 'Accessories', 'Peripherals and add-ons', 0);
INSERT INTO product_types VALUES (4, 'Media', 'Books, magazines, manuals', 0);
INSERT INTO product_types VALUES (5, 'Collectibles', 'Rare and collectible items', 0);

-- Hardware subcategories
INSERT INTO product_types VALUES (10, 'Computers', 'Complete computer systems', 1);
INSERT INTO product_types VALUES (11, 'Consoles', 'Gaming consoles', 1);
INSERT INTO product_types VALUES (12, 'Components', 'Replacement parts', 1);
INSERT INTO product_types VALUES (13, 'Monitors', 'Display devices', 1);

-- Software subcategories
INSERT INTO product_types VALUES (20, 'Games', 'Video games', 2);
INSERT INTO product_types VALUES (21, 'Productivity', 'Applications and tools', 2);
INSERT INTO product_types VALUES (22, 'Development', 'Programming tools', 2);
INSERT INTO product_types VALUES (23, 'Demos', 'Demo disks and coverdisks', 2);

-- Accessories subcategories
INSERT INTO product_types VALUES (30, 'Controllers', 'Joysticks, mice, gamepads', 3);
INSERT INTO product_types VALUES (31, 'Storage', 'Drives and memory', 3);
INSERT INTO product_types VALUES (32, 'Cables', 'Connectors and adapters', 3);
INSERT INTO product_types VALUES (33, 'Cases', 'Enclosures and covers', 3);

-- ============================================================
-- CONDITIONS - Item condition grades
-- ============================================================
CREATE TABLE conditions (
    id INTEGER PRIMARY KEY,
    grade TEXT,
    description TEXT,
    price_modifier INTEGER
);

-- Condition grades (price_modifier is percentage, 100 = full price)
INSERT INTO conditions VALUES (1, 'Mint', 'Perfect, like new condition', 150);
INSERT INTO conditions VALUES (2, 'Excellent', 'Minor signs of use only', 120);
INSERT INTO conditions VALUES (3, 'Very Good', 'Light wear, fully functional', 100);
INSERT INTO conditions VALUES (4, 'Good', 'Normal wear, works perfectly', 80);
INSERT INTO conditions VALUES (5, 'Fair', 'Heavy wear but functional', 60);
INSERT INTO conditions VALUES (6, 'Poor', 'Damaged but working', 40);
INSERT INTO conditions VALUES (7, 'For Parts', 'Not working, parts only', 20);
INSERT INTO conditions VALUES (8, 'New Sealed', 'Factory sealed, never opened', 200);

-- ============================================================
-- PRODUCTS - Inventory of items for sale
-- ============================================================
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    sku TEXT,
    name TEXT,
    description TEXT,
    type_id INTEGER,
    condition_id INTEGER,
    base_price INTEGER,
    stock INTEGER,
    added_date INTEGER
);

-- Computers (type 10)
INSERT INTO products VALUES (1, 'HW-A500-001', 'Amiga 500 Rev 6A', 'PAL Amiga 500 with 1MB chip RAM', 10, 3, 14900, 3, 20231015);
INSERT INTO products VALUES (2, 'HW-A500-002', 'Amiga 500 Plus', 'Amiga 500+ with ECS chipset', 10, 4, 17900, 2, 20231020);
INSERT INTO products VALUES (3, 'HW-A1200-001', 'Amiga 1200', 'Standard A1200 2MB chip RAM', 10, 3, 29900, 1, 20231101);
INSERT INTO products VALUES (4, 'HW-A1200-002', 'Amiga 1200 Tower', 'A1200 in custom tower case', 10, 2, 49900, 1, 20231105);
INSERT INTO products VALUES (5, 'HW-A4000-001', 'Amiga 4000/040', 'A4000 with 68040 25MHz', 10, 4, 89900, 1, 20231110);
INSERT INTO products VALUES (6, 'HW-C64-001', 'Commodore 64 Breadbin', 'Original brown case C64', 10, 4, 7900, 5, 20231001);
INSERT INTO products VALUES (7, 'HW-C64-002', 'Commodore 64C', 'Newer white case model', 10, 3, 8900, 3, 20231005);

-- Consoles (type 11)
INSERT INTO products VALUES (8, 'HW-CD32-001', 'Amiga CD32', 'PAL CD32 with controller', 11, 3, 19900, 2, 20231115);
INSERT INTO products VALUES (9, 'HW-CDTV-001', 'Commodore CDTV', 'CDTV with remote and keyboard', 11, 4, 24900, 1, 20231120);

-- Games (type 20)
INSERT INTO products VALUES (10, 'SW-TUR2-001', 'Turrican II', 'Complete in box, 2 disks', 20, 2, 4900, 4, 20231001);
INSERT INTO products VALUES (11, 'SW-LEM-001', 'Lemmings', 'Original release with poster', 20, 3, 2900, 6, 20231002);
INSERT INTO products VALUES (12, 'SW-SOB-001', 'Shadow of the Beast', 'Big box edition', 20, 2, 5900, 2, 20231003);
INSERT INTO products VALUES (13, 'SW-SEN-001', 'Sensible Soccer', 'Complete with manual', 20, 3, 1900, 8, 20231004);
INSERT INTO products VALUES (14, 'SW-WRM-001', 'Worms', 'Team17 original release', 20, 3, 2900, 5, 20231005);
INSERT INTO products VALUES (15, 'SW-SPB-001', 'Speedball 2', 'Bitmap Brothers classic', 20, 4, 2400, 3, 20231006);
INSERT INTO products VALUES (16, 'SW-CHE-001', 'Chaos Engine', 'Complete big box', 20, 2, 4400, 2, 20231007);
INSERT INTO products VALUES (17, 'SW-CAN-001', 'Cannon Fodder', 'War has never been so much fun', 20, 3, 3400, 4, 20231008);
INSERT INTO products VALUES (18, 'SW-POP-001', 'Populous', 'God game original', 20, 4, 1900, 5, 20231009);
INSERT INTO products VALUES (19, 'SW-ANO-001', 'Another World', 'Delphine masterpiece', 20, 2, 5400, 2, 20231010);
INSERT INTO products VALUES (20, 'SW-FLA-001', 'Flashback', 'Complete with poster', 20, 3, 3900, 3, 20231011);

-- Controllers (type 30)
INSERT INTO products VALUES (21, 'ACC-JOY-001', 'Competition Pro', 'Black edition joystick', 30, 3, 2900, 8, 20231101);
INSERT INTO products VALUES (22, 'ACC-JOY-002', 'Zipstik', 'Autofire joystick', 30, 4, 1900, 5, 20231102);
INSERT INTO products VALUES (23, 'ACC-JOY-003', 'QuickShot II Turbo', 'With suction cups', 30, 3, 1400, 12, 20231103);
INSERT INTO products VALUES (24, 'ACC-MOU-001', 'Amiga Tank Mouse', 'Original Commodore mouse', 30, 3, 2400, 6, 20231104);
INSERT INTO products VALUES (25, 'ACC-MOU-002', 'Amiga Mouse Beige', 'A1200/A4000 style', 30, 2, 2900, 4, 20231105);

-- Storage (type 31)
INSERT INTO products VALUES (26, 'ACC-DRV-001', 'External Floppy Drive', 'Amiga external 880K', 31, 3, 4900, 3, 20231106);
INSERT INTO products VALUES (27, 'ACC-HDD-001', 'SCSI Hard Drive 540MB', 'For A2000/A3000/A4000', 31, 4, 3900, 2, 20231107);
INSERT INTO products VALUES (28, 'ACC-MEM-001', 'A500 512K Trapdoor', 'Slow RAM expansion', 31, 3, 1900, 7, 20231108);
INSERT INTO products VALUES (29, 'ACC-MEM-002', 'A1200 4MB Fast RAM', '4MB PCMCIA expansion', 31, 2, 4900, 2, 20231109);
INSERT INTO products VALUES (30, 'ACC-CF-001', 'CF Card Adapter', 'IDE to CF for A1200', 31, 1, 2900, 10, 20231110);

-- ============================================================
-- COUNTRIES - Country reference
-- ============================================================
CREATE TABLE countries (
    id INTEGER PRIMARY KEY,
    code TEXT,
    name TEXT,
    shipping_zone INTEGER
);

INSERT INTO countries VALUES (1, 'UK', 'United Kingdom', 1);
INSERT INTO countries VALUES (2, 'DE', 'Germany', 2);
INSERT INTO countries VALUES (3, 'FR', 'France', 2);
INSERT INTO countries VALUES (4, 'NL', 'Netherlands', 2);
INSERT INTO countries VALUES (5, 'SE', 'Sweden', 2);
INSERT INTO countries VALUES (6, 'US', 'United States', 3);
INSERT INTO countries VALUES (7, 'CA', 'Canada', 3);
INSERT INTO countries VALUES (8, 'AU', 'Australia', 4);
INSERT INTO countries VALUES (9, 'JP', 'Japan', 4);
INSERT INTO countries VALUES (10, 'IT', 'Italy', 2);

-- ============================================================
-- CUSTOMERS - Customer accounts
-- ============================================================
CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    email TEXT,
    first_name TEXT,
    last_name TEXT,
    phone TEXT,
    created_date INTEGER,
    last_order INTEGER,
    total_orders INTEGER,
    total_spent INTEGER
);

INSERT INTO customers VALUES (1, 'john.smith@email.co.uk', 'John', 'Smith', '07700900001', 20230115, 20231201, 5, 35600);
INSERT INTO customers VALUES (2, 'anna.mueller@email.de', 'Anna', 'Mueller', '4915123456', 20230220, 20231128, 3, 18900);
INSERT INTO customers VALUES (3, 'pierre.dubois@email.fr', 'Pierre', 'Dubois', '33612345678', 20230305, 20231115, 2, 8900);
INSERT INTO customers VALUES (4, 'erik.svensson@email.se', 'Erik', 'Svensson', '46701234567', 20230410, 20231205, 4, 42500);
INSERT INTO customers VALUES (5, 'mike.jones@email.com', 'Mike', 'Jones', '12025551234', 20230515, 20231130, 6, 67800);
INSERT INTO customers VALUES (6, 'sarah.wilson@email.co.uk', 'Sarah', 'Wilson', '07700900002', 20230620, 20231125, 2, 12400);
INSERT INTO customers VALUES (7, 'hans.schmidt@email.de', 'Hans', 'Schmidt', '4915234567', 20230725, 20231210, 7, 98700);
INSERT INTO customers VALUES (8, 'jan.devries@email.nl', 'Jan', 'de Vries', '31612345678', 20230830, 20231118, 1, 4900);
INSERT INTO customers VALUES (9, 'marco.rossi@email.it', 'Marco', 'Rossi', '39312345678', 20230905, 20231122, 3, 21500);
INSERT INTO customers VALUES (10, 'david.brown@email.au', 'David', 'Brown', '61412345678', 20231010, 20231208, 2, 34900);

-- ============================================================
-- ADDRESSES - Customer shipping/billing addresses
-- ============================================================
CREATE TABLE addresses (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER,
    address_type TEXT,
    street1 TEXT,
    street2 TEXT,
    city TEXT,
    postal_code TEXT,
    country_id INTEGER,
    is_default INTEGER
);

INSERT INTO addresses VALUES (1, 1, 'shipping', '123 High Street', 'Flat 4', 'London', 'SW1A 1AA', 1, 1);
INSERT INTO addresses VALUES (2, 1, 'billing', '123 High Street', 'Flat 4', 'London', 'SW1A 1AA', 1, 1);
INSERT INTO addresses VALUES (3, 2, 'shipping', 'Hauptstrasse 42', '', 'Berlin', '10115', 2, 1);
INSERT INTO addresses VALUES (4, 2, 'billing', 'Hauptstrasse 42', '', 'Berlin', '10115', 2, 1);
INSERT INTO addresses VALUES (5, 3, 'shipping', '15 Rue de la Paix', '', 'Paris', '75002', 3, 1);
INSERT INTO addresses VALUES (6, 4, 'shipping', 'Kungsgatan 10', 'Lgh 301', 'Stockholm', '11143', 5, 1);
INSERT INTO addresses VALUES (7, 5, 'shipping', '456 Main Street', 'Apt 2B', 'New York', '10001', 6, 1);
INSERT INTO addresses VALUES (8, 6, 'shipping', '78 Church Lane', '', 'Manchester', 'M1 1AA', 1, 1);
INSERT INTO addresses VALUES (9, 7, 'shipping', 'Bahnhofstr 99', '', 'Munich', '80335', 2, 1);
INSERT INTO addresses VALUES (10, 7, 'work', 'Industrieweg 5', '', 'Munich', '80339', 2, 0);
INSERT INTO addresses VALUES (11, 8, 'shipping', 'Damrak 55', '', 'Amsterdam', '1012 LM', 4, 1);
INSERT INTO addresses VALUES (12, 9, 'shipping', 'Via Roma 25', '', 'Milan', '20121', 10, 1);
INSERT INTO addresses VALUES (13, 10, 'shipping', '88 George Street', '', 'Sydney', '2000', 8, 1);

-- ============================================================
-- ORDERS - Customer orders
-- ============================================================
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER,
    shipping_address_id INTEGER,
    order_date INTEGER,
    status TEXT,
    subtotal INTEGER,
    shipping INTEGER,
    tax INTEGER,
    total INTEGER,
    notes TEXT
);

-- Status: pending, paid, shipped, delivered, cancelled
INSERT INTO orders VALUES (1, 1, 1, 20231201, 'delivered', 19800, 500, 0, 20300, 'Gift wrap requested');
INSERT INTO orders VALUES (2, 2, 3, 20231128, 'delivered', 14800, 1200, 0, 16000, '');
INSERT INTO orders VALUES (3, 3, 5, 20231115, 'delivered', 7900, 1500, 0, 9400, '');
INSERT INTO orders VALUES (4, 4, 6, 20231205, 'shipped', 29900, 1800, 0, 31700, 'Fragile - Amiga 1200');
INSERT INTO orders VALUES (5, 5, 7, 20231130, 'delivered', 8700, 2500, 0, 11200, '');
INSERT INTO orders VALUES (6, 5, 7, 20231210, 'pending', 49900, 3500, 0, 53400, 'Amiga 4000 - insured');
INSERT INTO orders VALUES (7, 6, 8, 20231125, 'delivered', 5800, 500, 0, 6300, '');
INSERT INTO orders VALUES (8, 7, 9, 20231210, 'paid', 34500, 1200, 0, 35700, 'Bulk order');
INSERT INTO orders VALUES (9, 8, 11, 20231118, 'delivered', 4900, 1200, 0, 6100, '');
INSERT INTO orders VALUES (10, 9, 12, 20231122, 'shipped', 12800, 1800, 0, 14600, '');
INSERT INTO orders VALUES (11, 10, 13, 20231208, 'paid', 29900, 4500, 0, 34400, 'Express shipping');
INSERT INTO orders VALUES (12, 7, 10, 20231215, 'pending', 17900, 1200, 0, 19100, 'Work address');

-- ============================================================
-- ORDER_ITEMS - Line items within orders
-- ============================================================
CREATE TABLE order_items (
    id INTEGER PRIMARY KEY,
    order_id INTEGER,
    product_id INTEGER,
    quantity INTEGER,
    unit_price INTEGER,
    line_total INTEGER
);

-- Order 1: John bought Turrican II and Lemmings
INSERT INTO order_items VALUES (1, 1, 10, 1, 4900, 4900);
INSERT INTO order_items VALUES (2, 1, 11, 1, 2900, 2900);
INSERT INTO order_items VALUES (3, 1, 14, 1, 2900, 2900);
INSERT INTO order_items VALUES (4, 1, 17, 1, 3400, 3400);
INSERT INTO order_items VALUES (5, 1, 13, 1, 1900, 1900);
INSERT INTO order_items VALUES (6, 1, 18, 1, 1900, 1900);
INSERT INTO order_items VALUES (7, 1, 15, 1, 2400, 2400);

-- Order 2: Anna bought games
INSERT INTO order_items VALUES (8, 2, 12, 1, 5900, 5900);
INSERT INTO order_items VALUES (9, 2, 19, 1, 5400, 5400);
INSERT INTO order_items VALUES (10, 2, 17, 1, 3400, 3400);

-- Order 3: Pierre bought C64
INSERT INTO order_items VALUES (11, 3, 6, 1, 7900, 7900);

-- Order 4: Erik bought Amiga 1200
INSERT INTO order_items VALUES (12, 4, 3, 1, 29900, 29900);

-- Order 5: Mike bought accessories
INSERT INTO order_items VALUES (13, 5, 21, 1, 2900, 2900);
INSERT INTO order_items VALUES (14, 5, 24, 1, 2400, 2400);
INSERT INTO order_items VALUES (15, 5, 28, 1, 1900, 1900);
INSERT INTO order_items VALUES (16, 5, 23, 1, 1400, 1400);

-- Order 6: Mike pending A4000
INSERT INTO order_items VALUES (17, 6, 5, 1, 49900, 49900);

-- Order 7: Sarah bought games
INSERT INTO order_items VALUES (18, 7, 10, 1, 4900, 4900);
INSERT INTO order_items VALUES (19, 7, 11, 1, 2900, 2900);

-- Order 8: Hans bulk order
INSERT INTO order_items VALUES (20, 8, 16, 1, 4400, 4400);
INSERT INTO order_items VALUES (21, 8, 20, 1, 3900, 3900);
INSERT INTO order_items VALUES (22, 8, 12, 1, 5900, 5900);
INSERT INTO order_items VALUES (23, 8, 19, 1, 5400, 5400);
INSERT INTO order_items VALUES (24, 8, 26, 1, 4900, 4900);
INSERT INTO order_items VALUES (25, 8, 29, 1, 4900, 4900);
INSERT INTO order_items VALUES (26, 8, 30, 2, 2900, 5800);

-- Order 9: Jan bought Turrican II
INSERT INTO order_items VALUES (27, 9, 10, 1, 4900, 4900);

-- Order 10: Marco
INSERT INTO order_items VALUES (28, 10, 15, 1, 2400, 2400);
INSERT INTO order_items VALUES (29, 10, 16, 1, 4400, 4400);
INSERT INTO order_items VALUES (30, 10, 17, 1, 3400, 3400);
INSERT INTO order_items VALUES (31, 10, 22, 1, 1900, 1900);

-- Order 11: David in Australia
INSERT INTO order_items VALUES (32, 11, 3, 1, 29900, 29900);

-- Order 12: Hans work order
INSERT INTO order_items VALUES (33, 12, 2, 1, 17900, 17900);

-- ============================================================
-- PAYMENT_METHODS - Payment type reference
-- ============================================================
CREATE TABLE payment_methods (
    id INTEGER PRIMARY KEY,
    name TEXT,
    description TEXT,
    active INTEGER
);

INSERT INTO payment_methods VALUES (1, 'Credit Card', 'Visa/Mastercard', 1);
INSERT INTO payment_methods VALUES (2, 'PayPal', 'PayPal account', 1);
INSERT INTO payment_methods VALUES (3, 'Bank Transfer', 'Direct bank transfer', 1);
INSERT INTO payment_methods VALUES (4, 'Cash on Delivery', 'Pay when delivered', 0);

-- ============================================================
-- PAYMENTS - Payment transactions
-- ============================================================
CREATE TABLE payments (
    id INTEGER PRIMARY KEY,
    order_id INTEGER,
    method_id INTEGER,
    amount INTEGER,
    status TEXT,
    transaction_ref TEXT,
    payment_date INTEGER
);

-- Status: pending, completed, failed, refunded
INSERT INTO payments VALUES (1, 1, 1, 20300, 'completed', 'TXN-20231201-001', 20231201);
INSERT INTO payments VALUES (2, 2, 2, 16000, 'completed', 'PP-DE-20231128', 20231128);
INSERT INTO payments VALUES (3, 3, 3, 9400, 'completed', 'BT-FR-20231115', 20231116);
INSERT INTO payments VALUES (4, 4, 1, 31700, 'completed', 'TXN-20231205-001', 20231205);
INSERT INTO payments VALUES (5, 5, 2, 11200, 'completed', 'PP-US-20231130', 20231130);
INSERT INTO payments VALUES (6, 7, 1, 6300, 'completed', 'TXN-20231125-001', 20231125);
INSERT INTO payments VALUES (7, 8, 3, 35700, 'completed', 'BT-DE-20231210', 20231211);
INSERT INTO payments VALUES (8, 9, 2, 6100, 'completed', 'PP-NL-20231118', 20231118);
INSERT INTO payments VALUES (9, 10, 1, 14600, 'completed', 'TXN-20231122-001', 20231122);
INSERT INTO payments VALUES (10, 11, 2, 34400, 'completed', 'PP-AU-20231208', 20231208);

-- ============================================================
-- INVENTORY_LOG - Stock movement tracking
-- ============================================================
CREATE TABLE inventory_log (
    id INTEGER PRIMARY KEY,
    product_id INTEGER,
    change_type TEXT,
    quantity INTEGER,
    reference_id INTEGER,
    log_date INTEGER,
    notes TEXT
);

-- Change types: received, sold, returned, adjustment, damaged
INSERT INTO inventory_log VALUES (1, 1, 'received', 5, 0, 20231015, 'Initial stock');
INSERT INTO inventory_log VALUES (2, 2, 'received', 3, 0, 20231020, 'Initial stock');
INSERT INTO inventory_log VALUES (3, 3, 'received', 3, 0, 20231101, 'Initial stock');
INSERT INTO inventory_log VALUES (4, 10, 'received', 6, 0, 20231001, 'Initial stock');
INSERT INTO inventory_log VALUES (5, 10, 'sold', 1, 1, 20231201, 'Order 1');
INSERT INTO inventory_log VALUES (6, 11, 'sold', 1, 1, 20231201, 'Order 1');
INSERT INTO inventory_log VALUES (7, 10, 'sold', 1, 7, 20231125, 'Order 7');
INSERT INTO inventory_log VALUES (8, 11, 'sold', 1, 7, 20231125, 'Order 7');
INSERT INTO inventory_log VALUES (9, 3, 'sold', 1, 4, 20231205, 'Order 4');
INSERT INTO inventory_log VALUES (10, 6, 'sold', 1, 3, 20231115, 'Order 3');
INSERT INTO inventory_log VALUES (11, 5, 'reserved', 1, 6, 20231210, 'Order 6 pending');
INSERT INTO inventory_log VALUES (12, 21, 'received', 10, 0, 20231101, 'Bulk purchase');
INSERT INTO inventory_log VALUES (13, 21, 'sold', 1, 5, 20231130, 'Order 5');
INSERT INTO inventory_log VALUES (14, 3, 'sold', 1, 11, 20231208, 'Order 11');
INSERT INTO inventory_log VALUES (15, 30, 'received', 15, 0, 20231110, 'New CF adapters');
INSERT INTO inventory_log VALUES (16, 30, 'sold', 2, 8, 20231210, 'Order 8');

-- ============================================================
-- VERIFICATION QUERIES
-- ============================================================

-- Table counts
SELECT COUNT(*) FROM product_types;
SELECT COUNT(*) FROM conditions;
SELECT COUNT(*) FROM products;
SELECT COUNT(*) FROM countries;
SELECT COUNT(*) FROM customers;
SELECT COUNT(*) FROM addresses;
SELECT COUNT(*) FROM orders;
SELECT COUNT(*) FROM order_items;
SELECT COUNT(*) FROM payment_methods;
SELECT COUNT(*) FROM payments;
SELECT COUNT(*) FROM inventory_log;

-- Business queries
-- Total revenue
SELECT SUM(total) FROM orders WHERE status != 'cancelled';

-- Average order value
SELECT AVG(total) FROM orders WHERE status != 'cancelled';

-- Most expensive product
SELECT MAX(base_price) FROM products;

-- Cheapest product
SELECT MIN(base_price) FROM products;

-- Products in stock
SELECT COUNT(*) FROM products WHERE stock > 0;

-- Low stock items (less than 3)
SELECT * FROM products WHERE stock < 3;

-- Pending orders
SELECT * FROM orders WHERE status = 'pending';

-- Top customer by total spent
SELECT MAX(total_spent) FROM customers;

-- Orders by status
SELECT COUNT(*) FROM orders WHERE status = 'delivered';
SELECT COUNT(*) FROM orders WHERE status = 'shipped';
SELECT COUNT(*) FROM orders WHERE status = 'paid';
SELECT COUNT(*) FROM orders WHERE status = 'pending';
