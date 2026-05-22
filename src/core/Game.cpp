// =============================================================================
// Game.cpp — Core game class implementation
// =============================================================================

#include "core/Game.h"
#include "core/AssetRegistry.h"
#include "core/Logger.h"
#include "entities/Player.h"
#include "world/Camera.h"
#include "world/Tilemap.h"
#include <SDL.h>
#include <stdexcept>   // std::runtime_error
#include <algorithm>   // std::min
#include <chrono>      // high-resolution wall clock

// std::chrono::steady_clock is a monotonic clock — it never goes backward and
// is not affected by the user adjusting the system time. That makes it the
// right choice for measuring frame durations.
using Clock = std::chrono::steady_clock;

// Physics are stepped at a fixed rate of 120 ticks per second (every ~8.33 ms).
// A fixed timestep keeps physics deterministic: given the same inputs the
// simulation always produces the same result, regardless of frame rate.
// 120 Hz is double a typical 60 Hz display, which gives the interpolation in
// render() enough resolution to feel smooth at any refresh rate.
static constexpr double FIXED_DT = 1.0 / 120.0;


// =============================================================================
// Construction / Destruction
// =============================================================================

Game::Game(const GameConfig& cfg) {
    Logger::init();
    LOG_INFO("Starting %s v%s", cfg.title.c_str(), "0.1.0");

    // SDL_Init tells SDL which subsystems to start.
    //   SDL_INIT_VIDEO          — window, renderer, keyboard, mouse
    //   SDL_INIT_AUDIO          — sound mixing (needed for SDL_mixer later)
    //   SDL_INIT_GAMECONTROLLER — gamepad support via HID
    // Returns non-zero on failure; SDL_GetError() explains why.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
    LOG_DEBUG("SDL subsystems initialised (video, audio, gamecontroller)");

    // SDL_WINDOW_SHOWN    — display the window immediately on creation
    // SDL_WINDOW_RESIZABLE — let the user drag the window edges; the logical
    //                        size set below keeps the game resolution fixed.
    Uint32 winFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    m_window = SDL_CreateWindow(
        cfg.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // centre on screen
        cfg.width, cfg.height,
        winFlags
    );
    if (!m_window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }
    LOG_INFO("Window created: %s (%dx%d)", cfg.title.c_str(), cfg.width, cfg.height);

    // SDL_RENDERER_ACCELERATED — use the GPU (Metal / D3D / OpenGL) instead of
    //                            software rendering. Dramatically faster for
    //                            anything beyond trivial drawing.
    // SDL_RENDERER_PRESENTVSYNC — SDL_RenderPresent() blocks until the next
    //                             monitor refresh, capping the frame rate and
    //                             preventing screen tearing. Can be disabled for
    //                             uncapped rendering (e.g. benchmarking).
    Uint32 rendFlags = SDL_RENDERER_ACCELERATED;
    if (cfg.vsync) rendFlags |= SDL_RENDERER_PRESENTVSYNC;

    // -1 tells SDL to pick the first available rendering driver (usually Metal
    // on macOS, Direct3D 11 on Windows). Pass an index to pick a specific one.
    m_renderer = SDL_CreateRenderer(m_window, -1, rendFlags);
    if (!m_renderer) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        throw std::runtime_error(SDL_GetError());
    }

    // Log which renderer SDL picked so it's easy to verify Metal/D3D is active.
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(m_renderer, &info) == 0) {
        LOG_INFO("Renderer: %s (%s, vsync=%s)",
            info.name,
            (info.flags & SDL_RENDERER_ACCELERATED) ? "accelerated" : "software",
            (info.flags & SDL_RENDERER_PRESENTVSYNC) ? "on" : "off"
        );
    }

    // Logical size decouples the game's coordinate system from the physical
    // window size. All draw calls use (cfg.width × cfg.height) coordinates;
    // SDL scales and letter-boxes automatically when the window is resized.
    // This means the game always "thinks" it is 1280×720 even on a 4K display.
    SDL_RenderSetLogicalSize(m_renderer, cfg.width, cfg.height);
    LOG_DEBUG("Logical resolution locked to %dx%d", cfg.width, cfg.height);

    // Build all procedural textures. Must come after the renderer is ready.
    m_assets = std::make_unique<AssetRegistry>(m_renderer);

    // ── Room 1 layout ─────────────────────────────────────────────────────────
    // The map is 200×45 tiles (3200×720 px). We paint it with fill() calls
    // that work like a "stamp" — each call fills a rectangle of tiles.
    // Coordinates are (col, row, width, height) in tile units.

    // Ceiling — one tile row across the full width
    m_tilemap.fill(0,   0,  200, 1, TileType::Ground);

    // Left and right boundary walls
    m_tilemap.fill(0,   0,  1,  45, TileType::Ground);
    m_tilemap.fill(199, 0,  1,  45, TileType::Ground);

    // Ground — 9 rows from row 36 to the bottom (36 * 16 = 576px = FLOOR_Y)
    m_tilemap.fill(0,  36, 200,  9, TileType::Ground);

    // Platform A — visible immediately left of player start, one jump high
    // Row 30 = y=480px, cols 10-22 = x=160-352px
    m_tilemap.fill(10, 30, 13,  1, TileType::Platform);

    // Platform B — higher up, reachable from platform A
    // Row 24 = y=384px, cols 25-40 = x=400-640px
    m_tilemap.fill(25, 24, 16,  1, TileType::Platform);

    // Platform C — step back down on the right side
    // Row 29 = y=464px, cols 50-65 = x=800-1040px
    m_tilemap.fill(50, 29, 16,  1, TileType::Platform);

    // Platform D — off-screen right, visible once camera is added
    // Row 24 = y=384px, cols 80-95 = x=1280-1520px
    m_tilemap.fill(80, 24, 16,  1, TileType::Platform);

    // Raised ledge — a thick block to jump onto, off-screen right
    // cols 110-125, rows 30-35 = a 16-tile-tall solid pillar
    m_tilemap.fill(110, 30, 15, 6, TileType::Ground);

    LOG_INFO("Room 1 layout built (%dx%d tiles)", m_tilemap.cols(), m_tilemap.rows());

    // ── Spawn entities ────────────────────────────────────────────────────────
    // Create the player, hand ownership to EntityManager, and keep a raw pointer
    // for camera tracking. The raw pointer is safe because EntityManager outlives
    // any individual update/render call and we never store it beyond Game's scope.
    auto player = std::make_unique<Player>(100.f, 400.f, m_input);
    m_player    = player.get();
    m_entities.add(std::move(player));
    LOG_INFO("Entities spawned: %zu", m_entities.count());
}

