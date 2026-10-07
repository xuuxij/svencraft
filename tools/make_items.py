# Svencraft items: the one list of every inventory item, and everything generated from it
#   game/common/sc_items.h              ids, names, stack sizes, what crafting one gives (server and client)
#   sprites/svencraft/items.spr         48x48 icon per item id (frame = id; palette index 255 = transparent)
#   models/svencraft/itemflat.mdl       dropped non-block items: a flat icon, skin = id - SCI_FIRST_ITEM
# Blocks (ids below 64) take their names from game/dlls/svencraft/sc_inventory.cpp's order and their icons
# from the block textures; everything else is original 16x16 pixel art (tools/item_art.py).
import os, shutil, subprocess
import numpy as np
from PIL import Image
import item_art as art

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SDK / _BUILD)
ROOT = sc_paths.ROOT
GAME = sc_paths.GAMEDIR
WORK = sc_paths.work('items')
SDK = sc_paths.SDK
SPRGEN = os.path.join(SDK, 'sprites', 'sprgen.exe')
STUDIOMDL = os.path.join(SDK, 'modelling', 'studiomdl.exe')
os.makedirs(WORK, exist_ok=True)

# ---------------------------------------------------------------- the list
# kinds: block (placeable, id = block id), item (stacks in the inventory), tool / weapon / supply (crafting one
# hands it straight to the player: a tool tier, a Half-Life weapon, ammo or a pickup)
BLOCKS = ["Grass", "Dirt", "Stone", "Cobblestone", "Gravel", "Sand", "Bedrock", "Coal Ore", "Iron Ore", "Gold Ore",
          "Diamond Ore", "Xen Crystal Ore", "Log", "Leaves", "Planks", "Glass", "Mossy Cobblestone", "Bricks",
          "Tan Bricks", "Concrete", "Asphalt", "Scrap Metal", "Rubber", "Crafting Table", "Furnace", "Torch",
          "Torch", "Torch", "Torch", "Torch",      # 27-30: torches on walls (they drop as 26)
          "Furnace", "Furnace", "Furnace", "Furnace", "Furnace", "Furnace", "Furnace"]   # 31-37: facings, burning
