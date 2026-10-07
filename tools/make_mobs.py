# The block world's mobs besides the creeper (tools/make_creeper.py): built from code with original pixel art and
# compiled with the SDK's studiomdl into models/svencraft/:
#   zombie.mdl     a biped: green skin, a ragged teal shirt, blue trousers; arms held out in front
#   skeleton.mdl   a thin biped of bones with a bow in its right hand
#   spider.mdl     a wide, low body on eight legs, its eight eyes glowing red (a fullbright texture)
#   arrow.mdl      the skeleton's arrow
# Minecraft's proportions (a biped is 32 texture pixels tall); a pixel here is 2.25 units for the bipeds (72 tall,
# the player's height) and 2.5 for the spider (the creeper's scale). Skins: 0 normal, 1 the red flash when hurt.
# Sequences: idle, walk, run, attack (zombie, spider), aim (skeleton), die.
import math, os, random, shutil, subprocess
import numpy as np
from PIL import Image

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SDK / _BUILD)
GAME = sc_paths.GAMEDIR
WORK = sc_paths.work('mobs')
STUDIOMDL = os.path.join(sc_paths.SDK, 'modelling', 'studiomdl.exe')
os.makedirs(WORK, exist_ok=True)
UP = 8          # texture upscale (crisp pixels under bilinear filtering)
FACES = ('front', 'back', 'left', 'right', 'top', 'bottom')
Z = (0, 0, 0)


def shade(c, f):
    return tuple(int(max(0, min(255, v * f))) for v in c)


