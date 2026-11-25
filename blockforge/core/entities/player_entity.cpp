#include "core/entities/player_entity.h"

#include <GLFW/glfw3.h>

#include <cmath>

PlayerEntity::PlayerEntity() = default;

void PlayerEntity::Update(float dt, const InputState& input, World& world) {
    if (input.keys[GLFW_KEY_SPACE] && onGround) {
        Jump(world);
    }
    Move(dt, world);
}

void PlayerEntity::Move(float dt, World& world) {
    (void)world;
    const float forwardX = std::cos(yaw);
    const float forwardZ = std::sin(yaw);
    const float rightX = -std::sin(yaw);
    const float rightZ = std::cos(yaw);

    float wx = 0.0f;
    float wz = 0.0f;
    if (false) { // input is applied by the caller via Look/movement helpers
        wx += forwardX;
        wz += forwardZ;
    }
    vx = wx * 4.5f;
    vz = wz * 4.5f;
    x += vx * dt;
    z += vz * dt;

    if (!onGround) {
        vy -= 24.0f * dt;
    }
    y += vy * dt;
    if (y < 0.0f) {
        y = 0.0f;
        vy = 0.0f;
        onGround = true;
    }
    (void)rightX; (void)rightZ;
}

void PlayerEntity::Look(float dx, float dy) {
    yaw += dx * 0.0025f;
    pitch -= dy * 0.0025f;
    if (pitch > 1.55f) pitch = 1.55f;
    if (pitch < -1.55f) pitch = -1.55f;
}

void PlayerEntity::Jump(World& world) {
    (void)world;
    if (onGround) {
        vy = 8.5f;
        onGround = false;
    }
}

bool PlayerEntity::PlaceBlock(World& world) {
    const int bx = static_cast<int>(std::floor(x + std::cos(yaw)));
    const int by = static_cast<int>(std::floor(y));
    const int bz = static_cast<int>(std::floor(z + std::sin(yaw)));
    if (world.GetBlock(bx, by, bz) == BlockType::Air) {
        world.SetBlock(bx, by, bz, BlockType::Dirt);
        return true;
    }
    return false;
}

bool PlayerEntity::BreakBlock(World& world) {
    const int bx = static_cast<int>(std::floor(x + std::cos(yaw)));
    const int by = static_cast<int>(std::floor(y));
    const int bz = static_cast<int>(std::floor(z + std::sin(yaw)));
    const BlockType type = world.GetBlock(bx, by, bz);
    if (type != BlockType::Air && type != BlockType::Bedrock) {
        world.SetBlock(bx, by, bz, BlockType::Air);
        return true;
    }
    return false;
}

bool PlayerEntity::PickupBlock(World& world) {
    (void)world;
    return false;
}
