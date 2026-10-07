# Writes the block model sources: one SMD per shape (40x40 face, thickness T along Z, centered on the origin)
# and block.qc with a "shape" bodygroup and one skin family per material.
#   body 0 = cube (32), 1 = slab (16), 2 = plate (8), 3 = sheet (4)
BLOCK = 40.0   # one block, in world units (2 blocks fit the 72-unit player standing)
H = BLOCK / 2
SHAPES = [('cube', BLOCK), ('slab', 16.0), ('plate', 8.0), ('sheet', 4.0)]
# material order = skin index. (top, side, bottom)
MATS = [
    ('grass', 'grass_top', 'grass_side', 'dirt'),
    ('dirt', 'dirt', 'dirt', 'dirt'),
    ('stone', 'stone', 'stone', 'stone'),
    ('cobble', 'cobble', 'cobble', 'cobble'),
    ('brick', 'brick', 'brick', 'brick'),
    ('planks', 'planks', 'planks', 'planks'),
    ('log', 'log_top', 'log_side', 'log_top'),
    ('leaves', 'leaves', 'leaves', 'leaves'),
    ('sand', 'sand', 'sand', 'sand'),
    ('gravel', 'gravel', 'gravel', 'gravel'),
    ('snow', 'snow', 'snow', 'snow'),
    ('metal', 'metal', 'metal', 'metal'),
    ('concrete', 'concrete', 'concrete', 'concrete'),
    ('tile', 'tile', 'tile', 'tile'),
    ('rubber', 'rubber', 'rubber', 'rubber'),
    ('glass', 'glass', 'glass', 'glass'),
    ('circuit', 'circuit', 'circuit', 'circuit'),
    ('vent', 'vent', 'vent', 'vent'),
    ('workbench', 'bench_top', 'bench_side', 'planks'),
    ('iron_ore', 'iron_ore', 'iron_ore', 'iron_ore'),
    ('crystal_ore', 'crystal_ore', 'crystal_ore', 'crystal_ore'),
    ('glass_pane', 'glass', 'glass', 'glass'),
    ('door', 'planks', 'planks', 'planks'),
]

def write_smd(name, T):
    t = T / 2.0
    # faces: normal, 4 corners CCW seen from outside, uv per corner (side faces use only T/32 of the texture height)
    vs = T / BLOCK
    faces = {
        'top':    ((0, 0, 1),  [(-H, -H, t), (H, -H, t), (H, H, t), (-H, H, t)], [(0, 0), (1, 0), (1, 1), (0, 1)]),
        'bottom': ((0, 0, -1), [(-H, H, -t), (H, H, -t), (H, -H, -t), (-H, -H, -t)], [(0, 0), (1, 0), (1, 1), (0, 1)]),
        'px':     ((1, 0, 0),  [(H, -H, -t), (H, H, -t), (H, H, t), (H, -H, t)], [(0, 1 - vs), (1, 1 - vs), (1, 1), (0, 1)]),
        'nx':     ((-1, 0, 0), [(-H, H, -t), (-H, -H, -t), (-H, -H, t), (-H, H, t)], [(0, 1 - vs), (1, 1 - vs), (1, 1), (0, 1)]),
        'py':     ((0, 1, 0),  [(H, H, -t), (-H, H, -t), (-H, H, t), (H, H, t)], [(0, 1 - vs), (1, 1 - vs), (1, 1), (0, 1)]),
        'ny':     ((0, -1, 0), [(-H, -H, -t), (H, -H, -t), (H, -H, t), (-H, -H, t)], [(0, 1 - vs), (1, 1 - vs), (1, 1), (0, 1)]),
    }
    ref = {'top': MATS[0][1], 'bottom': MATS[0][3]}
    lines = ['version 1', 'nodes', '0 "root" -1', 'end', 'skeleton', 'time 0', '0 0 0 0 0 0 0', 'end', 'triangles']
    for fname, (n, c, uv) in faces.items():
        tex = ref.get(fname, MATS[0][2]) + '.bmp'
        for tri in ((0, 1, 2), (0, 2, 3)):
            lines.append(tex)
            for i in tri:
                x, y, z = c[i]; u, v = uv[i]
                lines.append(f'0 {x:.6f} {y:.6f} {z:.6f} {n[0]} {n[1]} {n[2]} {u:.6f} {v:.6f}')
    lines.append('end')
    open(name + '.smd', 'w').write('\n'.join(lines) + '\n')

for name, T in SHAPES:
    write_smd(name, T)

qc = ['$modelname "block.mdl"', '$cd "."', '$cdtexture "./tex"', '$scale 1.0', '$origin 0 0 0',
      '$bbox -20 -20 -20 20 20 20', '$cbox -20 -20 -20 20 20 20',
      '$bodygroup "shape"', '{'] + ['\tstudio "%s"' % n for n, _ in SHAPES] + ['}',
      '$sequence "idle" "cube" fps 1', '',
      '$texrendermode "glass.bmp" "masked"', '$texrendermode "leaves.bmp" "masked"', '',
      '$texturegroup "skinfamilies"', '{']
for m in MATS:
    qc.append('\t{ "%s.bmp" "%s.bmp" "%s.bmp" }' % m[1:])
qc.append('}')
open('block.qc', 'w').write('\n'.join(qc) + '\n')
print('shapes:', ', '.join(f'{i}={n}({T:g})' for i, (n, T) in enumerate(SHAPES)))
