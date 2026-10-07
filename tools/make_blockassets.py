# Block assets for the HUD and drops, built from gfx/blocks/*.tga and scripts/blocks.txt:
#   models/svencraft/blockitem.mdl   cube with one skin per block id (dropped items, held blocks)
#   sprites/svencraft/hotbar.spr     frame id = slot with block id's icon (0 = empty), frame N + id = selected slot
import os, shutil, subprocess
import numpy as np
from PIL import Image

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SDK / _BUILD)
GAME = sc_paths.GAMEDIR
WORK = sc_paths.work('blockassets')
SDK = sc_paths.SDK
STUDIOMDL = os.path.join(SDK, 'modelling', 'studiomdl.exe')
SPRGEN = os.path.join(SDK, 'sprites', 'sprgen.exe')
os.makedirs(WORK, exist_ok=True)

defs = {}
for line in open(os.path.join(GAME, 'scripts', 'blocks.txt')):
    t = line.split()
    if len(t) >= 5 and t[0].isdigit():
        defs[int(t[0])] = (t[1], t[2], t[3], t[4], 'alpha' in t[5:])
N = max(defs) + 1
ALPHA = {d[1] for d in defs.values() if d[4]} | {d[2] for d in defs.values() if d[4]}


def tex_rgba(name):
    return np.array(Image.open(os.path.join(GAME, 'gfx', 'blocks', name + '.tga')).convert('RGBA'))


# ---------------------------------------------------------------- model
def save_bmp(name):
    rgba = tex_rgba(name)
    rgb = Image.fromarray(rgba[..., :3], 'RGB')
    q = rgb.quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    idx = np.array(q)
    if name in ALPHA:
        idx[rgba[..., 3] < 128] = 255
    pal = q.getpalette()[:765] + [0, 0, 255]
    out = Image.fromarray(idx.astype(np.uint8), 'P'); out.putpalette(pal)
    out.save(os.path.join(WORK, name + '.bmp'))


H = 20.0
faces = {   # name: normal, corners (CCW from outside), uv
    'top': ((0, 0, 1), [(-H, -H, H), (H, -H, H), (H, H, H), (-H, H, H)]),
    'bottom': ((0, 0, -1), [(-H, H, -H), (H, H, -H), (H, -H, -H), (-H, -H, -H)]),
    'px': ((1, 0, 0), [(H, -H, -H), (H, H, -H), (H, H, H), (H, -H, H)]),
    'nx': ((-1, 0, 0), [(-H, H, -H), (-H, -H, -H), (-H, -H, H), (-H, H, H)]),
    'py': ((0, 1, 0), [(H, H, -H), (-H, H, -H), (-H, H, H), (H, H, H)]),
    'ny': ((0, -1, 0), [(-H, -H, -H), (H, -H, -H), (H, -H, H), (-H, -H, H)]),
}
UV = [(0, 0), (1, 0), (1, 1), (0, 1)]
first = defs[min(defs)]
ref = {'top': first[1], 'bottom': first[3]}
lines = ['version 1', 'nodes', '0 "root" -1', 'end', 'skeleton', 'time 0', '0 0 0 0 0 0 0', 'end', 'triangles']
for fname, (n, c) in faces.items():
    tex = ref.get(fname, first[2]) + '.bmp'
    for tri in ((0, 1, 2), (0, 2, 3)):
        lines.append(tex)
        for i in tri:
            x, y, z = c[i]; u, v = UV[i]
            lines.append(f'0 {x:.3f} {y:.3f} {z:.3f} {n[0]} {n[1]} {n[2]} {u} {v}')
lines.append('end')
open(os.path.join(WORK, 'blockitem.smd'), 'w').write('\n'.join(lines) + '\n')

texnames = sorted({t for d in defs.values() for t in d[1:4]})
for t in texnames:
    save_bmp(t)
qc = ['$modelname "blockitem.mdl"', '$cd "."', '$cdtexture "."', '$scale 1.0', '$origin 0 0 0 270',
      '$bbox -20 -20 -20 20 20 20', '$body "body" "blockitem"', '$sequence "idle" "blockitem" fps 1']
qc += ['$texrendermode "%s.bmp" "masked"' % t for t in sorted(ALPHA)]
qc += ['$texturegroup "skinfamilies"', '{']
for i in range(N):
    d = defs.get(i, first)
    qc.append('\t{ "%s.bmp" "%s.bmp" "%s.bmp" }' % (d[1], d[2], d[3]))
