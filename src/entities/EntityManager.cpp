// =============================================================================
// EntityManager.cpp
// =============================================================================

#include "entities/EntityManager.h"
#include "entities/Entity.h"
#include "entities/Enemy.h"
#include "entities/Player.h"
#include "world/Tilemap.h"
#include "core/AssetRegistry.h"
#include "core/Logger.h"
#include "world/Camera.h"
#include <SDL.h>
#include <algorithm>  // std::remove_if

// =============================================================================
// Mutation
// =============================================================================

void EntityManager::add(std::unique_ptr<Entity> entity) {
    // Move the unique_ptr into the vector — the manager is now the sole owner.
    this->m_entities.push_back(std::move(entity));
}

// =============================================================================
// Update
// =============================================================================

void EntityManager::update(const Tilemap& tilemap, const Player& player, double dt) {
    for (auto& e : this->m_entities) {
        // Save current position as "old" before physics runs.
        // render() will lerp between the saved and the post-update position to
        // produce smooth sub-tick motion even when the display rate differs from
        // the physics rate.
        e->saveOldPosition();
        e->update(tilemap, dt);
    }

    // ── Hit detection ─────────────────────────────────────────────────────────
    // Only runs while the player's attack box is active. Each overlapping enemy
    // takes 1 point of damage per tick the boxes intersect. The hit is logged
    // for debugging — remove the LOG_DEBUG call before shipping.
    if (player.isAttacking()) {
        SDL_Rect hb = player.attackHitbox();
        for (auto& e : this->m_entities) {
            Enemy* enemy = dynamic_cast<Enemy*>(e.get());
            if (enemy && !enemy->isDead()) {
                const SDL_Rect eh = enemy->hitbox();
                const bool overlaps =
                    eh.x         < hb.x + hb.w &&
                    eh.x + eh.w  > hb.x        &&
                    eh.y         < hb.y + hb.h &&
                    eh.y + eh.h  > hb.y;

                if (overlaps) {
                    enemy->takeDamage(1);
                    LOG_DEBUG("Hit enemy | hp=%d/%d", enemy->hp(), enemy->maxHp());
                }
            }
        }
    }

    // ── Dead entity removal ───────────────────────────────────────────────────
    // Erase-remove: moves all dead enemies to the end of the vector, then erases
    // them in one allocation. Safe to call every tick — does nothing when no
    // enemies are dead.
    this->m_entities.erase(
        std::remove_if(this->m_entities.begin(), this->m_entities.end(),
            [](const std::unique_ptr<Entity>& e) {
                const Enemy* enemy = dynamic_cast<const Enemy*>(e.get());
                return enemy && enemy->isDead();
            }),
        this->m_entities.end()
    );
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
    for (const auto& e : this->m_entities)
        e->render(renderer, assets, camera, alpha);
}
