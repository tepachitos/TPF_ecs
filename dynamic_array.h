#ifndef TPF_ECS_DYNAMIC_ARRAY_H
#define TPF_ECS_DYNAMIC_ARRAY_H

#include <SDL3/SDL_stdinc.h>

struct dynamic_array;

struct dynamic_array* dynamic_array_new(size_t item_align,
                                        size_t item_size,
                                        size_t initial_cap);
bool dynamic_array_reserve(struct dynamic_array* da, size_t required);
bool dynamic_array_has(struct dynamic_array* da, size_t i);
Uint8* dynamic_array_get(struct dynamic_array* da, size_t i);
bool dynamic_array_set(struct dynamic_array* da, size_t i, Uint8* v);
bool dynamic_array_push_back(struct dynamic_array* da, Uint8* v);
bool dynamic_array_pop_back(struct dynamic_array* da);
size_t dynamic_array_cap(struct dynamic_array* da);
size_t dynamic_array_len(struct dynamic_array* da);
void dynamic_array_destroy(struct dynamic_array* da);
#endif /* TPF_ECS_DYNAMIC_ARRAY_H */
