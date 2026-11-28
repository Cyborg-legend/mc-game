/* Headless check: the world shader must still vary each block face.
 *
 * Greedy meshing draws one quad across many blocks, so the per-block texture
 * variation lives in the fragment shader as a hash of the block cell behind
 * the face. This check fails if that hash, its quarter-turn branches or the
 * mirror bit ever disappear from the source.
 */
#include "shaders.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

static void expect(int ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

static int has(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}

int main(void) {
    const char *vert = shader_world_vertex();
    const char *frag = shader_world_fragment();

    expect(frag != NULL && frag[0] != '\0', "world fragment source is present");
    expect(has(frag, "#version 330 core"), "fragment is GLSL 330 core");
    expect(has(vert, "vWorldPos = world.xyz;"), "vertex passes the world position");

    /* The hash reads the cell behind the face, not the interpolated vertex. */
    expect(has(frag, "vec3 cellPos = vWorldPos - normalize(vNormal) * 0.5;"),
           "cell position is derived from the face");
    expect(has(frag, "ivec3 cell = ivec3(floor(cellPos));"),
           "cell position is floored to a block coordinate");

    /* Hash primes and the avalanche steps. */
    expect(has(frag, "73856093u"), "hash uses the x prime");
    expect(has(frag, "19349663u"), "hash uses the y prime");
    expect(has(frag, "83492791u"), "hash uses the z prime");
    expect(has(frag, "h ^= h >> 13;"), "hash folds the high bits down");
    expect(has(frag, "h *= 1274126177u;"), "hash multiplies to spread bits");
    expect(has(frag, "h ^= h >> 16;"), "hash finishes the avalanche");

    /* Four quarter-turns: spin 0 leaves the UVs alone, 1..3 rotate them. */
    expect(has(frag, "uint spin = (h >> 2) & 3u;"),
           "spin is masked to four quarter-turns");
    expect(has(frag, "if (spin == 1u) local = vec2(local.y, 1.0 - local.x);"),
           "quarter-turn 1 rotates the UVs");
    expect(has(frag, "else if (spin == 2u) local = vec2(1.0 - local.x, 1.0 - local.y);"),
           "quarter-turn 2 rotates the UVs");
    expect(has(frag, "else if (spin == 3u) local = vec2(1.0 - local.y, local.x);"),
           "quarter-turn 3 rotates the UVs");

    /* And an optional mirror so two blocks can share a turn but not a face. */
    expect(has(frag, "if ((h & 1u) != 0u) local.x = 1.0 - local.x;"),
           "mirror bit flips the UVs");

    /* The tile still repeats once per block, or merging would stretch it. */
    expect(has(frag, "vec2 local = fract(vUV);"),
           "tile UVs still repeat per block");
    expect(has(frag, "* 0.0625;"), "atlas UVs still land inside one 32px tile");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
