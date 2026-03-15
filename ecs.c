#include <TPF/TPF_ecs.h>
#include <TPF/TPF_ecs_types.h>

#include <TPF/build_config/ecs.h>

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>
#include <stdarg.h>

#include "component_table.h"
#include "dynamic_array.h"
#include "sparse_array.h"

typedef Uint8 TPF_ComponentOrTagID;  // internal use only
struct TPF_World {
  struct component_table** tables;
  struct sparse_array* entity_masks;
  struct dynamic_array* removed_entities;
  TPF_ComponentOrTagID last_table_id;
  TPF_EntityID last_eid;
  TPF_TagID alive_tid;
};

struct TPF_EntityCursor {
  struct dynamic_array* entities;
  size_t last_scanned_eid;
  TPF_World* world;
  TPF_ComponentMask filter;
};

static TPF_ComponentOrTagID find_component_or_tag(const TPF_World* world,
                                                  const char* name) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(name != NULL);
  if (world == NULL || name == NULL) {
    return TPF_ECS_INVALID_CID;
  }

  if (SDL_strlen(name) >= TPF_ECS_NAME_SIZE) {
    SDL_SetError("name too long: %s", name);
    return TPF_ECS_INVALID_CID;
  }

  for (unsigned i = 0; i < TPF_ECS_MAX_COMPONENTS; i++) {
    struct component_table* table = world->tables[i];
    if (table == NULL) {
      continue;
    }

    if (SDL_strncmp(name, table->config.name, TPF_ECS_NAME_SIZE) == 0) {
      return table->config.component_id;
    }
  }

  return TPF_ECS_INVALID_CID;
}

static TPF_ComponentOrTagID create_component_or_tag(TPF_World* world,
                                                    const char* name,
                                                    size_t align,
                                                    size_t size,
                                                    bool holds_data) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(name != NULL);
  if (world == NULL || name == NULL) {
    return TPF_ECS_INVALID_CID;
  }

  if (find_component_or_tag(world, name) != TPF_ECS_INVALID_CID) {
    SDL_SetError("component or tag already exists: %s", name);
    return TPF_ECS_INVALID_CID;
  }

  TPF_ComponentOrTagID new_cid = ++world->last_table_id;
  if (new_cid >= TPF_ECS_MAX_COMPONENTS) {
    world->last_table_id--;
    SDL_SetError("max number of components (and tags) reached: %d", new_cid);
    return TPF_ECS_INVALID_CID;
  }

  struct component_table_config config = {
      .component_id = new_cid,
      .initial_slots = TPF_ECS_PREALLOCATED_ENTITIES,
      .item_align = align,
      .item_size = size,
      .holds_data = holds_data,
  };
  SDL_strlcpy(config.name, name, TPF_ECS_NAME_SIZE);
  if (!component_table_configure(world->tables[new_cid], config)) {
    component_table_reset(world->tables[new_cid]);
    world->last_table_id--;
    SDL_SetError("could not configure component %s (size=%ld, align=%ld): %s",
                 name, size, align, SDL_GetError());
    return TPF_ECS_INVALID_CID;
  }

  return new_cid;
}

static bool has_component_or_tag(const TPF_World* world,
                                 TPF_EntityID eid,
                                 TPF_ComponentOrTagID ctid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->entity_masks != NULL);
  SDL_assert_paranoid(eid != TPF_ECS_INVALID_EID);
  SDL_assert_paranoid(ctid != TPF_ECS_INVALID_CID);
  if (world == NULL || world->entity_masks == NULL ||
      eid == TPF_ECS_INVALID_EID || ctid == TPF_ECS_INVALID_CID) {
    return false;
  }

  TPF_ComponentMask mask = sparse_array_get(world->entity_masks, eid);
  return (mask & ((TPF_ComponentMask)1u << ctid)) != 0;
}

