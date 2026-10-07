# Block textures for the Minecraft-style world: converts the prototype's 64x64 pixel-art BMPs to TGA
# (alpha from palette index 255 for see-through blocks), adds the extra ores/bedrock/mossy cobble, and
# writes scripts/blocks.txt. Original art (procedural), not Minecraft's.
import os
import numpy as np
from PIL import Image

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SVEN)
SRC = os.path.join(sc_paths.ASSETS, 'blocktex', 'tex')   # the prototype's art (assets_src/blocktex/maketex.py)
GAME = sc_paths.GAMEDIR
OUT = os.path.join(GAME, 'gfx', 'blocks')
os.makedirs(OUT, exist_ok=True)
os.makedirs(os.path.join(GAME, 'scripts'), exist_ok=True)
rng = np.random.default_rng(2026)
MASKED = {'glass', 'leaves'}   # (the torch is made here with its own alpha)


def load(name):
    im = Image.open(os.path.join(SRC, name + '.bmp'))
    idx = np.array(im)
    pal = np.zeros((256, 3), np.uint8)
    p = np.array(im.getpalette()[:768], np.uint8).reshape(-1, 3)
    pal[:len(p)] = p
    rgb = pal[idx]
    a = np.full(idx.shape, 255, np.uint8)
    if name in MASKED:
        a[idx == 255] = 0
    return np.dstack([rgb, a])


def save(name, rgba):
    Image.fromarray(rgba.astype(np.uint8), 'RGBA').save(os.path.join(OUT, name + '.tga'))


def art16(rgba):
    return rgba[::4, ::4].astype(np.float32)          # the 64x64 textures are 16x16 art upscaled 4x


def up4(a):
    return np.repeat(np.repeat(a, 4, 0), 4, 1)


def ore(base, fleck, edge, clusters, seed):
    r = np.random.default_rng(seed)
    a = art16(base).copy()
    for _ in range(clusters):
        cx, cy = r.integers(2, 14, 2)
        for _ in range(r.integers(3, 6)):
            x, y = cx + r.integers(-1, 2), cy + r.integers(-1, 2)
            a[y, x, :3] = fleck + r.normal(0, 10, 3)
            if y + 1 < 16:
                a[y + 1, x, :3] = edge
    return up4(np.clip(a, 0, 255))


for n in ['grass_top', 'grass_side', 'dirt', 'stone', 'cobble', 'gravel', 'sand', 'iron_ore', 'crystal_ore',
          'log_top', 'log_side', 'leaves', 'planks', 'glass']:
    save(n, load(n))

stone = load('stone')
save('coal_ore', ore(stone, np.array((30, 30, 32.0)), np.array((70, 70, 72.0)), 6, 1))
save('gold_ore', ore(stone, np.array((250, 215, 70.0)), np.array((170, 130, 30.0)), 5, 2))
save('diamond_ore', ore(stone, np.array((110, 235, 240.0)), np.array((40, 150, 160.0)), 4, 3))

b = rng.normal(0, 1, (16, 16, 1))
bed = np.clip(np.array((70, 70, 72.0)) + b * 30, 0, 255)
bed[rng.random((16, 16)) < 0.25] = (25, 25, 27)
save('bedrock', np.dstack([up4(bed), np.full((64, 64), 255)]))

cob = art16(load('cobble')).copy()
moss = rng.random((16, 16)) < 0.35
cob[moss, :3] = cob[moss, :3] * 0.5 + np.array((60, 110, 40.0)) * 0.5
save('mossy_cobble', up4(np.clip(cob, 0, 255)))

