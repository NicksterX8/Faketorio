#include "GameState.hpp"

#include <unordered_map>
#include <functional>
#include <array>
#include <vector>

#include <SDL2/SDL.h>
#include "constants.hpp"
#include "Textures.hpp"
#include "NC/cpp-vectors.hpp"
#include "NC/utils.h"
#include "SDL_FontCache/SDL_FontCache.h"
#include "SDL2_gfx/SDL2_gfx.h"
#include "Tiles.hpp"
#include "Player.hpp"
#include "Entities/Entities.hpp"
#include "EntitySystems/Rendering.hpp"
#include "EntitySystems/Systems.hpp"

//void foreachEntityInRange(EntityWorld* ecs, Vec2 point, float distance, std::function<void(Entity)> callback) {   
//}

GameState::~GameState() {
    chunkmap.destroy();
    ecs.destroy();
}

void GameState::init(SDL_Renderer* renderer, GameViewport* gameViewport) {

    chunkmap.init();
    int chunkRadius = 10;
    for (int chunkX = -chunkRadius; chunkX < chunkRadius; chunkX++) {
        for (int chunkY = -chunkRadius; chunkY < chunkRadius; chunkY++) {
            ChunkData* chunkdata = chunkmap.createChunk({chunkX, chunkY});
            generateChunk(chunkdata->chunk);
        }
    }

    ecs.init<COMPONENTS>();
    ecs.SetOnAdd<EC::Position>([&](EntityWorld* ecs, Entity entity){
        //entityPositionChanged(&chunkmap, ecs, entity, {NAN, NAN});
        Vec2 pos = ecs->Get<EC::Position>(entity)->vec2();
        if (ecs->EntityHas<EC::Size>(entity)) {
            Vec2 size = ecs->Get<EC::Size>(entity)->vec2();
            IVec2 minChunkPosition = toChunkPosition(pos - size.scaled(0.5f));
            IVec2 maxChunkPosition = toChunkPosition(pos + size.scaled(0.5f));
            for (int col = minChunkPosition.x; col <= maxChunkPosition.x; col++) {
                for (int row = minChunkPosition.y; row <= maxChunkPosition.y; row++) {
                    IVec2 chunkPosition = {col, row};
                    // add entity to new chunk
                    ChunkData* newChunkdata = chunkmap.getChunkData(chunkPosition);
                    if (newChunkdata) {
                        newChunkdata->closeEntities.push_back(entity);
                    }
                }
            }
        } else {
            IVec2 chunkPosition = toChunkPosition(pos);
            // add entity to new chunk
            ChunkData* newChunkdata = chunkmap.getChunkData(chunkPosition);
            if (newChunkdata) {
                newChunkdata->closeEntities.push_back(entity);
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
                    Log.Warn("couldn't remove entity");
                }
            });
        } else {
            chunkmap.getChunkData(toChunkPosition(entityPosition))->removeCloseEntity(entity);
        }
    });
    ecs.SetBeforeRemove<EC::Inventory>([](EntityWorld* ecs, Entity entity){
        ecs->Get<EC::Inventory>(entity)->inventory.destroy();
    });

    /*
    auto renderSystem = new SimpleRectRenderSystem(&ecs, renderer, gameViewport);
    ecs.NewSystem<SimpleRectRenderSystem>(renderSystem);
    ecs.NewSystems<SYSTEMS>();
    ecs.System<InserterSystem>()->chunkmap = &chunkmap;
    ecs.testInitialization();
    */

    player = Player(&ecs);

    ItemStack startInventory[] = {
        {Items::SpaceFloor, INFINITE_ITEM_QUANTITY},
        {Items::Grass, 64},
        {Items::Wall, 64},
        {Items::Grenade, INFINITE_ITEM_QUANTITY},
        {Items::SandGun, 1}
    };

    Inventory* playerInventory = player.inventory();
    for (int i = 0; i < (int)(sizeof(startInventory) / sizeof(ItemStack)); i++) {
        playerInventory->addItemStack(startInventory[i]);
    }
}