static bool is_valid_component(const TPF_World* world,
                               TPF_ComponentOrTagID ctid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->tables != NULL);
  SDL_assert_paranoid(ctid != TPF_ECS_INVALID_CID);
  if (world == NULL || world->tables == NULL || ctid == TPF_ECS_INVALID_CID) {
    return false;
  }

  struct component_table* table = world->tables[ctid];
  if (table == NULL) {
    return false;
  }

  return table->config.component_id == ctid && table->config.holds_data;
}

TPF_World* TPF_CreateWorld() {
  TPF_World* world = SDL_malloc(sizeof(TPF_World));
  if (world == NULL) {
    return NULL;
  }

  world->tables = NULL;
  world->entity_masks = NULL;
  world->removed_entities = NULL;
  world->last_eid = (TPF_EntityID)-1;
  world->last_table_id = (TPF_ComponentOrTagID)-1;
  world->alive_tid = (TPF_TagID)-1;

  // initialize arrays and tables
  world->entity_masks = sparse_array_new();
  if (world->entity_masks == NULL) {
    TPF_DestroyWorld(world);
    return NULL;
  }

  world->removed_entities =
      dynamic_array_new(_Alignof(TPF_EntityID), sizeof(TPF_EntityID),
                        TPF_ECS_PREALLOCATED_ENTITIES);
  if (world->removed_entities == NULL) {
    TPF_DestroyWorld(world);
    return NULL;
  }

  world->tables =
      SDL_malloc(sizeof(struct component_table*) * TPF_ECS_MAX_COMPONENTS);
  if (world->tables == NULL) {
    TPF_DestroyWorld(world);
    return NULL;
  }

  for (unsigned i = 0; i < TPF_ECS_MAX_COMPONENTS; i++) {
    world->tables[i] = component_table_new();
    if (world->tables[i] == NULL) {
      TPF_DestroyWorld(world);
      return NULL;
    }
  }

  world->alive_tid = TPF_CreateTag(world, TPF_ECS_ALIVE_TAG);
  if (world->alive_tid == TPF_ECS_INVALID_TID) {
    TPF_DestroyWorld(world);
    return NULL;
  }

  return world;
}

void TPF_DestroyWorld(TPF_World* world) {
  if (world == NULL) {
    return;
  }

  if (world->entity_masks != NULL) {
    sparse_array_destroy(world->entity_masks);
  }

  if (world->removed_entities != NULL) {
    dynamic_array_destroy(world->removed_entities);
  }

  if (world->tables != NULL) {
    for (unsigned i = 0; i < TPF_ECS_MAX_COMPONENTS; i++) {
      struct component_table* table = world->tables[i];
      if (table != NULL) {
        component_table_destroy(table);
      }
    }

    SDL_free(world->tables);
  }

  SDL_free(world);
}

TPF_ComponentID TPF_CreateComponent(TPF_World* world,
                                    const char* name,
                                    size_t align,
                                    size_t size) {
  return create_component_or_tag(world, name, align, size, true);
}

TPF_ComponentID TPF_FindComponentID(const TPF_World* world, const char* name) {
  TPF_ComponentOrTagID id = find_component_or_tag(world, name);
  if (TPF_IsTag(world, id)) {
    return TPF_ECS_INVALID_CID;
  }

  return id;
}

TPF_TagID TPF_CreateTag(TPF_World* world, const char* name) {
  return create_component_or_tag(world, name, 0, 0, false);
}

TPF_TagID TPF_FindTagID(const TPF_World* world, const char* name) {
  TPF_ComponentOrTagID id = find_component_or_tag(world, name);
  if (!TPF_IsTag(world, id)) {
    return TPF_ECS_INVALID_CID;
  }

  return id;
}

