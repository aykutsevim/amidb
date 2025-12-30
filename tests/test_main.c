/*
 * test_main.c - Main test runner for AmiDB
 *
 * Runs all unit tests and reports results.
 */

#include "test_harness.h"
#include <stdio.h>

/* Global log file handle */
FILE *g_test_log = NULL;

/* Declare test functions */
/* Phase 1 - Endian tests */
extern int test_endian_u16_roundtrip(void);
extern int test_endian_u16_byte_order(void);
extern int test_endian_u32_roundtrip(void);
extern int test_endian_u32_byte_order(void);
extern int test_endian_u32_zero(void);
extern int test_endian_u32_max(void);

/* Phase 1 - CRC32 tests */
extern int test_crc32_known_value(void);
extern int test_crc32_empty(void);
extern int test_crc32_incremental(void);
extern int test_crc32_different_data(void);
extern int test_crc32_one_bit_change(void);

/* Phase 2 - Pager tests */
extern int test_pager_mem_test(void);
extern int test_pager_file_test(void);
extern int test_pager_manual_open(void);
extern int test_pager_create_new(void);
extern int test_pager_allocate_pages(void);
extern int test_pager_write_read_page(void);
extern int test_pager_checksum_verification(void);
extern int test_pager_reopen_database(void);

/* Phase 2 - Cache tests */
extern int test_cache_create_destroy(void);
extern int test_cache_get_page_loads(void);
extern int test_cache_lru_eviction(void);
extern int test_cache_pin_prevents_eviction(void);
extern int test_cache_dirty_and_flush(void);
extern int test_cache_pin_list(void);

/* Phase 2 - Row tests */
extern int test_row_init_clear(void);
extern int test_row_integer(void);
extern int test_row_text(void);
extern int test_row_blob(void);
extern int test_row_null(void);
extern int test_row_mixed_types(void);
extern int test_row_serialize_integer(void);
extern int test_row_serialize_text(void);
extern int test_row_serialize_mixed(void);
extern int test_row_serialize_empty(void);

/* Phase 3A - B+Tree Basic tests */
extern int test_btree_create_close(void);
extern int test_btree_single_entry(void);
extern int test_btree_multiple_entries(void);
extern int test_btree_reverse_order(void);
extern int test_btree_delete(void);
extern int test_btree_cursor(void);
extern int test_btree_many_keys(void);
extern int test_btree_update(void);

/* Phase 3B - B+Tree Split tests */
extern int test_btree_split_100_keys(void);
extern int test_btree_split_500_keys(void);
extern int test_btree_split_reverse_500(void);
extern int test_btree_split_cursor_iteration(void);
extern int test_btree_split_update_after_split(void);

/* Phase 3B - B+Tree Merge tests */
extern int test_btree_merge_borrow(void);
extern int test_btree_merge_trigger(void);
extern int test_btree_merge_500_delete_400(void);
extern int test_btree_merge_delete_all(void);
extern int test_btree_merge_reverse_delete(void);

/* Phase 3C - WAL tests */
extern int test_wal_create_destroy(void);
extern int test_wal_write_begin_commit(void);
extern int test_wal_write_page_record(void);
extern int test_wal_flush_to_disk(void);
extern int test_wal_buffer_overflow(void);
extern int test_wal_checksum_validation(void);

/* Phase 3C - Transaction tests */
extern int test_txn_begin_commit(void);
extern int test_txn_begin_abort(void);
extern int test_txn_dirty_page_tracking(void);
extern int test_txn_pin_during_transaction(void);
extern int test_txn_multi_page_commit(void);
extern int test_txn_nested_abort(void);
extern int test_txn_commit_durability(void);
extern int test_txn_isolation(void);

/* Phase 3C - Recovery tests */
extern int test_recovery_committed_transaction(void);
extern int test_recovery_uncommitted_transaction(void);
extern int test_recovery_partial_commit(void);
extern int test_recovery_multiple_transactions(void);
extern int test_recovery_corrupt_wal_record(void);
extern int test_recovery_empty_wal(void);

/* Phase 3C - B+Tree Transaction Integration tests */
extern int test_btree_insert_with_transaction(void);
extern int test_btree_split_with_commit(void);
extern int test_btree_split_with_abort(void);
extern int test_btree_delete_merge_transaction(void);
extern int test_btree_complex_multi_operation(void);

