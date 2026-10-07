# sound/materials.txt for Svencraft: which material (footsteps, bullet impacts) every surface is.
# Svencraft's own entries come first, then Sven Co-op's whole list. Blocks are traced as "sc_block<id>"
# (engine pm_trace.c World_TraceTexture), the diggable town faces by their Half-Life texture names.
#   C concrete (the default)  M metal  D dirt  V vent  G grate  T tile  S slosh  W wood  P computer
#   Y glass  F flesh  N snow
import os

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SVEN)
ROOT = sc_paths.ROOT
SRC = os.path.join(sc_paths.SVEN, 'sound', 'materials.txt')
DST = os.path.join(sc_paths.GAMEDIR, 'sound', 'materials.txt')

# block ids: game/dlls/svencraft/sc_world.h
BLOCKS = {
    'D': [1, 2, 5, 6, 14, 23],                  # grass, dirt, gravel, sand, leaves, rubber
    'W': [13, 15, 24, 26, 27, 28, 29, 30],      # log, planks, crafting table, torches
    'Y': [16],                                  # glass
    'M': [22],                                  # metal
    'C': [3, 4, 7, 8, 9, 10, 11, 12, 17, 18, 19, 20, 21, 25] + list(range(31, 38)),   # stone, ores, bricks, furnaces
}
# the town (maps_src/make_town.py): ground, roads, buildings, cars
TEXTURES = {
    'C': ['OUT_SIDEWALK1'],
    'D': ['ROCKY_GRASS_01', 'OUT_DIRT1', 'TIRE_01', 'OUT_GROUND5', 'OUT_GROUND9', 'JUNGLE_FLOOR_02', 'CANYONSMUD1', 'DARKMOSS',
          'GRAVEL01', 'BOOT_GRASS_06', 'OUT_DIRT2'],
    'T': ['SLATE_ROOF_1', 'FL_TILE1', 'FL_TILE2', 'M_FLOOR17'],
    'W': ['NM_FARM04', 'NM_FARM05', 'NM_FARM07', 'PRXOUTWOOD1A', 'IN_FLOOR2', 'IN_FLOOR3', 'M_WOOD1', 'OUT_FENCE1', 'OUT_FENCE2',
          'OUT_FENCE3', 'COUCH_MAIN_T', 'DESK_GEN', 'DESK_DRAWER1', 'WOOD_PANEL_01', 'EASTWOOD01', 'EASTDOOR1', 'OUT_WD', 'CRATE02',
          'DTGLWOODW101', 'GREYWOOD'],
    'M': ['CAR_RED', 'CAR_BLUE', 'CAR_WHITE', 'CAR_BLACK', 'CAR_GREEN', 'CARSIDE_RED', 'CARSIDE_BLUE', 'CARSIDE_WHIT',
          'CARSIDE_BLAC', 'CARSIDE_GREE', 'CAR_FRONT', 'CAR_BACK', 'CAR_UNDER', 'CAR_CHROME', 'STREETLIGHT1', 'CAR_POLICE',
          'CARSIDE_POLI', 'LIGHTBAR', 'OUT_GALV1', 'IN_DUMPSTER1', 'OFF_DR1', 'BA_STEEL_01', 'BARREL_RED_01', 'BARREL_BLU_01',
          'DOOR_GARAGE_02', 'DOOR_MTL_01', 'H2OTANK_FRONT'],
    'Y': ['CAR_WINDOW', 'GLASS_DARK', 'GLASS_MED'],
    'G': ['OUT_GRATING1'],
}

lines = ['// Svencraft: the block world and the town (tools/make_materials.py)']
for t, ids in BLOCKS.items():
    lines += ['%s SC_BLOCK%d' % (t, i) for i in ids]
for t, names in TEXTURES.items():
    lines += ['%s %s' % (t, n[:12]) for n in names]
lines.append('')
os.makedirs(os.path.dirname(DST), exist_ok=True)
sven = open(SRC, 'rb').read().decode('latin-1').replace('\r\n', '\n')
# only 12 characters count: Sven's names that look the same as ours to the game go (OUT_SIDEWALK6A is metal)
ours = {l.split()[1][:12].upper() for l in lines[1:] if l}
sven = '\n'.join(l for l in sven.split('\n') if not (l[:1].isalpha() and len(l.split()) > 1 and l.split()[1][:12].upper() in ours))
open(DST, 'w', newline='\n', encoding='latin-1').write('\n'.join(lines) + '\n// Sven Co-op\n' + sven)
print('materials.txt: %d of ours + Sven\'s' % (len(lines) - 2))
