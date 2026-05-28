// =============================================================================
// Enemy.h — Abstract base class for all enemy types
//
// Enemy sits between Entity and the concrete enemy types in the hierarchy:
//
//   Entity  (position, velocity, AABB, collision resolution)
//     └── Enemy  (health, faction, facing direction)
//           ├── WalkingEnemy  (patrol + gravity)
//           └── FlyingEnemy   (sine-wave hover + horizontal patrol)
//
// What Enemy adds over Entity:
//   - Health: m_hp / m_maxHp, takeDamage(), isDead()
//   - Facing direction: m_facingRight (shared by all enemy subtypes)
//   - Common gravity constant (WalkingEnemy uses it; FlyingEnemy ignores it)
//
// What Enemy does NOT do:
//   - update() and render() remain pure virtual — each subtype has its own
//     movement AI and appearance.
//   - No InputManager dependency — enemies are driven by AI, not the player.
// =============================================================================

#pragma once
#include "entities/Entity.h"

class Enemy : public Entity {
public:
    // x, y     — initial world position (top-left of AABB).
    // w, h     — bounding-box dimensions (set by each concrete subtype).
    // hp       — starting and maximum hit points.
    Enemy(float x, float y, int w, int h, int hp);

    virtual ~Enemy() = default;

    // ── Health ────────────────────────────────────────────────────────────────

    // Reduce HP by amount. Clamped to 0 — HP never goes negative.
    void takeDamage(int amount);

    bool isDead()  const { return m_hp <= 0; }
    int  hp()      const { return m_hp; }
    int  maxHp()   const { return m_maxHp; }

    // Returns the world-space AABB used for combat hit detection.
    SDL_Rect hitbox() const;

    // ── Entity interface (still pure virtual) ─────────────────────────────────

    void update(const Tilemap& tilemap, double dt) override = 0;

    void render(SDL_Renderer* renderer,
                const AssetRegistry& assets,
                const Camera& camera,
                double alpha) const override = 0;

protected:
    int  m_hp;
    int  m_maxHp;

    // Direction the enemy last moved. Subtypes use it to:
    //   - flip the sprite when reversing direction
    //   - decide which side to probe for walls / ledge edges
    bool m_facingRight = true;

    // Gravity shared by ground-based enemies. Flying enemies ignore it.
    static constexpr float GRAVITY = 1800.f;
};
