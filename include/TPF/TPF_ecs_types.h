#ifndef TPF_ECS_TYPES_H
#define TPF_ECS_TYPES_H

#include <SDL3/SDL_stdinc.h>

typedef struct TPF_World TPF_World;
typedef Uint8 TPF_ComponentID;
typedef Uint8 TPF_TagID;
typedef Uint64 TPF_EntityID;
typedef Uint64 TPF_ComponentMask;

typedef struct TPF_EntityCursor TPF_EntityCursor;
typedef struct TPF_ScanOptions {
  TPF_ComponentMask filter;
  size_t cursor_cap;
} TPF_ScanOptions;

#define TPF_ECS_INVALID_CID ((TPF_ComponentID)-1)
#define TPF_ECS_INVALID_TID ((TPF_TagID)-1)
#define TPF_ECS_INVALID_EID ((TPF_EntityID)-1)
#define TPF_ECS_INVALID_MASK ((TPF_ComponentMask)-1)
#endif /* TPF_ECS_TYPES_H */
