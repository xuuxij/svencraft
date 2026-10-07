"""Minimal GoldSrc .spr (IDSP v2) parser + renderer."""
import struct, sys
import numpy as np
from PIL import Image

TYPES = {0: 'vp_parallel_upright', 1: 'facing_upright', 2: 'vp_parallel', 3: 'oriented', 4: 'vp_parallel_oriented'}
TEXFMT = {0: 'normal', 1: 'additive', 2: 'indexalpha', 3: 'alphatest'}


def parse(path):
    d = open(path, 'rb').read()
    ident, ver, typ, tex, brad, mw, mh, nf, beam, sync = struct.unpack_from('<4siiifiiifi', d, 0)
    assert ident == b'IDSP', ident
    assert ver == 2, ver
    off = 40
    ncol, = struct.unpack_from('<h', d, off); off += 2
    pal = np.frombuffer(d, np.uint8, ncol * 3, off).reshape(ncol, 3).copy(); off += ncol * 3
    frames = []
    for i in range(nf):
        ft, = struct.unpack_from('<i', d, off); off += 4
        if ft != 0:
            raise NotImplementedError('frame groups not supported')
        ox, oy, w, h = struct.unpack_from('<iiii', d, off); off += 16
        px = np.frombuffer(d, np.uint8, w * h, off).reshape(h, w).copy(); off += w * h
        frames.append(dict(origin=(ox, oy), w=w, h=h, px=px))
    assert off == len(d), (off, len(d))
    return dict(type=typ, typename=TYPES.get(typ, '?'), tex=tex, texname=TEXFMT.get(tex, '?'),
                radius=brad, maxw=mw, maxh=mh, nframes=nf, beam=beam, sync=sync,
                ncolors=ncol, pal=pal, frames=frames, size=len(d))


def render(spr, fi=0, mode=None, tint=(255, 255, 255), bg=(0, 0, 0)):
    """Render a frame to RGB, emulating the texture format over a background colour."""
    f = spr['frames'][fi]
    pal = spr['pal']
    rgb = pal[f['px']].astype(np.float32)
    mode = mode or spr['texname']
    bgc = np.zeros_like(rgb) + np.array(bg, np.float32)
    t = np.array(tint, np.float32) / 255.0
    if mode == 'additive':
        out = bgc + rgb * t
    elif mode == 'alphatest':
        m = (f['px'] == 255)[..., None]
        out = np.where(m, bgc, rgb * t)
    elif mode == 'indexalpha':
        a = f['px'][..., None].astype(np.float32) / 255.0
        out = bgc * (1 - a) + pal[255].astype(np.float32) * t * a
    else:
        out = rgb * t
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), 'RGB')


def describe(path):
    s = parse(path)
    print(f"{path}\n  size={s['size']} bytes  type={s['type']} ({s['typename']})  tex={s['tex']} ({s['texname']})"
          f"  max={s['maxw']}x{s['maxh']}  frames={s['nframes']}  radius={s['radius']:.2f}  beam={s['beam']}  sync={s['sync']}"
          f"  palette={s['ncolors']} colours")
    for i, f in enumerate(s['frames']):
        print(f"  frame {i}: origin={f['origin']} {f['w']}x{f['h']}")
    return s


if __name__ == '__main__':
    for p in sys.argv[1:]:
        describe(p)
