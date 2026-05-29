// =============================================================================
// EntityManager.cpp
// =============================================================================

#include "entities/EntityManager.h"
#include "entities/Entity.h"
#include "entities/Enemy.h"
#include "entities/Player.h"
#include "world/Tilemap.h"
#include "core/AssetRegistry.h"
#include "core/AudioManager.h"
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

void EntityManager::update(const Tilemap& tilemap, Player& player, AudioManager& audio, double dt) {
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
    // takes 1 point of damage per tick the boxes intersect.
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

    // ── Contact damage ────────────────────────────────────────────────────────
    // Any enemy body overlapping the player deals 1 damage. Player iframes
    // prevent this from firing more than once per IFRAMES_DURATION ticks.
    {
        const SDL_Rect ph {
            static_cast<int>(player.x()),
            static_cast<int>(player.y()),
            player.width(),
            player.height()
        };
        for (auto& e : this->m_entities) {
            Enemy* enemy = dynamic_cast<Enemy*>(e.get());
            if (enemy && !enemy->isDead()) {
                const SDL_Rect eh = enemy->hitbox();
                const bool overlaps =
                    eh.x        < ph.x + ph.w &&
                    eh.x + eh.w > ph.x        &&
                    eh.y        < ph.y + ph.h &&
                    eh.y + eh.h > ph.y;

                if (overlaps) {
                    player.takeDamage(1);
                    LOG_DEBUG("Player hit | hp=%d/%d", player.hp(), player.maxHp());
                }
            }
        }
    }

    // ── Dead entity removal ───────────────────────────────────────────────────
    // Play the death sound for each enemy about to be removed, then erase them.
    // Erase-remove moves dead enemies to the end of the vector and erases in
    // one allocation. Safe to call every tick — no-op when no enemies are dead.
    for (auto& e : this->m_entities) {
        const Enemy* enemy = dynamic_cast<const Enemy*>(e.get());
        if (enemy && enemy->isDead())
            audio.play(SoundID::EnemyDeath);
    }

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