class Mob:
    """one model: bones, boxes (in texture pixels), an atlas packed from the boxes' faces, and its animations"""

    def __init__(self, name, px, aw=64, ah=64, seed=1717):
        self.name, self.px, self.aw, self.ah = name, px, aw, ah
        self.img = np.zeros((ah, aw, 3), np.uint8)
        self.rng = random.Random(seed)
        self.bones, self.bi = [], {}
        self.tris = []
        self.reg = {}
        self.cur = [0, 0, 0]      # shelf packer: x, y, row height
        self.extra = []           # (texture name, rgb) more textures (fullbright eyes)

    # ---- skeleton
    def bone(self, name, parent, pos):
        """pos: the pivot, in pixels, relative to the parent's pivot"""
        self.bi[name] = len(self.bones)
        self.bones.append((name, -1 if parent is None else self.bi[parent], tuple(p * self.px for p in pos)))

    def pivot(self, name):
        """a bone's pivot in model space (pixels)"""
        i = self.bi[name]
        p = [0.0, 0.0, 0.0]
        while i >= 0:
            n, par, pos = self.bones[i]
            p = [p[k] + pos[k] / self.px for k in range(3)]
            i = par
        return p

    # ---- texture
    def alloc(self, w, h):
        x, y, rh = self.cur
        if x + w > self.aw:
            x, y, rh = 0, y + rh, 0
        assert y + h <= self.ah, (self.name, 'atlas full')
        self.cur = [x + w, y, max(rh, h)]
        return x, y

    def paint(self, x0, y0, w, h, pal, weights=None, grad=0.0):
        for y in range(h):
            for x in range(w):
                c = self.rng.choices(pal, weights)[0] if weights else self.rng.choice(pal)
                self.img[y0 + y, x0 + x] = shade(c, 1.0 - grad * (y / max(h - 1, 1)))

    def box(self, bone, mn, mx, part, painter, tex=None):
        """a box from mn to mx (model pixels) on `bone`; painter(face, x0, y0, w, h) fills each face's region"""
        x0, y0, z0 = mn
        x1, y1, z1 = mx
        dx, dy, dz = x1 - x0, y1 - y0, z1 - z0
        size = {'front': (dy, dz), 'back': (dy, dz), 'left': (dx, dz), 'right': (dx, dz), 'top': (dy, dx), 'bottom': (dy, dx)}
        for f in FACES:
            w, h = max(1, int(round(size[f][0]))), max(1, int(round(size[f][1])))
            ax, ay = self.alloc(w, h)
            self.reg[part + '_' + f] = (ax, ay, w, h)
            painter(f, ax, ay, w, h)
        s = self.px
        X0, Y0, Z0, X1, Y1, Z1 = x0 * s, y0 * s, z0 * s, x1 * s, y1 * s, z1 * s
        corners = {
            'front': ((1, 0, 0), [(X1, Y0, Z0), (X1, Y1, Z0), (X1, Y1, Z1), (X1, Y0, Z1)]),
            'back': ((-1, 0, 0), [(X0, Y1, Z0), (X0, Y0, Z0), (X0, Y0, Z1), (X0, Y1, Z1)]),
            'left': ((0, 1, 0), [(X1, Y1, Z0), (X0, Y1, Z0), (X0, Y1, Z1), (X1, Y1, Z1)]),
            'right': ((0, -1, 0), [(X0, Y0, Z0), (X1, Y0, Z0), (X1, Y0, Z1), (X0, Y0, Z1)]),
            'top': ((0, 0, 1), [(X0, Y1, Z1), (X0, Y0, Z1), (X1, Y0, Z1), (X1, Y1, Z1)]),
            'bottom': ((0, 0, -1), [(X0, Y0, Z0), (X0, Y1, Z0), (X1, Y1, Z0), (X1, Y0, Z0)]),
        }
        FR = [(0, 0), (1, 0), (1, 1), (0, 1)]
        for f, (n, c) in corners.items():
            reg = self.reg[part + '_' + f]
            for t in ((0, 1, 2), (0, 2, 3)):
                self.tris.append((tex or self.name + '.bmp', [(self.bi[bone], c[i], n, self.uv(reg, *FR[i])) for i in t]))

    def quad(self, bone, corners, normal, tex, aw, ah):
        """a flat quad using a whole small texture (aw x ah)"""
        FR = [(0, 0), (1, 0), (1, 1), (0, 1)]
        c = [tuple(v * self.px for v in p) for p in corners]
        for t in ((0, 1, 2), (0, 2, 3)):
            self.tris.append((tex, [(self.bi[bone], c[i], normal, FR[i]) for i in t]))

    def uv(self, reg, fu, fv):
        x0, y0, w, h = reg
        inset = 0.5 / UP
        u = (x0 + inset + fu * (w - 2 * inset)) / self.aw
        v = 1.0 - (y0 + inset + (1 - fv) * (h - 2 * inset)) / self.ah
        return u, v

    # ---- files
    def save_textures(self):
        def save(fname, rgb, w, h):
            big = Image.fromarray(rgb.astype(np.uint8), 'RGB').resize((w * UP, h * UP), Image.NEAREST)
            big.quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(os.path.join(WORK, fname))
            return big
        save(self.name + '.bmp', self.img, self.aw, self.ah).save(os.path.join(WORK, self.name + '_preview.png'))
        hurt = self.img * np.array([0.45, 0.25, 0.25]) + np.array([150, 20, 20])
        save(self.name + '_hurt.bmp', hurt, self.aw, self.ah)
        for fname, rgb in self.extra:
            save(fname, rgb, rgb.shape[1], rgb.shape[0])

    def nodes(self):
        return ['nodes'] + ['%d "%s" %d' % (i, b[0], b[1]) for i, b in enumerate(self.bones)] + ['end']

    def frame(self, t, pose):
        out = ['time %d' % t]
        for i, (name, parent, pos) in enumerate(self.bones):
            p, r = list(pos), [0.0, 0.0, 0.0]
            if name in pose:
                dp, dr = pose[name]
                p = [p[k] + dp[k] for k in range(3)]
                r = [r[k] + dr[k] for k in range(3)]
            out.append('%d %.4f %.4f %.4f %.5f %.5f %.5f' % (i, p[0], p[1], p[2], r[0], r[1], r[2]))
        return out

    def write_ref(self, rest=None):
        ref = ['version 1'] + self.nodes() + ['skeleton'] + self.frame(0, rest or {}) + ['end', 'triangles']
        for tex, vs in self.tris:
            ref.append(tex)
            for b, p, n, (u, v) in vs:
                ref.append('%d %.3f %.3f %.3f %d %d %d %.5f %.5f' % (b, p[0], p[1], p[2], n[0], n[1], n[2], u, v))
        ref.append('end')
        open(os.path.join(WORK, self.name + '_ref.smd'), 'w').write('\n'.join(ref) + '\n')

    def anim(self, seq, nframes, posefn):
        lines = ['version 1'] + self.nodes() + ['skeleton']
        for t in range(nframes):
            lines += self.frame(t, posefn(t / nframes))
        lines.append('end')
        open(os.path.join(WORK, '%s_%s.smd' % (self.name, seq)), 'w').write('\n'.join(lines) + '\n')

    def compile(self, bbox, eye, sequences, controllers=(), fullbright=()):
        qc = ['$modelname "%s.mdl"' % self.name, '$cd "."', '$cdtexture "."', '$scale 1.0', '$origin 0 0 0 270',
              '$bbox %g %g %g %g %g %g' % bbox, '$cbox %g %g %g %g %g %g' % bbox, '$eyeposition 0 0 %g' % eye,
              '$body "body" "%s_ref"' % self.name,
              '$texturegroup skinfamilies', '{', '{ "%s.bmp" }' % self.name, '{ "%s_hurt.bmp" }' % self.name, '}',
              '$texrendermode "%s_hurt.bmp" "fullbright"' % self.name]
        qc += ['$texrendermode "%s" "fullbright"' % f for f in fullbright]
        qc += ['$controller %d "%s" %s %g %g' % c for c in controllers]
        for name, smd, opts in sequences:
            qc.append('$sequence "%s" "%s_%s" %s' % (name, self.name, smd, opts))
        open(os.path.join(WORK, self.name + '.qc'), 'w').write('\n'.join(qc) + '\n')
        r = subprocess.run([STUDIOMDL, self.name + '.qc'], cwd=WORK, capture_output=True, text=True)
        out = os.path.join(WORK, self.name + '.mdl')
        if r.returncode != 0 or not os.path.exists(out):
            print(r.stdout[-2000:], r.stderr[-1000:])
            raise SystemExit('studiomdl failed for ' + self.name)
        os.makedirs(os.path.join(GAME, 'models', 'svencraft'), exist_ok=True)
        shutil.copy2(out, os.path.join(GAME, 'models', 'svencraft', self.name + '.mdl'))
        print('%s.mdl %d bytes, %d triangles' % (self.name, os.path.getsize(out), len(self.tris)))


