// =============================================================================
// Entity.cpp
// =============================================================================

#include "entities/Entity.h"
#include "world/Tilemap.h"

// =============================================================================
// Construction
// =============================================================================

Entity::Entity(float x, float y, int w, int h)
    : m_px(x), m_py(y)
    , m_ox(x), m_oy(y)   // start with old == current so frame 0 doesn't lerp
    , m_vx(0.f), m_vy(0.f)
    , m_w(w), m_h(h)
{}

// =============================================================================
// Lifecycle
// =============================================================================

void Entity::saveOldPosition() {
    // Called by EntityManager before update(). Stores the current position so
    // render() can interpolate between this and the post-update position.
    m_ox = m_px;
    m_oy = m_py;
}

// =============================================================================
// resolveX  —  horizontal collision
//
// After integrating m_px we probe the tile column at the player's leading edge
// (right when m_vx > 0, left when m_vx < 0). If any tile in the vertical body
// span at that column is solid, we snap m_px so the entity's edge aligns with
// the tile boundary and zero m_vx to stop horizontal movement.
//
// The vertical scan uses the current m_py — which is already resolved by
// resolveY() before resolveX() is called — so the floor tile the entity stands
// on is never in the scan range (its row is below m_py + m_h - 1).
// =============================================================================

void Entity::resolveX(const Tilemap& tilemap) {
    // Vertical span the entity body currently occupies.
    int rowTop = Tilemap::worldToRow(m_py);
    int rowBot = Tilemap::worldToRow(m_py + m_h - 1);

    if (m_vx > 0.f) {
        // ── Moving right: probe the column at the right edge ─────────────────
        int colR = Tilemap::worldToCol(m_px + m_w - 1);

        for (int r = rowTop; r <= rowBot; ++r) {
            if (tilemap.isSolid(colR, r)) {
                // Align right edge of entity to the left face of the blocking tile.
                m_px = Tilemap::colToWorld(colR) - m_w;
                m_vx = 0.f;
                break;
            }
        }

    } else if (m_vx < 0.f) {
        // ── Moving left: probe the column at the left edge ────────────────────
        int colL = Tilemap::worldToCol(m_px);

        for (int r = rowTop; r <= rowBot; ++r) {
            if (tilemap.isSolid(colL, r)) {
                // Align left edge of entity to the right face of the blocking tile.
                m_px = Tilemap::colToWorld(colL + 1);
                m_vx = 0.f;
                break;
            }
        }
    }
}

// =============================================================================
// resolveY  —  vertical collision
//
// Falling (m_vy >= 0):
//   Probe row = worldToRow(m_py + m_h). Using m_h (one past the last pixel)
//   rather than m_h - 1 is intentional: when the entity is resting exactly on
//   a tile surface its bottom pixel is at (tileRow * TILE_SIZE - 1), which maps
//   to the row ABOVE the ground. Using +m_h hits the correct ground row even
//   when the entity is flush with the surface.
//
// Rising (m_vy < 0):
//   Probe row = worldToRow(m_py). Both solid tiles and platform tiles block the
//   head — the entity's head bumps the underside of platforms.
//
// Platform one-way landing:
//   Platforms only stop a falling entity when it was above the platform surface
//   at the START of this tick. m_oy (saved by saveOldPosition before update)
//   captures that pre-integration position, so the condition
//   m_oy + m_h <= platformTop means "feet were at or above the surface".
//   Entities moving upward pass through platforms freely.
// =============================================================================

void Entity::resolveY(const Tilemap& tilemap) {
    // Horizontal span the entity body occupies.
    int colLeft  = Tilemap::worldToCol(m_px);
    int colRight = Tilemap::worldToCol(m_px + m_w - 1);

    // Save whether we were grounded before this tick. Used below so the
    // landing log fires only once (on the airborne → grounded transition)
    // rather than every tick the entity sits on the floor.
    bool wasOnGround = m_onGround;
    (void)wasOnGround;  // subclasses use it; suppress unused-variable warning here

    // Reset each tick; resolveY is the only place that sets it to true.
    m_onGround = false;

    if (m_vy >= 0.f) {
        // ── Falling or stationary: probe one row below the feet ───────────────
        int rowB = Tilemap::worldToRow(m_py + m_h);

        for (int c = colLeft; c <= colRight; ++c) {

            if (tilemap.isSolid(c, rowB)) {
                m_py       = Tilemap::rowToWorld(rowB) - m_h;
                m_vy       = 0.f;
                m_onGround = true;
                break;
            }

            // Platform: one-way — only land if feet were above the surface
            // at the start of this tick.
            if (tilemap.isPlatform(c, rowB)) {
                float platformTopY = Tilemap::rowToWorld(rowB);
                if (m_oy + m_h <= platformTopY) {
                    m_py       = platformTopY - m_h;
                    m_vy       = 0.f;
                    m_onGround = true;
                    break;
                }
                // Below platform top (jumping through from below) → no collision.
            }
        }

    } else {
        // ── Rising: probe the tile row at the head ────────────────────────────
        // Both solid tiles and platforms block upward movement so the entity's
        // head bumps the underside of platforms.
        int rowT = Tilemap::worldToRow(m_py);

        for (int c = colLeft; c <= colRight; ++c) {
            if (tilemap.isSolid(c, rowT) || tilemap.isPlatform(c, rowT)) {
                m_py = Tilemap::rowToWorld(rowT + 1);
                m_vy = 0.f;
                break;
            }
        }
    }
}
