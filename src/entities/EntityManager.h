// =============================================================================
// EntityManager.h — Owns and drives all active game entities
//
// EntityManager is the single point of truth for every live Entity in the
// game world. It:
//   - Takes exclusive ownership via unique_ptr (entities cannot outlive it).
//   - Drives the fixed-timestep update loop: saves each entity's previous
//     position (for render interpolation) then calls update().
//   - Drives rendering: calls each entity's render() in insertion order.
//
// Typical usage in Game:
//   m_entities.add(std::make_unique<Player>(...));
//   // In update():  m_entities.update(m_tilemap, dt);
//   // In render():  m_entities.render(renderer, assets, camera, alpha);
//
// Future extensions:
//   - remove(Entity*)  — mark dead entities for deferred cleanup
//   - query by type    — iterate only enemies, only projectiles, etc.
//   - spatial hash     — broad-phase collision culling for many entities
// =============================================================================

#pragma once
#include <memory>
#include <vector>
#include <cstddef>

// Forward declarations — full headers pulled in by EntityManager.cpp only.
class Entity;
class Tilemap;
class AssetRegistry;
class Camera;
struct SDL_Renderer;
class Player;

class EntityManager {
public:
    // Take exclusive ownership of an entity and append it to the active list.
    // The entity will be updated and rendered from this point forward.
    void add(std::unique_ptr<Entity> entity);

    // Advance all entities by dt seconds (the fixed physics timestep).
    // Calls saveOldPosition() on each entity before update() so that render()
    // has both the old and new positions available for interpolation.
    void update(const Tilemap& tilemap, const Player& player, double dt);

    // Draw all entities in insertion order.
    // alpha is the fractional tick blend factor passed down from Game::render().
    void render(SDL_Renderer* renderer,
                const AssetRegistry& assets,
                const Camera& camera,
                double alpha) const;

    // Number of entities currently managed.
    std::size_t count() const { return m_entities.size(); }

private:
    // Entities stored in insertion order. unique_ptr gives exclusive ownership
    // and ensures proper destruction even if a subclass has a non-trivial dtor.
    std::vector<std::unique_ptr<Entity>> m_entities;
};
