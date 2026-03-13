// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
// clang-format on

#include <SDL3/SDL_stdinc.h>
#include <cmocka.h>

#include "component_table.h"

#include <TPF/TPF_ecs_types.h>

typedef struct TestComponent {
  int hp;
  float speed;
  uint32_t flags;
} TestComponent;

static struct component_table_config make_test_config(TPF_ComponentID cid,
                                                      const char* name,
                                                      size_t initial_slots) {
  struct component_table_config cfg;
  SDL_memset(&cfg, 0, sizeof(cfg));

  cfg.component_id = cid;
  cfg.item_align = _Alignof(TestComponent);
  cfg.item_size = sizeof(TestComponent);
  cfg.initial_slots = initial_slots;

  if (name != NULL) {
    SDL_strlcpy(cfg.name, name, sizeof(cfg.name) - 1);
    cfg.name[sizeof(cfg.name) - 1] = '\0';
  }

  return cfg;
}

static struct component_table* make_configured_table(void) {
  struct component_table* table = component_table_new();
  assert_non_null(table);

  struct component_table_config cfg = make_test_config(1, "test_component", 4);
  assert_true(component_table_configure(table, cfg));

  return table;
}

static int setup_configured_table(void** state) {
  *state = make_configured_table();
  return 0;
}

static int teardown_table(void** state) {
  struct component_table* table = (struct component_table*)(*state);
  if (table != NULL) {
    component_table_destroy(table);
  }
  return 0;
}

static void test_component_table_new_destroy(void** state) {
  (void)state;

  struct component_table* table = component_table_new();
  assert_non_null(table);

  component_table_destroy(table);
}

static void test_component_table_configure_success(void** state) {
  (void)state;

  struct component_table* table = component_table_new();
  assert_non_null(table);

  struct component_table_config cfg = make_test_config(3, "position", 8);
  assert_true(component_table_configure(table, cfg));

  assert_null(component_table_get_slot(table, 1));

  component_table_destroy(table);
}

static void test_component_table_get_slot_missing_entity_returns_null(
    void** state) {
  struct component_table* table = (struct component_table*)(*state);

  assert_null(component_table_get_slot(table, 42));
}

static void test_component_table_add_slot_then_get_slot(void** state) {
  struct component_table* table = (struct component_table*)(*state);
  TPF_EntityID eid = 10;

  assert_true(component_table_add_slot(table, eid));

  Uint8* raw = component_table_get_slot(table, eid);
  assert_non_null(raw);

  TestComponent* c = (TestComponent*)raw;
  c->hp = 100;
  c->speed = 3.5f;
  c->flags = 0xDEADBEEFu;

  TestComponent* c2 = (TestComponent*)component_table_get_slot(table, eid);
  assert_non_null(c2);
  assert_int_equal(c2->hp, 100);
  assert_true(c2->speed == 3.5f);
  assert_int_equal((int)c2->flags, (int)0xDEADBEEFu);
}

static void test_component_table_add_same_slot_twice_is_idempotent(
    void** state) {
  struct component_table* table = (struct component_table*)(*state);
  TPF_EntityID eid = 11;

  assert_true(component_table_add_slot(table, eid));

  Uint8* first_ptr = component_table_get_slot(table, eid);
  assert_non_null(first_ptr);

  TestComponent* c = (TestComponent*)first_ptr;
  c->hp = 77;
  c->speed = 9.0f;
  c->flags = 123u;

  assert_true(component_table_add_slot(table, eid));

  Uint8* second_ptr = component_table_get_slot(table, eid);
  assert_non_null(second_ptr);

  TestComponent* c2 = (TestComponent*)second_ptr;
  assert_int_equal(c2->hp, 77);
  assert_true(c2->speed == 9.0f);
  assert_int_equal((int)c2->flags, 123);
}

static void test_component_table_delete_missing_slot_is_ok(void** state) {
  struct component_table* table = (struct component_table*)(*state);

  assert_true(component_table_del_slot(table, 999));
}

static void test_component_table_add_delete_single_slot(void** state) {
  struct component_table* table = (struct component_table*)(*state);
  TPF_EntityID eid = 20;

  assert_true(component_table_add_slot(table, eid));
  assert_non_null(component_table_get_slot(table, eid));

  assert_true(component_table_del_slot(table, eid));
  assert_null(component_table_get_slot(table, eid));

  assert_true(component_table_del_slot(table, eid));
}

static void test_component_table_delete_last_slot_keeps_others_valid(
    void** state) {
  struct component_table* table = (struct component_table*)(*state);

  TPF_EntityID a = 1;
  TPF_EntityID b = 2;
  TPF_EntityID c = 3;

  assert_true(component_table_add_slot(table, a));
  assert_true(component_table_add_slot(table, b));
  assert_true(component_table_add_slot(table, c));

  TestComponent* ca = (TestComponent*)component_table_get_slot(table, a);
  TestComponent* cb = (TestComponent*)component_table_get_slot(table, b);
  TestComponent* cc = (TestComponent*)component_table_get_slot(table, c);

  assert_non_null(ca);
  assert_non_null(cb);
  assert_non_null(cc);

  ca->hp = 10;
  cb->hp = 20;
  cc->hp = 30;

  assert_true(component_table_del_slot(table, c));

  assert_null(component_table_get_slot(table, c));

  ca = (TestComponent*)component_table_get_slot(table, a);
  cb = (TestComponent*)component_table_get_slot(table, b);

  assert_non_null(ca);
  assert_non_null(cb);
  assert_int_equal(ca->hp, 10);
  assert_int_equal(cb->hp, 20);
}