FLAT_BLOCKS = {26: 'torch', 27: 'torch', 28: 'torch', 29: 'torch', 30: 'torch'}   # drawn flat, not as cubes
ITEMS = [   # id, constant, name, art
    (64, 'STICK', 'Stick', 'stick'),
    (65, 'COAL', 'Coal', 'coal'),
    (66, 'CHARCOAL', 'Charcoal', 'charcoal'),
    (67, 'IRON_INGOT', 'Iron Ingot', 'iron_ingot'),
    (68, 'GOLD_INGOT', 'Gold Ingot', 'gold_ingot'),
    (69, 'DIAMOND', 'Diamond', 'diamond'),
    (70, 'XEN_CRYSTAL', 'Xen Crystal', 'xen_crystal'),
    (71, 'GUNPOWDER', 'Gunpowder', 'gunpowder'),
    (72, 'CIRCUIT', 'Circuit Board', 'circuit'),
    (73, 'SHEARS', 'Shears', 'shears'),
    (74, 'APPLE', 'Apple', 'apple'),
]
TOOLS = [   # id, constant, name, tool, tier
    (96, 'WOOD_PICKAXE', 'Wooden Pickaxe', 'pickaxe', 1), (97, 'STONE_PICKAXE', 'Stone Pickaxe', 'pickaxe', 2),
    (98, 'IRON_PICKAXE', 'Iron Pickaxe', 'pickaxe', 3),
    (99, 'WOOD_SHOVEL', 'Wooden Shovel', 'shovel', 1), (100, 'STONE_SHOVEL', 'Stone Shovel', 'shovel', 2),
    (101, 'IRON_SHOVEL', 'Iron Shovel', 'shovel', 3),
    (102, 'WOOD_AXE', 'Wooden Axe', 'axe', 1), (103, 'STONE_AXE', 'Stone Axe', 'axe', 2), (104, 'IRON_AXE', 'Iron Axe', 'axe', 3),
    (105, 'DIAMOND_PICKAXE', 'Diamond Pickaxe', 'pickaxe', 4), (106, 'DIAMOND_SHOVEL', 'Diamond Shovel', 'shovel', 4),
    (107, 'DIAMOND_AXE', 'Diamond Axe', 'axe', 4),
]
TIER_ART = ['wood', 'stone', 'iron', 'diamond']
GIVES = [   # id, constant, name, art, classname given
    (112, 'CROWBAR', 'Crowbar', 'crowbar', 'weapon_crowbar'),
    (113, 'HANDGUN', '9mm Handgun', 'pistol', 'weapon_9mmhandgun'),
    (114, 'MAGNUM', '.357 Magnum', 'revolver', 'weapon_357'),
    (115, 'SHOTGUN', 'Shotgun', 'shotgun', 'weapon_shotgun'),
    (116, 'MP5', 'MP5', 'mp5', 'weapon_9mmAR'),
    (117, 'CROSSBOW', 'Crossbow', 'crossbow', 'weapon_crossbow'),
    (118, 'GRENADE', 'Hand Grenades', 'grenade', 'weapon_handgrenade'),
    (119, 'RPG', 'RPG Launcher', 'rpg', 'weapon_rpg'),
    (120, 'GAUSS', 'Tau Cannon', 'gauss', 'weapon_gauss'),       # found, not crafted
    (121, 'EGON', 'Gluon Gun', 'egon', 'weapon_egon'),
    (122, 'HORNETGUN', 'Hivehand', 'hornetgun', 'weapon_hornetgun'),
    (123, 'SATCHEL', 'Satchel Charges', 'satchel', 'weapon_satchel'),
    (124, 'TRIPMINE', 'Tripmines', 'tripmine', 'weapon_tripmine'),
    (125, 'SNARK', 'Snarks', 'snark', 'weapon_snark'),
    (128, 'AMMO_9MM', '9mm Rounds', 'ammo_9mm', 'ammo_9mmclip'),
    (129, 'AMMO_357', '.357 Rounds', 'ammo_357', 'ammo_357'),
    (130, 'AMMO_SHELLS', 'Shotgun Shells', 'shells', 'ammo_buckshot'),
    (131, 'AMMO_MP5', 'MP5 Magazine', 'ammo_mp5', 'ammo_9mmAR'),
    (132, 'AMMO_BOLTS', 'Crossbow Bolts', 'bolts', 'ammo_crossbow'),
    (133, 'AMMO_ROCKET', 'RPG Rockets', 'rocket', 'ammo_rpgclip'),
    (134, 'MEDKIT', 'Health Kit', 'medkit', 'item_healthkit'),
    (135, 'BATTERY', 'HEV Battery', 'battery', 'item_battery'),
    # Opposing Force's (game/dlls/gearbox), which Sven Co-op uses: found, not crafted
    (136, 'SAW', 'M249 SAW', 'saw', 'weapon_m249'),
    (137, 'DEAGLE', 'Desert Eagle', 'deagle', 'weapon_eagle'),
    (138, 'SNIPER', 'M40A1 Sniper Rifle', 'sniper', 'weapon_sniperrifle'),
    (139, 'WRENCH', 'Pipe Wrench', 'wrench', 'weapon_pipewrench'),
    (140, 'SHOCKRIFLE', 'Shock Roach', 'shockrifle', 'weapon_shockrifle'),
    (141, 'SPORELAUNCHER', 'Spore Launcher', 'sporelauncher', 'weapon_sporelauncher'),
    (142, 'DISPLACER', 'Displacer Cannon', 'displacer', 'weapon_displacer'),
    (143, 'GRAPPLE', 'Barnacle Grapple', 'grapple', 'weapon_grapple'),
    (144, 'AMMO_556', '5.56 Box', 'ammo_556', 'ammo_556'),
    (145, 'AMMO_762', '7.62 Rounds', 'ammo_762', 'ammo_762'),
    (146, 'AMMO_SPORE', 'Spore', 'spore', 'ammo_spore'),
    # Sven Co-op's own (game/dlls/svencraft/sc_minigun.cpp, sc_medkit.cpp)
    (147, 'MINIGUN', 'Minigun', 'minigun', 'weapon_minigun'),
    (148, 'FIRSTAID', 'Medkit', 'firstaid', 'weapon_medkit'),
]
NUM = 149
WEAPONS = [g for g in GIVES if g[4].startswith('weapon_')]
# flat models (itemflat.mdl drops, the hand's held item): the items, the torch, the tools, the weapons
FLAT_ORDER = [i for i, c, n, a in ITEMS] + [26] + [t[0] for t in TOOLS] + [g[0] for g in WEAPONS]
NUM_BLOCK_IDS = len(BLOCKS) + 1

