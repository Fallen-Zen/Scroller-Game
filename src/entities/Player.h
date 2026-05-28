// =============================================================================
// Player.h — The player-controlled entity
//
// Player extends Entity with:
//   - Input reading (from InputManager) to set horizontal velocity and jump.
//   - Gravity integration and AABB tilemap collision (via Entity::resolveY/X).
//   - Sprite rendering: body + facing-direction eye indicator.
//
// Ownership:
//   - InputManager is owned by Game and outlives all entities. Player holds a
//     const reference — it does not own the input system.
//   - EntityManager owns the Player instance via unique_ptr<Entity>.
//   - Game holds a raw (non-owning) Player* so it can query centreX/Y for the
//     camera without downcasting through the EntityManager.
// =============================================================================

#pragma once
#include "entities/Entity.h"

// Forward declarations — full headers included in Player.cpp only.
class InputManager;

class Player : public Entity {
public:
    // Sprite dimensions: 32 × 48 pixels, matching the procedural texture in
    // AssetRegistry::makePlayer(). Passed to the Entity constructor as the AABB.
    static constexpr int WIDTH  = 32;
    static constexpr int HEIGHT = 48;

    // startX / startY — initial world position (top-left corner of the AABB).
    // input — the game's InputManager; Player reads it but does not own it.
    Player(float startX, float startY, const InputManager& input);

    // ── Entity interface ──────────────────────────────────────────────────────

    // Read input, apply physics, integrate position, resolve collisions.
    void update(const Tilemap& tilemap, double dt) override;

    // Draw the player body and facing-direction eye indicator.
    void render(SDL_Renderer* renderer,
                const AssetRegistry& assets,
                const Camera& camera,
                double alpha) const override;

    // ── Camera target ─────────────────────────────────────────────────────────

    // World-space centre of the player sprite — used by Camera::update() each
    // physics tick so the viewport stays centred on the player.
    float centreX() const { return this->m_posX + WIDTH * 0.5f; }
    float centreY() const { return this->m_posY + HEIGHT * 0.5f; }

    // True during the ATTACK_DURATION ticks following an attack input.
    // EntityManager queries this each tick to know whether to test the hitbox.
    bool isAttacking() const { return this->m_isAttacking; }

    // Returns the world-space rectangle the current swing occupies.
    // Positioned beside the player in the facing direction. Only meaningful
    // when isAttacking() is true.
    SDL_Rect attackHitbox() const;

private:
    // Non-owning reference to the game's shared input state.
    const InputManager& m_input;

    // ── Extended player state ─────────────────────────────────────────────────

    // True when the player last moved right. Persists when stopped so the
    // sprite holds its last direction instead of snapping to a default.
    bool m_facingRight = true;

    // Counts physics ticks since leaving the ground. Throttles airborne log
    // output to ~10 lines/second instead of 120.
    int m_airTicks = 0;

    // Vertical velocity from the previous tick. Compared against the current
    // m_velY to detect the jump arc peak (velY sign crossing 0 → positive).
    float m_prevVelY = 0.f;

    // True while the player is mid-swing. Set for ATTACK_DURATION ticks then cleared.
    bool m_isAttacking = false;

    // Ticks remaining in the current attack. Decrements each tick; attack ends at 0.
    int  m_attackTicks = 0;

    // ── Physics constants ─────────────────────────────────────────────────────

    // Downward acceleration in px/s². High value gives a snappy, arcade feel.
    static constexpr float GRAVITY    = 1800.f;

    // Horizontal movement speed while a direction key is held (px/s).
    static constexpr float MOVE_SPEED = 220.f;

    // Upward impulse applied when jumping. Negative because Y grows downward.
    static constexpr float JUMP_VEL   = -600.f;

    // Number of ticks the attack box stays active (~0.1 s at 120 Hz).
    static constexpr int ATTACK_DURATION = 12;

    // Attack box dimensions (pixels). Wider than tall — sword reach is
    // horizontal, not a tall vertical slash.
    static constexpr int ATTACK_W = 24;  // horizontal reach of the swing
    static constexpr int ATTACK_H = 32;  // vertical coverage of the swing
};
