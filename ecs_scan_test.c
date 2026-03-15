// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <TPF/TPF_ecs.h>
#include <TPF/TPF_ecs_types.h>

typedef struct test_ctx {
  TPF_World* world;
} test_ctx;

typedef struct test_position {
  float x;
  float y;
  float z;
} test_position;

typedef struct test_velocity {
  float x;
  float y;
  float z;
} test_velocity;

static int setup_world(void** state) {
  test_ctx* ctx = SDL_malloc(sizeof(*ctx));
  assert_non_null(ctx);

  ctx->world = TPF_CreateWorld();
  assert_non_null(ctx->world);

  *state = ctx;
  return 0;
}

static int teardown_world(void** state) {
  test_ctx* ctx = *state;
  if (ctx != NULL) {
    if (ctx->world != NULL) {
      TPF_DestroyWorld(ctx->world);
    }

    SDL_free(ctx);
  }
  return 0;
}

static TPF_ComponentMask bit_of(Uint8 id) {
  return ((TPF_ComponentMask)1u << id);
}

static void assert_cursor_contains_exact_ids(TPF_EntityCursor* cursor,
                                             const TPF_EntityID* expected,
                                             size_t expected_count) {
  assert_non_null(cursor);
  assert_int_equal(TPF_CursorGetCount(cursor), expected_count);

  for (size_t i = 0; i < expected_count; i++) {
    assert_int_equal(TPF_CursorGetEntityID(cursor, i), expected[i]);
  }
}

static void test_create_and_destroy_cursor(void** state) {
  test_ctx* ctx = *state;

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_int_equal(TPF_CursorGetCount(cursor), 0);
  assert_int_equal(TPF_CursorGetEntityID(cursor, 0), TPF_ECS_INVALID_EID);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_empty_world_returns_false_and_no_results(void** state) {
  test_ctx* ctx = *state;

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 4);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, 0));
  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_all_entities_with_zero_filter(void** state) {
  test_ctx* ctx = *state;

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e0, TPF_ECS_INVALID_EID);
  assert_int_not_equal(e1, TPF_ECS_INVALID_EID);
  assert_int_not_equal(e2, TPF_ECS_INVALID_EID);

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, 0));
  assert_true(TPF_ScanNext(cursor));

  {
    const TPF_EntityID expected[] = {e0, e1, e2};
    assert_cursor_contains_exact_ids(cursor, expected, 3);
  }

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_by_single_component(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID pos_cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  assert_int_not_equal(pos_cid, TPF_ECS_INVALID_CID);

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);

  assert_true(TPF_EnableComponent(ctx->world, e0, pos_cid));
  assert_true(TPF_EnableComponent(ctx->world, e2, pos_cid));

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, bit_of(pos_cid)));
  assert_true(TPF_ScanNext(cursor));

  {
    const TPF_EntityID expected[] = {e0, e2};
    assert_cursor_contains_exact_ids(cursor, expected, 2);
  }

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_by_single_tag(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID enemy_tid = TPF_CreateTag(ctx->world, "enemy");
  assert_int_not_equal(enemy_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);

  assert_true(TPF_SetTag(ctx->world, e0, enemy_tid));
  assert_true(TPF_SetTag(ctx->world, e2, enemy_tid));

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, bit_of(enemy_tid)));
  assert_true(TPF_ScanNext(cursor));

  {
    const TPF_EntityID expected[] = {e0, e2};
    assert_cursor_contains_exact_ids(cursor, expected, 2);
  }

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_by_component_and_tag(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID pos_cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  TPF_TagID enemy_tid = TPF_CreateTag(ctx->world, "enemy");

  assert_int_not_equal(pos_cid, TPF_ECS_INVALID_CID);
  assert_int_not_equal(enemy_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e3 = TPF_CreateEntity(ctx->world);

  assert_true(TPF_EnableComponent(ctx->world, e0, pos_cid));
  assert_true(TPF_SetTag(ctx->world, e0, enemy_tid));

  assert_true(TPF_EnableComponent(ctx->world, e1, pos_cid));

  assert_true(TPF_SetTag(ctx->world, e2, enemy_tid));

  assert_true(TPF_EnableComponent(ctx->world, e3, pos_cid));
  assert_true(TPF_SetTag(ctx->world, e3, enemy_tid));

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  TPF_ComponentMask filter = bit_of(pos_cid) | bit_of(enemy_tid);
  assert_true(TPF_ScanBegin(cursor, filter));
  assert_true(TPF_ScanNext(cursor));

  {
    const TPF_EntityID expected[] = {e0, e3};
    assert_cursor_contains_exact_ids(cursor, expected, 2);
  }

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_respects_cursor_capacity_as_pages(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID selected_tid = TPF_CreateTag(ctx->world, "selected");
  assert_int_not_equal(selected_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e3 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e4 = TPF_CreateEntity(ctx->world);

  assert_true(TPF_SetTag(ctx->world, e0, selected_tid));
  assert_true(TPF_SetTag(ctx->world, e1, selected_tid));
  assert_true(TPF_SetTag(ctx->world, e2, selected_tid));
  assert_true(TPF_SetTag(ctx->world, e3, selected_tid));
  assert_true(TPF_SetTag(ctx->world, e4, selected_tid));

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 2);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, bit_of(selected_tid)));

  assert_true(TPF_ScanNext(cursor));
  {
    const TPF_EntityID expected[] = {e0, e1};
    assert_cursor_contains_exact_ids(cursor, expected, 2);
  }

  assert_true(TPF_ScanNext(cursor));
  {
    const TPF_EntityID expected[] = {e2, e3};
    assert_cursor_contains_exact_ids(cursor, expected, 2);
  }

  assert_true(TPF_ScanNext(cursor));
  {
    const TPF_EntityID expected[] = {e4};
    assert_cursor_contains_exact_ids(cursor, expected, 1);
  }

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_skips_destroyed_entities(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID marked_tid = TPF_CreateTag(ctx->world, "marked");
  assert_int_not_equal(marked_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);

  assert_true(TPF_SetTag(ctx->world, e0, marked_tid));
  assert_true(TPF_SetTag(ctx->world, e1, marked_tid));
  assert_true(TPF_SetTag(ctx->world, e2, marked_tid));

  assert_true(TPF_DestroyEntity(ctx->world, e1));

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, bit_of(marked_tid)));
  assert_true(TPF_ScanNext(cursor));

  {
    const TPF_EntityID expected[] = {e0, e2};
    assert_cursor_contains_exact_ids(cursor, expected, 2);
  }

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_scan_alive_tag_matches_live_entities_only(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID alive_tid = TPF_GetAliveTag(ctx->world);
  assert_int_not_equal(alive_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);

  assert_true(TPF_DestroyEntity(ctx->world, e1));

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, bit_of(alive_tid)));
  assert_true(TPF_ScanNext(cursor));

  {
    const TPF_EntityID expected[] = {e0, e2};
    assert_cursor_contains_exact_ids(cursor, expected, 2);
  }

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_entity_id_is_recycled_after_destroy(void** state) {
  test_ctx* ctx = *state;

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e1 = TPF_CreateEntity(ctx->world);
  TPF_EntityID e2 = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e0, TPF_ECS_INVALID_EID);
  assert_int_not_equal(e1, TPF_ECS_INVALID_EID);
  assert_int_not_equal(e2, TPF_ECS_INVALID_EID);

  assert_true(TPF_DestroyEntity(ctx->world, e1));

  TPF_EntityID recycled = TPF_CreateEntity(ctx->world);
  assert_int_equal(recycled, e1);
}

