#include "player.h"
#include <GLFW/glfw3.h>
#include <cmath>

void Player::Update(float dt, const bool keys[], int mouseButtons[]) {
    float speed = isSprinting ? sprintSpeed : walkSpeed;

    if (isFlying) {
        if (keys[GLFW_KEY_W]) { vz += cosf(yaw) * speed * dt; vx += sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_S]) { vz -= cosf(yaw) * speed * dt; vx -= sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_A]) { vx -= cosf(yaw) * speed * dt; vz += sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_D]) { vx += cosf(yaw) * speed * dt; vz -= sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_SPACE]) vy += speed * dt;
        if (keys[GLFW_KEY_LEFT_SHIFT]) vy -= speed * dt;
    } else {
        if (keys[GLFW_KEY_W]) { vz += cosf(yaw) * speed * dt; vx += sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_S]) { vz -= cosf(yaw) * speed * dt; vx -= sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_A]) { vx -= cosf(yaw) * speed * dt; vz += sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_D]) { vx += cosf(yaw) * speed * dt; vz -= sinf(yaw) * speed * dt; }
        if (keys[GLFW_KEY_SPACE] && onGround) {
            vy = jumpVelocity;
            onGround = false;
        }
        vy += gravity * dt;
    }
}

void Player::Move(float dt, World& world) {
    x += vx * dt;
    y += vy * dt;
    z += vz * dt;

    onGround = false;
    int bx = (int)floorf(x);
    int by = (int)floorf(y - height + 0.1f);
    int bz = (int)floorf(z);

    BlockType below = world.GetBlock(bx, by, bz);
    if (below != BlockType::Air && below != BlockType::Water) {
        if (vy <= 0) {
            y = by + 1 + height - 0.1f;
            vy = 0;
            onGround = true;
        }
    }

    if (y < 0) { y = 0; vy = 0; onGround = true; }
}

void Player::Jump() {
    if (onGround && !isFlying) {
        vy = jumpVelocity;
        onGround = false;
    }
}

void Player::Look(float dx, float dy) {
    yaw += dx * 0.002f;
    pitch -= dy * 0.002f;
    if (pitch > 1.5f) pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;
}

bool Player::PlaceBlock(World& world, const int mouseButtons[]) {
    if (!mouseButtons[1]) return false;
    float dx = -sinf(yaw) * cosf(pitch);
    float dy = sinf(pitch);
    float dz = -cosf(yaw) * cosf(pitch);

    for (int i = 1; i <= 5; ++i) {
        float px = x + dx * i;
        float py = y - 0.5f + dy * i;
        float pz = z + dz * i;
        int bx = (int)floorf(px);
        int by = (int)floorf(py);
        int bz = (int)floorf(pz);

        if (world.GetBlock(bx, by, bz) == BlockType::Air) {
            InventorySlot& slot = inventory[hotbarIndex];
            if (slot.type != BlockType::Air && slot.count > 0) {
                world.SetBlock(bx, by, bz, slot.type);
                slot.count--;
                return true;
            }
        }
    }
    return false;
}

bool Player::BreakBlock(World& world, const int mouseButtons[]) {
    if (!mouseButtons[0]) return false;
    float dx = -sinf(yaw) * cosf(pitch);
    float dy = sinf(pitch);
    float dz = -cosf(yaw) * cosf(pitch);

    for (int i = 1; i <= 5; ++i) {
        float px = x + dx * i;
        float py = y - 0.5f + dy * i;
        float pz = z + dz * i;
        int bx = (int)floorf(px);
        int by = (int)floorf(py);
        int bz = (int)floorf(pz);

        BlockType bt = world.GetBlock(bx, by, bz);
        if (bt != BlockType::Air && bt != BlockType::Bedrock) {
            world.SetBlock(bx, by, bz, BlockType::Air);
            for (int s = 0; s < MAX_INVENTORY; ++s) {
                if (inventory[s].type == bt && inventory[s].count < 64) {
                    inventory[s].count++;
                    return true;
                }
            }
            for (int s = 0; s < MAX_INVENTORY; ++s) {
                if (inventory[s].type == BlockType::Air) {
                    inventory[s].type = bt;
                    inventory[s].count = 1;
                    return true;
                }
            }
        }
    }
    return false;
}

void Player::CycleHotbar(int direction) {
    hotbarIndex = (hotbarIndex + direction + HOTBAR_SLOTS) % HOTBAR_SLOTS;
}

void Player::SetHotbar(int index) {
    if (index >= 0 && index < HOTBAR_SLOTS) hotbarIndex = index;
}   