# crafting table and furnace, drawn over the planks / cobble / stone art
pl = art16(load('planks'))
dark = np.array((74, 52, 26.0))
top = pl.copy()
top[[0, 15], :, :3] = dark; top[:, [0, 15], :3] = dark            # frame
top[[5, 10], 1:15, :3] = top[[5, 10], 1:15, :3] * 0.55            # the 3x3 grid scored into the top
top[1:15, [5, 10], :3] = top[1:15, [5, 10], :3] * 0.55
save('craft_top', up4(np.clip(top, 0, 255)))
side = pl.copy()
side[0:2, :, :3] = dark; side[:, [0, 15], :3] = dark
saw = [(3, 3), (3, 4), (3, 5), (3, 6), (3, 7), (4, 3), (4, 4), (4, 5), (4, 6), (4, 7), (4, 8), (5, 8)]   # (y, x) blade
for y, x in saw:
    side[y + 3, x, :3] = (170, 170, 178)
for x in range(3, 9):
    side[9, x, :3] = (120, 120, 128)                                    # teeth line
for y in range(4, 12):
    side[y, 12, :3] = (96, 66, 34)                                      # hammer handle
side[3:5, 10:15, :3] = (130, 130, 138)                                  # hammer head
save('craft_side', up4(np.clip(side, 0, 255)))
st = art16(load('stone')).copy()
ftop = st.copy()
ftop[[0, 15], :, :3] *= 0.7; ftop[:, [0, 15], :3] *= 0.7
save('furnace_top', up4(np.clip(ftop, 0, 255)))
fs = art16(load('stone')).copy() * 1.05                                 # smooth light stone, like Minecraft's
fs[[0, 15], :, :3] *= 0.75; fs[:, [0, 15], :3] *= 0.75
fs[[5, 10], 1:15, :3] *= 0.85                                           # courses of stonework
save('furnace_side', up4(np.clip(fs, 0, 255)))
ff = fs.copy()                                                          # the front: one face, toward whoever placed it
ff[7:13, 4:12, :3] = (28, 26, 26)                                       # the mouth
ff[7, 4:12, :3] = (60, 58, 58); ff[12, 4:12, :3] = (90, 88, 88)
ff[11, 5:11, :3] = (70, 40, 20)                                         # ash
ff[4, 3:13, :3] = (110, 108, 108)                                       # lintel
save('furnace_front', up4(np.clip(ff, 0, 255)))
fl = ff.copy()                                                          # burning: flames in the mouth
FIRE = {'r': (196, 52, 18), 'o': (246, 132, 30), 'y': (255, 214, 72), 'w': (255, 246, 196)}
for y, row in enumerate(["..r..r..", ".rorrorr", "royorroy", "oyyoyywo", "oywyyywy"]):
    for x, ch in enumerate(row):
        if ch in FIRE:
            fl[7 + y, 4 + x, :3] = FIRE[ch]
fl[12, 4:12, :3] = (150, 90, 40)                                        # glowing embers on the sill
save('furnace_front_lit', up4(np.clip(fl, 0, 255)))

# torch: a stick in the middle two columns with its flame on rows 6-7 (the renderer draws only that part)
tor = np.zeros((16, 16, 4), np.float32)
tor[6, 7:9] = (255, 250, 196, 255)
tor[7, 7:9] = (255, 196, 64, 255)
tor[8:16, 7] = (128, 94, 54, 255)
tor[8:16, 8] = (98, 70, 38, 255)
save('torch', up4(tor))

