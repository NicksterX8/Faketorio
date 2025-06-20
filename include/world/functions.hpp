#ifndef WORLD_FUNCTIONS
#define WORLD_FUNCTIONS

#include "EntityWorld.hpp"

struct ChunkMap;

namespace World {

inline float getLayerHeight(int layer) {
    return layer * 0.05f - 0.5f;
}

inline float getEntityHeight(ECS::EntityID id, int renderLayer) {
    return getLayerHeight(renderLayer) + id * 0.0000001f;
}

inline bool pointInEntity(Vec2 point, Entity entity, const EntityWorld& ecs) {
    bool clickedOnEntity = false;

    const auto* viewbox = ecs.Get<const EC::ViewBox>(entity);
    const auto* position = ecs.Get<const EC::Position>(entity);
    if (viewbox && position) {
        FRect entityRect = viewbox->box.rect();
        entityRect.x += position->x;
        entityRect.y += position->y;
        clickedOnEntity = pointInRect(point, entityRect); 
    }
    
    return clickedOnEntity;
}

Entity findPlayerFocusedEntity(const EntityWorld& ecs, const ChunkMap& chunkmap, Vec2 playerMousePos);

Box getEntityViewBoxBounds(const EntityWorld* ecs, Entity entity);

void setEventCallbacks(EntityWorld& ecs, ChunkMap& chunkmap);

void forEachEntityInRange(const EntityWorld& ecs, const ChunkMap* chunkmap, Vec2 pos, float radius, const std::function<int(Entity)>& callback);

void forEachEntityNearPoint(const EntityWorld& ecs, const ChunkMap* chunkmap, Vec2 point, const std::function<int(Entity)>& callback);

void forEachEntityInBounds(const EntityWorld& ecs, const ChunkMap* chunkmap, Boxf bounds, const std::function<void(Entity)>& callback);

Entity findClosestEntityToPosition(const EntityWorld& ecs, const ChunkMap* chunkmap, Vec2 position);

}

#endif