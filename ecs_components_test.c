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

typedef struct test_stats {
  int hp;
  int mp;
} test_stats;

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

static void test_create_and_find_component(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_ComponentID found = TPF_FindComponentID(ctx->world, "position");
  assert_int_equal(found, cid);

  assert_false(TPF_IsTag(ctx->world, cid));
}

static void test_duplicate_component_name_fails(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid1 = TPF_CreateComponent(
      ctx->world, "stats", _Alignof(test_stats), sizeof(test_stats));
  assert_int_not_equal(cid1, TPF_ECS_INVALID_CID);

  TPF_ComponentID cid2 = TPF_CreateComponent(
      ctx->world, "stats", _Alignof(test_stats), sizeof(test_stats));
  assert_int_equal(cid2, TPF_ECS_INVALID_CID);

  TPF_ComponentID found = TPF_FindComponentID(ctx->world, "stats");
  assert_int_equal(found, cid1);
}

static void test_find_component_does_not_return_tag_id(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid = TPF_CreateTag(ctx->world, "enemy");
  assert_int_not_equal(tid, TPF_ECS_INVALID_TID);

  TPF_ComponentID cid = TPF_FindComponentID(ctx->world, "enemy");
  assert_int_equal(cid, TPF_ECS_INVALID_CID);
}

static void test_enable_and_has_component(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_false(TPF_HasComponent(ctx->world, e, cid));

  assert_true(TPF_EnableComponent(ctx->world, e, cid));
  assert_true(TPF_HasComponent(ctx->world, e, cid));
}

static void test_disable_component(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_EnableComponent(ctx->world, e, cid));
  assert_true(TPF_HasComponent(ctx->world, e, cid));

  assert_true(TPF_DisableComponent(ctx->world, e, cid));
  assert_false(TPF_HasComponent(ctx->world, e, cid));
  assert_ptr_equal(TPF_GetComponentData(ctx->world, e, cid), NULL);
}

static void test_get_component_data_returns_null_when_not_enabled(
    void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_ptr_equal(TPF_GetComponentData(ctx->world, e, cid), NULL);
}

static void test_set_and_get_component_data(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_EnableComponent(ctx->world, e, cid));

  test_position in = {.x = 10.5f, .y = -2.0f, .z = 99.25f};
  assert_true(TPF_SetComponentData(ctx->world, e, cid, &in));

  test_position* out = TPF_GetComponentData(ctx->world, e, cid);
  assert_non_null(out);
  assert_true(TPF_HasComponent(ctx->world, e, cid));

  assert_float_equal(out->x, in.x, 0.0001f);
  assert_float_equal(out->y, in.y, 0.0001f);
  assert_float_equal(out->z, in.z, 0.0001f);
}

static void test_set_component_data_fails_when_not_enabled(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "stats", _Alignof(test_stats), sizeof(test_stats));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  test_stats stats = {.hp = 50, .mp = 10};
  assert_false(TPF_SetComponentData(ctx->world, e, cid, &stats));
}

static void test_copy_component_data_between_entities(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "stats", _Alignof(test_stats), sizeof(test_stats));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID from = TPF_CreateEntity(ctx->world);
  TPF_EntityID to = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(from, TPF_ECS_INVALID_EID);
  assert_int_not_equal(to, TPF_ECS_INVALID_EID);

  assert_true(TPF_EnableComponent(ctx->world, from, cid));
  assert_true(TPF_EnableComponent(ctx->world, to, cid));

  test_stats in = {.hp = 123, .mp = 77};
  assert_true(TPF_SetComponentData(ctx->world, from, cid, &in));
  assert_true(TPF_CopyComponentData(ctx->world, from, to, cid));

  test_stats* out = TPF_GetComponentData(ctx->world, to, cid);
  assert_non_null(out);
  assert_int_equal(out->hp, 123);
  assert_int_equal(out->mp, 77);
}

static void test_copy_component_data_fails_if_destination_not_enabled(
    void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "stats", _Alignof(test_stats), sizeof(test_stats));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID from = TPF_CreateEntity(ctx->world);
  TPF_EntityID to = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(from, TPF_ECS_INVALID_EID);
  assert_int_not_equal(to, TPF_ECS_INVALID_EID);

  assert_true(TPF_EnableComponent(ctx->world, from, cid));

  test_stats in = {.hp = 10, .mp = 20};
  assert_true(TPF_SetComponentData(ctx->world, from, cid, &in));

  assert_false(TPF_CopyComponentData(ctx->world, from, to, cid));
}

