# The hand: models/svencraft/v_schand.mdl, the view model when the selected hotbar slot holds no tool or weapon.
# Sven's HEV glove (its crowbar view model with the crowbar taken out, same sequences, so the swing code is shared)
# holding the slot's block (a small cube) or item (a flat icon). The client picks body and skin from the slot:
#   body 0 empty hand, 1 cube (skin = block id), 2 flat (skin = SCI_NUM_BLOCK_IDS + g_SCFlatSkin[id])
# Needs make_blockassets.py and make_items.py run first (their bitmaps are reused).
import math, os, shutil, subprocess
import numpy as np

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SDK / _BUILD / _MDLDEC)
ROOT = sc_paths.ROOT
GAME = sc_paths.GAMEDIR
WORK = sc_paths.work('hand')
DEC = os.path.join(WORK, 'dec')
SDK = sc_paths.SDK
STUDIOMDL = os.path.join(SDK, 'modelling', 'studiomdl.exe')
MDLDEC = sc_paths.MDLDEC
BLOCKBMP = sc_paths.work('blockassets')
ITEMBMP = sc_paths.work('items')
os.makedirs(DEC, exist_ok=True)

# ---------------------------------------------------------------- Sven's crowbar view model, decompiled
if not os.path.exists(os.path.join(DEC, 'Sven_Hands_right_ref.smd')):
    shutil.copy2(os.path.join(ROOT, 'run', 'svencoop', 'models', 'v_crowbar.mdl'), DEC)
    shutil.copy2(os.path.join(ROOT, 'engine', 'utils', 'mdldec', 'res', 'activities.txt'), DEC)
    subprocess.run([MDLDEC, '-m', '-u', 'v_crowbar.mdl', '.'], cwd=DEC, check=True, capture_output=True)
# the crowbar's head is what shows of it, the fist stays under the screen's edge: the empty hand is raised into view
# by moving the root bone in every animation (in the source's space: +x left, -y forward, +z up)
RAISE = (-1.0, 0.0, 10.0)
for f in os.listdir(DEC):
    if f.startswith('Sven_HEV') or f == 'Sven_Hands_right_ref.smd':
        shutil.copy2(os.path.join(DEC, f), WORK)
    elif f.endswith('.smd') and f != 'Crowbar_ref.smd':
        L = open(os.path.join(DEC, f)).read().split('\n')
        sk = L.index('skeleton')
        for k in range(sk + 1, len(L)):
            t = L[k].split()
            if L[k].strip() == 'end':
                break
            if len(t) == 7 and t[0] == '0':
                L[k] = '0 %.6f %.6f %.6f %s %s %s' % (float(t[1]) + RAISE[0], float(t[2]) + RAISE[1], float(t[3]) + RAISE[2], t[4], t[5], t[6])
        open(os.path.join(WORK, f), 'w', newline='\n').write('\n'.join(L))


def rot(rx, ry, rz):
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return Rz @ Ry @ Rx


ref = open(os.path.join(DEC, 'Crowbar_ref.smd')).read().split('\n')
head = ref[:ref.index('triangles')]          # version, nodes, reference skeleton
nodes, pose = [], {}
i = ref.index('nodes') + 1
while ref[i].strip() != 'end':
    t = ref[i].split('"')
    nodes.append((t[1], int(t[2])))
    i += 1
i = ref.index('skeleton') + 2
while ref[i].strip() != 'end':
    t = ref[i].split()
    pose[int(t[0])] = [float(x) for x in t[1:]]
    i += 1
W = {}
for b, (name, par) in enumerate(nodes):
    M = np.eye(4)
    M[:3, :3] = rot(*pose[b][3:6])
    M[:3, 3] = pose[b][:3]
    W[b] = W[par] @ M if par >= 0 else M
HAND = [n for n, _ in nodes].index('Bip01 R Hand')

# where the crowbar's shaft goes through the fist, and the shaft's direction
cv = np.array([[float(x) for x in l.split()[1:4]] for l in ref[ref.index('triangles') + 1:] if len(l.split()) >= 9])
fist = np.mean([W[b][:3, 3] for b, (n, _) in enumerate(nodes) if n in ('Bip01 R Finger1', 'Bip01 R Finger2', 'Bip01 R Finger3', 'Bip01 R Finger4')], 0)
near = cv[np.linalg.norm(cv - fist, axis=1) < 5]
grip = near.mean(0)
u, s, vt = np.linalg.svd(cv - cv.mean(0))
shaft = vt[0] / np.linalg.norm(vt[0])
if shaft[2] < 0:
    shaft = -shaft