TPF_EntityID TPF_CreateEntity(TPF_World* world) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->entity_masks != NULL);
  SDL_assert_paranoid(world->removed_entities != NULL);
  if (world == NULL || world->entity_masks == NULL ||
      world->removed_entities == NULL) {
    return TPF_ECS_INVALID_EID;
  }

  TPF_ComponentMask mask = ((TPF_ComponentMask)1u << TPF_GetAliveTag(world));
  if (dynamic_array_len(world->removed_entities) > 0) {
    size_t last_idx = dynamic_array_len(world->removed_entities) - 1;
    TPF_EntityID recycled_entity_id =
        *(TPF_EntityID*)dynamic_array_get(world->removed_entities, last_idx);
    if (!dynamic_array_pop_back(world->removed_entities)) {
      SDL_SetError("could not recycle entity: %ld", recycled_entity_id);
      return TPF_ECS_INVALID_EID;
    }

    if (!sparse_array_set(world->entity_masks, recycled_entity_id, mask)) {
      SDL_SetError("could not recycle entity: %ld", recycled_entity_id);
      return TPF_ECS_INVALID_EID;
    }

    return recycled_entity_id;
  }

  TPF_EntityID new_eid = ++world->last_eid;
  if (world->last_eid >= TPF_ECS_MAX_ENTITIES - 1) {
    SDL_SetError("max number of entities reached: %ld", world->last_eid);
    world->last_eid--;
    return TPF_ECS_INVALID_EID;
  }

  if (!sparse_array_set(world->entity_masks, new_eid, mask)) {
    SDL_SetError("could not register mask for entity: %ld", new_eid);
    world->last_eid--;
    return TPF_ECS_INVALID_EID;
  }
  return new_eid;
}

TPF_EntityID TPF_DuplicateEntity(TPF_World* world, TPF_EntityID original_eid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->entity_masks != NULL);
  SDL_assert_paranoid(original_eid != TPF_ECS_INVALID_EID);
  if (world == NULL || world->entity_masks == NULL ||
      original_eid == TPF_ECS_INVALID_EID) {
    return TPF_ECS_INVALID_EID;
  }

  if (!sparse_array_has(world->entity_masks, original_eid)) {
    return TPF_ECS_INVALID_EID;
  }

  TPF_EntityID new_eid = TPF_CreateEntity(world);
  if (new_eid == TPF_ECS_INVALID_EID) {
    SDL_SetError("could not create new entity: %ld: %s", new_eid,
                 SDL_GetError());
    return TPF_ECS_INVALID_EID;
  }

  for (unsigned cid = 0; cid < TPF_ECS_MAX_COMPONENTS; cid++) {
    if (TPF_IsTag(world, cid)) {
      if (TPF_HasTag(world, original_eid, cid)) {
        if (!TPF_SetTag(world, new_eid, cid)) {
          SDL_SetError("could not set tag %u for entity %ld", cid, new_eid);
          TPF_DestroyEntity(world, new_eid);
          return TPF_ECS_INVALID_EID;
        }
      }
      continue;
    }

    if (!TPF_HasComponent(world, original_eid, cid)) {
      continue;
    }

    if (!TPF_EnableComponent(world, new_eid, cid)) {
      SDL_SetError("could not enable component %d for entity %ld", cid,
                   new_eid);
      TPF_DestroyEntity(world, new_eid);
      return TPF_ECS_INVALID_EID;
    }

    if (!TPF_CopyComponentData(world, original_eid, new_eid, cid)) {
      SDL_SetError("could not copy component data from %ld to %ld",
                   original_eid, new_eid);
      TPF_DestroyEntity(world, new_eid);
      return TPF_ECS_INVALID_EID;
    }
  }

  return new_eid;
}

bool TPF_DestroyEntity(TPF_World* world, TPF_EntityID entity_id) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->entity_masks != NULL);
  SDL_assert_paranoid(world->removed_entities != NULL);
  SDL_assert_paranoid(entity_id != TPF_ECS_INVALID_EID);
  if (world == NULL || world->entity_masks == NULL ||
      world->removed_entities == NULL || entity_id == TPF_ECS_INVALID_EID) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, entity_id)) {
    return false;
  }

  for (unsigned cid = 0; cid < TPF_ECS_MAX_COMPONENTS; cid++) {
    if (TPF_IsTag(world, cid)) {
      continue;
    }

    TPF_DisableComponent(world, entity_id, cid);
  }

  sparse_array_set(world->entity_masks, entity_id, 0);
  sparse_array_del(world->entity_masks, entity_id);
  return dynamic_array_push_back(world->removed_entities, &entity_id);
}