qc.append('}')
open(os.path.join(WORK, 'blockitem.qc'), 'w').write('\n'.join(qc) + '\n')
r = subprocess.run([STUDIOMDL, 'blockitem.qc'], cwd=WORK, capture_output=True, text=True)
assert os.path.exists(os.path.join(WORK, 'blockitem.mdl')), r.stdout[-2000:]
os.makedirs(os.path.join(GAME, 'models', 'svencraft'), exist_ok=True)
shutil.copy2(os.path.join(WORK, 'blockitem.mdl'), os.path.join(GAME, 'models', 'svencraft', 'blockitem.mdl'))
print('blockitem.mdl', N, 'skins')

# ---------------------------------------------------------------- hotbar sprite (isometric cube icons in slots)
FS, S = 48, 56
CX, HW, HT, SH, TOP = 24.0, 22.0, 11.0, 24.0, 1.0
SHADE = {'top': 1.0, 'left': 0.8, 'right': 0.6}


def render_cube(top, side):
    trgb, srgb = tex_rgba(top), tex_rgba(side)
    out = np.zeros((FS, FS, 3), np.float32); op = np.zeros((FS, FS), bool)
    ys, xs = np.mgrid[0:FS, 0:FS]; sx, sy = xs + 0.5, ys + 0.5
    for face in ('top', 'left', 'right'):
        if face == 'top':
            a = (sx - CX) / HW; b = (sy - TOP) / HT
            u, v = (a + b) / 2, (b - a) / 2
            src = trgb
        elif face == 'left':
            u = (sx - (CX - HW)) / HW; v = (sy - TOP - HT - u * HT) / SH; src = srgb
        else:
            z = ((CX + HW) - sx) / HW; v = (sy - TOP - HT - z * HT) / SH; u = 1 - z; src = srgb
        inside = (u >= 0) & (u <= 1) & (v >= 0) & (v <= 1)
        tu = np.clip((u * 64).astype(int), 0, 63) % src.shape[1]; tv = np.clip((v * 64).astype(int), 0, 63) % src.shape[0]
        px = src[tv, tu]
        m = inside & (px[..., 3] > 128)
        out[m] = px[..., :3][m] * SHADE[face]
        op[m] = True
    return out, op


def slot(selected):
    rgb = np.zeros((S, S, 3), np.float32)
    if selected:
        rgb[:] = (20, 20, 20); rgb[1:-1, 1:-1] = (235, 235, 235); rgb[4:-4, 4:-4] = (88, 88, 88)
    else:
        rgb[:] = (24, 24, 24); rgb[1:-1, 1:-1] = (120, 120, 120)
        rgb[1:3, 1:-1] = (150, 150, 150); rgb[1:-1, 1:3] = (150, 150, 150); rgb[3:-3, 3:-3] = (58, 58, 58)
    return rgb


frames = []
for sel in (False, True):
    for i in range(N):
        rgb = slot(sel)
        if i in defs and i > 0:
            ic, op = render_cube(defs[i][1], defs[i][2])
            o = (S - FS) // 2
            sub = rgb[o:o + FS, o:o + FS]; sub[op] = ic[op]
        frames.append(rgb)

atlas = np.concatenate(frames, 0).astype(np.uint8)
q = Image.fromarray(atlas, 'RGB').quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
idx = np.array(q); pal = q.getpalette()[:765] + [0, 0, 255]
PER = 16
qcl = ['$spritename hotbar', '$type vp_parallel', '$texture alphatest']
for si in range(0, len(frames), PER):
    chunk = idx[si * S:(si + PER) * S]
    n = chunk.shape[0] // S
    sheet = np.full((4 * S, 4 * S), 255, np.uint8)
    for k in range(n):
        r, c = divmod(k, 4)
        sheet[r * S:(r + 1) * S, c * S:(c + 1) * S] = chunk[k * S:(k + 1) * S]
    im = Image.fromarray(sheet, 'P'); im.putpalette(pal)
    name = 'hotbar_%d.bmp' % (si // PER)
    im.save(os.path.join(WORK, name))
    qcl.append('$load ' + name)
    for k in range(n):
        r, c = divmod(k, 4)
        qcl.append('$frame %d %d %d %d' % (c * S, r * S, S, S))
open(os.path.join(WORK, 'hotbar.qc'), 'w', newline='\n').write('\n'.join(qcl) + '\n')
r = subprocess.run([SPRGEN, 'hotbar.qc'], cwd=WORK, capture_output=True, text=True)
assert os.path.exists(os.path.join(WORK, 'hotbar.spr')), r.stdout + r.stderr
shutil.copy2(os.path.join(WORK, 'hotbar.spr'), os.path.join(GAME, 'sprites', 'svencraft', 'hotbar.spr'))
print('hotbar.spr', len(frames), 'frames (selected offset %d)' % N)
