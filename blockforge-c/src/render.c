#include "render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------- shaders */

static const char *kWorldVertex =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(location = 2) in vec2 aUV;\n"
    "layout(location = 3) in float aAO;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out vec3 vNormal;\n"
    "out vec2 vUV;\n"
    "out float vAO;\n"
    "out vec3 vWorldPos;\n"
    "void main() {\n"
    "    vec4 world = model * vec4(aPos, 1.0);\n"
    "    vWorldPos = world.xyz;\n"
    "    vNormal = mat3(model) * aNormal;\n"
    "    vUV = aUV;\n"
    "    vAO = aAO;\n"
    "    gl_Position = projection * view * world;\n"
    "}\n";

static const char *kWorldFragment =
    "#version 330 core\n"
    "in vec3 vNormal;\n"
    "in vec2 vUV;\n"
    "in float vAO;\n"
    "in vec3 vWorldPos;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D texAtlas;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 ambientColor;\n"
    "uniform vec3 lightColor;\n"
    "uniform vec3 fogColor;\n"
    "uniform float fogStart;\n"
    "uniform float fogDensity;\n"
    "void main() {\n"
    "    vec3 base = texture(texAtlas, vUV).rgb;\n"
    "    vec3 n = normalize(vNormal);\n"
    "    float faceShade = 0.70;\n"
    "    if (n.y > 0.5) faceShade = 1.0;\n"
    "    else if (n.y < -0.5) faceShade = 0.52;\n"
    "    else if (abs(n.x) > 0.5) faceShade = 0.82;\n"
    "    vec3 lit = ambientColor * 0.92 + lightColor * 0.55;\n"
    "    vec3 color = base * faceShade * lit * mix(0.45, 1.0, vAO);\n"
    "    float distanceToCamera = length(vWorldPos - viewPos);\n"
    "    float fog = clamp((distanceToCamera - fogStart) * fogDensity, 0.0, 1.0);\n"
    "    FragColor = vec4(mix(color, fogColor, fog), 1.0);\n"
    "}\n";

static const char *kUiVertex =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "uniform vec2 screenSize;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    vec2 ndc = vec2(aPos.x / screenSize.x * 2.0 - 1.0,\n"
    "                    1.0 - aPos.y / screenSize.y * 2.0);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char *kUiFragment =
    "#version 330 core\n"
    "in vec4 vColor;\n"
    "out vec4 FragColor;\n"
    "void main() { FragColor = vColor; }\n";

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(const char *vertex_source, const char *fragment_source) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(program);
        program = 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

/* -------------------------------------------------------------- camera */

static void camera_basis(const BgCamera *camera, float forward[3], float right[3], float up[3]) {
    const float cp = cosf(camera->pitch);
    forward[0] = cp * cosf(camera->yaw);
    forward[1] = sinf(camera->pitch);
    forward[2] = cp * sinf(camera->yaw);

    right[0] = -sinf(camera->yaw);
    right[1] = 0.0f;
    right[2] = cosf(camera->yaw);

    up[0] = -cosf(camera->yaw) * forward[1];
    up[1] = cosf(camera->yaw) * forward[0] + sinf(camera->yaw) * forward[2];
    up[2] = -sinf(camera->yaw) * forward[1];
}

static void camera_view_matrix(const BgCamera *camera, float m[16]) {
    float f[3], r[3], u[3];
    camera_basis(camera, f, r, u);
    const float eye[3] = { camera->x, camera->y, camera->z };

    m[0] = r[0];  m[1] = u[0];  m[2] = -f[0]; m[3] = 0.0f;
    m[4] = r[1];  m[5] = u[1];  m[6] = -f[1]; m[7] = 0.0f;
    m[8] = r[2];  m[9] = u[2];  m[10] = -f[2]; m[11] = 0.0f;
    m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
    m[15] = 1.0f;
}

static void camera_projection_matrix(const BgCamera *camera, float m[16]) {
    const float aspect = camera->aspect > 0.0001f ? camera->aspect : 16.0f / 9.0f;
    const float f = 1.0f / tanf(camera->fov * 0.5f);
    const float n = camera->near_plane;
    const float far = camera->far_plane;

    memset(m, 0, sizeof(float) * 16);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + n) / (n - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * n) / (n - far);
}

/* --------------------------------------------------------------- atlas */

