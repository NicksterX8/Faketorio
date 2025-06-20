#ifndef UPDATE_INCLUDED
#define UPDATE_INCLUDED

#include <vector>
#include <variant>
#include <random>
#include <SDL3/SDL.h>
#include <math.h>

#include "constants.hpp"
#include "rendering/textures.hpp"
#include "GameState.hpp"
#include "PlayerControls.hpp"
#include "GUI/Gui.hpp"
#include "sdl.hpp"
#include "Camera.hpp"
#include "rendering/rendering.hpp"
#include "ECS/system.hpp"

struct Game;

void rotateEntity(const EntityWorld& ecs, Entity entity, bool counterClockwise);

void setDefaultKeyBindings(Game& ctx, PlayerControls* controls);

struct CameraEntityFocus {
    Entity entity;
    bool rotateWithEntity;
};

struct CameraFocus {
    std::variant<
        CameraEntityFocus, // follow entity
        Vec2 // static point
    > focus;
};

namespace World {
    namespace RenderSystems {
        struct RenderEntitySystem;
    }
}

struct GameEntitySystems {
    ECS::System::SystemManager ecsRenderSystems;
    ECS::System::SystemManager ecsStateSystems;

    World::RenderSystems::RenderEntitySystem* renderEntitySys;
};

struct Game {
    SDLContext sdlCtx;
    GameState* state;
    Camera camera;
    CameraFocus cameraFocus;
    Gui* gui;
    PlayerControls* playerControls;
    MouseState lastUpdateMouseState;
    Vec2 lastUpdatePlayerTargetPos;
    DebugClass* debug;
    MetadataTracker metadata;
    RenderContext* renderContext;
    GameEntitySystems systems;
    Mode mode;

    Game(SDLContext sdlContext):
    sdlCtx(sdlContext), metadata(TARGET_FPS, TICKS_PER_SECOND, ENABLE_VSYNC) {
        Metadata = &metadata;

        debug = new DebugClass();
        Debug = debug;
        debug->debugging = DEBUG;

        state = NULL;
        gui = NULL;
        playerControls = NULL;
        renderContext = NULL;
        mode = Unstarted;
    }

    int init();

    int start();

    void quit();

    void destroy();

    int update();

    // @return 1 for quit game, 0 otherwise
    int handleEvent(const SDL_Event*);

#ifdef EMSCRIPTEN
    // update wrapper function to unwrap the void pointer main loop parameter into its properties
    static int emscriptenUpdateWrapper(void* param) {
        Game* game = static_cast<Game*>(param);
        if (game) {
            return game->update();
        } else {
            // handle error
            LogError("Parameter passed to emscripten update wrapper was null!");
            return -1;
        }
        
    }
#endif

    void setMode(Mode mode) {
        this->mode = mode;
    }

};

#endif