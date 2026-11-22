#include "mesh.h"
#include <array>

const std::array<std::array<float, 3>, 6> faceNormals = {{
    {0, -1, 0}, {0, 1, 0},
    {-1, 0, 0}, {1, 0, 0},
    {0, 0, -1}, {0, 0, 1}
}};

const std::array<std::array<float, 4>, 6> faceUvCoords = {{
    {0, 0, 1, 1}, {0, 0, 1, 1},
    {0, 0, 1, 1}, {0, 0, 1, 1},
    {0, 0, 1, 1}, {0, 0, 1, 1}
}};   