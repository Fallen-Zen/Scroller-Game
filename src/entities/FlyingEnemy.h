// =============================================================================
// FlyingEnemy.h — Airborne patrol enemy
//
// FlyingEnemy hovers in place with a sine-wave vertical oscillation and patrols
// horizontally back and forth. It ignores gravity entirely — its Y position is
// driven by the sine wave, not by physics integration.
//
// Behaviour:
//   - Moves horizontally at FLY_SPEED, bouncing off walls via resolveX().
//   - Y position oscillates around the spawn Y with amplitude HOVER_AMP and
//     period controlled by HOVER_FREQ. This gives a "bobbing" flight feel.
//   - No resolveY() call — the enemy passes over terrain freely.
//   - 2 HP — fewer than a WalkingEnemy since it's harder to hit mid-air.
// =============================================================================

#pragma once
#include "entities/Enemy.h"

class FlyingEnemy : public Enemy {
public:
    // Sprite: 24 × 20 px — wide and flat, suggesting a bat or hovering drone.
    static constexpr int W = 24;
    static constexpr int H = 20;

    // Spawns at (x, y). The Y coordinate becomes the centre of the hover arc.
    FlyingEnemy(float x, float y);

    void update(const Tilemap& tilemap, double dt) override;

    void render(SDL_Renderer* renderer,
                const AssetRegistry& assets,
                const Camera& camera,
                double alpha) const override;

private:
    // Y coordinate the enemy hovers around. The oscillation is applied relative
    // to this value so the enemy always returns to the same height.
    float m_baseY;

    // Current oscillation phase in radians. Increments each tick.
    float m_phase = 0.f;

    // Horizontal patrol speed (px/s).
    static constexpr float FLY_SPEED  = 55.f;

    // Vertical oscillation amplitude (px) and frequency (radians/s).
    static constexpr float HOVER_AMP  = 20.f;
    static constexpr float HOVER_FREQ = 2.5f;  // ~0.4 s per full bob
};
