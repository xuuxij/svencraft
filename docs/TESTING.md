# Testing Svencraft

Everything is tried in the sandbox map (`svencraft_sandbox`) before it is used anywhere else. The dev and
diagnostic commands below are real features, not test-only hacks: they are what the planned AI dungeon master
will act through (DESIGN.md), so keep them working and server-side.

## In-engine visual tests: tools/runtest.sh

```sh
sh tools/runtest.sh <name> "<cmd;cmd;...>" [timeout seconds, default 600]
# e.g.
sh tools/runtest.sh creeper1 "fps_max 100;sc_cave 2;w100;sc_summon monster_creeper 300;w100;screenshot;w100;screenshot"
```

- Closes a running `xash3d.exe` first (do not run it while someone is playing).
- Puts the player's `run/svencraft/config.cfg` back afterwards: the engine saves archived settings a test
  changed (`s_show`, `fps_max`, ...) when it quits.
- Writes `run/svencraft/tests/<name>.cfg`: wait aliases `w5 w10 w20 w50 w100 w1k`, `sv_cheats 1`, `god`,
  `notarget` (set `NT=" "` to leave monsters aware of you), your commands (a `w10` is added after every
  `screenshot`), then `quit`. With `fps_max 100`, `w100` is about one second.
- Runs `run/xash3d.exe -game svencraft -windowed -width 1280 -height 720 -dev 2 -log $ARGS +sc_seed $SEED +map $MAP`
  (`SEED` default 1717, `MAP` default `svencraft_sandbox`; `ARGS` more launch arguments: a co-op test with a
  second player is `ARGS="+maxplayers 2 +coop 1"` and `sc_bot`).
- Tiles the screenshots (2 per row, half size) into `tools/shots/<name>.png` and prints errors and warnings from
  `run/engine.log`.

`tools/vxash.ps1 <outdir> [seconds] [engine args]` records the game window twice a second instead (PowerShell;
`PrintWindow`, because screen grabs of the GL window come out black). It refuses to start when the game runs.

## Making tests deterministic

- `+sc_seed 1717` gives a fixed block world (caves, ores); with that seed `sc_cave 2` lands at block -1 0 -19.
  `sc_seed 0` (the default) makes a new world every map load.
- Place or clear exactly what a test needs with `sc_setblock` / `sc_carve`, remove monsters with
  `sc_clearmonsters`, pick the held item with `sc_hotbar item <id>`.
- Guns from `sc_give` come empty: give their ammo too and `+reload`.
- Read state from the log instead of guessing from screenshots: `sc_state` prints one line of JSON.
- `developer 3` shows the AI's `at_aiconsole` messages in `run/engine.log`.

## Commands

Cheat commands need `sv_cheats 1` (runtest and `Play Svencraft.bat` set it). Block coordinates are in blocks
(40 units).

| Command | Side | What |
|---|---|---|
| `sc_tp <x> <y> <z> [pitch yaw]` | server, cheat | teleport (world units) |
| `sc_cave [n]` | server, cheat | go to the n-th open cave spot (nearest the middle first) |
| `sc_give <id> [count]` | server, cheat | add an item or block to the inventory (default 64; ids: `game/common/sc_items.h`) |
| `sc_summon <classname> [distance]` | server, cheat | spawn a monster or entity on the ground ahead (default 160 units) |
| `sc_clearmonsters` | server, cheat | remove every monster |
| `sc_hurt <amount> [self]` | server, cheat | hurt what you look at (or yourself) and print its health |
| `sc_bot [name]` | server, cheat | a stand-in player ahead that just stands there (needs a free slot: `+maxplayers 2`) |
| `sc_setblock <x> <y> <z> <id>` | server, cheat | place (id) or break (0) a block, as a player would |
| `sc_carve <x> <y> <z> [x1 y1 z1]` | server, cheat | dig block cells out of the town geometry and the block world |
| `sc_hotbar <1..9> \| next \| prev \| last \| item <id>` | server | select a hotbar slot (number keys, wheel, Q) or the slot holding an item |
| `drop [all]` | server | throw one held item (G), or the stack |
| `sc_state` | server console / rcon | one line of JSON: map, time, players (position, view, health, armour, held item), monsters (class, position, health, enemy) |
| `sc_inventory` | client | open the inventory (K) |
| `sc_ui_point <slot>` or `<x> <y>`, `sc_ui_click [button] [shift]` | client | move the inventory cursor and click, for UI tests |
| `sc_hotkey <slot> <0-8>` | server | what a number key does over a slot with a screen open (the inventory sends it) |
| `sc_throwslot <slot> [all]`, `sc_gather <slot>` | server | the drop key over a slot; a double click there |
| `sc_spread <0/1> <slot> ...` | server | a drag across slots with a held stack (left: evenly, right: one each) |
| `sc_ui_drag <button> <slot> <slot> ...` | client | press over the first slot, drag across the rest, release (UI tests) |

`sc_screen`, `sc_click`, `sc_recipe` are sent by the client's inventory screen to the server; they are not meant
to be typed.

## Cvars

| Cvar | Default | What |
|---|---|---|
| `sc_seed` | 0 | block world seed (0 = new each map load) |
| `sc_alliance` | 0 | HL monsters vs the block world's creatures: 0 ignore them, 1 fight them, 2 fight alongside the players |
| `sc_falldamage` | 1 | 0 Half-Life, 1 Minecraft x5, 2 realistic curve |
| `sc_gunfeel` | 1 | recoil and spread bloom (0 = Half-Life's fixed cones) |
| `sc_penetration` | 1 | bullets go through thin materials |
| `sc_bulletlog` / `cl_bulletlog` | 0 | print every surface a round hits on the server / client (prediction check) |
| `sc_unlag_monsters`, `sc_unlag_max` | 1, 0.5 | rewind monsters to where the shooter saw them (seconds at most) |
| `sc_nightvision` | 0 | Opposing Force night vision instead of the flashlight |
| `cl_muzzlelight`, `cl_gunsmoke`, `cl_shell_life` | 1, 1, 10 | shot light, gun smoke, seconds spent cases stay |
| `cl_viewlag`, `cl_bobstyle`, `cl_bobamt_vert`, `cl_bobamt_lat`, `cl_landbob` | 1, 1, 1.2, 0.8, 1 | gun sway, step bob, landing dip (0 = Half-Life's) |
| `cl_blastfx` | 1 | view shake, head kick and flash from nearby explosions |
| `s_muffle`, `s_earring`, `s_earring_vol` | 0, 0, 0.35 | engine: dulled hearing and ear ringing after a blast (set by the client) |
| `sv_rollangle` | 1.5 (userconfig.d) | strafe roll |
| `r_voxels`, `r_dynworld` | 1, 1 | engine: draw the block world / the diggable map geometry (0 hides it, for checks) |

## Checking generators

The generators are deterministic: a fresh checkout reproduces the live `run/svencraft` files byte for byte. To
check a change without touching the live game folder, export a copy and run them there (see
`docs/PUBLISHING.md`, "Verifying a fresh checkout").
