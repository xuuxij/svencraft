"""Svencraft hotbar art: blockicons (18 isometric cubes, 48x48, alphatest) and slot / slot_sel (56x56 outlines).
Alphatest convention: palette index 255 = transparent key (blue 0,0,255); no opaque pixel uses index 255.
"""
import os
import numpy as np
from PIL import Image

TEX = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tex')
FS = 48                                  # frame size
COLS, ROWS = 6, 4                        # atlas layout for sprgen (positions are multiples of 8)
# 2:1 pixel-art projection: cube spans x 2..45, y 1..46 (1-2 px transparent margin)
CX, HW, HT, SH, TOP = 24.0, 22.0, 11.0, 24.0, 1.0

MATERIALS = [
    ('grass', 'grass_top', 'grass_side'), ('dirt', 'dirt', 'dirt'), ('stone', 'stone', 'stone'),
    ('cobble', 'cobble', 'cobble'), ('brick', 'brick', 'brick'), ('planks', 'planks', 'planks'),
    ('log', 'log_top', 'log_side'), ('leaves', 'leaves', 'leaves'), ('sand', 'sand', 'sand'),
    ('gravel', 'gravel', 'gravel'), ('snow', 'snow', 'snow'), ('metal', 'metal', 'metal'),
    ('concrete', 'concrete', 'concrete'), ('tile', 'tile', 'tile'), ('rubber', 'rubber', 'rubber'),
    ('glass', 'glass', 'glass'), ('circuit', 'circuit', 'circuit'), ('vent', 'vent', 'vent'),
    ('workbench', 'bench_top', 'bench_side'),
    ('iron_ore', 'iron_ore', 'iron_ore'), ('crystal_ore', 'crystal_ore', 'crystal_ore'),
    ('glass_pane', 'glass', 'glass'), ('door', 'door', 'door'),
]
MASKED = {'glass', 'leaves', 'door'}             # textures whose palette index 255 is a hole

SHADE = {'top': 1.0, 'left': 0.8, 'right': 0.6,
         # inner sides of the far faces, only visible through holes
         'bottom_in': 0.62, 'backx_in': 0.5, 'backz_in': 0.66}


def load_tex(name):
    im = Image.open(os.path.join(TEX, name + '.bmp'))
    assert im.mode == 'P', (name, im.mode)
    idx = np.array(im)
    p = np.array(im.getpalette()[:768], np.uint8).reshape(-1, 3)
    pal = np.zeros((256, 3), np.uint8); pal[:len(p)] = p   # BMPs store a compact palette (biClrUsed < 256)
    rgb = pal[idx].astype(np.float32)
    hole = (idx == 255) if name in MASKED else np.zeros(idx.shape, bool)
    return rgb, hole


def face_params(face, sx, sy):
    """Screen point -> (u, v) on the face, in [0,1] when inside."""
    if face in ('top', 'bottom_in'):
        y0 = TOP if face == 'top' else TOP + SH
        a = (sx - CX) / HW            # X - Z
        b = (sy - y0) / HT            # X + Z
        X, Z = (a + b) / 2, (b - a) / 2
        return X, Z
    if face == 'left':                # Z = 1, screen left->right = X 0..1
        X = (sx - (CX - HW)) / HW
        v = (sy - TOP - HT - X * HT) / SH
        return X, v
    if face == 'right':               # X = 1, screen left->right = Z 1..0
        Z = ((CX + HW) - sx) / HW
        v = (sy - TOP - HT - Z * HT) / SH
        return 1 - Z, v
    if face == 'backx_in':            # X = 0 plane, spans screen x CX-HW..CX
        Z = (CX - sx) / HW
        v = (sy - TOP - Z * HT) / SH
        return Z, v
    if face == 'backz_in':            # Z = 0 plane, spans screen x CX..CX+HW
        X = (sx - CX) / HW
        v = (sy - TOP - X * HT) / SH
        return 1 - X, v
    raise ValueError(face)


