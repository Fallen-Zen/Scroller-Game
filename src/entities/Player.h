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
    static constexpr int W = 32;
    static constexpr int H = 48;

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
    float centreX() const { return m_px + W * 0.5f; }
    float centreY() const { return m_py + H * 0.5f; }

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
    // m_vy to detect the jump arc peak (vy sign crossing 0 → positive).
    float m_prevVy = 0.f;

    // ── Physics constants ─────────────────────────────────────────────────────

    // Downward acceleration in px/s². High value gives a snappy, arcade feel.
    static constexpr float GRAVITY    = 1800.f;

    // Horizontal movement speed while a direction key is held (px/s).
    static constexpr float MOVE_SPEED = 220.f;

    // Upward impulse applied when jumping. Negative because Y grows downward.
    static constexpr float JUMP_VEL   = -600.f;
};
