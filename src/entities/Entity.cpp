// =============================================================================
// Entity.cpp
// =============================================================================

#include "entities/Entity.h"
#include "world/Tilemap.h"

// =============================================================================
// Construction
// =============================================================================

Entity::Entity(float x, float y, int w, int h)
    : m_posX(x), m_posY(y)
    , m_prevX(x), m_prevY(y)   // start with old == current so frame 0 doesn't lerp
    , m_velX(0.f), m_velY(0.f)
    , m_width(w), m_height(h)
{}

// =============================================================================
// Lifecycle
// =============================================================================

void Entity::saveOldPosition() {
    // Called by EntityManager before update(). Stores the current position so
    // render() can interpolate between this and the post-update position.
    this->m_prevX = this->m_posX;
    this->m_prevY = this->m_posY;
}

// =============================================================================
// resolveX  —  horizontal collision
//
// After integrating m_posX we probe the tile column at the player's leading edge
// (right when m_velX > 0, left when m_velX < 0). If any tile in the vertical body
// span at that column is solid, we snap m_posX so the entity's edge aligns with
// the tile boundary and zero m_velX to stop horizontal movement.
//
// The vertical scan uses the current m_posY — which is already resolved by
// resolveY() before resolveX() is called — so the floor tile the entity stands
// on is never in the scan range (its row is below m_posY + m_height - 1).
// =============================================================================

void Entity::resolveX(const Tilemap& tilemap) {
    // Vertical span the entity body currently occupies.
    int rowTop = Tilemap::worldToRow(this->m_posY);
    int rowBot = Tilemap::worldToRow(this->m_posY + this->m_height - 1);

    if (this->m_velX > 0.f) {
        // ── Moving right: probe the column at the right edge ─────────────────
        int colR = Tilemap::worldToCol(this->m_posX + this->m_width - 1);

        for (int r = rowTop; r <= rowBot; ++r) {
            if (tilemap.isSolid(colR, r)) {
                // Align right edge of entity to the left face of the blocking tile.
                this->m_posX = Tilemap::colToWorld(colR) - this->m_width;
                this->m_velX = 0.f;
                break;
            }
        }

    } else if (this->m_velX < 0.f) {
        // ── Moving left: probe the column at the left edge ────────────────────
        int colL = Tilemap::worldToCol(this->m_posX);

        for (int r = rowTop; r <= rowBot; ++r) {
            if (tilemap.isSolid(colL, r)) {
                // Align left edge of entity to the right face of the blocking tile.
                this->m_posX = Tilemap::colToWorld(colL + 1);
                this->m_velX = 0.f;
                break;
            }
        }
    }
}

// =============================================================================
// resolveY  —  vertical collision
//
// Falling (m_velY >= 0):
//   Probe row = worldToRow(m_posY + m_height). Using m_height (one past the last
//   pixel) rather than m_height - 1 is intentional: when the entity is resting
//   exactly on a tile surface its bottom pixel is at (tileRow * TILE_SIZE - 1),
//   which maps to the row ABOVE the ground. Using +m_height hits the correct
//   ground row even when the entity is flush with the surface.
//
// Rising (m_velY < 0):
//   Probe row = worldToRow(m_posY). Both solid tiles and platform tiles block the
//   head — the entity's head bumps the underside of platforms.
//
// Platform one-way landing:
//   Platforms only stop a falling entity when it was above the platform surface
//   at the START of this tick. m_prevY (saved by saveOldPosition before update)
//   captures that pre-integration position, so the condition
//   m_prevY + m_height <= platformTop means "feet were at or above the surface".
//   Entities moving upward pass through platforms freely.
// =============================================================================

void Entity::resolveY(const Tilemap& tilemap) {
    // Horizontal span the entity body occupies.
    int colLeft  = Tilemap::worldToCol(this->m_posX);
    int colRight = Tilemap::worldToCol(this->m_posX + this->m_width - 1);

    // Save whether we were grounded before this tick. Used below so the
    // landing log fires only once (on the airborne → grounded transition)
    // rather than every tick the entity sits on the floor.
    bool wasOnGround = this->m_onGround;
    (void)wasOnGround;  // subclasses use it; suppress unused-variable warning here

    // Reset each tick; resolveY is the only place that sets it to true.
    this->m_onGround = false;

    if (this->m_velY >= 0.f) {
        // ── Falling or stationary: probe one row below the feet ───────────────
        int rowB = Tilemap::worldToRow(this->m_posY + this->m_height);

        for (int c = colLeft; c <= colRight; ++c) {

            if (tilemap.isSolid(c, rowB)) {
                this->m_posY     = Tilemap::rowToWorld(rowB) - this->m_height;
                this->m_velY     = 0.f;
                this->m_onGround = true;
                break;
            }

            // Platform: one-way — only land if feet were above the surface
            // at the start of this tick.
            if (tilemap.isPlatform(c, rowB)) {
                float platformTopY = Tilemap::rowToWorld(rowB);
                if (this->m_prevY + this->m_height <= platformTopY) {
                    this->m_posY     = platformTopY - this->m_height;
                    this->m_velY     = 0.f;
                    this->m_onGround = true;
                    break;
                }
                // Below platform top (jumping through from below) → no collision.
            }
        }

    } else {
        // ── Rising: probe the tile row at the head ────────────────────────────
        // Both solid tiles and platforms block upward movement so the entity's
        // head bumps the underside of platforms.
        int rowT = Tilemap::worldToRow(this->m_posY);

        for (int c = colLeft; c <= colRight; ++c) {
            if (tilemap.isSolid(c, rowT) || tilemap.isPlatform(c, rowT)) {
                this->m_posY = Tilemap::rowToWorld(rowT + 1);
                this->m_velY = 0.f;
                break;
            }
        }
    }
}