# ---------------------------------------------------------------- the header
TOOLC = {'pickaxe': 1, 'shovel': 2, 'axe': 3}   # TOOL_* in game/dlls/svencraft/sc_game.h
rows = {}
for i, n in enumerate(BLOCKS, 1):
    rows[i] = ('"%s"' % n, 64, 'SCI_BLOCK', 'NULL', 0, 0)
UNSTACKED = {'SHEARS'}
for i, c, n, a in ITEMS:
    rows[i] = ('"%s"' % n, 1 if c in UNSTACKED else 64, 'SCI_ITEM', 'NULL', 0, 0)
for i, c, n, t, tier in TOOLS:
    rows[i] = ('"%s"' % n, 1, 'SCI_TOOL', 'NULL', TOOLC[t], tier)
for i, c, n, a, cls in GIVES:
    rows[i] = ('"%s"' % n, 1, 'SCI_WEAPON' if cls.startswith('weapon_') else 'SCI_SUPPLY', '"%s"' % cls, 0, 0)

h = ['/*', 'sc_items.h - Svencraft inventory items (generated by tools/make_items.py: edit that, not this)', '',
     'Ids below SCI_FIRST_ITEM are blocks (sc_world.h); items, tools and weapons live in the inventory (the hotbar slot',
     'decides what is held: a tool, a Half-Life weapon, or the hand with a block); supplies (ammo, health, armour) are',
     'handed straight over when crafted.',
     '*/', '#ifndef SC_ITEMS_H', '#define SC_ITEMS_H', '',
     'enum { SCI_NONE = 0, SCI_BLOCK, SCI_ITEM, SCI_TOOL, SCI_WEAPON, SCI_SUPPLY };', '',
     '#define SCI_FIRST_ITEM\t64', '#define SCI_COUNT\t%d' % NUM, '', 'enum', '{']
for i, c, n, a in ITEMS:
    h.append('\tSCITEM_%s = %d,' % (c, i))
for i, c, n, t, tier in TOOLS:
    h.append('\tSCITEM_%s = %d,' % (c, i))
for i, c, n, a, cls in GIVES:
    h.append('\tSCITEM_%s = %d,' % (c, i))
h += ['};', '', '#define SCI_FLAT_TORCH_SKIN	%d	// itemflat.mdl: skins are the items in order, then the torch' % len(ITEMS),
      '#define SCI_NUM_BLOCK_IDS	%d	// v_schand.mdl: skins 0..this-1 are blocks, then the flat skins' % NUM_BLOCK_IDS]
flat = {i: k for k, i in enumerate(FLAT_ORDER)}
for b in FLAT_BLOCKS:
    flat[b] = flat[26]
h += ['', '// itemflat.mdl skin for each id drawn flat (items, torches, tools, weapons), -1 for cubes and the rest',
      'static const signed char g_SCFlatSkin[SCI_COUNT] =', '{']
vals = [str(flat.get(i, -1)) for i in range(NUM)]
for k in range(0, NUM, 16):
    h.append('\t' + ', '.join(vals[k:k + 16]) + ',')
h += ['};']
h += ['', 'typedef struct', '{', '\tconst char\t*name;\t\t// NULL: no item with this id', '\tint\t\tstack;',
      '\tint\t\tkind;', '\tconst char\t*give;\t\t// weapons and supplies: the entity handed over',
      '\tint\t\ttool;\t\t// tools: TOOL_* (sc_game.h: 1 pickaxe, 2 shovel, 3 axe) and tier', '\tint\t\ttier;', '} scitem_t;', '',
      'static const scitem_t g_SCItems[SCI_COUNT] =', '{']
for i in range(NUM):
    if i in rows:
        n, st, k, g, t, tier = rows[i]
        h.append('\t/* %3d */ { %s, %d, %s, %s, %s, %d },' % (i, n, st, k, g, t, tier))
    else:
        h.append('\t/* %3d */ { NULL, 0, SCI_NONE, NULL, 0, 0 },' % i)
