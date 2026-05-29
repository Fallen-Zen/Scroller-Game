// =============================================================================
// Game.h — Core game class declaration
//
// Game owns the SDL window, renderer, and the main loop. It is the top-level
// object: main() creates one, calls run(), and when run() returns the game is
// over. Everything else (physics, rendering, audio, levels) will eventually be
// owned or coordinated by Game.
// =============================================================================

#pragma once
#include "AssetRegistry.h"
#include "AudioManager.h"
#include "InputManager.h"
#include "entities/EntityManager.h"
#include "world/Camera.h"
#include "world/Tilemap.h"
#include <SDL.h>
#include <memory>
#include <string>

// Forward declaration — Game holds a non-owning Player* for camera tracking.
// EntityManager owns the actual instance via unique_ptr.
class Player;

// -----------------------------------------------------------------------------
// GameConfig — plain data struct for startup settings
//
// Using a dedicated config struct instead of constructor parameters makes it
// easy to add new options (fullscreen, target FPS, audio volume…) without
// changing the constructor signature every time.
// -----------------------------------------------------------------------------
struct GameConfig {
    std::string title  = "Scroller"; // Window title bar text
    int         width  = 1280;       // Logical/physical width in pixels
    int         height = 720;        // Logical/physical height in pixels
    bool        vsync  = true;       // Sync present() to monitor refresh rate
};

// -----------------------------------------------------------------------------
// Game — the single object that owns the SDL context and drives the loop
// -----------------------------------------------------------------------------
class Game {
public:
    // Constructor: initialises SDL, creates the window and hardware renderer.
    // Throws std::runtime_error (wrapping SDL_GetError) if anything fails,
    // so the caller (main) can catch it and show a user-friendly message.
    explicit Game(const GameConfig& cfg = {});

    // Destructor: tears down SDL objects in reverse creation order.
    // Because SDL_Quit() is called here, it's important that no SDL calls
    // happen after this object is destroyed.
    ~Game();

    // Game is non-copyable and non-movable. SDL_Window and SDL_Renderer are raw
    // handles that SDL owns internally. Copying would create two owners that
    // both try to destroy the same resource (double-free). Moving would leave
    // the source with dangled pointers that its destructor would still call
    // SDL_Destroy* on. Neither operation makes sense for a top-level object
    // that owns the entire SDL context for the lifetime of the program.
    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&)                 = delete;
    Game& operator=(Game&&)      = delete;

    // Starts the main loop. Blocks until the player quits.
    // Returns 0 on clean exit, 1 on error (suitable for returning from main).
    int run();

private:
    // ── Main loop helpers ─────────────────────────────────────────────────────

    // Drains the SDL event queue. Called once per frame before physics ticks.
    // Translates raw SDL events into the boolean flags the rest of the class
    // reads (m_left, m_right, m_running, …).
    void processEvents();

    // Advances the simulation by exactly `dt` seconds (the fixed timestep).
    // Called potentially multiple times per frame to "catch up" if rendering
    // was slow. Using a fixed dt makes physics deterministic and reproducible.
    void update(double dt);

    // Draws one frame. `alpha` is a blend factor in [0,1] that represents how
    // far between the last two physics ticks we currently are. Interpolating
    // the render position with alpha produces sub-tick smooth motion even when
    // the physics rate (120 Hz) differs from the display rate (e.g. 60 Hz).
    void render(double alpha);

    // ── SDL handles ──────────────────────────────────────────────────────────

    // SDL_Window represents the OS window (title bar, resize handle, etc.).
    SDL_Window*   m_window   = nullptr;

    // SDL_Renderer is the 2D drawing context attached to the window.
    // It abstracts over Metal (macOS), Direct3D (Windows), and OpenGL.
    SDL_Renderer* m_renderer = nullptr;

    // Loop runs while this is true; set to false to exit cleanly.
    bool m_running = false;

    // Translates raw SDL events into named Actions (Left, Right, Jump, …).
    // Owned by Game so its lifetime matches the SDL context.
    InputManager m_input;

    // Central texture store. Constructed after the renderer is ready.
    // unique_ptr allows deferred construction inside the Game constructor body
    // (member variables initialise before the constructor body runs, but we
    // need a valid SDL_Renderer* before we can build textures).
    std::unique_ptr<AssetRegistry> m_assets;

    // Owns all loaded sound effects and the SDL_mixer device. Constructed after
    // SDL_Init (which starts the audio subsystem) and before entities spawn so
    // they can receive a reference at construction time.
    std::unique_ptr<AudioManager> m_audio;

    // The game world as a tile grid. 200×45 tiles = 3200×720 px — wider than
    // the viewport so the camera system (next step) has room to scroll.
    // Initialised with the first room layout in the Game constructor.
    Tilemap m_tilemap { 200, 45 };

    // Viewport camera. Constructed after m_tilemap so it can query world size.
    // viewW=1280, viewH=720, worldW/H from the tilemap dimensions.
    Camera m_camera { 1280, 720, 200 * Tilemap::TILE_SIZE, 45 * Tilemap::TILE_SIZE };

    // ── Entity system ─────────────────────────────────────────────────────────

    // Owns all active game entities (Player, enemies, projectiles…).
    // Drives saveOldPosition → update → render for each one every tick.
    EntityManager m_entities;

    // Non-owning pointer to the player entity inside m_entities.
    // Used by update() to feed the player's world position to the camera each
    // tick. EntityManager owns the actual instance via unique_ptr.
    Player* m_player = nullptr;
};
