// =============================================================================
// Enemy.cpp
// =============================================================================

#include "entities/Enemy.h"
#include <algorithm>  // std::max

Enemy::Enemy(float x, float y, int w, int h, int hp)
    : Entity(x, y, w, h)
    , m_hp(hp)
    , m_maxHp(hp)
{}

void Enemy::takeDamage(int amount) {
    // Clamp to 0 so isDead() is a simple zero-check.
    this->m_hp = std::max(0, this->m_hp - amount);
}

SDL_Rect Enemy::hitbox() const {
    return SDL_Rect{
        static_cast<int>(this->m_posX),
        static_cast<int>(this->m_posY),
        this->m_width,
        this->m_height
    };
}