h += ['};', '', 'static inline const scitem_t *SC_Item( int id )', '{',
      '\treturn ( id > 0 && id < SCI_COUNT && g_SCItems[id].name ) ? &g_SCItems[id] : 0;', '}', '', '#endif // SC_ITEMS_H', '']
open(os.path.join(ROOT, 'game', 'common', 'sc_items.h'), 'w', newline='\n').write('\n'.join(h))

# ---------------------------------------------------------------- new pixel art
art.MAT.update({
    'K': ((76, 76, 82), (44, 44, 50), (22, 22, 26)),          # coal
    'Q': ((96, 74, 58), (62, 46, 36), (34, 24, 20)),          # charcoal
    'I': ((246, 246, 250), (210, 210, 218), (150, 150, 162)), # iron
    'A': ((255, 244, 140), (246, 204, 54), (184, 132, 22)),   # gold
    'M': ((200, 255, 250), (90, 226, 224), (32, 150, 164)),   # diamond
    'Z': ((255, 240, 160), (246, 184, 62), (196, 110, 30)),   # Xen crystal (amber, glowing)
    'U': ((156, 156, 156), (112, 112, 112), (74, 74, 74)),    # gunpowder
    'V': ((96, 176, 86), (52, 132, 54), (30, 92, 36)),        # circuit board
})
NEW = {
    'stick': ["................", "................", "............WW..", "...........WW...", "..........WW....",
              ".........WW.....", "........WW......", ".......WW.......", "......WW........", ".....WW.........",
              "....WW..........", "...WW...........", "..WW............", "................", "................", "................"],
    'coal': ["................", "................", "................", ".....KKKK.......", "....KKKKKKK.....",
             "...KKKKKKKKK....", "...KKKgKKKKKK...", "..KKKKKKKKgKK...", "..KKKKKKKKKKK...", "..KKgKKKKKKKKK..",
             "...KKKKKKKKKK...", "...KKKKKgKKK....", "....KKKKKKK.....", "......KKK.......", "................", "................"],
    'iron_ingot': ["................", "................", "................", "................", "................",
                   ".....IIIIIIII...", "....IIIIIIIIII..", "...IIIIIIIIIIII.", "..IIIIIIIIIIII..", ".IIIIIIIIIIII...",
                   ".IIIIIIIIIII....", "..IIIIIIIII.....", "................", "................", "................", "................"],
    'diamond': ["................", "................", "....MMMMMMMM....", "...MMwMMMMMMM...", "..MMwMMMMMMMMM..",
                ".MMMMMMMMMMMMMM.", ".MMMMMMMMMMMMMM.", "..MMMMMMMMMMMM..", "...MMMMMMMMMM...", "....MMMMMMMM....",
                ".....MMMMMM.....", "......MMMM......", ".......MM.......", "................", "................", "................"],
    'xen_crystal': ["................", "........Z.......", ".......ZZ.......", ".......ZZZ......", "......ZZwZ......",
                    "......ZZZZ..Z...", "..Z...ZZZZ.ZZ...", "..ZZ..ZZwZ.ZZ...", "..ZZZ.ZZZZZZZ...", "...ZZZZZZZZZ....",
                    "...ZZZZZZZZZ....", "....ZZZZZZZ.....", "....ZZZZZZZ.....", ".....ZZZZZ......", "................", "................"],
    'gunpowder': ["................", "................", "................", "................", "................",
                  "................", ".......UU.......", "......UUUU......", ".....UUdUUU.....", "....UUUUUUdU....",
                  "...UUdUUUUUUU...", "..UUUUUUUdUUUU..", ".UUUUdUUUUUUUUU.", "..UUUUUUUUUUUU..", "................", "................"],
    'circuit': ["................", "................", "..VVVVVVVVVVVV..", "..VyyyyVVVVyyV..", "..VVVVyVVVVyVV..",
                "..VddddVddVyVV..", "..VddddVddVyyV..", "..VVVVyVVVVVyV..", "..VyyyyVVyyyyV..", "..VyVVVVVyVVVV..",
                "..VyVddddyVddV..", "..VyVddddyVddV..", "..VyyyyyyyVVVV..", "..VVVVVVVVVVVV..", "................", "................"],
}
NEW['torch'] = ["................", "................", "................", ".......yy.......", "......ywwy......",
                "......yyyy......", ".......zz.......", ".......WW.......", ".......WW.......", ".......WW.......",
                ".......WW.......", ".......WW.......", ".......WW.......", ".......WW.......", "................", "................"]