def grid_onto(m, x0, y0, rows, colours):
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in colours:
                m.img[y0 + y, x0 + x] = colours[ch]


def die_pose(lift):
    def die(f):
        e = 1 - (1 - min(f * 1.25, 1.0)) ** 2        # tips over onto its side, then lies still
        return {'root': ((0, 0, lift * e), (math.pi / 2 * e, 0, 0))}
    return die


# ================================================================ the bipeds (zombie, skeleton)
def biped(m, limb):
    """Minecraft's biped in pixels: legs 12, body 12, head 8; `limb` is the arm and leg thickness (4 or 2)"""
    m.bone('root', None, (0, 0, 0))
    m.bone('body', 'root', (0, 0, 12))
    m.bone('head', 'body', (0, 0, 12))
    off = 4 + limb / 2                              # the arms hang beside the 8-wide body
    m.bone('arm_l', 'body', (0, off, 10))           # shoulder pivots, 2 below the body's top
    m.bone('arm_r', 'body', (0, -off, 10))
    m.bone('leg_l', 'root', (0, limb / 2 if limb == 4 else 2, 12))
    m.bone('leg_r', 'root', (0, -(limb / 2 if limb == 4 else 2), 12))


def biped_boxes(m, limb, paint_head, paint_body, paint_arm, paint_leg, body_depth=4):
    h = limb / 2
    m.box('head', (-4, -4, 24), (4, 4, 32), 'head', paint_head)
    m.box('body', (-body_depth / 2, -4, 12), (body_depth / 2, 4, 24), 'body', paint_body)
    for side, s in (('l', 1), ('r', -1)):
        cy = s * (4 + h)
        m.box('arm_' + side, (-h, cy - h, 12), (h, cy + h, 24), 'arm_' + side, paint_arm)
        ly = s * (h if limb == 4 else 2)
        m.box('leg_' + side, (-h, ly - h, 0), (h, ly + h, 12), 'leg_' + side, paint_leg)


def walk_legs(f, amp=0.6):
    s = amp * math.sin(2 * math.pi * f)
    return {'leg_l': (Z, (0, s, 0)), 'leg_r': (Z, (0, -s, 0))}


