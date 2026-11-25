#include "physics/player.h"
#include "world/chunk.h"

#include <GLFW/glfw3.h>

#include <cmath>

namespace {
constexpr float kEyeHeight = 1.62f;
constexpr float kGravity = 24.0f;
constexpr float kWalkSpeed = 4.5f;
constexpr float kSprintSpeed = 7.0f;
constexpr float kJumpSpeed = 8.5f;

bool IsSolidAt(const World& world, int x, int y, int z) {
    return IsCollidable(world.GetBlock(x, y, z));
}
} // namespace

Player::Player() {
    for (int i = 0; i < 36; ++i) inventory[i] = static_cast<uint8_t>(BlockType::Air);
    for (int i = 0; i < 9; ++i) hotbar[i] = static_cast<uint8_t>(BlockType::Air);
    hotbar[0] = static_cast<uint8_t>(BlockType::Dirt);
    hotbar[1] = static_cast<uint8_t>(BlockType::Stone);
    hotbar[2] = static_cast<uint8_t>(BlockType::Planks);
    hotbar[3] = static_cast<uint8_t>(BlockType::Emberstone);
    hotbar[4] = static_cast<uint8_t>(BlockType::Ashsoil);
    hotbar[5] = static_cast<uint8_t>(BlockType::Lumenstone);
    hotbar[6] = static_cast<uint8_t>(BlockType::Voidstone);
    hotbar[7] = static_cast<uint8_t>(BlockType::Portal);
    hotbar[8] = static_cast<uint8_t>(BlockType::CrimsonWood);
    for (int i = 0; i < 9; ++i) {
        inventory[i] = hotbar[i];
    }
}

void Player::Update(float dt, const InputState& input, const World& world) {
    if (!onGround) {
        vy -= kGravity * dt;
    }

    // Movement in the XZ plane relative to yaw.
    const float forwardX = std::cos(yaw);
    const float forwardZ = std::sin(yaw);
    const float rightX = -std::sin(yaw);
    const float rightZ = std::cos(yaw);

    float wx = 0.0f;
    float wz = 0.0f;
    if (input.keys[GLFW_KEY_W]) { wx += forwardX; wz += forwardZ; }
    if (input.keys[GLFW_KEY_S]) { wx -= forwardX; wz -= forwardZ; }
    if (input.keys[GLFW_KEY_D]) { wx += rightX; wz += rightZ; }
    if (input.keys[GLFW_KEY_A]) { wx -= rightX; wz -= rightZ; }

    const float len = std::sqrt(wx * wx + wz * wz);
    if (len > 0.0001f) {
        wx /= len;
        wz /= len;
    }

    const float speed = input.keys[GLFW_KEY_LEFT_SHIFT] ? kSprintSpeed : kWalkSpeed;
    vx = wx * speed;
    vz = wz * speed;

    if (input.keys[GLFW_KEY_SPACE] && onGround) {
        vy = kJumpSpeed;
        onGround = false;
    }

    // Horizontal move with per-axis collision resolution.
    const int feetY = static_cast<int>(std::floor(y));
    const int headY = static_cast<int>(std::floor(y)) + 1;

    float newX = x + vx * dt;
    int bx = static_cast<int>(std::floor(newX + (vx > 0.0f ? 0.3f : -0.3f)));
    if (!IsSolidAt(world, bx, feetY, static_cast<int>(std::floor(z))) &&
        !IsSolidAt(world, bx, headY, static_cast<int>(std::floor(z)))) {
        x = newX;
    }

    float newZ = z + vz * dt;
    int bz = static_cast<int>(std::floor(newZ + (vz > 0.0f ? 0.3f : -0.3f)));
    if (!IsSolidAt(world, static_cast<int>(std::floor(x)), feetY, bz) &&
        !IsSolidAt(world, static_cast<int>(std::floor(x)), headY, bz)) {
        z = newZ;
    }

    // Vertical move.
    y += vy * dt;
    if (y < 0.0f) {
        y = 0.0f;
        vy = 0.0f;
        onGround = true;
    }

    // Land on the highest solid block under the player.
    const int px = static_cast<int>(std::floor(x));
    const int pz = static_cast<int>(std::floor(z));
    const int scan = static_cast<int>(std::floor(y));
    for (int by = scan; by >= 0 && by > scan - 3; --by) {
        if (IsSolidAt(world, px, by, pz)) {
            const float top = static_cast<float>(by + 1);
            if (y <= top + 0.2f && vy <= 0.0f) {
                y = top;
                vy = 0.0f;
                onGround = true;
            }
            break;
        }
    }
}