E16 = '................'
NEW['gauss'] = [E16, E16, E16, "....OOOOOO......", "...OBBBBBBO.....", "..SOBBwBBBOSSSS.", "..SOBBBBBBOSSSS.",
                "..SOBBBBBBOSSS..", "...OBBBBBBO.....", "....OOOOOO......", "....PP...PP.....", "....PP..........",
                E16, E16, E16, E16]
NEW['egon'] = [E16, E16, E16, E16, "...CCCCCCCC.....", "..CCCCCCCCCSSS..", "..CCdCCdCCCSSSB.", "..CCCCCCCCCSSS..",
               "...PPPPPPPP.....", "....PP..PP......", "....PP..........", E16, E16, E16, E16, E16]
NEW['hornetgun'] = [E16, E16, E16, "......OOOO......", "....OOOOOOOO....", "...OOyOOyOOOO...", "..OOOOOOOOOODD..",
                    "..OOOyOOyOOODDD.", "...OOOOOOOOODD..", "....OOOOOOOO....", "......DD........", ".....DDD........",
                    E16, E16, E16, E16]
NEW['satchel'] = [E16, E16, ".....SSSSSS.....", "....S......S....", "....S......S....", "...DDDDDDDDDD...",
                  "...DDDDDDDDDD...", "...DDrDDDDDDD...", "...DDDDDDDDDD...", "...DDDDDDDDDD...", "...DDDDDDDDDD...",
                  E16, E16, E16, E16, E16]
NEW['tripmine'] = [E16, E16, E16, E16, E16, "...PPPPPPPP.....", "...PPPPPPPPS....", "...PPdddPPPSrr..",
                   "...PPPPPPPPS....", "...PPPPPPPP.....", "....SS..SS......", E16, E16, E16, E16, E16]
NEW['snark'] = [E16, E16, E16, E16, "......EEEE......", "....EEEEEEEE....", "...EEwnEEwnEE...", "...EEEEEEEEEE...",
                "..EEEEEEEEEEEE..", "..EEyEEEEEEyEE..", "...EEEEEEEEEE...", "....E.E..E.E....", E16, E16, E16, E16]
NEW['shears'] = [E16, E16, "...........I....", "..........II....", ".........II.....", "....I...II......",
                 "....II.II.......", ".....III........", "......I.........", ".....PIP........", "....PP..PP......",
                 "...PP....PP.....", "...P......P.....", "...PP....PP.....", "....PPPPPP......", E16]
NEW['apple'] = [E16, E16, ".......D........", ".......D.GG.....", ".......DGG......", "....EEEDEEE.....",
                "...EEwEEEEEE....", "..EEwEEEEEEEE...", "..EEEEEEEEEEE...", "..EEEEEEEEEEE...", "..EEEEEEEEEEE...",
                "...EEEEEEEEE....", "...EEEEEEEEE....", "....EEE.EEE.....", E16, E16]
# Opposing Force's weapons and ammo
art.MAT.update({
    'J': ((150, 224, 255), (70, 160, 230), (36, 96, 170)),     # shock roach blue
    'L': ((190, 236, 100), (128, 190, 56), (80, 132, 32)),     # spore green
    'N': ((198, 142, 170), (156, 100, 132), (108, 64, 92)),    # alien flesh
    'T': ((160, 244, 210), (70, 196, 152), (32, 124, 98)),     # displacer glow
})
NEW['saw'] = [E16, E16, E16, "......PPPP......", ".PP..PPPPPPP....", ".PPPPPPPPPPPSSS.", ".PPPPPPPPPPP....",
              ".PP...GGGG.P....", ".P....GGGG..P...", "......GGGG...P..", "......GGGG......", E16, E16, E16, E16, E16]
NEW['deagle'] = [E16, E16, E16, E16, ".CCCCCCCCCCCCC..", ".CCCCCdddCCCCC..", ".CCCCCCCCCCCCC..", "..CCCC.C..C.....",
                 "..PPPP.CCCC.....", ".PPPP...........", ".PPPP...........", ".PPPP...........", "..PP............",
                 E16, E16, E16]