Game::~Game() {
    // SDL objects must be destroyed in reverse creation order. Destroying the
    // renderer before the window is required — the renderer holds references
    // to GPU resources tied to the window's surface.
    LOG_INFO("Shutting down");
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window)   SDL_DestroyWindow(m_window);

    // SDL_Quit shuts down every subsystem started by SDL_Init and releases
    // all internal SDL resources. Nothing SDL-related should be called after
    // this point.
    SDL_Quit();
    Logger::shutdown();
}


// =============================================================================
// Main loop  —  the "fixed timestep with interpolation" pattern
//
// The loop solves a classic problem: physics must run at a stable rate to stay
// deterministic, but rendering runs at whatever rate the GPU/vsync allows.
//
// Solution (Glenn Fiedler's "Fix Your Timestep"):
//   1. Measure wall-clock time elapsed since the last frame (elapsed).
//   2. Add it to an accumulator.
//   3. Consume the accumulator in fixed-size chunks (FIXED_DT), running one
//      physics tick per chunk. This can be 0, 1, or several ticks per frame.
//   4. Whatever is left in the accumulator is a fractional tick. Use it as a
//      blend factor (alpha) to interpolate the render position between the
//      previous and current physics state, producing visually smooth motion.
// =============================================================================

int Game::run() {
    m_running = true;
    LOG_INFO("Main loop started (fixed timestep: %.0f Hz)", 1.0 / FIXED_DT);

    auto   previous    = Clock::now(); // wall-clock time at the last frame
    double accumulator = 0.0;          // un-simulated time carried between frames

    while (m_running) {
        auto   current = Clock::now();
        // duration<double> converts the tick difference to fractional seconds.
        double elapsed = std::chrono::duration<double>(current - previous).count();
        previous = current;

        // Cap elapsed to 250 ms. If the game was paused in a debugger or the OS
        // suspended it, elapsed could be seconds. Without the cap the physics
        // loop would run hundreds of ticks trying to "catch up", making the
        // game freeze for several seconds — the "spiral of death". Capping it
        // means the game simply runs in slow-motion for that one frame instead.
        accumulator += std::min(elapsed, 0.25);

        // Poll OS events first so input flags are up to date before physics runs.
        processEvents();

        // Run as many fixed physics ticks as the elapsed time demands.
        // EntityManager::update() saves each entity's old position internally
        // before calling update(), so we no longer need to do it here.
        while (accumulator >= FIXED_DT) {
            update(FIXED_DT);
            accumulator -= FIXED_DT;
        }

        // alpha is in [0, 1): the fraction of a tick still in the accumulator.
        // 0.0 = we are exactly on a tick boundary (rare).
        // 0.99 = we are almost at the next tick.
        // Passing it to render() lets it draw the player at a fractional
        // position between the last two ticks, which looks smooth.
        double alpha = accumulator / FIXED_DT;
        render(alpha);

        // Clear one-frame edge signals (justPressed / justReleased) so they
        // don't carry over and fire again on the next frame.
        m_input.endFrame();
    }

    return 0;
}