static void test_component_functions_reject_tag_ids(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid = TPF_CreateTag(ctx->world, "questgiver");
  assert_int_not_equal(tid, TPF_ECS_INVALID_TID);
  assert_true(TPF_IsTag(ctx->world, tid));

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_false(TPF_EnableComponent(ctx->world, e, tid));
  assert_false(TPF_DisableComponent(ctx->world, e, tid));
  assert_false(TPF_SetComponentData(ctx->world, e, tid, &(int){42}));
  assert_false(TPF_CopyComponentData(ctx->world, e, e, tid));
  assert_ptr_equal(TPF_GetComponentData(ctx->world, e, tid), NULL);

  /* This assumes your public semantics are strict:
     component queries should reject tag ids. */
  assert_false(TPF_HasComponent(ctx->world, e, tid));
}

static void test_duplicate_entity_copies_components(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID pos_cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  TPF_ComponentID stats_cid = TPF_CreateComponent(
      ctx->world, "stats", _Alignof(test_stats), sizeof(test_stats));
  assert_int_not_equal(pos_cid, TPF_ECS_INVALID_CID);
  assert_int_not_equal(stats_cid, TPF_ECS_INVALID_CID);

  TPF_EntityID original = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(original, TPF_ECS_INVALID_EID);

  assert_true(TPF_EnableComponent(ctx->world, original, pos_cid));
  assert_true(TPF_EnableComponent(ctx->world, original, stats_cid));

  test_position pos = {.x = 1.0f, .y = 2.0f, .z = 3.0f};
  test_stats stats = {.hp = 900, .mp = 250};

  assert_true(TPF_SetComponentData(ctx->world, original, pos_cid, &pos));
  assert_true(TPF_SetComponentData(ctx->world, original, stats_cid, &stats));

  TPF_EntityID copy = TPF_DuplicateEntity(ctx->world, original);
  assert_int_not_equal(copy, TPF_ECS_INVALID_EID);

  assert_true(TPF_HasComponent(ctx->world, copy, pos_cid));
  assert_true(TPF_HasComponent(ctx->world, copy, stats_cid));

  test_position* copy_pos = TPF_GetComponentData(ctx->world, copy, pos_cid);
  test_stats* copy_stats = TPF_GetComponentData(ctx->world, copy, stats_cid);

  assert_non_null(copy_pos);
  assert_non_null(copy_stats);

  assert_float_equal(copy_pos->x, 1.0f, 0.0001f);
  assert_float_equal(copy_pos->y, 2.0f, 0.0001f);
  assert_float_equal(copy_pos->z, 3.0f, 0.0001f);

  assert_int_equal(copy_stats->hp, 900);
  assert_int_equal(copy_stats->mp, 250);
}

static void test_destroyed_entity_no_longer_accepts_components(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "position", _Alignof(test_position), sizeof(test_position));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_EnableComponent(ctx->world, e, cid));

  test_position pos = {.x = 7.0f, .y = 8.0f, .z = 9.0f};
  assert_true(TPF_SetComponentData(ctx->world, e, cid, &pos));
  assert_non_null(TPF_GetComponentData(ctx->world, e, cid));

  assert_true(TPF_DestroyEntity(ctx->world, e));

  assert_false(TPF_HasComponent(ctx->world, e, cid));
  assert_false(TPF_EnableComponent(ctx->world, e, cid));
  assert_false(TPF_DisableComponent(ctx->world, e, cid));
  assert_false(TPF_SetComponentData(ctx->world, e, cid, &pos));
  assert_ptr_equal(TPF_GetComponentData(ctx->world, e, cid), NULL);
}

static void test_component_on_invalid_entity_fails(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(
      ctx->world, "stats", _Alignof(test_stats), sizeof(test_stats));
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  test_stats stats = {.hp = 1, .mp = 2};

  assert_false(TPF_HasComponent(ctx->world, TPF_ECS_INVALID_EID, cid));
  assert_false(TPF_EnableComponent(ctx->world, TPF_ECS_INVALID_EID, cid));
  assert_false(TPF_DisableComponent(ctx->world, TPF_ECS_INVALID_EID, cid));
  assert_false(
      TPF_SetComponentData(ctx->world, TPF_ECS_INVALID_EID, cid, &stats));
  assert_ptr_equal(TPF_GetComponentData(ctx->world, TPF_ECS_INVALID_EID, cid),
                   NULL);
  assert_false(TPF_CopyComponentData(ctx->world, TPF_ECS_INVALID_EID,
                                     TPF_ECS_INVALID_EID, cid));
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(test_create_and_find_component,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(test_duplicate_component_name_fails,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_find_component_does_not_return_tag_id, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_enable_and_has_component,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(test_disable_component, setup_world,
                                      teardown_world),
      cmocka_unit_test_setup_teardown(
          test_get_component_data_returns_null_when_not_enabled, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_set_and_get_component_data,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_set_component_data_fails_when_not_enabled, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_copy_component_data_between_entities,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_copy_component_data_fails_if_destination_not_enabled,
          setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(test_component_functions_reject_tag_ids,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(test_duplicate_entity_copies_components,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_destroyed_entity_no_longer_accepts_components, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_component_on_invalid_entity_fails,
                                      setup_world, teardown_world),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
