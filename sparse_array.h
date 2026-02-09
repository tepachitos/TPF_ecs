#ifndef TPF_ECS_SPARSE_ARRAY_H
#define TPF_ECS_SPARSE_ARRAY_H

#include <SDL3/SDL_stdinc.h>
#define TPF_ECS_SPARSE_ARRAY_PAGE_SIZE 1024
#define TPF_ECS_INVALID_INDEX ((size_t)-1)

struct page {
  size_t dense_index[TPF_ECS_SPARSE_ARRAY_PAGE_SIZE];
};

struct sparse_array {
  struct page** pages;
  size_t cap;
};

struct sparse_array* sparse_array_new();
bool sparse_array_has(struct sparse_array* sa, size_t i);
size_t sparse_array_get(struct sparse_array* sa, size_t i);
bool sparse_array_set(struct sparse_array* sa, size_t i, size_t value);
void sparse_array_del(struct sparse_array* sa, size_t i);
void sparse_array_destroy(struct sparse_array* sa);
#endif /* TPF_ECS_SPARSE_ARRAY_H */
