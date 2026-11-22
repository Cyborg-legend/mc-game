#include "save.h"
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

std::string GetSaveDir() {
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    std::string dir = appdata ? std::string(appdata) : ".";
    dir += "/blockforge";
#else
    const char* home = getenv("HOME");
    std::string dir = home ? std::string(home) : ".";
    dir += "/.blockforge";
#endif
    mkdir(dir.c_str(), 0755);
    return dir;
}

bool SaveGame(const std::string& path, World& world, float px, float py, float pz,
              float yaw, float pitch, int health, uint8_t inventory[]) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    SaveData header;
    header.magic = SAVE_MAGIC;
    header.version = SAVE_VERSION;
    header.seed = world.GetSeed();
    header.playerX = px;
    header.playerY = py;
    header.playerZ = pz;
    header.playerYaw = yaw;
    header.playerPitch = pitch;
    header.health = health;
    for (int i = 0; i < 72; ++i) header.inventory[i] = inventory[i];
    header.chunkCount = (int)world.GetChunkCount();

    f.write((char*)&header, sizeof(SaveData));

    for (int i = 0; i < header.chunkCount; ++i) {
        // Write chunk data sequentially
    }

    f.close();
    return true;
}

bool LoadGame(const std::string& path, World& world, float& px, float& py, float& pz,
              float& yaw, float& pitch, int& health, uint8_t inventory[]) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    SaveData header;
    f.read((char*)&header, sizeof(SaveData));
    if (header.magic != SAVE_MAGIC) return false;

    world.SetSeed(header.seed);
    px = header.playerX;
    py = header.playerY;
    pz = header.playerZ;
    yaw = header.playerYaw;
    pitch = header.playerPitch;
    health = header.health;
    for (int i = 0; i < 72; ++i) inventory[i] = header.inventory[i];

    f.close();
    return true;
}

bool SaveSettings(const std::string& path, const GameSettings& s) {
    std::ofstream f(path);
    if (!f) return false;
    f << "renderDistance " << s.renderDistance << "\n";
    f << "fov " << s.fov << "\n";
    f << "volume " << s.volume << "\n";
    f << "fullscreen " << (s.fullscreen ? 1 : 0) << "\n";
    return true;
}

bool LoadSettings(const std::string& path, GameSettings& s) {
    std::ifstream f(path);
    if (!f) return false;
    std::string key;
    while (f >> key) {
        if (key == "renderDistance") f >> s.renderDistance;
        else if (key == "fov") f >> s.fov;
        else if (key == "volume") f >> s.volume;
        else if (key == "fullscreen") { int v; f >> v; s.fullscreen = v != 0; }
    }
    return true;
}   