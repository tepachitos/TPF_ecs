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

static void test_alive_tag_exists_and_is_set_on_new_entities(void** state) {
  test_ctx* ctx = *state;
  assert_non_null(ctx);
  assert_non_null(ctx->world);

  TPF_TagID alive_tid = TPF_GetAliveTag(ctx->world);
  assert_int_not_equal(alive_tid, TPF_ECS_INVALID_TID);
  assert_true(TPF_IsTag(ctx->world, alive_tid));

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_HasTag(ctx->world, e, alive_tid));
}

static void test_create_and_find_tag(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid = TPF_CreateTag(ctx->world, "enemy");
  assert_int_not_equal(tid, TPF_ECS_INVALID_TID);

  TPF_TagID found = TPF_FindTagID(ctx->world, "enemy");
  assert_int_equal(found, tid);

  assert_true(TPF_IsTag(ctx->world, tid));
}

static void test_duplicate_tag_name_fails(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid1 = TPF_CreateTag(ctx->world, "quest");
  assert_int_not_equal(tid1, TPF_ECS_INVALID_TID);

  TPF_TagID tid2 = TPF_CreateTag(ctx->world, "quest");
  assert_int_equal(tid2, TPF_ECS_INVALID_TID);

  TPF_TagID found = TPF_FindTagID(ctx->world, "quest");
  assert_int_equal(found, tid1);
}

static void test_find_tag_does_not_return_component_id(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(ctx->world, "transform",
                                            _Alignof(float), sizeof(float) * 4);
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);

  TPF_TagID tid = TPF_FindTagID(ctx->world, "transform");
  assert_int_equal(tid, TPF_ECS_INVALID_TID);
}

static void test_set_and_has_tag(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid = TPF_CreateTag(ctx->world, "selected");
  assert_int_not_equal(tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_false(TPF_HasTag(ctx->world, e, tid));

  assert_true(TPF_SetTag(ctx->world, e, tid));
  assert_true(TPF_HasTag(ctx->world, e, tid));
}

static void test_clear_tag(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid = TPF_CreateTag(ctx->world, "visible");
  assert_int_not_equal(tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_SetTag(ctx->world, e, tid));
  assert_true(TPF_HasTag(ctx->world, e, tid));

  assert_true(TPF_ClearTag(ctx->world, e, tid));
  assert_false(TPF_HasTag(ctx->world, e, tid));
}

static void test_toggle_tag(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid = TPF_CreateTag(ctx->world, "highlighted");
  assert_int_not_equal(tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_false(TPF_HasTag(ctx->world, e, tid));

  assert_true(TPF_ToggleTag(ctx->world, e, tid));
  assert_true(TPF_HasTag(ctx->world, e, tid));

  assert_true(TPF_ToggleTag(ctx->world, e, tid));
  assert_false(TPF_HasTag(ctx->world, e, tid));
}

static void test_tag_functions_reject_component_ids(void** state) {
  test_ctx* ctx = *state;

  TPF_ComponentID cid = TPF_CreateComponent(ctx->world, "velocity",
                                            _Alignof(float), sizeof(float) * 3);
  assert_int_not_equal(cid, TPF_ECS_INVALID_CID);
  assert_false(TPF_IsTag(ctx->world, cid));

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_false(TPF_SetTag(ctx->world, e, cid));
  assert_false(TPF_ClearTag(ctx->world, e, cid));
  assert_false(TPF_ToggleTag(ctx->world, e, cid));

  /* This assertion assumes your reviewed API semantics:
     TPF_HasTag should reject component IDs and return false. */
  assert_false(TPF_HasTag(ctx->world, e, cid));
}

static void test_duplicate_entity_copies_tags(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID friendly_tid = TPF_CreateTag(ctx->world, "friendly");
  TPF_TagID vendor_tid = TPF_CreateTag(ctx->world, "vendor");
  assert_int_not_equal(friendly_tid, TPF_ECS_INVALID_TID);
  assert_int_not_equal(vendor_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID original = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(original, TPF_ECS_INVALID_EID);

  assert_true(TPF_SetTag(ctx->world, original, friendly_tid));
  assert_true(TPF_SetTag(ctx->world, original, vendor_tid));

  TPF_EntityID copy = TPF_DuplicateEntity(ctx->world, original);
  assert_int_not_equal(copy, TPF_ECS_INVALID_EID);

  assert_true(TPF_HasTag(ctx->world, copy, TPF_GetAliveTag(ctx->world)));
  assert_true(TPF_HasTag(ctx->world, copy, friendly_tid));
  assert_true(TPF_HasTag(ctx->world, copy, vendor_tid));
}

static void test_destroyed_entity_no_longer_accepts_tags(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID tid = TPF_CreateTag(ctx->world, "temporary");
  assert_int_not_equal(tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_SetTag(ctx->world, e, tid));
  assert_true(TPF_HasTag(ctx->world, e, tid));

  assert_true(TPF_DestroyEntity(ctx->world, e));

  assert_false(TPF_HasTag(ctx->world, e, tid));
  assert_false(TPF_SetTag(ctx->world, e, tid));
  assert_false(TPF_ClearTag(ctx->world, e, tid));
  assert_false(TPF_ToggleTag(ctx->world, e, tid));
}

static void test_clearing_alive_tag_is_allowed_currently(void** state) {
  test_ctx* ctx = *state;

  TPF_TagID alive_tid = TPF_GetAliveTag(ctx->world);
  assert_int_not_equal(alive_tid, TPF_ECS_INVALID_TID);

  TPF_EntityID e = TPF_CreateEntity(ctx->world);
  assert_int_not_equal(e, TPF_ECS_INVALID_EID);

  assert_true(TPF_HasTag(ctx->world, e, alive_tid));

  /* This documents current behavior.
     If later you decide alive must be immutable, change this test. */
  assert_true(TPF_ClearTag(ctx->world, e, alive_tid));
  assert_false(TPF_HasTag(ctx->world, e, alive_tid));
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(
          test_alive_tag_exists_and_is_set_on_new_entities, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_create_and_find_tag, setup_world,
                                      teardown_world),
      cmocka_unit_test_setup_teardown(test_duplicate_tag_name_fails,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_find_tag_does_not_return_component_id, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(test_set_and_has_tag, setup_world,
                                      teardown_world),
      cmocka_unit_test_setup_teardown(test_clear_tag, setup_world,
                                      teardown_world),
      cmocka_unit_test_setup_teardown(test_toggle_tag, setup_world,
                                      teardown_world),
      cmocka_unit_test_setup_teardown(test_tag_functions_reject_component_ids,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(test_duplicate_entity_copies_tags,
                                      setup_world, teardown_world),
      cmocka_unit_test_setup_teardown(
          test_destroyed_entity_no_longer_accepts_tags, setup_world,
          teardown_world),
      cmocka_unit_test_setup_teardown(
          test_clearing_alive_tag_is_allowed_currently, setup_world,
          teardown_world),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
