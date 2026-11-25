#pragma once

#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/world/chunk.h"

static constexpr uint32_t SAVE_MAGIC = 0x47524642u; // "BFRG"
static constexpr uint32_t SAVE_VERSION = 3u; // v3 adds the dimension byte

// Settings persisted to settings.json next to the save file.
struct GameSettings {
    int renderDistance = 8;   // chunks
    float fov = 70.0f;        // 60-110
    float volume = 50.0f;     // 0-100
    bool fullscreen = false;
    int mouseSensitivity = 5; // 1-10

    int moveForward = GLFW_KEY_W;
    int moveBack = GLFW_KEY_S;
    int moveLeft = GLFW_KEY_A;
    int moveRight = GLFW_KEY_D;
    int jump = GLFW_KEY_SPACE;
    int inventory = GLFW_KEY_E;
    int sprint = GLFW_KEY_LEFT_SHIFT;
};

struct ChunkSaveData {
    uint64_t id = 0;
    bool dirty = false;
    std::array<uint8_t, CHUNK_VOLUME> blockData{};
};

struct EntitySaveData {
    uint8_t type = 0;
    float position[3] = { 0.0f, 0.0f, 0.0f };
    int health = 20;
    uint8_t state = 0;
};

struct SaveSlot {
    std::string slotName;
    uint64_t seed = 0;
    std::vector<ChunkSaveData> chunks;
    std::vector<EntitySaveData> entities;
    World world;
    GameSettings settings;

    // Player inventory: 36 slots, one block type id each.
    std::array<uint8_t, 36> inventory{};

    SaveSlot();

    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);
};

bool SaveSettings(const std::string& path, const GameSettings& settings);
bool LoadSettings(const std::string& path, GameSettings& settings);
