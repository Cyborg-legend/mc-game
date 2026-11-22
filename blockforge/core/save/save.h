#pragma once

#include <string>
#include <cstdint>
#include <fstream>
#include "core/world/chunk.h"

static constexpr uint32_t SAVE_MAGIC = 0x47524642;
static constexpr uint32_t SAVE_VERSION = 1;

struct GameSettings {
    int renderDistance = 16;
    float fov = 70.0f;
    float volume = 50.0f;
    bool fullscreen = false;
};

struct SaveData {
    uint32_t magic;
    uint32_t version;
    uint64_t seed;
    float playerX, playerY, playerZ;
    float playerYaw, playerPitch;
    int health;
    uint8_t inventory[72];
    int chunkCount;
};

bool SaveGame(const std::string& path, World& world, float px, float py, float pz,
              float yaw, float pitch, int health, uint8_t inventory[]);
bool LoadGame(const std::string& path, World& world, float& px, float& py, float& pz,
              float& yaw, float& pitch, int& health, uint8_t inventory[]);
bool SaveSettings(const std::string& path, const GameSettings& s);
bool LoadSettings(const std::string& path, GameSettings& s);
std::string GetSaveDir();   