def render_cube(top_name, side_name):
    top_rgb, top_hole = load_tex(top_name)
    side_rgb, side_hole = load_tex(side_name)
    out = np.zeros((FS, FS, 3), np.float32)
    opaque = np.zeros((FS, FS), bool)
    ys, xs = np.mgrid[0:FS, 0:FS]
    sx, sy = xs + 0.5, ys + 0.5
    eps = 1e-6
    # painter's order: far inner faces first, then the three visible faces
    for face in ('bottom_in', 'backx_in', 'backz_in', 'top', 'left', 'right'):
        u, v = face_params(face, sx, sy)
        inside = (u >= -eps) & (u <= 1 + eps) & (v >= -eps) & (v <= 1 + eps)
        rgb, hole = (top_rgb, top_hole) if face in ('top', 'bottom_in') else (side_rgb, side_hole)
        tu = np.clip((u * 64).astype(int), 0, 63)
        tv = np.clip((v * 64).astype(int), 0, 63)
        m = inside & ~hole[tv, tu]
        out[m] = rgb[tv, tu][m] * SHADE[face]
        opaque[m] = True
        if face in ('top', 'left', 'right'):
            # a hole in a front face shows what is behind it (inner far faces) or nothing
            pass
    return np.clip(np.round(out), 0, 255).astype(np.uint8), opaque


FLAT_ICONS = {'glass_pane', 'door'}   # items drawn flat (like Minecraft item sprites), not as cubes
OUTLINE = (34, 34, 34)


def flat_icon(name):
    """16-px pixel art of the item's texture, drawn flat at 3x with a 1px dark outline, centred in FS x FS."""
    rgb, hole = load_tex(name)
    h, w = hole.shape
    art = rgb[::4, ::4]; ah = hole[::4, ::4]                 # textures are 16-px art upscaled 4x
    if name == 'door':
        art = art[::2, ::2]; ah = ah[::2, ::2]               # 8x16 door silhouette
    ah = ah.copy()
    filled = ~ah
    if name == 'glass':
        filled[:] = True                                     # the pane icon shows the glass faintly filled in
        art = np.where(ah[..., None], art * 0 + (178, 214, 228), art)
    a = np.repeat(np.repeat(art, 3, 0), 3, 1); f = np.repeat(np.repeat(filled, 3, 0), 3, 1)
    out = np.zeros((FS, FS, 3), np.float32); op = np.zeros((FS, FS), bool)
    y0 = (FS - a.shape[0]) // 2; x0 = (FS - a.shape[1]) // 2
    out[y0:y0 + a.shape[0], x0:x0 + a.shape[1]] = a
    op[y0:y0 + a.shape[0], x0:x0 + a.shape[1]] = f
    # outline: one ring around the item's bounding box
    ring = np.zeros_like(op)
    ys, xs = y0 - 1, x0 - 1
    ye, xe = y0 + a.shape[0], x0 + a.shape[1]
    ring[max(ys, 0):ye + 1, max(xs, 0)] = True; ring[max(ys, 0):ye + 1, min(xe, FS - 1)] = True
    ring[max(ys, 0), max(xs, 0):xe + 1] = True; ring[min(ye, FS - 1), max(xs, 0):xe + 1] = True
    out[ring] = OUTLINE; op |= ring
    return np.clip(np.round(out), 0, 255).astype(np.uint8), op


def material_icon(name, top, side):
    return flat_icon(side) if name in FLAT_ICONS else render_cube(top, side)