# ---------------------------------------------------------------- zombie
def make_zombie():
    m = Mob('zombie', 2.25, 64, 48, seed=31)
    SKIN = [(86, 132, 66), (74, 118, 56), (98, 146, 76), (68, 106, 52), (110, 156, 86)]
    HAIR = [(46, 74, 40), (40, 64, 34), (54, 84, 46)]
    SHIRT = [(0, 160, 160), (0, 140, 142), (16, 176, 172), (0, 122, 126)]
    PANTS = [(66, 64, 150), (56, 54, 132), (76, 74, 166), (48, 46, 118)]
    SHOE = [(70, 70, 74), (58, 58, 62)]

    def head(f, x, y, w, h):
        m.paint(x, y, w, h, HAIR if f == 'top' else SKIN)
        if f in ('front', 'back', 'left', 'right'):
            m.paint(x, y, w, 1 if f != 'back' else 3, HAIR)          # hairline
        if f == 'front':
            grid_onto(m, x, y, ["........", "........", "........",
                                ".EE..EE.", ".Ee..eE.", "...dd...", "..MMMM..", "........"],
                      {'E': (22, 30, 20), 'e': (42, 58, 36), 'd': (52, 84, 42), 'M': (40, 60, 34)})

    def body(f, x, y, w, h):
        m.paint(x, y, w, h, SHIRT)
        if f in ('front', 'back', 'left', 'right'):
            for i in range(w):                                   # a torn hem showing skin
                if m.rng.random() < 0.45:
                    m.img[y + h - 1, x + i] = m.rng.choice(SKIN)
            if f == 'front':
                m.img[y + 2, x + 3] = m.img[y + 3, x + 4] = (0, 104, 110)   # a rip

    def arm(f, x, y, w, h):
        m.paint(x, y, w, h, SKIN)
        if f in ('front', 'back', 'left', 'right'):
            m.paint(x, y, w, 4, SHIRT)                           # sleeve

    def leg(f, x, y, w, h):
        m.paint(x, y, w, h, PANTS)
        if f in ('front', 'back', 'left', 'right'):
            m.paint(x, y + h - 2, w, 2, SHOE)
        if f == 'bottom':
            m.paint(x, y, w, h, SHOE)

    biped(m, 4)
    biped_boxes(m, 4, head, body, arm, leg)
    m.save_textures()
    FWD = -math.pi / 2            # arms held straight out in front
    m.write_ref()

    def idle(f):
        s = math.sin(2 * math.pi * f)
        p = {'arm_l': (Z, (0.04 * s, FWD + 0.05 * s, 0)), 'arm_r': (Z, (-0.04 * s, FWD - 0.05 * s, 0)),
             'head': (Z, (0, 0.04 * s, 0))}
        return p

    def walk(f):
        p = walk_legs(f)
        b = 0.12 * math.sin(2 * math.pi * f)
        p.update({'arm_l': (Z, (0, FWD + b, 0)), 'arm_r': (Z, (0, FWD - b, 0)),
                  'body': ((0, 0, 0.6 * abs(math.cos(2 * math.pi * f))), Z)})
        return p

    def attack(f):
        k = math.sin(math.pi * f)                                # a swipe down and back up
        return {'arm_l': (Z, (0, FWD + 0.9 * k, 0)), 'arm_r': (Z, (0, FWD + 0.9 * k, 0)),
                'body': (Z, (0, 0.12 * k, 0))}

    die = die_pose(9)

    def die_arms(f):
        p = die(f)
        p.update({'arm_l': (Z, (0, FWD, 0)), 'arm_r': (Z, (0, FWD, 0))})
        return p
    m.anim('idle', 40, idle)
    m.anim('walk', 24, walk)
    m.anim('attack', 12, attack)
    m.anim('die', 20, die_arms)
    m.compile((-12, -12, 0, 12, 12, 72), 64,
              [('idle', 'idle', 'loop fps 20 ACT_IDLE 1'), ('walk', 'walk', 'loop fps 24 ACT_WALK 1'),
               ('run', 'walk', 'loop fps 36 ACT_RUN 1'), ('attack', 'attack', 'fps 24 ACT_MELEE_ATTACK1 1'),
               ('die', 'die', 'fps 20 ACT_DIESIMPLE 1')],
              controllers=[(0, 'head', 'ZR', -80, 80)])