BLOCKS = [  # id name top side bottom flags - ids must match game/dlls/svencraft/sc_world.h
    (1, 'grass', 'grass_top', 'grass_side', 'dirt', ''),
    (2, 'dirt', 'dirt', 'dirt', 'dirt', ''),
    (3, 'stone', 'stone', 'stone', 'stone', ''),
    (4, 'cobble', 'cobble', 'cobble', 'cobble', ''),
    (5, 'gravel', 'gravel', 'gravel', 'gravel', ''),
    (6, 'sand', 'sand', 'sand', 'sand', ''),
    (7, 'bedrock', 'bedrock', 'bedrock', 'bedrock', ''),
    (8, 'coal_ore', 'coal_ore', 'coal_ore', 'coal_ore', ''),
    (9, 'iron_ore', 'iron_ore', 'iron_ore', 'iron_ore', ''),
    (10, 'gold_ore', 'gold_ore', 'gold_ore', 'gold_ore', ''),
    (11, 'diamond_ore', 'diamond_ore', 'diamond_ore', 'diamond_ore', ''),
    (12, 'crystal_ore', 'crystal_ore', 'crystal_ore', 'crystal_ore', ''),
    (13, 'log', 'log_top', 'log_side', 'log_top', ''),
    (14, 'leaves', 'leaves', 'leaves', 'leaves', 'alpha'),
    (15, 'planks', 'planks', 'planks', 'planks', ''),
    (16, 'glass', 'glass', 'glass', 'glass', 'alpha joined'),
    (17, 'mossy_cobble', 'mossy_cobble', 'mossy_cobble', 'mossy_cobble', ''),
    (24, 'crafting_table', 'craft_top', 'craft_side', 'planks', ''),
    (25, 'furnace', 'furnace_top', 'furnace_side', 'furnace_top', 'front_ny=furnace_front'),   # front toward -y
    (26, 'torch', 'torch', 'torch', 'torch', 'alpha torch light14'),
    (27, 'wall_torch_px', 'torch', 'torch', 'torch', 'alpha torch_px light14'),
    (28, 'wall_torch_nx', 'torch', 'torch', 'torch', 'alpha torch_nx light14'),
    (29, 'wall_torch_py', 'torch', 'torch', 'torch', 'alpha torch_py light14'),
    (30, 'wall_torch_ny', 'torch', 'torch', 'torch', 'alpha torch_ny light14'),
    # the furnace's other facings, and all four burning (lit like Minecraft's: 13)
    (31, 'furnace_px', 'furnace_top', 'furnace_side', 'furnace_top', 'front_px=furnace_front'),
    (32, 'furnace_nx', 'furnace_top', 'furnace_side', 'furnace_top', 'front_nx=furnace_front'),
    (33, 'furnace_py', 'furnace_top', 'furnace_side', 'furnace_top', 'front_py=furnace_front'),
    (34, 'furnace_lit', 'furnace_top', 'furnace_side', 'furnace_top', 'front_ny=furnace_front_lit light13'),
    (35, 'furnace_lit_px', 'furnace_top', 'furnace_side', 'furnace_top', 'front_px=furnace_front_lit light13'),
    (36, 'furnace_lit_nx', 'furnace_top', 'furnace_side', 'furnace_top', 'front_nx=furnace_front_lit light13'),
    (37, 'furnace_lit_py', 'furnace_top', 'furnace_side', 'furnace_top', 'front_py=furnace_front_lit light13'),
    # realistic blocks: Half-Life textures at world scale (1 texel = 1 unit), so they match the map's walls
    (18, 'brick', 'wall_brick1', 'wall_brick1', 'wall_brick1', 'world'),
    (19, 'tan_brick', 'wall_brick3', 'wall_brick3', 'wall_brick3', 'world'),
    (20, 'concrete', 'out_concrete1', 'out_concrete1', 'out_concrete1', 'world'),
    (21, 'asphalt', 'out_asphalt1', 'out_asphalt1', 'out_asphalt1', 'world'),
    (22, 'metal', 'prxmetal5a', 'prxmetal5a', 'prxmetal5a', 'world'),
    (23, 'rubber', 'tire_02', 'tire_02', 'tire_02', 'world'),
]
import wadtex
for name in ('WALL_BRICK1', 'WALL_BRICK3', 'OUT_CONCRETE1', 'OUT_ASPHALT1', 'PRXMETAL5A', 'TIRE_02'):
    im, _ = wadtex.load(name)
    save(name.lower(), np.dstack([np.array(im), np.full(im.size[::-1], 255)]))
with open(os.path.join(GAME, 'scripts', 'blocks.txt'), 'w', newline='\n') as f:
    f.write('// Svencraft block looks: id name top side bottom [alpha] [joined]   (ids: game/dlls/svencraft/sc_world.h)\n')
    for b in BLOCKS:
        f.write(' '.join(str(x) for x in b).rstrip() + '\n')
print('textures:', len(os.listdir(OUT)), 'blocks:', len(BLOCKS))