def quantize_alphatest(rgb, opaque):
    """RGB + opaque mask -> 8-bit indices (0..254 opaque, 255 transparent) and 256-colour palette."""
    fill = rgb[opaque][0]
    src = rgb.copy(); src[~opaque] = fill
    uniq = np.unique(src.reshape(-1, 3), axis=0)
    if len(uniq) <= 255:
        lut = {tuple(c): i for i, c in enumerate(uniq)}
        idx = np.array([lut[tuple(c)] for c in src.reshape(-1, 3)], np.uint8).reshape(src.shape[:2])
        pal = np.zeros((256, 3), np.uint8); pal[:len(uniq)] = uniq
    else:
        # median-cut seed, then weighted k-means (Lloyd) refinement over the opaque colours only
        q = Image.fromarray(src, 'RGB').quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        cent = np.array(q.getpalette()[:765], np.float64).reshape(-1, 3)
        cols, counts = np.unique(rgb[opaque].reshape(-1, 3), axis=0, return_counts=True)
        cols = cols.astype(np.float64)
        for _ in range(40):
            d = ((cols[:, None, :] - cent[None, :, :]) ** 2).sum(-1)
            a = d.argmin(1)
            for k in range(len(cent)):
                sel = a == k
                if sel.any():
                    cent[k] = (cols[sel] * counts[sel, None]).sum(0) / counts[sel].sum()
                else:   # re-seed an empty cluster at the worst-represented colour
                    worst = d[np.arange(len(cols)), a].argmax()
                    cent[k] = cols[worst]
        cent = np.clip(np.round(cent), 0, 255)
        flat = src.reshape(-1, 3).astype(np.float64)
        idx = np.zeros(len(flat), np.uint8)
        for s0 in range(0, len(flat), 8192):
            blk = flat[s0:s0 + 8192]
            idx[s0:s0 + 8192] = ((blk[:, None, :] - cent[None, :, :]) ** 2).sum(-1).argmin(1)
        idx = idx.reshape(src.shape[:2])
        pal = np.zeros((256, 3), np.uint8)
        pal[:len(cent)] = cent.astype(np.uint8)
    assert idx[opaque].max() <= 254
    idx[~opaque] = 255
    pal[255] = (0, 0, 255)
    return idx, pal, len(uniq)


def save_bmp(idx, pal, path):
    im = Image.fromarray(idx, 'P'); im.putpalette(pal.reshape(-1).tolist()); im.save(path)


def build_blocks():
    atlas = np.zeros((ROWS * FS, COLS * FS, 3), np.uint8)
    amask = np.zeros((ROWS * FS, COLS * FS), bool)
    for i, (name, t, s) in enumerate(MATERIALS):
        rgb, op = material_icon(name, t, s)
        r, c = divmod(i, COLS)
        atlas[r * FS:(r + 1) * FS, c * FS:(c + 1) * FS] = rgb
        amask[r * FS:(r + 1) * FS, c * FS:(c + 1) * FS] = op
    idx, pal, nuniq = quantize_alphatest(atlas, amask)
    save_bmp(idx, pal, 'blockicons.bmp')
    err = np.abs(pal[idx].astype(int) - atlas.astype(int))[amask]
    print(f'blockicons: {nuniq} unique colours before quantize; mean abs err {err.mean():.2f}, max {err.max()}')
    with open('blockicons.qc', 'w', newline='\n') as f:
        f.write('$spritename blockicons\n$type vp_parallel\n$texture alphatest\n$load blockicons.bmp\n')
        for i in range(len(MATERIALS)):
            r, c = divmod(i, COLS)
            f.write(f'$frame {c * FS} {r * FS} {FS} {FS}\n')


def build_slot(name, size, width, colour):
    idx = np.full((size, size), 255, np.uint8)
    idx[:width, :] = 0; idx[-width:, :] = 0; idx[:, :width] = 0; idx[:, -width:] = 0
    pal = np.zeros((256, 3), np.uint8)
    pal[0] = colour
    pal[255] = (0, 0, 255)
    save_bmp(idx, pal, name + '.bmp')
    with open(name + '.qc', 'w', newline='\n') as f:
        f.write(f'$spritename {name}\n$type vp_parallel\n$texture alphatest\n$load {name}.bmp\n$frame 0 0 {size} {size}\n')


if __name__ == '__main__':
    build_blocks()
    build_slot('slot', 56, 2, (198, 198, 198))
    build_slot('slot_sel', 56, 3, (255, 255, 255))
    print('done')