typedef enum {
    PAT_UNUSED = -1,
    PAT_FLAT = 0,
    PAT_SPECKLE,
    PAT_GRASS_TOP,
    PAT_GRASS_SIDE,
    PAT_BARK,
    PAT_RINGS,
    PAT_FOLIAGE,
    PAT_LIQUID,
    PAT_CHUNKY,
    PAT_PLANKS,
    PAT_GRID,
    PAT_BRICK,
    PAT_ORE,
    PAT_GLOW,
    PAT_SWIRL,
    PAT_CRYSTAL,
    PAT_ASH
} Pattern;

typedef struct {
    unsigned char r, g, b;
    unsigned char ar, ag, ab;
    Pattern pattern;
} TileDef;

static const TileDef kTiles[32] = {
    {   0,   0,   0,   0,   0,   0, PAT_UNUSED },      /* void */
    { 128, 128, 132,  90,  90,  94, PAT_SPECKLE },     /* stone */
    { 122,  88,  60,  96,  68,  45, PAT_SPECKLE },     /* dirt */
    {  96, 158,  62,  74, 124,  48, PAT_GRASS_TOP },   /* grass top */
    { 122,  88,  60,  96, 158,  62, PAT_GRASS_SIDE },  /* grass side */
    {  96,  74,  48,  70,  54,  35, PAT_BARK },        /* bark */
    { 150, 118,  78, 120,  92,  60, PAT_RINGS },       /* wood rings */
    {  58, 118,  48,  40,  86,  34, PAT_FOLIAGE },     /* leaves */
    { 222, 210, 164, 200, 188, 145, PAT_SPECKLE },     /* sand */
    {  54,  98, 190,  90, 140, 220, PAT_LIQUID },      /* water */
    {  52,  52,  56,  30,  30,  34, PAT_CHUNKY },      /* bedrock */
    { 166, 132,  84, 132, 102,  62, PAT_PLANKS },      /* planks */
    { 140, 110,  72, 110,  84,  54, PAT_BARK },        /* sticks */
    { 150, 112,  64, 110,  80,  45, PAT_GRID },        /* craft top */
    { 136, 100,  58, 104,  74,  42, PAT_PLANKS },      /* craft side */
    { 158,  72,  58, 190, 180, 170, PAT_BRICK },       /* brick */
    { 110,  44,  40,  78,  28,  26, PAT_SPECKLE },     /* emberstone */
    {  74,  62,  66,  54,  44,  48, PAT_ASH },         /* ashsoil */
    { 232, 206, 110, 255, 240, 180, PAT_GLOW },        /* lumenstone */
    { 196,  82,  28, 255, 150,  40, PAT_LIQUID },      /* magma */
    {  46,  34,  58,  90,  66, 120, PAT_CRYSTAL },     /* voidstone */
    {  92,  60,  54, 140, 120, 110, PAT_BRICK },       /* cinderbrick */
    { 226, 224, 214, 200, 198, 190, PAT_SPECKLE },     /* pale quartz */
    { 104,  44,  44,  78,  32,  32, PAT_BARK },        /* crimson bark */
    { 150,  74,  66, 120,  55,  50, PAT_RINGS },       /* crimson rings */
    { 126,  44,  44,  92,  30,  30, PAT_FOLIAGE },     /* bloodleaves */
    { 128, 128, 132, 230, 140,  50, PAT_ORE },         /* emberlode */
    { 128, 128, 132, 210, 190, 160, PAT_ORE },         /* ironlode */
    {  86,  44, 140, 200, 120, 255, PAT_SWIRL },       /* rift portal */
    {  70, 150, 120, 170, 255, 210, PAT_GLOW },        /* glowmoss */
    {  74,  70,  72, 100,  96,  96, PAT_CHUNKY },      /* cinderstone */
    { 176, 170, 160, 150, 144, 136, PAT_ASH }          /* ashblock */
};

static uint32_t pixel_hash(int tile, int px, int py) {
    uint32_t h = (uint32_t)tile * 374761393u + (uint32_t)px * 668265263u +
                 (uint32_t)py * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return h;
}

static float pixel_noise(int tile, int px, int py) {
    return (float)(pixel_hash(tile, px, py) & 0xFFFFu) / 65535.0f;
}

static unsigned char clamp255(float value) {
    if (value < 0.0f) return 0;
    if (value > 255.0f) return 255;
    return (unsigned char)value;
}