# ---------------------------------------------------------------- held meshes (on the hand bone)
HX, HY, HZ = W[HAND][:3, 0], W[HAND][:3, 1], W[HAND][:3, 2]
CUBE = 4.5          # half size: a block about the size of the fist
FLAT = 6.5
# the block sits in the palm, pushed out of the fingers along the hand's z (the side the fingers curl to)
centre = grip + HZ * 1.5 + shaft * 1.0


def frame_axes(a, b):
    a = a / np.linalg.norm(a)
    b = b - a * (a @ b)
    b /= np.linalg.norm(b)
    return a, b, np.cross(a, b)


def turn(axes, axis, deg):
    """rotate the three axes about one of them"""
    c, s_ = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    a = list(axes)
    i, j = [k for k in range(3) if k != axis]
    a[i], a[j] = c * axes[i] + s_ * axes[j], -s_ * axes[i] + c * axes[j]
    return a


AX = frame_axes(shaft, HZ)                  # x along the shaft, y out of the fingers, z across
AX = turn(AX, 0, 45)                        # a little turn, so two faces show, like Minecraft's
AX = turn(AX, 1, 20)
T = []


def quad(tex, corners, n, uvs=((0, 0), (1, 0), (1, 1), (0, 1))):
    for tri in ((0, 1, 2), (0, 2, 3)):
        T.append(tex)
        for k in tri:
            p = corners[k]
            T.append('%d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f' % (HAND, p[0], p[1], p[2], n[0], n[1], n[2], uvs[k][0], uvs[k][1]))


def cube_smd(top, side, bottom):
    T.clear()
    ax, ay, az = AX
    c = centre
    h = CUBE
    # faces: (texture, normal axis, sign) with corners CCW from outside; texture up is +z (az) on sides
    for tex, n, u_, v_ in ((side, ax, ay, az), (side, -ax, -ay, az), (side, ay, -ax, az), (side, -ay, ax, az),
                           (top, az, ay, -ax), (bottom, -az, ay, ax)):
        o = c + n * h
        quad(tex, [o - u_ * h - v_ * h, o + u_ * h - v_ * h, o + u_ * h + v_ * h, o - u_ * h + v_ * h], n,
             ((0.004, 0.004), (0.996, 0.004), (0.996, 0.996), (0.004, 0.996)))
    return T[:]


def flat_smd(tex):
    T.clear()
    # the icon stands in the fist like the crowbar did: along the shaft, facing across the hand
    ax, ay, az = frame_axes(shaft, HZ)
    c = grip + shaft * FLAT * 0.6
    for n, sgn in ((az, 1), (-az, -1)):
        u_ = -ay * sgn
        quad(tex, [c - u_ * FLAT - ax * FLAT, c + u_ * FLAT - ax * FLAT, c + u_ * FLAT + ax * FLAT, c - u_ * FLAT + ax * FLAT], n)
    return T[:]


def write_smd(name, tris):
    open(os.path.join(WORK, name + '.smd'), 'w', newline='\n').write('\n'.join(head + ['triangles'] + tris + ['end']) + '\n')


# ---------------------------------------------------------------- skins
defs = {}
for line in open(os.path.join(GAME, 'scripts', 'blocks.txt')):
    t = line.split()
    if len(t) >= 5 and t[0].isdigit():
        defs[int(t[0])] = (t[2], t[3], t[4], 'alpha' in t[5:])
hdr = open(os.path.join(ROOT, 'game', 'common', 'sc_items.h')).read()
NBLOCK = int(hdr.split('#define SCI_NUM_BLOCK_IDS')[1].split()[0])
flat_skins = [l.split('"')[1] for l in open(os.path.join(ITEMBMP, 'itemflat.qc')) if l.strip().startswith('{ "')]
first = defs[min(defs)]
write_smd('held_cube', cube_smd(first[0] + '.bmp', first[1] + '.bmp', first[2] + '.bmp'))
write_smd('held_flat', flat_smd(flat_skins[0]))
masked = set()
for b, (t, s_, bt, alpha) in defs.items():
    for x in (t, s_, bt):
        shutil.copy2(os.path.join(BLOCKBMP, x + '.bmp'), WORK)
        if alpha:
            masked.add(x + '.bmp')