static void test_recycled_entity_starts_only_with_alive_tag(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID pos_cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  TPF_TagID enemy_tid = TPF_CreateTag(ctx->world, "enemy");
  TPF_TagID alive_tid = TPF_GetAliveTag(ctx->world);

  assert_int_not_equal(pos_cid, TPF_ECS_INVALID_CID);
  assert_int_not_equal(enemy_tid, TPF_ECS_INVALID_TID);
  assert_int_not_equal(alive_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_EnableComponent(ctx->world, e, pos_cid));
  assert_true(TPF_SetTag(ctx->world, e, enemy_tid));

  assert_true(TPF_DestroyEntity(ctx->world, e));

  TPF_EntityID recycled = TPF_CreateEntity(ctx->world);
  assert_int_equal(recycled, e);

  assert_true(TPF_HasTag(ctx->world, recycled, alive_tid));
  assert_false(TPF_HasTag(ctx->world, recycled, enemy_tid));
  assert_false(TPF_HasComponent(ctx->world, recycled, pos_cid));
  assert_ptr_equal(TPF_GetComponentData(ctx->world, recycled, pos_cid), NULL);
}

static void test_scan_no_match_returns_false_and_empty_page(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID vel_cid = TPF_CreateComponent(
      ctx->world, "velocity", _Alignof(test_velocity), sizeof(test_velocity));
  assert_int_not_equal(vel_cid, TPF_ECS_INVALID_CID);

  (void)TPF_CreateEntity(ctx->world);
  (void)TPF_CreateEntity(ctx->world);

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, bit_of(vel_cid)));
  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

static void test_cursor_out_of_bounds_access_returns_invalid_eid(void** state) {
  test_ctx* ctx = *state;

  TPF_EntityID e0 = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e0, TPF_ECS_INVALID_EID);

  TPF_EntityCursor* cursor = TPF_CreateEntityCursor(ctx->world, 8);
  assert_non_null(cursor);

  assert_true(TPF_ScanBegin(cursor, 0));
  assert_true(TPF_ScanNext(cursor));

  assert_int_equal(TPF_CursorGetEntityID(cursor, 1), TPF_ECS_INVALID_EID);
  assert_int_equal(TPF_CursorGetEntityID(cursor, 999), TPF_ECS_INVALID_EID);

  assert_false(TPF_ScanNext(cursor));
  assert_int_equal(TPF_CursorGetCount(cursor), 0);

  TPF_DestroyEntityCursor(cursor);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(test_create_and_destroy_cursor,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_scan_empty_world_returns_false_and_no_results, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_scan_all_entities_with_zero_filter,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(test_scan_by_single_component,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(test_scan_by_single_tag, setup_world,
                                      teardown_world),
      cmocka_unit_test_setup_teardown(test_scan_by_component_and_tag,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_scan_respects_cursor_capacity_as_pages, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_scan_skips_destroyed_entities,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_scan_alive_tag_matches_live_entities_only, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_entity_id_is_recycled_after_destroy,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_recycled_entity_starts_only_with_alive_tag, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(
          test_scan_no_match_returns_false_and_empty_page, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(
          test_cursor_out_of_bounds_access_returns_invalid_eid, setup_world,
          teardown_world),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
