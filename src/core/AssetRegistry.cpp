// =============================================================================
// AssetRegistry.cpp
// =============================================================================

#include "core/AssetRegistry.h"
#include "core/Logger.h"
#include <stdexcept>
#include <string>

// =============================================================================
// Construction / Destruction
// =============================================================================

AssetRegistry::AssetRegistry(SDL_Renderer* renderer)
    : m_renderer(renderer)
{
    LOG_INFO("AssetRegistry: generating procedural textures...");

    // Build every texture. The order must match the TextureID enum.
    // If any painter returns nullptr, upload() has already logged the error —
    // we throw so Game's constructor fails cleanly rather than running with a
    // broken texture set.
    m_textures[static_cast<int>(TextureID::Player)]     = makePlayer();
    m_textures[static_cast<int>(TextureID::PlayerEye)]  = makePlayerEye();
    m_textures[static_cast<int>(TextureID::TileGround)] = makeTileGround();

    for (int i = 0; i < N; ++i) {
        if (!m_textures[i])
            throw std::runtime_error("AssetRegistry: failed to create texture id " + std::to_string(i));
    }

    LOG_INFO("AssetRegistry: %d textures ready", N);
}

AssetRegistry::~AssetRegistry() {
    // Destroy in reverse order (mirrors construction, good habit).
    for (int i = N - 1; i >= 0; --i) {
        if (m_textures[i]) {
            SDL_DestroyTexture(m_textures[i]);
            m_textures[i] = nullptr;
        }
    }
    LOG_DEBUG("AssetRegistry: all textures destroyed");
}

// =============================================================================
// Public API
// =============================================================================

SDL_Texture* AssetRegistry::get(TextureID id) const {
    return m_textures[static_cast<int>(id)];
}

// =============================================================================
// Procedural painters
//
// Each painter follows the same three-step pattern:
//   1. createSurface()  — allocate a CPU-side pixel buffer
//   2. fillRect() calls — paint colored rectangles (our "pixel art")
//   3. upload()         — send to GPU, free the CPU buffer, return handle
//
// SDL_Surface lives on the CPU (RAM). SDL_Texture lives on the GPU (VRAM).
// We only need the surface during construction — once uploaded, the texture
// is self-contained on the GPU and the surface is discarded.
// =============================================================================

SDL_Texture* AssetRegistry::makePlayer() {
    // 32 wide × 48 tall — a typical humanoid aspect ratio (2:3)
    SDL_Surface* s = createSurface(32, 48);
    if (!s) return nullptr;

    // Body — solid red-orange, same colour as the old placeholder rect
    fillRect(s, nullptr, 220, 80, 60);

    // Helmet / head — slightly lighter band across the top 12 rows
    SDL_Rect head { 2, 2, 28, 12 };
    fillRect(s, &head, 240, 110, 80);

    // Belt — dark stripe across the middle to break up the silhouette
    SDL_Rect belt { 0, 28, 32, 4 };
    fillRect(s, &belt, 160, 50, 40);

    // Boots — darker strip at the bottom
    SDL_Rect boots { 0, 42, 32, 6 };
    fillRect(s, &boots, 140, 50, 30);

    LOG_DEBUG("AssetRegistry: Player texture painted (32x48)");
    return upload(s);
}

SDL_Texture* AssetRegistry::makePlayerEye() {
    // 8×8 white square — drawn on top of the player body in Game::render()
    // to indicate facing direction (offset left or right depending on m_facingRight)
    SDL_Surface* s = createSurface(8, 8);
    if (!s) return nullptr;

    fillRect(s, nullptr, 255, 255, 255);

    // Small dark pupil in the centre to make it look like an actual eye
    SDL_Rect pupil { 2, 2, 4, 4 };
    fillRect(s, &pupil, 30, 30, 30);

    LOG_DEBUG("AssetRegistry: PlayerEye texture painted (8x8)");
    return upload(s);
}

SDL_Texture* AssetRegistry::makeTileGround() {
    // 16×16 — standard tile size for the world grid
    SDL_Surface* s = createSurface(16, 16);
    if (!s) return nullptr;

    // Base fill — dark earthy green
    fillRect(s, nullptr, 60, 90, 60);

    // Top highlight strip — lighter green, gives the tile a "grass top" look
    SDL_Rect topStrip { 0, 0, 16, 3 };
    fillRect(s, &topStrip, 100, 160, 80);

    // Subtle corner pixels — dark, simulates ambient occlusion between tiles
    SDL_Rect tlCorner { 0, 0, 1, 1 };
    SDL_Rect trCorner { 15, 0, 1, 1 };
    fillRect(s, &tlCorner, 40, 65, 40);
    fillRect(s, &trCorner, 40, 65, 40);

    LOG_DEBUG("AssetRegistry: TileGround texture painted (16x16)");
    return upload(s);
}

// =============================================================================
// Painting helpers
// =============================================================================

SDL_Surface* AssetRegistry::createSurface(int w, int h) {
    // SDL_PIXELFORMAT_RGBA8888 — 4 bytes per pixel (red, green, blue, alpha),
    // 8 bits each. Alpha allows transparent pixels in future sprites.
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(
        0,                       // flags — always 0 in SDL2
        w, h,
        32,                      // bits per pixel
        SDL_PIXELFORMAT_RGBA8888
    );
    if (!s)
        LOG_ERROR("createSurface(%d,%d) failed: %s", w, h, SDL_GetError());
    return s;
}

void AssetRegistry::fillRect(SDL_Surface* surf, const SDL_Rect* rect,
                              Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    // SDL_MapRGBA converts separate channel values into a single packed Uint32
    // that matches the surface's pixel format. Always use it rather than
    // bit-shifting manually — the byte order differs between platforms.
    Uint32 colour = SDL_MapRGBA(surf->format, r, g, b, a);
    SDL_FillRect(surf, rect, colour);
}

SDL_Texture* AssetRegistry::upload(SDL_Surface* surf) {
    // SDL_CreateTextureFromSurface copies the pixel data from CPU RAM to GPU
    // VRAM. After this call the surface is no longer needed — free it
    // immediately to avoid holding two copies of the pixel data in memory.
    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    SDL_FreeSurface(surf);  // always free even if tex creation failed

    if (!tex)
        LOG_ERROR("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());

    return tex;
}
