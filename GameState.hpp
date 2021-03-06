#ifndef GAME_STATE_INCLUDED
#define GAME_STATE_INCLUDED

#include <unordered_map>
#include <functional>
#include <array>
#include <vector>

#include <SDL2/SDL.h>
#include "constants.hpp"
#include "Textures.hpp"
#include "NC/cpp-vectors.hpp"
#include "NC/utils.h"

#include "Tiles.hpp"
#include "Chunks.hpp"
#include "Player.hpp"
#include "ECS/ECS.hpp"
#include "GameViewport.hpp"

struct GameState {
    ChunkMap chunkmap;
    Player player;
    EntityWorld ecs;

    ~GameState();

    void free();
    void init(SDL_Renderer*, GameViewport*);
};

/*
* Get a line of tile coordinates from start to end, using DDA.
* Returns a line of size lineSize that must be freed using free()
* @param lineSize a pointer to an int to be filled in the with size of the line.
*/
IVec2* worldLine(Vec2 start, Vec2 end, int* lineSize);

void worldLineAlgorithm(Vec2 start, Vec2 end, std::function<int(IVec2)> callback);

bool pointIsOnTileEntity(const EntityWorld* ecs, Entity tileEntity, IVec2 tilePosition, Vec2 point);

OptionalEntity<> findTileEntityAtPosition(const GameState* state, Vec2 position);

/*
* Remove (or destroy) the entity on the tile, if it exists.
* Probably shouldn't use this tbh, not a good function.
* @return A boolean representing whether the entity was destroyed.
*/
bool removeEntityOnTile(EntityWorld* ecs, Tile* tile);

bool placeEntityOnTile(EntityWorld* ecs, Tile* tile, Entity entity);

void forEachEntityInRange(const EntityWorld* ecs, const ChunkMap* chunkmap, Vec2 pos, float radius, std::function<int(EntityType<EC::Position>)> callback);

void forEachEntityNearPoint(const ComponentManager<EC::Position, const EC::Size>& ecs, const ChunkMap* chunkmap, Vec2 point, std::function<int(EntityType<EC::Position>)> callback);

void forEachChunkContainingBounds(const ChunkMap* chunkmap, Vec2 position, Vec2 size, std::function<void(ChunkData*)> callback);

OptionalEntity<EC::Position, EC::Size>
findFirstEntityAtPosition(const EntityWorld* ecs, const ChunkMap* chunkmap, Vec2 position);

OptionalEntity<EC::Position>
findClosestEntityToPosition(const EntityWorld* ecs, const ChunkMap* chunkmap, Vec2 position);

bool pointInsideEntityBounds(Vec2 point, EntityType<EC::Position, EC::Size> entity, const ComponentManager<const EC::Position, const EC::Size>& ecs);

#endif