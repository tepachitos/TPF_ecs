#include "component_table.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>

#include "TPF/TPF_ecs_types.h"

#include "dynamic_array.h"
#include "sparse_array.h"

static size_t align_is_pow2(const size_t a) {
  return a && ((a & (a - 1)) == 0);
}

static bool is_config_valid(struct component_table_config config) {
  if (config.item_size == 0) {
    SDL_SetError("component_table requires item_size > 0");
    return false;
  }

  if (config.item_align == 0) {
    SDL_SetError("component_table requires item_align != 0");
    return false;
  }

  if (!align_is_pow2(config.item_align)) {
    SDL_SetError("component_table requires item_align to be power of 2");
    return false;
  }

  if (config.component_id == TPF_ECS_INVALID_CID) {
    SDL_SetError("component_id is invalid");
    return false;
  }

  return true;
}

struct component_table* component_table_new() {
  struct component_table* table = SDL_malloc(sizeof(struct component_table));
  if (table == NULL) {
    return NULL;
  }

  table->config = (struct component_table_config){0};
  table->config.component_id = TPF_ECS_INVALID_CID;
  table->sparse = NULL;
  table->dense = NULL;
  table->dense_entity_ids = NULL;
  return table;
}

void component_table_reset(struct component_table* table) {
  if (table == NULL) {
    return;
  }

  if (table->dense != NULL) {
    dynamic_array_destroy(table->dense);
    table->dense = NULL;
  }

  if (table->dense_entity_ids != NULL) {
    dynamic_array_destroy(table->dense_entity_ids);
    table->dense_entity_ids = NULL;
  }

  if (table->sparse != NULL) {
    sparse_array_destroy(table->sparse);
    table->sparse = NULL;
  }

  table->config.component_id = TPF_ECS_INVALID_CID;
  table->config.initial_slots = 0;
  table->config.item_size = 0;
  table->config.item_align = 0;
  SDL_memset(table->config.name, 0, TPF_ECS_NAME_SIZE);
}

bool component_table_configure(struct component_table* table,
                               struct component_table_config config) {
  SDL_assert_paranoid(table != NULL);
  if (table == NULL) {
    return false;
  }

  if (!is_config_valid(config)) {
    return false;
  }

  component_table_reset(table);
  table->dense = dynamic_array_new(config.item_align, config.item_size,
                                   config.initial_slots);
  if (table->dense == NULL) {
    return false;
  }

  table->dense_entity_ids = dynamic_array_new(
      _Alignof(TPF_EntityID), sizeof(TPF_EntityID), config.initial_slots);
  if (table->dense_entity_ids == NULL) {
    dynamic_array_destroy(table->dense);
    table->dense = NULL;
    return false;
  }

  table->sparse = sparse_array_new();
  if (table->sparse == NULL) {
    dynamic_array_destroy(table->dense);
    table->dense = NULL;

    dynamic_array_destroy(table->dense_entity_ids);
    table->dense_entity_ids = NULL;
    return false;
  }

  table->config = config;
  return true;
}

Uint8* component_table_get_slot(struct component_table* table,
                                TPF_EntityID entity_id) {
  SDL_assert_paranoid(table != NULL);
  SDL_assert_paranoid(entity_id != TPF_ECS_INVALID_EID);
  if (table == NULL || entity_id == TPF_ECS_INVALID_EID) {
    return NULL;
  }

  if (table->dense == NULL || table->sparse == NULL) {
    return NULL;
  }

  size_t max_index = dynamic_array_len(table->dense);
  size_t index = sparse_array_get(table->sparse, entity_id);
  if (index == TPF_ECS_INVALID_INDEX || index >= max_index) {
    return NULL;
  }

  return dynamic_array_get(table->dense, index);
}

bool component_table_add_slot(struct component_table* table,
                              TPF_EntityID entity_id) {
  SDL_assert_paranoid(table != NULL);
  SDL_assert_paranoid(entity_id != TPF_ECS_INVALID_EID);
  if (table == NULL || entity_id == TPF_ECS_INVALID_EID) {
    return false;
  }

  if (table->sparse == NULL || table->dense_entity_ids == NULL) {
    return false;
  }

  if (sparse_array_has(table->sparse, entity_id)) {
    return true;
  }

  size_t index = dynamic_array_len(table->dense_entity_ids);

  if (table->dense != NULL) {
    if (!dynamic_array_push_back(table->dense, NULL)) {
      return false;
    }
  }

  if (!dynamic_array_push_back(table->dense_entity_ids, (Uint8*)&entity_id)) {
    if (table->dense != NULL) {
      dynamic_array_pop_back(table->dense);
    }
    return false;
  }

  if (!sparse_array_set(table->sparse, entity_id, index)) {
    if (table->dense != NULL) {
      dynamic_array_pop_back(table->dense);
    }
    dynamic_array_pop_back(table->dense_entity_ids);
    return false;
  }

  return true;
}

bool component_table_del_slot(struct component_table* table,
                              TPF_EntityID entity_id) {
  SDL_assert_paranoid(table != NULL);
  SDL_assert_paranoid(entity_id != TPF_ECS_INVALID_EID);
  if (table == NULL || entity_id == TPF_ECS_INVALID_EID) {
    return false;
  }

  if (table->sparse == NULL || table->dense_entity_ids == NULL) {
    return false;
  }

  if (!sparse_array_has(table->sparse, entity_id)) {
    return true;
  }

  size_t del_index = sparse_array_get(table->sparse, entity_id);
  if (del_index == TPF_ECS_INVALID_INDEX) {
    return true;
  }

  size_t dense_len = dynamic_array_len(table->dense_entity_ids);
  if (del_index >= dense_len || dense_len == 0) {
    return false;
  }

  size_t last_index = dense_len - 1;

  if (del_index != last_index) {
    TPF_EntityID* last_entity_ptr =
        (TPF_EntityID*)dynamic_array_get(table->dense_entity_ids, last_index);
    if (last_entity_ptr == NULL) {
      return false;
    }

    TPF_EntityID last_entity = *last_entity_ptr;

    if (table->dense != NULL) {
      Uint8* last_data = dynamic_array_get(table->dense, last_index);
      if (last_data == NULL) {
        return false;
      }

      if (!dynamic_array_set(table->dense, del_index, last_data)) {
        return false;
      }
    }

    if (!dynamic_array_set(table->dense_entity_ids, del_index,
                           (Uint8*)&last_entity)) {
      return false;
    }

    if (!sparse_array_set(table->sparse, last_entity, del_index)) {
      return false;
    }
  }

  if (table->dense != NULL) {
    if (!dynamic_array_pop_back(table->dense)) {
      return false;
    }
  }

  if (!dynamic_array_pop_back(table->dense_entity_ids)) {
    return false;
  }

  sparse_array_del(table->sparse, entity_id);
  return true;
}

void component_table_destroy(struct component_table* table) {
  if (table == NULL) {
    return;
  }

  component_table_reset(table);
  SDL_free(table);
}
