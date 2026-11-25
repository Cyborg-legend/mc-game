#pragma once

#include <cstdint>
#include <string>

#include "core/world/chunk.h"
#include "platform/common/iplatform.h"
#include "ui/ui.h"

// Options chosen on the "Create World" screen.
struct WorldOptions {
    uint32_t seed = 20251125u;
    int renderDistance = 6; // chunks, 3..8
    Dimension startingRealm = Dimension::Upperworld;
};

enum class MenuAction {
    None,
    NewGame,
    ContinueGame,
    QuitGame
};

// Edge-triggered key tracking for menu screens: a key counts once per press,
// not once per frame it is held.
struct KeyEdge {
    bool previous[INPUT_KEY_COUNT]{};

    bool Pressed(const InputState& input, int key) const;
    void Sync(const InputState& input);
    void Reset();
    void Latch(int key);
};

// Keyboard-driven title screen plus the world creation settings.
//
// Root:    Continue World / New World / Quit
// Create:  Seed, Render Distance, Starting Realm, Create World, Back
//
// Navigation: Up/Down select, Left/Right change, Enter confirm, Esc back.
class MainMenu {
public:
    MainMenu();

    void Reset(bool hasSave);
    MenuAction Update(const InputState& input, bool hasSave);
    void Draw(UiDrawList& ui, float width, float height, bool hasSave) const;

    const WorldOptions& Options() const { return options_; }

private:
    enum class Screen {
        Root,
        Create
    };

    Screen screen_ = Screen::Root;
    WorldOptions options_;
    std::string seedText_;
    int selection_ = 0;
    KeyEdge keys_;

    uint32_t ParseSeed() const;
    void SyncSeedText();
};

enum class PauseAction {
    None,
    Resume,
    SaveAndTitle,
    QuitGame
};

// In-game pause overlay: Resume / Save & Return to Title / Quit. The world is
// frozen behind it, so it reads as a pause rather than a screen change.
class PauseMenu {
public:
    void Reset();
    PauseAction Update(const InputState& input);
    void Draw(UiDrawList& ui, float width, float height) const;

private:
    int selection_ = 0;
    KeyEdge keys_;
};
