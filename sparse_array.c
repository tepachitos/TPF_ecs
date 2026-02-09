#include "sparse_array.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>

static bool ensure_page_table(struct sparse_array* sa, size_t hi) {
  SDL_assert_paranoid(sa != NULL);

  if (hi < sa->cap) {
    return true;
  }

  size_t old_cap = sa->cap;
  size_t new_cap = old_cap ? old_cap : 1;
  while (hi >= new_cap) {
    if (new_cap > (SIZE_MAX / 2)) {
      return false;
    }
    new_cap *= 2;
  }

  size_t new_bytes = new_cap * sizeof(struct page*);
  struct page** new_pages = SDL_realloc(sa->pages, new_bytes);
  if (new_pages == NULL) {
    return false;
  }

  // zero only the newly added slots
  SDL_memset(&new_pages[old_cap], 0,
             (new_cap - old_cap) * sizeof(struct page*));

  sa->pages = new_pages;
  sa->cap = new_cap;
  return true;
}

struct sparse_array* sparse_array_new() {
  struct sparse_array* sa = SDL_malloc(sizeof(struct sparse_array));
  if (sa == NULL) {
    return NULL;
  }

  sa->pages = NULL;
  sa->cap = 0;
  return sa;
}

bool sparse_array_has(struct sparse_array* sa, size_t i) {
  SDL_assert_paranoid(sa != NULL);
  SDL_assert_paranoid(i != TPF_ECS_INVALID_INDEX);
  return sparse_array_get(sa, i) != TPF_ECS_INVALID_INDEX;
}

size_t sparse_array_get(struct sparse_array* sa, size_t i) {
  SDL_assert_paranoid(sa != NULL);
  SDL_assert_paranoid(i != TPF_ECS_INVALID_INDEX);

  if (sa == NULL || i == TPF_ECS_INVALID_INDEX) {
    return TPF_ECS_INVALID_INDEX;
  }

  size_t hi = i / TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;
  size_t lo = i % TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;

  if (sa->pages == NULL || hi >= sa->cap) {
    return TPF_ECS_INVALID_INDEX;
  }

  struct page* page = sa->pages[hi];
  if (page == NULL) {
    return TPF_ECS_INVALID_INDEX;
  }

  return page->dense_index[lo];
}

bool sparse_array_set(struct sparse_array* sa, size_t i, size_t value) {
  SDL_assert_paranoid(sa != NULL);
  SDL_assert_paranoid(i != TPF_ECS_INVALID_INDEX);

  if (sa == NULL || i == TPF_ECS_INVALID_INDEX) {
    return false;
  }

  size_t hi = i / TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;
  size_t lo = i % TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;

  if (!ensure_page_table(sa, hi)) {
    return false;
  }

  struct page* page = sa->pages[hi];
  if (page == NULL) {
    page = SDL_malloc(sizeof(*page));
    if (page == NULL) {
      return false;
    }
    // Fill with INVALID (all bytes 0xFF)
    SDL_memset(page->dense_index, 0xFF, sizeof(page->dense_index));
    sa->pages[hi] = page;
  }

  page->dense_index[lo] = value;
  return true;
}

void sparse_array_del(struct sparse_array* sa, size_t i) {
  SDL_assert_paranoid(sa != NULL);
  SDL_assert_paranoid(i != TPF_ECS_INVALID_INDEX);

  if (sa == NULL || i == TPF_ECS_INVALID_INDEX) {
    return;
  }

  size_t hi = i / TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;
  size_t lo = i % TPF_ECS_SPARSE_ARRAY_PAGE_SIZE;

  if (sa->pages == NULL || hi >= sa->cap) {
    return;
  }

  struct page* page = sa->pages[hi];
  if (page == NULL) {
    return;
  }

  page->dense_index[lo] = TPF_ECS_INVALID_INDEX;
}

void sparse_array_destroy(struct sparse_array* sa) {
  SDL_assert_paranoid(sa != NULL);
  if (sa == NULL) {
    return;
  }

  if (sa->pages != NULL) {
    for (size_t hi = 0; hi < sa->cap; hi++) {
      SDL_free(sa->pages[hi]);
    }
    SDL_free(sa->pages);
  }

  SDL_free(sa);
}
