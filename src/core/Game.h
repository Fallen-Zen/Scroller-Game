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
#include "InputManager.h"
#include <SDL.h>
#include <memory>
#include <string>

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

    // ── Player state ─────────────────────────────────────────────────────────

    // Current position (pixels, top-left of the player rectangle).
    // Y increases downward — (0,0) is the top-left of the screen.
    float m_px = 100.f, m_py = 400.f;

    // Position at the start of the previous physics tick.
    // Stored so render() can linearly interpolate between old and new positions
    // to produce motion that looks smooth at any frame rate.
    float m_ox = 100.f, m_oy = 400.f;

    // Current velocity in pixels per second (signed: positive = right / down).
    float m_vx = 0.f, m_vy = 0.f;

    // True while the player is resting on a solid surface.
    // Jumping is only allowed when on the ground (prevents double-jumping).
    bool m_onGround = false;

    // Last direction the player moved. Persists when they stop so the sprite
    // keeps facing that way instead of snapping back to a default.
    // true = facing right, false = facing left.
    bool m_facingRight = true;

    // ── Debug / observability ─────────────────────────────────────────────────

    // Counts physics ticks since the last in-air log line. Used to throttle
    // airborne debug output to ~10 lines/second instead of 120.
    int m_airTicks = 0;

    // m_vy sign from the previous tick — used to detect the arc peak (the
    // moment vertical velocity crosses from negative/upward to positive/downward).
    float m_prevVy = 0.f;

    // ── Physics constants ─────────────────────────────────────────────────────

    // Downward acceleration in px/s². Earth is ~9.8 m/s²; games typically use
    // much higher values so jumps feel snappy rather than floaty.
    static constexpr float GRAVITY    = 1800.f;

    // Horizontal speed while a direction key is held (px/s).
    static constexpr float MOVE_SPEED = 220.f;

    // Initial vertical velocity applied when jumping. Negative because Y grows
    // downward — a negative vy means moving upward.
    static constexpr float JUMP_VEL   = -600.f;

    // Y coordinate (pixels from top) of the ground surface. The player's top
    // edge is clamped here on landing, giving the illusion of standing on it.
    static constexpr int   FLOOR_Y    = 580;
};
