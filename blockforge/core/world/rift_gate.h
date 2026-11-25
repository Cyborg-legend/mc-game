#pragma once

#include "core/world/chunk.h"

// Feet height of an existing rift gate near the column, or -1 when there is
// none. Used by BuildRiftGate to leave a standing gate alone.
int FindRiftGateY(const World& world, int gateX, int gateZ);

// Builds the rift gate shrine (paved court, voidstone frame, portal surface)
// only when no gate exists yet. Loading a save or arriving in a realm that
// already has one never re-carves the court or erases nearby builds.
// Returns the court's standing height.
int BuildRiftGate(World& world, int gateX, int gateZ);