static void paint_pixel(const TileDef *def, int tile, int px, int py, unsigned char out[3]) {
    const float base[3] = { (float)def->r, (float)def->g, (float)def->b };
    const float accent[3] = { (float)def->ar, (float)def->ag, (float)def->ab };
    const float n = pixel_noise(tile, px, py);
    const float cx = (float)px - 7.5f;
    const float cy = (float)py - 7.5f;
    const float radius = sqrtf(cx * cx + cy * cy);

    float shade = 1.0f;
    float accent_mix = 0.0f;

    switch (def->pattern) {
        case PAT_FLAT:
            shade = 0.94f + n * 0.10f;
            break;
        case PAT_SPECKLE:
            shade = 0.86f + n * 0.24f;
            if (n > 0.94f) shade *= 0.74f;
            else if (n < 0.06f) shade *= 1.12f;
            if (((px * 5 + py * 11 + tile * 3) % 17) == 0) shade *= 0.88f;
            break;
        case PAT_GRASS_TOP:
            shade = 0.90f + n * 0.18f;
            if (((px * 3 + py * 5 + tile) % 13) == 0) shade *= 0.80f;
            if (((px * 7 + py * 2) % 19) == 0) shade *= 1.10f;
            break;
        case PAT_GRASS_SIDE:
            if (py < 3 + (((px * 3 + 1) % 3 == 0) ? 1 : 0)) {
                accent_mix = 1.0f;
                shade = 0.90f + n * 0.16f;
            } else {
                shade = 0.84f + n * 0.26f;
            }
            break;
        case PAT_BARK:
            shade = 0.82f + 0.18f * sinf((float)px * 1.7f + (float)tile);
            if (px % 5 == 0) shade *= 0.70f;
            if (py % 9 == 0 && px % 3 == 0) shade *= 0.84f;
            shade += n * 0.06f;
            break;
        case PAT_RINGS:
            shade = 0.74f + 0.28f * sinf(radius * 2.4f);
            if (radius < 1.2f) shade = 0.52f;
            break;
        case PAT_FOLIAGE: {
            const float clump = pixel_noise(tile, px / 2, py / 2);
            shade = 0.68f + clump * 0.48f;
            if (clump > 0.82f) shade = 0.46f;
            if (pixel_noise(tile + 91, px, py) > 0.93f) accent_mix = 0.8f;
            break;
        }
        case PAT_LIQUID:
            shade = 0.84f + 0.16f * sinf((float)py * 0.75f + (float)px * 0.35f + n * 1.4f);
            if (py % 6 == 0) shade *= 1.12f;
            break;
        case PAT_CHUNKY: {
            const int bx = px / 4;
            const int by = py / 4;
            shade = 0.80f + 0.26f * pixel_noise(tile + bx * 7, by, 0);
            if (px % 4 == 0 || py % 4 == 0) shade *= 0.82f;
            break;
        }
        case PAT_PLANKS:
            shade = 0.92f + n * 0.12f;
            if (py % 5 == 0 || py == 15) shade = 0.60f;
            if ((px + (py / 5) * 5) % 8 == 0) shade *= 0.76f;
            if (((px * 3 + py * 7) % 11) == 0) shade *= 0.88f;
            break;
        case PAT_GRID:
            shade = (px % 6 < 1 || py % 6 < 1) ? 0.55f : (0.90f + n * 0.14f);
            break;
        case PAT_BRICK: {
            const int row = py / 5;
            const int offset = (row % 2) * 4;
            if (py % 5 == 0 || ((px + offset) % 8) == 0) {
                accent_mix = 1.0f;
                shade = 0.94f;
            } else {
                shade = 0.86f + n * 0.20f;
            }
            break;
        }
        case PAT_ORE:
            shade = 0.84f + n * 0.22f;
            if (radius < 4.2f + n * 1.6f) {
                accent_mix = 1.0f;
                shade = 0.96f + n * 0.16f;
                if (radius > 3.3f) shade *= 0.76f;
            }
            break;
        case PAT_GLOW:
            shade = 1.10f - radius * 0.04f + n * 0.08f;
            if (radius < 2.2f) accent_mix = 0.8f;
            if (radius > 6.6f) shade *= 0.84f;
            break;
        case PAT_SWIRL: {
            const float wave = sinf(((float)px + (float)py) * 0.9f + n * 2.2f);
            accent_mix = (wave > 0.15f) ? 1.0f : 0.0f;
            shade = 0.92f + n * 0.12f;
            break;
        }
        case PAT_CRYSTAL: {
            const int band = (px + py * 2) % 11;
            if (band < 3) {
                accent_mix = 0.55f;
                shade = 1.22f;
            } else if (band < 6) {
                shade = 0.74f;
            } else {
                shade = 0.96f + n * 0.08f;
            }
            break;
        }
        case PAT_ASH:
            shade = 0.88f + n * 0.18f;
            if (n > 0.94f) shade = 0.60f;
            if (((px * 5 + py * 3) % 23) == 0) shade *= 1.10f;
            break;
        case PAT_UNUSED:
        default:
            shade = 0.0f;
            break;
    }

    for (int i = 0; i < 3; ++i) {
        const float value = base[i] * shade + (accent[i] * shade - base[i] * shade) * accent_mix;
        out[i] = clamp255(value);
    }
}