bool TPF_HasComponent(const TPF_World* world,
                      TPF_EntityID eid,
                      TPF_ComponentID cid) {
  if (!sparse_array_has(world->entity_masks, eid)) {
    return false;
  }

  return is_valid_component(world, cid) &&
         has_component_or_tag(world, eid, cid);
}

bool TPF_EnableComponent(TPF_World* world,
                         TPF_EntityID eid,
                         TPF_ComponentID cid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->entity_masks != NULL);
  SDL_assert_paranoid(eid != TPF_ECS_INVALID_EID);
  SDL_assert_paranoid(cid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(cid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || world->entity_masks == NULL ||
      eid == TPF_ECS_INVALID_EID || cid == TPF_ECS_INVALID_CID ||
      cid >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, eid) ||
      !is_valid_component(world, cid)) {
    return false;
  }

  TPF_ComponentMask mask = sparse_array_get(world->entity_masks, eid);
  if (!component_table_add_slot(world->tables[cid], eid)) {
    return false;
  }

  if (!sparse_array_set(world->entity_masks, eid,
                        mask | ((TPF_ComponentMask)1u << cid))) {
    component_table_del_slot(world->tables[cid], eid);
    return false;
  }

  return true;
}

bool TPF_DisableComponent(TPF_World* world,
                          TPF_EntityID eid,
                          TPF_ComponentID cid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->entity_masks != NULL);
  SDL_assert_paranoid(eid != TPF_ECS_INVALID_EID);
  SDL_assert_paranoid(cid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(cid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || world->entity_masks == NULL ||
      eid == TPF_ECS_INVALID_EID || cid == TPF_ECS_INVALID_CID ||
      cid >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, eid) ||
      !is_valid_component(world, cid)) {
    return false;
  }

  TPF_ComponentMask mask = sparse_array_get(world->entity_masks, eid);
  if (!sparse_array_set(world->entity_masks, eid,
                        mask & ~((TPF_ComponentMask)1u << cid))) {
    return false;
  }

  if (!component_table_del_slot(world->tables[cid], eid)) {
    sparse_array_set(world->entity_masks, eid, mask);
    return false;
  }

  return true;
}

void* TPF_GetComponentData(const TPF_World* world,
                           TPF_EntityID eid,
                           TPF_ComponentID cid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->tables != NULL);
  SDL_assert_paranoid(eid != TPF_ECS_INVALID_EID);
  SDL_assert_paranoid(cid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(cid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || world->tables == NULL || eid == TPF_ECS_INVALID_EID ||
      cid == TPF_ECS_INVALID_CID || cid >= TPF_ECS_MAX_COMPONENTS) {
    return NULL;
  }

  if (!sparse_array_has(world->entity_masks, eid) ||
      !is_valid_component(world, cid)) {
    return NULL;
  }

  return component_table_get_slot(world->tables[cid], eid);
}

bool TPF_SetComponentData(TPF_World* world,
                          TPF_EntityID eid,
                          TPF_ComponentID cid,
                          const void* data) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->tables != NULL);
  SDL_assert_paranoid(eid != TPF_ECS_INVALID_EID);
  SDL_assert_paranoid(cid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(cid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || world->tables == NULL || eid == TPF_ECS_INVALID_EID ||
      cid == TPF_ECS_INVALID_CID || cid >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, eid) ||
      !is_valid_component(world, cid)) {
    return false;
  }

  return component_table_set_slot(world->tables[cid], eid, data);
}

