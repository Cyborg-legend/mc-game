#pragma once

#include "meshing/mesh.h"
#include "core/world/chunk.h"

// Chunk mesher. Emits one quad per exposed block face with baked per-vertex AO.
// Neighbour lookups go through the World so shared chunk borders do not
// generate duplicate internal faces.
class GreedyMesher {
public:
    static MeshChunk Mesh(const World& world, const Chunk& chunk);
};
