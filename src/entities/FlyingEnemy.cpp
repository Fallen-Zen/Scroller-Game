// =============================================================================
// FlyingEnemy.cpp
// =============================================================================

#include "entities/FlyingEnemy.h"
#include "core/AssetRegistry.h"
#include "world/Camera.h"
#include "world/Tilemap.h"
#include <SDL.h>
#include <cmath>  // std::sinf

// =============================================================================
// Construction
// =============================================================================

FlyingEnemy::FlyingEnemy(float x, float y)
    : Enemy(x, y, WIDTH, HEIGHT, /*hp=*/2)
    , m_baseY(y)
{}

// =============================================================================
// Update
//
// The flying enemy has a simplified physics model:
//   - No gravity. Y is driven entirely by the sine oscillation.
//   - Horizontal velocity set from facing direction, bounded by resolveX().
//   - When resolveX() zeroes velX (wall hit), flip facing direction.
// =============================================================================

void FlyingEnemy::update(const Tilemap& tilemap, double dt) {
    float fdt = static_cast<float>(dt);

    // ── Horizontal patrol ─────────────────────────────────────────────────────
    this->m_velX = this->m_facingRight ? FLY_SPEED : -FLY_SPEED;

    float intendedVelX = this->m_velX;
    this->m_posX += this->m_velX * fdt;
    this->resolveX(tilemap);  // bounce off solid walls

    // Wall hit: resolveX zeroed velX while we were moving → flip.
    if (this->m_velX == 0.f && intendedVelX != 0.f)
        this->m_facingRight = !this->m_facingRight;

    // ── Vertical sine-wave hover ──────────────────────────────────────────────
    // Advance the phase and compute the new Y directly from the wave.
    // We write m_posY (and m_prevY via saveOldPosition) rather than integrating
    // a velocity, so the oscillation is perfectly periodic with no drift.
    this->m_phase += HOVER_FREQ * fdt;
    if (this->m_phase > 6.2832f) this->m_phase -= 6.2832f;  // keep in [0, 2π)

    this->m_posY = this->m_baseY + HOVER_AMP * std::sinf(this->m_phase);

    // m_velY is kept in sync for render interpolation continuity.
    this->m_velY = HOVER_AMP * HOVER_FREQ * std::cosf(this->m_phase);
}

// =============================================================================
// Render
// =============================================================================

void FlyingEnemy::render(SDL_Renderer* renderer,
                         const AssetRegistry& assets,
                         const Camera& camera,
                         double alpha) const
{
    // Interpolated world position for this render frame.
    float rx = static_cast<float>(this->m_prevX + (this->m_posX - this->m_prevX) * alpha);
    float ry = static_cast<float>(this->m_prevY + (this->m_posY - this->m_prevY) * alpha);

    int sx = camera.toScreenX(rx, alpha);
    int sy = camera.toScreenY(ry, alpha);

    // ── Wings: dark purple, wider than the body ───────────────────────────────
    SDL_SetRenderDrawColor(renderer, 80, 20, 120, 255);
    SDL_Rect wingL { sx - 6,           sy + 4, 10, HEIGHT - 8 };
    SDL_Rect wingR { sx + WIDTH - 4,   sy + 4, 10, HEIGHT - 8 };
    SDL_RenderFillRect(renderer, &wingL);
    SDL_RenderFillRect(renderer, &wingR);

    // ── Body: bright purple core ──────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 150, 60, 200, 255);
    SDL_Rect body { sx, sy, WIDTH, HEIGHT };
    SDL_RenderFillRect(renderer, &body);

    // ── Eyes: bright green orbs ───────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 0, 220, 80, 255);
    int eyeX = sx + (this->m_facingRight ? WIDTH - 7 : 1);
    SDL_Rect eye { eyeX, sy + 5, 6, 6 };
    SDL_RenderFillRect(renderer, &eye);

    // ── HP bar ────────────────────────────────────────────────────────────────
    // Only draw if damaged so healthy enemies don't show a bar.
    if (this->m_hp < this->m_maxHp) {
        static constexpr int BAR_W   = WIDTH;
        static constexpr int BAR_H   = 6;
        static constexpr int BAR_GAP = 10;  // pixels above the sprite top
        const int barY = sy - BAR_GAP - BAR_H;

        // Background (empty bar)
        SDL_SetRenderDrawColor(renderer, 80, 0, 100, 255);
        SDL_Rect bg { sx, barY, BAR_W, BAR_H };
        SDL_RenderFillRect(renderer, &bg);

        // Filled portion — scales with remaining HP
        SDL_SetRenderDrawColor(renderer, 220, 80, 255, 255);
        int filled = (this->m_hp * BAR_W) / this->m_maxHp;
        SDL_Rect fill { sx, barY, filled, BAR_H };
        SDL_RenderFillRect(renderer, &fill);
    }

    (void)assets;  // drawn with primitives, no texture lookup needed
}