NEW['sniper'] = [E16, E16, E16, ".....SSSSSS.....", "......S..S......", ".DD.DPPPPPPPPPP.", ".DDDDDDDDP......",
                 ".DDDD..P........", ".DDD............", E16, E16, E16, E16, E16, E16, E16]
NEW['wrench'] = [E16, "..SSS...........", ".SS.SS..........", ".S...S..........", ".SS..SS.........",
                 "..SSSSSS........", ".....SSS........", "......EEE.......", ".......EEE......", "........EEE.....",
                 ".........EEE....", "..........EEE...", "...........EEE..", "............EE..", E16, E16]
NEW['shockrifle'] = [E16, E16, E16, "......NNNN......", "....NNNNNNNN....", "..JJNNNNNNNNNJJ.", ".JwJNNNNNNNNNJJ.",
                     "..JJNNNNNNNNN...", "....NNNNNN......", ".....NN.NN......", "......N..N......", E16, E16, E16, E16, E16]
NEW['sporelauncher'] = [E16, E16, E16, E16, "....LLLL........", "...LLwLLLNNNNNN.", "...LLLLLLNNNNNN.",
                        "...LLLLLLNNNNN..", "....LLLL..NN....", "..........NN....", "...........N....",
                        E16, E16, E16, E16, E16]
NEW['displacer'] = [E16, E16, E16, "....SSSSSSS.....", "...STTTTTTTS....", "..SSTTwTTTTSSSS.", "..SSTTTTTTTSSSS.",
                    "...STTTTTTTS....", "....SSSSSSS.....", ".....PP..PP.....", "......P.........", E16, E16, E16, E16, E16]
NEW['grapple'] = [E16, E16, E16, "...NNNNN........", "..NNNNNNN.......", "..NNrrrNNsssssss", "..NNNNNNN.......",
                  "...NNNNN........", "....N.N.N.......", E16, E16, E16, E16, E16, E16, E16]
NEW['ammo_556'] = [E16, E16, E16, E16, "...Y.Y.Y.Y......", "...Y.Y.Y.Y......", "..GGGGGGGGGG....", "..GGGGGGGGGG....",
                   "..GGnnnnnnGG....", "..GGGGGGGGGG....", "..GGGGGGGGGG....", E16, E16, E16, E16, E16]
NEW['ammo_762'] = [E16, E16, E16, "....O...O...O...", "...YYY.YYY.YYY..", "...YYY.YYY.YYY..", "...YYY.YYY.YYY..",
                   "...YYY.YYY.YYY..", "...YYY.YYY.YYY..", "...YYY.YYY.YYY..", "...YYY.YYY.YYY..", E16, E16, E16, E16, E16]
NEW['spore'] = [E16, E16, E16, E16, ".....LLLL.......", "....LLwLLL......", "...LLwLLLLL.....", "...LLLLLLLL.....",
                "...LLLLLLLL.....", "....LLLLLL......", ".....LLLL.......", E16, E16, E16, E16, E16]
NEW['minigun'] = [E16, E16, E16, "....PPPPP.......", "...PSSSSSPPPPPP.", "..PPSdSdSSSSSSS.", "..PPSSSSSSSSSSS.",
                  "..PPSdSdSSSSSSS.", "...PSSSSSPPPPPP.", "....PPPPP.......", ".....PP.........", "....PPP.........",
                  E16, E16, E16, E16]
NEW['firstaid'] = [E16, E16, ".....dddddd.....", "....d......d....", "..EEEEEEEEEEEE..", "..EEEEEwwEEEEE..",
                   "..EEEEEwwEEEEE..", "..EEEwwwwwwEEE..", "..EEEwwwwwwEEE..", "..EEEEEwwEEEEE..", "..EEEEEwwEEEEE..",
                   "..EEEEEEEEEEEE..", "...PP......PP...", E16, E16, E16]
NEW['charcoal'] = [r.replace('K', 'Q') for r in NEW['coal']]
NEW['gold_ingot'] = [r.replace('I', 'A') for r in NEW['iron_ingot']]
GRIDS = dict(art.ITEM_GRIDS, **NEW)


def item_icon16(name):
    return art.pixel_icon(GRIDS[name])


# ---------------------------------------------------------------- block icons (isometric cubes)
defs = {}
for line in open(os.path.join(GAME, 'scripts', 'blocks.txt')):
    t = line.split()
    if len(t) >= 5 and t[0].isdigit():
        front = [f.split('=')[1] for f in t[5:] if f.startswith('front_')]
        defs[int(t[0])] = (t[2], t[3], front[0] if front else None)


