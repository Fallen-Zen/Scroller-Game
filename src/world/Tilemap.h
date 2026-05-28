// =============================================================================
// Tilemap.h — 2D grid of tiles
//
// The tilemap is the backbone of the game world. It stores a rectangular grid
// of TileType values, answers collision queries ("is this cell solid?"), and
// draws itself to the screen one tile at a time.
//
// Responsibilities:
//   - Hold tile data (pure data, no game logic)
//   - Answer spatial queries: isSolid(), worldToTile(), tileToWorld()
//   - Render the visible portion of the map
//
// What it does NOT do:
//   - Physics (that's the Physics system in Layer 2)
//   - Spawning entities (that's the Room system in Layer 6)
//   - Loading from files (maps are built in code for now)
// =============================================================================

#pragma once
#include <SDL.h>
#include <cstdint>
#include <vector>

// Forward declarations — avoids including full headers here.
class AssetRegistry;

// -----------------------------------------------------------------------------
// TileType — every kind of tile the world can contain
//
// Stored as uint8_t (1 byte per cell) to keep map memory small.
// A 200×50 map costs 200×50×1 = 10 000 bytes — trivial.
//
// Add new tile types here as the game grows (Spike, Ladder, Water…).
// -----------------------------------------------------------------------------
enum class TileType : uint8_t {
    Air      = 0,  // Empty space — no collision, not drawn
    Ground   = 1,  // Fully solid on all four sides (wall, floor, ceiling)
    Platform = 2,  // One-way: solid only when approached from above
                   // (player can jump through from below)
};

// -----------------------------------------------------------------------------
// Tilemap
// -----------------------------------------------------------------------------
class Tilemap {
public:
    // Each tile is TILE_SIZE × TILE_SIZE pixels in world space.
    // 16 is standard for classic action platformers — small enough for fine
    // detail, large enough to see clearly at 1280×720.
    static constexpr int TILE_SIZE = 16;

    // Construct an empty map (all Air) of the given dimensions.
    Tilemap(int cols, int rows);

    // ── Data access ───────────────────────────────────────────────────────────

    // Read a single tile. Returns Air for out-of-bounds coordinates so callers
    // don't need to bounds-check before every query.
    TileType get(int col, int row) const;

    // Write a single tile. Out-of-bounds writes are silently ignored.
    void set(int col, int row, TileType type);

    // Bulk-fill a rectangular region with one tile type.
    // Useful for painting walls, floors, and platforms in one call.
    void fill(int col, int row, int w, int h, TileType type);

    // ── Collision queries ─────────────────────────────────────────────────────

    // True if the tile at (col, row) blocks movement from all directions.
    bool isSolid(int col, int row) const;

    // True if the tile at (col, row) is a one-way platform.
    bool isPlatform(int col, int row) const;

    // True if (col, row) is within the map bounds.
    bool isInBounds(int col, int row) const;

    // ── Coordinate conversion ─────────────────────────────────────────────────

    // Convert a world-space pixel position to a tile grid coordinate.
    // e.g. worldX=34 → col=2  (34 / TILE_SIZE = 2)
    static int worldToCol(float worldX) { return static_cast<int>(worldX) / TILE_SIZE; }
    static int worldToRow(float worldY) { return static_cast<int>(worldY) / TILE_SIZE; }

    // Convert a tile grid coordinate to the top-left pixel of that tile.
    static float colToWorld(int col) { return static_cast<float>(col * TILE_SIZE); }
    static float rowToWorld(int row) { return static_cast<float>(row * TILE_SIZE); }

    // ── Dimensions ────────────────────────────────────────────────────────────

    int cols()     const { return m_cols; }
    int rows()     const { return m_rows; }
    int widthPx()  const { return m_cols * TILE_SIZE; }  // total map width in px
    int heightPx() const { return m_rows * TILE_SIZE; }  // total map height in px

    // ── Rendering ─────────────────────────────────────────────────────────────

    // Draw all visible tiles. cameraX/Y is the top-left world coordinate
    // currently on screen — tiles outside the viewport are skipped.
    // viewW/viewH is the viewport size in pixels (typically 1280×720).
    void render(SDL_Renderer* renderer, const AssetRegistry& assets,
                int cameraX, int cameraY,
                int viewW, int viewH) const;

private:
    int m_cols;
    int m_rows;

    // Grid stored in row-major order: index = row * m_cols + col.
    // Row 0 is the top of the map, row (m_rows-1) is the bottom.
    std::vector<TileType> m_tiles;

    // Compute the flat array index for (col, row). No bounds check here —
    // callers that need safety use isInBounds() first.
    int idx(int col, int row) const { return row * m_cols + col; }
};
