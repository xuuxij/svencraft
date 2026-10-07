# Building Svencraft

Windows x64 only for now. Commands are for Git Bash from the project root unless they say otherwise.

**The short way:** install the prerequisites below (Sven Co-op and its SDK, the Visual Studio C++ build tools,
Python 3, Git), clone with `--recursive`, and run `setup.bat` (`tools/setup.py`). It finds Sven Co-op and the SDK
in any Steam library, installs numpy and Pillow, fetches missing submodules, downloads SDL2 into `deps/SDL2_VC`,
configures and builds the engine (with `--enable-utils` for `mdldec.exe`) and the game, copies what they make into
`run/` (`xash3d.exe`, `xash.dll`, `ref_gl.dll`, `menu.dll`, `filesystem_stdio.dll`, `mdldec.exe`, `SDL2.dll`,
`svencraft/extras.pk3`, the game DLLs), runs `tools/setup_run.py`, then every generator below whose output is
missing, ending with the map. Again with `--rebuild` it redoes everything; `--check` only checks. The rest of this
page is what it does, step by step.

## Prerequisites

| What | Why | Notes |
|---|---|---|
| Visual Studio Build Tools, "Desktop development with C++" | compiles the engine and game | built with VS 2019 Build Tools (MSVC 16.11); waf finds MSVC itself |
| Python 3 with `numpy`, `Pillow` | waf, every generator | `python -m pip install numpy pillow` (3.11 used) |
| Git for Windows (Git Bash) | clone, the `.sh` scripts | |
| Sven Co-op (Steam app 225840, free) | the content the game runs on | the game reads it through `run/svencoop` |
| Sven Co-op SDK (Steam, Tools) | `modelling/studiomdl.exe`, `sprites/sprgen.exe`, `mapping/compilers/SC-*.exe` | |
| SDL2 dev package for VC (`SDL2-devel-2.32.10-VC.zip`) | engine build, `SDL2.dll` | unpack into `deps/`, rename the folder to `SDL2_VC` (`setup.bat` does it) |
| (optional) Xash3D FWGS Windows build (`xash3d-fwgs-win32-amd64.7z`) | only its FFmpeg DLLs (intro videos); our engine build makes everything else it holds | from the `continuous` release of FWGS/xash3d-fwgs |

Sven Co-op and the SDK are expected in Steam's default library. If they are elsewhere, set (in the shell or
Windows' environment) `SVENCRAFT_SVEN` to the `...\Sven Co-op\svencoop` folder and `SVENCRAFT_SDK` to the
`...\Sven Co-op SDK` folder. `python tools/sc_paths.py` prints what the tools will use.

```sh
mkdir -p deps
curl -L -o deps/SDL2.zip https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip
(cd deps && unzip -q SDL2.zip && mv SDL2-2.32.10 SDL2_VC)
```

## Fresh checkout setup

```sh
git clone --recursive https://github.com/xuuxij/svencraft.git   # private: needs access
cd svencraft
python tools/setup_run.py        # report + create what is missing; safe to re-run
```

