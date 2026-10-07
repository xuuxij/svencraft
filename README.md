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
setup.bat              one command: builds everything and generates the assets (tools/setup.py)
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

## Playing it (one command)

Windows 10/11 x64. Install these first (all free):

1. **Sven Co-op** from Steam, and the **Sven Co-op SDK** (Steam: Library > Tools > "Sven Co-op SDK"). The game
   reads Sven's models, sounds and textures from your install; the SDK compiles the models and the map.
2. **Build Tools for Visual Studio** (https://visualstudio.microsoft.com/downloads/, under "Tools for Visual
   Studio") with the **"Desktop development with C++"** workload.
3. **Python 3** (https://www.python.org/downloads/; tick "Add python.exe to PATH").
4. **Git for Windows** (https://git-scm.com/download/win).

Then:

```sh
git clone --recursive https://github.com/xuuxij/svencraft.git
```

and double-click **`setup.bat`** in the `svencraft` folder. It checks the prerequisites (it finds Sven Co-op in
any Steam library), installs numpy and Pillow, downloads SDL2, builds the engine and the game, and generates the
assets and the sandbox map: about 10-20 minutes the first time, with a log in `setup.log`. When it says it is
ready, double-click **`Play Svencraft.bat`**.

Run `setup.bat` again after pulling changes (`git pull`, then `git submodule update --init --recursive`): it
only redoes what is missing; `setup.bat --rebuild` rebuilds and regenerates everything, `setup.bat --check` only
checks the prerequisites. If Sven Co-op or the SDK cannot be found, set `SVENCRAFT_SVEN` (the `...\svencoop`
folder) or `SVENCRAFT_SDK`.

## Building by hand

The steps `setup.bat` runs, for development (details and troubleshooting: [docs/BUILDING.md](docs/BUILDING.md)):

```sh
# engine (needs deps/SDL2_VC: SDL2-devel-2.32.10-VC.zip, unpacked and renamed) and game; waf finds MSVC
cd engine && python waf configure -T release -8 --msvc_targets=x64 -s ../deps/SDL2_VC --enable-utils && cd ..
cd game   && python waf configure -T release -8 --msvc_targets=x64 --prefix=../run && cd ..
sh tools/build.sh engine game        # builds both and installs the DLLs into run/ (setup.bat also installs
                                     # xash3d.exe, menu.dll, filesystem_stdio.dll, mdldec.exe, SDL2.dll, extras.pk3)
python tools/setup_run.py            # run/ folders, the run/svencoop junction, Sven's pain sprite and sentences
# generated assets (order matters for the first four), then the map
python tools/make_blocktex.py && python tools/make_blockassets.py && python tools/make_items.py && python tools/make_hand.py
python tools/make_tools.py && python tools/make_creeper.py && python tools/make_font.py
python tools/make_materials.py && python tools/make_skillcfg.py
python maps_src/make_town.py         # a few minutes: compiles svencraft_sandbox with the SDK compilers
```

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