bool Player::Raycast(const World& world, float maxDist, int& outX, int& outY, int& outZ, BlockType& outType) const {
    const float dirX = std::cos(pitch) * std::cos(yaw);
    const float dirY = std::sin(pitch);
    const float dirZ = std::cos(pitch) * std::sin(yaw);

    const float eyeX = x;
    const float eyeY = y + kEyeHeight;
    const float eyeZ = z;

    int prevX = static_cast<int>(std::floor(eyeX));
    int prevY = static_cast<int>(std::floor(eyeY));
    int prevZ = static_cast<int>(std::floor(eyeZ));

    for (float t = 0.0f; t <= maxDist; t += 0.05f) {
        const int bx = static_cast<int>(std::floor(eyeX + dirX * t));
        const int by = static_cast<int>(std::floor(eyeY + dirY * t));
        const int bz = static_cast<int>(std::floor(eyeZ + dirZ * t));

        const BlockType type = world.GetBlock(bx, by, bz);
        if (type != BlockType::Air && type != BlockType::Water) {
            outX = bx;
            outY = by;
            outZ = bz;
            outType = type;
            // Leave the previous (empty) cell available through outX/outY/outZ
            // callers that need it can recompute one step back.
            (void)prevX; (void)prevY; (void)prevZ;
            return true;
        }
        prevX = bx; prevY = by; prevZ = bz;
    }
    return false;
}

bool Player::BreakBlock(World& world) {
    int bx = 0, by = 0, bz = 0;
    BlockType type = BlockType::Air;
    if (!Raycast(world, 6.0f, bx, by, bz, type)) {
        return false;
    }
    if (type == BlockType::Bedrock) {
        return false;
    }
    world.SetBlock(bx, by, bz, BlockType::Air);
    return true;
}

bool Player::PlaceBlock(World& world, BlockType type, int* outX, int* outY, int* outZ) {
    int bx = 0, by = 0, bz = 0;
    BlockType hit = BlockType::Air;
    if (!Raycast(world, 6.0f, bx, by, bz, hit)) {
        return false;
    }

    // Place against the face the ray entered: step a little further back than the hit.
    const float dirX = std::cos(pitch) * std::cos(yaw);
    const float dirY = std::sin(pitch);
    const float dirZ = std::cos(pitch) * std::sin(yaw);
    const float eyeX = x;
    const float eyeY = y + kEyeHeight;
    const float eyeZ = z;

    for (float t = 0.0f; t <= 6.0f; t += 0.05f) {
        const int cx = static_cast<int>(std::floor(eyeX + dirX * t));
        const int cy = static_cast<int>(std::floor(eyeY + dirY * t));
        const int cz = static_cast<int>(std::floor(eyeZ + dirZ * t));
        if (cx == bx && cy == by && cz == bz) {
            // One step before the hit cell.
            const float bt = (t > 0.1f) ? (t - 0.1f) : 0.0f;
            const int px = static_cast<int>(std::floor(eyeX + dirX * bt));
            const int py = static_cast<int>(std::floor(eyeY + dirY * bt));
            const int pz = static_cast<int>(std::floor(eyeZ + dirZ * bt));
            if (world.GetBlock(px, py, pz) == BlockType::Air) {
                world.SetBlock(px, py, pz, type);
                if (outX) *outX = px;
                if (outY) *outY = py;
                if (outZ) *outZ = pz;
                return true;
            }
            return false;
        }
    }
    return false;
}
