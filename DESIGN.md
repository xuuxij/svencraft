# Svencraft — design

A new game: it plays and looks like Sven Co-op (Half-Life-style realistic maps, co-op, HL weapons and
monsters), and the Minecraft world is breaking into it.

## Premise
- Rifts between the worlds open in the Sven world: portals, and caves that open up underground.
- Through them the Minecraft world spills in: blocky terrain, Minecraft-style mobs (creeper, zombie,
  skeleton, spider, and others; original models and art, not Mojang assets).
- Sven Co-op's enemies (HL soldiers, aliens, ...) end up teaming up with the players to fight the
  Minecraft invaders.

## Campaign arc (slow burn)
1. **Normal Sven Co-op.** It starts out looking and playing exactly like Sven: a realistic map, fighting HL
   monsters, low-res and pixelated textures, the classic look.
2. **Hints that something is wrong.** Still regular Sven, but small wrong things keep showing up: a creeper in
   the distance blows up a group of Sven enemies, a single out-of-place block, a strange sound, a
   glimpse of a Minecraft mob that is gone when you look again.
3. **Escalation.** The hints get bigger and more frequent.
4. **Breach.** Whole rifts open up, or the players fall into a cave and have to fight their way out of the
   Minecraft world.
5. **Alliance.** Sven's monsters turn against the Minecraft invaders and end up fighting alongside the players.

Mission scripting has to support this: scripted "hint" events (spawn a mob at a distance with a target,
place a stray block, play a sound), triggers for rifts and cave collapses, and AI relationships that change
as the story escalates.

## The two worlds
- **Sven world (surface):** realistic Half-Life-style maps: real textures, terrain, roads, buildings, Sven's
  cars and props, baked lighting. Minecraft mechanics apply here too: dig the map's own geometry out a block
  at a time (40-unit cells on the block grid), walls break by thickness, props (cars, furniture) mine into materials,
  build with realistic-textured blocks, craft.
- **Minecraft world (underground and inside rifts):** 100% Minecraft style: 40-unit blocks (the 72-unit
  player fits 2-high tunnels and jumps 1 block), pixel-art blocks, caves, ores, Minecraft mobs.

## Engine (Xash3D FWGS fork, engine/)
- `engine/common/voxel.c`, `common/voxel_api.h`: the block world. Part of the world model: every engine trace
  (player movement, monsters, bullets, grenades) collides with solid blocks. Game DLLs use `Vox_GetAPI`.
- `ref/gl/gl_voxel.c`: draws the block world as 16^3 section meshes (sky darkness + ambient occlusion).
  Block looks come from `scripts/blocks.txt` + `gfx/blocks/*.tga` in the game directory (flags: alpha, joined,
  world, torch shapes, `lightN`, `front_px/nx/py/ny=<texture>` for a furnace's mouth). Block light (torches 14,
  burning furnaces 13) floods out like Minecraft's, stopped by blocks and by the town's walls; it also brightens
  the town's meshes and models nearby. Torches and burning furnaces give off Minecraft-style flame/smoke squares.
