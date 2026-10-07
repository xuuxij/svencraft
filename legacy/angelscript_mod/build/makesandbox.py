# Builds svencraft.wad (block textures) and svencraft_sandbox.map (Valve 220) for the SDK compilers.
import struct, os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
TEX = os.path.join(HERE, 'tex'); OUT = os.path.join(HERE, 'map')
os.makedirs(OUT, exist_ok=True)
GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop"
ZHLT = r"C:\Program Files (x86)\Steam\steamapps\common\Sven Co-op SDK\mapping\compilers\zhlt.wad"

# ---------- WAD3 ----------
def bedrock():
    rng = np.random.default_rng(7)
    img = np.clip(np.array((60, 60, 62), float) + rng.normal(0, 14, (16, 16, 1)), 0, 255)
    img[rng.random((16, 16)) < 0.2] = (30, 30, 32)
    return Image.fromarray(img.astype(np.uint8), 'RGB').quantize(256).resize((64, 64), Image.NEAREST)

WADTEX = {  # wad name -> source bmp
    'sc_grass_top': 'grass_top', 'sc_grass_side': 'grass_side', 'sc_dirt': 'dirt', 'sc_stone': 'stone',
    'sc_cobble': 'cobble', 'sc_brick': 'brick', 'sc_planks': 'planks', 'sc_log_top': 'log_top', 'sc_log_side': 'log_side',
    '{sc_leaves': 'leaves', 'sc_sand': 'sand', 'sc_gravel': 'gravel', 'sc_snow': 'snow', 'sc_metal': 'metal',
    'sc_concrete': 'concrete', 'sc_tile': 'tile', 'sc_rubber': 'rubber', '{sc_glass': 'glass', 'sc_circuit': 'circuit',
    'sc_vent': 'vent', 'sc_bench_top': 'bench_top', 'sc_bench_side': 'bench_side', 'sc_iron_ore': 'iron_ore', 'sc_crystal': 'crystal_ore', 'sc_bedrock': None,
}

def miptex(name, img):
    assert img.mode == 'P' and img.size[0] % 16 == 0
    w, h = img.size
    pal = (img.getpalette() + [0] * 768)[:768]
    mips = [np.array(img, np.uint8)]
    for k in (1, 2, 3):
        mips.append(np.ascontiguousarray(mips[0][::2 ** k, ::2 ** k]))  # nearest: keeps pixel art and masked index 255
    hdr = 16 + 4 * 2 + 4 * 4
    offs, data, o = [], b'', hdr
    for m in mips:
        offs.append(o); data += m.tobytes(); o += m.size
    return name.encode().ljust(16, b'\0') + struct.pack('<II4I', w, h, *offs) + data + struct.pack('<H', 256) + bytes(pal) + b'\0\0'

# Minecraft-style face shading baked into textures: _x = sides facing X, _y = sides facing Y, _b = bottom.
SHADES = {'_x': 0.70, '_y': 0.85, '_b': 0.55}

def shaded(img, k):
    out = img.copy()
    pal = (img.getpalette() + [0] * 768)[:768]
    pal = [int(v * k) if i < 255 * 3 else v for i, v in enumerate(pal)]
    out.putpalette(pal)
    return out

lumps = []
for wname, src in WADTEX.items():
    img = bedrock() if src is None else Image.open(os.path.join(TEX, src + '.bmp'))
    lumps.append((wname, miptex(wname, img)))
    if src is not None:
        for suf, k in SHADES.items():
            lumps.append((wname + suf, miptex(wname + suf, shaded(img, k))))
blob, dirents, pos = b'', b'', 12
for name, body in lumps:
    dirents += struct.pack('<iiibbh16s', pos, len(body), len(body), 0x43, 0, 0, name.encode().ljust(16, b'\0'))
    blob += body; pos += len(body)
wadpath = os.path.join(OUT, 'svencraft.wad')
open(wadpath, 'wb').write(b'WAD3' + struct.pack('<ii', len(lumps), pos) + blob + dirents)

