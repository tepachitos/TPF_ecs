// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <TPF/TPF_ecs.h>
#include <TPF/TPF_ecs_types.h>

typedef struct {
  float x;
  float y;
  float z;
} Position;

typedef struct {
  float x;
  float y;
  float z;
} Velocity;

typedef struct {
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

static void test_ecs_example_of_use(void** state) {
  test_ctx* ctx = *state;
  TPF_World* world = ctx->world;

  /* 1. Register components and tags */
  TPF_ComponentID position_cid = TPF_CreateComponent(
      world, "position", _Alignof(Position), sizeof(Position));
  TPF_ComponentID velocity_cid = TPF_CreateComponent(
      world, "velocity", _Alignof(Velocity), sizeof(Velocity));
  TPF_TagID enemy_tid = TPF_CreateTag(world, "enemy");
  TPF_TagID moving_tid = TPF_CreateTag(world, "moving");

  assert_int_not_equal(position_cid, TPF_ECS_INVALID_CID);
  assert_int_not_equal(velocity_cid, TPF_ECS_INVALID_CID);
  assert_int_not_equal(enemy_tid, TPF_ECS_INVALID_TID);
  assert_int_not_equal(moving_tid, TPF_ECS_INVALID_TID);

  assert_int_equal(TPF_FindComponentID(world, "position"), position_cid);
  assert_int_equal(TPF_FindComponentID(world, "velocity"), velocity_cid);
  assert_int_equal(TPF_FindTagID(world, "enemy"), enemy_tid);
  assert_int_equal(TPF_FindTagID(world, "moving"), moving_tid);

  /* 2. Create some entities */
  TPF_EntityID e0 = TPF_CreateEntity(world);
  TPF_EntityID e1 = TPF_CreateEntity(world);
  TPF_EntityID e2 = TPF_CreateEntity(world);

  assert_int_not_equal(e0, TPF_ECS_INVALID_EID);
  assert_int_not_equal(e1, TPF_ECS_INVALID_EID);
  assert_int_not_equal(e2, TPF_ECS_INVALID_EID);

  /* 3. Attach components/tags in a normal gameplay-like flow */
  assert_true(TPF_EnableComponent(world, e0, position_cid));
  assert_true(TPF_EnableComponent(world, e0, velocity_cid));
  assert_true(TPF_SetTag(world, e0, enemy_tid));
  assert_true(TPF_SetTag(world, e0, moving_tid));

  assert_true(TPF_EnableComponent(world, e1, position_cid));
  assert_true(TPF_SetTag(world, e1, enemy_tid));

  assert_true(TPF_EnableComponent(world, e2, position_cid));
  assert_true(TPF_EnableComponent(world, e2, velocity_cid));
  assert_true(TPF_SetTag(world, e2, moving_tid));

  /* 4. Set component data */
  {
    Position p = {10.0f, 20.0f, 30.0f};
    Velocity v = {1.0f, 0.0f, 0.0f};

    assert_true(TPF_SetComponentData(world, e0, position_cid, &p));
    assert_true(TPF_SetComponentData(world, e0, velocity_cid, &v));
  }

  {
    Position p = {-5.0f, 2.5f, 8.0f};
    assert_true(TPF_SetComponentData(world, e1, position_cid, &p));
  }

  {
    Position p = {100.0f, 200.0f, 300.0f};
    Velocity v = {0.0f, -1.0f, 0.5f};

    assert_true(TPF_SetComponentData(world, e2, position_cid, &p));
    assert_true(TPF_SetComponentData(world, e2, velocity_cid, &v));
  }

  /* 5. Read data back */
  {
    Position* p0 = TPF_GetComponentData(world, e0, position_cid);
    Velocity* v0 = TPF_GetComponentData(world, e0, velocity_cid);

    assert_non_null(p0);
    assert_non_null(v0);

    assert_float_equal(p0->x, 10.0f, 0.0001f);
    assert_float_equal(p0->y, 20.0f, 0.0001f);
    assert_float_equal(p0->z, 30.0f, 0.0001f);

    assert_float_equal(v0->x, 1.0f, 0.0001f);
    assert_float_equal(v0->y, 0.0f, 0.0001f);
    assert_float_equal(v0->z, 0.0f, 0.0001f);
  }

  /* 6. Duplicate an entity */
  TPF_EntityID e3 = TPF_DuplicateEntity(world, e0);
  assert_int_not_equal(e3, TPF_ECS_INVALID_EID);

  assert_true(TPF_HasComponent(world, e3, position_cid));
  assert_true(TPF_HasComponent(world, e3, velocity_cid));
  assert_true(TPF_HasTag(world, e3, enemy_tid));
  assert_true(TPF_HasTag(world, e3, moving_tid));

  {
    Position* p3 = TPF_GetComponentData(world, e3, position_cid);
    Velocity* v3 = TPF_GetComponentData(world, e3, velocity_cid);

    assert_non_null(p3);
    assert_non_null(v3);

    assert_float_equal(p3->x, 10.0f, 0.0001f);
    assert_float_equal(p3->y, 20.0f, 0.0001f);
    assert_float_equal(p3->z, 30.0f, 0.0001f);

    assert_float_equal(v3->x, 1.0f, 0.0001f);
    assert_float_equal(v3->y, 0.0f, 0.0001f);
    assert_float_equal(v3->z, 0.0f, 0.0001f);
  }

  /* 7. Scan entities with a simple filter:
        entities that have both position and enemy tag */
  {
    TPF_EntityCursor* cursor = TPF_CreateEntityCursor(world, 16);
    assert_non_null(cursor);

    TPF_ComponentMask filter = TPF_MakeFilter(2, position_cid, enemy_tid);
    assert_true(TPF_ScanBegin(cursor, filter));
    assert_true(TPF_ScanNext(cursor));

    size_t count = TPF_CursorGetCount(cursor);
    assert_int_equal(count, 3);

    assert_int_equal(TPF_CursorGetEntityID(cursor, 0), e0);
    assert_int_equal(TPF_CursorGetEntityID(cursor, 1), e1);
    assert_int_equal(TPF_CursorGetEntityID(cursor, 2), e3);

    assert_false(TPF_ScanNext(cursor));
    assert_int_equal(TPF_CursorGetCount(cursor), 0);

    TPF_DestroyEntityCursor(cursor);
  }

  /* 8. Disable/remove some data from one entity */
  assert_true(TPF_ClearTag(world, e1, enemy_tid));
  assert_false(TPF_HasTag(world, e1, enemy_tid));

  assert_true(TPF_DisableComponent(world, e2, velocity_cid));
  assert_false(TPF_HasComponent(world, e2, velocity_cid));
  assert_ptr_equal(TPF_GetComponentData(world, e2, velocity_cid), NULL);

  /* 9. Destroy an entity and verify it stops participating */
  assert_true(TPF_DestroyEntity(world, e0));

  assert_false(TPF_HasComponent(world, e0, position_cid));
  assert_false(TPF_HasTag(world, e0, enemy_tid));

  {
    TPF_EntityCursor* cursor = TPF_CreateEntityCursor(world, 16);
    assert_non_null(cursor);

    TPF_ComponentMask filter = TPF_MakeFilter(2, position_cid, enemy_tid);
    assert_true(TPF_ScanBegin(cursor, filter));
    assert_true(TPF_ScanNext(cursor));

    size_t count = TPF_CursorGetCount(cursor);
    assert_int_equal(count, 1);
    assert_int_equal(TPF_CursorGetEntityID(cursor, 0), e3);

    assert_false(TPF_ScanNext(cursor));
    assert_int_equal(TPF_CursorGetCount(cursor), 0);

    TPF_DestroyEntityCursor(cursor);
  }

  /* 10. Create another entity to show normal continued usage */
  {
    TPF_EntityID e4 = TPF_CreateEntity(world);
    assert_int_not_equal(e4, TPF_ECS_INVALID_EID);

    assert_true(TPF_EnableComponent(world, e4, position_cid));
    assert_true(TPF_SetTag(world, e4, moving_tid));

    Position p = {999.0f, 1.0f, 2.0f};
    assert_true(TPF_SetComponentData(world, e4, position_cid, &p));

    Position* out = TPF_GetComponentData(world, e4, position_cid);
    assert_non_null(out);
    assert_float_equal(out->x, 999.0f, 0.0001f);
  }
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(test_ecs_example_of_use, setup_world,
                                      teardown_world),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
