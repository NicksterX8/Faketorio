#ifndef TILES_INCLUDED
#define TILES_INCLUDED

#include "constants.hpp"
#include "NC/colors.h"
#include "Textures.hpp"
#include <SDL2/SDL.h>
#include "Items.hpp"
#include "ECS/EntityType.hpp"

extern float TilePixels;
extern float TileWidth;
extern float TileHeight;

using ECS::OptionalEntity;

namespace TileTypes {
    enum E: Uint16 {
        Empty = 0,
        Space = 1,
        Grass,
        Sand,
        Water,
        SpaceFloor,
        Wall
    };

    enum Flags: Uint32 {
        Walkable = 1,
        Mineable = 2
    };

    struct MineableTile {
        Item item; // item given by being mined (or 0 for nothing)
    };
}
typedef Uint16 TileType;
typedef TileTypes::Flags TileTypeFlag;

#define NUM_TILE_TYPES 7

struct TileTypeDataStruct {
    Color color;
    SDL_Texture* background;
    Uint32 flags;

    TileTypes::MineableTile mineable;
};

extern TileTypeDataStruct TileTypeData[NUM_TILE_TYPES];

struct Tile {
    TileType type;
    OptionalEntity<> entity;

    Tile();
    Tile(TileType type);
};

#endif