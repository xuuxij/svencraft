# Average lightmap brightness per brush model (and per face direction) in a GoldSrc BSP.
import struct, sys, math, re, collections
p = sys.argv[1]
d = open(p, 'rb').read()
lumps = [struct.unpack_from('<ii', d, 4 + 8 * i) for i in range(15)]
ENT, PLANES, TEX, VERTS, VIS, NODES, TEXINFO, FACES, LIGHT, CLIP, LEAVES, MARK, EDGES, SURFEDGES, MODELS = range(15)
def lump(i): o, l = lumps[i]; return d[o:o + l]
planes = [struct.unpack_from('<4fi', lump(PLANES), 20 * i) for i in range(lumps[PLANES][1] // 20)]
verts = [struct.unpack_from('<3f', lump(VERTS), 12 * i) for i in range(lumps[VERTS][1] // 12)]
edges = [struct.unpack_from('<2H', lump(EDGES), 4 * i) for i in range(lumps[EDGES][1] // 4)]
surfedges = struct.unpack_from('<%di' % (lumps[SURFEDGES][1] // 4), lump(SURFEDGES))
texinfo = [struct.unpack_from('<8f2i', lump(TEXINFO), 40 * i) for i in range(lumps[TEXINFO][1] // 40)]
faces = [struct.unpack_from('<HHiHH4Bi', lump(FACES), 20 * i) for i in range(lumps[FACES][1] // 20)]
models = [struct.unpack_from('<9f7i', lump(MODELS), 64 * i) for i in range(lumps[MODELS][1] // 64)]
light = lump(LIGHT)
# entity model -> classname/targetname
ents = re.findall(r'\{([^{}]*)\}', lump(ENT).decode('latin-1'))
names = {}
for e in ents:
    kv = dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', e))
    if kv.get('model', '').startswith('*'):
        names[int(kv['model'][1:])] = kv.get('classname', '') + ' ' + kv.get('targetname', '')

def face_light(fi):
    plane, side, fe, ne, ti, s0, s1, s2, s3, lofs = faces[fi]
    if lofs < 0: return None, None
    tx = texinfo[ti]
    pts = []
    for k in range(ne):
        se = surfedges[fe + k]
        v = verts[edges[se][0]] if se >= 0 else verts[edges[-se][1]]
        pts.append(v)
    ss = [v[0] * tx[0] + v[1] * tx[1] + v[2] * tx[2] + tx[3] for v in pts]
    ts = [v[0] * tx[4] + v[1] * tx[5] + v[2] * tx[6] + tx[7] for v in pts]
    w = int(math.ceil(max(ss) / 16) - math.floor(min(ss) / 16)) + 1
    h = int(math.ceil(max(ts) / 16) - math.floor(min(ts) / 16)) + 1
    n = w * h * 3
    samp = light[lofs:lofs + n]
    nrm = planes[plane][:3]
    if side: nrm = tuple(-c for c in nrm)
    return (sum(samp) / max(1, len(samp))), nrm

for mi in [int(a) for a in sys.argv[2:]] or range(len(models)):
    m = models[mi]
    firstface, numfaces = m[14], m[15]
    bydir = collections.defaultdict(list)
    for fi in range(firstface, firstface + numfaces):
        val, nrm = face_light(fi)
        if val is None: continue
        key = 'up' if nrm[2] > 0.7 else 'down' if nrm[2] < -0.7 else 'side'
        bydir[key].append(val)
    print('*%d %-28s' % (mi, names.get(mi, 'world' if mi == 0 else '?')), {k: round(sum(v) / len(v)) for k, v in bydir.items()})
