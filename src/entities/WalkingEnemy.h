// =============================================================================
// WalkingEnemy.h — Ground-based patrol enemy
//
// WalkingEnemy walks back and forth along whatever surface it stands on.
// It reverses direction when it hits a wall or detects a ledge ahead.
//
// Behaviour:
//   - Moves at WALK_SPEED in the current facing direction.
//   - Before stepping, probes the tile below the leading foot. If that tile is
//     air (no floor ahead), the enemy turns around — it won't walk off ledges.
//   - After integrating X, if resolveX() zeroed the velocity the enemy hit a
//     wall and flips direction for the next tick.
//   - Gravity is applied every tick so the enemy falls into pits, sticks to
//     slopes, and lands correctly on platforms.
// =============================================================================

#pragma once
#include "entities/Enemy.h"

class WalkingEnemy : public Enemy {
public:
    // Sprite: 28 × 32 px — slightly smaller and squatter than the player.
    static constexpr int W = 28;
    static constexpr int H = 32;

    // Spawns at (x, y) facing right with 3 HP.
    WalkingEnemy(float x, float y);

    void update(const Tilemap& tilemap, double dt) override;

    void render(SDL_Renderer* renderer,
                const AssetRegistry& assets,
                const Camera& camera,
                double alpha) const override;

private:
    // Horizontal patrol speed (px/s). Slower than the player so it's beatable.
    static constexpr float WALK_SPEED = 75.f;
};