# ---------------------------------------------------------------- skeleton
def make_skeleton():
    m = Mob('skeleton', 2.25, 64, 48, seed=37)
    BONE = [(208, 208, 204), (190, 190, 186), (222, 222, 218), (176, 176, 172)]
    DARK = [(54, 54, 54), (40, 40, 40), (66, 66, 64)]
    WOOD = [(128, 92, 50), (110, 78, 42), (146, 106, 60)]

    def head(f, x, y, w, h):
        m.paint(x, y, w, h, BONE)
        if f == 'front':
            grid_onto(m, x, y, ["........", "........", "........",
                                ".EE..EE.", ".EE..EE.", "...nn...", ".TtTtTt.", "........"],
                      {'E': (30, 30, 30), 'n': (70, 70, 68), 'T': (226, 226, 222), 't': (90, 90, 88)})
        if f == 'bottom':
            m.paint(x, y, w, h, DARK)

    def body(f, x, y, w, h):
        m.paint(x, y, w, h, DARK)                                # the gaps between the ribs
        if f in ('front', 'back'):
            for r in (1, 3, 5, 7):                               # ribs
                for i in range(1, w - 1):
                    m.img[y + r, x + i] = m.rng.choice(BONE)
            for r in range(h):                                   # spine
                m.img[y + r, x + w // 2] = m.img[y + r, x + w // 2 - 1] = m.rng.choice(BONE)
            for i in range(w):                                   # pelvis
                m.img[y + h - 2, x + i] = m.img[y + h - 3, x + i] = m.rng.choice(BONE)
        elif f in ('left', 'right'):
            for r in (1, 3, 5, 7, h - 3, h - 2):
                for i in range(w):
                    m.img[y + r, x + i] = m.rng.choice(BONE)
        else:
            m.paint(x, y, w, h, BONE)

    def limb(f, x, y, w, h):
        m.paint(x, y, w, h, BONE)
        if f in ('front', 'back', 'left', 'right'):
            for i in range(w):
                m.img[y + h // 2, x + i] = shade(m.rng.choice(BONE), 0.75)   # the joint

    biped(m, 2)
    biped_boxes(m, 2, head, body, limb, limb, body_depth=4)
    # the bow, held in the right hand (a bone of its own so it can stay upright whatever the arm does)
    hand = m.pivot('arm_r')
    m.bone('bow', 'arm_r', (0, 0, -10))                          # at the hand, 10 below the shoulder
    bz = hand[2] - 10
    by = hand[1]
    def wood(f, x, y, w, h):
        m.paint(x, y, w, h, WOOD)
    def string(f, x, y, w, h):
        m.paint(x, y, w, h, [(222, 218, 200), (200, 196, 180)])
    # the limbs bow forward of the hand (+x), the string behind; vertical when the arm hangs
    for i, (z0, z1, xo) in enumerate(((-7, -4, 1.5), (-4, -1, 2.5), (-1, 1, 3), (1, 4, 2.5), (4, 7, 1.5))):
        m.box('bow', (xo - 0.5, by - 0.5, bz + z0), (xo + 0.5, by + 0.5, bz + z1), 'bow%d' % i, wood)
    m.box('bow', (0.6, by - 0.15, bz - 6.5), (0.9, by + 0.15, bz + 6.5), 'bowstring', string)
    m.save_textures()
    m.write_ref()
    AIM = -math.pi / 2

    def keep_bow(arm_pitch):
        return {'bow': (Z, (0, -arm_pitch, 0))}      # (only pitch: the bow stays upright)

    def idle(f):
        s = 0.05 * math.sin(2 * math.pi * f)
        p = {'arm_l': (Z, (0, s, 0)), 'arm_r': (Z, (0, -s, 0)), 'head': (Z, (0, 0.03 * s, 0))}
        p.update(keep_bow(-s))
        return p

    def walk(f):
        p = walk_legs(f)
        a = 0.55 * math.sin(2 * math.pi * f)
        p.update({'arm_l': (Z, (0, -a, 0)), 'arm_r': (Z, (0, a, 0))})
        p.update(keep_bow(a))
        return p

    def aim(f):
        # both arms up, the left one drawing the string back to the face
        s = 0.02 * math.sin(2 * math.pi * f)
        p = {'arm_r': (Z, (0, AIM + s, 0)), 'arm_l': (Z, (0, AIM + 0.15 + s, -0.5)), 'head': (Z, (0, 0, 0))}
        p.update(keep_bow(AIM + s))
        return p

    def aimwalk(f):
        p = walk_legs(f, 0.45)
        p.update({'arm_r': (Z, (0, AIM, 0)), 'arm_l': (Z, (0, AIM + 0.15, -0.5))})
        p.update(keep_bow(AIM))
        return p

    die = die_pose(9)
    m.anim('idle', 40, idle)
    m.anim('walk', 24, walk)
    m.anim('aim', 20, aim)
    m.anim('aimwalk', 24, aimwalk)
    m.anim('die', 20, die)
    m.compile((-10, -10, 0, 10, 10, 72), 64,
              [('idle', 'idle', 'loop fps 20 ACT_IDLE 1'), ('walk', 'walk', 'loop fps 24 ACT_WALK 1'),
               ('run', 'walk', 'loop fps 34 ACT_RUN 1'), ('aim', 'aim', 'loop fps 20 ACT_RANGE_ATTACK1 1'),
               ('aimwalk', 'aimwalk', 'loop fps 24'), ('die', 'die', 'fps 20 ACT_DIESIMPLE 1')],
              controllers=[(0, 'head', 'ZR', -80, 80)])


# ---------------------------------------------------------------- spider
def make_spider():
    m = Mob('spider', 2.5, 64, 64, seed=41)
    BODY = [(58, 50, 46), (48, 42, 38), (70, 60, 54), (40, 34, 32)]
    DARKER = [(36, 30, 28), (30, 26, 24), (44, 38, 34)]
    LEG = [(52, 46, 42), (44, 38, 36), (62, 54, 50)]

    def head(f, x, y, w, h):
        m.paint(x, y, w, h, DARKER)
        if f == 'front':
            for ex, ey in ((1, 2), (6, 2), (2, 3), (5, 3), (3, 4), (4, 4), (2, 5), (5, 5)):
                if ex < w and ey < h:
                    m.img[y + ey, x + ex] = (20, 14, 14)          # the eye sockets (the eyes glow over them)

    def neck(f, x, y, w, h):
        m.paint(x, y, w, h, BODY)

    def abdomen(f, x, y, w, h):
        m.paint(x, y, w, h, BODY)
        if f == 'top':
            for i in range(h):                                   # a darker stripe down the back
                for j in range(w // 2 - 2, w // 2 + 2):
                    m.img[y + i, x + j] = m.rng.choice(DARKER)
            for i in range(1, h - 1, 3):
                m.img[y + i, x + 2] = m.img[y + i, x + w - 3] = (88, 76, 66)

    def leg(f, x, y, w, h):
        m.paint(x, y, w, h, LEG)
        if f in ('top', 'bottom', 'front', 'back'):
            m.img[y:y + h, x + w // 2] = (34, 30, 28)             # the knee

    # pixels: x forward, y left; the body hangs 9 above the ground (Minecraft's spider)
    m.bone('root', None, (0, 0, 0))
    m.bone('body', 'root', (0, 0, 9))
    m.bone('head', 'body', (3, 0, 0))
    m.box('body', (-3, -3, -3), (3, 3, 3), 'neck', neck)                     # the thorax
    m.box('body', (-15, -5, -4), (-3, 5, 4), 'abdomen', abdomen)            # 12 long behind it
    m.box('head', (0, -4, -4), (8, 4, 4), 'head', head)
    legs = []
    for side, s in (('l', 1), ('r', -1)):
        for i, (lx, yaw) in enumerate(((1.5, 0.75), (0.5, 0.25), (-0.5, -0.25), (-1.5, -0.75))):
            n = 'leg_%s%d' % (side, i)
            m.bone(n, 'body', (lx, 3 * s, 0))
            legs.append((n, s, -yaw * s))   # (+z turns a left leg backwards: the front ones turn the other way)
            # 16 long out to the side, 2 thick; posed down and fanned out by the bone (rest = straight out)
            m.box(n, (lx - 1, 3 * s if s > 0 else 3 * s - 16, -1), (lx + 1, 3 * s + 16 if s > 0 else 3 * s, 1), n, leg)
    # the eyes: small fullbright quads just in front of the head
    eyes = np.zeros((4, 4, 3), np.uint8)
    eyes[:, :] = (200, 26, 26)
    eyes[1:3, 1:3] = (250, 70, 60)
    m.extra.append(('spider_eyes.bmp', eyes))
    hx = 8.05
    for ex, ey in ((1, 2), (6, 2), (2, 3), (5, 3), (3, 4), (4, 4), (2, 5), (5, 5)):
        y0, z0 = 4 - ex - 1, 4 - ey - 1          # face pixel (col ex from the left as seen, row ey from the top)
        m.quad('head', [(hx, y0, z0), (hx, y0 + 1, z0), (hx, y0 + 1, z0 + 1), (hx, y0, z0 + 1)], (1, 0, 0), 'spider_eyes.bmp', 4, 4)
    m.save_textures()

    def leg_pose(n, s, yaw, swing=0.0, lift=0.0):
        # fanned out (yaw), angled down to the ground (roll: a left leg goes down for -x), plus the stride
        return (Z, (-(0.62 + lift) * s, 0, yaw + swing))

    def base_legs(t=0.0, amp=0.0, lifts=0.0):
        p = {}
        for k, (n, s, yaw) in enumerate(legs):
            ph = 2 * math.pi * t + (math.pi if (k % 2) ^ (k >= 4) else 0)
            p[n] = leg_pose(n, s, yaw, amp * math.sin(ph), -lifts * max(0.0, math.cos(ph)))
        return p

    m.write_ref()

    def idle(f):
        p = base_legs()
        p['body'] = ((0, 0, 0.25 * math.sin(2 * math.pi * f)), Z)
        p['head'] = (Z, (0, 0, 0.05 * math.sin(2 * math.pi * f)))
        return p

    def walk(f):
        p = base_legs(f, 0.32, 0.25)
        p['body'] = ((0, 0, 0.6 * abs(math.sin(4 * math.pi * f))), Z)
        return p

    def attack(f):
        k = math.sin(math.pi * f)
        p = base_legs()
        p['body'] = ((4 * k, 0, 2 * k), (0, -0.25 * k, 0))       # rears and lunges
        for n, s, yaw in legs[:1] + legs[4:5]:
            p[n] = leg_pose(n, s, yaw, -0.3 * k * s, -0.9 * k)   # front legs up and forward
        return p

    def die(f):
        e = 1 - (1 - min(f * 1.25, 1.0)) ** 2
        p = base_legs()
        for n, s, yaw in legs:
            p[n] = leg_pose(n, s, yaw * (1 - 0.6 * e), 0, 0.5 * e)   # legs curl in
        p['root'] = ((0, 0, 14 * e), (math.pi / 2 * e, 0, 0))
        return p
    m.anim('idle', 40, idle)
    m.anim('walk', 16, walk)
    m.anim('attack', 12, attack)
    m.anim('die', 20, die)
    m.compile((-16, -16, 0, 16, 16, 36), 24,
              [('idle', 'idle', 'loop fps 20 ACT_IDLE 1'), ('walk', 'walk', 'loop fps 24 ACT_WALK 1'),
               ('run', 'walk', 'loop fps 36 ACT_RUN 1'), ('attack', 'attack', 'fps 24 ACT_MELEE_ATTACK1 1'),
               ('die', 'die', 'fps 20 ACT_DIESIMPLE 1')],
              controllers=[(0, 'head', 'ZR', -40, 40)], fullbright=['spider_eyes.bmp'])


# ---------------------------------------------------------------- the arrow
def make_arrow():
    m = Mob('arrow', 2.0, 32, 32, seed=43)
    m.bone('root', None, (0, 0, 0))
    SHAFT = [(128, 96, 56), (112, 84, 48)]
    HEAD = [(150, 150, 156), (120, 120, 128)]
    FLETCH = [(232, 232, 232), (210, 210, 210)]
    m.box('root', (-7, -0.4, -0.4), (5, 0.4, 0.4), 'shaft', lambda f, x, y, w, h: m.paint(x, y, w, h, SHAFT))
    m.box('root', (5, -0.8, -0.8), (8, 0.8, 0.8), 'tip', lambda f, x, y, w, h: m.paint(x, y, w, h, HEAD))
    m.box('root', (-7, -0.1, -1.4), (-4, 0.1, 1.4), 'fletch1', lambda f, x, y, w, h: m.paint(x, y, w, h, FLETCH))
    m.box('root', (-7, -1.4, -0.1), (-4, 1.4, 0.1), 'fletch2', lambda f, x, y, w, h: m.paint(x, y, w, h, FLETCH))
    m.save_textures()
    m.write_ref()
    m.anim('idle', 2, lambda f: {})
    m.compile((-14, -3, -3, 16, 3, 3), 0, [('idle', 'idle', 'fps 10')])


make_zombie()
make_skeleton()
make_spider()
make_arrow()