static GLuint build_atlas(void) {
    static unsigned char pixels[256 * 256 * 4];

    for (int i = 0; i < 256 * 256; ++i) {
        pixels[i * 4 + 0] = 0;
        pixels[i * 4 + 1] = 0;
        pixels[i * 4 + 2] = 0;
        pixels[i * 4 + 3] = 255;
    }

    for (int tile = 0; tile < 32; ++tile) {
        const TileDef *def = &kTiles[tile];
        if (def->pattern == PAT_UNUSED) {
            continue;
        }
        const int origin_x = (tile % 16) * 16;
        const int origin_y = (tile / 16) * 16;
        for (int py = 0; py < 16; ++py) {
            for (int px = 0; px < 16; ++px) {
                unsigned char rgb[3];
                paint_pixel(def, tile, px, py, rgb);
                unsigned char *p = &pixels[((origin_y + py) * 256 + (origin_x + px)) * 4];
                p[0] = rgb[0];
                p[1] = rgb[1];
                p[2] = rgb[2];
                p[3] = 255;
            }
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}

/* ------------------------------------------------------------- renderer */

bool renderer_init(BgRenderer *renderer) {
    memset(renderer, 0, sizeof(*renderer));

    glewExperimental = GL_TRUE;
    const GLenum status = glewInit();
    if (status != GLEW_OK) {
        fprintf(stderr, "GLEW init failed: %s\n", (const char *)glewGetErrorString(status));
        return false;
    }
    while (glGetError() != GL_NO_ERROR) {
    }

    const char *version = (const char *)glGetString(GL_VERSION);
    const char *device = (const char *)glGetString(GL_RENDERER);
    fprintf(stderr, "OpenGL %s | %s\n", version ? version : "unknown",
            device ? device : "unknown renderer");

    if (!GLEW_VERSION_3_3) {
        fprintf(stderr, "OpenGL 3.3 is required but the driver does not expose it\n");
        return false;
    }

    renderer->program = link_program(kWorldVertex, kWorldFragment);
    renderer->ui_program = link_program(kUiVertex, kUiFragment);
    if (!renderer->program || !renderer->ui_program) {
        return false;
    }

    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    glGenBuffers(1, &renderer->ibo);
    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ibo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, uv));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, ao));
    glBindVertexArray(0);

    glGenVertexArrays(1, &renderer->ui_vao);
    glGenBuffers(1, &renderer->ui_vbo);
    glBindVertexArray(renderer->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    renderer->atlas = build_atlas();
    renderer_set_environment(renderer, BG_UPPERWORLD);
    return true;
}

void renderer_shutdown(BgRenderer *renderer) {
    renderer_clear_chunks(renderer);
    free(renderer->chunks);
    renderer->chunks = NULL;
    if (renderer->vbo) glDeleteBuffers(1, &renderer->vbo);
    if (renderer->ibo) glDeleteBuffers(1, &renderer->ibo);
    if (renderer->vao) glDeleteVertexArrays(1, &renderer->vao);
    if (renderer->ui_vbo) glDeleteBuffers(1, &renderer->ui_vbo);
    if (renderer->ui_vao) glDeleteVertexArrays(1, &renderer->ui_vao);
    if (renderer->atlas) glDeleteTextures(1, &renderer->atlas);
    if (renderer->program) glDeleteProgram(renderer->program);
    if (renderer->ui_program) glDeleteProgram(renderer->ui_program);
    memset(renderer, 0, sizeof(*renderer));
}

