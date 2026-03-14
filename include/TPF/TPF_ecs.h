#ifndef TPF_ECS_H
#define TPF_ECS_H

#include <SDL3/SDL_stdinc.h>
#include "TPF_ecs_types.h"

TPF_World* TPF_CreateWorld();
void TPF_DestroyWorld(TPF_World* world);

TPF_ComponentID TPF_CreateComponent(TPF_World* world,
                                    const char* name,
                                    size_t align,
                                    size_t size);
TPF_ComponentID TPF_FindComponentID(const TPF_World* world, const char* name);

TPF_TagID TPF_CreateTag(TPF_World* world, const char* name);
TPF_TagID TPF_FindTagID(const TPF_World* world, const char* name);

TPF_EntityID TPF_CreateEntity(TPF_World* world);
TPF_EntityID TPF_DuplicateEntity(TPF_World* world, TPF_EntityID copying_eid);
bool TPF_DestroyEntity(TPF_World* world, TPF_EntityID deleting_eid);

bool TPF_HasComponent(const TPF_World* world,
                      TPF_EntityID eid,
                      TPF_ComponentID cid);
bool TPF_EnableComponent(TPF_World* world,
                         TPF_EntityID eid,
                         TPF_ComponentID cid);
bool TPF_DisableComponent(TPF_World* world,
                          TPF_EntityID eid,
                          TPF_ComponentID cid);
void* TPF_GetComponentData(const TPF_World* world,
                           TPF_EntityID eid,
                           TPF_ComponentID cid);
bool TPF_SetComponentData(TPF_World* world,
                          TPF_EntityID eid,
                          TPF_ComponentID cid,
                          const void* data);
bool TPF_CopyComponentData(TPF_World* world,
                           TPF_EntityID from_eid,
                           TPF_EntityID to_eid,
                           TPF_ComponentID cid);

bool TPF_HasTag(const TPF_World* world, TPF_EntityID eid, TPF_TagID tid);
bool TPF_SetTag(TPF_World* world, TPF_EntityID eid, TPF_TagID tid);
bool TPF_ClearTag(TPF_World* world, TPF_EntityID eid, TPF_TagID tid);
bool TPF_ToggleTag(TPF_World* world, TPF_EntityID eid, TPF_TagID tid);

TPF_TagID TPF_GetAliveTag(const TPF_World* world);
bool TPF_IsTag(const TPF_World* world, Uint8 cid);

TPF_EntityCursor* TPF_CreateEntityCursor(TPF_World* world, size_t cap);
size_t TPF_CursorGetCount(TPF_EntityCursor* cursor);
TPF_EntityID TPF_CursorGetEntityID(TPF_EntityCursor* cursor, size_t index);
bool TPF_ScanBegin(TPF_EntityCursor* cursor, TPF_ComponentMask filter);
bool TPF_ScanNext(TPF_EntityCursor* cursor);
void TPF_DestroyEntityCursor(TPF_EntityCursor* cursor);
#endif /* TPF_ECS_H */
