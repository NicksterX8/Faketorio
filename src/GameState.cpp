#include "GameState.hpp"

#include <functional>
#include <array>
#include <glm/geometric.hpp>

#include <SDL2/SDL.h>
#include "constants.hpp"
#include "rendering/textures.hpp"
#include "utils/vectors.hpp"
#include "Tiles.hpp"
#include "Player.hpp"
#include "ECS/entities/entities.hpp"
#include "ECS/systems/systems.hpp"
#include "metadata/ecs/ecs.hpp"

void makeItemPrototypes(ItemManager& im) {
    using namespace items;
    auto* pm = &im.prototypes;

    auto tile = New<ITC::Proto::Placeable>(pm, ItemTypes::Tile);

    ItemPrototype prototypes[] = {
        tile,
    };

    for (int i = 0; i < sizeof(prototypes) / sizeof(ItemPrototype); i++) {
        pm->add(prototypes[i]);
    }
}

void GameState::init() {
    /* Init Chunkmap */
    chunkmap.init();
    int chunkRadius = 4;
    for (int chunkX = -chunkRadius; chunkX < chunkRadius; chunkX++) {
        for (int chunkY = -chunkRadius; chunkY < chunkRadius; chunkY++) {
            ChunkData* chunkdata = chunkmap.newChunkAt({chunkX, chunkY});
            if (chunkdata) {
                generateChunk(chunkdata);
            } else {
                LogError("Failed to create chunk at tile (%d,%d) for initialization", chunkX * CHUNKSIZE, chunkY * CHUNKSIZE);
                continue;
            }
        }
    }

    /* Init ECS */
    ecs = EntityWorld::Init<ECS_COMPONENTS>();
    ecs.SetOnAdd<EC::Position>([&](EntityWorld* ecs, Entity entity) {
        //entityPositionChanged(&chunkmap, ecs, entity, {NAN, NAN});
        Vec2 pos = ecs->Get<EC::Position>(entity)->vec2();
        if (ecs->EntityHas<EC::Size>(entity)) {
            Vec2 size = ecs->Get<EC::Size>(entity)->vec2();
            IVec2 minChunkPosition = toChunkPosition(pos - size * 0.5f);
            IVec2 maxChunkPosition = toChunkPosition(pos + size * 0.5f);
            for (int col = minChunkPosition.x; col <= maxChunkPosition.x; col++) {
                for (int row = minChunkPosition.y; row <= maxChunkPosition.y; row++) {
                    IVec2 chunkPosition = {col, row};
                    // add entity to new chunk
                    ChunkData* newChunkdata = chunkmap.get(chunkPosition);
                    if (newChunkdata) {
                        newChunkdata->closeEntities.push(entity);
                    }
                }
            }
        } else {
            IVec2 chunkPosition = toChunkPosition(pos);
            // add entity to new chunk
            ChunkData* newChunkdata = chunkmap.get(chunkPosition);
            if (newChunkdata) {
                newChunkdata->closeEntities.push(entity);
            }
        }
    });
    ecs.SetOnAdd<EC::Size>([&](EntityWorld* ecs, Entity entity){
        //if (ecs->EntityHas<EC::Position>(entity))
        //    entitySizeChanged(&chunkmap, ecs, entity, {NAN, NAN});
    });
    ecs.SetBeforeRemove<EC::Position>([&](EntityWorld* ecs, Entity entity){
        Vec2 entityPosition = ecs->Get<EC::Position>(entity)->vec2();
        if (ecs->EntityHas<EC::Size>(entity)) {
            forEachChunkContainingBounds(&chunkmap, entityPosition, ecs->Get<EC::Size>(entity)->vec2(), [entity](ChunkData* chunkdata){
                if (!chunkdata->removeCloseEntity(entity)) {
                    LogWarn("couldn't remove entity");
                }
            });
        } else {
            chunkmap.get(toChunkPosition(entityPosition))->removeCloseEntity(entity);
        }
    });
    ecs.SetBeforeRemove<EC::Inventory>([this](EntityWorld* ecs, Entity entity){
        ecs->Get<EC::Inventory>(entity)->inventory.destroy(this->itemManager.inventoryAllocator);
    });

    /* Init Items */
    My::Vec<items::ComponentInfo> componentInfo; // TODO: Unimportant: This is leaked currently
    {
        using namespace ITC;
        constexpr auto componentInfoArray = GECS::getComponentInfoList<ITEM_COMPONENTS_LIST>();
        componentInfo = My::Vec<items::ComponentInfo>(&componentInfoArray[0], componentInfoArray.size());
    }
    itemManager = ItemManager(ArrayRef(componentInfo.data, componentInfo.size));

    makeItemPrototypes(itemManager);

    /* Init Player */
    player = Player(&ecs, Vec2(0, 0), itemManager);

    ItemStack startInventory[] = {
        items::Items::sandGun(itemManager),
        ItemStack(items::Items::tile(itemManager, TileTypes::Grass), 64),
        ItemStack(items::Items::tile(itemManager, TileTypes::Sand), 24),
        ItemStack(items::Items::tile(itemManager, TileTypes::Grass), 2)
    };

    Inventory* playerInventory = player.inventory();
    for (int i = 0; i < (int)(sizeof(startInventory) / sizeof(ItemStack)); i++) {
        playerInventory->addItemStack(startInventory[i]);
    }
}

