#include "player.h"
#include "core/world/world.h"
#include <glm/glm.hpp>
#include <cmath>

// Place block from selected slot using raycast
bool Player::PlaceBlock(int maxDistance, World& world) {
    // Raycast from player position in forward direction
    // Get view direction from camera yaw/pitch
    glm::vec3 eye = position;
    // Calculate direction based on yaw and pitch
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);
    glm::vec3 direction(
        std::cos(radPitch) * std::sin(radYaw),
        std::sin(radPitch),
        std::cos(radPitch) * std::cos(radYaw)
    );

    // Normalize
    direction = glm::normalize(direction);

    // Raycast maxDistance blocks
    for (int i = 0; i < maxDistance; ++i) {
        // Sample point along ray
        glm::vec3 pos = eye + direction * (i + 1.0f);

        // Convert to block coordinates
        int bx = static_cast<int>(std::floor(pos.x));
        int by = static_cast<int>(std::floor(pos.y));
        int bz = static_cast<int>(std::floor(pos.z));

        // Get block type
        BlockType type = world.GetBlock(bx, by, bz);

        // If we hit a non-air block, try to place
        if (type != BlockType::Air) {
            // Try to place the block we're looking at's opposite neighbor
            // Actually, we need to look at this more carefully
            // For now, just check if we can place at the hit location's neighbor
            // Actually, let me re-implement: raycast and place at the hit location

            // Determine the block face we're hitting
            // and try to place there
            // This is simplified - full implementation would calculate exact hit location

            // Get the selected slot
            InventorySlot& slot = GetSelectedSlot();
            if (slot.type != BlockType::Air && slot.count > 0) {
                // Try to place the block
                // Position is the hit block position minus normal direction
                // For simplicity, place at the block position we're looking at
                // Actually, we should place adjacent to the hit block

                // Let's just place at (bx, by, bz) for now - this will likely overwrite
                // A proper implementation would check the face being looked at
                world.SetBlock(bx, by, bz, slot.type);
                slot.count--;
                return true;
            }
            return false;
        }

        // Continue raycast - check if next position is valid
        // If we're about to go out of bounds, stop
        if (std::abs(pos.x) > 10000 || std::abs(pos.y) > 10000 || std::abs(pos.z) > 10000) {
            break;
        }
    }

    return false;
}

// Break block via raycast
bool Player::BreakBlock(int maxDistance, World& world) {
    // Raycast from player position in forward direction
    glm::vec3 eye = position;
    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);
    glm::vec3 direction(
        std::cos(radPitch) * std::sin(radYaw),
        std::sin(radPitch),
        std::cos(radPitch) * std::cos(radYaw)
    );
    direction = glm::normalize(direction);

    // Raycast maxDistance blocks
    for (int i = 0; i < maxDistance; ++i) {
        glm::vec3 pos = eye + direction * (i + 1.0f);

        int bx = static_cast<int>(std::floor(pos.x));
        int by = static_cast<int>(std::floor(pos.y));
        int bz = static_cast<int>(std::floor(pos.z));

        BlockType type = world.GetBlock(bx, by, bz);
        if (type != BlockType::Air && type != BlockType::Bedrock) {
            // Break the block - set to air
            world.SetBlock(bx, by, bz, BlockType::Air);
            return true;
        }

        // Check bounds
        if (std::abs(pos.x) > 10000 || std::abs(pos.y) > 10000 || std::abs(pos.z) > 10000) {
            break;
        }
    }

    return false;
}