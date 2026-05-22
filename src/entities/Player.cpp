// =============================================================================
// Player.cpp
// =============================================================================

#include "entities/Player.h"
#include "core/AssetRegistry.h"
#include "core/InputManager.h"
#include "core/Logger.h"
#include "world/Camera.h"
#include "world/Tilemap.h"
#include <SDL.h>

// =============================================================================
// Construction
// =============================================================================

Player::Player(float startX, float startY, const InputManager& input)
    : Entity(startX, startY, W, H)
    , m_input(input)
{}

// =============================================================================
// Update — fixed-timestep physics tick
//
// Order of operations (Y-first to prevent floor tile appearing as a wall):
//   1. Read input → set m_vx, consume the jump button.
//   2. Apply gravity to m_vy (semi-implicit Euler).
//   3. Integrate Y, then resolveY() — m_py is now correct.
//   4. Integrate X, then resolveX() — m_px is now correct.
//   5. Emit debug logs.
// =============================================================================

void Player::update(const Tilemap& tilemap, double dt) {
    float fdt = static_cast<float>(dt);

    // ── Input → horizontal velocity ───────────────────────────────────────────
    // Reset to zero each tick — no momentum/friction yet. The player stops
    // instantly when no directional key is held ("digital" movement).
    m_vx = 0.f;
    if (m_input.isHeld(Action::Left))  m_vx -= MOVE_SPEED;
    if (m_input.isHeld(Action::Right)) m_vx += MOVE_SPEED;

    // Track facing direction. Only updated while actually moving so the sprite
    // holds its last direction when the player stops.
    if (m_vx > 0.f) m_facingRight = true;
    if (m_vx < 0.f) m_facingRight = false;

    // ── Jump ─────────────────────────────────────────────────────────────────
    // isPressed fires exactly once on the first tick the button is down, so
    // the impulse is applied only once per press even if held.
    if (m_input.isPressed(Action::Jump) && m_onGround) {
        m_vy       = JUMP_VEL;
        m_onGround = false;
        m_airTicks = 0;
        LOG_DEBUG("Jump  | pos=(%.0f, %.0f)  vy=%.0f", m_px, m_py, m_vy);
    }

    // ── Gravity (semi-implicit Euler) ─────────────────────────────────────────
    // Accumulate velocity first, then integrate position below. Semi-implicit
    // Euler is more energy-stable than classic Euler for spring-like forces,
    // preventing the player from slowly gaining height over many jumps.
    m_prevVy  = m_vy;
    m_vy     += GRAVITY * fdt;

    // ── Integrate Y → resolve vertical collisions ────────────────────────────
    m_py += m_vy * fdt;
    resolveY(tilemap);   // snaps m_py, updates m_onGround

    // ── Integrate X → resolve horizontal collisions ──────────────────────────
    // Y is already resolved, so the floor tile is no longer in the horizontal
    // scan range — no corner-sticking at tile edges.
    m_px += m_vx * fdt;
    resolveX(tilemap);   // snaps m_px

    // ── Debug: arc peak ───────────────────────────────────────────────────────
    // The peak is the tick where m_vy crosses from negative (upward) to
    // positive (downward). Log it once for easy jump-height inspection.
    if (!m_onGround && m_prevVy < 0.f && m_vy >= 0.f)
        LOG_DEBUG("Peak  | pos=(%.0f, %.0f)  vy=%.0f→%.0f",
                  m_px, m_py, m_prevVy, m_vy);

    // ── Debug: airborne throttled log ─────────────────────────────────────────
    // 120 ticks/s is too noisy. Log every 12 ticks (~10 lines/second) instead.
    if (!m_onGround) {
        ++m_airTicks;
        if (m_airTicks % 12 == 0)
            LOG_DEBUG("Air   | pos=(%.0f, %.0f)  vy=%+.1f", m_px, m_py, m_vy);
    }
}

// =============================================================================
// Render — draw the player at the interpolated position
//
// alpha blends between (m_ox, m_oy) — the position at the start of the last
// physics tick — and (m_px, m_py) — the position at the end of it. This
// produces sub-tick smooth motion at any frame rate.
// =============================================================================

void Player::render(SDL_Renderer* renderer,
                    const AssetRegistry& assets,
                    const Camera& camera,
                    double alpha) const
{
    // Interpolate world position for this render frame.
    float rx = static_cast<float>(m_ox + (m_px - m_ox) * alpha);
    float ry = static_cast<float>(m_oy + (m_py - m_oy) * alpha);

    // Convert world → screen using the camera's interpolated offset.
    int sx = camera.toScreenX(rx, alpha);
    int sy = camera.toScreenY(ry, alpha);

    // ── Body ──────────────────────────────────────────────────────────────────
    SDL_Rect body { sx, sy, W, H };
    SDL_RenderCopy(renderer, assets.get(TextureID::Player), nullptr, &body);

    // ── Eye (facing-direction indicator) ─────────────────────────────────────
    // The eye sits in the upper-right quadrant when facing right, upper-left
    // when facing left. Offset values are tuned to the procedural sprite.
    SDL_Rect eye {
        sx + (m_facingRight ? 20 : 4),
        sy + 10,
        8, 8
    };
    SDL_RenderCopy(renderer, assets.get(TextureID::PlayerEye), nullptr, &eye);
}
