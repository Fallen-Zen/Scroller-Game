// =============================================================================
// Game.cpp — Core game class implementation
// =============================================================================

#include "core/Game.h"
#include "core/AssetRegistry.h"
#include "core/Logger.h"
#include <SDL.h>
#include <stdexcept>   // std::runtime_error
#include <algorithm>   // std::min, std::clamp
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
        while (accumulator >= FIXED_DT) {
            // Save the current position as "old" before advancing physics.
            // render() will interpolate between (m_ox, m_oy) and (m_px, m_py).
            m_ox = m_px;
            m_oy = m_py;
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
// A "physics update" moves the simulation one step forward in time. Because dt
// is always the same value (FIXED_DT), the maths is stable and reproducible.
//
// The model here is deliberately simple (no friction, instant direction change)
// and is a placeholder for a proper character controller later.
// =============================================================================

void Game::update(double dt) {
    // chrono uses double for precision, but positions are stored as float.
    // We cast once here so every calculation below uses float arithmetic.
    float fdt = static_cast<float>(dt);

    // ── Horizontal velocity ───────────────────────────────────────────────────
    // Reset to zero each tick so the player stops instantly when no key is held.
    // This is "digital" movement — no acceleration/deceleration ramp. It feels
    // responsive but snappy. A proper character controller would add friction.
    m_vx = 0.f;
    if (m_input.isHeld(Action::Left))  m_vx -= MOVE_SPEED;
    if (m_input.isHeld(Action::Right)) m_vx += MOVE_SPEED;

    // Only update facing when actually moving — this way the sprite holds its
    // last direction when the player stops instead of defaulting to right.
    if (m_vx > 0.f) m_facingRight = true;
    if (m_vx < 0.f) m_facingRight = false;

    // ── Jump ─────────────────────────────────────────────────────────────────
    // isPressed fires only on the first frame the button is down, so the jump
    // impulse is applied exactly once per press even if the button is held.
    if (m_input.isPressed(Action::Jump) && m_onGround) {
        m_vy       = JUMP_VEL;
        m_onGround = false;
        m_airTicks = 0;
        LOG_DEBUG("Jump  | pos=(%.0f, %.0f)  vy=%.0f", m_px, m_py, m_vy);
    }

    // Pause / quit via the Pause action (Escape or Start button).
    if (m_input.isPressed(Action::Pause))
        m_running = false;

    // ── Gravity ───────────────────────────────────────────────────────────────
    // Gravity is a constant downward acceleration. Each tick we increase the
    // vertical velocity by (GRAVITY × dt). Because Y grows downward, adding a
    // positive value makes the player fall.
    // Semi-implicit Euler: apply acceleration to velocity, then velocity to
    // position. This is more stable than "classic" Euler for spring-like forces.
    m_prevVy  = m_vy;
    m_vy     += GRAVITY * fdt;

    // ── Integrate position ───────────────────────────────────────────────────
    // position += velocity × time  (the fundamental kinematic equation)
    m_px += m_vx * fdt;
    m_py += m_vy * fdt;

    // ── Arc peak detection ────────────────────────────────────────────────────
    // The peak is the tick where m_vy crosses from negative (rising) to
    // positive (falling). We compare the sign before and after gravity was
    // applied this tick.
    if (!m_onGround && m_prevVy < 0.f && m_vy >= 0.f)
        LOG_DEBUG("Peak  | pos=(%.0f, %.0f)  vy=%.0f→%.0f", m_px, m_py, m_prevVy, m_vy);

    // ── In-air throttled log ──────────────────────────────────────────────────
    // Log position and velocity while airborne, but only every 12 ticks
    // (~10 times/sec) to keep the output readable.
    if (!m_onGround) {
        ++m_airTicks;
        if (m_airTicks % 12 == 0)
            LOG_DEBUG("Air   | pos=(%.0f, %.0f)  vy=%+.1f", m_px, m_py, m_vy);
    }

    // ── Floor collision ───────────────────────────────────────────────────────
    // If the player's top-left Y has passed the floor Y, push them back up and
    // zero vertical velocity so they don't keep accelerating underground.
    // m_onGround lets the jump check above know a jump is allowed.
    if (m_py >= static_cast<float>(FLOOR_Y)) {
        if (!m_onGround)
            LOG_DEBUG("Land  | pos=(%.0f, %.0f)  impact_vy=%.0f  airTicks=%d",
                      m_px, m_py, m_vy, m_airTicks);
        m_py       = static_cast<float>(FLOOR_Y);
        m_vy       = 0.f;
        m_onGround = true;
        m_airTicks = 0;
    } else {
        // Once airborne, mark as off-ground so jump can't be retriggered until
        // the next landing. (This flag was already false after the jump impulse,
        // but the else keeps it correct if somehow m_py rises above FLOOR_Y.)
        m_onGround = false;
    }

    // ── Horizontal boundary ───────────────────────────────────────────────────
    // Prevent the player leaving the left or right edge of the 1280 px canvas.
    // 1280 - 32 = 1248: right boundary accounts for the player's 32 px width
    // so the whole rectangle stays on screen.
    m_px = std::clamp(m_px, 0.f, 1248.f);
}


// =============================================================================
// Rendering  —  called once per frame with an interpolation factor
//
// Rendering is intentionally separated from physics. The renderer never modifies
// game state — it only reads it. This separation is what allows us to render at
// any frame rate while physics ticks at a fixed rate.
// =============================================================================

void Game::render(double alpha) {
    // ── Interpolated draw position ────────────────────────────────────────────
    // alpha blends between the position at the start of the last tick (m_ox/oy)
    // and the position at the end of it (m_px/py). The result is where the
    // player "should" visually be right now, between two physics samples.
    //
    // Formula: lerp(a, b, t) = a + (b - a) * t
    //
    // Without interpolation, fast-moving objects jitter because the physics
    // ticks (120 Hz) don't align with display refreshes (60 Hz). With it, the
    // drawn position updates smoothly every frame.
    float rx = static_cast<float>(m_ox + (m_px - m_ox) * alpha);
    float ry = static_cast<float>(m_oy + (m_py - m_oy) * alpha);

    // ── Background ────────────────────────────────────────────────────────────
    // SDL_SetRenderDrawColor sets the RGBA colour for all subsequent draw calls
    // until changed. Colours are 0–255. This dark-blue clears the whole screen.
    SDL_SetRenderDrawColor(m_renderer, 18, 18, 30, 255);
    // SDL_RenderClear fills the entire render target with the current draw
    // colour, erasing whatever was drawn in the previous frame.
    SDL_RenderClear(m_renderer);

    // ── Floor tiles ───────────────────────────────────────────────────────────
    // Tile the ground texture (16×16) across the bottom of the screen.
    // Each tile is drawn individually so the tilemap system (Layer 2) can
    // replace this loop with a proper grid lookup later.
    SDL_Texture* groundTex = m_assets->get(TextureID::TileGround);
    constexpr int TILE_SIZE = 16;
    // Number of tile rows to fill from FLOOR_Y down to the bottom of screen.
    constexpr int FLOOR_ROWS = (720 - FLOOR_Y) / TILE_SIZE + 1;
    constexpr int TILE_COLS  = 1280 / TILE_SIZE;

    for (int row = 0; row < FLOOR_ROWS; ++row) {
        for (int col = 0; col < TILE_COLS; ++col) {
            SDL_Rect dst {
                col * TILE_SIZE,
                FLOOR_Y + row * TILE_SIZE,
                TILE_SIZE, TILE_SIZE
            };
            SDL_RenderCopy(m_renderer, groundTex, nullptr, &dst);
        }
    }

    // ── Player body ───────────────────────────────────────────────────────────
    // SDL_RenderCopy draws a texture stretched to fill the destination rect.
    // src=nullptr means "use the full texture". The texture was generated by
    // AssetRegistry::makePlayer() at startup — no file loading required.
    SDL_Rect playerDst {
        static_cast<int>(rx),
        static_cast<int>(ry),
        32, 48
    };
    SDL_RenderCopy(m_renderer, m_assets->get(TextureID::Player), nullptr, &playerDst);

    // ── Facing indicator ("eye") ─────────────────────────────────────────────
    // Drawn as a separate texture on top of the body so it can later be
    // replaced with an animated eye or directional indicator independently.
    SDL_Rect eyeDst {
        static_cast<int>(rx) + (m_facingRight ? 20 : 4),
        static_cast<int>(ry) + 10,
        8, 8
    };
    SDL_RenderCopy(m_renderer, m_assets->get(TextureID::PlayerEye), nullptr, &eyeDst);

    // ── Present ───────────────────────────────────────────────────────────────
    // Everything drawn above went to an off-screen back buffer. SDL_RenderPresent
    // swaps back and front buffers so the finished frame becomes visible on
    // screen. With vsync enabled this call also blocks until the monitor's next
    // refresh, which is what caps the frame rate and prevents tearing.
    SDL_RenderPresent(m_renderer);
}