void GameState::destroy() {
    chunkmap.destroy();
    ecs.destroy();
}

llvm::SmallVector<IVec2> raytraceDDA(const Vec2 start, const Vec2 end) {
    const IVec2 start_i = vecFloori(start);
    const IVec2 end_i = vecFloori(end);
    const int lineLength = (abs(end_i.x - start_i.x) + abs(end_i.y - start_i.y)) + 1;
    llvm::SmallVector<IVec2> line(lineLength, {}); // make small vector of size lineLength left uninitialized
    const Vec2 delta = end - start;
    const Vec2 dir = glm::normalize(delta);
    const Vec2 unitStep = {sqrt(1 + (dir.y / dir.x) * (dir.y / dir.x)), sqrt(1 + (dir.x / dir.y) * (dir.x / dir.y))};

    IVec2 mapCheck = start_i;
    Vec2 rayLength;
    IVec2 step;
    if (dir.x < 0) {
        step.x = -1;
        rayLength.x = (start.x - mapCheck.x) * unitStep.x;
    } else {
        step.x = 1;
        rayLength.x = ((mapCheck.x + 1) - start.x) * unitStep.x;
    }
    if (dir.y < 0) {
        step.y = -1;
        rayLength.y = (start.y - mapCheck.y) * unitStep.y;
    } else {
        step.y = 1;
        rayLength.y = ((mapCheck.y + 1) - start.y) * unitStep.y;
    }

    // have to check for horizontal and vertical lines because of divison by zero things

    // horizontal line
    if (start.y == end.y) {
        for (int i = 0; i < lineLength; i++) {
            line[i] = mapCheck;
            mapCheck.x += step.x;
        }
    }
    // vertical line
    else if (start_i.x == end_i.x) {
        for (int i = 0; i < lineLength; i++) {
            line[i] = mapCheck;
            mapCheck.y += step.y;
        }
    }
    // any other line
    else {
        for (int i = 0; i < lineLength; i++) {
            line[i] = mapCheck;
            if (rayLength.x < rayLength.y) {
                mapCheck.x += step.x;
                rayLength.x += unitStep.x;
            } else {
                mapCheck.y += step.y;
                rayLength.y += unitStep.y;
            }
        }
    }
    
    return line;
}