- Sound: Sven ships Ogg/MP3 data in `.wav` files (footsteps among them); the WAV loader decodes those too.
- `engine/common/dynworld.c`: the diggable Sven-world geometry (`maps/<map>.dyn`, exported by the map builder):
  Quake 3-style brush collision, carving by CSG in block-sized cells, meshes per 256-unit cell with faces cut
  along the block grid (a dug hole always shows the faces around it). Lit like Half-Life: a lightmap texel
  every 16 units (sun with shadows, sky light, the map's lamps), drawn doubled like GoldSrc's overbright
  lightmaps (`ref/gl/gl_dynworld.c`). Decals (bullet holes, blood, scorch marks) stick to it, under the
  lightmap, and are refitted when it is dug. Models standing in these worlds are lit from them too. The map's
  brush entities (cars, doors, crates) get their shadows from a hidden copy of it the light compiler sees.
  Dynamic lights (muzzle flashes, explosions, burning cars, the flashlight) light it and the block world per
  pixel (a falloff texture over each face, GoldSrc's dlight numbers); the flashlight there is a cone from the eye
  (a soft spot projected per pixel, fading out by 700 units: `gl_dynworld.c` R_SpotDraw). Digging updates light like Minecraft: the
  cells a dug hole can let light into are relit (a hole in a roof lets the sun in), and the block world's sky
  map follows (a shaft down to a cave lights its floor) via a log of dig boxes the renderer reads.
- Material sounds: traces report blocks as `sc_block<id>` and diggable faces by their texture, so bullet
  impacts and footsteps know grass from asphalt, planks or glass (`run/svencraft/sound/materials.txt` from
  `tools/make_materials.py`; Sven's wood, snow and flesh footsteps added).

## Game code (hlsdk-portable fork, game/)
- Done: world generation (underground, caves, ores, the rift), mining/placing, tools, digging the Sven world,
  props and cars into materials, drops.
- **Creeper** (`dlls/svencraft/sc_creeper.cpp`, model from `tools/make_creeper.py`): chases players and HL
  monsters, hops blocks, hisses/flashes/swells, Minecraft's blast curve, blocky crater in both worlds, drops
  gunpowder. Monster relationships: block-world creatures hate everything alive; `sc_alliance` 0/1/2 =
  HL monsters ignore them / fight them / also leave players alone (the story's alliance).
- **Inventory and crafting** (`sc_crafting.cpp`, client `cl_dll/svencraft/hud_inventory.cpp`): Minecraft's
  36-slot inventory and click rules (left/right/shift, number keys 1-9 swap the slot under the cursor with that
  hotbar slot, the drop key over a slot throws one (Shift/Ctrl: the stack), a double click gathers the same item
  onto the mouse up to a stack, dragging a held stack across slots spreads it evenly (left) or one each (right),
  throwing), 2x2 grid in the inventory (K), crafting table
  3x3 and furnace (E or right-click on them), recipe book that lays recipes out. Items and recipes:
  `tools/make_items.py` -> `common/sc_items.h`, `common/sc_recipes.h`. Vanilla recipes plus the Half-Life
  side: guns, ammo, medkits, batteries. Furnaces smelt on Minecraft's clock (iron/gold ore, scrap metal from
  cars -> ingots; sand, cobble, logs), face whoever placed them and show their fire (and light) while burning.
  Leaves drop themselves only to shears (two iron ingots), otherwise now and then a stick or, rarely, an apple;
  right-click eats an apple (no hunger here: it mends 20).
- **Gunfire** (client `cl_dll/svencraft/sc_effects.cpp`): Sven's scripted muzzle flashes (studio event 5005,
  `events/muzzle_*.txt`: the MP5's and shotgun's), every shot lights its surroundings for an instant (a world
  dlight, so the town and the caves flash too) and leaves a wisp of smoke; spent cases lie around 10 s.
  cvars `cl_muzzlelight`, `cl_gunsmoke`, `cl_shell_life`. Monsters open plain doors in their way (Sven's rule).
  A hit looks like what it hit (`ev_hldm.cpp`): sparks off metal, glints off glass, splinters off wood, a puff of
  dust coloured like the surface; ricochets whine only off hard surfaces. A round going into water from the air
  splashes where it enters: droplets thrown up, a breath of spray and a plip (`SC_BulletSplash`, the client's for
  players' shots, the server's `SCSplash` message for monsters').
- **Recoil and spread** (`dlls/svencraft/sc_gunfeel.h`, `sc_gunfeel 1`): the cone opens with speed, in the air
  and with sustained fire (bloom, dying away at 0.1/s), narrows crouched; each round kicks the aim up and a little
  sideways (`pev->punchangle`, which Half-Life's movement brings back down), so the MP5 climbs. Predicted: the
  bloom rides in `weapon_data_t.fuser4`, the punch in the predicted clientdata, and the client's bullets aim at
  the same view + punch as the server's (the server used to count the punch twice, `player.cpp` PostThink).
  0 gives Half-Life's fixed cones.
- **Opposing Force arsenal and bestiary** (hlsdk-portable's `opforfixed` branch, merged: `dlls/gearbox/`), as in
  Sven Co-op, with Sven's models: M249, Desert Eagle (laser), M40A1 sniper (scope), pipe wrench, shock roach, spore
  launcher, displacer, barnacle grapple (hotbar items 136-146, found, not crafted); allied grunts (medic, torch),
  male assassins, shock troopers, pit drones, gonomes, voltigores, Otis, zombie Barney/soldier, black-ops
  Apache/Osprey. Their guns use the recoil/spread and penetration rules below; their monsters' health and damage
  come from Sven's skill.cfg (`tools/make_skillcfg.py` maps Opposing Force's names). No knife or penguin (Sven has
  no models). The flashlight stays Half-Life's; `sc_nightvision 1` swaps in Opposing Force's night vision. Every
  summonable monster (`sc_summon`, later the dungeon master) loads with the map (`sc_inventory.cpp` bestiary).
  Also merged: `hlfixed` (crowbar, gauss, tripmine, grenade, charger and explosion fixes; Barney won't shoot
  through players; the room type is resent through `m_SndRoomtype`, which `sc_acoustics.cpp` now feeds).
  `tools/merge_branch.py` merges an upstream branch into the uncommitted fork; `docs/REFERENCES.md` lists what else
  is worth taking from other projects.
- **Heavy weapons grunt** (`monster_hwgrunt`, `sc_hwgrunt.cpp`, Sven Co-op's, on Half-Life's grunt AI): 200 health,
  no grenades or kicks; the barrels spin up before he fires (his aimed pause is the spin-up) and wind down after,
  5.56 rounds about fifteen a second; dying, he drops the minigun. `sc_summon monster_hwgrunt`.
- **Robot grunt** (`monster_robogrunt`, `sc_robogrunt.cpp`, Sven Co-op's): Half-Life's grunt in Sven's rgrunt.mdl, a
  machine: no blood (sparks, a burst off the head), double damage from energy, four times from shock, Sven's VOX
  voice (RB_ sentences: `CHGrunt::Voice` lets a grunt kind speak another group); dead, it smokes and blows up a few
  seconds later (100), sooner if hit again, leaving metal parts.
- **Minigun** (`sc_minigun.cpp`, Sven Co-op's, hotbar item 147): +attack winds the barrels up for half a second,
  then fires 5.56 every 0.06 s; +attack2 keeps them spinning without firing; letting go winds them down. It slows
  the carrier (210, spinning 150). Predicted like the other guns (barrel state and timer in the weapon data).
- **Prediction data** (`run/svencraft/delta.lst`): Sven's network field list without Half-Life's `vuser1-4` and
  `fuser1-4` client fields left the client blind to reserve ammo, so every weapon that fires from it (gauss, gluon,
  hornets, grenades, satchels, snarks, shock roach, minigun) showed nothing until the server's echo; our copy adds
  them back.
- **Summoning** (`SC_PlaceCreature`, `sc_world.cpp`): a summoned creature gets free floor that fits its size
  (human or large hull, clear of other creatures, the engine's own "stuck in wall" test passed), searching rings
  around the spot; the dungeon master's spawns will go through the same place. `sc_state` also reports the weapon
  in hand (clip, ammo), the inventory and each monster's entity index.
- **New weapon models** (`tools/fpmodel/`): a converter from rigged, animated sources (glTF/FBX/.blend) to
  GoldSrc view models through a portable Blender; per-weapon JSON configs say which frames make which Half-Life
  sequence. First picks: DJMaesen's sniper rifle, minigun and crossbow (CC BY 4.0, credited in docs/CREDITS.md).
- **Who's that** (`sc_status.cpp`, client `hud_target.cpp`): like Sven, the monster or player under the crosshair
  shows its name and health below it, "Friend:" in green (it won't attack you), "Enemy:" in red, others yellow
  (`cl_targetinfo`).
- **Hit what you see** (`sc_lagcomp.cpp`, adapted from SevenKewp): the engine rewinds only players for a shot; here
  the monsters are put back where the shooter saw them too (ping + interpolation, at most `sc_unlag_max` 0.5 s)
  while a player's rounds are traced (`sc_unlag_monsters`).
- **Penetration** (`dlls/svencraft/sc_ballistics.h`, `sc_penetration 1`): a round goes on through glass, wood,
  sheet metal, snow (9mm 16 "wood units", .357/Deagle 40, 5.56 30, 7.62 60, a pellet 6; brick costs 8 a unit,
  metal 5, glass 0.5) and
  hurts what is behind, weaker; the client draws the exit hole. `sc_bulletlog 1` / `cl_bulletlog 1` print each
  surface (and the aim) on both sides, to check that prediction matches.
- **Feel** (client `view.cpp`, `sc_effects.cpp`): the gun trails turns and drifts against the motion, the bob
  follows the footsteps, a hard landing dips the view (`cl_viewlag`, `cl_bobstyle`, `cl_bobamt_vert/lat`,
  `cl_landbob`; 0 gives Half-Life's); the gun pulls back off a wall in front; strafing rolls the view a little
  (`sv_rollangle 1.5`). Getting hurt flinches the view by the damage, rolled away from the attacker. Explosions
  near you (`SCBlast` message from `SC_Explosion` and burning cars) shake the view by distance and cover, kick the
  head and flash when in sight (`cl_blastfx`); a close one dulls the hearing and leaves the ears ringing for a few
  seconds (the engine's mixer, `sound/s_mix.c`: `s_muffle`, `s_earring`, `s_earring_vol`).
- **Fog and haze** (`env_fog`, Sven's keys: rendercolor, iuser2 start, iuser3 end, flag 1 starts off, flag 2 fogs
  the sky; `sc_fog r g b start end [sky]` from the console): the client applies it every frame through the triangle
  API; the town's and the block world's multi-pass drawing fogs each pass right (grey for the doubled lightmap, black
  for added light) so nothing darkens or glows in it. The town has a light distance haze (1200 to 9000 units).
- **Steam, smoke and fire** (`sc_emitter`: kind 0 steam, 1 smoke, 2 fire; rate, scale, rendercolor, renderamt):
  the client puffs rise, grow, slow and drift while the emitter is in view. Steam rises from the town's manholes
  and faintly from the roof units. `sc_summon <class> [dist] [model] [seq] [key=value ...]` sets any keys.
- **Wall chargers** (`func_healthcharger`, `func_recharge`) take Sven's keys (`CustomJuice`, `CustomRechargeTime`,
  `TriggerOnEmpty`, `TriggerOnRecharged`, `CustomDeniedSound` / `StartSound` / `LoopSound`; `sc_world.cpp`). Empty
  ones come back like in Sven co-op: after 60 s (health) and 30 s (suit) unless the map says otherwise; single-player
  Half-Life never refilled them.
- **Medkit** (`weapon_medkit`, Sven's, adapted from SevenKewp; `sc_medkit.cpp`): every player starts with one
  (hotbar item 148, half charged). Primary heals the player or friendly creature in front 10 a use; the charge
  comes back a point every 0.6 s up to 100. Held secondary for two seconds by a fallen player brings them back
  with 50 health for 50 charge (a fallen ally, such as Barney or a scientist, comes back whole: its own spawn again
  where it lies); a red cross marks fallen players for whoever holds a medkit, and a revive in progress holds off
  their respawn. The gibbed can't be revived.
- **Co-op rules**: a hosted game is co-op (Half-Life's rules, `coop 1`): Svencraft's own `listenserver.cfg` /
  `server.cfg` replace Sven's, which set deathmatch (and the engine turns deathmatch on for more than one player
  unless coop is set); `motd.txt` is ours. The dead wait at their bodies (no death camera, no forced respawn)
  until they choose to respawn or a medic revives them; the body keeps the way it fell (the engine turns a
  player to their view after every move). Players don't hurt each other, by bullet or blast (their own blasts
  still hurt them); `mp_friendlyfire 1` allows it. Antiblock (Sven's): use held for 0.3 s on a teammate in the
  way changes places with them (`SC_AntiBlock`).
- **Water** is Half-Life's: a `func_water` (Sven's see-through kind, `WaveHeight`) gives swimming, the murky
  underwater fog from the water texture (the engine reads its colour and density from palette entries 3 and 4;
  `make_town.py` `water_texture()` makes the pond's own, murkier copy of Half-Life's dam water), the O2 counter and drowning (health given back after surfacing), a jump
  out at the bank, and no fall damage landing in it; a fall into it throws up a splash as big as the fall was
  fast (`SC_WaterEntry`). Its box reaches down to the block world's top, so whatever is
  dug out under a pond floods. Half-Life's walkers don't swim: the AI nodes leave the water out, and monsters
  can't see across the surface.
- **Temporary entities**: a fading animated sprite (smoke, dust, steam) used to restart its death time every frame
  in the Half-Life client code and live for ever, filling the 500 temporary entities until nothing new could
  appear (shells, sparks); fixed in `entity.cpp`. `cl_tentlog 1` prints what they are. One-frame puff sprites
  showed nothing: impact dust is Sven's Puff1, gun smoke wep_smoke_02.
- **Death and respawn** (`client.cpp` respawn, `sc_respawn 1`): Sven's way even alone: press fire and you are
  back at a spawn point, the world as it was (Half-Life's single player reloaded the level, which would lose all
  digging and building; the engine refused that command anyway, leaving the player dead for good). What you
  carried lies where you fell, Minecraft style; the wooden starting tools come once per connection, not again.
- **Friendly fire on allies** (`mp_npckill`, Sven's): 1 (default) anything can hurt the players' allies and they
  turn on a player who shoots them, 0 nothing can, 2 only the enemy can (checked where damage enters every ally
  class, so a stray grenade doesn't provoke Barney either). The .357's zoom works in every game, as in Sven.
- **Falls** (`sc_player.cpp`): `sc_falldamage 1` Minecraft's x5 (5 per block beyond three), 2 a realistic curve,
  0 Half-Life's; snow halves it.
- **Speech**: the engine and the game read the sentences (the H.E.V. suit's voice, Barney, the scientists, the
  grunts' radio, Opposing Force's allies) from `sound/sentences.txt`, which Sven Co-op keeps as
  `default_sentences.txt`; `tools/setup_run.py` copies it into place, and the game takes its ~670 groups
  (Half-Life allowed 200).
- **Room acoustics** (`sc_acoustics.cpp`): the DSP room type (echo) is worked out around each player twice a
  second: open sky none, under a roof a concrete room of the size the walls say, below the block world's top a
  cavern. A head inside a solid block suffocates (Minecraft's 1 per half second, x5).
- **Explosions** (`sc_blast.cpp`): Minecraft's rays with blast resistance, through blocks and the town alike
  (a thin wall only costs a ray the steps inside it). Creeper power 3; every Half-Life explosion too (damage /
  60: grenade 1.7, rocket 2.5, satchel 2.7). Half-Life breakables drop their material (a crate: two planks).
  Bullets break what bullets would: glass blocks at once, leaves in a couple of hits, wood (blocks, the town's
  desks and counters) after a magazine or so; stone, brick, earth and metal only chip. Fuel drums (`sc_barrel`)
  go up when shot, one after another.
- **Cars** (`sc_car.cpp`): take damage like in Sven maps: smoke when hurt, catch fire, blow up a few seconds
  later (fireball, metal, a blast that can set off the next car), then turn into their burnt copy (the map
  builds one per car). Wreck or not, they can be taken apart for scrap metal.
- **Hotbar** (`sc_hotbar.cpp`): like Minecraft, the selected slot decides what is held. Tools (wood, stone,
  iron, diamond) and every HL gun are inventory items; a gun item in the inventory means the player has that HL
  weapon (crafted guns come loaded, world pickups need a free slot, a weapon whose item leaves is unloaded into
  the ammo pool). Anything else is held in the bare hand (`v_schand.mdl`: Sven's glove holding the block or
  item). Number keys / mouse wheel pick slots, Q the previous one, G (drop) throws one. Tools wear out on
  Minecraft's counts (59/131/250/1561 uses, 1 per block, 2 per hit; wear bar under the icon). Falling hits are
  critical (x1.5). Supplies (ammo, health kit, battery) are handed over when crafted.
- Balance: Minecraft numbers x5 (20 health there = 100 here) for health and damage.
- Planned: more Minecraft mobs, saving, Sven-style co-op rules.

## Phases: Half-Life first, Minecraft later
A player dropping in at the start sees and plays Half-Life / Sven Co-op only: the HL/Sven HUD and weapon
selection menu, HL's rules (fall damage and the rest). The Minecraft side arrives with the story: the hotbar UI
(alongside the HL HUD or instead of it, to be decided), Minecraft's rules, the Minecraft-style tools and weapons
players find. So every Minecraft mechanic stays switchable (per-feature cvars such as `sc_falldamage` now, one
phase setting that flips them together later) and nothing Minecraft gets removed.

## Co-op sections and stats (planned)
Played like Sven Co-op: a team works through the sections of a map or scenario. If the whole team dies or fails
a section, the section resets and everyone starts it again, keeping what they had when they entered it (only that
section's progress and pickups are lost).
Stats matter: kills (how many, which enemies), per-weapon stats, accuracy and the rest, kept per section, per
mission and over a player's whole career. Each player's stats show at the end of every section and of the
mission; the lifetime ones on the player page of the menu.
- Build toward it: inventories that can be snapshotted per player at a section's start; every game event that
  counts (shots fired and hit, damage, kills with weapon and victim, blocks broken, deaths) goes through one place
  that both the stats and the dungeon master's event stream read.

## AI dungeon master (planned)
An LLM watches each live game as its dungeon master: player chat, where everyone is on the map, what is
happening. It acts through the game's own tools (spawning monsters, placing or carving blocks, triggering
events, the hints and rifts of the campaign) and pays for every action from a budget of points it has to manage,
playing to beat the players, not to crush them.
- The dev/diagnostic commands (`sc_summon`, `sc_setblock`, `sc_carve`, `sc_tp`, `sc_give`, `sc_clearmonsters`,
  `sc_cave`, `sc_hurt`, `sc_bot`, ...) are kept as real features: they become the DM's actions (server-side, with a point cost).
- `sc_state` (server command, `sc_state.cpp`): one line of JSON with the map, time, every player (position,
  view, health, armour, held item) and every monster (class, position, health, its enemy). Tests read it too.
- Still to build: an event stream (chat, kills, blocks broken, explosions), a command channel in for the DM
  process, and the point economy.

## Sandbox first
Every concept is built and tested in the sandbox map before any storyline or mission gameplay is made from it.
`maps_src/make_town.py` builds `svencraft_sandbox`: a realistic HL-style town (road, sidewalks, brush-built cars,
street lamps, Sven props, hills) on top of the block world (worldspawn `sc_blocktop` = -4), with one rift
(`info_sc_rift`) where the Minecraft ground breaks through, Half-Life's Xen flora growing around it (trees that
lash out, light plants, hair, spores). Its buildings are Half-Life style and all diggable:
an office (two floors, stairs, desks), a shop (counter, shelves, gable roof), a warehouse (racks, breakable
crates, some of them pushable, cars, Half-Life's health and H.E.V. wall chargers by the door) and a house; framed windows with shootable glass, doors that open, door lamps, ceiling tiles,
carpet/lino/wood floors, parapets and air conditioners on the flat roofs.
Town v3 (`maps_src/town_v3.py`, assets and layout in `docs/town_v3_assets.md`): Elm St running north to a
clapboard church with a steeple and a graveyard; Big Tony's Pizza (patio, booths, counter, stools); the police
station (two floors, cells, a lot with police cars); a three-storey apartment block with a fire escape (a ladder
from its top landing to the roof) and a yard;
a park; a pond in the field behind the back alley (a hollow in the diggable ground with muddy banks, reeds,
ferns, a plank jetty over the deep end, frogs, two of Half-Life's leeches); the Rockwell gas station (canopy, pumps, kiosk, service bays with roll-up doors); power lines, hydrants,
dumpsters, a phone booth, a bus stop, signs (generated textures), zebra crossings; and the town's sounds:
ambient_generic loops (birds, wind, hums, radios) and `sc_ambient_random` one-shots (birds, a dog, the church
bell, a phone).

## Final phase: graphics upgrades
For the Half-Life/Sven-style parts of the game, after the gameplay is in place: graphical enhancements such as
normal/bump mapping, better dynamic lighting and shadows, possibly ray-traced lighting, and similar modern
renderer features (the engine and renderer are ours, so these can go straight into ref/gl).

## Content
- `run/svencoop` is a junction to Sven Co-op's content (models, sounds, textures, maps) for the Sven look.
- `run/svencraft` is the game directory (gameinfo.txt, game DLLs, our maps and assets).

## Tools
- `tools/build.sh [game] [engine] [map]`, `tools/runtest.sh <name> "<cmds>"` (in-engine screenshots tiled into
  `tools/shots/`, block world seed `SEED`, default 1717); dev commands `sc_tp`, `sc_give`, `sc_summon`,
  `sc_clearmonsters`, `sc_setblock`, `sc_hotbar item <id>`, `sc_ui_point`/`sc_ui_click`; cvar `sc_seed` (0 = a
  new world each map load).
- Asset generators: `make_blocktex.py` (block textures, blocks.txt) -> `make_blockassets.py` -> `make_items.py`
  (items, icons, flat drop model) -> `make_hand.py` (the hand view model); `make_tools.py` (tool models from
  Sven's crowbar via `toolmdl.py`), `make_creeper.py`, `make_font.py`, `make_skillcfg.py`.