/*
* Get a line of tile coordinates from start to end, using DDA.
* Returns a line of size lineSize that must be freed using free()
* @param lineSize a pointer to an int to be filled in the with size of the line.
*/
IVec2* worldLine(Vec2 start, Vec2 end, int* lineSize) {
    IVec2 startTile = {(int)floor(start.x), (int)floor(start.y)};
    IVec2 endTile = {(int)floor(end.x), (int)floor(end.y)};
    int lineTileLength = (abs(endTile.x - startTile.x) + abs(endTile.y - startTile.y)) + 1;
    IVec2* line = (IVec2*)malloc(lineTileLength * sizeof(IVec2));
    if (lineSize) *lineSize = lineTileLength;

    Vec2 dir = ((Vec2)(end - start)).norm();

    IVec2 mapCheck = startTile;
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

void worldLineAlgorithm(Vec2 start, Vec2 end, std::function<int(IVec2)> callback) {
    Vec2 delta = end - start;
    Vec2 dir = delta.norm();

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
    IVec2 tilePosition = position.floorToIVec();
    Tile* selectedTile = getTileAtPosition(state->chunkmap, tilePosition);
    if (selectedTile) {
        auto tileEntity = selectedTile->entity;
        if (tileEntity.Exists(&state->ecs)) {
            if (pointIsOnTileEntity(&state->ecs, tileEntity, tilePosition, position)) {
                return tileEntity;
            }
        }
    }

    // no entity could be found at that position
    return NullEntity;
}

bool removeEntityOnTile(EntityWorld* ecs, Tile* tile) {
    if (tile->entity.Exists(ecs)) {
        ecs->Destroy(tile->entity);
        return true;
    }
    tile->entity = NullEntity;
    return false;
}

bool placeEntityOnTile(EntityWorld* ecs, Tile* tile, Entity entity) {
    if (!tile->entity.Exists(ecs)) {
        tile->entity = entity;
        return true;
    }
    return false;
}

void forEachEntityInRange(const ComponentManager<EC::Position, EC::Size>& ecs, const ChunkMap* chunkmap, Vec2 pos, float radius, std::function<int(EntityType<EC::Position>)> callback) {
    int nCheckedEntities = 0;
    radius = abs(radius);
    float radiusSqrd = radius * radius;

    // form rectangle of chunks to search for entities in
    IVec2 highChunkBound = {(int)floor((pos.x + radius) / CHUNKSIZE), (int)floor((pos.y + radius) / CHUNKSIZE)};
    IVec2 lowChunkBound =  {(int)floor((pos.x - radius) / CHUNKSIZE), (int)floor((pos.y - radius) / CHUNKSIZE)};

    // go through each chunk in chunk search rectangle
    for (int y = lowChunkBound.y; y <= highChunkBound.y; y++) {
        for (int x = lowChunkBound.x; x <= highChunkBound.x; x++) {
            const ChunkData* chunkdata = chunkmap->getChunkData({x, y});
            if (chunkdata == NULL) {
                // entities can't be in non-existent chunks
                continue;
            }

            for (size_t e = 0; e < chunkdata->closeEntities.size(); e++) {
                EntityType<EC::Position> closeEntity = chunkdata->closeEntities[e].cast<EC::Position>();
                if (!ecs.EntityExists(closeEntity)) {
                    Log.Error("Entity in closeEntities is dead!");
                    continue;
                }
                Vec2 entityCenter = ecs.Get<EC::Position>(closeEntity)->vec2();
                if (closeEntity.Has<EC::Size>(&ecs)) {
                    Vec2 halfSize = ecs.Get<EC::Size>(closeEntity)->vec2().scaled(0.5f);
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

void forEachEntityNearPoint(const ComponentManager<EC::Position, const EC::Size>& ecs, const ChunkMap* chunkmap, Vec2 point, std::function<int(EntityType<EC::Position>)> callback) {
    IVec2 chunkPos = toChunkPosition(point);
    const ChunkData* chunkdata = chunkmap->getChunkData(chunkPos);
    if (!chunkdata) {
        // entities can't be in non-existent chunks
        return;
    }

    for (size_t e = 0; e < chunkdata->closeEntities.size(); e++) {
        EntityType<EC::Position> closeEntity = chunkdata->closeEntities[e].cast<EC::Position>();
        if (callback(closeEntity)) {
            return;
        }
    }
}

void forEachChunkContainingBounds(const ChunkMap* chunkmap, Vec2 position, Vec2 size, std::function<void(ChunkData*)> callback) {
    IVec2 minChunkPosition = toChunkPosition(position - size.scaled(0.5f));
    IVec2 maxChunkPosition = toChunkPosition(position + size.scaled(0.5f));
    for (int col = minChunkPosition.x; col <= maxChunkPosition.x; col++) {
        for (int row = minChunkPosition.y; row <= maxChunkPosition.y; row++) {
            IVec2 chunkPosition = {col, row};
            ChunkData* chunkdata = chunkmap->getChunkData(chunkPosition);
            if (chunkdata) {
                callback(chunkdata);
            }
        }
    }
}

OptionalEntity<EC::Position, EC::Size>
findFirstEntityAtPosition(const EntityWorld& ecs, const ChunkMap* chunkmap, Vec2 position) {
    OptionalEntity<EC::Position, EC::Size> foundEntity;
    forEachEntityNearPoint(ComponentManager<EC::Position, const EC::Render, EC::Size>(&ecs), chunkmap, position, [&](EntityType<EC::Position> entity){
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
    EntityType<EC::Position> closestEntity = NullEntity;
    float closestDistSqrd = INFINITY;
    forEachEntityNearPoint(ecs, chunkmap, position, [&](EntityType<EC::Position> entity){
        Vec2 entityPos = ecs->Get<EC::Position>(entity)->vec2();
        Vec2 delta = entityPos - position;
        float entityDistSqrd = delta.distSqrd();
        if (entityDistSqrd < closestDistSqrd) {
            closestEntity = entity;
            closestDistSqrd = entityDistSqrd;
        }
        return 0;
    });

    return closestEntity;
}

bool pointInsideEntityBounds(Vec2 point, const EntityType<EC::Position, EC::Size> entity, const ComponentManager<const EC::Position, const EC::Size>& ecs) {
    Vec2 entityPosition = entity.Get<const EC::Position>(&ecs)->vec2();
    Vec2 entitySize = entity.Get<const EC::Size>(&ecs)->vec2();

    Vec2 minEntityPos = entityPosition - entitySize.scaled(0.5f);
    Vec2 maxEntityPos = entityPosition + entitySize.scaled(0.5f);

    if (point.x > minEntityPos.x && point.x < maxEntityPos.x &&
        point.y > minEntityPos.y && point.y < maxEntityPos.y) {
        
        return true;
    }
    return false;
}