# The creeper: models/svencraft/creeper.mdl, built from code (original pixel art, compiled with the SDK's studiomdl)
#   bones: root, body, head, four legs; sequences: idle, walk, run, fuse, die
#   one block = 40 units = 16 texture pixels, so a pixel is 2.5 units: 26 px tall = 65 units
import math, os, random, shutil, subprocess
import numpy as np
from PIL import Image

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SDK / _BUILD)
GAME = sc_paths.GAMEDIR
WORK = sc_paths.work('creeper')
SDK = sc_paths.SDK
STUDIOMDL = os.path.join(SDK, 'modelling', 'studiomdl.exe')
os.makedirs(WORK, exist_ok=True)

PX = 2.5        # units per texture pixel
UP = 8          # texture upscale (crisp pixels under bilinear filtering)
AW, AH = 48, 32  # atlas size in pixels

# ---------------------------------------------------------------- texture
rng = random.Random(1717)
SHADES = [(74, 152, 58), (88, 170, 70), (62, 132, 50), (104, 186, 84), (52, 112, 44), (138, 204, 116), (70, 126, 62)]
WEIGHTS = [30, 22, 18, 10, 8, 6, 6]
EYE, MOUTH = (14, 20, 14), (34, 48, 32)

img = np.zeros((AH, AW, 3), np.uint8)


def mottle(x0, y0, w, h, darken=0.0):
    for y in range(h):
        for x in range(w):
            c = rng.choices(SHADES, WEIGHTS)[0]
            f = 1.0 - darken * (y / max(h - 1, 1))      # legs get darker towards the feet
            img[y0 + y, x0 + x] = [int(v * f) for v in c]


FACE = ["........",
        "........",
        ".EE..EE.",
        ".EE..EE.",
        "...MM...",
        "..MMMM..",
        "..MMMM..",
        "..M..M.."]
REG = {}        # region name -> (x0, y0, w, h) in atlas pixels


def region(name, x0, y0, w, h, darken=0.0):
    REG[name] = (x0, y0, w, h)
    mottle(x0, y0, w, h, darken)


for i, f in enumerate(['front', 'back', 'left', 'right', 'top', 'bottom']):
    region('head_' + f, 8 * i, 0, 8, 8)
for y, row in enumerate(FACE):
    for x, ch in enumerate(row):
        if ch == 'E':
            img[y, x] = EYE
        elif ch == 'M':
            img[y, x] = MOUTH
# a little highlight in each eye so it doesn't read as a hole
img[2, 2] = img[2, 6] = (40, 58, 40)
region('body_front', 0, 8, 8, 12)
region('body_back', 8, 8, 8, 12)
region('body_left', 16, 8, 4, 12)
region('body_right', 20, 8, 4, 12)
region('body_top', 24, 8, 8, 4)
region('body_bottom', 32, 8, 8, 4)
for i, f in enumerate(['front', 'back', 'left', 'right']):
    region('leg_' + f, 4 * i, 20, 4, 6, darken=0.25)
region('leg_bottom', 16, 20, 4, 4, darken=0.0)
region('leg_top', 20, 20, 4, 4)
# feet: a darker sole row on each side
img[25, 0:16] = (img[25, 0:16] * 0.7).astype(np.uint8)

def save(name, rgb):
    big = Image.fromarray(rgb.astype(np.uint8), 'RGB').resize((AW * UP, AH * UP), Image.NEAREST)
    big.quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(os.path.join(WORK, name + '.bmp'))
    return big


save('creeper', img).save(os.path.join(WORK, 'creeper_preview.png'))
# skins: 1 = the white flash while it hisses, 2 = the red flash when it gets hurt
save('creeper_white', img * 0.25 + 255 * 0.75)
save('creeper_hurt', img * np.array([0.45, 0.25, 0.25]) + np.array([150, 20, 20]))

# ---------------------------------------------------------------- geometry
BONES = [('root', -1, (0, 0, 0)), ('body', 0, (0, 0, 15)), ('head', 1, (0, 0, 30)),
         ('leg_fl', 0, (10, 5, 15)), ('leg_fr', 0, (10, -5, 15)), ('leg_bl', 0, (-10, 5, 15)), ('leg_br', 0, (-10, -5, 15))]
BI = {b[0]: i for i, b in enumerate(BONES)}


def uv(reg, fu, fv):
    """atlas region + fractions (u right, v up) -> smd uv (v up from the bottom of the image)"""
    x0, y0, w, h = REG[reg]
    inset = 0.5 / UP
    u = (x0 + inset + fu * (w - 2 * inset)) / AW
    v = 1.0 - (y0 + inset + (1 - fv) * (h - 2 * inset)) / AH
    return u, v


tris = []


def box(bone, mn, mx, part):
    x0, y0, z0 = mn
    x1, y1, z1 = mx
    # corners bottom-left, bottom-right, top-right, top-left as seen from outside; texture up = +z (top: +x)
    faces = {
        'front': ((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
        'back': ((-1, 0, 0), [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)]),
        'left': ((0, 1, 0), [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)]),
        'right': ((0, -1, 0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
        'top': ((0, 0, 1), [(x0, y1, z1), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1)]),
        'bottom': ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)]),
    }
    FR = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for f, (n, c) in faces.items():
        reg = part + '_' + f
        for t in ((0, 1, 2), (0, 2, 3)):
            tris.append(('creeper.bmp', [(BI[bone], c[i], n, uv(reg, *FR[i])) for i in t]))


