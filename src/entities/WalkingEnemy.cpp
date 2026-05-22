// =============================================================================
// WalkingEnemy.cpp
// =============================================================================

#include "entities/WalkingEnemy.h"
#include "core/AssetRegistry.h"
#include "core/Logger.h"
#include "world/Camera.h"
#include "world/Tilemap.h"
#include <SDL.h>

// =============================================================================
// Construction
// =============================================================================

WalkingEnemy::WalkingEnemy(float x, float y)
    : Enemy(x, y, W, H, /*hp=*/3)
{}

// =============================================================================
// Update
//
// Each tick:
//   1. Set vx from facing direction.
//   2. Ledge detection — if no solid tile below the leading foot, flip.
//   3. Gravity + integrate Y → resolveY.
//   4. Integrate X → resolveX. If resolveX zeroed vx, we hit a wall — flip.
// =============================================================================

void WalkingEnemy::update(const Tilemap& tilemap, double dt) {
    float fdt = static_cast<float>(dt);

    // ── Horizontal velocity from patrol direction ─────────────────────────────
    m_vx = m_facingRight ? WALK_SPEED : -WALK_SPEED;

    // ── Ledge detection (only meaningful when grounded) ───────────────────────
    // Check the tile one column ahead of the leading foot.
    // If it is air the enemy would walk off the edge — flip instead.
    if (m_onGround) {
        // The leading foot is one pixel past the front of the sprite.
        float leadingEdgeX = m_facingRight
            ? m_px + W          // one px past right edge
            : m_px - 1.f;       // one px past left edge

        int leadCol = Tilemap::worldToCol(leadingEdgeX);
        int footRow = Tilemap::worldToRow(m_py + H); // row below feet

        bool floorAhead = tilemap.isSolid(leadCol, footRow)
                       || tilemap.isPlatform(leadCol, footRow);

        if (!floorAhead) {
            m_facingRight = !m_facingRight;
            m_vx = m_facingRight ? WALK_SPEED : -WALK_SPEED;
        }
    }

    // ── Gravity ───────────────────────────────────────────────────────────────
    m_vy += GRAVITY * fdt;

    // ── Integrate Y → resolve vertical collisions ────────────────────────────
    m_py += m_vy * fdt;
    resolveY(tilemap);

    // ── Integrate X → resolve horizontal collisions ──────────────────────────
    // Capture vx before resolveX so we can detect a wall hit (resolveX zeros
    // vx when the entity is snapped back from a solid tile).
    float intendedVx = m_vx;
    m_px += m_vx * fdt;
    resolveX(tilemap);

    // If resolveX zeroed vx while we were moving, we walked into a wall → flip.
    if (m_onGround && m_vx == 0.f && intendedVx != 0.f) {
        m_facingRight = !m_facingRight;
        LOG_DEBUG("WalkEnemy | wall flip at px=%.0f", m_px);
    }
}

// =============================================================================
// Render
// =============================================================================

void WalkingEnemy::render(SDL_Renderer* renderer,
                          const AssetRegistry& assets,
                          const Camera& camera,
                          double alpha) const
{
    // Interpolated world position for this render frame.
    float rx = static_cast<float>(m_ox + (m_px - m_ox) * alpha);
    float ry = static_cast<float>(m_oy + (m_py - m_oy) * alpha);

    int sx = camera.toScreenX(rx, alpha);
    int sy = camera.toScreenY(ry, alpha);

    // ── Body: dark red squat rectangle ───────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_Rect body { sx, sy, W, H };
    SDL_RenderFillRect(renderer, &body);

    // ── Darker torso stripe ───────────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 130, 25, 25, 255);
    SDL_Rect stripe { sx + 4, sy + H / 3, W - 8, H / 3 };
    SDL_RenderFillRect(renderer, &stripe);

    // ── Eyes: bright yellow, positioned based on facing direction ─────────────
    SDL_SetRenderDrawColor(renderer, 240, 220, 0, 255);
    int eyeX = sx + (m_facingRight ? W - 9 : 3);
    SDL_Rect eye { eyeX, sy + 6, 6, 6 };
    SDL_RenderFillRect(renderer, &eye);

    // ── HP bar above the sprite ───────────────────────────────────────────────
    // Only draw if damaged so healthy enemies don't show a bar.
    if (m_hp < m_maxHp) {
        int barW = W;
        int barH = 3;
        int barY = sy - 6;

        // Background (empty bar)
        SDL_SetRenderDrawColor(renderer, 60, 0, 0, 255);
        SDL_Rect bg { sx, barY, barW, barH };
        SDL_RenderFillRect(renderer, &bg);

        // Filled portion
        SDL_SetRenderDrawColor(renderer, 220, 40, 40, 255);
        int filled = (m_hp * barW) / m_maxHp;
        SDL_Rect fill { sx, barY, filled, barH };
        SDL_RenderFillRect(renderer, &fill);
    }

    (void)assets;  // no texture needed — drawn with primitives
}
