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
    : Enemy(x, y, WIDTH, HEIGHT, /*hp=*/50)
{}

// =============================================================================
// Update
//
// Each tick:
//   1. Set velX from facing direction.
//   2. Ledge detection — if no solid tile below the leading foot, flip.
//   3. Gravity + integrate Y → resolveY.
//   4. Integrate X → resolveX. If resolveX zeroed velX, we hit a wall — flip.
// =============================================================================

void WalkingEnemy::update(const Tilemap& tilemap, double dt) {
    float fdt = static_cast<float>(dt);

    // ── Horizontal velocity from patrol direction ─────────────────────────────
    this->m_velX = this->m_facingRight ? WALK_SPEED : -WALK_SPEED;

    // ── Ledge detection (only meaningful when grounded) ───────────────────────
    // Check the tile one column ahead of the leading foot.
    // If it is air the enemy would walk off the edge — flip instead.
    if (this->m_onGround) {
        // The leading foot is one pixel past the front of the sprite.
        float leadingEdgeX = this->m_facingRight
            ? this->m_posX + WIDTH    // one px past right edge
            : this->m_posX - 1.f;    // one px past left edge

        int leadCol = Tilemap::worldToCol(leadingEdgeX);
        int footRow = Tilemap::worldToRow(this->m_posY + HEIGHT); // row below feet

        bool floorAhead = tilemap.isSolid(leadCol, footRow)
                       || tilemap.isPlatform(leadCol, footRow);

        if (!floorAhead) {
            this->m_facingRight = !this->m_facingRight;
            this->m_velX = this->m_facingRight ? WALK_SPEED : -WALK_SPEED;
        }
    }

    // ── Gravity ───────────────────────────────────────────────────────────────
    this->m_velY += GRAVITY * fdt;

    // ── Integrate Y → resolve vertical collisions ────────────────────────────
    this->m_posY += this->m_velY * fdt;
    this->resolveY(tilemap);

    // ── Integrate X → resolve horizontal collisions ──────────────────────────
    // Capture velX before resolveX so we can detect a wall hit (resolveX zeros
    // velX when the entity is snapped back from a solid tile).
    float intendedVelX = this->m_velX;
    this->m_posX += this->m_velX * fdt;
    this->resolveX(tilemap);

    // If resolveX zeroed velX while we were moving, we walked into a wall → flip.
    if (this->m_onGround && this->m_velX == 0.f && intendedVelX != 0.f) {
        this->m_facingRight = !this->m_facingRight;
        LOG_DEBUG("WalkEnemy | wall flip at posX=%.0f", this->m_posX);
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
    float rx = static_cast<float>(this->m_prevX + (this->m_posX - this->m_prevX) * alpha);
    float ry = static_cast<float>(this->m_prevY + (this->m_posY - this->m_prevY) * alpha);

    int sx = camera.toScreenX(rx, alpha);
    int sy = camera.toScreenY(ry, alpha);

    // ── Body: dark red squat rectangle ───────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_Rect body { sx, sy, WIDTH, HEIGHT };
    SDL_RenderFillRect(renderer, &body);

    // ── Darker torso stripe ───────────────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 130, 25, 25, 255);
    SDL_Rect stripe { sx + 4, sy + HEIGHT / 3, WIDTH - 8, HEIGHT / 3 };
    SDL_RenderFillRect(renderer, &stripe);

    // ── Eyes: bright yellow, positioned based on facing direction ─────────────
    SDL_SetRenderDrawColor(renderer, 240, 220, 0, 255);
    int eyeX = sx + (this->m_facingRight ? WIDTH - 9 : 3);
    SDL_Rect eye { eyeX, sy + 6, 6, 6 };
    SDL_RenderFillRect(renderer, &eye);

    // ── HP bar above the sprite ───────────────────────────────────────────────
    // Only draw if damaged so healthy enemies don't show a bar.
    if (this->m_hp < this->m_maxHp) {
        static constexpr int BAR_W   = WIDTH;
        static constexpr int BAR_H   = 6;
        static constexpr int BAR_GAP = 10;  // pixels above the sprite top
        const int barY = sy - BAR_GAP - BAR_H;

        // Background (empty bar)
        SDL_SetRenderDrawColor(renderer, 100, 20, 20, 255);
        SDL_Rect bg { sx, barY, BAR_W, BAR_H };
        SDL_RenderFillRect(renderer, &bg);

        // Filled portion — scales with remaining HP
        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
        int filled = (this->m_hp * BAR_W) / this->m_maxHp;
        SDL_Rect fill { sx, barY, filled, BAR_H };
        SDL_RenderFillRect(renderer, &fill);
    }

    (void)assets;  // no texture needed — drawn with primitives
}
