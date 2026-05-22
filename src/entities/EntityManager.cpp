// =============================================================================
// EntityManager.cpp
// =============================================================================

#include "entities/EntityManager.h"
#include "entities/Entity.h"
#include "world/Tilemap.h"
#include "core/AssetRegistry.h"
#include "world/Camera.h"
#include <SDL.h>

// =============================================================================
// Mutation
// =============================================================================

void EntityManager::add(std::unique_ptr<Entity> entity) {
    // Move the unique_ptr into the vector — the manager is now the sole owner.
    m_entities.push_back(std::move(entity));
}

// =============================================================================
// Update
// =============================================================================

void EntityManager::update(const Tilemap& tilemap, double dt) {
    for (auto& e : m_entities) {
        // Save current position as "old" before physics runs.
        // render() will lerp between the saved and the post-update position to
        // produce smooth sub-tick motion even when the display rate differs from
        // the physics rate.
        e->saveOldPosition();
        e->update(tilemap, dt);
    }
}

// =============================================================================
// Render
// =============================================================================

void EntityManager::render(SDL_Renderer* renderer,
                           const AssetRegistry& assets,
                           const Camera& camera,
                           double alpha) const
{
    // Draw in insertion order. For a small entity count this is fine; a layered
    // sort (by Y or by draw-layer enum) can be added later if needed.
    for (const auto& e : m_entities)
        e->render(renderer, assets, camera, alpha);
}
