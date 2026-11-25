#include "core/world/rift_gate.h"

namespace {

// The glowing corner stones are unique to the shrine, so they identify a gate
// whose portal surface has been broken without touching anything else.
int FindRiftGateFrameY(const World& world, int gateX, int gateZ) {
    for (int y = 2; y + 1 < UNDERWORLD_TOP; ++y) {
        if (world.GetBlock(gateX - 1, y, gateZ) == BlockType::Lumenstone &&
            world.GetBlock(gateX + 2, y, gateZ) == BlockType::Lumenstone) {
            return y - 2; // corner stones sit two blocks above the feet
        }
    }
    return -1;
}

void BuildPortalSurface(World& world, int gateX, int gateZ, int feetY) {
    for (int dy = 0; dy < 3; ++dy) {
        world.SetBlock(gateX, feetY + dy, gateZ, BlockType::Portal);
        world.SetBlock(gateX + 1, feetY + dy, gateZ, BlockType::Portal);
    }
}

} // namespace

int FindRiftGateY(const World& world, int gateX, int gateZ) {
    for (int y = 0; y < UNDERWORLD_TOP; ++y) {
        for (int dx = -1; dx <= 2; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (world.GetBlock(gateX + dx, y, gateZ + dz) == BlockType::Portal) {
                    return y;
                }
            }
        }
    }
    return -1;
}

int BuildRiftGate(World& world, int gateX, int gateZ) {
    const int existingY = FindRiftGateY(world, gateX, gateZ);
    if (existingY >= 0) {
        return existingY;
    }

    // A standing shrine with a broken portal is repaired in place: re-carving
    // here would erase whatever the player built on the court.
    const int frameY = FindRiftGateFrameY(world, gateX, gateZ);
    if (frameY >= 0) {
        BuildPortalSurface(world, gateX, gateZ, frameY);
        return frameY;
    }

    int baseY = world.SurfaceY(gateX, gateZ);
    if (baseY < 3) baseY = 3;
    const int y = baseY;

    // Paved court. Building upwards keeps the gate visible from the spawn point
    // instead of tucked into a carved pit you have to walk out of.
    for (int dz = -7; dz <= 4; ++dz) {
        for (int dx = -5; dx <= 6; ++dx) {
            world.SetBlock(gateX + dx, y - 1, gateZ + dz, BlockType::Voidstone);
            for (int dy = 0; dy <= 6; ++dy) {
                if (world.GetBlock(gateX + dx, y + dy, gateZ + dz) == BlockType::Bedrock) {
                    continue;
                }
                world.SetBlock(gateX + dx, y + dy, gateZ + dz, BlockType::Air);
            }
            // Support the paving on sloped ground so the court never floats.
            for (int dy = 2; dy <= 4; ++dy) {
                const int supportY = y - dy;
                if (supportY < 0) {
                    continue;
                }
                if (world.GetBlock(gateX + dx, supportY, gateZ + dz) == BlockType::Air) {
                    world.SetBlock(gateX + dx, supportY, gateZ + dz, BlockType::Stone);
                }
            }
        }
    }

    // Frame.
    for (int dy = -1; dy <= 3; ++dy) {
        world.SetBlock(gateX - 1, y + dy, gateZ, BlockType::Voidstone);
        world.SetBlock(gateX + 2, y + dy, gateZ, BlockType::Voidstone);
    }
    for (int dx = -1; dx <= 2; ++dx) {
        world.SetBlock(gateX + dx, y - 1, gateZ, BlockType::Voidstone);
        world.SetBlock(gateX + dx, y + 3, gateZ, BlockType::Voidstone);
    }

    BuildPortalSurface(world, gateX, gateZ, y);

    // Glowing corner stones make the gate readable in the dark realm.
    world.SetBlock(gateX - 1, y + 2, gateZ, BlockType::Lumenstone);
    world.SetBlock(gateX + 2, y + 2, gateZ, BlockType::Lumenstone);

    return y;
}
