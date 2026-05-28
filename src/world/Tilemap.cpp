// =============================================================================
// Tilemap.cpp
// =============================================================================

#include "world/Tilemap.h"
#include "core/AssetRegistry.h"
#include "core/Logger.h"
#include <algorithm>  // std::max, std::min

// =============================================================================
// Construction
// =============================================================================

Tilemap::Tilemap(int cols, int rows)
    : m_cols(cols)
    , m_rows(rows)
    , m_tiles(cols * rows, TileType::Air)  // initialise every cell to Air
{
    LOG_DEBUG("Tilemap created: %dx%d tiles (%dx%d px)",
              cols, rows, widthPx(), heightPx());
}

// =============================================================================
// Data access
// =============================================================================

TileType Tilemap::get(int col, int row) const {
    // Return Air for out-of-bounds so callers can query freely near edges
    // without crashing. Physics code in particular queries tiles at player
    // corners and doesn't want to bounds-check every time.
    if (!isInBounds(col, row)) return TileType::Air;
    return m_tiles[idx(col, row)];
}

void Tilemap::set(int col, int row, TileType type) {
    if (!isInBounds(col, row)) return;
    m_tiles[idx(col, row)] = type;
}

void Tilemap::fill(int col, int row, int w, int h, TileType type) {
    // Clamp the fill region to map bounds so callers don't need to be precise.
    int c0 = std::max(col, 0);
    int r0 = std::max(row, 0);
    int c1 = std::min(col + w, m_cols);
    int r1 = std::min(row + h, m_rows);

    for (int r = r0; r < r1; ++r)
        for (int c = c0; c < c1; ++c)
            m_tiles[idx(c, r)] = type;
}

// =============================================================================
// Collision queries
// =============================================================================

bool Tilemap::isInBounds(int col, int row) const {
    return col >= 0 && col < m_cols && row >= 0 && row < m_rows;
}

bool Tilemap::isSolid(int col, int row) const {
    // Out-of-bounds tiles (past map edges) are treated as solid walls so
    // entities can't walk off the edge of the world.
    if (!isInBounds(col, row)) return true;
    return m_tiles[idx(col, row)] == TileType::Ground;
}

bool Tilemap::isPlatform(int col, int row) const {
    if (!isInBounds(col, row)) return false;
    return m_tiles[idx(col, row)] == TileType::Platform;
}

// =============================================================================
// Rendering
// =============================================================================

void Tilemap::render(SDL_Renderer* renderer, const AssetRegistry& assets,
                     int cameraX, int cameraY,
                     int viewW, int viewH) const
{
    // ── Compute which tile columns and rows are on screen ─────────────────────
    // Convert the camera's world-pixel position to tile coordinates, then
    // expand by one tile on each side to avoid pop-in at the screen edge.
    //
    // Example: cameraX=24, TILE_SIZE=16 → firstCol = 24/16 - 1 = 0 (clamped)
    int firstCol = std::max(0,       cameraX / TILE_SIZE - 1);
    int lastCol  = std::min(m_cols,  (cameraX + viewW) / TILE_SIZE + 2);
    int firstRow = std::max(0,       cameraY / TILE_SIZE - 1);
    int lastRow  = std::min(m_rows,  (cameraY + viewH) / TILE_SIZE + 2);

    for (int row = firstRow; row < lastRow; ++row) {
        for (int col = firstCol; col < lastCol; ++col) {
            TileType type = get(col, row);

            // Air tiles are invisible — nothing to draw.
            if (type == TileType::Air) continue;

            // Convert tile grid position → world pixel position → screen position.
            // Screen position = world position - camera offset.
            SDL_Rect dst {
                col * TILE_SIZE - cameraX,
                row * TILE_SIZE - cameraY,
                TILE_SIZE,
                TILE_SIZE
            };

            // Choose the texture based on tile type.
            // Platform uses the same ground texture for now — a distinct
            // texture (e.g. a wooden plank) can be added to AssetRegistry later.
            TextureID texId = (type == TileType::Ground)
                ? TextureID::TileGround
                : TextureID::TileGround;  // Platform — same texture for now

            SDL_RenderCopy(renderer, assets.get(texId), nullptr, &dst);
        }
    }
}
