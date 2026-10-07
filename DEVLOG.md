# Svencraft devlog

Newest first. Dates are UTC. Design decisions and the full feature notes live in [DESIGN.md](DESIGN.md); this is
the record of what landed when. Entries before this file existed were reconstructed from the design notes, file
dates and the build history.

## 2026-10-07

### Atmosphere
- Fog: `env_fog` (Sven's keys) and `sc_fog`; the town and block renderers fog their light passes correctly; the
  sandbox town gets a light distance haze.
- `sc_emitter` steam/smoke/fire sources; steam from the town's manholes and roof units.
- A destroyed car pours a column of black smoke for 30 seconds, like Sven's burning wrecks.
- Fixed: fading animated sprites never died (Half-Life client tempent code), so smoke, dust and steam filled the
  500 temporary entities over time; impact dust and gun smoke switched to sprites that actually show.
  `cl_tentlog` diagnostic; `sc_summon` takes key=value pairs.

### Mechanics QA
- Fixed: a dead player could never come back (single player's "reload" is refused by the engine). Now Sven's
  respawn at a spawn point (`sc_respawn`); the inventory lies where the player died and the starting tools are
  given once per connection (they were duplicated on every death).
- Half-Life's health and H.E.V. wall chargers in the warehouse, with Sven's charger keys; empty chargers refill
  like Sven's (60 s / 30 s). A steel ladder from the apartment's fire escape to its roof.
- Water: a pond behind the back alley (Half-Life's `func_water`: swimming, underwater fog, drowning and recovery,
  no fall damage diving in), with a jetty, reeds, frogs, leeches and murky water; bullets splash where they enter water, and so does a
  player falling in.
- Fixed: every spoken sentence was silent (the H.E.V. suit, Barney, the scientists, the grunts' radio):
  Sven Co-op names the file `default_sentences.txt`; `setup_run.py` now puts it where the engine and game look,
  and the game takes Sven's ~670 sentence groups.
- Sven's heavy weapons grunt (`monster_hwgrunt`): spins up his minigun, fires in long bursts, drops it when he dies.
- Sven's robot grunt (`monster_robogrunt`): sparks instead of blood, VOX voice, smokes and explodes after death.
- Sven's medkit: heal teammates and allies, revive fallen players and allies (hold secondary 2 s), self-recharging; every
  player starts with one. Dead players wait at their bodies for a medic.
- Fixed: hosting a game from the menu ran Sven Co-op's `listenserver.cfg`, which made it deathmatch, and showed
  Sven's welcome message; Svencraft has its own (co-op) server configs and message now. In co-op players no
  longer hurt each other (Sven's rule; `mp_friendlyfire 1` to allow). Sven's antiblock: hold use on a teammate in
  the way to change places.
- Inventory, Minecraft's shortcuts: number keys 1-9 over a slot swap it with that hotbar slot, the drop key (G)
  over a slot throws one (Shift/Ctrl: the stack), a double click gathers the same item onto the mouse, dragging a
  held stack spreads it (left: evenly, right: one each).
- `sc_hurt` and `sc_bot` (a stand-in player) dev commands; `runtest.sh` takes `ARGS` for co-op tests.
- Half-Life's Xen trees stood on the hills as if they were Earth trees; they and the rest of the Xen flora (light
  plants, hair, spores) now grow around the rift only.
- `mp_npckill` (Sven's friendly-fire rule for allies, 0/1/2). The .357 zooms in every game. Every Half-Life gun
  re-tested (reloads, clips, alternate fire, scopes, prediction).

### Weapon QA pass
- Tested every Opposing Force gun (fire, reload, secondary, scope, laser, recharge), pickups into the hotbar,
  weapons dropped by killed grunts, an ally-vs-alien battle and the medic's healing.
- Fixed: Sven's `delta.lst` lacks Half-Life's `vuser`/`fuser` client fields, so the client never knew its reserve
  ammo and every clipless weapon went unpredicted; `run/svencraft/delta.lst` adds them back.
- Fixed: `sc_summon` placed creatures overlapping walls and each other ("stuck in wall"); now it finds free floor
  sized for the creature. The displacer's teleport goes to a spawn point in maps without Xen targets.
- Opposing Force gun events got the muzzle light, smoke and halved camera kick the Half-Life ones have.
- New: Sven Co-op's minigun (spin-up, sustained fire, spin-only, slows the carrier), hotbar item 147.
- Chose three new weapon models (DJMaesen's sniper rifle, minigun and crossbow, CC BY 4.0) from a 20-model
  shortlist; portable Blender 4.2 LTS set up in `deps/` for converting them.
- `tools/fpmodel/build_fp.py` + `convert_fp.py`: rigged, animated glTF/FBX/.blend -> decimated mesh, one bone per
  vertex, 256-colour textures, named sequences with events, compiled by the Sven SDK's studiomdl. Proven end to end
  on a CC BY sample (Khronos Cesium Man) shown in game with `sc_summon cycler <dist> <model> [sequence]` (new: a
  model for the summoned entity).

### Repository and publishing kit
- Prepared the project for a private GitHub repo: README, this devlog, `docs/BUILDING.md`, `TESTING.md`,
  `UPSTREAM.md`, `LICENSES.md`, `PUBLISHING.md`; `.gitignore` / `.gitattributes`; `tools/publish/` (manifest,
  `export.sh`, `audit.py`, `publish.sh`). Nothing is published until the owner says so.
- Tools no longer carry machine-specific paths: `tools/sc_paths.py` finds the project from the checkout, with
  `SVENCRAFT_*` environment overrides for the game folder, Sven Co-op, the SDK and the work folders.
  `build.sh`, `runtest.sh` and `vxash.ps1` find the project from their own location.
- Rescued the prototype's procedural block-texture art (and its generator `maketex.py`) and the old test map's
  WAD from a temporary build folder into `assets_src/`; `make_blocktex.py` / `make_testmap.py` read them there.
  Snapshot of the AngelScript prototype in `legacy/angelscript_mod/`.
- `tools/setup_run.py` prepares a fresh checkout's `run/` folder. Verified from a fresh export: every asset
  generator reproduces the live game files byte for byte.

### Upstream merges
- Merged hlsdk-portable's `hlfixed` (Half-Life fixes) and `opforfixed` (Opposing Force weapons and monsters:
  `dlls/gearbox/`, `cl_dll/gearbox/`) into the game fork, file by file, with `tools/merge_branch.py`; conflicts
  in `cbase.h`, `player.cpp`, `pm_shared.c`, `barney.cpp` resolved by hand. Opposing Force night vision behind
  `sc_nightvision`.
- Monster lag compensation for hitscan (`sc_unlag_monsters`, `sc_unlag_max`), adapted from SevenKewp.
- Opposing Force guns as hotbar items 136-146 with new pixel icons; their recoil/spread profiles and penetration
  power (5.56, 7.62, Deagle); Sven's skill values mapped to Opposing Force's names; knife and penguin left out
  (no Sven models); every summonable monster precached with the map; tool weapons moved to ids 26-29.
- Fixed: the monster relationship table was read only for the first 14 classes, so Opposing Force's aliens and
  allied grunts had no friends or enemies.
- Sven-style target info: name and health under the crosshair, green friend / red enemy (`cl_targetinfo`).
- The flashlight is a cone of light from the eye in the block and diggable worlds (soft spot, fades to 700).
- Map: the hidden shadow-caster copy no longer generates collision hulls (`zhlt_noclip`): clipnodes 14657 ->
  6937, planes 4716 -> 2662.
- Surveyed open-source GoldSrc/Xash3D projects for reusable code: `docs/REFERENCES.md`.

### Half-Life / Sven realism
- Recoil and spread (`sc_gunfeel`): bloom with movement, air and sustained fire, crouch bonus, view kick that
  climbs; predicted on the client, and the server no longer counts the punch twice.
- Bullet penetration (`sc_penetration`): rounds go through glass, wood, sheet metal and leaves with per-calibre
  power and hurt what is behind; exit holes; `sc_bulletlog` / `cl_bulletlog` to compare server and client.
- Explosions near the player shake and kick the view and flash (`cl_blastfx`); a close one muffles hearing and
  leaves the ears ringing (engine mixer: `s_muffle`, `s_earring`).
- Fall damage modes (`sc_falldamage`: Half-Life, Minecraft x5, realistic); snow halves it.
- Room acoustics: the DSP room type follows the space around each player (sky, roof, cavern); suffocation in
  solid blocks.
- View feel: gun sway against turns, bob in step with footsteps, landing dip, strafe roll (`sv_rollangle 1.5`),
  damage flinch.
- Gunfire: Sven's muzzle flashes, a light flash per shot, gun smoke, shells that stay 10 s; hit effects by
  material (sparks, glass glints, splinters, coloured dust); ricochet sounds only off hard surfaces.
- Fuel drums that explode in chains; bullets break glass and wood (blocks and the town's furniture).
- Cars take damage like in Sven maps: smoke, fire, explosion, then a burnt copy; scrap metal from wrecks.
- Minecraft-ray explosions with blast resistance for the creeper and every Half-Life explosive, through blocks and
  the town alike; Half-Life breakables drop their material (a crate: two planks).

### Design direction
- Phases: a player dropping in sees pure Half-Life / Sven Co-op (HUD, weapon menu, rules); the Minecraft UI,
  rules and tools arrive later with the story, so every Minecraft mechanic stays switchable.
- Planned: Sven-style co-op sections (team wipe resets the section, keeping the entry loadout) and detailed
  stats per section, mission and career; an AI dungeon master that spends a point budget through the game's own
  dev commands. `sc_state` prints the game state as one line of JSON for tests and the future DM.

### The sandbox town
- Town v3 (`maps_src/town_v3.py`, `docs/town_v3_assets.md`): Elm St with a clapboard church and graveyard, a
  pizza place, the police station with cells and police cars, an apartment block with a fire escape, a park, a gas
  station with service bays, power lines, hydrants, dumpsters, a phone booth, a bus stop, generated signs, and
  ambient sound (loops and random one-shots).
- Half-Life-style interiors: an office, a shop, a warehouse with pushable crates, a house; framed windows with
  shootable glass, doors, door lamps, ceiling tiles, floors, roof details.
- The town is lit like GoldSrc: lightmaps with sun shadows, sky light and the map's lamps, drawn overbright;
  digging relights what a hole lets light into. Decals stick to the diggable town. Per-pixel dynamic lights
  (muzzle flashes, explosions, burning cars, the flashlight) on the town and the blocks.
- Material sounds: footsteps and impacts know grass, asphalt, planks, glass ... (`tools/make_materials.py`).

### Minecraft side
- Accuracy pass: torches and Minecraft block light (also on the town's meshes and models), torch and furnace
  flame particles, falling sand and gravel, Minecraft mining times, tool wear (59/131/250/1561), diamond tools,
  furnaces facing their placer and lit while burning, critical hits when falling, shears and apples.
- The hotbar decides what is held, like Minecraft: tools and every HL gun are inventory items; anything else is
  held in Sven's glove (`v_schand.mdl`, `tools/make_hand.py`); tool models built from Sven's crowbar
  (`tools/make_tools.py`).
- Inventory and crafting: Minecraft's 36 slots and click rules, the 2x2 grid (K), crafting table and furnace (E),
  a recipe book, Half-Life recipes (guns, ammo, medkits, batteries); items generated by `tools/make_items.py`.
- The creeper: chases players and HL monsters, hisses and swells, Minecraft's blast, blocky crater in both
  worlds; monster relationships via `sc_alliance`.
- AI node graph for the sandbox; Sven's skill values (`tools/make_skillcfg.py`; without them monsters have no
  health).

### A standalone game
- Left the Sven Co-op plugin behind for our own engine and game code: Xash3D FWGS and hlsdk-portable, cloned and
  built with MSVC, running Sven Co-op's content through `run/svencoop`.
- Engine: the block world as part of the world model (every trace collides with blocks), drawn as 16^3 section
  meshes with sky darkness and ambient occlusion; the map's own geometry made diggable in block cells
  (`dynworld.c`); Sven's Ogg/MP3-in-WAV sounds decoded.
- Game: world generation (caves, ores, the rift), mining and placing, tools, digging the town, props and cars into
  materials, drops.
- Map generator `maps_src/make_town.py` (+ `maplib.py`) builds the sandbox town with the SDK compilers; block
  textures, block item model and hotbar sprite from `tools/make_blocktex.py` / `make_blockassets.py`.

## 2026-10-06

### The AngelScript prototype (Sven Co-op plugin)
- Set up a copy of Sven Co-op to mod, and the Sven Co-op SDK.
- A server plugin brought Minecraft mechanics into unmodified Sven Co-op: mining and placing blocks (block
  models), map walls broken down into materials, props and cars broken into parts, axe/pickaxe/shovel in tiers
  (models derived from the crowbar), a HUD hotbar with counts, a crafting window built from HUD sprites with an
  emulated cursor, a generated sandbox map, caves, world and inventory saves, and bot-driven self-tests plus a
  recorded visual test tour.
- Limits of the closed engine (about 1000 block entities, no runtime change to the world model) led to the
  standalone game. Snapshot: `legacy/angelscript_mod/`.