def tex_rgba(name):
    return np.array(Image.open(os.path.join(GAME, 'gfx', 'blocks', name + '.tga')).convert('RGBA'))


FS = 48
CX, HW, HT, SH, TOP = 24.0, 22.0, 11.0, 24.0, 1.0
SHADE = {'top': 1.0, 'left': 0.8, 'right': 0.6}


def render_cube(top, side, front=None):
    trgb, srgb = tex_rgba(top), tex_rgba(side)
    frgb = tex_rgba(front) if front else srgb
    out = np.zeros((FS, FS, 3), np.float32); op = np.zeros((FS, FS), bool)
    ys, xs = np.mgrid[0:FS, 0:FS]; sx, sy = xs + 0.5, ys + 0.5
    for face in ('top', 'left', 'right'):
        if face == 'top':
            a = (sx - CX) / HW; b = (sy - TOP) / HT
            u, v = (a + b) / 2, (b - a) / 2
            src = trgb
        elif face == 'left':
            u = (sx - (CX - HW)) / HW; v = (sy - TOP - HT - u * HT) / SH; src = frgb
        else:
            z = ((CX + HW) - sx) / HW; v = (sy - TOP - HT - z * HT) / SH; u = 1 - z; src = srgb
        inside = (u >= 0) & (u <= 1) & (v >= 0) & (v <= 1)
        tu = np.clip((u * 64).astype(int), 0, 63) % src.shape[1]; tv = np.clip((v * 64).astype(int), 0, 63) % src.shape[0]
        px = src[tv, tu]
        m = inside & (px[..., 3] > 128)
        out[m] = px[..., :3][m] * SHADE[face]
        op[m] = True
    return out, op


icons = {}
for i in range(1, len(BLOCKS) + 1):
    if i in FLAT_BLOCKS:
        icons[i] = art.x3(*item_icon16(FLAT_BLOCKS[i]))
    elif i in defs:
        icons[i] = render_cube(*defs[i])
for i, c, n, a in ITEMS:
    icons[i] = art.x3(*item_icon16(a))
for i, c, n, t, tier in TOOLS:
    icons[i] = art.x3(*art.pixel_icon(art.TOOL_GRIDS[t], {'H': art.TIER[TIER_ART[tier - 1]]}))
for i, c, n, a, cls in GIVES:
    icons[i] = art.x3(*item_icon16(a))

# ---------------------------------------------------------------- items.spr
rgb = np.zeros((NUM * FS, FS, 3), np.uint8)
op = np.zeros((NUM * FS, FS), bool)
for i, (r, o) in icons.items():
    rgb[i * FS:(i + 1) * FS] = np.clip(r, 0, 255).astype(np.uint8)
    op[i * FS:(i + 1) * FS] = o
