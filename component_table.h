#ifndef TPF_ECS_COMPONENT_TABLE_H
#define TPF_ECS_COMPONENT_TABLE_H

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>

#include <TPF/TPF_ecs_types.h>
#include <TPF/build_config/ecs.h>

#include "dynamic_array.h"
#include "sparse_array.h"

struct component_table_config {
  TPF_ComponentID component_id;
  size_t item_align;
  size_t item_size;
  size_t initial_slots;
  char name[TPF_ECS_NAME_SIZE];
};

struct component_table {
  struct sparse_array* sparse;
  struct dynamic_array* dense;
  struct dynamic_array* dense_entity_ids;
  struct component_table_config config;
};

struct component_table* component_table_new();
void component_table_reset(struct component_table* table);
bool component_table_configure(struct component_table* table,
                               struct component_table_config config);
Uint8* component_table_get_slot(struct component_table* table,
                                TPF_EntityID entity_id);
bool component_table_add_slot(struct component_table* table,
                              TPF_EntityID entity_id);
bool component_table_del_slot(struct component_table* table,
                              TPF_EntityID entity_id);
void component_table_destroy(struct component_table* table);
#endif /* TPF_ECS_COMPONENT_TABLE_H */