void renderer_set_environment(BgRenderer *renderer, Dimension dimension) {
    if (dimension == BG_UNDERWORLD) {
        const float sky[3] = { 0.10f, 0.03f, 0.03f };
        const float fog[3] = { 0.20f, 0.05f, 0.04f };
        const float ambient[3] = { 0.34f, 0.16f, 0.14f };
        const float light[3] = { 1.00f, 0.52f, 0.22f };
        memcpy(renderer->sky, sky, sizeof(sky));
        memcpy(renderer->fog_color, fog, sizeof(fog));
        memcpy(renderer->ambient, ambient, sizeof(ambient));
        memcpy(renderer->light_color, light, sizeof(light));
        renderer->fog_start = 22.0f;
        renderer->fog_density = 0.0075f;
    } else {
        const float sky[3] = { 0.52f, 0.72f, 0.95f };
        const float fog[3] = { 0.62f, 0.78f, 0.96f };
        const float ambient[3] = { 0.44f, 0.44f, 0.46f };
        const float light[3] = { 1.00f, 0.98f, 0.90f };
        memcpy(renderer->sky, sky, sizeof(sky));
        memcpy(renderer->fog_color, fog, sizeof(fog));
        memcpy(renderer->ambient, ambient, sizeof(ambient));
        memcpy(renderer->light_color, light, sizeof(light));
        renderer->fog_start = 80.0f;
        renderer->fog_density = 0.0018f;
    }
}

static BgChunkDraw *find_chunk_draw(BgRenderer *renderer, uint64_t key) {
    for (size_t i = 0; i < renderer->chunk_count; ++i) {
        if (renderer->chunks[i].key == key) {
            return &renderer->chunks[i];
        }
    }
    return NULL;
}

void renderer_upload_chunk(BgRenderer *renderer, uint64_t key, const BgMesh *mesh) {
    BgChunkDraw *draw = find_chunk_draw(renderer, key);
    if (mesh->index_count == 0) {
        if (draw) {
            draw->index_count = 0;
        }
        return;
    }

    if (!draw) {
        if (renderer->chunk_count == renderer->chunk_capacity) {
            const size_t next = renderer->chunk_capacity ? renderer->chunk_capacity * 2 : 256;
            BgChunkDraw *grown = (BgChunkDraw *)realloc(renderer->chunks, next * sizeof(BgChunkDraw));
            if (!grown) {
                return;
            }
            renderer->chunks = grown;
            renderer->chunk_capacity = next;
        }
        draw = &renderer->chunks[renderer->chunk_count++];
        memset(draw, 0, sizeof(*draw));
        draw->key = key;
        glGenVertexArrays(1, &draw->vao);
        glGenBuffers(1, &draw->vbo);
        glGenBuffers(1, &draw->ibo);

        glBindVertexArray(draw->vao);
        glBindBuffer(GL_ARRAY_BUFFER, draw->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw->ibo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, uv));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BgVertex), (void *)offsetof(BgVertex, ao));
        glBindVertexArray(0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, draw->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->vertex_count * sizeof(BgVertex)),
                 mesh->vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)),
                 mesh->indices, GL_STATIC_DRAW);
    draw->index_count = (GLsizei)mesh->index_count;
}

void renderer_clear_chunks(BgRenderer *renderer) {
    for (size_t i = 0; i < renderer->chunk_count; ++i) {
        BgChunkDraw *draw = &renderer->chunks[i];
        if (draw->vbo) glDeleteBuffers(1, &draw->vbo);
        if (draw->ibo) glDeleteBuffers(1, &draw->ibo);
        if (draw->vao) glDeleteVertexArrays(1, &draw->vao);
    }
    renderer->chunk_count = 0;
}

void renderer_get_size(BgRenderer *renderer, int *width, int *height) {
    GLint viewport[4] = { 0, 0, 1280, 720 };
    glGetIntegerv(GL_VIEWPORT, viewport);
    renderer->width = viewport[2] > 0 ? viewport[2] : 1280;
    renderer->height = viewport[3] > 0 ? viewport[3] : 720;
    if (width) *width = renderer->width;
    if (height) *height = renderer->height;
}

