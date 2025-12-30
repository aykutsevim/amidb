# Makefile for AmiDB - Cross-compilation with m68k-amigaos-gcc
# Build on Linux, run on Amiga

# Compiler settings
CC = /home/pi/amiga-gcc/bin/m68k-amigaos-gcc
AR = /home/pi/amiga-gcc/bin/m68k-amigaos-ar
RANLIB = /home/pi/amiga-gcc/bin/m68k-amigaos-ranlib

# Compiler flags for 68000
CFLAGS = -m68000 -O2 -Wall -fomit-frame-pointer -fno-builtin
CFLAGS += -noixemul -Isrc

# Linker flags
LDFLAGS = -m68000 -noixemul
LIBS = -lm

# Directories
SRC_DIR = src
OBJ_DIR = obj
TEST_DIR = tests
EXAMPLE_DIR = examples

# Source files
UTIL_SRCS = $(SRC_DIR)/util/crc32.c $(SRC_DIR)/util/hash.c
OS_SRCS = $(SRC_DIR)/os/file_amiga.c $(SRC_DIR)/os/mem_amiga.c
API_SRCS = $(SRC_DIR)/api/error.c
STORAGE_SRCS = $(SRC_DIR)/storage/pager.c $(SRC_DIR)/storage/cache.c $(SRC_DIR)/storage/row.c $(SRC_DIR)/storage/btree.c
TXN_SRCS = $(SRC_DIR)/txn/wal.c $(SRC_DIR)/txn/txn.c
SQL_SRCS = $(SRC_DIR)/sql/lexer.c $(SRC_DIR)/sql/parser.c $(SRC_DIR)/sql/catalog.c $(SRC_DIR)/sql/executor.c

# REPL source (only included in shell build)
REPL_SRCS = $(SRC_DIR)/sql/repl.c

# Test files
TEST_SRCS = $(TEST_DIR)/test_main.c $(TEST_DIR)/test_endian.c $(TEST_DIR)/test_crc32.c $(TEST_DIR)/test_pager.c $(TEST_DIR)/test_cache.c $(TEST_DIR)/test_row.c $(TEST_DIR)/test_btree_basic.c $(TEST_DIR)/test_btree_split.c $(TEST_DIR)/test_btree_merge.c $(TEST_DIR)/test_wal.c $(TEST_DIR)/test_txn.c $(TEST_DIR)/test_recovery.c $(TEST_DIR)/test_btree_txn.c $(TEST_DIR)/test_sql_lexer.c $(TEST_DIR)/test_sql_parser.c $(TEST_DIR)/test_sql_catalog.c $(TEST_DIR)/test_sql_e2e.c

# Example files
EXAMPLE_SRCS = $(EXAMPLE_DIR)/inventory_demo.c

# Object files
UTIL_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(UTIL_SRCS))
OS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(OS_SRCS))
API_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(API_SRCS))
STORAGE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(STORAGE_SRCS))
TXN_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TXN_SRCS))
SQL_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SQL_SRCS))
REPL_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(REPL_SRCS))
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))

# Core library objects (without REPL)
ALL_OBJS = $(UTIL_OBJS) $(OS_OBJS) $(API_OBJS) $(STORAGE_OBJS) $(TXN_OBJS) $(SQL_OBJS)

# Shell objects (with REPL)
SHELL_OBJS = $(ALL_OBJS) $(REPL_OBJS)

# Targets
.PHONY: all clean test help examples shell

all: amidb_tests amidb_shell examples

help:
	@echo "AmiDB Makefile for m68k-amigaos-gcc"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build everything (default)"
	@echo "  amidb_tests  - Build test executable (no REPL, smaller binary)"
	@echo "  amidb_shell  - Build interactive SQL shell (with REPL)"
	@echo "  examples     - Build example programs"
	@echo "  clean        - Remove all build files"
	@echo "  test         - Build and copy to Amiga (if path set)"
	@echo ""
	@echo "Usage:"
	@echo "  make              - Compile tests, shell, and examples"
	@echo "  make amidb_shell  - Compile only SQL shell"
	@echo "  make examples     - Compile only examples"
	@echo "  make clean        - Clean build files"

# Create obj directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)/util $(OBJ_DIR)/os $(OBJ_DIR)/api $(OBJ_DIR)/storage $(OBJ_DIR)/txn $(OBJ_DIR)/sql

# Compile utility modules
$(OBJ_DIR)/util/%.o: $(SRC_DIR)/util/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile OS modules
$(OBJ_DIR)/os/%.o: $(SRC_DIR)/os/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile API modules
$(OBJ_DIR)/api/%.o: $(SRC_DIR)/api/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile storage modules
$(OBJ_DIR)/storage/%.o: $(SRC_DIR)/storage/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile transaction modules
$(OBJ_DIR)/txn/%.o: $(SRC_DIR)/txn/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile SQL modules
$(OBJ_DIR)/sql/%.o: $(SRC_DIR)/sql/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test modules
$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile shell main
$(OBJ_DIR)/shell_main.o: $(SRC_DIR)/shell_main.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Link test executable (no REPL)
amidb_tests: $(ALL_OBJS) $(TEST_OBJS)
	@echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo ""
	@echo "==============================================="
	@echo "BUILD SUCCESSFUL!"
	@echo "Executable created: amidb_tests"
	@echo "Transfer to Amiga and run: ./amidb_tests"
	@echo "==============================================="

# Link SQL shell executable (with REPL)
amidb_shell: $(SHELL_OBJS) $(OBJ_DIR)/shell_main.o
	@echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo ""
	@echo "==============================================="
	@echo "SHELL BUILD SUCCESSFUL!"
	@echo "Executable created: amidb_shell"
	@echo "Transfer to Amiga and run: ./amidb_shell"
	@echo "==============================================="

# Compile example programs
$(OBJ_DIR)/%.o: $(EXAMPLE_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Link example programs
inventory_demo: $(ALL_OBJS) $(OBJ_DIR)/inventory_demo.o
	@echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Example program created: $@"

# Build all examples
examples: inventory_demo
	@echo ""
	@echo "==============================================="
	@echo "EXAMPLES BUILD SUCCESSFUL!"
	@echo "Created: inventory_demo"
	@echo "Transfer to Amiga and run: ./inventory_demo"
	@echo "==============================================="

# Clean build files
clean:
	@echo "Cleaning build files..."
	rm -rf $(OBJ_DIR)
	rm -f amidb_tests amidb_shell inventory_demo libamidb.a
	@echo "Clean complete."

# Test target - build and optionally copy to Amiga
test: amidb_tests
	@echo ""
	@echo "To run on Amiga via Amiberry:"
	@echo "The executable is already in the shared folder"
	@echo "On Amiga: cd Work:Code/AmiDB"
	@echo "          ./amidb_tests"