`setup_run.py` creates the folders the generators write into, the `run/svencoop` junction to your Sven Co-op
content, `run/svencraft/extras.pk3` (from `run/valve/` when an engine package is there; `setup.bat` copies the
engine build's own), and copies Sven Co-op's
`sprites/pain.spr` to `run/svencraft/sprites/640_pain.spr` / `320_pain.spr` (the HUD damage indicator) and its
`sound/default_sentences.txt` to `run/svencraft/sound/sentences.txt` (the name the engine and the game read the
spoken sentences from). It never overwrites anything and lists what is still missing.

## Engine and game

```sh
cd engine && python waf configure -T release -8 --msvc_targets=x64 -s ../deps/SDL2_VC --enable-utils && cd ..
cd game   && python waf configure -T release -8 --msvc_targets=x64 --prefix=../run && cd ..
sh tools/build.sh engine game     # or just: sh tools/build.sh   (= game)
```

`tools/build.sh [game] [engine] [map]` builds the parts asked for and installs them:
`game/build/dlls/hl_amd64.dll` -> `run/svencraft/dlls/`, `game/build/cl_dll/client_amd64.dll` ->
`run/svencraft/cl_dlls/`, `engine/build/engine/xash.dll` and `engine/build/ref/gl/ref_gl.dll` -> `run/`
(`map` runs `maps_src/make_town.py`). It closes a running `xash3d.exe` first, writes the full log to
`game|engine/build/last.log` and installs nothing when waf reports a failed build. Re-run `waf configure` after
adding source files (the wscripts glob them at configure time).

## Generated assets

The repo keeps generators, not their outputs (most outputs are derived from Sven Co-op content). Run them after
the first build and whenever their inputs change. The first four depend on each other in this order.

| Tool | Writes (in `run/svencraft/` unless noted) | Reads |
|---|---|---|
| `tools/make_blocktex.py` | `gfx/blocks/*.tga`, `scripts/blocks.txt` | `assets_src/blocktex/tex/*.bmp`; six realistic block textures from Sven's WADs (`tools/wadtex.py`) |
| `tools/make_blockassets.py` | `models/svencraft/blockitem.mdl`, `sprites/svencraft/hotbar.spr` | blocks.txt + gfx/blocks; SDK studiomdl, sprgen |
| `tools/make_items.py` | `game/common/sc_items.h` (repo), `sprites/svencraft/items.spr`, `models/svencraft/itemflat.mdl`, `tools/shots/items_preview.png` | `tools/item_art.py`, the block art; SDK |
| `tools/make_hand.py` | `models/svencraft/v_schand.mdl` | Sven's `models/v_crowbar.mdl` (decompiled with `run/mdldec.exe`), the bitmaps in `tools/build_blockassets` and `tools/build_items`, `game/common/sc_items.h`; SDK studiomdl |
| `tools/make_tools.py` (+ `toolmdl.py`) | `models/svencraft/{v,p,w}_sc{pickaxe,shovel,axe}{1..4}.mdl` | Sven's `{v,p,w}_crowbar.mdl` |
| `tools/make_creeper.py` | `models/svencraft/creeper.mdl` | (drawn in code); SDK studiomdl |
| `tools/make_font.py` | `sprites/svencraft/font.spr` | Pillow's built-in font; SDK sprgen |
| `tools/make_materials.py` | `sound/materials.txt` | Sven's `sound/materials.txt` |
| `tools/make_skillcfg.py` | `skill.cfg` (without it every monster has 0 health and guns do no damage) | `game/dlls/game.cpp`, Sven's `skill.cfg` |
| `maps_src/make_town.py` | `maps/svencraft_sandbox.bsp`, `maps/svencraft_sandbox.dyn`, `svencraft_dyn.wad` (and `maps_src/svencraft_sandbox.*`) | `town_v3.py`, `maplib.py`, Sven's WADs; SDK compilers (a few minutes) |
| `tools/make_testmap.py` | `maps/svencraft_world.bsp` (the old empty test box) | `assets_src/testmap/svencraft_old.wad`, Sven's `halflife.wad`; SDK compilers |
| `tools/setup_run.py` | `sprites/640_pain.spr`, `sprites/320_pain.spr`, `sound/sentences.txt`, `extras.pk3`, `run/svencoop` | Sven's `sprites/pain.spr` and `sound/default_sentences.txt`, `run/valve/extras.pk3` |

```sh
python tools/make_blocktex.py && python tools/make_blockassets.py && python tools/make_items.py && python tools/make_hand.py
python tools/make_tools.py && python tools/make_creeper.py && python tools/make_font.py
python tools/make_materials.py && python tools/make_skillcfg.py
python maps_src/make_town.py
```

Work folders: `tools/build_<name>/` (override the parent with `SVENCRAFT_BUILD`); `SVENCRAFT_GAMEDIR` sends the
outputs that the tools write through `sc_paths.GAMEDIR` elsewhere (`make_font.py`, `make_skillcfg.py` and the
extras of `make_items.py` still write under the checkout). New block ids also go in
`game/dlls/svencraft/sc_world.h`, `g_SCBlocks` and the `BLOCKS` names in `make_items.py`; items and recipes are
edited in `tools/make_items.py` and `game/common/sc_recipes.h`.

## What is in run/svencraft

| Kind | Files |
|---|---|
| Hand-maintained (in the repo) | `gameinfo.txt`, `userconfig.d/svencraft_keys.cfg`, `userconfig.d/svencraft_settings.cfg` (forced at every start, after `config.cfg`), `sprites/weapon_sc_{axe,pickaxe,shovel}.txt` |
| Prebuilt original art (in the repo) | `sprites/svencraft/{counts,cracks,cursor,sctools,sctools_s,craftpanel_l,craftpanel_r,craftslots,slot,slot_sel}.spr`, made by the prototype's scripts in `legacy/angelscript_mod/build/sprites/` (the last five are not used by the current HUD) |
| Built | `dlls/hl_amd64.dll`, `cl_dlls/client_amd64.dll` (`tools/build.sh game`) |
| Generated | everything in the table above |
| Written by the engine at run time (never commit) | `config.cfg` (+ `.bak`), `video.cfg`, `opengl.cfg`, `vfs.cfg`, `.xash_id`, `voice_ban.dt`, `media/cdaudio.txt` (default playlist), `.fontcache/`, `scrshots/`, `maps/graphs/*.nod|*.nrp` (AI node graph, built on the first load of a map); `run/engine.log` with `-log` |
| Written by tests | `tests/*.cfg` (`tools/runtest.sh`), `tools/shots/*.png` |

## Play

`Play Svencraft.bat` starts `run/xash3d.exe -game svencraft -console +sv_cheats 1 +map svencraft_sandbox`.
`run/svencraft/gameinfo.txt` sets `basedir "svencoop"`, so the engine falls back to Sven Co-op's content for
everything the game folder does not have.

## Troubleshooting

- Monsters die at once / guns do nothing: `skill.cfg` is missing; run `tools/make_skillcfg.py`.
- Blocks without textures, a missing hand/tool/creeper model or HUD icons: run the generator chain from
  `make_blocktex.py` (and check `python tools/setup_run.py --check`).
- `studiomdl` / `sprgen` / `SC-*.exe` not found: the SDK is not where `tools/sc_paths.py` looks (`SVENCRAFT_SDK`).
- "Could not load HUD sprite sprites/640_pain.spr": run `tools/setup_run.py`.
- Engine build cannot find SDL2: `deps/SDL2_VC` must contain `include/` and `lib/x64/`.
