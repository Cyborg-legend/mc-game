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
    bool prevKeys_[INPUT_KEY_COUNT]{};

    bool Pressed(const InputState& input, int key) const;
    uint32_t ParseSeed() const;
    void SyncSeedText();
    void DrawRow(UiDrawList& ui, float x, float y, float w, float h,
                 const std::string& label, const std::string& value,
                 bool selected) const;
    void DrawButton(UiDrawList& ui, float x, float y, float w, float h,
                    const std::string& label, bool selected, bool enabled) const;
};
