# Map-writing helpers for Svencraft maps (Valve 220 .map, WAD3, compile with the SDK compilers).
#
# A brush is a list of faces; a face is (vertices, texture[, texinfo]) with the vertices of a convex polygon in
# world units. Each face is written with three of its vertices, ordered so the plane normal points out of the brush.
import os, struct, subprocess, math
import numpy as np
from PIL import Image

import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'tools'))
import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_SDK / _SVEN)
SDK = os.path.join(sc_paths.SDK, 'mapping', 'compilers')
SVEN = sc_paths.SVEN


# ---------------------------------------------------------------- textures / WAD
def miptex(name, img):
    """PIL 'P' image (w, h multiples of 16) -> WAD3 miptex lump."""
    assert img.mode == 'P' and img.size[0] % 16 == 0 and img.size[1] % 16 == 0, (name, img.mode, img.size)
    w, h = img.size
    pal = (img.getpalette() + [0] * 768)[:768]
    rgb = np.array(img.convert('RGB'))
    mips = [np.array(img, np.uint8)]
    for k in (1, 2, 3):   # box-filtered mips re-quantized to the same palette
        s = 2 ** k
        small = rgb.reshape(h // s, s, w // s, s, 3).mean(axis=(1, 3)).astype(np.uint8)
        q = Image.fromarray(small, 'RGB').quantize(palette=img, dither=Image.Dither.NONE)
        mips.append(np.array(q, np.uint8))
    offs, data, o = [], b'', 16 + 4 * 2 + 4 * 4
    for m in mips:
        offs.append(o); data += m.tobytes(); o += m.size
    return name.encode().ljust(16, b'\0') + struct.pack('<II4I', w, h, *offs) + data + struct.pack('<H', 256) + bytes(pal) + b'\0\0'


def write_wad(path, images):
    """images: {name: PIL image (RGB or P)}"""
    blob, dirents, pos = b'', b'', 12
    for name, img in images.items():
        if img.mode != 'P':
            img = img.convert('RGB').quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        body = miptex(name, img)
        dirents += struct.pack('<iiibbh16s', pos, len(body), len(body), 0x43, 0, 0, name.encode().ljust(16, b'\0'))
        blob += body; pos += len(body)
    open(path, 'wb').write(b'WAD3' + struct.pack('<ii', len(images), pos) + blob + dirents)


# ---------------------------------------------------------------- geometry
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def world_axes(n):
    """Valve 220 texture axes aligned to the dominant axis of the face normal."""
    ax = max(range(3), key=lambda i: abs(n[i]))
    if ax == 2:
        return (1, 0, 0), (0, -1, 0)
    if ax == 0:
        return (0, 1, 0), (0, 0, -1)
    return (1, 0, 0), (0, 0, -1)


class Brush:
    def __init__(self):
        self.faces = []   # (verts, tex, u, v, shift_u, shift_v, scale_u, scale_v)

    def face(self, verts, tex, u=None, v=None, shift=(0, 0), scale=(1, 1)):
        self.faces.append([list(verts), tex, u, v, shift, scale])
        return self

    def text(self):
        # centroid of all vertices: every face normal must point away from it
        allv = [p for f in self.faces for p in f[0]]
        c = tuple(sum(p[i] for p in allv) / len(allv) for i in range(3))
        out = ['{']
        for verts, tex, u, v, shift, scale in self.faces:
            p0, p1, p2 = verts[0], verts[1], verts[2]
            n = cross(sub(p0, p1), sub(p2, p1))
            fc = tuple(sum(p[i] for p in verts) / len(verts) for i in range(3))
            if dot(n, sub(fc, c)) < 0:
                p0, p2 = p2, p0
                n = (-n[0], -n[1], -n[2])
            if u is None:
                u, v = world_axes(n)
            out.append('( %s ) ( %s ) ( %s ) %s [ %g %g %g %g ] [ %g %g %g %g ] 0 %g %g' % (
                ' '.join('%g' % x for x in p0), ' '.join('%g' % x for x in p1), ' '.join('%g' % x for x in p2),
                tex, u[0], u[1], u[2], shift[0], v[0], v[1], v[2], shift[1], scale[0], scale[1]))
        out.append('}')
        return '\n'.join(out)


def box(mn, mx, tex, **kw):
    """Axis-aligned box. tex: name, or dict with keys top/bottom/side/x+/x-/y+/y- (falls back to side, then 'all')."""
    (x0, y0, z0), (x1, y1, z1) = mn, mx
    assert x1 > x0 and y1 > y0 and z1 > z0, (mn, mx)
    if isinstance(tex, str):
        tex = {'all': tex}
    def t(key, fallback='side'):
        return tex.get(key) or tex.get(fallback) or tex.get('side') or tex.get('all') or next(iter(tex.values()))
    b = Brush()
    b.face([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], t('top', 'all'), **kw.get('top', {}))
    b.face([(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)], t('bottom', 'all'), **kw.get('bottom', {}))
    b.face([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], t('x+'), **kw.get('x+', {}))
    b.face([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], t('x-'), **kw.get('x-', {}))
    b.face([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], t('y+'), **kw.get('y+', {}))
    b.face([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], t('y-'), **kw.get('y-', {}))
    return b


def prism(poly, zb, ztop, tex_top, tex_side, tex_bottom=None, top_kw=None):
    """Vertical prism over a convex 2D polygon (counter-clockwise), bottom at zb, top vertices at heights ztop[i]
    (the top vertices must be coplanar - triangles always are)."""
    tex_bottom = tex_bottom or tex_side
    n = len(poly)
    b = Brush()
    b.face([(x, y, z) for (x, y), z in zip(poly, ztop)], tex_top, **(top_kw or {}))
    b.face([(x, y, zb) for (x, y) in reversed(poly)], tex_bottom)
    for i in range(n):
        (xa, ya), (xb, yb) = poly[i], poly[(i + 1) % n]
        b.face([(xa, ya, zb), (xb, yb, zb), (xb, yb, ztop[(i + 1) % n]), (xa, ya, ztop[i])], tex_side)
    return b


def cylinder_y(cx, cy0, cy1, cz, cx_off, r, sides, tex_cap, tex_side):
    """Prism along the Y axis: an n-gon in the XZ plane around (cx, cz), from y = cy0 to cy1."""
    pts = [(cx + r * math.cos(2 * math.pi * (i + 0.5) / sides), cz + r * math.sin(2 * math.pi * (i + 0.5) / sides)) for i in range(sides)]
    pts = [(round(x), round(z)) for x, z in pts]
    b = Brush()
    b.face([(x, cy1, z) for x, z in pts], tex_cap, u=(1, 0, 0), v=(0, 0, -1),
           shift=(-(cx - r) * (128 / (2 * r)), (cz + r) * (128 / (2 * r))), scale=(2 * r / 128, 2 * r / 128))
    b.face([(x, cy0, z) for x, z in reversed(pts)], tex_cap, u=(1, 0, 0), v=(0, 0, -1),
           shift=(-(cx - r) * (128 / (2 * r)), (cz + r) * (128 / (2 * r))), scale=(2 * r / 128, 2 * r / 128))
    for i in range(sides):
        (xa, za), (xb, zb) = pts[i], pts[(i + 1) % sides]
        b.face([(xa, cy0, za), (xb, cy0, zb), (xb, cy1, zb), (xa, cy1, za)], tex_side)
    return b


def entity(kv, brushes=()):
    s = '{\n' + ''.join('"%s" "%s"\n' % (k, v) for k, v in kv.items())
    s += ''.join(b.text() + '\n' for b in brushes)
    return s + '}\n'


def compile_map(src_dir, name, rad_args=('-bounce', '1', '-extra'), vis_args=()):
    for exe, args in (('SC-CSG_x64.exe', ['-nowadtextures']), ('SC-BSP_x64.exe', []), ('SC-VIS_x64.exe', list(vis_args)),
                      ('SC-RAD_x64.exe', list(rad_args))):
        r = subprocess.run([os.path.join(SDK, exe), name] + args, cwd=src_dir, capture_output=True, text=True)
        log = r.stdout + r.stderr
        errs = [l for l in log.splitlines() if 'error' in l.lower() and 'no errors' not in l.lower()]
        if r.returncode != 0 or errs:
            print(exe, 'returned', r.returncode); print('\n'.join(errs[:20]) or log[-3000:])
            if r.returncode != 0:
                raise SystemExit(1)
    return os.path.join(src_dir, name + '.bsp')


def cylinder_x(cy, cx0, cx1, cz, r, sides, tex_cap, tex_side):
    """Prism along the X axis: an n-gon in the YZ plane around (cy, cz), from x = cx0 to cx1."""
    pts = [(cy + r * math.cos(2 * math.pi * (i + 0.5) / sides), cz + r * math.sin(2 * math.pi * (i + 0.5) / sides)) for i in range(sides)]
    pts = [(round(y), round(z)) for y, z in pts]
    s = 2 * r / 128
    cap = dict(u=(0, 1, 0), v=(0, 0, -1), shift=(-(cy - r) / s, (cz + r) / s), scale=(s, s))
    b = Brush()
    b.face([(cx1, y, z) for y, z in pts], tex_cap, **cap)
    b.face([(cx0, y, z) for y, z in reversed(pts)], tex_cap, **cap)
    for i in range(sides):
        (ya, za), (yb, zb) = pts[i], pts[(i + 1) % sides]
        b.face([(cx0, ya, za), (cx1, ya, za), (cx1, yb, zb), (cx0, yb, zb)], tex_side)
    return b


def _norm(v):
    l = math.sqrt(dot(v, v))
    return (v[0] / l, v[1] / l, v[2] / l)


def brush_planes(b):
    """(normal, dist, texture, (u, su, shift_u), (v, sv, shift_v)) per face, normals pointing out of the brush"""
    allv = [p for f in b.faces for p in f[0]]
    c = tuple(sum(p[i] for p in allv) / len(allv) for i in range(3))
    out = []
    for verts, tex, u, v, shift, scale in b.faces:
        p0, p1, p2 = verts[0], verts[1], verts[2]
        n = cross(sub(p0, p1), sub(p2, p1))
        fc = tuple(sum(p[i] for p in verts) / len(verts) for i in range(3))
        if dot(n, sub(fc, c)) < 0:
            n = (-n[0], -n[1], -n[2])
        n = _norm(n)
        if u is None:
            u, v = world_axes(n)
        out.append((n, dot(n, p1), tex, (u, scale[0], shift[0]), (v, scale[1], shift[1])))
    return out


def write_dyn(path, entries, texsize):
    """entries: [(Brush, material id, cut texture name)]; texsize(name) -> (w, h). Format: see engine dynworld.c"""
    names = []
    def tid(name):
        if name not in names:
            names.append(name)
        return names.index(name)
    body = []
    for b, mat, cut in entries:
        planes = brush_planes(b)
        body.append('brush %d %d %d' % (mat, tid(cut), len(planes)))
        for n, d, tex, (u, su, hu), (v, sv, hv) in planes:
            body.append('side %.6f %.6f %.6f %.4f %d %.6f %.6f %.6f %.4f %.6f %.6f %.6f %.4f' % (
                n[0], n[1], n[2], d, tid(tex), u[0] / su, u[1] / su, u[2] / su, hu, v[0] / sv, v[1] / sv, v[2] / sv, hv))
    head = ['dynworld 1'] + ['tex %s %d %d' % (nm, *texsize(nm)) for nm in names]
    open(path, 'w', newline='\n').write('\n'.join(head + body) + '\n')
    return len(entries), len(names)
