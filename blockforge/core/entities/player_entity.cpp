#include "core/physics/player.h"
#include "core/world/chunk.h"
#include "platform/common/iplatform.h"
#include <GLFW/glfw3.h>

void PlayerEntity_Update(Player& player, World& world, const InputState& input, float dt) {
    player.Look(input.mouseDX, input.mouseDY);
    player.Update(dt, input.keys, (int[]){});
    player.Move(dt, world);
    player.PlaceBlock(world, (int[]){});
    player.BreakBlock(world, (int[]){});

    if (input.keys[GLFW_KEY_ONE]) player.SetHotbar(0);
    if (input.keys[GLFW_KEY_TWO]) player.SetHotbar(1);
    if (input.keys[GLFW_KEY_THREE]) player.SetHotbar(2);
    if (input.keys[GLFW_KEY_FOUR]) player.SetHotbar(3);
    if (input.keys[GLFW_KEY_FIVE]) player.SetHotbar(4);
    if (input.keys[GLFW_KEY_SIX]) player.SetHotbar(5);
    if (input.keys[GLFW_KEY_SEVEN]) player.SetHotbar(6);
    if (input.keys[GLFW_KEY_EIGHT]) player.SetHotbar(7);
    if (input.keys[GLFW_KEY_NINE]) player.SetHotbar(8);
}   