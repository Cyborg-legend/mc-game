# BlockForge

A small voxel sandbox written in C11 on GLFW and OpenGL 3.3. Everything you
see is generated at runtime: there are no image or sound files in the
repository. The block atlas is painted into memory at startup and every sound
is synthesised by a tiny waveOut mixer.

Mine, build, craft, and carry the Emberheart through a rift to put down the
Cinder Alpha, then strike it into a voidstone frame to open the way to
**Cinderhold** and its Cinder Sovereign.

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
as the CI workflow does, or found through the system package manager.

Windows (Visual Studio):

```
cmake -B build -S .
cmake --build build --config Release
```

Linux:

```
sudo apt install cmake gcc libglfw3-dev libglew-dev
cmake -B build -S .
cmake --build build
./build/blockforge
```

The headless checks in `tests/` need no window or GPU. They build alongside the
game (`-DBUILD_TESTING=ON` is the default) and run with
`ctest --test-dir build --output-on-failure`.

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
- `worlds/slot_<id>.sav`, `slot_<id>_under.sav`, `slot_<id>_cinder.sav` are the
  Upperworld, Underworld and Cinderhold saves for one world.
- Each save keeps its previous generation as `<name>.sav.bak`. If the main file
  is ever damaged, the game loads the backup instead of refusing the world.
- `worlds/settings.cfg` holds sensitivity, FOV, render distance, invert-look,
  fullscreen and the key bindings.
- `screenshots/screenshot_NNN.bmp` is where `F2` writes 24-bit BMP captures.

## Licence

No licence file has been chosen yet. Pick one before distributing builds.