# ---------- MAP ----------
BLOCK = 40     # world units per block (must match SC_BLOCK_SIZE in sc_blocks.as)
SCALE = BLOCK / 64.0   # 64px texture covers one block
AXES = {  # outward normal -> (u, v) plane tangents with u x v = normal, then texture U/V axes
    (0, 0, 1):  ((1, 0, 0), (0, 1, 0), (1, 0, 0), (0, -1, 0)),
    (0, 0, -1): ((0, 1, 0), (1, 0, 0), (1, 0, 0), (0, -1, 0)),
    (1, 0, 0):  ((0, 1, 0), (0, 0, 1), (0, 1, 0), (0, 0, -1)),
    (-1, 0, 0): ((0, 0, 1), (0, 1, 0), (0, 1, 0), (0, 0, -1)),
    (0, 1, 0):  ((0, 0, 1), (1, 0, 0), (1, 0, 0), (0, 0, -1)),
    (0, -1, 0): ((1, 0, 0), (0, 0, 1), (1, 0, 0), (0, 0, -1)),
}

def fmt(q):
    return '( %d %d %d )' % tuple(q)

def box(mn, mx, tex, scale=None):
    """Axis-aligned brush. tex: one texture name, or dict with 'top', 'bottom', 'side'."""
    sc = SCALE if scale is None else scale
    if isinstance(tex, str):
        tex = {'top': tex, 'bottom': tex, 'side': tex}
    lines = ['{']
    for n, (u, v, tu, tv) in AXES.items():
        p = [mx[i] if n[i] > 0 else mn[i] for i in range(3)]
        p0 = [p[i] + u[i] * 64 for i in range(3)]
        p2 = [p[i] + v[i] * 64 for i in range(3)]
        t = tex['top'] if n[2] > 0 else tex['bottom'] if n[2] < 0 else (tex.get('sx', tex.get('side')) if n[0] else tex.get('sy', tex.get('side')))
        lines.append('%s %s %s %s [ %d %d %d 0 ] [ %d %d %d 0 ] 0 %s %s' % (fmt(p0), fmt(p), fmt(p2), t, *tu, *tv, sc, sc))
    lines.append('}')
    return '\n'.join(lines)

def ent(kv, brushes=()):
    return '{\n' + ''.join('"%s" "%s"\n' % (k, v) for k, v in kv.items()) + ''.join(b + '\n' for b in brushes) + '}\n'

H, TOP, CEIL = 32 * BLOCK, 1536, 1552   # half-size of the play area (64x64 blocks), sky height
BEDROCK = -11 * BLOCK                   # top of the bedrock = bottom of the terrain (11 blocks deep)
FLOOR = -1056                           # bottom of the hidden template room
world = [
    box((-H, -H, BEDROCK - 64), (H, H, BEDROCK), 'sc_bedrock'),                                     # bedrock (seals the template room)
    box((-H, -H, FLOOR + 16), (-H + 16, H, BEDROCK - 64), 'sc_bedrock'),                           # template room walls: no sky,
    box((H - 16, -H, FLOOR + 16), (H, H, BEDROCK - 64), 'sc_bedrock'),                             # so templates get only uniform
    box((-H + 16, -H, FLOOR + 16), (H - 16, -H + 16, BEDROCK - 64), 'sc_bedrock'),                 # _minlight and tile seamlessly
    box((-H + 16, H - 16, FLOOR + 16), (H - 16, H, BEDROCK - 64), 'sc_bedrock'),
    box((-H - 16, -H - 16, FLOOR), (H + 16, H + 16, FLOOR + 16), 'sc_bedrock'),                     # template room floor
    box((-H - 16, -H - 16, TOP), (H + 16, H + 16, CEIL), 'sky'),                                   # ceiling
    box((-H - 16, -H - 16, FLOOR), (-H, H + 16, CEIL), 'sky'),
    box((H, -H - 16, FLOOR), (H + 16, H + 16, CEIL), 'sky'),
    box((-H, -H - 16, FLOOR), (H, -H, CEIL), 'sky'),
    box((-H, H, FLOOR), (H, H + 16, CEIL), 'sky'),
]
wads = ';'.join([os.path.join(OUT, 'svencraft.wad')] + [os.path.join(GAME, w) for w in ('halflife.wad', 'Opfor.wad', 'cs_bdog.wad', 'op4ctf.wad', 'barney.wad')] + [ZHLT])
out = ent({'classname': 'worldspawn', 'mapversion': '220', 'wad': wads, 'skyname': 'grassy',
           'message': 'Svencraft Sandbox', 'maxrange': '8192'}, world)

