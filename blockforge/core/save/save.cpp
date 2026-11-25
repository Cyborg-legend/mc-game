#include "core/save/save.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

SaveSlot::SaveSlot() {
    inventory.fill(static_cast<uint8_t>(BlockType::Air));
}

bool SaveSlot::SaveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file for writing: " << path << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(&SAVE_MAGIC), sizeof(SAVE_MAGIC));
    file.write(reinterpret_cast<const char*>(&SAVE_VERSION), sizeof(SAVE_VERSION));
    file.write(reinterpret_cast<const char*>(&seed), sizeof(seed));
    const uint8_t dimension = static_cast<uint8_t>(world.GetDimension());
    file.write(reinterpret_cast<const char*>(&dimension), sizeof(dimension));

    const uint32_t chunkCount = static_cast<uint32_t>(world.GetChunks().size());
    file.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));
    for (const auto& entry : world.GetChunks()) {
        const uint64_t id = entry.first;
        const Chunk& chunk = entry.second;
        file.write(reinterpret_cast<const char*>(&id), sizeof(id));
        file.write(reinterpret_cast<const char*>(chunk.blocks), CHUNK_VOLUME);
    }

    const uint32_t entityCount = static_cast<uint32_t>(entities.size());
    file.write(reinterpret_cast<const char*>(&entityCount), sizeof(entityCount));
    for (const EntitySaveData& entity : entities) {
        file.write(reinterpret_cast<const char*>(&entity.type), sizeof(entity.type));
        file.write(reinterpret_cast<const char*>(entity.position), sizeof(entity.position));
        file.write(reinterpret_cast<const char*>(&entity.health), sizeof(entity.health));
        file.write(reinterpret_cast<const char*>(&entity.state), sizeof(entity.state));
    }

    file.write(reinterpret_cast<const char*>(inventory.data()), inventory.size());
    return file.good();
}

bool SaveSlot::LoadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!file.good() || magic != SAVE_MAGIC || (version != 2u && version != SAVE_VERSION)) {
        std::cerr << "Invalid save file: " << path << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&seed), sizeof(seed));

    Dimension dimension = Dimension::Upperworld;
    if (version >= 3u) {
        uint8_t dimensionByte = 0;
        file.read(reinterpret_cast<char*>(&dimensionByte), sizeof(dimensionByte));
        if (dimensionByte < static_cast<uint8_t>(Dimension::Count)) {
            dimension = static_cast<Dimension>(dimensionByte);
        }
    }

    uint32_t chunkCount = 0;
    file.read(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));
    world = World();
    world.SetSeed(seed);
    world.SetDimension(dimension);
    for (uint32_t i = 0; i < chunkCount; ++i) {
        uint64_t id = 0;
        file.read(reinterpret_cast<char*>(&id), sizeof(id));
        int cx = 0, cy = 0, cz = 0;
        World::UnpackKey(id, cx, cy, cz);
        Chunk& chunk = world.GetOrCreateChunk(cx, cy, cz);
        file.read(reinterpret_cast<char*>(chunk.blocks), CHUNK_VOLUME);
    }

    uint32_t entityCount = 0;
    file.read(reinterpret_cast<char*>(&entityCount), sizeof(entityCount));
    entities.resize(entityCount);
    for (uint32_t i = 0; i < entityCount; ++i) {
        EntitySaveData& entity = entities[i];
        file.read(reinterpret_cast<char*>(&entity.type), sizeof(entity.type));
        file.read(reinterpret_cast<char*>(entity.position), sizeof(entity.position));
        file.read(reinterpret_cast<char*>(&entity.health), sizeof(entity.health));
        file.read(reinterpret_cast<char*>(&entity.state), sizeof(entity.state));
    }

    file.read(reinterpret_cast<char*>(inventory.data()), inventory.size());
    return file.good() || file.eof();
}

bool SaveSettings(const std::string& path, const GameSettings& settings) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open settings file for writing: " << path << std::endl;
        return false;
    }

    nlohmann::json json;
    json["renderDistance"] = settings.renderDistance;
    json["fov"] = settings.fov;
    json["volume"] = settings.volume;
    json["fullscreen"] = settings.fullscreen;
    json["mouseSensitivity"] = settings.mouseSensitivity;
    json["moveForward"] = settings.moveForward;
    json["moveBack"] = settings.moveBack;
    json["moveLeft"] = settings.moveLeft;
    json["moveRight"] = settings.moveRight;
    json["jump"] = settings.jump;
    json["inventory"] = settings.inventory;
    json["sprint"] = settings.sprint;

    file << json.dump(2);
    return file.good();
}

bool LoadSettings(const std::string& path, GameSettings& settings) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse settings: " << e.what() << std::endl;
        return false;
    }

    settings.renderDistance = json.value("renderDistance", settings.renderDistance);
    settings.fov = json.value("fov", settings.fov);
    settings.volume = json.value("volume", settings.volume);
    settings.fullscreen = json.value("fullscreen", settings.fullscreen);
    settings.mouseSensitivity = json.value("mouseSensitivity", settings.mouseSensitivity);
    settings.moveForward = json.value("moveForward", settings.moveForward);
    settings.moveBack = json.value("moveBack", settings.moveBack);
    settings.moveLeft = json.value("moveLeft", settings.moveLeft);
    settings.moveRight = json.value("moveRight", settings.moveRight);
    settings.jump = json.value("jump", settings.jump);
    settings.inventory = json.value("inventory", settings.inventory);
    settings.sprint = json.value("sprint", settings.sprint);
    return true;
}
