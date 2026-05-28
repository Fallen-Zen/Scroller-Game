// =============================================================================
// Camera.cpp
// =============================================================================

#include "world/Camera.h"
#include <algorithm>  // std::clamp

// =============================================================================
// Construction
// =============================================================================

Camera::Camera(int viewW, int viewH, int worldW, int worldH)
    : m_viewW(viewW), m_viewH(viewH)
    , m_worldW(worldW), m_worldH(worldH)
{
    // Start at the top-left of the world so the first frame isn't blank.
    m_x = m_ox = 0.f;
    m_y = m_oy = 0.f;
}

// =============================================================================
// Update
// =============================================================================

void Camera::update(float targetX, float targetY, double dt) {
    // Save current position before moving — render() will blend between
    // (m_ox, m_oy) and (m_x, m_y) using the frame's alpha value.
    m_ox = m_x;
    m_oy = m_y;

    // Compute where the camera wants to be: centred on the target.
    // Subtract half the viewport so the target lands in the middle of the screen.
    float desiredX = targetX - static_cast<float>(m_viewW) * 0.5f;
    float desiredY = targetY - static_cast<float>(m_viewH) * 0.5f;

    // Lerp (exponential smoothing): each tick we close a fraction of the
    // remaining gap. The fraction is (LERP_SPEED × dt).
    //
    //   new = current + (desired - current) * speed * dt
    //
    // At LERP_SPEED=6, dt=1/120: each tick closes 6/120 = 5% of the gap.
    // After ~0.5s the camera has covered >95% of a large pan — feels snappy
    // but not jarring. Tune LERP_SPEED in Camera.h to taste.
    float t = static_cast<float>(LERP_SPEED * dt);
    m_x += (desiredX - m_x) * t;
    m_y += (desiredY - m_y) * t;

    clamp();
}

void Camera::snapTo(float targetX, float targetY) {
    m_x  = m_ox = targetX - static_cast<float>(m_viewW) * 0.5f;
    m_y  = m_oy = targetY - static_cast<float>(m_viewH) * 0.5f;
    clamp();
}

// =============================================================================
// Query
// =============================================================================

float Camera::interpX(double alpha) const {
    // Blend between previous and current position using the render alpha.
    // This is identical to how the player position is interpolated in render().
    return static_cast<float>(m_ox + (m_x - m_ox) * alpha);
}

float Camera::interpY(double alpha) const {
    return static_cast<float>(m_oy + (m_y - m_oy) * alpha);
}

int Camera::screenOffsetX(double alpha) const {
    return static_cast<int>(interpX(alpha));
}

int Camera::screenOffsetY(double alpha) const {
    return static_cast<int>(interpY(alpha));
}

int Camera::toScreenX(float worldX, double alpha) const {
    return static_cast<int>(worldX - interpX(alpha));
}

int Camera::toScreenY(float worldY, double alpha) const {
    return static_cast<int>(worldY - interpY(alpha));
}

// =============================================================================
// Private helpers
// =============================================================================

void Camera::clamp() {
    // Prevent the camera from showing past the left or top of the world.
    // Prevent the camera from showing past the right or bottom either.
    // The right/bottom boundary is worldSize - viewportSize: if the world is
    // 3200px wide and the viewport is 1280px, the camera's max X is 1920.
    float maxX = static_cast<float>(m_worldW - m_viewW);
    float maxY = static_cast<float>(m_worldH - m_viewH);

    m_x = std::clamp(m_x, 0.f, maxX);
    m_y = std::clamp(m_y, 0.f, maxY);
}
