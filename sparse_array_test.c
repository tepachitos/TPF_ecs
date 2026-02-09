// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
// clang-format on

#include <cmocka.h>

#include "sparse_array.h"

static void test_new_destroy_smoke(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);
  // Should be safe to destroy immediately.
  sparse_array_destroy(sa);
}

static void test_get_missing_on_new_array(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  // Any valid index should be missing at start.
  assert_int_equal(sparse_array_get(sa, 0), (int)TPF_ECS_INVALID_INDEX);
  assert_false(sparse_array_has(sa, 0));

  assert_int_equal(sparse_array_get(sa, 123456), (int)TPF_ECS_INVALID_INDEX);
  assert_false(sparse_array_has(sa, 123456));

  sparse_array_destroy(sa);
}

static void test_set_and_get_within_same_page(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  const size_t i = 7;
  const size_t v = 42;

  assert_true(sparse_array_set(sa, i, v));
  assert_true(sparse_array_has(sa, i));
  assert_int_equal(sparse_array_get(sa, i), (int)v);

  // Neighbor indices still missing
  assert_int_equal(sparse_array_get(sa, i + 1), (int)TPF_ECS_INVALID_INDEX);
  assert_false(sparse_array_has(sa, i + 1));

  sparse_array_destroy(sa);
}

static void test_set_and_get_across_pages(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  const size_t p = (size_t)TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;

  // Put values on different pages:
  const size_t i0 = 0;          // page 0
  const size_t i1 = p - 1;      // page 0 (last slot)
  const size_t i2 = p;          // page 1 (first slot)
  const size_t i3 = 3 * p + 5;  // page 3

  assert_true(sparse_array_set(sa, i0, 100));
  assert_true(sparse_array_set(sa, i1, 101));
  assert_true(sparse_array_set(sa, i2, 200));
  assert_true(sparse_array_set(sa, i3, 305));

  assert_int_equal(sparse_array_get(sa, i0), 100);
  assert_int_equal(sparse_array_get(sa, i1), 101);
  assert_int_equal(sparse_array_get(sa, i2), 200);
  assert_int_equal(sparse_array_get(sa, i3), 305);

  // Spot check missing values around boundaries:
  assert_int_equal(sparse_array_get(sa, i2 + 1), (int)TPF_ECS_INVALID_INDEX);
  assert_false(sparse_array_has(sa, i2 + 1));

  sparse_array_destroy(sa);
}

static void test_overwrite_existing_value(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  const size_t i = (size_t)TPF_ECS_SPARSE_ARRAY_PAGE_SIZE + 99;

  assert_true(sparse_array_set(sa, i, 1));
  assert_int_equal(sparse_array_get(sa, i), 1);

  assert_true(sparse_array_set(sa, i, 999));
  assert_int_equal(sparse_array_get(sa, i), 999);

  sparse_array_destroy(sa);
}

static void test_delete_value(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  const size_t i = 123;
  assert_true(sparse_array_set(sa, i, 777));
  assert_true(sparse_array_has(sa, i));
  assert_int_equal(sparse_array_get(sa, i), 777);

  sparse_array_del(sa, i);

  assert_false(sparse_array_has(sa, i));
  assert_int_equal(sparse_array_get(sa, i), (int)TPF_ECS_INVALID_INDEX);

  sparse_array_destroy(sa);
}

static void test_delete_missing_is_noop(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  // Should not crash.
  sparse_array_del(sa, 0);
  sparse_array_del(sa, (size_t)TPF_ECS_SPARSE_ARRAY_PAGE_SIZE * 1000);

  // Still missing
  assert_int_equal(sparse_array_get(sa, 0), (int)TPF_ECS_INVALID_INDEX);

  sparse_array_destroy(sa);
}

static void test_invalid_index_rejected(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  // Per your code: set/get/has assert on INVALID in paranoid builds,
  // but set/get also have runtime checks returning false/INVALID.
  // We'll only test set() runtime behavior here.
  assert_false(sparse_array_set(sa, (size_t)TPF_ECS_INVALID_INDEX, 123));

  sparse_array_destroy(sa);
}

static void test_growth_large_index(void** state) {
  (void)state;
  struct sparse_array* sa = sparse_array_new();
  assert_non_null(sa);

  const size_t p = (size_t)TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;

  // Force growth far out:
  const size_t big = 10000 * p + 3;

  assert_true(sparse_array_set(sa, big, 0xBEEF));
  assert_true(sparse_array_has(sa, big));
  assert_int_equal(sparse_array_get(sa, big), 0xBEEF);

  // Ensure earlier stuff still missing:
  assert_int_equal(sparse_array_get(sa, 1), (int)TPF_ECS_INVALID_INDEX);

  // And we can still set something near zero after growth:
  assert_true(sparse_array_set(sa, 2, 0xCAFE));
  assert_int_equal(sparse_array_get(sa, 2), 0xCAFE);

  sparse_array_destroy(sa);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_new_destroy_smoke),
      cmocka_unit_test(test_get_missing_on_new_array),
      cmocka_unit_test(test_set_and_get_within_same_page),
      cmocka_unit_test(test_set_and_get_across_pages),
      cmocka_unit_test(test_overwrite_existing_value),
      cmocka_unit_test(test_delete_value),
      cmocka_unit_test(test_delete_missing_is_noop),
      cmocka_unit_test(test_invalid_index_rejected),
      cmocka_unit_test(test_growth_large_index),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
