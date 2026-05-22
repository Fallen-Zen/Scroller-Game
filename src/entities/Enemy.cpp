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
    m_hp = std::max(0, m_hp - amount);
}
