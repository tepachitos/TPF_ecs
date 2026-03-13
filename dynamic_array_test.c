// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
// clang-format on

#include <cmocka.h>

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_stdinc.h>

#include "dynamic_array.h"

static void test_new_destroy_basic(void** state) {
  (void)state;

  struct dynamic_array* da = dynamic_array_new(8, 12, 0);
  assert_non_null(da);
  assert_int_equal(dynamic_array_len(da), 0);
  assert_int_equal(dynamic_array_cap(da), 0);

  dynamic_array_destroy(da);
}

static void test_has_get_set_bounds(void** state) {
  (void)state;

  struct dynamic_array* da = dynamic_array_new(8, 12, 2);
  assert_non_null(da);
  assert_true(dynamic_array_cap(da) >= 2);
  assert_int_equal(dynamic_array_len(da), 0);

  assert_false(dynamic_array_has(da, 0));
  assert_null(dynamic_array_get(da, 0));
  assert_false(dynamic_array_set(da, 0, (Uint8*)"abcdefghijkl"));

  Uint8 v0[12];
  for (int i = 0; i < 12; i++)
    v0[i] = (Uint8)i;

  assert_true(dynamic_array_push_back(da, v0));
  assert_int_equal(dynamic_array_len(da), 1);
  assert_true(dynamic_array_has(da, 0));
  assert_false(dynamic_array_has(da, 1));

  Uint8* p0 = dynamic_array_get(da, 0);
  assert_non_null(p0);
  assert_memory_equal(p0, v0, 12);

  // set index 0
  Uint8 v1[12];
  for (int i = 0; i < 12; i++)
    v1[i] = (Uint8)(100 + i);

  assert_true(dynamic_array_set(da, 0, v1));
  assert_memory_equal(dynamic_array_get(da, 0), v1, 12);

  // bounds again
  assert_null(dynamic_array_get(da, 1));
  assert_false(dynamic_array_set(da, 1, v0));

  dynamic_array_destroy(da);
}

static void test_stride_is_aligned_up_via_pointer_diff(void** state) {
  (void)state;

  struct dynamic_array* da = dynamic_array_new(8, 12, 0);
  assert_non_null(da);

  Uint8 a[12] = {0};
  Uint8 b[12] = {1};

  assert_true(dynamic_array_push_back(da, a));
  assert_true(dynamic_array_push_back(da, b));

  Uint8* p0 = dynamic_array_get(da, 0);
  Uint8* p1 = dynamic_array_get(da, 1);
  assert_non_null(p0);
  assert_non_null(p1);

  ptrdiff_t diff = (ptrdiff_t)(p1 - p0);
  assert_int_equal(diff, 16);  // stride inferred by pointer spacing

  dynamic_array_destroy(da);
}

static void test_push_back_null_zeroes_item_size_only(void** state) {
  (void)state;

  struct dynamic_array* da = dynamic_array_new(8, 12, 0);
  assert_non_null(da);

  Uint8 v[12];
  for (int i = 0; i < 12; i++) {
    v[i] = (Uint8)(0xAA);
  }

  assert_true(dynamic_array_push_back(da, v));
  assert_int_equal(dynamic_array_len(da), 1);
  assert_memory_equal(dynamic_array_get(da, 0), v, 12);

  assert_true(dynamic_array_push_back(da, NULL));
  assert_int_equal(dynamic_array_len(da), 2);

  Uint8* p = dynamic_array_get(da, 1);
  assert_non_null(p);

  Uint8 zeros[12] = {0};
  assert_memory_equal(p, zeros, 12);

  dynamic_array_destroy(da);
}

static void test_reserve_preserves_contents(void** state) {
  (void)state;

  struct dynamic_array* da = dynamic_array_new(8, 12, 0);
  assert_non_null(da);

  Uint8 v0[12], v1[12];
  for (int i = 0; i < 12; i++) {
    v0[i] = (Uint8)(i + 1);
    v1[i] = (Uint8)(i + 50);
  }

  assert_true(dynamic_array_push_back(da, v0));
  assert_true(dynamic_array_push_back(da, v1));

  size_t old_cap = dynamic_array_cap(da);
  assert_true(dynamic_array_reserve(da, old_cap + 32));
  assert_true(dynamic_array_cap(da) >= old_cap + 32);
  assert_int_equal(dynamic_array_len(da), 2);

  assert_memory_equal(dynamic_array_get(da, 0), v0, 12);
  assert_memory_equal(dynamic_array_get(da, 1), v1, 12);

  dynamic_array_destroy(da);
}

static void test_pop_back_and_shrink_to_zero(void** state) {
  (void)state;

  struct dynamic_array* da = dynamic_array_new(8, 12, 0);
  assert_non_null(da);

  bool empty_pop = dynamic_array_pop_back(da);
  assert_false(empty_pop);
  assert_int_equal(dynamic_array_len(da), 0);
  assert_int_equal(dynamic_array_cap(da), 0);

  Uint8 data[12];
  for (int i = 0; i < 12; i++) {
    data[i] = (Uint8)i;
  }

  for (int k = 0; k < 40; k++) {
    data[0] = (Uint8)k;
    bool push_back = dynamic_array_push_back(da, data);
    assert_true(push_back);
  }

  size_t cap_after_push = dynamic_array_cap(da);
  assert_true(cap_after_push >= 40);
  assert_int_equal(dynamic_array_len(da), 40);

  for (int k = 0; k < 40; k++) {
    bool pop_back = dynamic_array_pop_back(da);
    assert_true(pop_back);
  }

  assert_int_equal(dynamic_array_len(da), 0);
  assert_int_equal(dynamic_array_cap(da), 0);

  dynamic_array_destroy(da);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_new_destroy_basic),
      cmocka_unit_test(test_has_get_set_bounds),
      cmocka_unit_test(test_stride_is_aligned_up_via_pointer_diff),
      cmocka_unit_test(test_push_back_null_zeroes_item_size_only),
      cmocka_unit_test(test_reserve_preserves_contents),
      cmocka_unit_test(test_pop_back_and_shrink_to_zero),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
