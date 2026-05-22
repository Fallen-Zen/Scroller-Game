// =============================================================================
// Entity.h — Abstract base class for all game objects
//
// An Entity is anything in the game world that has a position, a velocity, and
// needs to update and render itself each tick. The Player, enemies, projectiles,
// and pickups will all be Entities.
//
// Design principles:
//   - Entity owns only the state that every game object shares: position,
//     velocity, AABB dimensions, and ground-contact flag.
//   - Collision resolution (resolveX / resolveY) lives here as protected
//     helpers so every subclass gets correct AABB-vs-tilemap physics for free.
//   - update() and render() are pure virtual — subclasses decide behaviour.
//   - EntityManager owns entities via unique_ptr. Entity is non-copyable.
// =============================================================================

#pragma once
#include <SDL.h>   // SDL_Renderer (used in render signatures)

// Forward declarations — headers are included in the .cpp files only.
class Tilemap;
class AssetRegistry;
class Camera;

class Entity {
public:
    // x, y  — initial world-space position of the top-left corner (pixels).
    // w, h  — axis-aligned bounding box dimensions (pixels).
    Entity(float x, float y, int w, int h);
    virtual ~Entity() = default;

    // Entities are owned by unique_ptr in EntityManager.
    // Copying would duplicate SDL handles or shared references — never valid.
    Entity(const Entity&)            = delete;
    Entity& operator=(const Entity&) = delete;

    // ── Lifecycle (called by EntityManager) ───────────────────────────────────

    // Snapshot the current position as the "previous" position before the next
    // physics tick. EntityManager calls this before update() so that render()
    // can interpolate between the two positions for sub-tick smooth motion.
    void saveOldPosition();

    // Advance this entity's simulation by dt seconds.
    // Subclasses read input, apply forces, integrate position, and resolve
    // collisions in this method. dt is always the fixed timestep (~1/120 s).
    virtual void update(const Tilemap& tilemap, double dt) = 0;

    // Draw this entity for the current render frame.
    // alpha is the interpolation factor in [0,1] between the previous physics
    // tick (alpha=0) and the current one (alpha=1). Use it to blend between
    // (m_ox, m_oy) and (m_px, m_py) for visually smooth motion.
    virtual void render(SDL_Renderer* renderer,
                        const AssetRegistry& assets,
                        const Camera& camera,
                        double alpha) const = 0;

    // ── Accessors ─────────────────────────────────────────────────────────────

    float x()        const { return m_px; }
    float y()        const { return m_py; }
    int   width()    const { return m_w; }
    int   height()   const { return m_h; }
    bool  onGround() const { return m_onGround; }

protected:
    // ── Physics state ─────────────────────────────────────────────────────────

    // Current world-space position (top-left of the AABB), in pixels.
    float m_px, m_py;

    // Position at the start of the previous physics tick.
    // Kept in sync by saveOldPosition(). render() blends between these and
    // the current position using the frame's alpha value.
    float m_ox, m_oy;

    // Current velocity in pixels per second (signed: positive = right / down).
    float m_vx, m_vy;

    // Axis-aligned bounding box. Set once in the constructor, not changed.
    int m_w, m_h;

    // True when this entity is resting on a solid surface or platform.
    // Set to true by resolveY() when a floor collision is detected.
    // Reset to false at the top of resolveY() each tick.
    bool m_onGround = false;

    // ── Collision helpers ─────────────────────────────────────────────────────
    //
    // Both methods must be called AFTER integrating the corresponding axis.
    // resolveY() must be called before resolveX() within the same tick so the
    // floor tile never appears inside the horizontal scan range (see Game.cpp
    // for the detailed explanation of the Y-first ordering).

    // Snap this entity out of horizontal (wall) tile overlaps.
    // Checks the tile column at the leading horizontal edge and snaps back if
    // any tile in the vertical body span is solid.
    void resolveX(const Tilemap& tilemap);

    // Snap this entity out of vertical (floor / ceiling) tile overlaps.
    // Handles solid tiles and one-way platforms. Updates m_onGround.
    void resolveY(const Tilemap& tilemap);
};