/* Phase 4 - SQL Lexer tests */
extern int test_lexer_keywords(void);
extern int test_lexer_identifiers(void);
extern int test_lexer_integers(void);
extern int test_lexer_strings(void);
extern int test_lexer_symbols(void);
extern int test_lexer_whitespace(void);
extern int test_lexer_comments(void);
extern int test_lexer_multitoken(void);

/* Phase 4 - SQL Parser tests */
extern int test_parser_create_explicit_pk(void);
extern int test_parser_create_implicit_rowid(void);
extern int test_parser_create_multiple_columns(void);
extern int test_parser_create_multiple_pk_error(void);
extern int test_parser_create_no_columns_error(void);
extern int test_parser_trailing_semicolon(void);
extern int test_parser_case_insensitive(void);

/* Phase 4 - SQL Catalog tests */
extern int test_catalog_create_get(void);
extern int test_catalog_implicit_rowid(void);
extern int test_catalog_duplicate_table(void);
extern int test_catalog_drop_table(void);
extern int test_catalog_list_tables(void);
extern int test_catalog_persistence(void);

/* Phase 4 - SQL End-to-End tests */
extern int test_e2e_create_table_explicit_pk(void);
extern int test_e2e_create_table_implicit_rowid(void);
extern int test_e2e_create_table_validation(void);
extern int test_e2e_multiple_tables(void);
extern int test_e2e_insert_explicit_pk(void);
extern int test_e2e_insert_implicit_rowid(void);
extern int test_e2e_insert_validation(void);
extern int test_e2e_select_all(void);
extern int test_e2e_select_where_pk(void);
extern int test_e2e_select_where_nonpk(void);
extern int test_e2e_select_no_match(void);
extern int test_e2e_order_by_pk_asc(void);
extern int test_e2e_order_by_nonpk(void);
extern int test_e2e_limit_only(void);
extern int test_e2e_order_limit_combined(void);

/* Phase 5 - DROP TABLE tests */
extern int test_e2e_drop_table_basic(void);
extern int test_e2e_drop_table_nonexistent(void);
extern int test_e2e_drop_table_recreate(void);

/* Phase 5 - COUNT aggregate tests */
extern int test_e2e_count_star_basic(void);
extern int test_e2e_count_star_empty(void);
extern int test_e2e_count_star_where(void);
extern int test_e2e_count_column(void);

/* Phase 5 - SUM aggregate tests */
extern int test_e2e_sum_basic(void);
extern int test_e2e_sum_empty(void);
extern int test_e2e_sum_where(void);

/* Phase 5 - AVG aggregate tests */
extern int test_e2e_avg_basic(void);
extern int test_e2e_avg_empty(void);
extern int test_e2e_avg_where(void);

/* Phase 5 - MIN aggregate tests */
extern int test_e2e_min_basic(void);
extern int test_e2e_min_empty(void);
extern int test_e2e_min_where(void);

/* Phase 5 - MAX aggregate tests */
extern int test_e2e_max_basic(void);
extern int test_e2e_max_empty(void);
extern int test_e2e_max_where(void);

