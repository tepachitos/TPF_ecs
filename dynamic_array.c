#include "dynamic_array.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>

#include <stdalign.h>
#include <stddef.h>

struct dynamic_array {
  Uint8* data;
  size_t item_align;
  size_t item_size;
  size_t stride;
  size_t cap;
  size_t len;
};

static size_t align_is_pow2(const size_t a) {
  return a && ((a & (a - 1)) == 0);
}

static bool align_is_supported_by_realloc(size_t a) {
  const size_t max = (alignof(max_align_t) < (2 * sizeof(void*)))
                         ? alignof(max_align_t)
                         : (2 * sizeof(void*));
  return a <= max;
}

static bool mul_overflow_size(size_t a, size_t b, size_t* out) {
#if defined(__has_builtin)
#if __has_builtin(__builtin_mul_overflow)
  return __builtin_mul_overflow(a, b, out);
#endif
#endif
  if (a == 0 || b == 0) {
    *out = 0;
    return false;
  }
  if (a > SIZE_MAX / b) {
    return true;
  }
  *out = a * b;
  return false;
}

static bool add_overflow_size(size_t a, size_t b, size_t* out) {
#if defined(__has_builtin)
#if __has_builtin(__builtin_add_overflow)
  return __builtin_add_overflow(a, b, out);
#endif
#endif
  if (a > SIZE_MAX - b) {
    return true;
  }
  *out = a + b;
  return false;
}

static bool align_up_checked(size_t n, size_t a, size_t* out) {
  SDL_assert(a != 0 && (a & (a - 1)) == 0);
  size_t t;
  if (add_overflow_size(n, a - 1, &t)) {
    return false;
  }
  *out = t & ~(a - 1);
  return true;
}

struct dynamic_array* dynamic_array_new(size_t item_align,
                                        size_t item_size,
                                        size_t initial_cap) {
  if (item_size == 0) {
    SDL_assert(false && "item size cannot be zero");
    return NULL;
  }

  if (!align_is_pow2(item_align) && align_is_supported_by_realloc(item_align)) {
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    SDL_assert(false &&
               "alignment is not a power of 2 or cannot be reallocated");
    return NULL;
  }

  struct dynamic_array* da = SDL_malloc(sizeof(struct dynamic_array));
  if (da == NULL) {
    return NULL;
  }

  size_t stride;
  if (!align_up_checked(item_size, item_align, &stride)) {
    SDL_assert(false && "stride overflow");
    SDL_free(da);
    return NULL;
  }

  da->item_align = item_align;
  da->item_size = item_size;
  da->stride = stride;
  da->len = 0;
  da->cap = 0;
  da->data = NULL;
  if (initial_cap > 0) {
    if (!dynamic_array_reserve(da, initial_cap)) {
      SDL_free(da);
      return NULL;
    }
  }

  return da;
}

bool dynamic_array_reserve(struct dynamic_array* da, size_t required) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return false;
  }

  if (required <= da->cap) {
    return true;
  }

  size_t bytes;
  if (mul_overflow_size(da->stride, required, &bytes)) {
    SDL_assert(false && "reserve overflow");
    return false;
  }

  Uint8* new_data = SDL_realloc(da->data, bytes);
  if (new_data == NULL) {
    return false;
  }

  da->data = new_data;
  da->cap = required;

  SDL_assert_paranoid(da->len <= da->cap);
  SDL_assert_paranoid(da->cap == 0 ? da->data == NULL : da->data != NULL);
  SDL_assert_paranoid(da->stride >= da->item_size);
  SDL_assert_paranoid((da->stride % da->item_align) == 0);
  return true;
}

bool dynamic_array_has(struct dynamic_array* da, size_t i) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return false;
  }

  return i < da->len;
}

void* dynamic_array_get(struct dynamic_array* da, size_t i) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return NULL;
  }

  if (i >= da->len) {
    return NULL;
  }

  return da->data + (i * da->stride);
}

bool dynamic_array_set(struct dynamic_array* da, size_t i, const void* v) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return false;
  }

  if (i >= da->len) {
    return false;
  }

  Uint8* head = da->data + (i * da->stride);
  if (v != NULL) {
    SDL_memcpy(head, v, da->item_size);
  } else {
    SDL_memset(head, 0, da->item_size);
  }

  return true;
}

bool dynamic_array_push_back(struct dynamic_array* da, const void* v) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return false;
  }

  size_t new_count;
  if (add_overflow_size(da->len, 1, &new_count)) {
    SDL_assert(false && "len overflow");
    return false;
  }

  if (new_count > da->cap) {
    size_t new_cap = da->cap ? da->cap * 2 : 8;
    size_t bytes;
    if (mul_overflow_size(da->stride, new_cap, &bytes)) {
      SDL_assert(false && "grow overflow");
      return false;
    }

    Uint8* new_data = SDL_realloc(da->data, bytes);
    if (new_data == NULL) {
      return false;
    }

    da->cap = new_cap;
    da->data = new_data;
  }

  da->len = new_count;
  bool set_last_value = dynamic_array_set(da, new_count - 1, v);

  SDL_assert_paranoid(da->len <= da->cap);
  SDL_assert_paranoid(da->cap == 0 ? da->data == NULL : da->data != NULL);
  SDL_assert_paranoid(da->stride >= da->item_size);
  SDL_assert_paranoid((da->stride % da->item_align) == 0);
  return set_last_value;
}

bool dynamic_array_pop_back(struct dynamic_array* da) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return false;
  }

  if (da->len == 0) {
    return false;
  }

  da->len -= 1;
  if (da->len <= da->cap / 4) {
    size_t min_cap = 0;
    size_t new_cap = da->cap / 2;
    if (new_cap < min_cap) {
      new_cap = min_cap;
    }
    if (new_cap < da->len) {
      new_cap = da->len;
    }

    if (da->len == 0) {
      SDL_free(da->data);
      da->data = NULL;
      da->len = 0;
      da->cap = 0;
      return true;
    }

    size_t bytes;
    if (mul_overflow_size(da->stride, new_cap, &bytes)) {
      SDL_assert(false && "grow overflow");
      return false;
    }

    Uint8* new_data = SDL_realloc(da->data, bytes);
    if (new_data == NULL) {
      return false;
    }

    da->cap = new_cap;
    da->data = new_data;
  }

  SDL_assert_paranoid(da->len <= da->cap);
  SDL_assert_paranoid(da->cap == 0 ? da->data == NULL : da->data != NULL);
  SDL_assert_paranoid(da->stride >= da->item_size);
  SDL_assert_paranoid((da->stride % da->item_align) == 0);
  return true;
}

void dynamic_array_clear(struct dynamic_array* da) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return;
  }

  size_t bytes;
  if (mul_overflow_size(da->stride, da->cap, &bytes)) {
    SDL_assert(false && "reserve overflow");
    return;
  }

  SDL_memset(da->data, 0, bytes);
  da->len = 0;
  return;
}

size_t dynamic_array_cap(struct dynamic_array* da) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return 0;
  }

  return da->cap;
}

size_t dynamic_array_len(struct dynamic_array* da) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return 0;
  }

  return da->len;
}

void dynamic_array_destroy(struct dynamic_array* da) {
  SDL_assert(da != NULL);
  if (da == NULL) {
    return;
  }

  SDL_free(da->data);
  SDL_free(da);
}
