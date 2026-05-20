// =============================================================================
// AssetRegistry.h — Central store for all SDL_Texture* handles
//
// All game art is procedural — every texture is painted at startup by drawing
// colored rectangles onto an SDL_Surface, then uploading it to the GPU as an
// SDL_Texture. No image files are ever loaded.
//
// Why a registry?
//   Without one, every system that needs to draw something would have to own
//   its own texture, manage its lifetime, and know how to create it. The
//   registry centralises that: create once, share everywhere, destroy once.
//
// Usage:
//   // In Game (after renderer is created):
//   m_assets = std::make_unique<AssetRegistry>(m_renderer);
//
//   // Anywhere that draws:
//   SDL_Texture* tex = m_assets->get(TextureID::Player);
//   SDL_RenderCopy(renderer, tex, nullptr, &destRect);
// =============================================================================

#pragma once
#include <SDL.h>
#include <array>

// -----------------------------------------------------------------------------
// TextureID — every procedural texture the game owns
//
// Add new entries here as new art is needed. Count must stay last.
// Using an enum class means you can't accidentally pass a raw int and the
// compiler enforces that every ID is handled where required.
// -----------------------------------------------------------------------------
enum class TextureID {
    Player,        // 32×48 — player body (red-orange)
    PlayerEye,     // 8×8   — facing indicator (white square)
    TileGround,    // 16×16 — ground/floor tile (dark green with highlight)
    Count          // sentinel — keep last
};

// -----------------------------------------------------------------------------
// AssetRegistry
// -----------------------------------------------------------------------------
class AssetRegistry {
public:
    // Generates all procedural textures immediately. Requires a valid renderer.
    // Throws std::runtime_error if any texture fails to create.
    explicit AssetRegistry(SDL_Renderer* renderer);

    // Destroys all owned textures and releases GPU memory.
    ~AssetRegistry();

    // Non-copyable, non-movable — owns GPU texture handles.
    AssetRegistry(const AssetRegistry&)            = delete;
    AssetRegistry& operator=(const AssetRegistry&) = delete;
    AssetRegistry(AssetRegistry&&)                 = delete;
    AssetRegistry& operator=(AssetRegistry&&)      = delete;

    // Return the texture for the given ID. Always valid after construction.
    SDL_Texture* get(TextureID id) const;

private:
    static constexpr int N = static_cast<int>(TextureID::Count);

    // All textures stored in a fixed array, indexed by TextureID.
    // Stored as raw pointers because SDL owns the GPU resource — we just hold
    // the handle and call SDL_DestroyTexture when done.
    std::array<SDL_Texture*, N> m_textures{};

    // Cached renderer reference — needed only during construction for painting.
    SDL_Renderer* m_renderer = nullptr;

    // ── Procedural painters ───────────────────────────────────────────────────
    // Each method builds one texture. They are called once in the constructor
    // and the result stored in m_textures. Separated into methods to keep the
    // constructor readable and to make individual textures easy to tweak.

    SDL_Texture* makePlayer();     // 32×48 red-orange body
    SDL_Texture* makePlayerEye();  // 8×8 white square
    SDL_Texture* makeTileGround(); // 16×16 dark green tile with top highlight

    // ── Painting helpers ──────────────────────────────────────────────────────

    // Create a blank RGBA surface of the given size. Returns nullptr on failure.
    // The caller owns the surface and must SDL_FreeSurface() it.
    static SDL_Surface* createSurface(int w, int h);

    // Fill a rectangle on a surface with an RGBA colour.
    // nullptr rect fills the entire surface (same as SDL_FillRect semantics).
    static void fillRect(SDL_Surface* surf, const SDL_Rect* rect,
                         Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);

    // Upload a surface to the GPU as a texture, then free the surface.
    // Returns nullptr and logs an error if upload fails.
    SDL_Texture* upload(SDL_Surface* surf);
};