static void draw_sky(BgRenderer *renderer) {
    int width = 1280;
    int height = 720;
    renderer_get_size(renderer, &width, &height);

    const float top[3] = { renderer->sky[0] * 0.88f, renderer->sky[1] * 0.88f,
                           renderer->sky[2] * 0.88f };
    const float bottom[3] = { renderer->fog_color[0], renderer->fog_color[1],
                              renderer->fog_color[2] };
    const float w = (float)width;
    const float h = (float)height;
    const float verts[6][6] = {
        { 0.0f, 0.0f, top[0], top[1], top[2], 1.0f },
        { w, 0.0f, top[0], top[1], top[2], 1.0f },
        { w, h, bottom[0], bottom[1], bottom[2], 1.0f },
        { 0.0f, 0.0f, top[0], top[1], top[2], 1.0f },
        { w, h, bottom[0], bottom[1], bottom[2], 1.0f },
        { 0.0f, h, bottom[0], bottom[1], bottom[2], 1.0f }
    };

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(renderer->ui_program);
    glUniform2f(glGetUniformLocation(renderer->ui_program, "screenSize"), w, h);
    glBindVertexArray(renderer->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(verts), verts, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void renderer_draw(BgRenderer *renderer, const BgCamera *camera) {
    renderer->draw_calls = 0;

    glClearColor(renderer->sky[0], renderer->sky[1], renderer->sky[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_sky(renderer);

    float view[16];
    float projection[16];
    camera_view_matrix(camera, view);
    camera_projection_matrix(camera, projection);

    glUseProgram(renderer->program);
    const float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "model"), 1, GL_FALSE, identity);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(renderer->program, "projection"), 1, GL_FALSE, projection);
    glUniform3f(glGetUniformLocation(renderer->program, "viewPos"), camera->x, camera->y, camera->z);
    glUniform3fv(glGetUniformLocation(renderer->program, "ambientColor"), 1, renderer->ambient);
    glUniform3fv(glGetUniformLocation(renderer->program, "lightColor"), 1, renderer->light_color);
    glUniform3fv(glGetUniformLocation(renderer->program, "fogColor"), 1, renderer->fog_color);
    glUniform1f(glGetUniformLocation(renderer->program, "fogStart"), renderer->fog_start);
    glUniform1f(glGetUniformLocation(renderer->program, "fogDensity"), renderer->fog_density);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->atlas);
    glUniform1i(glGetUniformLocation(renderer->program, "texAtlas"), 0);

    for (size_t i = 0; i < renderer->chunk_count; ++i) {
        const BgChunkDraw *draw = &renderer->chunks[i];
        if (draw->index_count == 0) {
            continue;
        }
        glBindVertexArray(draw->vao);
        glDrawElements(GL_TRIANGLES, draw->index_count, GL_UNSIGNED_INT, NULL);
        renderer->draw_calls++;
    }
    glBindVertexArray(0);
}

/* ------------------------------------------------------------------- UI */

static bool ui_reserve(BgUi *ui, size_t floats_needed) {
    if (ui->capacity >= floats_needed) {
        return true;
    }
    size_t next = ui->capacity ? ui->capacity : 4096;
    while (next < floats_needed) {
        next *= 2;
    }
    float *grown = (float *)realloc(ui->verts, next * sizeof(float));
    if (!grown) {
        return false;
    }
    ui->verts = grown;
    ui->capacity = next;
    return true;
}

void ui_clear(BgUi *ui) {
    ui->floats = 0;
}

void ui_free(BgUi *ui) {
    free(ui->verts);
    ui->verts = NULL;
    ui->floats = 0;
    ui->capacity = 0;
}

void ui_rect(BgUi *ui, float x, float y, float w, float h, const float color[4]) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    if (!ui_reserve(ui, ui->floats + 6 * 6)) {
        return;
    }
    const float x1 = x + w;
    const float y1 = y + h;
    const float verts[6][6] = {
        { x, y, color[0], color[1], color[2], color[3] },
        { x1, y, color[0], color[1], color[2], color[3] },
        { x1, y1, color[0], color[1], color[2], color[3] },
        { x, y, color[0], color[1], color[2], color[3] },
        { x1, y1, color[0], color[1], color[2], color[3] },
        { x, y1, color[0], color[1], color[2], color[3] }
    };
    memcpy(&ui->verts[ui->floats], verts, sizeof(verts));
    ui->floats += 6 * 6;
}

/* 5x7 bitmap font; bit n of a column is the pixel on row n (0 = top). */
typedef struct {
    char code;
    unsigned char columns[5];
} Glyph;

