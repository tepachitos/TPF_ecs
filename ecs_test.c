// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <TPF/TPF_ecs.h>

static void test_dummy(void** state) {
  (void)state;
  assert_int_equal(42, TPF_ECSHelloWorld());
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_dummy),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
