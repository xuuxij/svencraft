# Town v3: assets and layout (svencraft_sandbox)

Design and asset discovery for the next version of `maps_src/make_town.py`. Every texture, model and sound named
here was checked: textures against `wadtex.index()` (name, WAD, pixel size), models on disk under `run/svencoop/models`
(sequence 0 bbox read from the file), sounds on disk under `run/svencoop/sound` (real format plus loop flag, decided the
same way `engine/client/soundlib/snd_wav.c` decides it). Contact sheets of the top picks, viewed and judged for an HL
look, are in this folder:

- `town_v3_exterior.png`: walls, roofs, windows, doors, signs
- `town_v3_interior.png`: floors, walls, ceilings, shop fittings, lockers, posters
- `town_v3_street.png`: road, markings, curbs, drains, poles, fences, masked textures, vehicle textures
- `town_v3_terrain.png`: ground textures tiled 2x2 so the seams show, with their seam ratio

Sheet labels: blue means the WAD is in the map's BSP list, so the texture works on brush entities too. Orange with `*`
means the texture works only on diggable `.dyn` brushes.

Reading conventions:
- Model sizes are the sequence 0 box, x by y by z.
- A short model name such as `gins_x.mdl` means `models/ginsmodels/`; `arc_*`, `fern*` and `shrub1` mean
  `models/hunger/vegitation/`.
- A few names also exist in other folders (`models/sp_portal/ginsengavenger/`, `models/sc_psyko/`); use the ones
  given here.

## 0. Rules found while checking (read before building)

1. **Two WAD lists.**
   - Diggable `.dyn` brushes can use any texture in `tools/wadtex.py` `WADS`: halflife, Opfor, tfc, tfc2, bridge3,
     cs_bdog, snd, nw, op4ctf, deathmission, barney, decay, sandstone, neilm4, neilm5, scrpg2 and liquids.
   - Brush entities (doors, glass, cars, func_wall, func_illusionary) can only use the BSP list in `make_town.py`:
     halflife, Opfor, tfc, tfc2, bridge3, op4ctf, nw, barney and decay.
   - Fix: append `cs_bdog.wad`, `neilm4.wad`, `neilm5.wad` and `deathmission.wad` to the BSP list. That costs nothing,
     because they sit in the `svencoop` basedir at run time.
2. **neilm2.wad (They Hunger) is not indexed, but worth adding** to `wadtex.WADS` and the BSP list. It has the best
   small-town pieces of any WAD. Seen and confirmed:

   | Texture | Size | What it is |
   |---|---|---|
   | `nm_cop2` | 128x128 | sheriff star badge, for the police sign |
   | `nm_sign7` | 240x80 | "Drink Pepsi-Cola 5c", for the diner |
   | `{nm_sign10` | 240x32 | "Brakes Tune-ups Mufflers", for the garage |
   | `nm_sign8` | 112x48 | "Big Paul's Pawnshop" |
   | `nm_bb_16` | 192x256 | "The White Swan" pub sign |
   | `nm_grave1`, `nm_grave2`, `nm_grave4` | 64x112 | headstones (1 and 2 have skulls; 4 is plain) |
   | `nm_glass3`, `nm_glass8` | 64x128 | arched church windows |
   | `nm_door1`, `nm_door2`, `nm_door3` | 64x96 | house doors |
   | `nm_roof1`, `nm_roof3` | 80x64 | shingles |

   `nm_grave5` in deathmission is a pile of skulls, not a headstone.
3. **Masked `{` textures break on `.dyn` brushes.** `ref/gl/gl_dynworld.c:628` turns alpha test off for the
   diggable world, so the blue key colour would show. Use them only on `func_wall` or `func_illusionary` with
   `rendermode 4` and `renderamt 255`. That covers fences, cables, railings, blinds, laundry lines and graffiti.
4. **Looping sounds.** A sound loops when it is PCM WAV with a `cue ` chunk (loop start before the end), or Ogg/MP3
   with a `LOOPSTART` tag. Each sound below is marked LOOP or one-shot.
   - 24-bit WAVs do not load (only 8/16-bit). That rules out `bridge/ticking.wav`, `mustardf/bell.wav` and
     `th_escape/fluorescent.wav`.
   - The `mustardf/factorysounds/*` files are float or WAVE_EXTENSIBLE, which also fails, except `fridge.wav` (16-bit).
5. **sc_prop** (`game/dlls/svencraft/sc_entities.cpp`):
   - Always plays sequence 0, so tree variants chosen by sequence (`ouitz_tree1`) are not reachable.
   - `skin`, `body`, `scale` and `rendermode` work as plain entvars keys.
   - Several models have their origin off the base; the per-model notes below give the z offset needed.
6. **Mineable props** (`sc_props.cpp` `g_SCPropDefs`) match by substring. Props that already break into materials:
   `tree|oak|palm`, `bush|fern|plant`, `rock`, `barrel|drum`, `crate`, `trashcan`, `bench|chair|table`,
   `couch|sofa|bed`, `cabinet|locker`, `sink|toilet`, `radio|fan|clock`, `sandbag`, `lamp|light` and `forklift`.
   Anything wider than 256 that is not a tree counts as scenery. The new props below marked "new def" need an entry.

## 1. Layout proposal

```
+----------------------------------------------+   1 char = 64 units; x -1472..1472 (left->right), y 1088..-1024 (top->bottom)
|                                      ~~~~~~  | y=1088
| ..................+++++CC........   ~~~~~~~~ |      . flat zone (v3)       ~ rift (1150,760) r330
| ..................+++++CC........  ~~~~~~~~~~|      = main road  - sidewalk  | Elm St (new)  : back alley (new)
| ..................+++++CC.qqqqqxx  ~~~~~~~~~~|      O office  S shop  W warehouse  H house  p lot   (existing)
| ..................+++++CC.qqqqqxx  ~~~~~~~~~~| y=832   D diner  d patio   P police  q police lot
| ::::::::::::::::::+++++||.qqqqqxx  ~~~~~~~~~~|      C church  + graveyard   A apartments  f fire escape  y yard
| ...OOOOOOO........+++++||.qqqqqxx  ~~~~~~~~~~|      k park   g gas forecourt  c canopy  G kiosk + service bays
| ...OOOOOOO..SSSSS.....|||.PPPPPxx  ~~~~~~~~~~|      x rift checkpoint (military, alliance phase)   @ spawn
| ...OOOOOOO..SSSSS.DDDDD||.PPPPPxx   ~~~~~~~~~| y=576
| ...OOOOOOO..SSSSS.DDDDD||.PPPPPxx    ~~~~~~~ |
| ...OOOOOOO..SSSSS.DDDDD||.PPPPPxx      ~~~   |
| ..................DDDDD||.PPPPPxx            |
| ..................ddddd||.PPPPPxx            | y=320
| ..................ddddd||........            |
| ----------------------|||--------            |
|==============================================|
|==============================================| y=64
|===========@==================================|
|==============================================|
| ---------------------------------gggggggggg. |
| ..pppppppppppp...................gggggggggg. | y=-192
| ..pppppppppppkkkkk...............gccccccggg. |
| ..pppppppppppkkkkk.....AAAAAAA...gccccccggg. |
| ..pWWWWWWWWppkkkkkHHHH.AAAAAAff..gccccccggg. |
| ..pWWWWWWWWppkkkkkHHHH.AAAAAAff..gccccccggg. | y=-448
| ..pWWWWWWWWppkkkkkHHHH.AAAAAAff..gggggggggg. |
| ..pWWWWWWWWppp....HHHH.AAAAAAff..gggggGGGGG. |
| ..pWWWWWWWWppp.........AAAAAAA........GGGGG. |
| ..pWWWWWWWWppp.........yyyyyyyy.......GGGGG. | y=-704
| ..pWWWWWWWWppp.........yyyyyyyy.......GGGGG. |
| ..pppppppppppp.........yyyyyyyy.......GGGGG. |
| ..pppppppppppp.........yyyyyyyy............. |
|   pppppppppppp                  ............ | y=-960
+----------------------------------------------+
```