static const Glyph kGlyphs[] = {
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { '!', { 0x00, 0x00, 0x5F, 0x00, 0x00 } },
    { '"', { 0x00, 0x07, 0x00, 0x07, 0x00 } },
    { '#', { 0x14, 0x7F, 0x14, 0x7F, 0x14 } },
    { '%', { 0x23, 0x13, 0x08, 0x64, 0x62 } },
    { '&', { 0x36, 0x49, 0x55, 0x22, 0x50 } },
    { '\'', { 0x00, 0x05, 0x03, 0x00, 0x00 } },
    { '(', { 0x00, 0x1C, 0x22, 0x41, 0x00 } },
    { ')', { 0x00, 0x41, 0x22, 0x1C, 0x00 } },
    { '*', { 0x14, 0x08, 0x3E, 0x08, 0x14 } },
    { '+', { 0x08, 0x08, 0x3E, 0x08, 0x08 } },
    { ',', { 0x00, 0x50, 0x30, 0x00, 0x00 } },
    { '-', { 0x08, 0x08, 0x08, 0x08, 0x08 } },
    { '.', { 0x00, 0x60, 0x60, 0x00, 0x00 } },
    { '/', { 0x20, 0x10, 0x08, 0x04, 0x02 } },
    { '0', { 0x3E, 0x51, 0x49, 0x45, 0x3E } },
    { '1', { 0x00, 0x42, 0x7F, 0x40, 0x00 } },
    { '2', { 0x42, 0x61, 0x51, 0x49, 0x46 } },
    { '3', { 0x21, 0x41, 0x45, 0x4B, 0x31 } },
    { '4', { 0x18, 0x14, 0x12, 0x7F, 0x10 } },
    { '5', { 0x27, 0x45, 0x45, 0x45, 0x39 } },
    { '6', { 0x3C, 0x4A, 0x49, 0x49, 0x30 } },
    { '7', { 0x01, 0x71, 0x09, 0x05, 0x03 } },
    { '8', { 0x36, 0x49, 0x49, 0x49, 0x36 } },
    { '9', { 0x06, 0x49, 0x49, 0x29, 0x1E } },
    { ':', { 0x00, 0x36, 0x36, 0x00, 0x00 } },
    { ';', { 0x00, 0x56, 0x36, 0x00, 0x00 } },
    { '<', { 0x08, 0x14, 0x22, 0x41, 0x00 } },
    { '=', { 0x14, 0x14, 0x14, 0x14, 0x14 } },
    { '>', { 0x00, 0x41, 0x22, 0x14, 0x08 } },
    { '?', { 0x02, 0x01, 0x51, 0x09, 0x06 } },
    { '@', { 0x32, 0x49, 0x79, 0x41, 0x3E } },
    { 'A', { 0x7E, 0x11, 0x11, 0x11, 0x7E } },
    { 'B', { 0x7F, 0x49, 0x49, 0x49, 0x36 } },
    { 'C', { 0x3E, 0x41, 0x41, 0x41, 0x22 } },
    { 'D', { 0x7F, 0x41, 0x41, 0x22, 0x1C } },
    { 'E', { 0x7F, 0x49, 0x49, 0x49, 0x41 } },
    { 'F', { 0x7F, 0x09, 0x09, 0x09, 0x01 } },
    { 'G', { 0x3E, 0x41, 0x49, 0x49, 0x7A } },
    { 'H', { 0x7F, 0x08, 0x08, 0x08, 0x7F } },
    { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
    { 'J', { 0x20, 0x40, 0x41, 0x3F, 0x01 } },
    { 'K', { 0x7F, 0x08, 0x14, 0x22, 0x41 } },
    { 'L', { 0x7F, 0x40, 0x40, 0x40, 0x40 } },
    { 'M', { 0x7F, 0x02, 0x0C, 0x02, 0x7F } },
    { 'N', { 0x7F, 0x04, 0x08, 0x10, 0x7F } },
    { 'O', { 0x3E, 0x41, 0x41, 0x41, 0x3E } },
    { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
    { 'Q', { 0x3E, 0x41, 0x51, 0x21, 0x5E } },
    { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
    { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
    { 'T', { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
    { 'U', { 0x3F, 0x40, 0x40, 0x40, 0x3F } },
    { 'V', { 0x1F, 0x20, 0x40, 0x20, 0x1F } },
    { 'W', { 0x3F, 0x40, 0x38, 0x40, 0x3F } },
    { 'X', { 0x63, 0x14, 0x08, 0x14, 0x63 } },
    { 'Y', { 0x07, 0x08, 0x70, 0x08, 0x07 } },
    { 'Z', { 0x61, 0x51, 0x49, 0x45, 0x43 } }
};

static const Glyph *find_glyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    for (size_t i = 0; i < sizeof(kGlyphs) / sizeof(kGlyphs[0]); ++i) {
        if (kGlyphs[i].code == c) {
            return &kGlyphs[i];
        }
    }
    return NULL;
}

void ui_text(BgUi *ui, float x, float y, float scale, const char *text, const float color[4]) {
    float pen = x;
    for (const char *p = text; *p; ++p) {
        const Glyph *glyph = find_glyph(*p);
        if (glyph) {
            for (int col = 0; col < 5; ++col) {
                const unsigned char bits = glyph->columns[col];
                if (!bits) {
                    continue;
                }
                for (int row = 0; row < 7; ++row) {
                    if (bits & (1u << row)) {
                        ui_rect(ui, pen + (float)col * scale, y + (float)row * scale,
                                scale, scale, color);
                    }
                }
            }
        }
        pen += 6.0f * scale;
    }
}

float ui_text_width(const char *text, float scale) {
    const size_t length = strlen(text);
    return length ? (float)length * 6.0f * scale - scale : 0.0f;
}

void ui_text_centered(BgUi *ui, float center_x, float y, float scale, const char *text,
                      const float color[4]) {
    ui_text(ui, center_x - ui_text_width(text, scale) * 0.5f, y, scale, text, color);
}

void renderer_draw_ui(BgRenderer *renderer, const BgUi *ui) {
    if (!renderer->ui_program || ui->floats == 0) {
        return;
    }
    int width = 1280;
    int height = 720;
    renderer_get_size(renderer, &width, &height);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(renderer->ui_program);
    glUniform2f(glGetUniformLocation(renderer->ui_program, "screenSize"),
                (float)width, (float)height);
    glBindVertexArray(renderer->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->ui_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ui->floats * sizeof(float)), ui->verts,
                 GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(ui->floats / 6));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void block_color(BlockId block, float rgb[3]) {
    const int tile = block_tile(block, 2);
    const TileDef *def = &kTiles[tile];
    rgb[0] = (float)def->r / 255.0f;
    rgb[1] = (float)def->g / 255.0f;
    rgb[2] = (float)def->b / 255.0f;
}

void renderer_draw_hud(BgRenderer *renderer, const BgPlayer *player) {
    int width = 1280;
    int height = 720;
    renderer_get_size(renderer, &width, &height);
    const float w = (float)width;
    const float h = (float)height;

    BgUi ui;
    memset(&ui, 0, sizeof(ui));

    const float white[4] = { 1.0f, 1.0f, 1.0f, 0.85f };
    ui_rect(&ui, w * 0.5f - 10.0f, h * 0.5f - 1.0f, 20.0f, 2.0f, white);
    ui_rect(&ui, w * 0.5f - 1.0f, h * 0.5f - 10.0f, 2.0f, 20.0f, white);

    const float slot = 44.0f;
    const float gap = 4.0f;
    const float total = 9.0f * slot + 8.0f * gap;
    const float bar_x = (w - total) * 0.5f;
    const float bar_y = h - slot - 18.0f;

    for (int i = 0; i < 9; ++i) {
        const float x = bar_x + (float)i * (slot + gap);
        const bool selected = (i == player->selected);
        const float frame[4] = { selected ? 1.0f : 0.20f, selected ? 1.0f : 0.20f,
                                 selected ? 1.0f : 0.24f, selected ? 0.95f : 0.80f };
        ui_rect(&ui, x - 3.0f, bar_y - 3.0f, slot + 6.0f, slot + 6.0f, frame);

        const float inner[4] = { 0.04f, 0.04f, 0.05f, 0.85f };
        ui_rect(&ui, x, bar_y, slot, slot, inner);

        float rgb[3];
        block_color(player->hotbar[i], rgb);
        const float icon[4] = { rgb[0], rgb[1], rgb[2], 0.95f };
        ui_rect(&ui, x + 5.0f, bar_y + 5.0f, slot - 10.0f, slot - 10.0f, icon);

        const float top[4] = {
            rgb[0] * 1.45f > 1.0f ? 1.0f : rgb[0] * 1.45f,
            rgb[1] * 1.45f > 1.0f ? 1.0f : rgb[1] * 1.45f,
            rgb[2] * 1.45f > 1.0f ? 1.0f : rgb[2] * 1.45f,
            0.9f
        };
        ui_rect(&ui, x + 5.0f, bar_y + 5.0f, slot - 10.0f, 5.0f, top);

        const float bottom[4] = { rgb[0] * 0.6f, rgb[1] * 0.6f, rgb[2] * 0.6f, 0.9f };
        ui_rect(&ui, x + 5.0f, bar_y + slot - 11.0f, slot - 10.0f, 6.0f, bottom);
    }

    for (int i = 0; i < 10; ++i) {
        const bool full = player->health > i * 2;
        const float heart[4] = { full ? 0.86f : 0.28f, full ? 0.16f : 0.12f,
                                 full ? 0.20f : 0.14f, 0.95f };
        ui_rect(&ui, 20.0f + (float)i * 22.0f, h - 44.0f, 16.0f, 16.0f, heart);
    }

    renderer_draw_ui(renderer, &ui);
    ui_free(&ui);
}