/*
* Get a line of chunk coordinates from start to end, using DDA.
* Returns a line of size lineSize that must be freed using free()
* @param lineSize a pointer to an int to be filled in the with size of the line.

IVec2* worldChunkLine(Vec2 startChunkCoord, Vec2 endChunkCoord, int* lineSize) {
    IVec2 startChunk = vecFloori(startChunkCoord);
    IVec2 endChunk = vecFloori(endChunkCoord);
    IVec2 delta = endChunk - startChunk;
    int lineTileLength = (abs(delta.x) + abs(delta.y)) + 1;
    IVec2* line = Alloc<IVec2>(lineTileLength);
    if (lineSize) *lineSize = lineTileLength;

    Vec2 dir = glm::normalize((Vec2)(endChunk - startChunk));

    IVec2 mapCheck = startChunk;
    Vec2 unitStep = {sqrt(1 + (dir.y / dir.x) * (dir.y / dir.x)), sqrt(1 + (dir.x / dir.y) * (dir.x / dir.y))};
    Vec2 rayLength;
    IVec2 step;
    if (dir.x < 0) {
        step.x = -1;
        rayLength.x = (start.x - mapCheck.x) * unitStep.x;
    } else {
        step.x = 1;
        rayLength.x = ((mapCheck.x + 1) - start.x) * unitStep.x;
    }
    if (dir.y < 0) {
        step.y = -1;
        rayLength.y = (start.y - mapCheck.y) * unitStep.y;
    } else {
        step.y = 1;
        rayLength.y = ((mapCheck.y + 1) - start.y) * unitStep.y;
    }

    // have to check for horizontal and vertical lines because of divison by zero things

    // horizontal line
    if (startTile.y == endTile.y) {
        for (int i = 0; i < lineTileLength; i++) {
            line[i] = mapCheck;
            mapCheck.x += step.x;
        }
    }
    // vertical line
    else if (startTile.x == endTile.x) {
        for (int i = 0; i < lineTileLength; i++) {
            line[i] = mapCheck;
            mapCheck.y += step.y;
        }
    }
    // any other line
    else {
        for (int i = 0; i < lineTileLength; i++) {
            line[i] = mapCheck;
            if (rayLength.x < rayLength.y) {
                mapCheck.x += step.x;
                rayLength.x += unitStep.x;
            } else {
                mapCheck.y += step.y;
                rayLength.y += unitStep.y;
            }
        }
    }
    
    return line;
}
*/

void worldLineAlgorithm(Vec2 start, Vec2 end, const std::function<int(IVec2)>& callback) {
    Vec2 delta = end - start;
    Vec2 dir = glm::normalize(delta);

    IVec2 mapCheck = {(int)floor(start.x), (int)floor(start.y)};
    Vec2 unitStep = {sqrt(1 + (dir.y / dir.x) * (dir.y / dir.x)), sqrt(1 + (dir.x / dir.y) * (dir.x / dir.y))};
    Vec2 rayLength;
    IVec2 step;
    if (dir.x < 0) {
        step.x = -1;
        rayLength.x = (start.x - mapCheck.x) * unitStep.x;
    } else {
        step.x = 1;
        rayLength.x = ((mapCheck.x + 1) - start.x) * unitStep.x;
    }
    if (dir.y < 0) {
        step.y = -1;
        rayLength.y = (start.y - mapCheck.y) * unitStep.y;
    } else {
        step.y = 1;
        rayLength.y = ((mapCheck.y + 1) - start.y) * unitStep.y;
    }

    // horizontal line
    if (floor(start.y) == floor(end.y)) {
        int iterations = abs(floor(end.x) - mapCheck.x) + 1;
        for (int i = 0; i < iterations; i++) {
            if (callback(mapCheck)) {
                return;
            }
            mapCheck.x += step.x;
        }
        return;
    }
    // vertical line
    if (floor(start.x) == floor(end.x)) {
        int iterations = abs(floor(end.y) - mapCheck.y) + 1;
        for (int i = 0; i < iterations; i++) {
            if (callback(mapCheck)) {
                return;
            }
            mapCheck.y += step.y;
        }
        return;
    }

    int iterations = 0; // for making sure it doesn't go infinitely
    while (iterations < 300) {
        if (rayLength.x < rayLength.y) {
            mapCheck.x += step.x;
            rayLength.x += unitStep.x;
        } else {
            mapCheck.y += step.y;
            rayLength.y += unitStep.y;
        }

        if (callback(mapCheck)) {
            return;
        }

        if (mapCheck.x == (int)floor(end.x) && mapCheck.y == (int)floor(end.y)) {
            break;
        }

        iterations++;
    }
}