q = Image.fromarray(rgb, 'RGB').quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
idx = np.array(q)
idx[~op] = 255
pal = q.getpalette()[:765] + [0, 0, 255]
qcl = ['$spritename items', '$type vp_parallel', '$texture alphatest']
PER = 16
for si in range(0, NUM, PER):
    sheet = np.full((4 * FS, 4 * FS), 255, np.uint8)
    n = min(PER, NUM - si)
    for k in range(n):
        r_, c_ = divmod(k, 4)
        sheet[r_ * FS:(r_ + 1) * FS, c_ * FS:(c_ + 1) * FS] = idx[(si + k) * FS:(si + k + 1) * FS]
    im = Image.fromarray(sheet, 'P'); im.putpalette(pal)
    name = 'items_%d.bmp' % (si // PER)
    im.save(os.path.join(WORK, name))
    qcl.append('$load ' + name)
    for k in range(n):
        r_, c_ = divmod(k, 4)
        qcl.append('$frame %d %d %d %d' % (c_ * FS, r_ * FS, FS, FS))
open(os.path.join(WORK, 'items.qc'), 'w', newline='\n').write('\n'.join(qcl) + '\n')
if os.path.exists(os.path.join(WORK, 'items.spr')):
    os.remove(os.path.join(WORK, 'items.spr'))
r = subprocess.run([SPRGEN, 'items.qc'], cwd=WORK, capture_output=True, text=True)
assert os.path.exists(os.path.join(WORK, 'items.spr')), r.stdout + r.stderr
shutil.copy2(os.path.join(WORK, 'items.spr'), os.path.join(GAME, 'sprites', 'svencraft', 'items.spr'))

# preview sheet for checking the art
prev = Image.new('RGB', (16 * 52, ((NUM + 15) // 16) * 52), (60, 60, 60))
for i, (r_, o) in icons.items():
    tile = np.full((FS, FS, 3), 60, np.uint8); tile[o] = np.clip(r_[o], 0, 255)
    prev.paste(Image.fromarray(tile), ((i % 16) * 52 + 2, (i // 16) * 52 + 2))
prev.save(os.path.join(ROOT, 'tools', 'shots', 'items_preview.png'))

# ---------------------------------------------------------------- itemflat.mdl (dropped items)
skins = []
FLAT_ART = {i: ('item_' + a, item_icon16(a)) for i, c, n, a in ITEMS}
FLAT_ART[26] = ('item_torch', item_icon16('torch'))
for i, c, n, t, tier in TOOLS:
    FLAT_ART[i] = ('tool_%s%d' % (t, tier), art.pixel_icon(art.TOOL_GRIDS[t], {'H': art.TIER[TIER_ART[tier - 1]]}))
for i, c, n, a, cls in WEAPONS:
    FLAT_ART[i] = ('item_' + a, item_icon16(a))
for i in FLAT_ORDER:
    a, (r16, o16) = FLAT_ART[i]
    big = np.repeat(np.repeat(np.clip(r16, 0, 255).astype(np.uint8), 4, 0), 4, 1)
    obig = np.repeat(np.repeat(o16, 4, 0), 4, 1)
    qq = Image.fromarray(big, 'RGB').quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    ii = np.array(qq); ii[~obig] = 255
    out = Image.fromarray(ii.astype(np.uint8), 'P'); out.putpalette(qq.getpalette()[:765] + [0, 0, 255])
    out.save(os.path.join(WORK, '%s.bmp' % a))
    skins.append('%s.bmp' % a)
H = 8.0
smd = ['version 1', 'nodes', '0 "root" -1', 'end', 'skeleton', 'time 0', '0 0 0 0 0 0 0', 'end', 'triangles']
for nx, quad in ((1, [(0, -H, -H), (0, H, -H), (0, H, H), (0, -H, H)]), (-1, [(0, H, -H), (0, -H, -H), (0, -H, H), (0, H, H)])):
    uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for tri in ((0, 1, 2), (0, 2, 3)):
        smd.append(skins[0])
        for k in tri:
            x, y, z = quad[k]; u, v = uvs[k]
            smd.append('0 %.2f %.2f %.2f %d 0 0 %.4f %.4f' % (x, y, z + H, nx, u, v))
smd.append('end')
open(os.path.join(WORK, 'itemflat.smd'), 'w', newline='\n').write('\n'.join(smd) + '\n')
qc = ['$modelname "itemflat.mdl"', '$cd "."', '$cdtexture "."', '$scale 1.0', '$origin 0 0 0 270',
      '$bbox -8 -8 0 8 8 16', '$body "body" "itemflat"', '$sequence "idle" "itemflat" fps 1']
qc += ['$texrendermode "%s" "masked"' % s_ for s_ in skins]
qc += ['$texturegroup "skinfamilies"', '{'] + ['\t{ "%s" }' % s_ for s_ in skins] + ['}']
open(os.path.join(WORK, 'itemflat.qc'), 'w', newline='\n').write('\n'.join(qc) + '\n')
if os.path.exists(os.path.join(WORK, 'itemflat.mdl')):
    os.remove(os.path.join(WORK, 'itemflat.mdl'))
r = subprocess.run([STUDIOMDL, 'itemflat.qc'], cwd=WORK, capture_output=True, text=True)
assert os.path.exists(os.path.join(WORK, 'itemflat.mdl')), r.stdout[-2000:]
shutil.copy2(os.path.join(WORK, 'itemflat.mdl'), os.path.join(GAME, 'models', 'svencraft', 'itemflat.mdl'))
print('sc_items.h: %d ids; items.spr %d frames; itemflat.mdl %d skins' % (NUM, NUM, len(skins)))
