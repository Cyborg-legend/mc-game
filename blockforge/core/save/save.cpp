#include "save.h"
#include <fstream>

// ============================================================
// SaveSlot implementation
// ============================================================

bool SaveSlot::SaveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to save game to: " << path << std::endl;
        return false;
    }

    // Write magic
    file.write(reinterpret_cast<const char*>(&SAVE_MAGIC), sizeof(SAVE_MAGIC));
    // Write version
    file.write(reinterpret_cast<const char*>(&SAVE_VERSION), sizeof(SAVE_VERSION));
    // Write seed
    file.write(reinterpret_cast<const char*>(&seed), sizeof(seed));
    // Write chunk count
    uint32_t chunkCount = static_cast<uint32_t>(chunks.size());
    file.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));
    // Write chunk data
    for (const auto& chunk : chunks) {
        file.write(reinterpret_cast<const char*>(&chunk.id), sizeof(chunk.id));
        uint8_t dirty = chunk.dirty ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&dirty), sizeof(dirty));
        file.write(reinterpret_cast<const char*>(chunk.blockData.data()), chunk.blockData.size());
    }
    // Write entity count
    uint32_t entityCount = static_cast<uint32_t>(entities.size());
    file.write(reinterpret_cast<const char*>(&entityCount), sizeof(entityCount));
    // Write entity data
    for (const auto& entity : entities) {
        file.write(reinterpret_cast<const char*>(&entity.type), sizeof(entity.type));
        file.write(reinterpret_cast<const char*>(entity.position), sizeof(entity.position));
        file.write(reinterpret_cast<const char*>(&entity.health), sizeof(entity.health));
        uint8_t state = entity.state;
        file.write(reinterpret_cast<const char*>(&state), sizeof(state));
    }
    // Write player state
    file.write(reinterpret_cast<const char*>(player.position), sizeof(player.position));
    // Write inventory (36 slots, each: 1 byte type + 1 byte count)
    file.write(reinterpret_cast<const char*>(inventory.data()), inventory.size());
    // Write settings
    // Note: settings are saved separately via SaveSettings/LoadSettings

    file.close();
    return true;
}

bool SaveSettings(const std::string& path, const GameSettings& settings) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to save settings to: " << path << std::endl;
        return false;
    }

    nlohmann::json jsonSettings;
    jsonSettings["renderDistance"] = settings.renderDistance;
    jsonSettings["fov"] = settings.fov;
    jsonSettings["volume"] = settings.volume;
    jsonSettings["fullscreen"] = settings.fullscreen;
    jsonSettings["mouseSensitivity"] = settings.mouseSensitivity;
    jsonSettings["moveForward"] = settings.moveForward;
    jsonSettings["moveBack"] = settings.moveBack;
    jsonSettings["moveLeft"] = settings.moveLeft;
    jsonSettings["moveRight"] = settings.moveRight;
    jsonSettings["jump"] = settings.jump;
    jsonSettings["inventory"] = settings.inventory;
    jsonSettings["sprint"] = settings.sprint;

    file << jsonSettings.dump(4); // Pretty print with 4 space indent
    file.close();
    return true;
}

bool LoadSettings(const std::string& path, GameSettings& settings) {
    std::ifstream file(path);
    if (!file.is_open()) {
        // No settings file exists, use defaults
        std::cerr << "No settings file found at: " << path << ", using defaults" << std::endl;
        return false;
    }

    nlohmann::json jsonSettings;
    try {
        file >> jsonSettings;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse settings file: " << e.what() << std::endl;
        return false;
    }

    // Load with defaults
    settings.renderDistance = jsonSettings.value("renderDistance", 16);
    settings.fov = jsonSettings.value("fov", 70.0f);
    settings.volume = jsonSettings.value("volume", 50.0f);
    settings.fullscreen = jsonSettings.value("fullscreen", false);
    settings.mouseSensitivity = jsonSettings.value("mouseSensitivity", 1);
    settings.moveForward = jsonSettings.value("moveForward", GLFW_KEY_W);
    settings.moveBack = jsonSettings.value("moveBack", GLFW_KEY_S);
    settings.moveLeft = jsonSettings.value("moveLeft", GLFW_KEY_A);
    settings.moveRight = jsonSettings.value("moveRight", GLFW_KEY_D);
    settings.jump = jsonSettings.value("jump", GLFW_KEY_SPACE);
    settings.inventory = jsonSettings.value("inventory", GLFW_KEY_E);
    settings.sprint = jsonSettings.value("sprint", GLFW_KEY_LEFT_SHIFT);

    return true;
}