/* Main test runner */
int main(void) {
    int passed = 0;
    int failed = 0;

    /* Open log file */
    g_test_log = fopen("test_results.txt", "w");
    if (!g_test_log) {
        printf("WARNING: Could not open test_results.txt for writing\n");
    }

    test_printf("===============================================\n");
    test_printf("AmiDB Test Suite - BUILD v3.6\n");
    test_printf("===============================================\n");
    test_printf("Fixed: Duplicate check moved to SQL layer\n");
    test_printf("B+Tree now supports UPDATE (upsert behavior)\n");
    test_printf("===============================================\n\n");

    /* Phase 1: Foundation Tests */
    TEST_SECTION("Phase 1: Foundation");

    test_printf("\nEndian Conversion Tests:\n");
    RUN_TEST(endian_u16_roundtrip);
    RUN_TEST(endian_u16_byte_order);
    RUN_TEST(endian_u32_roundtrip);
    RUN_TEST(endian_u32_byte_order);
    RUN_TEST(endian_u32_zero);
    RUN_TEST(endian_u32_max);

    test_printf("\nCRC32 Tests:\n");
    RUN_TEST(crc32_known_value);
    RUN_TEST(crc32_empty);
    RUN_TEST(crc32_incremental);
    RUN_TEST(crc32_different_data);
    RUN_TEST(crc32_one_bit_change);

    /* Phase 2: Storage Tests */
    TEST_SECTION("Phase 2: Storage Engine");

    test_printf("\nPager Tests:\n");
    RUN_TEST(pager_mem_test);
    RUN_TEST(pager_file_test);
    RUN_TEST(pager_manual_open);
    RUN_TEST(pager_create_new);
    RUN_TEST(pager_allocate_pages);
    RUN_TEST(pager_write_read_page);
    RUN_TEST(pager_checksum_verification);
    RUN_TEST(pager_reopen_database);

    test_printf("\nCache Tests:\n");
    RUN_TEST(cache_create_destroy);
    RUN_TEST(cache_get_page_loads);
    RUN_TEST(cache_lru_eviction);
    RUN_TEST(cache_pin_prevents_eviction);
    RUN_TEST(cache_dirty_and_flush);
    RUN_TEST(cache_pin_list);

    test_printf("\nRow Tests:\n");
    RUN_TEST(row_init_clear);
    RUN_TEST(row_integer);
    RUN_TEST(row_text);
    RUN_TEST(row_blob);
    RUN_TEST(row_null);
    RUN_TEST(row_mixed_types);
    RUN_TEST(row_serialize_integer);
    RUN_TEST(row_serialize_text);
    RUN_TEST(row_serialize_mixed);
    RUN_TEST(row_serialize_empty);

    /* Phase 3A: B+Tree Tests */
    TEST_SECTION("Phase 3A: B+Tree Basics");

    test_printf("\nB+Tree Tests:\n");
    RUN_TEST(btree_create_close);
    RUN_TEST(btree_single_entry);
    RUN_TEST(btree_multiple_entries);
    RUN_TEST(btree_reverse_order);
    RUN_TEST(btree_delete);
    RUN_TEST(btree_cursor);
    RUN_TEST(btree_many_keys);
    RUN_TEST(btree_update);

    /* Phase 3B: B+Tree Split Tests */
    TEST_SECTION("Phase 3B: B+Tree Split/Merge");

    test_printf("\nB+Tree Split Tests:\n");
    RUN_TEST(btree_split_100_keys);
    RUN_TEST(btree_split_500_keys);
    RUN_TEST(btree_split_reverse_500);
    RUN_TEST(btree_split_cursor_iteration);
    RUN_TEST(btree_split_update_after_split);

    test_printf("\nB+Tree Merge Tests:\n");
    RUN_TEST(btree_merge_borrow);
    RUN_TEST(btree_merge_trigger);
    RUN_TEST(btree_merge_500_delete_400);
    RUN_TEST(btree_merge_delete_all);
    RUN_TEST(btree_merge_reverse_delete);

    /* Phase 3C: WAL and Transaction Tests */
    TEST_SECTION("Phase 3C: WAL and Transactions");

    test_printf("\nWAL Tests:\n");
    RUN_TEST(wal_create_destroy);
    RUN_TEST(wal_write_begin_commit);
    RUN_TEST(wal_write_page_record);
    RUN_TEST(wal_flush_to_disk);
    RUN_TEST(wal_buffer_overflow);
    RUN_TEST(wal_checksum_validation);

    test_printf("\nTransaction Manager Tests:\n");
    RUN_TEST(txn_begin_commit);
    RUN_TEST(txn_begin_abort);
    RUN_TEST(txn_dirty_page_tracking);
    RUN_TEST(txn_pin_during_transaction);
    RUN_TEST(txn_multi_page_commit);
    RUN_TEST(txn_nested_abort);
    RUN_TEST(txn_commit_durability);
    RUN_TEST(txn_isolation);

    test_printf("\nCrash Recovery Tests:\n");
    RUN_TEST(recovery_committed_transaction);
    RUN_TEST(recovery_uncommitted_transaction);
    RUN_TEST(recovery_partial_commit);
    RUN_TEST(recovery_multiple_transactions);
    RUN_TEST(recovery_corrupt_wal_record);
    RUN_TEST(recovery_empty_wal);

    test_printf("\nB+Tree Transaction Integration Tests:\n");
    RUN_TEST(btree_insert_with_transaction);
    RUN_TEST(btree_split_with_commit);
    RUN_TEST(btree_split_with_abort);
    RUN_TEST(btree_delete_merge_transaction);
    RUN_TEST(btree_complex_multi_operation);

    /* Phase 4: SQL Parser Tests */
    TEST_SECTION("Phase 4: SQL Parser");

    test_printf("\nSQL Lexer Tests:\n");
    RUN_TEST(lexer_keywords);
    RUN_TEST(lexer_identifiers);
    RUN_TEST(lexer_integers);
    RUN_TEST(lexer_strings);
    RUN_TEST(lexer_symbols);
    RUN_TEST(lexer_whitespace);
    RUN_TEST(lexer_comments);
    RUN_TEST(lexer_multitoken);

    test_printf("\nSQL Parser Tests:\n");
    RUN_TEST(parser_create_explicit_pk);
    RUN_TEST(parser_create_implicit_rowid);
    RUN_TEST(parser_create_multiple_columns);
    RUN_TEST(parser_create_multiple_pk_error);
    RUN_TEST(parser_create_no_columns_error);
    RUN_TEST(parser_trailing_semicolon);
    RUN_TEST(parser_case_insensitive);

    test_printf("\nSQL Catalog Tests:\n");
    RUN_TEST(catalog_create_get);
    RUN_TEST(catalog_implicit_rowid);
    RUN_TEST(catalog_duplicate_table);
    RUN_TEST(catalog_drop_table);
    RUN_TEST(catalog_list_tables);
    RUN_TEST(catalog_persistence);

    test_printf("\nSQL End-to-End Tests:\n");
    RUN_TEST(e2e_create_table_explicit_pk);
    RUN_TEST(e2e_create_table_implicit_rowid);
    RUN_TEST(e2e_create_table_validation);
    RUN_TEST(e2e_multiple_tables);
    RUN_TEST(e2e_insert_explicit_pk);
    RUN_TEST(e2e_insert_implicit_rowid);
    RUN_TEST(e2e_insert_validation);
    RUN_TEST(e2e_select_all);
    RUN_TEST(e2e_select_where_pk);
    RUN_TEST(e2e_select_where_nonpk);
    RUN_TEST(e2e_select_no_match);
    RUN_TEST(e2e_order_by_pk_asc);
    RUN_TEST(e2e_order_by_nonpk);
    RUN_TEST(e2e_limit_only);
    RUN_TEST(e2e_order_limit_combined);

    /* Phase 5: Advanced SQL */
    TEST_SECTION("Phase 5: Advanced SQL");

    test_printf("\nDROP TABLE Tests:\n");
    RUN_TEST(e2e_drop_table_basic);
    RUN_TEST(e2e_drop_table_nonexistent);
    RUN_TEST(e2e_drop_table_recreate);

    test_printf("\nCOUNT Aggregate Tests:\n");
    RUN_TEST(e2e_count_star_basic);
    RUN_TEST(e2e_count_star_empty);
    RUN_TEST(e2e_count_star_where);
    RUN_TEST(e2e_count_column);

    test_printf("\nSUM Aggregate Tests:\n");
    RUN_TEST(e2e_sum_basic);
    RUN_TEST(e2e_sum_empty);
    RUN_TEST(e2e_sum_where);

    test_printf("\nAVG Aggregate Tests:\n");
    RUN_TEST(e2e_avg_basic);
    RUN_TEST(e2e_avg_empty);
    RUN_TEST(e2e_avg_where);

    test_printf("\nMIN Aggregate Tests:\n");
    RUN_TEST(e2e_min_basic);
    RUN_TEST(e2e_min_empty);
    RUN_TEST(e2e_min_where);

    test_printf("\nMAX Aggregate Tests:\n");
    RUN_TEST(e2e_max_basic);
    RUN_TEST(e2e_max_empty);
    RUN_TEST(e2e_max_where);

    /* Summary */
    test_printf("\n===============================================\n");
    test_printf("Test Results\n");
    test_printf("===============================================\n");
    test_printf("Passed: %d\n", passed);
    test_printf("Failed: %d\n", failed);
    test_printf("Total:  %d\n", passed + failed);
    test_printf("===============================================\n");

    if (failed > 0) {
        test_printf("\n*** TESTS FAILED ***\n");
        test_printf("\nPress RETURN to continue...");
        fflush(stdout);
        getchar();
        if (g_test_log) {
            fclose(g_test_log);
        }
        return 1;
    }

    test_printf("\n*** ALL TESTS PASSED ***\n");
    test_printf("\nPress RETURN to continue...");
    fflush(stdout);
    getchar();
    if (g_test_log) {
        fclose(g_test_log);
    }
    return 0;
}
