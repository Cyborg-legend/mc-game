# BlockForge

A small voxel sandbox written in C11 on GLFW and OpenGL 3.3. Everything you
see is generated at runtime: there are no image or sound files in the
repository. The block atlas is painted into memory at startup and every sound
is synthesised by a tiny waveOut mixer.

Mine, build, craft, and carry the Emberheart through a rift to put down the
Cinder Alpha, then strike it into a voidstone frame to open the way to
**Cinderhold** and its Cinder Sovereign. When the Sovereign roots itself to
slam the floor, a magma ring fills around the blast radius: the lit arc shows
how much of its wind-up is left, and the ring marks where the danger ends.

Every Sovereign kill leaves a **Cinder Core**. Reforge it at a crafting table
with three Ember Shards and two Sticks for the **Sovereign Pick**: the same
tier as the Ember Pick but about 40% faster and far longer-lived, so the boss
finally pays out something to play with.

## Controls

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` | Walk (rebindable) |
| Mouse | Look |
| `Shift` | Sprint (rebindable) |
| `Space` | Jump / swim up (rebindable) |
| Left mouse (hold) | Mine the block under the crosshair |
| Right mouse | Place the held block, use an item, or open a chest |
| `1`–`9`, mouse wheel | Choose a hotbar slot |
| `E` | Inventory and crafting (rebindable) |
| `Q` | Drop the selected item (rebindable) |
| `F` | Eat meat (rebindable) |
| `T` | Sort the open chest, or your pack |
| Shift + click | Move a whole stack between the chest and your pack |
| `F2` | Save a screenshot to `screenshots/` |
| `F3` | Debug overlay |
| `Esc` | Pause (saves on quit) |

Walk, sprint, jump, inventory, drop and eat can be rebound in **Settings →
Controls**: pick the row, press Enter, then press the new key. Backspace resets
one row to its default. The table lives in `worlds/settings.cfg`.

Each row of the world list shows a small seed-derived preview of the world, the
realm it was last saved in and how long ago it was last played (`PLAYED 3 H
AGO`, `NEVER PLAYED`). The stamp is written when a world is opened.

Settings also cover mouse sensitivity, field of view, render distance,
inverted look, fullscreen, a VSync toggle and a frame cap; all of them are
saved with the bindings. The frame cap (60, 120, 144 or 240 FPS, or off) only
applies while VSync is off, where it stops the game rendering as fast as the
graphics card allows.

A tool's remaining life shows in its slot's wear bar, in a hover tooltip in the
inventory and chest, and as a warning above the hotbar once it is down to its
last quarter.

Losing window focus pauses the game and writes the world out, so alt-tabbing
is always safe.

The strip in the top-right corner is the rift compass: it names the nearest lit
rift or Cinderhold gate, how far away it is in blocks, and which way to turn.
It stays hidden while no way through is within range.

## Building

Requirements: CMake 3.18+, a C11 compiler, GLFW 3.4 and GLEW 2.2. Both
libraries may be unpacked beside the sources in `glfw_deps/` and `glew_deps/`
as the CI workflow does, or found through the system package manager. On Linux
the build accepts a distro GLFW through its CMake config, its `pkg-config`
file, or a plain header and library search, so older packages work too.

Windows (Visual Studio):

```
cmake -B build -S .
cmake --build build --config Release
```

Linux:

```
sudo apt install cmake gcc pkg-config libglfw3-dev libglew-dev
cmake -B build -S .
cmake --build build
./build/blockforge
```

The headless checks in `tests/` need no window or GPU. They build alongside the
game (`-DBUILD_TESTING=ON` is the default) and run with
`ctest --test-dir build --output-on-failure`.

### Experimental: chunk generation on a worker thread

```
cmake -B build -S . -DBLOCKFORGE_ASYNC_CHUNKS=ON
cmake --build build
```

With that switch on, one POSIX worker thread builds chunk terrain from the seed
and hands block arrays back to the main thread, which still does the lighting,
tree decoration and meshing. The queue is bounded and refuses duplicate chunks,
and a client that falls behind drops finished chunks rather than blocking the
worker. It is off by default, is not available on Windows, and keeps the
synchronous path as the supported one.

On Windows, GLFW and GLEW are linked statically, so `blockforge.exe` runs on
its own with no DLLs beside it. Every push to `main` builds the exe and a
`blockforge-windows.zip` in the GitHub Actions workflow.

## Your own textures

The atlas is painted in code at startup, so the game needs no asset files. To
replace that art, drop files into a `textures/` folder beside the executable;
they are loaded over the painted tiles when the atlas is built:

- `textures/atlas.png` (or `.bmp`) at exactly 512x512 (16x16 tiles of 32x32)
  replaces the whole atlas at once.
- `textures/tile_NN.png` (or `.bmp`) replaces one tile. `NN` is the tile index
  in the atlas, two digits: `tile_01` is stone, `tile_02` dirt, `tile_03` grass
  top, and so on. Any size works; the image is scaled to 32x32.

PNGs may be 8-bit grey, grey+alpha, RGB, RGBA or palette, and must not be
interlaced. BMPs may be uncompressed 24- or 32-bit. A missing folder, an
unreadable file or a wrong-sized full atlas is ignored and the painted tile is
kept. Applied overrides are reported on stderr at startup.

Every block face turns its tile by a quarter and mirrors it according to a
hash of its world position, so a long merged wall still shows per-block
variation even though it is drawn as one quad.

## Saves

Worlds live in `worlds/` next to the executable:

- `worlds/worlds.idx` lists the worlds, with a `.bak` copy of the previous list.
  Each line is `id<TAB>name<TAB>seed<TAB>realm<TAB>last played`, where the last
  field is a Unix stamp and `0` means the world has never been opened. Lists
  written by older builds (without the stamp) still load.
- `worlds/slot_<id>.sav`, `slot_<id>_under.sav`, `slot_<id>_cinder.sav` are the
  Upperworld, Underworld and Cinderhold saves for one world.
- Each save keeps its previous generation as `<name>.sav.bak`. If the main file
  is ever damaged, the game loads the backup instead of refusing the world.
- `worlds/settings.cfg` holds sensitivity, FOV, render distance, invert-look,
  fullscreen, VSync, the frame cap and the key bindings.
- `worlds/last_crash.log` is written on a fatal signal. It names the build,
  realm, position, health, quest stage and world, and includes the OpenGL
  driver string plus the last few messages the driver sent.
- `screenshots/screenshot_NNN.bmp` is where `F2` writes 24-bit BMP captures.

## Licence

No licence file has been chosen yet. Pick one before distributing builds.
