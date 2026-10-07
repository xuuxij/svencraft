# Svencraft

A co-op shooter that looks and plays like Sven Co-op (Half-Life-style realistic maps, HL weapons and monsters,
the low-res classic look) while the Minecraft world breaks into it: rifts and caves open, blocky terrain and
Minecraft-style mobs spill through, and in the end Sven's own monsters side with the players against the
invaders. Everything Minecraft is original art and code (no Mojang assets).

It runs on our own forks of the open-source [Xash3D FWGS](https://github.com/FWGS/xash3d-fwgs) engine and the
[hlsdk-portable](https://github.com/FWGS/hlsdk-portable) game code, and reads Sven Co-op's content (models,
sounds, textures) from your own Sven Co-op install. The vision and the feature notes are in
[DESIGN.md](DESIGN.md); the history is in [DEVLOG.md](DEVLOG.md).

## Status (2026-10-07)

Early development, private. Everything is built and tested in one sandbox map (`svencraft_sandbox`: a small
Half-Life-style town over a generated block world with caves and a rift). Working:

- **Engine:** a Minecraft-style block world that every trace collides with (`engine/common/voxel.c`); the map's
  own geometry is diggable in block-sized cells with GoldSrc-style lightmaps, decals and per-pixel dynamic lights
  (`engine/common/dynworld.c`, `ref/gl/gl_dynworld.c`); Minecraft-style block light (torches, furnaces); blast
  muffling and ear ringing in the sound mixer; material sounds for blocks and the town.
- **Game:** world generation (caves, ores, the rift), mining and placing, tools and tool wear, Minecraft's
  inventory (with its click, drag and number-key shortcuts), crafting table, furnace and recipe book, the hotbar
  (tools and every HL gun as items), the creeper, Minecraft-ray explosions through blocks and the town, cars that
  burn and explode, breakable glass and wood, bullet penetration, recoil and spread, view feel (sway, bob, landing
  dip, blast shake), room acoustics, `sc_state` JSON for tests and the planned AI dungeon master.
- **Half-Life / Sven Co-op:** Opposing Force's weapons and monsters (merged from upstream), Sven's minigun, medkit
  (heal, revive players and allies), heavy weapons grunt and robot grunt; co-op rules (no friendly fire, the dead
  wait for a medic, antiblock); wall chargers, ladders, water (swimming, drowning, splashes), speech for the suit
  and the NPCs, fog and steam, Xen flora around the rift.
- **Planned:** more Minecraft mobs, saving, Sven-style co-op sections with per-section reset, detailed player
  stats, the AI dungeon master, a story/phase switch (Half-Life first, Minecraft later), day and night, renderer
  upgrades.

## Layout

```
DESIGN.md              vision, campaign arc, feature notes (the design reference)
DEVLOG.md              what happened when, newest first
Play Svencraft.bat     launches the sandbox from run/
docs/                  BUILDING, TESTING, UPSTREAM, LICENSES, PUBLISHING, REFERENCES, realism and town plans
engine/                Xash3D FWGS fork (upstream clone + our changes; 3rdparty/* are git submodules)
game/                  hlsdk-portable fork (dlls/svencraft/, cl_dll/svencraft/, common/sc_*.h are ours)
tools/                 asset generators (make_*.py), sc_paths.py, build.sh, runtest.sh, setup_run.py, publish/
maps_src/              the map generators (make_town.py, town_v3.py, maplib.py); outputs are not kept
assets_src/            hand-kept source art: the procedural block textures, the old test map's WAD
legacy/                the first prototype, a Sven Co-op AngelScript plugin (reference only)
run/                   runtime folder: engine package + built DLLs + run/svencraft (game folder) + run/svencoop
                       (a junction to your Sven Co-op content). Only run/svencraft's hand-kept files are tracked.
```

## Requirements

- Windows 10/11 x64
- Visual Studio Build Tools with the C++ workload (MSVC; built with VS 2019 Build Tools, MSVC 16.11)
- Python 3 (3.11 used) with `numpy` and `Pillow`; Git (with Git Bash for the `.sh` scripts)
- [Sven Co-op](https://store.steampowered.com/app/225840/) (free on Steam) and the Sven Co-op SDK (Steam: Tools)
  for `studiomdl`, `sprgen` and the map compilers
- SDL2 development package for Visual C++ (SDL 2.32.10 used) and the Xash3D FWGS Windows build
  (`xash3d-fwgs-win32-amd64.7z`) for the runtime files we do not build

## Quick start

From a clone made with `git clone --recursive` (the engine's and game's submodules are needed). Details and
troubleshooting: [docs/BUILDING.md](docs/BUILDING.md).

```sh
# 1. third-party pieces (not in the repo)
#    deps/SDL2_VC  <- SDL2-devel-2.32.10-VC.zip, unpacked and renamed
#    run/          <- the contents of xash3d-fwgs-win32-amd64.7z (xash3d.exe, SDL2.dll, menu.dll, valve/, ...)
# 2. engine and game (run from a VS x64 developer prompt or let waf find MSVC)
cd engine && python waf configure -T release -8 --msvc_targets=x64 -s ../deps/SDL2_VC && cd ..
cd game   && python waf configure -T release -8 --msvc_targets=x64 --prefix=../run && cd ..
python tools/setup_run.py            # run/ folders, the run/svencoop junction, Sven's pain sprite and sentences, extras.pk3
sh tools/build.sh engine game        # builds both and installs the DLLs into run/
# 3. generated assets (order matters for the first four), then the map
python tools/make_blocktex.py && python tools/make_blockassets.py && python tools/make_items.py && python tools/make_hand.py
python tools/make_tools.py && python tools/make_creeper.py && python tools/make_font.py
python tools/make_materials.py && python tools/make_skillcfg.py
python maps_src/make_town.py         # a few minutes: compiles svencraft_sandbox with the SDK compilers
# 4. play
"Play Svencraft.bat"
```

Paths are found from the checkout itself; when Sven Co-op or its SDK are not in Steam's default library, set
`SVENCRAFT_SVEN` / `SVENCRAFT_SDK` (see `tools/sc_paths.py`).

## Documentation

| File | What |
|---|---|
| [DESIGN.md](DESIGN.md) | vision, campaign arc, systems, planned features |
| [DEVLOG.md](DEVLOG.md) | dated history |
| [docs/BUILDING.md](docs/BUILDING.md) | prerequisites, building, the asset pipeline (which tool makes which file), the map |
| [docs/TESTING.md](docs/TESTING.md) | `tools/runtest.sh`, dev and diagnostic commands, cvars |
| [docs/UPSTREAM.md](docs/UPSTREAM.md) | the engine/game bases, merging upstream, what we changed |
| [docs/LICENSES.md](docs/LICENSES.md) | licences per part; what is not included |
| [docs/PUBLISHING.md](docs/PUBLISHING.md) | how this repo is exported, audited and pushed |
| [docs/REFERENCES.md](docs/REFERENCES.md) | open-source projects we can learn from or reuse |
| [docs/realism_plan.md](docs/realism_plan.md) | Half-Life / Sven realism work plan |
| [docs/town_v3_assets.md](docs/town_v3_assets.md) | the sandbox town's buildings, textures and props |
| [legacy/angelscript_mod/README.md](legacy/angelscript_mod/README.md) | the first prototype |

## Licensing (summary)

- `engine/`: Xash3D FWGS, GNU GPL v3 or later (per-file notices), with some Half-Life SDK files.
- `game/`: hlsdk-portable, under the Half-Life 1 SDK licence (`game/LICENSE`): free distribution only.
- Our own code and art outside those trees: not yet licensed; all rights reserved for now (private repo).
- Sven Co-op and Half-Life content is not included and must not be committed: the game reads it from your own
  Sven Co-op install at run time, and the generators derive files from it locally.

Full details: [docs/LICENSES.md](docs/LICENSES.md).
