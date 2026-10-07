# Builds Minecraft-style tool models (axe / pickaxe) from the crowbar models by replacing the crowbar mesh
# with new blocky geometry in the same bone space. Skeleton, animations, hands and events are untouched,
# so the result plays exactly like the crowbar.
#   python toolmdl.py <src.mdl> <dst.mdl> <axe|pickaxe|shovel> <tier 1=wood 2=stone 3=iron>
import struct, sys
import numpy as np
from PIL import Image

src, dst, kind = sys.argv[1], sys.argv[2], sys.argv[3]
tier = int(sys.argv[4]) if len(sys.argv) > 4 else 3
d = bytearray(open(src, 'rb').read())
I = lambda o: struct.unpack_from('<i', d, o)[0]
hdr = struct.unpack_from('<i26i', d, 136)
H = dict(zip("flags numbones boneindex numbonecontrollers bonecontrollerindex numhitboxes hitboxindex numseq seqindex numseqgroups seqgroupindex numtextures textureindex texturedataindex numskinref numskinfamilies skinindex numbodyparts bodypartindex numattachments attachmentindex soundtable soundindex soundgroups soundgroupindex numtransitions transitionindex".split(), hdr))
TEXDATA = H['texturedataindex']

# --- find the tool's sub-model (body part "studio")
model_off = None
for b in range(H['numbodyparts']):
    o = H['bodypartindex'] + 76 * b
    name = bytes(d[o:o + 64]).split(b'\0')[0].decode()
    nm, base, mi = struct.unpack_from('<3i', d, o + 64)
    if name == 'studio':
        model_off = mi
assert model_off is not None, 'no "studio" body part'
f = list(struct.unpack_from('<if10i', d, model_off + 64))
nmesh, meshi, nverts, vinfo, vidx, nnorms, ninfo, nidx = f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]
assert max(meshi, vinfo, vidx, ninfo, nidx) < TEXDATA
bone = d[vinfo]
V = np.array([struct.unpack_from('<3f', d, vidx + 12 * i) for i in range(nverts)])
skinrefs = [struct.unpack_from('<5i', d, meshi + 20 * k)[2] for k in range(nmesh)]
skinref = skinrefs[0]
skins = struct.unpack_from('<%dh' % (H['numskinref'] * H['numskinfamilies']), d, H['skinindex'])
tex_slot = skins[skinref]

# --- crowbar axis (PCA) and which end is the head
c0 = V.mean(0)
u = np.linalg.svd(V - c0)[2][0]
t = (V - c0) @ u
tmin, tmax = t.min(), t.max()
L = tmax - tmin
if abs(tmax + (c0 @ u)) < abs(tmin + (c0 @ u)):   # head = end farther from the bone origin (the hand)
    u, t, tmin, tmax = -u, -t, -tmax, -tmin
head = c0 + u * tmax
grip = c0 + u * tmin
# hook direction: perpendicular offset of the vertices near the head end
near = V[t > tmax - 0.2 * L] - c0
perp = near - np.outer(near @ u, u)
h = perp.mean(0)
if np.linalg.norm(h) < 1e-3:
    h = np.cross(u, [0, 0, 1] if abs(u[2]) < 0.9 else [1, 0, 0])
h = h / np.linalg.norm(h)
w = np.cross(u, h)

# --- geometry: boxes in (u, h, w) frame, region 0 = wood (atlas rows 0-63), 1 = iron (rows 64-127)
boxes = []
def box(t0, t1, h0, h1, w0, w1, region):
    boxes.append((t0, t1, h0, h1, w0, w1, region))
r = 0.028 * L                      # handle half-thickness
box(tmin, tmax, -r, r, -r, r, 0)   # handle along the whole crowbar
if kind == 'axe':
    box(tmax - 0.24 * L, tmax + 0.02 * L, -0.05 * L, 0.16 * L, -1.6 * r, 1.6 * r, 1)   # head block
    box(tmax - 0.28 * L, tmax + 0.05 * L, 0.16 * L, 0.24 * L, -1.0 * r, 1.0 * r, 1)    # wider blade edge
elif kind == 'shovel':
    box(tmax - 0.02 * L, tmax + 0.22 * L, -0.10 * L, 0.10 * L, -0.7 * r, 0.7 * r, 1)   # flat blade past the end
    box(tmax - 0.05 * L, tmax + 0.00 * L, -0.05 * L, 0.05 * L, -1.3 * r, 1.3 * r, 1)   # collar
else:
    box(tmax - 0.07 * L, tmax + 0.01 * L, -0.30 * L, 0.30 * L, -1.4 * r, 1.4 * r, 1)   # pick bar
    box(tmax - 0.12 * L, tmax - 0.04 * L, 0.26 * L, 0.36 * L, -1.0 * r, 1.0 * r, 1)    # tips bend toward the grip
    box(tmax - 0.12 * L, tmax - 0.04 * L, -0.36 * L, -0.26 * L, -1.0 * r, 1.0 * r, 1)