box('body', (-5, -10, 15), (5, 10, 45), 'body')
box('head', (-10, -10, 45), (10, 10, 65), 'head')
for leg, (lx, ly, lz) in [(b[0], b[2]) for b in BONES if b[0].startswith('leg')]:
    box(leg, (lx - 5, ly - 5, 0), (lx + 5, ly + 5, 15), 'leg')


def nodes():
    return ['nodes'] + ['%d "%s" %d' % (i, b[0], b[1]) for i, b in enumerate(BONES)] + ['end']


def frame(t, pose):
    out = ['time %d' % t]
    for i, (name, parent, pos) in enumerate(BONES):
        p = list(pos)
        r = [0.0, 0.0, 0.0]
        if name in pose:
            dp, dr = pose[name]
            p = [p[k] + dp[k] for k in range(3)]
            r = [r[k] + dr[k] for k in range(3)]
        out.append('%d %.4f %.4f %.4f %.5f %.5f %.5f' % (i, p[0], p[1], p[2], r[0], r[1], r[2]))
    return out


ref = ['version 1'] + nodes() + ['skeleton'] + frame(0, {}) + ['end', 'triangles']
for tex, vs in tris:
    ref.append(tex)
    for b, p, n, (u, v) in vs:
        ref.append('%d %.3f %.3f %.3f %d %d %d %.5f %.5f' % (b, p[0], p[1], p[2], n[0], n[1], n[2], u, v))
ref.append('end')
open(os.path.join(WORK, 'creeper_ref.smd'), 'w').write('\n'.join(ref) + '\n')


def anim(name, nframes, posefn):
    lines = ['version 1'] + nodes() + ['skeleton']
    for t in range(nframes):
        lines += frame(t, posefn(t / nframes))
    lines.append('end')
    open(os.path.join(WORK, name + '.smd'), 'w').write('\n'.join(lines) + '\n')


Z = (0, 0, 0)


def idle(f):
    s = math.sin(2 * math.pi * f)
    return {'head': (Z, (0, 0, 0.07 * s)), 'body': ((0, 0, 0.15 * math.sin(4 * math.pi * f)), Z)}


def walk(f):
    s = 0.55 * math.sin(2 * math.pi * f)
    return {'leg_fl': (Z, (0, s, 0)), 'leg_br': (Z, (0, s, 0)), 'leg_fr': (Z, (0, -s, 0)), 'leg_bl': (Z, (0, -s, 0)),
            'body': ((0, 0, 0.5 * abs(math.cos(2 * math.pi * f))), Z), 'head': (Z, (0, 0, 0.04 * math.sin(2 * math.pi * f)))}


def fuse(f):
    j = 0.6 * math.sin(2 * math.pi * f * 3)
    return {'body': ((0, 0.3 * j, 0), Z), 'head': (Z, (0.03 * j, 0, 0))}


def die(f):
    e = 1 - (1 - min(f * 1.25, 1.0)) ** 2        # eases out, then lies still
    return {'root': ((0, 0, 10 * e), (math.pi / 2 * e, 0, 0))}


anim('creeper_idle', 40, idle)
anim('creeper_walk', 20, walk)
anim('creeper_fuse', 12, fuse)
anim('creeper_die', 20, die)

qc = ['$modelname "creeper.mdl"', '$cd "."', '$cdtexture "."', '$scale 1.0', '$origin 0 0 0 270',
      '$bbox -12 -12 0 12 12 64', '$cbox -12 -12 0 12 12 64', '$eyeposition 0 0 58',
      '$body "body" "creeper_ref"',
      '$texturegroup skinfamilies', '{', '{ "creeper.bmp" }', '{ "creeper_white.bmp" }', '{ "creeper_hurt.bmp" }', '}',
      '$texrendermode "creeper_white.bmp" "fullbright"', '$texrendermode "creeper_hurt.bmp" "fullbright"',
      '$controller 0 "head" ZR -80 80',
      '$sequence "idle" "creeper_idle" loop fps 20 ACT_IDLE 1',
      '$sequence "walk" "creeper_walk" loop fps 22 ACT_WALK 1',
      '$sequence "run" "creeper_walk" loop fps 32 ACT_RUN 1',
      '$sequence "fuse" "creeper_fuse" loop fps 20',
      '$sequence "die" "creeper_die" fps 20 ACT_DIESIMPLE 1']
open(os.path.join(WORK, 'creeper.qc'), 'w').write('\n'.join(qc) + '\n')
r = subprocess.run([STUDIOMDL, 'creeper.qc'], cwd=WORK, capture_output=True, text=True)
if r.returncode != 0 or not os.path.exists(os.path.join(WORK, 'creeper.mdl')):
    print(r.stdout[-2000:], r.stderr[-1000:])
    raise SystemExit('studiomdl failed')
os.makedirs(os.path.join(GAME, 'models', 'svencraft'), exist_ok=True)
shutil.copy2(os.path.join(WORK, 'creeper.mdl'), os.path.join(GAME, 'models', 'svencraft', 'creeper.mdl'))
print('creeper.mdl', os.path.getsize(os.path.join(WORK, 'creeper.mdl')), 'bytes,', len(tris), 'triangles')