static void test_component_table_delete_middle_slot_swap_remove_keeps_data(
    void** state) {
  struct component_table* table = (struct component_table*)(*state);

  TPF_EntityID a = 100;
  TPF_EntityID b = 200;
  TPF_EntityID c = 300;

  assert_true(component_table_add_slot(table, a));
  assert_true(component_table_add_slot(table, b));
  assert_true(component_table_add_slot(table, c));

  TestComponent* ca = (TestComponent*)component_table_get_slot(table, a);
  TestComponent* cb = (TestComponent*)component_table_get_slot(table, b);
  TestComponent* cc = (TestComponent*)component_table_get_slot(table, c);

  assert_non_null(ca);
  assert_non_null(cb);
  assert_non_null(cc);

  ca->hp = 111;
  cb->hp = 222;
  cc->hp = 333;

  ca->speed = 1.0f;
  cb->speed = 2.0f;
  cc->speed = 3.0f;

  ca->flags = 11u;
  cb->flags = 22u;
  cc->flags = 33u;

  assert_true(component_table_del_slot(table, b));

  assert_null(component_table_get_slot(table, b));

  TestComponent* ca2 = (TestComponent*)component_table_get_slot(table, a);
  TestComponent* cc2 = (TestComponent*)component_table_get_slot(table, c);

  assert_non_null(ca2);
  assert_non_null(cc2);

  assert_int_equal(ca2->hp, 111);
  assert_true(ca2->speed == 1.0f);
  assert_int_equal((int)ca2->flags, 11);

  assert_int_equal(cc2->hp, 333);
  assert_true(cc2->speed == 3.0f);
  assert_int_equal((int)cc2->flags, 33);
}

static void test_component_table_reset_clears_slots(void** state) {
  struct component_table* table = (struct component_table*)(*state);

  assert_true(component_table_add_slot(table, 7));
  assert_non_null(component_table_get_slot(table, 7));

  component_table_reset(table);

  assert_null(component_table_get_slot(table, 7));
  assert_false(component_table_add_slot(table, 7));
}

static void test_component_table_reconfigure_replaces_previous_storage(
    void** state) {
  struct component_table* table = (struct component_table*)(*state);

  assert_true(component_table_add_slot(table, 55));
  assert_non_null(component_table_get_slot(table, 55));

  struct component_table_config cfg2 = make_test_config(9, "velocity", 2);
  assert_true(component_table_configure(table, cfg2));

  assert_null(component_table_get_slot(table, 55));

  assert_true(component_table_add_slot(table, 66));
  assert_non_null(component_table_get_slot(table, 66));
}

static void test_component_table_multiple_entities_roundtrip(void** state) {
  struct component_table* table = (struct component_table*)(*state);

  enum { N = 64 };
  TPF_EntityID ids[N];

  for (int i = 0; i < N; ++i) {
    ids[i] = (TPF_EntityID)(1000 + i);
    assert_true(component_table_add_slot(table, ids[i]));

    TestComponent* c = (TestComponent*)component_table_get_slot(table, ids[i]);
    assert_non_null(c);

    c->hp = i * 10;
    c->speed = (float)i * 0.25f;
    c->flags = (uint32_t)i;
  }

  for (int i = 0; i < N; ++i) {
    TestComponent* c = (TestComponent*)component_table_get_slot(table, ids[i]);
    assert_non_null(c);

    assert_int_equal(c->hp, i * 10);
    assert_true(c->speed == (float)i * 0.25f);
    assert_int_equal((int)c->flags, i);
  }
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_component_table_new_destroy),
      cmocka_unit_test(test_component_table_configure_success),

      cmocka_unit_test_setup_teardown(
          test_component_table_get_slot_missing_entity_returns_null,
          setup_configured_table, teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_add_slot_then_get_slot, setup_configured_table,
          teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_add_same_slot_twice_is_idempotent,
          setup_configured_table, teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_delete_missing_slot_is_ok,
          setup_configured_table, teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_add_delete_single_slot, setup_configured_table,
          teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_delete_last_slot_keeps_others_valid,
          setup_configured_table, teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_delete_middle_slot_swap_remove_keeps_data,
          setup_configured_table, teardown_table),

      cmocka_unit_test_setup_teardown(test_component_table_reset_clears_slots,
                                      setup_configured_table, teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_reconfigure_replaces_previous_storage,
          setup_configured_table, teardown_table),

      cmocka_unit_test_setup_teardown(
          test_component_table_multiple_entities_roundtrip,
          setup_configured_table, teardown_table),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