**Flat zone v3** (`flat_dist`): the town rectangle grows north to `y 1024`, and a second rectangle is added for the
gas station. The extension stays clear of the rift. `y 1024` is a 256-grid line, so the ground is exactly flat up to it
and starts rising on the next row.

```python
rects = [(-1400, -900, 700, 1024), (700, -1000, 1450, -128)]
```

Ground height checked with a copy of `height()`:

| Site | Height now | After the change |
|---|---|---|
| Gas station | 0..136 | 0 |
| Church site | 8..152 | 0 |
| Every other new site | 0 | 0 |

| Element | Footprint / position (x, y) | Notes |
|---|---|---|
| Elm St (N-S side street) | road x 40..200, y 128..768; sidewalks x 0..40 and 200..240 | `road2` turned 90 deg (u=(0,1,0), v=(1,0,0)); ends at the church forecourt (`cobble1a`, y 768..792). The church closes the view up the street. |
| Back alley | x -1400..-272, y 712..776 | `M_ASPHALT1` (patched), dumpsters, graffiti; dead-ends at the graveyard fence |
| Diner "Big Tony's" | x -256..16, y 296..536, h 160 (front to 200) | door y- at x -120; patio y 200..296 |
| Police station | x 264..616, y 296..616, h 232, 2 floors | door y- at x 440; lot x 264..616, y 640..840 (drive in from Elm St at y 660..740); flagpole (300,240). 223 units from the rift's edge (lot corner 204). |
| Church | nave x 40..200, y 792..1016; steeple x 88..152, y 792..856 | door y- (faces down Elm St) |
| Graveyard | x -240..24, y 680..1000 | fenced; gate on the Elm St side |
| Apartment block | x 96..480, y -704..-368, 3 storeys, h 352 | door y+ at x 288; fire escape on the x+ face x 480..528, y -640..-432; yard x 80..560, y -900..-720 |
| Park | x -540..-300, y -560..-260 | lawn, paths, memorial |
| Gas station | forecourt x 720..1360, y -600..-128; canopy x 816..1120, y -480..-256 (z 184..208); kiosk + 2 service bays x 1040..1360, y -840..-600, h 176 | back lot y -900..-840, chain-link fence |
| Rift checkpoint | x 600..700, y 300..900 | `snd/barrier.mdl` line, existing sandbags, military truck at (650,600) along y (147 units from the rift's edge), for the alliance arc |

**Moves forced by the new buildings:**
- Houndeyes at (300,600) and (360,650) go to (660,860) and (700,940).
- The headcrab at (650,420) goes to (680,420).
- The zombie at (500,-500) goes to (620,-640).
- The black car at (260,88) moves to (520,88), clear of the new crosswalk.
- The spawn at x -760..-520, y ±24 stays clear.

**Lamps:** keep the existing ones; their x positions avoid Elm St. Add brush lamps at (232,420) and (232,700) on Elm St,
and at (720,-176) and (1380,-176) by the gas station.

## 2. Buildings

Wall materials for `MATERIAL` (block ids from `sc_world.h`):

| Block | Wall textures |
|---|---|
| brick 18 | `WALL_BRICK2`, `WALL_BRICK5`, `Brick05`, `Brick05-2` |
| tan brick 19 | `PRXMSBRICK1A` |
| concrete 20 | `CinderBlock01`, `E_WALL_WHITE1`, `stucco_01` |
| planks 15 | `nm_farm04`, `nm_farm05`, `PRXOUTWOOD1A`, `OUT_WD` |
| metal 22 | `OUT_GALV1` |
| cobble 4 | `castlestone1`, `ROCK_GREY` |

### 2.1 Gas station + service garage ("Rockwell Fuel & Service")

**Exterior**

| Part | Texture (wad, px) | Notes |
|---|---|---|
| kiosk / bay walls | `PRXMSBRICK1A` (nw 128) | cream glazed brick, 1950s station look; `OUT_CONCRETE1` (tfc2 128) sills and trim |
| bay side and back walls | `OUT_GALV1` (halflife 128) | corrugated galvanised metal |
| roof | `DOUBLE_ASPHALT` (tfc 256) | parapet as in `building()` |
| kiosk door | `OFF_DR3` (tfc 128) | glass double doors in a wood frame (BSP: func_door_rotating) |
| kiosk windows | `GLASS_MED` (halflife 128) | big panes, `BA_STEEL_01` mullions |
| bay doors | `OFF_DR1` (tfc 192x176) | roll-up door with a window strip; func_door moving up (`angles -90 0 0`), 2 doors x 1192..1272 and 1280..1352, z 0..88. Alternative: `DOOR_GARAGE_02` (Opfor 192x144). |
| canopy fascia | `barrel_red_01` (barney 48x64) | red ribbed band; alternative `C1A3YELLOW` (halflife 80x96, yellow gradient) |
| canopy underside | `E_WALL_WHITE1` (tfc2 128) | with 4 flush `lampB_on` (nw 128x32) panels and `light` 255 250 235 120 at z 176 |
| canopy posts | `BA_STEEL_01` 16x16 | at (848,-448), (1088,-448), (848,-288), (1088,-288) |
| pump islands | `OUT_CONCRETE1` top, `IN_CURB1` sides | 40x144x6 at x 896 and 1040, y -440..-296; `STRIPES1` (halflife 32) bollards r4 h36 at the ends |
| pumps (brush) | sides `barrel_red_01`, faces `H2OTANK_FRONT` (Opfor 32x48, white panel with dials), top `BA_STEEL_01` | 28x20x60 boxes at y -400 and -336 on each island. Alternative face: `CIV_RECHARGER` (Opfor 64x128). |
| signs | `C1A4_FUEL3` (halflife 160x32 "Fuel Storage"), `nm_sign18` (neilm4 48x48 "fuel oil", dyn), `OUT_TNK1` (halflife 32x48 "Danger" tank) | price pylon at (744,-180): 2 posts to z 200 with a 96x8x64 board, generated `sign_gas` (section 7). `{nm_sign10` "Brakes Tune-ups Mufflers" once neilm2 is added. |
| forecourt | `OUT_SIDEWALK6` (tfc2 256, BSP) or `CONCRETETILE` (deathmission 256, dyn) | `{oil1` / `{oil2` decals (`decals.wad`, from the basedir) by the pumps; check that `infodecal` lands on dyn faces |
| back-lot fence | `{HLXFENCE1` (nw 256x192) | chain-link, func_wall rendermode 4, 96 high (scale 0.5), along y -900 and x 1408 |

**Interior**

| Area | Floor | Walls | Ceiling |
|---|---|---|---|
| Kiosk | `FIFTIES_FLR02` (halflife 64, black/grey checker) | `E_WALL_WHITE1` | `CEILING_TILE` (nw 128x64) + `lampB_on` |
| Bays | `CONCRETETILE` | `IN_WALL16` (tfc2 128, white-painted block) | `OUT_GALV1` underside |

Kiosk fittings:
- counter (`counter()` with `wood_panel_01`)
- shelf boxes faced with `ba_cola_01`, `ba_food_01`, `ba_snacks_01` (barney 64, product rows)
- a cooler wall of `ba_freezer_01` (barney 64) doors
- outside by the door: brush vending machines faced with `GEN_VEND1` (halflife 64x96), `SNACK_MACH_01` (Opfor 64x96)
  and `pepsifront` / `pepsiside1` (nw 128x256 / 96x256)
- `POSTER16` calendar (halflife 64x96)

Bays:
- `TOOLS` pegboard (Opfor 256x128) over a bench faced with `WORKBENCH` (Opfor 128x64)
- tyre stacks: cylinders r14 h10, sides `TIRE_02` (Opfor 64x32), tops `ba_tire_02` (barney 64)
- a brush car on blocks in bay 1

**Props** (sequence 0 bbox; z offset where the origin is not at the base)

| Model | Size (x,y,z) | Use | Mineable |
|---|---|---|---|
| `models/hunger/item_gascan.mdl` | 22x22x29 | red "Gasoline" cans by the bays | new def (metal) |
| `models/th_escape/item_carbattery.mdl` | 16x24x19 | "GR8" battery on the bench | new def |
| `models/ginsmodels/gins_drums.mdl` | 32x28x48 | oil drums, 2 skins | yes |
| `models/snd/woodbarrel.mdl` | 33x33x45 | rain barrel at the back | yes |
| `models/tool_box.mdl`, `models/mustardf/green_toolbox.mdl` | 21x40x24 | bench | new def |
| `models/mustardf/can_blue.mdl`, `can_yellow.mdl` | 20x20x25 | paint/oil cans | new def |
| `models/ginsmodels/gins_shelves1.mdl` | 25x71x90 | shelf with boxes, back pivot x 0..25 | new def (planks) |
| `models/ginsmodels/lag_radio.mdl` | 33x32x16 | on the counter | yes |
| `models/ginsmodels/gins_flatscrn.mdl` | 29x30x28 | till / computer | new def |
| `models/ginsmodels/gins_clock.mdl` | 2x15x16 | wall clock; faces +x, origin is its centre | yes |
| `models/ginsmodels/gins_sawhorse.mdl` | 99x50x29 | bay | new def |

**Sounds**
- `ambience/vendmachine.wav` (LOOP 4.9 s) at the vending machines, volume 4, small radius
- `ambience/freezer_fan.wav` (LOOP 2.9 s) at the cooler
- `ambience/fluorescent.wav` (LOOP 4.0 s) in the kiosk
- `ambience/electrical_hum1.wav` (LOOP 1.2 s) under the canopy
- in the bays: `sc_robination/arc_welder_long.wav` (LOOP 3.2 s) and `ambience/hammer.wav` (LOOP 4.4 s), toggled

### 2.2 Diner "Big Tony's Pizza"

**Exterior**

| Part | Texture | Notes |
|---|---|---|
| walls | `Brick05` (cs_bdog 256, dyn) or `WALL_BRICK5` (tfc2 256) | false front raised to 200 on y- |
| sign | `sign_pizza_01` (barney 128x128 "Big Tony's Pizza") | 112x4x112 board over the door at z 140..252; second board `sign_taco_01` ("Tesla's Tacos") on the x+ side wall. Add `nm_sign7` Pepsi-Cola once neilm2 is in. |
| storefront | `GLASS_MED` breakables, 2 x 96 wide, z 32..128 | `BA_STEEL_01` frames |
| door | `OFF_DR3` (tfc 128) | func_door_rotating at x -120 |
| awnings | `models/snd/awningL.mdl` 80x112x48, skin 2 = red stripes (0 orange, 1 blue) | origin is the top back edge (z -48..0, extends +x 80): put it on the wall face at z 136 with yaw 270 so it points -y |
| patio | `OUT_SIDEWALK6` slab y 200..296 | planters: dbox 64x24x20, `darkmoss` top + `arc_flower` |

**Interior**

| Floor | Walls | Ceiling |
|---|---|---|
| `FIFTIES_FLR03` (halflife 64, black/cream checker) | `FIFTIES_WALL14` (halflife 128, wood wainscot under cream) | `FIFTIES_CEIL01` (halflife 64x80) |

The kitchen strip (y 488..536) gets `FL_TILE1` (tfc2 256).

Brush-built:
- counter (`counter()`) along y 456..480
- 5 stools: cylinder r8 h28 in `BA_STEEL_01` with an r10 seat, `barrel_red_01` sides
- 3 booths on the window side: table 48x32 with `M_WOOD1` top; seat/back boxes faced `couch_main_t` (nw 128x256 brown leather)
- menu boards over the counter: `ba_pizzamenu` (barney 128x64) and `ba_tacomenu` (barney 256x64)
- back-bar shelf: `nm_bottles1` (neilm5 192x144, dyn)
- posters: `POSTER19` (Babe Ruth), `POSTER9` (eagle)
- kitchen: `ba_freezer_01` doors

**Props**

| Model | Size | Use | Mineable |
|---|---|---|---|
| `models/ginsmodels/wb_umbrella.mdl` | 147x163x126 | red/white "Bistro" patio umbrella, 2 on the patio | new def |
| `models/ginsmodels/wb_table01.mdl` + `wb_seat.mdl` | 35x35x29 / 27x20x37 | under the umbrellas | table yes / seat new def |
| `models/ginsmodels/gins_bartable.mdl` | 88x96x61, **z -48..13: origin = floor + 48** | wood cafe tables; body 2 = flower vase | yes |
| `models/ginsmodels/gins_ceilfan.mdl` | 92x92x54, hangs (z -54..0) | 2 ceiling fans, origin at the ceiling | yes |
| `models/ginsmodels/gins_butts.mdl` | 29x27x39 | ashtray can by the door | new def |
| `models/ginsmodels/gins_creteytrashcan.mdl` | 32x32x45 | concrete bin on the patio | yes |
| `models/ginsmodels/lag_radio.mdl` | 33x32x16 | radio on the counter | yes |
| `models/ginsmodels/gins_pot.mdl`, `gins_mug.mdl` | 15x16x15 / 8x6x6 | kitchen, counter | new def |
| `models/revil/Furnature/sink.mdl` | 21x36x63 | kitchen sink, sane pivot | yes |
| `models/hunger/vegitation/arc_flower.mdl` | 62x58x49 | planters | new def (plant) |

**Sounds**
- `hunger/thambs/radiomusic.wav` (LOOP 23.5 s, old-time radio music) at the radio, volume 3, small radius
- `fans/fan1.wav` (LOOP 1.9 s) at the fans, volume 2
- `ambience/freezer_fan.wav` in the kitchen
- optional `hunger/thambs3/crowd1.wav` / `crowd2.wav` one-shots

### 2.3 Police station (2 floors)

**Exterior**

| Part | Texture | Notes |
|---|---|---|
| walls | `WALL_BRICK2` (tfc2 256, dark red) | `OUT_CONCRETE1` trim; `building(..., floors=2)` |
| windows | glass + `{PRXMSBLIND1B` (nw 64, venetian blinds) | blinds as func_illusionary rendermode 4, 2 units inside the glass |
| door | `OFF_DR2` (tfc 128, dark glass doors) | steps as existing |
| sign | generated `sign_police` (section 7), or `nm_cop2` (neilm2) | over the door |
| flag | `POSTER11` (halflife 64x96, US flag) | 48x72 func_illusionary board at z 240..312 on a `BA_STEEL_01` pole r3 h320 at (300,240) |
| lot | `DOUBLE_ASPHALT` | stripes per section 3 |

**Interior**

| Area | Floor | Walls | Ceiling |
|---|---|---|---|
| Lobby | `C1A1_LINO` (already used) | `FIFTIES_WALL14` | `CEILING_TILE` + `lampB_on` |
| Cells | `CONCRETETILE` | `IN_WALL17` (tfc2 128, painted yellow block) | `CEILING_TILE` |
| Upstairs offices | `carpet_blue` (nw 256) | `wall_plaster_1` (nw 256) | `CEILING_TILE` |

Brush-built:
- front desk (`counter()`)
- bulletin board 96x2x64 faced with `nm_sign12` (corkboard, deathmission 96x64), with `nm_sign11` "WANTED! Unlawfully
  large" (neilm4 144x192) next to it
- 2 holding cells, 96x96:
  - bars: 2-unit func_wall faced with `{GATE_01` (Opfor 64x112), rendermode 4
  - cell door: func_door faced with `{GATE_01`
  - bunk: dbox 72x32x20 with `BEDDING` (Opfor 80x144) on top
- lockers: 32x24x96 boxes faced with `PRXLOCKER1A` (nw 64x128) or `s_locker` (barney 128)
- clock `CLOCK1` (halflife 64)
- indoor flag `POSTER11`
- desks with the existing `desk()` upstairs

**Props**

| Model | Size | Use | Mineable |
|---|---|---|---|
| `models/ginsmodels/lag_radio.mdl` | 33x32x16 | dispatcher set (mic + radar skins) | yes |
| `models/ginsmodels/gins_flatscrn.mdl` | 29x30x28 | desk terminal | new def |
| `models/ginsmodels/gins_deskchair.mdl` | 31x32x48, z -32..16 (+32) | desks | yes |
| `models/filecabinet.mdl` | 29x23x62 | offices | yes |
| `models/ginsmodels/bench3.mdl` | 90x32x46 | lobby | yes |
| `models/adamr/securitycam.mdl` | 13x24x20, hangs (z -20..0) | lobby, cells | new def |
| `models/revil/Furnature/toilet.mdl` | 45x27x48 | cells | yes |
| `models/ginsmodels/gins_pkgsgns.mdl` | 1x12x72; skin 0 "Reserved Parking", skin 1 handicap | lot | new def |
| `models/ginsmodels/lag_loudspeaker48.mdl` | 22x19x13, z -14..-1 (wall mount) | outer corner, z 180 | new def |
| `models/ginsmodels/iw_light_out.mdl` | 8x24x16 | wall lamps by the door | yes (light) |

**Sounds**
- `hunger/thambs/radio_static1.wav` (LOOP 6.7 s) at the dispatcher, volume 3
- random one-shots: `hunger/thambs/rs_cop1.wav` and `rs_cop2.wav` (police radio), `hunger/thambs/cop1.wav`,
  `hunger/thambs/telephone.wav` (ring)
- `ambience/fluorescent.wav` in the cells

### 2.4 Apartment block (3 storeys) with fire escape

**Exterior**

| Part | Texture | Notes |
|---|---|---|
| walls | `Brick05` (cs_bdog 256, dyn); blind west side `Brick05-2` (bricked windows painted in) | floor slabs at z 120 and 240 (extend `building()` beyond 2 floors) |
| windows | real glass on floors 1-2; `Window03a` / `Window03b` (cs_bdog 64x144, curtains) as flat fake windows on floor 3 and the back | |
| entrance | `FIFTIES_DR6` (halflife 64x96 wood door) | `OUT_CONCRETE1` stoop, 2 steps |
| roof | `DOUBLE_ASPHALT` | parapet; water tank: 64-wide cylinder, `OUT_WD` (halflife 128x96 weathered planks), `BA_STEEL_01` legs |
| boarded flat | `BOARD_WINDOW` (Opfor 96x128) | one window |
| fire escape | see below | x+ face, 3 levels |

Fire escape:
- platforms 48x208 at z 120 and 240, floor `OUT_GRATING1` (tfc2 32)
- railings `{GRATE1A` (nw 256x128), func_wall rendermode 4, 40 high
- stair flights of 6 steps (24 run, 20 rise), `OUT_GRATING1` treads
- drop ladder `{PRXLADDER1A` (nw 64) as func_illusionary, plus a func_ladder volume down to z 40

**Interior**

| Area | Floor | Walls | Ceiling |
|---|---|---|---|
| Hall | `IN_FLOOR3` (tfc2 128 wood) + `EASTCARPET2` runner (tfc2 256) | `EASTPLSTR2B` (tfc2 256x128, worn plaster) | `CEILING_TILE` |
| Flats | `carpet_green` | `wall_plaster_1` | `FIFTIES_CEIL01` |
| Kitchen | `FL_TILE2` (tfc2 256) | `wall_plaster_1` | `FIFTIES_CEIL01` |
| Bathroom | `M_FLOOR17` (tfc2 128 hex tile) | `wall_plaster_1` | `FIFTIES_CEIL01` |

Stairs: `4_STEPS01` (already used). Pictures: `PIC_02` (Opfor 96x80 family photo), `POSTER10` (surfer).

**Props**
- `models/ginsmodels/couch.mdl` (41x112x54, back at x 0)
- `gins_table20.mdl` (20x20x20) and `models/chair.mdl` (34x36x54)
- `lag_radio.mdl`
- `gins_flatscrn.mdl`
- bed: brush, `M_WOOD1` frame + `BEDDING`
- `revil/Furnature/sink.mdl` and `toilet.mdl`
- `gins_mirror3.mdl` (24x2x38)
- `gins_papersmags.mdl`, `gins_mug.mdl`
- plants: `gins_leafy.mdl` (38x48x51), `gins_motherinlawstongue.mdl` (19x20x46), `models/uplant3.mdl` (14x17x48)
- vases: `gins_vase_r.mdl` / `gins_vase_b.mdl` (13x15x36)

**Yard** (y -900..-720):
- `OUT_FENCE1` and `OUT_FENCE3` (tfc2 128, wooden; dyn is fine), 96 high
- clothesline: `{EASTLINE1`, `{EASTLINE2`, `{EASTLINE3` (tfc2 256x128, laundry), func_illusionary rendermode 4 between
  two posts textured `TELEPHONEPOLE` (tfc2 32x128) at x 160 and 416, y -800, z 60..124
- dumpster at (440,-760)
- ground `BOOT_GRASS_06` with a `BOOT_DIRT_01` worn path

**Sounds**
- hall: `ambience/fluorescent.wav`
- one flat: `hunger/thambs/radiomusic.wav`
- another flat: `ambience/crtnoise.wav` (LOOP 1.5 s, TV static)
- `hunger/thambs/dialtone.wav` (LOOP 7.1 s) at a phone left off the hook

### 2.5 Church + graveyard (end of Elm St)

**Exterior**

| Part | Texture | Notes |
|---|---|---|
| walls | `nm_farm05` (neilm5 256x80, white horizontal clapboard, dyn) | `OUT_CONCRETE1` plinth 16 high |
| roof | `nm_farm07` (neilm5 192x128, grey shingles) | gable 96 (`building(gable=...)`); alternative `SLATE_ROOF_1` (tfc2 256, BSP) |
| doors | `EASTDOOR1` (tfc2 256, arched planked double doors) | 2 func_door_rotating leaves, each 48x112, at x 120 on y- |
| windows | glass slits 32x80, 3 per long side, `M_WOOD1` frames | `nm_glass3` / `nm_glass8` (arched) once neilm2 is in |
| steeple | x 88..152, y 792..856, walls `nm_farm05` up to z 352 | belfry louvres `EASTWNDWBTALL` (tfc2 64x128) on 4 sides, z 260..340; spire is a prism to z 448 in `nm_farm07` |
| clock | `models/ginsmodels/gins_clock_tower.mdl` | 4x113x113 clock face; faces +x, so yaw 270 on the south side, z 220 |
| forecourt | `cobble1a` (deathmission 256, tiles very cleanly) | |

**Interior**
- floor `IN_FLOOR2` (tfc2 128 planks), aisle runner `M_CARPET2` or `ITAL_CARPET` (tfc2 256x128)
- walls: `wood_panel_01` wainscot to z 48, `wall_plaster_1` above
- ceiling under the slope: `M_CEILING3` (tfc2 192x160, wood beams)
- 6 rows of brush pews (seat 64x16 plus a 64x4x20 back), `M_WOOD1`
- altar: dbox 64x24x36 in `wood_panel_01`
- `models/adamr/vase.mdl` (37x37x121 tall urn) by the altar; `gins_vase_r` flowers on it
- 3 `models/adamr/ceiling_lamp.mdl` (30x30x45) with warm lights (255 220 170)

**Graveyard**
- headstones: dbox 24x6x40 in `ROCK_GREY` (tfc2 256) or `castlestone1` (deathmission 256), 3 rows of 5; a stone
  cross from 2 boxes. `nm_grave4` (neilm2) faces once that WAD is added.
- fence `OUT_FENCE3`
- ground `jungle_floor_02` (op4ctf 256, dark grass)
- trees: `models/sc_robination/dead_tree08.mdl` (125x91x252 bare trunk) and `models/ginsmodels/gins_oak2.mdl`
  (278x186x232, **z -132..99: origin = ground + 132**)

**Sounds**
- `hunger/thambs/bell2.wav` (one-shot 5.4 s; or `tfc/ambience/bell.wav`, 16-bit, 3.0 s), triggered on a timer or a
  story event
- `toonrun/organ.wav` (one-shot 15.9 s, 8-bit) when used
- outside: `hunger/thambs/crow.wav` and `hunger/thambs/owl1.wav` one-shots

### 2.6 Park (x -540..-300, y -560..-260)

- ground `OUT_GROUND5` (tfc2 256 lawn); cross paths in `OUT_SIDEWALK6`
- `models/ginsmodels/bench3.mdl` x2 (90x32x46)
- `models/ginsmodels/iw_picnic.mdl` (62x88x30, aluminium picnic table; new def)
- `models/hunger/vegitation/tree2.mdl` and `gins_oak2.mdl`
- 4 round flower beds: ring of `castlestone1` with `arc_flower` and `arc_bush` (178x169x146)
- memorial: dbox 48x48x80 in `castlestone1` with an `I_CHURCH1A` (tfc2 256x128, stone with bronze plaque) face
- `models/ginsmodels/gins_coinop_binocs70.mdl` (coin-op binoculars head, **z -64..-2: origin = ground + 64**) on a
  post, looking at the rift, as a story hook
- sounds: `hunger/thambs/leaves1.wav`, `ambience/wren1.wav`, `hunger/thambs/bird1.wav` one-shots

### 2.7 Optional extras (assets confirmed; not in the layout)

- **School:**
  - walls `IN_WALL17` / `IN_WALL18` (painted block), lockers `PRXLOCKER1A`, clock `CLOCK1`, flag `POSTER11`
  - shelves `books_02` (decay 64x128) and `ba_books` (barney 128x64), `EYE_CHART` (Opfor 96x128) in the nurse's room
  - no chalkboard in the indexed WADs, so generate one
- **Cinema** "Rosedale Theater":
  - `{nm_marqee1` / `{nm_marqee2` (neilm5 240x128 marquee "Now Playing Frankenstein")
  - posters `nm_movie01` (Frankenstein), `nm_movie02` (Dracula), `nm_movie03` (The Mummy), `nm_movie04`,
    `nm_movie11`, all neilm5
  - `nm_movie05` ("STAFF ONLY!")
  - sound `hunger/thambs/projector.wav` (8.8 s)
- **Derelict / condemned look:** `nm_sign13` (neilm4 80x32 "Danger Condemned"), `BOARD_WINDOW`, `CEIL_TILE_DMG`
  (Opfor), graffiti `{DGFgraffiti` / `{dgfwashere` (bridge3 240x128, func_illusionary rendermode 4 on alley walls)

## 3. Street furniture and outdoor detail

| Item | Asset | Build / position |
|---|---|---|
| Power poles | `models/snd/pole.mdl` (180x24x480; wooden pole, crossarm along x at yaw 0) | yaw 90 (crossarm across the road). Positions: y -232 at x -1344, -896, -448, 0, 448; then (740,-232) and (1380,-232). New def: log/planks. |
| Power lines | `{PRXCABLES1` / `{PRXCABLES1A` (nw 128x64, sagging cables); `{cap_wire` (nw 128, taut) | func_illusionary rendermode 4, 1 unit thick, from crossarm end to crossarm end at y -232±80, z 420..470, texture stretched along the span |
| Transformers | `models/ginsmodels/gins_transformer.mdl` (33x26x49, **hangs: z -49..0**) | on the poles at x 0 and 448, z 400, offset 12 to the road side |
| Electrical boxes | `models/ginsmodels/gins_substation.mdl` (122x96x179, z -64..115: origin = ground + 64); `models/ginsmodels/elecswitch.mdl` (wall box, 8x33x65) | substation north of the checkpoint (640,960); switch on the gas station bay wall |
| Fire hydrants | brush: cylinder r6 h28 + cap r8 h4 + 2 nozzles, `barrel_red_01` | (-760,172), (-240,-172), (232,172), (660,-172), (1120,-150) |
| Phone booth | brush 40x40x96: `GLASS_MED` sides, `BA_STEEL_01` frame/roof, back panel `nm_tely2` (deathmission 48x112, wall phone image, dyn) | (1320,-180) at the gas station; (-290,232) between shop and diner. Sound `hunger/thambs/dialtone.wav`, volume 1, tiny radius; or `telephone.wav` ring. |
| Mailbox | brush 20x16x40, `barrel_blu_01` body, `BA_STEEL_01` legs; generated `sign_mail` plate | (520,172) by the police station |
| Bus stop | shelter brush 128x72x96 (`GLASS_MED` back/sides, `BA_STEEL_01` frame, `OUT_GALV1` roof) + `bench3.mdl` + generated `sign_bus` on a pole | south verge x -200..-72, y -232..-160 |
| Benches | `models/ginsmodels/bench3.mdl` (yes) | bus stop, park x2, church lawn, Elm St (232,560) |
| Bins | `models/ginsmodels/gins_trashcan.mdl` (11x18x19), `gins_creteytrashcan.mdl` (32x32x45), `gins_butts.mdl` (ashtray) | by lamps, park, diner and police doors |
| Dumpsters | brush 96x48x56: sides `IN_DUMPSTER1` / `IN_DUMPSTER2` (tfc2 128), lid `OUT_DMPLID` (halflife 64x48), ends `OUT_DMP1C` (halflife 80x64) | alley (-560,744), behind the diner (-60,620), apartment yard (440,-760), gas station back lot (1300,-870). Each gets `ambience/flies.wav` (LOOP 8.8 s), volume 2, small radius. |
| Parking signs | `models/ginsmodels/gins_pkgsgns.mdl` (Reserved / handicap), `models/ginsmodels/gins_pkgsgn2.mdl` ("Authorized parking only", 1x18x72) | police lot; warehouse lot entrance (-560,-220) |
| Traffic signs | none in the WADs: generate `sign_stop` and `sign_speed` (section 7); 2x2 `BA_STEEL_01` post, board 32x2x32 | stop sign at the mouth of Elm St (32,212); speed signs at the town entrances (-1380,-220) and (1380,220) |
| Crosswalks | `WHITE` (halflife 32, pure white): non-solid func_illusionary bars 1 unit thick at z 2..3 | Main road east of Elm: 8 bars, each 96 (x) by 16 (y), x 216..312, y -120..120 at a 32 pitch. Across Elm St: 5 bars, 16 (x) by 88 (y), y 208..296, x 40..200. Stop line 8 wide, x 40..120, y 304..312. |
| Centre lines | `road2` has them painted in; for Elm St turn the texture; `road3` (bridge3 1024, double yellow + edge lines; scale 0.25 across a 256 road) is an option for the main road | |
| Parking lines | `WHITE` / `YELLOW` (halflife) bars 4 wide, 1 thick; or decals `{pstripe1` / `{pstripe2` (decals.wad 16x128 / 128x16), `{handi` (48x48) | warehouse lot (bays 112 wide), police lot, gas station |
| Kerb paint | `PRXPAVE3` (nw 128, red/white diagonal) on the kerb sides by the hydrants and the bus stop; `STRIPES1` (halflife 32) on bollards | |
| Manholes | `OUT_MANHOLE2` (tfc2 128), `MANHOLE_COVER` (Opfor 128), `ba_manhole_01` (barney 64): a 48-wide disc 1 unit above the road | (-600,40), (120,600) on Elm St, (900,-40) |
| Storm drains | `I_GRATE1` (tfc2 64x32 kerb grate) set into the kerb face; `OUT_DRAIN` (tfc2 128, drain in concrete) in the forecourt and lots; `OUT_GRATING1` (tfc2 32) | kerb drains at (-1000,128), (-400,-128), (500,128) |
| Kerbs, sidewalks | existing `OUT_SIDEWALK1` + `IN_CURB1`; `OUT_SIDEWALK6` (tfc2 256, large slabs) for plazas and the forecourt; `SIDEWALK1` (tfc 256) as a variant | |
| Fences | wood `OUT_FENCE1` / `OUT_FENCE2` / `OUT_FENCE3` (tfc2 128, solid, dyn OK); hedge on fence `nm_fence01` (neilm5 256x192, dyn); chain-link `{HLXFENCE1` / `{cj_fence` (nw 256x192); wood rail `{RAILING_02` (Opfor 64); camo net `{OUT_NET1` (halflife 128) | masked ones as func_wall rendermode 4 |
| Barriers | `models/snd/barrier.mdl` (24x80x40, jersey barrier; skin 1 is striped; new def: concrete); `concbar` (bridge3 1024x512) for brush barriers; `models/snd/gate.mdl` (13x157x190 metal gate) | rift checkpoint x 660, y 320..720 |
| Lamps | existing brush street lamps (`streetlight1`); `streetlight2` (bridge3 256x512) as a variant glass | wall lamps: `iw_light_out.mdl`; hanging: `adamr/ceiling_lamp.mdl`; ceiling tube: `snd/tubelight1.mdl` (24x128x8, z -8..0) |
| Awnings | `models/snd/awningL.mdl` (80x112x48) / `models/snd/awningM.mdl` (56x64x42), 3 skins (orange / blue / red stripes) | shop windows (existing shop too) |
| Market stall | `models/snd/stall.mdl` (106x114x92, striped cloth, 3 skins) | newspaper or fruit stand at the park entrance (-420,-290) |
| Vegetation | already used: `hunger/vegitation/tree1.mdl` (286x285x342), `tree2.mdl` (357x357x341), `ginsmodels/lag_oaktree.mdl` (552x369x462, **z -136..325**), `tree.mdl` (110x69x190), `bush1.mdl` (69x133x72), `bush2.mdl` (80x67x114), `fern1.mdl` (72x72x52) | |
| More vegetation | `hunger/vegitation/arc_bush.mdl` (178x169x146 hedge bush), `arc_flower.mdl` (62x58x49 pink flowers), `arc_fern.mdl` (127x120x82), `shrub1.mdl` (72x72x52 bare twigs), `fern2.mdl` (73x82x24 low), `zalec_tree1.mdl` (187x137x378 slim street tree), `arc_xer_tree1.mdl` (499x468x622 big leafy tree, **z -52..570**), `ginsmodels/gins_oak2.mdl` (**origin = ground + 132**), `sc_robination/dead_tree02.mdl` (377x377x591 white dead tree, **z -100..491**), `dead_tree08.mdl` (125x91x252 dead trunk), `svencooprpg2/cattail.mdl` (4x3x46 reeds), `svencooprpg2/mushroom.mdl` (41x40x39) | Street trees: `zalec_tree1` on the Elm St sidewalks. Dead trees and mushrooms on the rift's rim are a corruption hint. |
| Rocks | `models/big_rock.mdl` (33x31x33), `models/snd/stones1.mdl` (149x121x4 flat stepping stones), `models/snd/rock_L1.mdl` / `rock_L2.mdl` (656x803x512 outcrops, buried, scenery), `models/xen_rockgib_big.mdl` (38x43x27) | stepping stones on the church lawn |
| Seagulls | `models/ginsmodels/gins_gull.mdl` (16x16x10, 3 sequences, but sc_prop plays sequence 0) | on rooftops |

## 4. Vehicles

No civilian car, van, bus or truck **models** exist in Sven's content. Every `.mdl` was scanned, the addon folders
included. Vehicles are therefore brush-built like the current `car()`, from these HL texture sets (all BSP-safe, so they
work for `sc_car`):

| Vehicle | Textures (wad, px) | Notes |
|---|---|---|
| SUV (white, logo on the doors) | `TRK2_SIDE` (halflife 256x112), `TRK2_FRONT` (64x96), `TRK2_BACK` (64x112), `TRK2_HOOD` (64x64), `TRK2_ROOF` (64x96) | 4 matching views: an extruded box with a cab step. Park at the police lot and the apartment kerb (420,-88). |
| Semi trailer | `18WHL_OUTSIDE` (Opfor 64x128 ribbed aluminium), `18WHL_INSIDE` (64x128), `18WHL_FLOOR` (64), wheels `TRK_TIRE` (halflife 64) | 448x96x128 box behind the warehouse, x -1180..-732, y -960..-864 |
| Flatbed / tow truck | `FLATBED_TOP` (halflife 128x256), `FLATBED_BUMPER` (192x64), `FLATBED_ENGINE` (96x128) | gas station yard (1250,-560) |
| Military truck (alliance phase) | `truck_side` (bridge3 512x256), `truck_front` / `truck_rear` (224x256), `truck_bed` (192x256), `truck_wheel` (256) | rift checkpoint (650,600) along y |
| Shipping containers | `contain1a`, `contain2a` (bridge3 1024x512 sides), `contain1b` (512 ends); `bb_container_si` (256x128) / `bb_container_fr` (256) "Titan Shipping" (deathmission, dyn) | 320x96x104 boxes; 2 at x -680..-584, y -960..-640 |
| Truck back door | `PRXTRUCK1A` (nw 128) | box-truck rear |
| Police cars | existing `car()` with a generated livery (section 7) | 2 in the police lot (360,740) and (520,740) along y |

Model vehicles and wrecks for set dressing:

| Model | Size | Notes |
|---|---|---|
| `models/forklift_static.mdl` | 60x114x187, **z -119..68: origin = ground + 119** | already mineable; (-640,-520) by the containers |
| `models/ginsmodels/lag_cessna.mdl` | 340x444x99 | small plane; a crash in the hills |
| `models/snd/tank_dead.mdl` | 308x218x78 | wreck |
| `models/snd/tank_body.mdl` + `tank_turret.mdl` | 127x257x77 / 298x122x95 | |
| `models/crashed_osprey.mdl` | 710x796x302 | |
| `models/hunger/huey_apache.mdl` | 640x532x239 | |
| `models/loader.mdl` | 330x218x167 | |

These are scenery-sized and fit the military arc.

## 5. Ambient sound

Paths are relative to `run/svencoop/sound`. `ambient_generic` flags: 1 everywhere, 2/4/8 small/medium/large radius,
32 not looped. Health is volume (0..10).

Random intermittent one-shots (birds, dogs, radio chatter) have no stock HL entity. Two options:
- loop them with a `multi_manager` and a `trigger_random`-like chain;
- add a small game entity `sc_ambient_random` (wav list, min/max delay, radius).

**Outdoor beds (LOOP)**

| File | Length / format | Use |
|---|---|---|
| `tu3sday/forest_noise.wav` | 16.3 s, 22 kHz 8-bit | town-wide day bed (birds/forest), flag 1, volume 2 |
| `ambience/wind2.wav` | 9.9 s | general wind on the hills, large radius |
| `ambience/wind1.wav` | 2.9 s | |
| `misc/wind2.wav` | 2.6 s | |
| `hunger/thambs/zo_wind3.wav`, `zo_windg.wav`, `zo_coldw.wav` | 4.4 / 4.5 / 6.0 s, 22 kHz | gusty, colder: dusk or the rift |
| `descrcl/desertwind.wav` | 39.6 s, 16-bit | long smooth wind, no audible loop point |
| `ambience/crickets.wav`, `ambience/cricket.wav` | 3.6 / 1.9 s | night, grass edges |
| `hunger/thambs/crickets.wav`, `hunger/thambs/cricket.wav` | 4.4 / 3.2 s, 22 kHz | |
| `ambience/flies.wav` | 8.8 s | dumpsters |
| `ambience/waterrun.wav`, `ambience/water_flowing1.wav` | 2.6 / 1.8 s | gutter, broken hydrant |
| `ambience/drips.wav` | 7.3 s | |
| `ambience/electrical_hum1.wav` | 1.2 s | transformers, canopy lights |
| `ambience/siren.wav`, `hunger/thambs/siren1.wav` | 5.6 / 5.5 s | town alarm (story) |
| `hunger/thambs3/wagon.wav` | 10.0 s | creaking wheels |

**Outdoor one-shots**

| File | Length | Use |
|---|---|---|
| `ambience/wren1.wav`, `ambience/hawk1.wav`, `ambience/quail1.wav` | 1.1 / 0.2 / 0.3 s | birds |
| `hunger/thambs/bird1.wav` | 3.9 s | birds |
| `hunger/thambs/crow.wav` | 3.2 s | birds, graveyard |
| `hunger/thambs/weirdbird.wav` | 4.0 s | birds, rift hint |
| `hunger/thambs/owl1.wav` | 0.8 s | night |
| `hunger/thambs/frog.wav` | 0.4 s | night |
| `ambience/bee1.wav` | 1.2 s | flower beds |
| `hunger/thambs/dogamb1.wav` | 6.8 s | distant dog barking |
| `hunger/thambs/wolf_01.wav` | 5.8 s | night |
| `hunger/thambs/catyowl.wav` | 3.8 s | cat |
| `hunger/thambs/leaves1.wav` | 1.4 s | |
| `hunger/thambs/wind.wav`, `hunger/thambs/wind1.wav` | 29.4 / 17.4 s, not looped | gust events |
| `hunger/thambs/bell2.wav`; `tfc/ambience/bell.wav` | 5.4 s; 3.0 s | church bell |
| `ambience/jetflyby1.wav`, `hunger/thambs3/chopper.wav` | 7.6 / 6.8 s | aircraft passing |
| `misc/truck_stop.wav`, `misc/outro_truck.wav` | 3.5 / 4.7 s | distant traffic |
| `ambience/distantmortar1.wav` | 2.1 s | later story phases |
| `thunder.wav`, `hunger/thambs/thunder1.wav` | 4.5 / 3.2 s | |

There is no continuous distant-traffic loop in the content. Truck and plane one-shots every minute or two stand in for it.

**Indoor (LOOP unless noted)**

| File | Length / format | Use |
|---|---|---|
| `ambience/fluorescent.wav` | 4.0 s | fluorescent hum: police, kiosk, halls |
| `toonrun/lighthum.wav` | 1.4 s, 22 kHz | lighter tube hum |
| `ambience/electrical_hum2.wav` | 1.5 s | electrical rooms |
| `adamr/electricbuzz.wav` | 1.9 s | electrical rooms |
| `ambience/ventilation.wav` | 3.2 s, 22 kHz | AC/ventilation hum (rooftop AC boxes, offices) |
| `fans/fan1.wav`, `fans/fan3.wav` | 1.9 / 4.7 s | ceiling fan, wall fan |
| `ambience/freezer_fan.wav` | 2.9 s | fridges, coolers |
| `ambience/vendmachine.wav` | 4.9 s, 22 kHz | vending machines |
| `ambience/crtnoise.wav` | 1.5 s | TV static |
| `hunger/thambs/radiomusic.wav` | 23.5 s | old radio music: diner, flats |
| `hunger/thambs/radio_static1.wav`, `hunger/thambs/static.wav` | 6.7 / 3.7 s | police radio, a dead radio |
| `hunger/thambs/dialtone.wav` | 7.1 s | phone off the hook |
| `hunger/recorder/tape_machine.wav` | 1.5 s | tape recorder (police) |
| `common/nuke_ticking.wav` | 0.5 s, loop from sample 3 | the only ticking loop; a timer tick, so listen before using it as a wall clock |
| `sc_robination/arc_welder_long.wav` | 3.2 s | garage bays |
| `ambience/hammer.wav` | 4.4 s | garage bays |
| `ambience/littlemachine.wav`, `ambience/industrial2.wav` | 0.7 / 4.8 s | warehouse machinery |
| `sc_persia/persia_crowd.wav` | 28.9 s | crowd murmur; use at low volume (Middle-Eastern flavour) |
| one-shots: `hunger/thambs/telephone.wav` (2.1 s ring), `hunger/thambs/rs_cop1.wav` / `rs_cop2.wav`, `hunger/thambs/cop1.wav`, `grunts2/radio1.wav`, `radio/static.wav`, `hunger/thambs/projector.wav`, `toonrun/organ.wav`, `hunger/thambs3/crowd1.wav` / `crowd2.wav`, `misc/party1.wav`, `buttons/bell1.wav` (shop door bell 0.7 s), `mustardf/factorysounds/fridge.wav` (9.3 s, 16-bit 44 kHz) | | |

**Unusable** (checked): `bridge/ticking.wav`, `mustardf/bell.wav`, `th_escape/fluorescent.wav` (24-bit), and
`mustardf/factorysounds/conveyor*.wav`, `drone_loop.wav`, `mixing.wav` and `random_metal_drop.wav` (float or
extensible formats).

## 6. Terrain ground textures

Seam is the wrap-around edge difference divided by the normal neighbour difference; about 1.0 tiles cleanly. All are
in `town_v3_terrain.png`, tiled 2x2.

| Texture | WAD | Px | Seam x/y | Use |
|---|---|---|---|---|
| `rocky_grass_01` | op4ctf | 256 | 0.96/1.05 | current hills; keep as the base |
| `OUT_GROUND5` | tfc2 | 256 | 1.00/1.13 | mown lawn (park, yards, church lawn); light yellow-green |
| `OUT_GROUND4` | tfc2 | 128 | 0.94/1.00 | same grass at 128 (repeats more) |
| `BOOT_GRASS_06` | Opfor | 256 | 1.19/1.00 | lush dark-green grass; second lawn tone, damp areas |
| `jungle_floor_02` | op4ctf | 256 | 1.14/1.01 | dark grass with clover patches: hollows, graveyard |
| `OUT_GROUND9` | tfc2 | 256 | 1.02/1.13 | dry grass/dirt mix; hilltops, verges (mix with rocky_grass by noise) |
| `OUT_GRND1` | halflife | 160 | 1.06/0.99 | HL c1a0 scrub (red-brown with dark shrubs); a dry hillside variant |
| `OUT_GROUND1` | tfc2 | 192 | 1.07/1.21 | grey-purple scrub; rocky slopes |
| `OUT_DIRT1` | halflife | 128 | 0.96/1.08 | olive dirt (already used for cuts) |
| `OUT_DIRT2` | tfc2 | 128 | 1.02/0.85 | brown dirt: paths, yards |
| `BOOT_DIRT_01` | Opfor | 256 | 1.12/1.20 | plain path dirt; matches the BOOT_GRASS path tiles |
| `BOOT_GRASS_04` | Opfor | 256 | 1.14/1.17 | **dirt path through grass** (vertical in the texture, turn it for E-W); repeats along the path |
| `BOOT_GRASS_01` | Opfor | 256 | 1.18/1.03 | path widening / junction piece |
| `BOOT_GRASS_05` | Opfor | 256 | 1.11/**3.68** | path edge / end piece: one tile only, do not repeat in y |
| `DIRT_01` | Opfor | 64x128 | 1.21/1.15 | dark wet **mud** |
| `OUT_MUD1` | halflife | 128 | 0.94/0.59 | pink-red clay mud (reads alien; use sparingly, rift rim) |
| `CANYONSMUD1` | tfc | 128 | 1.00/0.63 | red-brown cracked mud |
| `Gravel01` | cs_bdog (dyn) | 256 | 0.81/0.79 | dark grey **gravel**: driveways, rail bed, gas station back lot (very clean) |
| `OUT_GRAVEL3` | tfc2 | 64 | 0.94/1.08 | light grey gravel (small repeat) |
| `OUT_GRAVEL` | halflife | 96 | 0.86/0.98 | near-black gravel/coal |
| `cobble1a` | deathmission (dyn) | 256 | 0.93/0.79 | cobbles: church forecourt, old square |
| `OUT_STONES1` | tfc2 | 128 | 1.00/1.23 | flagstone path |
| `OUT_HAY1` | tfc2 | 128 | 1.22/1.05 | hay (farm edge) |
| `ROCK_GREY` | tfc2 | 256 | 1.06/1.21 | pale limestone for steep faces; also headstones |
| `OUT_ROCK4` (current) / `OUT_ROCK5` | tfc2 | 256x160 | 1.23/1.34 / 1.08/1.15 | steep slopes (ROCK5 is blue-grey and blotchy) |

Suggestions:
- Pick hill textures by slope as now (`OUT_ROCK4` > 0.85, `OUT_DIRT1` > 0.5).
- Below that, vary the grass with `vnoise`: `rocky_grass_01` by default, `OUT_GROUND9` on high dry ground,
  `jungle_floor_02` in low wet spots.
- The town's own lawns use `OUT_GROUND5` and `BOOT_GRASS_06`.

## 7. Textures to generate in make_town.py (`TEX`, like the car paint)

The WADs have none of these:

| Name | Size | Content |
|---|---|---|
| `sign_stop` | 64x64 | red octagon, white "STOP" |
| `sign_speed` | 48x64 | white board, "SPEED LIMIT 35" |
| `sign_bus` | 48x64 | blue board, "BUS STOP" |
| `sign_mail` | 32x16 | "U.S. MAIL" |
| `sign_gas` | 96x64 | price board: brand name and "GAS .89 / DIESEL .79" |
| `sign_police` | 128x32 | "POLICE", gold on dark blue (or use `nm_cop2` after adding neilm2) |
| `car_police`, `carside_police` | 64 / 256x64 | white doors with a star on black (same shapes as `car_side()`) |
| `lightbar` | 32x8 | red/blue |

Use PIL `ImageDraw` text. Keep the palette low-saturation so the signs sit with the WAD art.

## 8. Material tables to extend

`tools/make_materials.py` `TEXTURES` (footsteps and impacts; only the first 12 characters count). Defaults to
concrete when missing.

| Type | Textures |
|---|---|
| W | `nm_farm04`, `nm_farm05`, `PRXOUTWOOD1A`, `IN_FLOOR2`, `IN_FLOOR3`, `M_WOOD1`, `OUT_FENCE2`, `OUT_FENCE3`, `couch_main_t` |
| M | `OUT_GALV1`, `IN_DUMPSTER1`, `OFF_DR1`, `18WHL_OUTSIDE`, `TRK2_SIDE`, `truck_side`, `contain1a` |
| D | `OUT_GROUND5`, `OUT_GROUND9`, `jungle_floor_02`, `CANYONSMUD1`, `darkmoss`, `Gravel01` |
| T | `FL_TILE1`, `FL_TILE2`, `M_FLOOR17` |
| Y | `GLASS_MED` |
| G | `OUT_GRATING1` |

Already tagged by Sven's list: `BOOT_GRASS_*` (D), `OUT_DIRT2` (D), `FIFTIES_FLR*` (T), `EASTCARPET2` (T),
`OUT_SIDEWALK*` (C), `barrel_*` (M), `IN_DOOR*` (M), `TIRE_01` (D), `OUT_MANHOLE2` (M).

New `g_SCPropDefs` keys:

| Keys | Drops |
|---|---|
| `pole` | log 2 |
| `transformer\|substation\|elecswitch\|gascan\|battery\|toolbox\|tool_box\|can_\|picnic\|flatscrn\|securitycam\|loudspeaker\|pkgsgn` | metal 1 |
| `shelves\|sawhorse` | planks 2 |
| `awning\|stall\|umbrella\|wb_seat` | planks 1 |
| `barrier` | concrete 2 |
| `flower\|shrub\|cattail\|mushroom\|leafy\|motherinlaw` | leaves 1 |
| `stones` | cobble 1 |
| `butts` | metal 1 |