bool pointIsOnTileEntity(const EntityWorld* ecs, const Entity tileEntity, IVec2 tilePosition, Vec2 point) {
    // check if the click was actually on the entity.
    bool clickedOnEntity = true;
    // if the entity doesnt have size data, assume the click was on the entity I guess,
    // since you cant do checks otherwise.
    // Kind of undecided whether the player should be able to click on an entity with no size
    auto size = ecs->Get<EC::Size>(tileEntity);
    if (size) {
        // if there isn't a position component, assume the position is the top left corner of the tile
        Vec2 position = Vec2(tilePosition.x, tilePosition.y);
        auto positionComponent = ecs->Get<EC::Position>(tileEntity);
        if (positionComponent) {
            position.x = positionComponent->x;
            position.y = positionComponent->y;
        }
        
        // now that we have position and size, do actual geometry check
        clickedOnEntity = (
            point.x > position.x && point.x < position.x + size->width &&
            point.y > position.y && point.y < position.y + size->height);        
    }

    return clickedOnEntity;
}

OptionalEntity<> findTileEntityAtPosition(const GameState* state, Vec2 position) {
    IVec2 tilePosition = vecFloori(position);
    Tile* selectedTile = getTileAtPosition(state->chunkmap, tilePosition);
    if (selectedTile) {
        /*
        auto tileEntity = selectedTile->entity;
        if (tileEntity.Exists(&state->ecs)) {
            if (pointIsOnTileEntity(&state->ecs, tileEntity, tilePosition, position)) {
                return tileEntity;
            }
        }
        */
    }

    // no entity could be found at that position
    return NullEntity;
}

bool removeEntityOnTile(EntityWorld* ecs, Tile* tile) {
    /*
    if (tile->entity.Exists(ecs)) {
        ecs->Destroy(tile->entity);
        return true;
    }
    tile->entity = NullEntity;
    */
    return false;
}

bool placeEntityOnTile(EntityWorld* ecs, Tile* tile, Entity entity) {
    /*
    if (!tile->entity.Exists(ecs)) {
        tile->entity = entity;
        return true;
    }
    */
    return false;
}

void forEachEntityInRange(const ComponentManager<EC::Position, EC::Size>& ecs, const ChunkMap* chunkmap, Vec2 pos, float radius, const std::function<int(EntityT<EC::Position>)>& callback) {
    int nCheckedEntities = 0;
    radius = abs(radius);
    float radiusSqrd = radius * radius;

    // form rectangle of chunks to search for entities in
    IVec2 highChunkBound = {(int)floor((pos.x + radius) / CHUNKSIZE), (int)floor((pos.y + radius) / CHUNKSIZE)};
    IVec2 lowChunkBound =  {(int)floor((pos.x - radius) / CHUNKSIZE), (int)floor((pos.y - radius) / CHUNKSIZE)};

    // go through each chunk in chunk search rectangle
    for (int y = lowChunkBound.y; y <= highChunkBound.y; y++) {
        for (int x = lowChunkBound.x; x <= highChunkBound.x; x++) {
            const ChunkData* chunkdata = chunkmap->get({x, y});
            if (chunkdata == NULL) {
                // entities can't be in non-existent chunks
                continue;
            }

            for (int e = 0; e < chunkdata->closeEntities.size; e++) {
                EntityT<EC::Position> closeEntity = chunkdata->closeEntities[e].cast<EC::Position>();
                if (!ecs.EntityExists(closeEntity)) {
                    LogError("Entity in closeEntities is dead!");
                    continue;
                }
                Vec2 entityCenter = ecs.Get<EC::Position>(closeEntity)->vec2();
                if (closeEntity.Has<EC::Size>(&ecs)) {
                    Vec2 halfSize = ecs.Get<EC::Size>(closeEntity)->vec2() * 0.5f;
                    entityCenter += halfSize;
                    radiusSqrd += (halfSize.x * halfSize.x + halfSize.y * halfSize.y) * M_SQRT2;
                }
                Vec2 delta = entityCenter - pos;
                float distSqrd = delta.x * delta.x + delta.y * delta.y;
                if (distSqrd < radiusSqrd) {
                    // entity is within radius
                    nCheckedEntities++;
                    if (callback(closeEntity)) {
                        return;
                    }
                }
            }
        }
    }
}

