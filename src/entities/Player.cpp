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
    : Entity(startX, startY, WIDTH, HEIGHT)
    , m_input(input)
{}

// =============================================================================
// Update — fixed-timestep physics tick
//
// Order of operations (Y-first to prevent floor tile appearing as a wall):
//   1. Read input → set m_velX, consume the jump button.
//   2. Apply gravity to m_velY (semi-implicit Euler).
//   3. Integrate Y, then resolveY() — m_posY is now correct.
//   4. Integrate X, then resolveX() — m_posX is now correct.
//   5. Emit debug logs.
// =============================================================================

void Player::update(const Tilemap& tilemap, double dt) {
    float fdt = static_cast<float>(dt);

    // ── Input → horizontal velocity ───────────────────────────────────────────
    // Reset to zero each tick — no momentum/friction yet. The player stops
    // instantly when no directional key is held ("digital" movement).
    this->m_velX = 0.f;
    if (this->m_input.isHeld(Action::Left))  this->m_velX -= MOVE_SPEED;
    if (this->m_input.isHeld(Action::Right)) this->m_velX += MOVE_SPEED;

    // Track facing direction. Only updated while actually moving so the sprite
    // holds its last direction when the player stops.
    if (this->m_velX > 0.f) this->m_facingRight = true;
    if (this->m_velX < 0.f) this->m_facingRight = false;

    // ── Jump ─────────────────────────────────────────────────────────────────
    // isPressed fires exactly once on the first tick the button is down, so
    // the impulse is applied only once per press even if held.
    if (this->m_input.isPressed(Action::Jump) && this->m_onGround) {
        this->m_velY     = JUMP_VEL;
        this->m_onGround = false;
        this->m_airTicks = 0;
        LOG_DEBUG("Jump  | pos=(%.0f, %.0f)  velY=%.0f", this->m_posX, this->m_posY, this->m_velY);
    }

    // ── Attack ────────────────────────────────────────────────────────────────
    // isPressed ensures the swing triggers once per button press, not every tick.
    // The !m_isAttacking guard prevents restarting a swing mid-animation.
    if (this->m_input.isPressed(Action::Attack) && !this->m_isAttacking) {
        this->m_isAttacking = true;
        this->m_attackTicks = ATTACK_DURATION;
    }

    // Count down the active attack. When the timer expires, clear the flag so
    // the hitbox stops being active and a new attack can be started.
    if (this->m_isAttacking) {
        this->m_attackTicks--;
        if (this->m_attackTicks == 0) {
            this->m_isAttacking = false;
        }
    }

    // ── Gravity (semi-implicit Euler) ─────────────────────────────────────────
    // Accumulate velocity first, then integrate position below. Semi-implicit
    // Euler is more energy-stable than classic Euler for spring-like forces,
    // preventing the player from slowly gaining height over many jumps.
    this->m_prevVelY  = this->m_velY;
    this->m_velY     += GRAVITY * fdt;

    // ── Integrate Y → resolve vertical collisions ────────────────────────────
    this->m_posY += this->m_velY * fdt;
    this->resolveY(tilemap);   // snaps m_posY, updates m_onGround

    // ── Integrate X → resolve horizontal collisions ──────────────────────────
    // Y is already resolved, so the floor tile is no longer in the horizontal
    // scan range — no corner-sticking at tile edges.
    this->m_posX += this->m_velX * fdt;
    this->resolveX(tilemap);   // snaps m_posX

    // ── Debug: arc peak ───────────────────────────────────────────────────────
    // The peak is the tick where m_velY crosses from negative (upward) to
    // positive (downward). Log it once for easy jump-height inspection.
    if (!this->m_onGround && this->m_prevVelY < 0.f && this->m_velY >= 0.f)
        LOG_DEBUG("Peak  | pos=(%.0f, %.0f)  velY=%.0f→%.0f",
                  this->m_posX, this->m_posY, this->m_prevVelY, this->m_velY);

    // ── Debug: airborne throttled log ─────────────────────────────────────────
    // 120 ticks/s is too noisy. Log every 12 ticks (~10 lines/second) instead.
    if (!this->m_onGround) {
        ++this->m_airTicks;
        if (this->m_airTicks % 12 == 0)
            LOG_DEBUG("Air   | pos=(%.0f, %.0f)  velY=%+.1f", this->m_posX, this->m_posY, this->m_velY);
    }
}

// =============================================================================
// Render — draw the player at the interpolated position
//
// alpha blends between (m_prevX, m_prevY) — the position at the start of the
// last physics tick — and (m_posX, m_posY) — the position at the end of it.
// This produces sub-tick smooth motion at any frame rate.
// =============================================================================

void Player::render(SDL_Renderer* renderer,
                    const AssetRegistry& assets,
                    const Camera& camera,
                    double alpha) const
{
    // Interpolate world position for this render frame.
    float rx = static_cast<float>(this->m_prevX + (this->m_posX - this->m_prevX) * alpha);
    float ry = static_cast<float>(this->m_prevY + (this->m_posY - this->m_prevY) * alpha);

    // Convert world → screen using the camera's interpolated offset.
    int sx = camera.toScreenX(rx, alpha);
    int sy = camera.toScreenY(ry, alpha);

    // ── Body ──────────────────────────────────────────────────────────────────
    SDL_Rect body { sx, sy, WIDTH, HEIGHT };
    SDL_RenderCopy(renderer, assets.get(TextureID::Player), nullptr, &body);

    // ── Eye (facing-direction indicator) ─────────────────────────────────────
    // The eye sits in the upper-right quadrant when facing right, upper-left
    // when facing left. Offset values are tuned to the procedural sprite.
    SDL_Rect eye {
        sx + (this->m_facingRight ? 20 : 4),
        sy + 10,
        8, 8
    };
    SDL_RenderCopy(renderer, assets.get(TextureID::PlayerEye), nullptr, &eye);

    // ── Attack box (debug visualisation) ─────────────────────────────────────
    // Drawn as a yellow outline so the swing area is visible during development.
    // Remove or gate behind a debug flag before shipping.
    if (this->m_isAttacking) {
        SDL_Rect hb = this->attackHitbox();
        hb.x = camera.toScreenX(hb.x, alpha);
        hb.y = camera.toScreenY(hb.y, alpha);
        SDL_SetRenderDrawColor(renderer, 255, 220, 0, 255);
        SDL_RenderDrawRect(renderer, &hb);
    }
}

// =============================================================================
// attackHitbox — world-space rectangle the current swing occupies
//
// The box extends horizontally from the player's leading edge in the facing
// direction. Vertically it is centred on the player body. Only call this when
// isAttacking() is true — the result is meaningless otherwise.
// =============================================================================

SDL_Rect Player::attackHitbox() const {
    const int x = this->m_facingRight
        ? static_cast<int>(this->m_posX) + this->WIDTH   // right of player body
        : static_cast<int>(this->m_posX) - this->ATTACK_W; // left of player body
    const int y = static_cast<int>(this->m_posY) + (this->HEIGHT - ATTACK_H) / 2;
    return SDL_Rect{ x, y, this->ATTACK_W, this->ATTACK_H };
}