# Angled sun plus sky light so vertical faces aren't black (brush entities don't shadow each other, so terrain
# pieces copied from the templates still tile seamlessly: each face direction gets one uniform brightness).
out += ent({'classname': 'light_environment', 'origin': '0 0 1400', 'pitch': '-60', 'angles': '0 35 0',
            '_light': '255 248 230 280', '_diffuse_light': '165 185 225 110'})
out += ent({'classname': 'info_player_start', 'origin': '0 0 48', 'angles': '0 90 0'})
for x, y in [(64, 0), (-64, 0), (0, 64), (0, -64), (96, 96), (-96, -96), (96, -96), (-96, 96)]:
    out += ent({'classname': 'info_player_deathmatch', 'origin': '%d %d 48' % (x, y), 'angles': '0 90 0'})

# Block templates: one brush entity per (material, size). The plugin copies their models ("*N") at runtime.
FACES = {'grass': ('sc_grass_top', 'sc_grass_side', 'sc_dirt'), 'log': ('sc_log_top', 'sc_log_side', 'sc_log_top'),
         'leaves': ('{sc_leaves',) * 3, 'glass': ('{sc_glass',) * 3, 'workbench': ('sc_bench_top', 'sc_bench_side', 'sc_planks'),
         'crystal_ore': ('sc_crystal',) * 3}   # texture names max 15 chars incl. the _x/_y/_b shading suffix
MATS = ['grass', 'dirt', 'stone', 'cobble', 'brick', 'planks', 'log', 'leaves', 'sand', 'gravel', 'snow', 'metal',
        'concrete', 'tile', 'rubber', 'glass', 'circuit', 'vent', 'workbench', 'iron_ore', 'crystal_ore']
SIZES = {'grass': [(8, 8, 1), (4, 4, 1), (2, 2, 1)], 'dirt': [(8, 8, 2), (4, 4, 1), (2, 2, 1)],
         'stone': [(8, 8, 8), (4, 4, 4), (2, 2, 2)],
         'sand': [(8, 8, 2), (8, 8, 1), (4, 4, 1), (2, 2, 1)], 'gravel': [(8, 8, 1), (4, 4, 1), (2, 2, 1)]}
templates = [(m, s, False) for m in MATS for s in SIZES.get(m, []) + [(1, 1, 1)]]
# Dimmer copies for underground terrain (natural stone and ores), named sctpd_*: caves and shafts read as dark
DARK = [('stone', s) for s in SIZES['stone'] + [(1, 1, 1)]] + [('iron_ore', (1, 1, 1)), ('crystal_ore', (1, 1, 1))]
templates += [(m, s, True) for m, s in DARK]
x, y, rowd = -H + 2 * BLOCK, -H + 2 * BLOCK, 0
zb = -24 * BLOCK                             # template bottoms float above the room floor (grid aligned)
for m, (sx, sy, sz), dark in templates:
    w, d = sx * BLOCK, sy * BLOCK
    if x + w > H - 2 * BLOCK:
        x, y, rowd = -H + 2 * BLOCK, y + rowd + 3 * BLOCK, 0
    mn = (x, y, zb); mx = (x + w, y + d, zb + sz * BLOCK)
    c = [(mn[i] + mx[i]) // 2 for i in range(3)]
    top, side, bot = FACES.get(m, ('sc_' + m,) * 3)
    kv = {'classname': 'func_wall', 'targetname': ('sctpd_' if dark else 'sctpl_') + '%s_%d_%d_%d' % (m, sx, sy, sz), '_minlight': '0.24' if dark else '0.72'}
    if m in ('leaves', 'glass'):
        kv.update({'rendermode': '4', 'renderamt': '255'})
    out += ent(kv, [box(mn, mx, {'top': top, 'sx': side + '_x', 'sy': side + '_y', 'bottom': bot + '_b'}),
                    box([v - 8 for v in c], [v + 8 for v in c], 'ORIGIN')])
    x += w + 3 * BLOCK; rowd = max(rowd, d)
assert y + rowd < H, 'templates overflow the template room'
import testarea
out += testarea.build(box, ent)
assert zb % BLOCK == 0 and zb > FLOOR + 16 and zb + 8 * BLOCK < BEDROCK - 64
open(os.path.join(OUT, 'svencraft_sandbox.map'), 'w', newline='\n').write(out)
print('wad: %d textures, %d bytes; map: %d templates' % (len(lumps), os.path.getsize(wadpath), len(templates)))