void forEachEntityNearPoint(const ComponentManager<EC::Position, const EC::Size>& ecs, const ChunkMap* chunkmap, Vec2 point, const std::function<int(EntityT<EC::Position>)>& callback) {
    IVec2 chunkPos = toChunkPosition(point);
    const ChunkData* chunkdata = chunkmap->get(chunkPos);
    if (!chunkdata) {
        // entities can't be in non-existent chunks
        return;
    }

    for (int e = 0; e < chunkdata->closeEntities.size; e++) {
        EntityT<EC::Position> closeEntity = chunkdata->closeEntities[e].cast<EC::Position>();
        if (callback(closeEntity)) {
            return;
        }
    }
}

void forEachChunkContainingBounds(const ChunkMap* chunkmap, Vec2 position, Vec2 size, const std::function<void(ChunkData*)>& callback) {
    IVec2 minChunkPosition = toChunkPosition(position - size * 0.5f);
    IVec2 maxChunkPosition = toChunkPosition(position + size * 0.5f);
    for (int col = minChunkPosition.x; col <= maxChunkPosition.x; col++) {
        for (int row = minChunkPosition.y; row <= maxChunkPosition.y; row++) {
            IVec2 chunkPosition = {col, row};
            ChunkData* chunkdata = chunkmap->get(chunkPosition);
            if (chunkdata) {
                callback(chunkdata);
            }
        }
    }
}

void forEachEntityInBounds(const ComponentManager<const EC::Position, const EC::Size>& ecs, const ChunkMap* chunkmap, Vec2 position, Vec2 size, const std::function<void(EntityT<EC::Position>)>& callback) {
    forEachChunkContainingBounds(chunkmap, position, size, [&](ChunkData* chunkdata){
        //position -= size/2.0f;
        Vec2 min = position - size/2.0f;
        Vec2 max = position + size/2.0f;

        for (int i = 0; i < chunkdata->closeEntities.size; i++) {
            auto closeEntity = chunkdata->closeEntities[i].cast<EC::Position>();
            assert(ecs.EntityExists(closeEntity));
            /*
            if (!ecs.EntityHas<EC::Position>(closeEntity)) {
                LogError("ERROROR!\n");
            }
            if (closeEntity.id == 2001 && closeEntity.version == 1) {
                LogInfo("That one!\n");
                auto ptr = ecs.Get<const EC::Position>(closeEntity);
                if (!ptr) {
                    LogError("UH OH!");
                }
            }
            */
            Vec2 entityPos = {0, 0};
            auto* entityPosPtr = ecs.Get<const EC::Position>(closeEntity);
            if (entityPosPtr) {
                entityPos = entityPosPtr->vec2();
            }
            if (ecs.EntityHas<EC::Size>(closeEntity)) {
                auto entitySize = ecs.Get<const EC::Size>(closeEntity)->vec2();
                Vec2 eMin = entityPos - entitySize / 2.0f;
                Vec2 eMax = entityPos + entitySize / 2.0f;
                entityPos -= entitySize / 2.0f;
                if (eMin.x < max.x && eMax.x > min.x &&
                    eMin.y < max.y && eMax.y > min.y)
                {
                    callback(closeEntity);
                }
            } else {
                if (entityPos.x > min.x && entityPos.x < max.x &&
                    entityPos.y > min.y && entityPos.y < max.y)
                {
                    callback(closeEntity);
                }
            }
            
        }
    });
}

