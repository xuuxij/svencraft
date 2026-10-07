# Blocky car prop (models/svencraft/car.mdl): painted body (3 colour skins), glass cabin, rubber tyres, lights, grille.
# +X is forward. Built from boxes; every face maps the full 0..1 texture.
import os, subprocess
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'car'); os.makedirs(OUT, exist_ok=True)
STUDIOMDL = r"C:\Program Files (x86)\Steam\steamapps\common\Sven Co-op SDK\modelling\studiomdl.exe"
rng = np.random.default_rng(42)
S = 16

def save(name, img16):
    rgb = Image.fromarray(np.clip(img16, 0, 255).astype(np.uint8), 'RGB')
    rgb.quantize(colors=256, method=Image.Quantize.MEDIANCUT).resize((64, 64), Image.NEAREST).save(os.path.join(OUT, name + '.bmp'))

def paint(col):
    img = np.zeros((S, S, 3)) + np.array(col, float) + rng.normal(0, 5, (S, S, 1))
    img[1, :] += 35; img[:, 1] += 20          # highlight edge
    img[S - 1, :] *= 0.7; img[:, S - 1] *= 0.8
    return img

save('paint_red', paint((175, 35, 30)))
save('paint_blue', paint((35, 70, 170)))
save('paint_yellow', paint((215, 175, 35)))
win = np.zeros((S, S, 3)); win[:] = (40, 70, 95)
for i in range(S):
    win[i, max(0, i - 6):i - 2] = (120, 160, 190)   # diagonal glare
win[0, :] = win[S - 1, :] = win[:, 0] = win[:, S - 1] = (25, 25, 28)
save('window', win)
tire = np.zeros((S, S, 3)) + 32 + rng.normal(0, 3, (S, S, 1))
for y in range(0, S, 3): tire[y, :] = (20, 20, 22)
tire[5:11, 5:11] = (150, 150, 155)                 # hub cap
save('tire', tire)
light = np.zeros((S, S, 3)); light[:] = (250, 245, 200); light[0, :] = light[:, 0] = (200, 190, 140)
save('headlight', light)
tail = np.zeros((S, S, 3)); tail[:] = (200, 20, 20); tail[0, :] = (120, 10, 10)
save('taillight', tail)
grille = np.zeros((S, S, 3)); grille[:] = (60, 60, 64)
for y in range(1, S, 3): grille[y, 1:S - 1] = (20, 20, 22)
save('grille', grille)

FACES = {  # normal -> corner order (CCW from outside) as functions of (mn, mx)
    (0, 0, 1):  lambda a, b: [(a[0], a[1], b[2]), (b[0], a[1], b[2]), (b[0], b[1], b[2]), (a[0], b[1], b[2])],
    (0, 0, -1): lambda a, b: [(a[0], b[1], a[2]), (b[0], b[1], a[2]), (b[0], a[1], a[2]), (a[0], a[1], a[2])],
    (1, 0, 0):  lambda a, b: [(b[0], a[1], a[2]), (b[0], b[1], a[2]), (b[0], b[1], b[2]), (b[0], a[1], b[2])],
    (-1, 0, 0): lambda a, b: [(a[0], b[1], a[2]), (a[0], a[1], a[2]), (a[0], a[1], b[2]), (a[0], b[1], b[2])],
    (0, 1, 0):  lambda a, b: [(b[0], b[1], a[2]), (a[0], b[1], a[2]), (a[0], b[1], b[2]), (b[0], b[1], b[2])],
    (0, -1, 0): lambda a, b: [(a[0], a[1], a[2]), (b[0], a[1], a[2]), (b[0], a[1], b[2]), (a[0], a[1], b[2])],
}
UV = [(0, 0), (1, 0), (1, 1), (0, 1)]
tris = []

def box(mn, mx, tex):
    """tex: texture for all faces, or dict normal->texture with key 'default'."""
    for n, corners in FACES.items():
        t = tex if isinstance(tex, str) else tex.get(n, tex['default'])
        c = corners(mn, mx)
        for tri in ((0, 1, 2), (0, 2, 3)):
            tris.append((t, [(c[i], n, UV[i]) for i in tri]))

box((-80, -36, 14), (80, 36, 42), {'default': 'paint_red', (1, 0, 0): 'grille'})       # body
box((-44, -32, 42), (28, 32, 68), {'default': 'window', (0, 0, 1): 'paint_red'})      # cabin
for x in (-50, 50):
    for y0, y1 in ((36, 44), (-44, -36)):
        box((x - 12, y0, 0), (x + 12, y1, 24), 'tire')                                  # wheels
for y0, y1 in ((18, 30), (-30, -18)):
    box((80, y0, 30), (82, y1, 38), 'headlight')
    box((-82, y0, 30), (-80, y1, 38), 'taillight')

lines = ['version 1', 'nodes', '0 "root" -1', 'end', 'skeleton', 'time 0', '0 0 0 0 0 0 0', 'end', 'triangles']
for t, verts in tris:
    lines.append(t + '.bmp')
    for (x, y, z), n, (u, v) in verts:
        lines.append(f'0 {x:.3f} {y:.3f} {z:.3f} {n[0]} {n[1]} {n[2]} {u} {v}')
lines.append('end')
open(os.path.join(OUT, 'car.smd'), 'w').write('\n'.join(lines) + '\n')

qc = ['$modelname "car.mdl"', '$cd "."', '$cdtexture "."', '$scale 1.0', '$origin 0 0 0',
      '$bbox -82 -44 0 82 44 68', '$cbox -82 -44 0 82 44 68',
      '$body "body" "car"', '$sequence "idle" "car" fps 1', '',
      '$texturegroup "paint"', '{', '\t{ "paint_red.bmp" }', '\t{ "paint_blue.bmp" }', '\t{ "paint_yellow.bmp" }', '}']
open(os.path.join(OUT, 'car.qc'), 'w').write('\n'.join(qc) + '\n')
r = subprocess.run([STUDIOMDL, 'car.qc'], cwd=OUT, capture_output=True, text=True)
print('\n'.join(l for l in r.stdout.splitlines() if any(k in l.lower() for k in ('error', 'warn', 'total', 'vertices'))))
print('car.mdl', os.path.getsize(os.path.join(OUT, 'car.mdl')) if os.path.exists(os.path.join(OUT, 'car.mdl')) else 'MISSING')