bool TPF_CopyComponentData(TPF_World* world,
                           TPF_EntityID from_eid,
                           TPF_EntityID to_eid,
                           TPF_ComponentID cid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(world->tables != NULL);
  SDL_assert_paranoid(from_eid != TPF_ECS_INVALID_EID);
  SDL_assert_paranoid(to_eid != TPF_ECS_INVALID_EID);
  SDL_assert_paranoid(cid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(cid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || world->tables == NULL ||
      from_eid == TPF_ECS_INVALID_EID || to_eid == TPF_ECS_INVALID_EID ||
      cid == TPF_ECS_INVALID_CID || cid >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, from_eid) ||
      !sparse_array_has(world->entity_masks, to_eid) ||
      !is_valid_component(world, cid)) {
    return false;
  }

  return component_table_copy_slot(world->tables[cid], from_eid, to_eid);
}

bool TPF_HasTag(const TPF_World* world, TPF_EntityID eid, TPF_TagID tid) {
  if (!sparse_array_has(world->entity_masks, eid)) {
    return false;
  }

  return TPF_IsTag(world, tid) && has_component_or_tag(world, eid, tid);
}

bool TPF_SetTag(TPF_World* world, TPF_EntityID eid, TPF_TagID tid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(tid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(tid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || tid == TPF_ECS_INVALID_CID ||
      tid >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, eid) || !TPF_IsTag(world, tid)) {
    return false;
  }

  TPF_ComponentMask mask = sparse_array_get(world->entity_masks, eid);
  return sparse_array_set(world->entity_masks, eid,
                          mask | ((TPF_ComponentMask)1u << tid));
}

bool TPF_ClearTag(TPF_World* world, TPF_EntityID eid, TPF_TagID tid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(tid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(tid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || tid == TPF_ECS_INVALID_CID ||
      tid >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, eid) || !TPF_IsTag(world, tid)) {
    return false;
  }

  TPF_ComponentMask mask = sparse_array_get(world->entity_masks, eid);
  return sparse_array_set(world->entity_masks, eid,
                          mask & ~((TPF_ComponentMask)1u << tid));
}

bool TPF_ToggleTag(TPF_World* world, TPF_EntityID eid, TPF_TagID tid) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(tid != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(tid < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || tid == TPF_ECS_INVALID_CID ||
      tid >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  if (!sparse_array_has(world->entity_masks, eid) || !TPF_IsTag(world, tid)) {
    return false;
  }

  TPF_ComponentMask mask = sparse_array_get(world->entity_masks, eid);
  return sparse_array_set(world->entity_masks, eid,
                          mask ^ ((TPF_ComponentMask)1u << tid));
}

TPF_TagID TPF_GetAliveTag(const TPF_World* world) {
  SDL_assert_paranoid(world != NULL);
  if (world == NULL) {
    return TPF_ECS_INVALID_TID;
  }

  return world->alive_tid;
}

bool TPF_IsTag(const TPF_World* world, Uint8 id) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(id != TPF_ECS_INVALID_CID);
  SDL_assert_paranoid(id < TPF_ECS_MAX_COMPONENTS);
  if (world == NULL || world->tables == NULL || id == TPF_ECS_INVALID_CID ||
      id >= TPF_ECS_MAX_COMPONENTS) {
    return false;
  }

  struct component_table* table = world->tables[id];
  if (table == NULL) {
    return false;
  }

  return table->config.component_id == id && !table->config.holds_data;
}

TPF_ComponentMask TPF_MakeFilter(unsigned n, ...) {
  SDL_assert_paranoid(n < TPF_ECS_MAX_COMPONENTS);
  if (n >= TPF_ECS_MAX_COMPONENTS) {
    return 0;
  }

  va_list va;
  va_start(va, n);
  TPF_ComponentMask mask = 0;
  for (unsigned i = 0; i < n; i++) {
    mask |= ((TPF_ComponentMask)1u << va_arg(va, TPF_ComponentMask));
  }
  va_end(va);

  return mask;
}

