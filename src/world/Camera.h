// =============================================================================
// Camera.h — 2D viewport that follows the player
//
// The camera defines which portion of the world is currently visible on screen.
// It stores (m_x, m_y): the world-space coordinate of the viewport's top-left
// corner. Everything drawn on screen is offset by subtracting this value.
//
// Example:
//   Player is at world position (800, 400).
//   Camera centres on the player: camX = 800 - 640 = 160.
//   Player appears on screen at: screenX = 800 - 160 = 640 (centre).
//
// Smooth follow:
//   The camera doesn't snap instantly — it lerps (linearly interpolates)
//   toward the target each physics tick. This feels more natural than rigid
//   tracking and hides small jitter in the player's movement.
//
// Render interpolation:
//   Like the player, the camera stores its position at the previous physics
//   tick (m_ox, m_oy). render() blends between old and current with alpha
//   so the viewport moves smoothly at any frame rate.
// =============================================================================

#pragma once

class Camera {
public:
    // viewW / viewH  — viewport size in pixels (typically 1280 × 720).
    // worldW / worldH — total world size in pixels — used for clamping.
    Camera(int viewW, int viewH, int worldW, int worldH);

    // ── Update (call once per physics tick) ───────────────────────────────────

    // Smoothly move toward centering on (targetX, targetY) in world space.
    // targetX/Y should be the centre of whatever the camera tracks (player
    // centre, not top-left). dt is the fixed physics timestep in seconds.
    // Saves the previous position in (m_ox, m_oy) before updating.
    void update(float targetX, float targetY, double dt);

    // Snap the camera instantly to centre on (x, y) with no lerp.
    // Use for room transitions so the viewport doesn't drift across the seam.
    void snapTo(float targetX, float targetY);

    // ── Query (call from render) ──────────────────────────────────────────────

    // Interpolated top-left world position for the current render frame.
    // alpha is the same blend factor passed to Game::render().
    // Returns the smooth sub-tick position rather than the last physics tick.
    float interpX(double alpha) const;
    float interpY(double alpha) const;

    // Integer versions — for passing to SDL draw calls and Tilemap::render().
    // Use these rather than casting interpX() yourself.
    int screenOffsetX(double alpha) const;
    int screenOffsetY(double alpha) const;

    // Convert a world-space position to a screen-space position.
    // Equivalent to worldX - screenOffsetX(alpha).
    int toScreenX(float worldX, double alpha) const;
    int toScreenY(float worldY, double alpha) const;

private:
    // Current physics-tick camera position (top-left of viewport in world px).
    float m_x = 0.f, m_y = 0.f;

    // Camera position at the previous physics tick — used for render lerp.
    float m_ox = 0.f, m_oy = 0.f;

    int m_viewW,  m_viewH;   // viewport dimensions in pixels
    int m_worldW, m_worldH;  // world dimensions in pixels (clamping bounds)

    // How quickly the camera catches up to the target.
    // Higher = snappier. 6.0 gives a responsive but smooth feel.
    // At 120 Hz each tick closes (6/120) = 5% of the remaining gap.
    static constexpr float LERP_SPEED = 6.f;

    // Clamp the camera so the viewport never shows outside the world bounds.
    void clamp();
};
