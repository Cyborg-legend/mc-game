#include "physics/player.h"
#include "world/chunk.h"
#include <cmath>

Player::Player() : x(0.0f), y(0.0f), z(0.0f), vx(0.0f), vy(0.0f), vz(0.0f),
    onGround(false), health(20) {
    // Initialize inventory to Air (0)
    for (int i = 0; i < 36; ++i) inventory[i] = (uint8_t)BlockType::Air;
    for (int i = 0; i < 9; ++i) hotbar[i] = (uint8_t)BlockType::Air;
    selectedSlot = 0;
}

void Player::Update(float dt, const InputState& input) {
    // Apply gravity
    if (!onGround) {
        vy -= 20.0f * dt; // Gravity
    }

    // Movement direction
    float dx = 0.0f, dz = 0.0f;
    float speed = 4.0f; // Normal walk speed

    if (input.keys[GLFW_KEY_LEFT_SHIFT]) {
        speed = 6.0f; // Sprint speed
    }

    if (input.keys[GLFW_KEY_W]) {
        dx += std::sinf(0.0f) * speed; // Simplified - would use actual yaw
        dz += -std::cosf(0.0f) * speed;
    }
    if (input.keys[GLFW_KEY_S]) {
        dx -= std::sinf(0.0f) * speed;
        dz -= -std::cosf(0.0f) * speed;
    }
    if (input.keys[GLFW_KEY_A]) {
        dx += std::cosf(0.0f) * speed; // Strafe left
        dz += std::sinf(0.0f) * speed;
    }
    if (input.keys[GLFW_KEY_D]) {
        dx -= std::cosf(0.0f) * speed; // Strafe right
        dz -= std::sinf(0.0f) * speed;
    }

    // Normalize direction if moving diagonally
    float len = std::sqrtf(dx * dx + dz * dz);
    if (len > 0.001f) {
        dx /= len;
        dz /= len;
    }

    // Apply movement
    vx = dx;
    vz = dz;

    // Jump
    if (input.keys[GLFW_KEY_SPACE] && onGround) {
        vy = 8.0f; // Jump velocity
        onGround = false;
    }

    // Simple position update (no collision resolution in this basic version)
    x += vx * dt;
    z += vz * dt;

    // Keep player at y=0 or above if on ground
    if (y <= 0.0f && onGround) {
        y = 0.0f;
    } else if (!onGround) {
        y += vy * dt;
    }
}

bool Player::Raycast(const World& world, float maxDist, float& outX, float& outY, float& outZ, BlockType& outType) {
    // Simple raycast from player position in direction of look
    // For now, just raycast along +Y direction (straight up for debugging)
    // In a full implementation, this would use the player's view direction

    // Start at player position
    float rx = x, ry = y, rz = z;

    return false;
}

bool Player::PlaceBlock(World& world, int mouseButton) {
    // Place block logic
    return false;
}

bool Player::BreakBlock(World& world) {
    // Break block logic
    return false;
}
