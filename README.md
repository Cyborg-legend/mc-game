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
| `W` `A` `S` `D` | Walk |
| Mouse | Look |
| `Shift` | Sprint |
| `Space` | Jump / swim up |
| Left mouse (hold) | Mine the block under the crosshair |
| Right mouse | Place the held block, use an item, or open a chest |
| `1`–`9`, mouse wheel | Choose a hotbar slot |
| `E` or `C` | Inventory and crafting |
| `F3` | Debug overlay |
| `Esc` | Pause (saves on quit) |

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

On Windows, GLFW and GLEW are linked statically, so `blockforge.exe` runs on
its own with no DLLs beside it. Every push to `main` builds the exe and a
`blockforge-windows.zip` in the GitHub Actions workflow.

## Saves

Worlds live in `worlds/` next to the executable:

- `worlds/worlds.idx` lists the worlds, with a `.bak` copy of the previous list.
- `worlds/slot_<id>.sav`, `slot_<id>_under.sav`, `slot_<id>_cinder.sav` are the
  Upperworld, Underworld and Cinderhold saves for one world.
- Each save keeps its previous generation as `<name>.sav.bak`. If the main file
  is ever damaged, the game loads the backup instead of refusing the world.
- `worlds/settings.cfg` holds sensitivity, FOV, render distance, invert-look and
  fullscreen.

## Licence

No licence file has been chosen yet. Pick one before distributing builds.