verts, norms, tris = [], [], []    # tris: list of fans [(vi, ni, s, t) x4]
for (t0, t1, h0, h1, w0, w1, region) in boxes:
    base = len(verts)
    corners = {}
    for a, tv in enumerate((t0, t1)):
        for b_, hv in enumerate((h0, h1)):
            for c_, wv in enumerate((w0, w1)):
                corners[(a, b_, c_)] = len(verts)
                verts.append(c0 + u * tv + h * hv + w * wv)
    faces = [((1, None, None), u), ((0, None, None), -u), ((None, 1, None), h), ((None, 0, None), -h), ((None, None, 1), w), ((None, None, 0), -w)]
    for fixed, n in faces:
        ax = [i for i, v in enumerate(fixed) if v is not None][0]
        others = [i for i in range(3) if i != ax]
        quad = []
        for (p, q) in ((0, 0), (1, 0), (1, 1), (0, 1)):
            key = [0, 0, 0]; key[ax] = fixed[ax]; key[others[0]] = p; key[others[1]] = q
            quad.append(corners[tuple(key)])
        P = [verts[i] for i in quad]
        if np.cross(P[1] - P[0], P[2] - P[0]) @ n > 0:   # engine draws clockwise-from-outside
            quad = quad[::-1]
        ni = len(norms); norms.append(n)
        y0 = 64 * region
        st = [(0, y0), (63, y0), (63, y0 + 63), (0, y0 + 63)]
        tris.append([(vi, ni, s, tt) for vi, (s, tt) in zip(quad, st)])

# --- serialize new geometry, inserted just before the texture data
def align(b):
    return b + b'\0' * (-len(b) % 4)
blob = b''
off = TEXDATA
vert_off = off + len(blob); blob = align(blob + b''.join(struct.pack('<3f', *v) for v in verts))
vinfo_off = off + len(blob); blob = align(blob + bytes([bone] * len(verts)))
norm_off = off + len(blob); blob = align(blob + b''.join(struct.pack('<3f', *n) for n in norms))
ninfo_off = off + len(blob); blob = align(blob + bytes([bone] * len(norms)))
cmds = b''.join(struct.pack('<h', -4) + b''.join(struct.pack('<4h', *c) for c in fan) for fan in tris) + struct.pack('<h', 0)
tri_off = off + len(blob); blob = align(blob + cmds)
mesh_off = off + len(blob); blob = align(blob + struct.pack('<5i', len(tris) * 2, tri_off, skinref, len(norms), 0))
ins = len(blob)

# model struct: one mesh, new arrays
f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9] = 1, mesh_off, len(verts), vinfo_off, vert_off, len(norms), ninfo_off, norm_off
f[1] = float(max(np.linalg.norm(v) for v in verts))
struct.pack_into('<if10i', d, model_off + 64, *f)

# shift texture pixel offsets, header texture data index and length
for i in range(H['numtextures']):
    o = H['textureindex'] + 80 * i
    struct.pack_into('<i', d, o + 76, I(o + 76) + ins)
struct.pack_into('<i', d, 136 + 4 * 13, TEXDATA + ins)
out = d[:TEXDATA] + blob + d[TEXDATA:]

# --- new 64x128 atlas texture (wood over iron), appended; replaces the crowbar texture slot
rng = np.random.default_rng(5)
wood = np.zeros((16, 16, 3)) + (118, 84, 48) + rng.normal(0, 6, (16, 16, 1))
for x in range(0, 16, 4): wood[:, x] *= 0.82
HEAD = {1: (150, 112, 64), 2: (128, 128, 128), 3: (198, 200, 204)}[tier]      # wood / stone / iron head
iron = np.zeros((16, 16, 3)) + HEAD + rng.normal(0, 7 if tier == 2 else 5, (16, 16, 1))
iron[0, :] = iron[:, 0] = np.clip(np.array(HEAD) * 1.18, 0, 255); iron[15, :] = iron[:, 15] = np.array(HEAD) * 0.6
atlas = np.concatenate([wood, iron], 0).clip(0, 255).astype(np.uint8)
img = Image.fromarray(atlas, 'RGB').quantize(256).resize((64, 128), Image.NEAREST)
pix = img.tobytes()
pal = bytes((img.getpalette() + [0] * 768)[:768])
tex_pix = len(out)
out += pix + pal
o = H['textureindex'] + 80 * tex_slot
struct.pack_into('<64s', out, o, ('sc_%s%d.bmp' % (kind, tier)).encode())
struct.pack_into('<4i', out, o + 64, 0, 64, 128, tex_pix)
struct.pack_into('<i', out, 72, len(out))       # header length
open(dst, 'wb').write(out)
print('%s: %d verts, %d faces, L=%.1f, bone %d, texture slot %d, %d bytes' % (dst.split('\\')[-1].split('/')[-1], len(verts), len(tris), L, bone, tex_slot, len(out)))