TPF_EntityCursor* TPF_CreateEntityCursor(TPF_World* world, size_t cap) {
  SDL_assert_paranoid(world != NULL);
  SDL_assert_paranoid(cap > 0);
  if (world == NULL || cap == 0) {
    return NULL;
  }

  TPF_EntityCursor* cursor = SDL_malloc(sizeof(TPF_EntityCursor));
  if (cursor == NULL) {
    return NULL;
  }

  cursor->world = world;
  cursor->last_scanned_eid = 0;
  cursor->filter = 0;
  cursor->entities =
      dynamic_array_new(_Alignof(TPF_EntityID), sizeof(TPF_EntityID), cap);
  if (cursor->entities == NULL) {
    TPF_DestroyEntityCursor(cursor);
    return NULL;
  }

  return cursor;
}

size_t TPF_CursorGetCount(TPF_EntityCursor* cursor) {
  SDL_assert_paranoid(cursor != NULL);
  SDL_assert_paranoid(cursor->entities != NULL);
  if (cursor == NULL || cursor->entities == NULL) {
    return 0;
  }

  return dynamic_array_len(cursor->entities);
}

TPF_EntityID TPF_CursorGetEntityID(TPF_EntityCursor* cursor, size_t index) {
  SDL_assert_paranoid(cursor != NULL);
  SDL_assert_paranoid(cursor->entities != NULL);
  if (cursor == NULL || cursor->entities == NULL) {
    return TPF_ECS_INVALID_EID;
  }

  if (!dynamic_array_has(cursor->entities, index)) {
    return TPF_ECS_INVALID_EID;
  }

  return *(TPF_EntityID*)dynamic_array_get(cursor->entities, index);
}

bool TPF_ScanBegin(TPF_EntityCursor* cursor, TPF_ComponentMask filter) {
  SDL_assert_paranoid(cursor != NULL);
  SDL_assert_paranoid(cursor->entities != NULL);
  if (cursor == NULL || cursor->entities == NULL) {
    return false;
  }

  cursor->filter = filter;
  cursor->last_scanned_eid = 0;
  dynamic_array_clear(cursor->entities);
  return true;
}

bool TPF_ScanNext(TPF_EntityCursor* cursor) {
  SDL_assert_paranoid(cursor != NULL);
  SDL_assert_paranoid(cursor->world != NULL);
  SDL_assert_paranoid(cursor->entities != NULL);
  if (cursor == NULL || cursor->world == NULL || cursor->entities == NULL) {
    return false;
  }

  TPF_World* world = cursor->world;
  TPF_ComponentMask filter = cursor->filter;
  TPF_EntityID cur_eid = cursor->last_scanned_eid;
  size_t cap = dynamic_array_cap(cursor->entities);
  size_t cursor_pos = 0;

  if (world->last_eid == TPF_ECS_INVALID_EID) {
    return false;
  }

  dynamic_array_clear(cursor->entities);
  for (; cursor_pos < cap && cur_eid <= world->last_eid; cur_eid++) {
    TPF_ComponentMask mask = sparse_array_get(world->entity_masks, cur_eid);
    if (mask == TPF_ECS_INVALID_INDEX) {
      continue;
    }

    if ((mask & filter) != filter) {
      continue;
    }

    if (!dynamic_array_push_back(cursor->entities, &cur_eid)) {
      SDL_SetError("could not set entity_id for cursor index %ld", cursor_pos);
      cursor->last_scanned_eid = cur_eid;
      return false;
    }

    cursor_pos++;
  }

  cursor->last_scanned_eid = cur_eid;
  return dynamic_array_len(cursor->entities) > 0;
}

void TPF_DestroyEntityCursor(TPF_EntityCursor* cursor) {
  if (cursor == NULL) {
    return;
  }

  if (cursor->entities != NULL) {
    dynamic_array_destroy(cursor->entities);
  }

  SDL_free(cursor);
}