for f in flat_skins:
    shutil.copy2(os.path.join(ITEMBMP, f), WORK)
    masked.add(f)

qc = ['$modelname "v_schand.mdl"', '$cd "."', '$cdtexture "."', '$cliptotextures', '$scale 1.0',
      '$body "hands" "Sven_Hands_right_ref"', '$bodygroup "held"', '{', 'blank', 'studio "held_cube"', 'studio "held_flat"', '}']
qc += ['$texrendermode "%s" "masked"' % m for m in sorted(masked)]
qc += ['$texturegroup "skinfamilies"', '{']
for b in range(NBLOCK):
    d = defs.get(b, first)
    qc.append('\t{ "%s.bmp" "%s.bmp" "%s.bmp" "%s" }' % (d[0], d[1], d[2], flat_skins[0]))
for f in flat_skins:
    qc.append('\t{ "%s.bmp" "%s.bmp" "%s.bmp" "%s" }' % (first[0], first[1], first[2], f))
qc.append('}')
# the swing: the crowbar's big chops would sweep the raised arm across the screen, so the hand gets Minecraft's
# short jab instead (the idle pose pushed forward, down and in, and back), built on the root bone
idle = open(os.path.join(WORK, 'idle1.smd')).read().split('\n')
sk = idle.index('skeleton')
pose0 = idle[sk + 2:idle.index('time 1')] if 'time 1' in idle else None
JAB_FRAMES = 9
JAB_FLICK = 0.7
jab = idle[:sk + 1]
for fr in range(JAB_FRAMES):
    s_ = math.sin(math.pi * fr / (JAB_FRAMES - 1))
    d = (1.0 * s_, 0.0, -2.5 * s_)     # left, forward (-y), down: never forward, or the upper arm comes into view
    jab.append('time %d' % fr)
    for l in pose0:
        t = l.split()
        if t[0] == '0':
            l = '0 %.6f %.6f %.6f %s %s %s' % (float(t[1]) + d[0], float(t[2]) + d[1], float(t[3]) + d[2], t[4], t[5], t[6])
        elif int(t[0]) == HAND:
            # the wrist flicks the fist down (the hand's last rotation turns it about the forearm's sideways axis)
            l = '%s %s %s %s %s %s %.6f' % (t[0], t[1], t[2], t[3], t[4], t[5], float(t[6]) + JAB_FLICK * s_)
        jab.append(l)
jab.append('end')
open(os.path.join(WORK, 'jab.smd'), 'w', newline='\n').write('\n'.join(jab) + '\n')
# the crowbar's sequences, in its order (the tool code's animation numbers), the swings replaced by the jab
dq = open(os.path.join(DEC, 'v_crowbar.qc')).read()
for blk in dq.split('$sequence ')[1:]:
    name = blk.split('"')[1]
    fps = blk.split('fps ')[1].split()[0]
    if name.startswith('attack'):
        qc.append('$sequence "%s" "jab" fps 30' % name)
    else:
        qc.append('$sequence "%s" "%s" fps %s%s' % (name, name, fps, ' loop' if 'loop' in blk.split('}')[0] else ''))
open(os.path.join(WORK, 'v_schand.qc'), 'w', newline='\n').write('\n'.join(qc) + '\n')
if os.path.exists(os.path.join(WORK, 'v_schand.mdl')):
    os.remove(os.path.join(WORK, 'v_schand.mdl'))
r = subprocess.run([STUDIOMDL, 'v_schand.qc'], cwd=WORK, capture_output=True, text=True)
if not os.path.exists(os.path.join(WORK, 'v_schand.mdl')):
    print(r.stdout[-3000:], r.stderr[-1000:])
    raise SystemExit('studiomdl failed')
shutil.copy2(os.path.join(WORK, 'v_schand.mdl'), os.path.join(GAME, 'models', 'svencraft', 'v_schand.mdl'))
print('v_schand.mdl: %d skins (%d blocks + %d flat), grip %s shaft %s' % (NBLOCK + len(flat_skins), NBLOCK, len(flat_skins),
      np.round(grip, 1), np.round(shaft, 2)))
