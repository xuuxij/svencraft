# Legacy: the Sven Co-op AngelScript prototype

Svencraft started on 2026-10-06 as a server plugin for an unmodified copy of Sven Co-op ("Svencraft Coop", a
copy of the game folder run under Sven Co-op's own Steam app id). It proved the ideas: mining and placing
blocks, digging the map's walls into materials, tools and tool tiers, a crafting window drawn from HUD sprites,
cars and props that break into parts, caves under a generated sandbox map, and world/inventory saves.
It ran into the closed engine's limits (about 1000 block entities, no way to change worldspawn at runtime), so
the project moved to its own engine and game code the same night (see `../../DEVLOG.md`). This folder is a
read-only snapshot kept for reference; nothing here is built by the current game.

## Contents
- `plugin/` — the AngelScript plugin as it was installed under
  `svencoop_addon/scripts/plugins/svencraft/` (entry point `svencraft.as`; `sc_*.as` modules: blocks, crafting,
  inventory, tools, props, materials, saves, HUD, the dev tour used for visual tests).
- `build/` — the scripts that produced its assets and patched the plugin while it was developed:
  - `makesandbox.py` + `buildmap.sh` — the WAD and `.map` of the old `svencraft_sandbox`, compiled with the
    Sven Co-op SDK compilers (the WAD is kept as `../../assets_src/testmap/svencraft_old.wad`);
  - `makecube.py`, `makedoor.py`, `makecar.py`, `*.qc`, `*.smd`, `car/` — the block, door and car models;
  - `toolmdl.py` — the first tool-model builder (the current one is `tools/toolmdl.py`);
  - `sprites/make_*.py` — the HUD sprites. Ten of their outputs are still used by the game as prebuilt files in
    `run/svencraft/sprites/svencraft/` (counts, cracks, cursor, sctools, sctools_s, craftpanel_l/_r,
    craftslots, slot, slot_sel); `sprites/make_tools.py` draws the weapon-menu tool icons from code;
  - `genmodelinfo.py`, `mdl*.py`, `entscan.py`, `bsplight.py`, `winding.py` — model/BSP inspection helpers;
  - `srvtest.py`, `srvtest_lib.py`, `reloadtest.py`, `vtest.ps1`, `worldtest.as` — the dedicated-server and
    windowed visual tests;
  - `patch*.py` — one-off edits applied to the plugin and scripts during development, in order;
  - `launcher/` — a small launcher and a script that pointed the non-Steam shortcut at it (abandoned: Steam's
    authentication requires the game to run as Sven Co-op's own app id).
- The procedural block-texture generator (`maketex.py`) and its 64x64 BMPs moved to `../../assets_src/blocktex/`,
  because the current game's block textures (`tools/make_blocktex.py`) are built from them.

## Notes
- All art made by these scripts is original (procedural pixel art drawn in code); compiled models, sprites,
  maps and previews were not copied. Tool models were derived from Sven Co-op's crowbar models and are not
  included.
- Paths in the copies were made generic: references to the old build folder became `.` (run the scripts from
  this `build/` folder), and the Steam user id in `launcher/set_shortcut.py` became `<steam-user-id>`.
  Paths into the Steam library (`C:\Program Files (x86)\Steam\steamapps\common\...`) are Steam's defaults.
- The scripts expect the Sven Co-op copy and the SDK at their default Steam locations and write into that copy's
  `svencoop_addon` folder; they are kept for reference, not maintained.
