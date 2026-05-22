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
    : Enemy(x, y, W, H, /*hp=*/2)
    , m_baseY(y)
{}

// =============================================================================
// Update
//
// The flying enemy has a simplified physics model:
//   - No gravity. Y is driven entirely by the sine oscillation.
//   - Horizontal velocity set from facing direction, bounded by resolveX().
//   - When resolveX() zeroes vx (wall hit), flip facing direction.
// =============================================================================

void FlyingEnemy::update(const Tilemap& tilemap, double dt) {
    float fdt = static_cast<float>(dt);

    // ── Horizontal patrol ─────────────────────────────────────────────────────
    m_vx = m_facingRight ? FLY_SPEED : -FLY_SPEED;

    float intendedVx = m_vx;
    m_px += m_vx * fdt;
    resolveX(tilemap);  // bounce off solid walls

    // Wall hit: resolveX zeroed vx while we were moving → flip.
    if (m_vx == 0.f && intendedVx != 0.f)
        m_facingRight = !m_facingRight;

    // ── Vertical sine-wave hover ──────────────────────────────────────────────
    // Advance the phase and compute the new Y directly from the wave.
    // We write m_py (and m_oy via saveOldPosition) rather than integrating
    // a velocity, so the oscillation is perfectly periodic with no drift.
    m_phase += HOVER_FREQ * fdt;
    if (m_phase > 6.2832f) m_phase -= 6.2832f;  // keep in [0, 2π)

    m_py = m_baseY + HOVER_AMP * std::sinf(m_phase);

    // m_vy is kept in sync for render interpolation continuity.
    m_vy = HOVER_AMP * HOVER_FREQ * std::cosf(m_phase);
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
    float rx = static_cast<float>(m_ox + (m_px - m_ox) * alpha);
    float ry = static_cast<float>(m_oy + (m_py - m_oy) * alpha);

    int sx = camera.toScreenX(rx, alpha);
    int sy = camera.toScreenY(ry, alpha);

    // ── Wings: dark purple, wider than the body ───────────────────────────────
    SDL_SetRenderDrawColor(renderer, 80, 20, 120, 255);
    SDL_Rect wingL { sx - 6,      sy + 4, 10, H - 8 };
    SDL_Rect wingR { sx + W - 4,  sy + 4, 10, H - 8 };
    SDL_RenderFillRect(renderer, &wingL);
    SDL_RenderFillRect(renderer, &wingR);

    // ── Body: bright purple core ──────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 150, 60, 200, 255);
    SDL_Rect body { sx, sy, W, H };
    SDL_RenderFillRect(renderer, &body);

    // ── Eyes: bright green orbs ───────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 0, 220, 80, 255);
    int eyeX = sx + (m_facingRight ? W - 7 : 1);
    SDL_Rect eye { eyeX, sy + 5, 6, 6 };
    SDL_RenderFillRect(renderer, &eye);

    // ── HP bar ────────────────────────────────────────────────────────────────
    if (m_hp < m_maxHp) {
        SDL_SetRenderDrawColor(renderer, 40, 0, 60, 255);
        SDL_Rect bg { sx, sy - 6, W, 3 };
        SDL_RenderFillRect(renderer, &bg);

        SDL_SetRenderDrawColor(renderer, 150, 60, 200, 255);
        int filled = (m_hp * W) / m_maxHp;
        SDL_Rect fill { sx, sy - 6, filled, 3 };
        SDL_RenderFillRect(renderer, &fill);
    }

    (void)assets;  // drawn with primitives, no texture lookup needed
}