// =============================================================================
// Event handling
//
// SDL collects OS events (keyboard, mouse, window resize, quit signal, …) in
// an internal queue. SDL_PollEvent() removes and returns one event at a time,
// returning 0 when the queue is empty.
//
// Rather than acting on events directly inside update(), we translate them into
// simple boolean flags (m_left, m_right, m_running). This keeps input handling
// decoupled from physics — update() only reads flags, not SDL types.
// =============================================================================

void Game::processEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        // Let InputManager inspect every event first so it can update its
        // keyboard and gamepad state before we read it in update().
        m_input.handleEvent(e);

        // Game-level events that aren't Actions (window lifecycle, quit signal)
        // are still handled directly here.
        switch (e.type) {

        // SDL_QUIT is sent when the user clicks the window's × button or the
        // OS asks the application to terminate (e.g. Cmd+Q on macOS).
        case SDL_QUIT:
            m_running = false;
            break;

        // SDL_WINDOWEVENT bundles several sub-events (resize, focus, close…).
        // SDL_WINDOWEVENT_CLOSE fires when the window's × is clicked on some
        // platforms in addition to (or instead of) SDL_QUIT.
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE)
                m_running = false;
            break;

        default:
            break;
        }
    }
}


// =============================================================================
// Physics update  —  called with a fixed dt every tick
//
// Game::update() is now thin: it handles game-level actions (pause/quit),
// delegates all entity physics to EntityManager, then positions the camera.
// All movement, gravity, and collision logic lives in Player::update() and
// Entity::resolveX/Y().
// =============================================================================

void Game::update(double dt) {
    // Game-level input: Pause / quit. Checked here rather than in Player so
    // the game can exit cleanly regardless of which entity currently has focus.
    if (m_input.isPressed(Action::Pause))
        m_running = false;

    // Tick all entities. EntityManager saves each entity's old position first
    // (for render interpolation) then calls entity->update(tilemap, dt).
    m_entities.update(m_tilemap, dt);

    // Keep the camera centred on the player. centreX/Y return the world-space
    // midpoint of the player's AABB — smoother to follow than the top-left.
    if (m_player)
        m_camera.update(m_player->centreX(), m_player->centreY(), dt);
}


// =============================================================================
// Rendering  —  called once per frame with an interpolation factor
//
// Rendering is intentionally separated from physics. The renderer never modifies
// game state — it only reads it. This separation allows rendering at any frame
// rate while physics ticks at a fixed rate.
// =============================================================================

void Game::render(double alpha) {
    // ── Background ────────────────────────────────────────────────────────────
    SDL_SetRenderDrawColor(m_renderer, 18, 18, 30, 255);
    SDL_RenderClear(m_renderer);

    // ── Tilemap ───────────────────────────────────────────────────────────────
    // Pass the interpolated camera offset so only visible tiles are drawn.
    int camX = m_camera.screenOffsetX(alpha);
    int camY = m_camera.screenOffsetY(alpha);
    m_tilemap.render(m_renderer, *m_assets, camX, camY, 1280, 720);

    // ── Entities ──────────────────────────────────────────────────────────────
    // Each entity handles its own interpolation and screen-space conversion
    // using the camera. Game::render() no longer needs to know about the player
    // sprite directly — that responsibility belongs to Player::render().
    m_entities.render(m_renderer, *m_assets, m_camera, alpha);

    // ── Present ───────────────────────────────────────────────────────────────
    // Everything drawn above went to an off-screen back buffer. SDL_RenderPresent
    // swaps back and front buffers so the finished frame becomes visible on
    // screen. With vsync enabled this call also blocks until the monitor's next
    // refresh, which is what caps the frame rate and prevents tearing.
    SDL_RenderPresent(m_renderer);
}
