#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <array>
#include <fstream>
#include "nlohmann/json.hpp"
#include "core/world/world.h"

// Magic number for BlockForge saves
static constexpr uint32_t SAVE_MAGIC = 0x47524642; // "BFRG" in little-endian
static constexpr uint32_t SAVE_VERSION = 1;

// Save data per chunk
struct ChunkSaveData {
    uint64_t id;
    bool dirty;
    std::array<uint8_t, 4096> blockData;
};

// Save data per entity
struct EntitySaveData {
    uint8_t type;
    float position[3];
    int health;
    uint8_t state;
};

// Save slot structure
struct SaveSlot {
    std::string slotName;
    uint64_t seed;
    std::vector<ChunkSaveData> chunks;
    std::vector<EntitySaveData> entities;
    World world; // The world state
    GameSettings settings;
};

// Player inventory in save format (36 slots, each: 1 byte type + 1 byte count)
std::array<uint8_t, 72> inventory; // 36 * (type + count)

// Constructor
SaveSlot() : seed(0) {
    inventory.fill(0);
}

// Save entire slot to file
bool SaveToFile(const std::string& path) const;

// Load entire slot from file
bool LoadFromFile(const std::string& path);
};

// Main menu state
enum class MainMenuState {
    Play,
    Settings,
    Quit
};

// Settings structure - saved to ~/.blockforge/settings.json
struct GameSettings {
    int renderDistance = 16; // 8-64
    float fov = 70.0f;       // 60-110
    float volume = 50.0f;    // 0-100
    bool fullscreen = false;
    int mouseSensitivity = 1; // 1-10

    // Keybinds (GLFW key codes)
    int moveForward = GLFW_KEY_W;
    int moveBack = GLFW_KEY_S;
    int moveLeft = GLFW_KEY_A;
    int moveRight = GLFW_KEY_D;
    int jump = GLFW_KEY_SPACE;
    int inventory = GLFW_KEY_E;
    int sprint = GLFW_KEY_LEFT_SHIFT;
};

// Save/Load functions for settings
bool SaveSettings(const std::string& path, const GameSettings& settings);
bool LoadSettings(const std::string& path, GameSettings& settings);