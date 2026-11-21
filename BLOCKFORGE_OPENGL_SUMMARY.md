# BlockForge OpenGL 3.3 Renderer

Added OpenGL 3.3 renderer support for systems without Vulkan (like Intel HD 2500 graphics).

## New Files Created

### `blockforge/opengl/shaders/vertex.glsl`
Vertex shader for chunk rendering with position, normal, UV, and AO attributes. Supports model/view/projection matrices and lighting uniforms.

### `blockforge/opengl/shaders/fragment.glsl`
Fragment shader with per-block lighting (ambient, diffuse, specular), AO application, and block-type color mapping. Includes a 16-block type color palette with approximate Minecraft-like colors. Water has semi-transparent rendering.

### `blockforge/opengl/shader.h`
Shader class wrapper for compiling and linking GLSL shaders. Provides uniform-setting utilities (setMat4, setVec3, setFloat, setInt).

### `blockforge/opengl/opengl_renderer.h`
OpenGLRenderer class using modern OpenGL 3.3 core profile:
- Manages shader program, VAO/VBO/IBO
- Uploads chunk meshes from greedy mesher
- Frustum/culing via distance test (configurable render distance 8-64)
- Adaptive render distance based on frame time (updates every 10s)
- Texture atlas support, light position, render distance uniforms
- Camera with yaw/pitch/fov, perspective projection

### `blockforge/opengl/opengl_renderer.cpp`
Full implementation of OpenGLRenderer:
- GLEW initialization and OpenGL version reporting
- Depth testing, cull face enabled
- VAO/VBO attribute layout: position (3), normal (3), UV (2), AO (1)
- Mesh upload/update per chunk
- Chunk visibility testing against camera frustum
- Render loop drawing all visible chunks
- Adaptive render distance smoothing

### `blockforge/renderer/opengl/CMakeLists.txt`
New CMake target `opengl` compiling the renderer sources and linking against:
- `core` (ECS, world, chunks)
- `meshing` (greedy mesher)
- OpenGL32 (Windows) / OpenGL (non-Windows)
- GLEW and GLFW

## CMake Changes

### `blockforge/CMakeLists.txt`
- Added `option(BLOCKFORGE_VULKAN_RENDERER "Use Vulkan renderer" OFF)` 
- Conditionally links `vulkan` library only when Vulkan renderer is enabled
- When OFF (default), OpenGL renderer is used

### `blockforge/renderer/CMakeLists.txt`
- Conditional compilation: 
  - `BLOCKFORGE_VULKAN_RENDERER=ON`: includes vulkan/ shaders/ meshing subdirectories
  - `BLOCKFORGE_VULKAN_RENDERER=OFF` (default): includes opengl/ meshing subdirectories

### `blockforge/CMakeLists.txt` (top-level)
- `target_link_libraries(blockforge PRIVATE vulkan)` only when Vulkan option enabled

## Build Options

### Use OpenGL (recommended for Intel HD 2500):
```bash
cmake -B build -DBLOCKFORGE_VULKAN_RENDERER=OFF
cmake --build build
```

### Use Vulkan (original):
```bash
cmake -B build -DBLOCKFORGE_VULKAN_RENDERER=ON
cmake --build build
```

## Technical Details

- **Shader version**: `#version 330 core` (OpenGL 3.3)
- **Vertex shader**: Location-based attributes (0=pos, 1=normal, 2=uv, 3=ao)
- **Fragment shader**: Per-block lighting with Blinn-Phong specular, AO modulation, block-type color lookup from 256x256 texture atlas
- **Render distance**: Adaptive 8-64 chunks, updates every 10s based on frame time
- **Performance**: No Vulkan SDK required - just OpenGL 3.3+ capable GPU and standard GLFW/GLEW

## Compatibility

Intel HD 2500 (integrated graphics with OpenGL 3.3 support) should now be able to render BlockForge. The renderer uses core-profile OpenGL 3.3 features widely supported on hardware from ~2012 onward.