OptionalEntity<EC::Position, EC::Size>
findFirstEntityAtPosition(const EntityWorld& ecs, const ChunkMap* chunkmap, Vec2 position) {
    OptionalEntity<EC::Position, EC::Size> foundEntity;
    forEachEntityNearPoint(ComponentManager<EC::Position, const EC::Render, EC::Size>(&ecs), chunkmap, position, [&](EntityT<EC::Position> entity){
        if (entity.Has<EC::Size>(&ecs)) {
            Vec2 entityPos = ecs.Get<EC::Position>(entity)->vec2();
            Vec2 size = ecs.Get<EC::Size>(entity)->vec2();
            if (
              position.x > entityPos.x && position.x < entityPos.x + size.x &&
              position.y > entityPos.y && position.y < entityPos.y + size.y) {
                // player is targeting this entity
                foundEntity = entity.cast<EC::Position, EC::Size>();
                return 1;
            }
        }
        return 0;
    });

    return foundEntity;
}



OptionalEntity<EC::Position>
findClosestEntityToPosition(const EntityWorld* ecs, const ChunkMap* chunkmap, Vec2 position) {
    EntityT<EC::Position> closestEntity = NullEntity;
    float closestDistSqrd = INFINITY;
    forEachEntityNearPoint(ecs, chunkmap, position, [&](EntityT<EC::Position> entity){
        Vec2 entityPos = ecs->Get<EC::Position>(entity)->vec2();
        Vec2 delta = entityPos - position;
        float entityDistSqrd = delta.x*delta.x + delta.y*delta.y;
        if (entityDistSqrd < closestDistSqrd) {
            closestEntity = entity;
            closestDistSqrd = entityDistSqrd;
        }
        return 0;
    });

    return closestEntity;
}

bool pointInsideEntityBounds(Vec2 point, const EntityT<EC::Position, EC::Size> entity, const ComponentManager<const EC::Position, const EC::Size>& ecs) {
    Vec2 entityPosition = entity.Get<const EC::Position>(&ecs)->vec2();
    Vec2 entitySize = entity.Get<const EC::Size>(&ecs)->vec2();

    Vec2 minEntityPos = entityPosition - entitySize * 0.5f;
    Vec2 maxEntityPos = entityPosition + entitySize * 0.5f;

    if (point.x > minEntityPos.x && point.x < maxEntityPos.x &&
        point.y > minEntityPos.y && point.y < maxEntityPos.y) {
        
        return true;
    }
    return false;
}

OptionalEntity<EC::Position, EC::Size, EC::Render>
findPlayerFocusedEntity(const ComponentManager<EC::Position, const EC::Size, const EC::Render>& ecs, const ChunkMap& chunkmap, Vec2 playerMousePos) {
    Vec2 target = playerMousePos;
    OptionalEntity<EC::Position, EC::Size, EC::Render> focusedEntity;
    int focusedEntityLayer = RenderLayers::Lowest;
    Uint32 focusedEntityRenderIndex = 0;
    forEachEntityNearPoint(ecs, &chunkmap, target,
    [&](EntityT<EC::Position> entity){
        if (!entity.Has<EC::Size, EC::Render>(&ecs)) {
            return 0;
        }

        auto render = entity.Get<const EC::Render>(&ecs);
        int entityLayer = render->layer;
        Uint32 entityRenderIndex = 0;

        if (pointInsideEntityBounds(target, entity.cast<EC::Position, EC::Size>(), ecs)) {
            if (entityLayer > focusedEntityLayer || (entityLayer == focusedEntityLayer && entityRenderIndex > focusedEntityRenderIndex)) {
                focusedEntity = entity.cast<EC::Position, EC::Size, EC::Render>();
                focusedEntityLayer = entityLayer;
                focusedEntityRenderIndex = entityRenderIndex;
            }
        }

        return 0;
    });
    return focusedEntity;
}