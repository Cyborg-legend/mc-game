#ifndef BLOCKFORGE_C_SHADERS_H
#define BLOCKFORGE_C_SHADERS_H

/* GLSL sources kept free of GL types so a headless check can read them
 * without a window, a GPU or the GLFW/GLEW headers. */

const char *shader_world_vertex(void);
const char *shader_world_fragment(void);
const char *shader_ui_vertex(void);
const char *shader_ui_fragment(void);

#endif
