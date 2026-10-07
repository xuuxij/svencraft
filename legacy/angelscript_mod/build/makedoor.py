# Wooden door: tex/door.bmp (16x32 pixel art upscaled 4x, masked windows) and door.mdl.
#   body 0 "hinge": origin on the hinge axis at floor level, panel along +X (-3..37), thickness Y -3..3, Z 0..80
#   body 1 "item":  the same panel centred on the origin (for dropped items)
import os, shutil, subprocess
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
STUDIOMDL = r"C:\Program Files (x86)\Steam\steamapps\common\Sven Co-op SDK\modelling\studiomdl.exe"
INSTALL = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\models\svencraft"
rng = np.random.default_rng(4242)

W, H = 16, 32
BOARD = np.array((162, 130, 78.0))
DARK = np.array((118, 90, 52.0))
FRAME = np.array((132, 102, 60.0))
SEAM = np.array((104, 80, 46.0))


def door_art():
    img = np.zeros((H, W, 3))
    hole = np.zeros((H, W), bool)
    for x in range(W):                                   # vertical boards with per-board tone
        tone = rng.normal(0, 7)
        img[:, x] = BOARD + tone + rng.normal(0, 5, (H, 1))
    for x in (4, 8, 12):                                 # board seams
        img[:, x] = SEAM + rng.normal(0, 4, (H, 3))
    img[0, :] = img[H - 1, :] = DARK                     # outer frame
    img[:, 0] = img[:, W - 1] = DARK
    for y0, y1 in ((2, 12),):                            # window band: frame + two panes
        img[y0, 2:14] = FRAME; img[y1, 2:14] = FRAME
        img[y0:y1 + 1, 2] = FRAME; img[y0:y1 + 1, 13] = FRAME
        img[y0:y1 + 1, 7:9] = FRAME
        hole[y0 + 1:y1, 3:7] = True
        hole[y0 + 1:y1, 9:13] = True
    img[14:16, 1:15] = DARK                              # middle rail
    img[29:31, 1:15] = DARK                              # kick rail
    img[17:19, 12:14] = (175, 175, 178)                  # iron handle on the free edge
    img[19, 12:14] = (95, 95, 100)
    img[17:20, 11] = (95, 95, 100)
    img[hole] = (0, 0, 255)
    return np.clip(img, 0, 255), hole


def save_masked(rgb, hole, path, scale=4):
    im = Image.fromarray(rgb.astype(np.uint8), 'RGB')
    q = im.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    idx = np.array(q); idx[hole] = 255
    p = q.getpalette()[:765]; p += [0] * (765 - len(p)); p += [0, 0, 255]
    out = Image.fromarray(idx.astype(np.uint8), 'P'); out.putpalette(p)
    out = out.resize((rgb.shape[1] * scale, rgb.shape[0] * scale), Image.NEAREST)
    out.save(path)
    Image.fromarray(rgb.astype(np.uint8)).resize((rgb.shape[1] * 8, rgb.shape[0] * 8), Image.NEAREST).save(path.replace('door.bmp', '_preview_door.png'))


rgb, hole = door_art()
save_masked(rgb, hole, os.path.join('tex', 'door.bmp'))

T, PW, PH = 6.0, 40.0, 80.0
EDGE_U = 0.1   # edges sample a thin strip of the frame


def panel_smd(path, ox, oy, oz):
    x0, x1, y0, y1, z0, z1 = -T / 2 + ox, PW - T / 2 + ox, -T / 2 + oy, T / 2 + oy, oz, PH + oz
    def u(x): return (x - x0) / (x1 - x0)
    def v(z): return (z - z0) / (z1 - z0)
    faces = [
        # normal, corners (CCW from outside), uvs
        ((0, 1, 0), [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)]),
        ((0, -1, 0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
        ((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
        ((-1, 0, 0), [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)]),
        ((0, 0, 1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
        ((0, 0, -1), [(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)]),
    ]
    lines = ['version 1', 'nodes', '0 "root" -1', 'end', 'skeleton', 'time 0', '0 0 0 0 0 0 0', 'end', 'triangles']
    for n, c in faces:
        if n[1] != 0:
            uv = [(u(p[0]), v(p[2])) for p in c]
        elif n[0] != 0:
            uv = [(EDGE_U * (p[1] - y0) / T, v(p[2])) for p in c]
        else:
            uv = [(u(p[0]), 1.0 - 0.02 * (p[1] - y0) / T) if n[2] > 0 else (u(p[0]), 0.02 * (p[1] - y0) / T) for p in c]
        for tri in ((0, 1, 2), (0, 2, 3)):
            lines.append('door.bmp')
            for i in tri:
                x, y, z = c[i]
                lines.append(f'0 {x:.4f} {y:.4f} {z:.4f} {n[0]} {n[1]} {n[2]} {uv[i][0]:.5f} {uv[i][1]:.5f}')
    lines.append('end')
    open(path, 'w').write('\n'.join(lines) + '\n')


panel_smd('door_hinge.smd', 0, 0, 0)
panel_smd('door_item.smd', -(PW - T) / 2, 0, -PH / 2)
qc = ['$modelname "door.mdl"', '$cd "."', '$cdtexture "./tex"', '$scale 1.0', '$origin 0 0 0 270',   # studiomdl adds 90 degrees of yaw; cancel it so local X stays world X
      '$bbox -3 -3 0 37 3 80', '$cbox -3 -3 0 37 3 80',
      '$bodygroup "mode"', '{', '\tstudio "door_hinge"', '\tstudio "door_item"', '}',
      '$texrendermode "door.bmp" "masked"',
      '$sequence "idle" "door_hinge" fps 1']
open('door.qc', 'w').write('\n'.join(qc) + '\n')
r = subprocess.run([STUDIOMDL, 'door.qc'], cwd=HERE, capture_output=True, text=True)
bad = [l for l in r.stdout.splitlines() if 'error' in l.lower() or 'warn' in l.lower()]
print('\n'.join(bad) if bad else 'studiomdl ok')
assert os.path.exists('door.mdl'), r.stdout[-2000:]
shutil.copy2('door.mdl', os.path.join(INSTALL, 'door.mdl'))
print('door.mdl', os.path.getsize('door.mdl'))
