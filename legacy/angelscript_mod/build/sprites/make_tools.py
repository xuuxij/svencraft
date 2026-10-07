"""Svencraft weapon-selection icons: pickaxe, shovel, axe (170x45 each), in the style of
640hud1 (unselected: shaded outline drawing) and 640hud4 (selected: solid white + glow).
Greyscale identity palette (index i == grey i), drawn additively by the client and tinted by the HUD colour.
"""
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SS = 8                      # supersampling factor
W, H = 170, 45              # icon cell
SHEET_W, SHEET_H = 256, 136  # sprgen needs multiples of 8 (<=256)
BG_EVEN, BG_ODD = 21, 29     # stock scanline panel values (measured from 640hud1/4)


def P(pts):
    return [(x * SS, y * SS) for x, y in pts]


class Icon:
    def __init__(self):
        self.parts = []   # (name, material, hi-res bool mask)
        self.lines = []   # extra detail strokes: (pts, width_px, level_unsel, level_sel)

    def _new(self):
        im = Image.new('L', (W * SS, H * SS), 0)
        return im, ImageDraw.Draw(im)

    def poly(self, name, mat, pts):
        im, d = self._new(); d.polygon(P(pts), fill=255)
        self.parts.append((name, mat, np.array(im) > 127))

    def rrect(self, name, mat, x0, y0, x1, y1, r):
        im, d = self._new(); d.rounded_rectangle([x0 * SS, y0 * SS, x1 * SS, y1 * SS], r * SS, fill=255)
        self.parts.append((name, mat, np.array(im) > 127))

    def stroke(self, name, mat, pts, width):
        im, d = self._new()
        d.line(P(pts), fill=255, width=int(round(width * SS)), joint='curve')
        r = width * SS / 2
        for x, y in P([pts[0], pts[-1]]):
            d.ellipse([x - r, y - r, x + r, y + r], fill=255)
        self.parts.append((name, mat, np.array(im) > 127))

    def detail(self, pts, width, lu, ls):
        self.lines.append((pts, width, lu, ls))


def band(mask, r_px):
    """Inner boundary band of a hi-res mask, r_px final pixels wide."""
    k = int(round(r_px * SS)) * 2 + 1
    er = np.array(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(k))) > 127
    return mask & ~er


def shade(mat, mask, sel):
    """Interior level for a part (hi-res)."""
    h, w = mask.shape
    ys, xs = np.nonzero(mask)
    yy = np.arange(h)[:, None].repeat(w, 1).astype(np.float32)
    xx = np.arange(w)[None, :].repeat(h, 0).astype(np.float32)
    y0, y1 = ys.min(), ys.max() + 1
    t = np.clip((yy - y0) / max(1, (y1 - y0)), 0, 1)    # 0 top .. 1 bottom
    if sel:
        base = {'metal': 255.0, 'wood': 238.0, 'grip': 228.0}[mat]
        out = np.full(mask.shape, base, np.float32)
        if mat == 'grip':
            stripes = ((xx + yy * 0.9) // (2.2 * SS)) % 2 == 0
            out[stripes] = 196
        return out
    if mat == 'metal':
        out = 150 - 60 * t
        out = out + 70 * np.exp(-((t - 0.22) / 0.09) ** 2)   # specular streak
    elif mat == 'wood':
        out = 92 - 28 * t
    else:  # grip wrap
        out = np.full(mask.shape, 70.0, np.float32)
        stripes = ((xx + yy * 0.9) // (2.2 * SS)) % 2 == 0
        out[stripes] = 150
    return out.astype(np.float32)


def render(icon, sel):
    sil = np.zeros((H * SS, W * SS), bool)
    val = np.zeros((H * SS, W * SS), np.float32)
    owner = np.full((H * SS, W * SS), -1, np.int32)
    for k, (name, mat, m) in enumerate(icon.parts):   # later parts paint over earlier ones
        val[m] = shade(mat, m, sel)[m]
        owner[m] = k
        sil |= m
    for k, (name, mat, m) in enumerate(icon.parts):   # part boundaries, visible portion only
        b = band(m, 0.55) & (owner == k)
        # also outline where a later part covers this one (the seam), seen from the covering part
        val[b] = 205 if not sel else 190
    # detail strokes
    if icon.lines:
        for pts, wd, lu, ls in icon.lines:
            im = Image.new('L', (W * SS, H * SS), 0)
            ImageDraw.Draw(im).line(P(pts), fill=255, width=max(1, int(round(wd * SS))), joint='curve')
            m = (np.array(im) > 127) & sil
            val[m] = lu if not sel else ls
    # silhouette outline
    ob = band(sil, 1.15)
    val[ob] = 238 if not sel else 255
    val[~sil] = 0
    # downsample: premultiplied value + coverage
    def down(a):
        return np.array(Image.fromarray(a.astype(np.float32), 'F').resize((W, H), Image.BOX))
    cov = down(sil.astype(np.float32))
    pv = down(val * sil)
    return cov, pv


def panel():
    bg = np.zeros((H, W), np.float32)
    bg[0::2] = BG_EVEN
    bg[1::2] = BG_ODD
    return bg


def compose(icon, sel):
    cov, pv = render(icon, sel)
    bg = panel()
    if sel:
        g = Image.fromarray((cov * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.6))
        glow = np.array(g).astype(np.float32) / 255.0
        glow = np.clip(glow * 2.2, 0, 1) ** 1.15 * 128
        under = np.maximum(bg, glow)
    else:
        under = bg
    out = pv + under * (1 - cov)
    return np.clip(np.round(out), 0, 255).astype(np.uint8)


# ---------------------------------------------------------------- shapes

def pickaxe():
    ic = Icon()
    # handle (wood), slight taper, rounded far end
    ic.poly('handle', 'wood', [(22, 19.9), (150, 19.3), (163.5, 19.5), (165.5, 21), (165.5, 24), (163.5, 25.5),
                               (150, 25.7), (22, 25.1)])
    ic.rrect('grip', 'grip', 134, 18.5, 163, 26.5, 2.2)
    ic.detail([(38, 22.5), (128, 22.5)], 0.55, 118, 214)       # grain
    # crescent head (metal): convex away from the handle, points sweep back toward it
    pts_l, pts_r = [], []
    N = 60
    for i in range(N + 1):
        t = -1 + 2 * i / N
        cx, cy = 20.5 + 19.0 * abs(t) ** 2.2, 22.5 + 21.3 * t
        dx = 19.0 * 2.2 * abs(t) ** 1.2 * (1 if t >= 0 else -1)
        dy = 21.3
        L = math.hypot(dx, dy)
        nx, ny = dy / L, -dx / L
        h = 6.2 * (1 - abs(t) ** 1.35) + 0.3
        pts_l.append((cx - nx * h, cy - ny * h))
        pts_r.append((cx + nx * h, cy + ny * h))
    ic.poly('head', 'metal', pts_l + pts_r[::-1])
    # eye collar wrapping the handle
    ic.rrect('collar', 'metal', 23, 18.0, 31, 27.0, 1.4)
    # edge highlights along the pick arms
    ic.detail([(17.2, 20), (17.6, 13), (20.5, 6)], 0.5, 220, 205)
    ic.detail([(17.2, 25), (17.6, 32), (20.5, 39)], 0.5, 220, 205)
    return ic


def shovel():
    ic = Icon()
    # shaft
    ic.poly('shaft', 'wood', [(60, 20.1), (142, 19.9), (142, 25.1), (60, 24.9)])
    ic.detail([(70, 22.5), (134, 22.5)], 0.55, 118, 214)
    # D-grip: two straps + crossbar
    ic.stroke('strap1', 'wood', [(140, 21.2), (148, 16.5), (156.5, 12.2)], 3.0)
    ic.stroke('strap2', 'wood', [(140, 23.8), (148, 28.5), (156.5, 32.8)], 3.0)
    ic.rrect('bar', 'grip', 154.5, 7.5, 163.5, 37.5, 3.2)
    ic.rrect('ferrule', 'metal', 134, 18.8, 143, 26.2, 1.4)
    # blade (round point), tip at left
    top, bot = [], []
    for i in range(0, 61):
        x = 3 + (50 - 3) * i / 60
        u = max(0.0, (27 - x) / 24)
        w = 14.8 * (1 - u ** 1.7)
        if x > 47.5:
            w -= (x - 47.5) * 0.9
        top.append((x, 22.5 - w)); bot.append((x, 22.5 + w))
    ic.poly('blade', 'metal', top + bot[::-1])
    # socket / neck, tapering into the shaft
    ic.poly('neck', 'metal', [(44, 17.0), (52, 18.2), (66, 19.6), (66, 25.4), (52, 26.8), (44, 28.0)])
    # blade details: centre spine + turned step
    ic.detail([(14, 22.5), (44, 22.5)], 0.6, 215, 205)
    ic.detail([(44.5, 9.5), (44.5, 35.5)], 0.55, 200, 205)
    return ic


def axe():
    ic = Icon()
    hy = 30.5
    # handle with gentle curve and swelled knob at the far end
    top, bot = [], []
    N = 60
    for i in range(N + 1):
        x = 22 + (166 - 22) * i / N
        c = hy + 1.4 * math.sin((x - 22) / 144 * math.pi * 1.3)
        h = 2.9 + 0.3 * (x - 22) / 144
        if x > 150:
            h += 1.7 * ((x - 150) / 16) ** 2
        top.append((x, c - h)); bot.append((x, c + h))
    ic.poly('handle', 'wood', top + bot[::-1])
    ic.detail([(40, hy - 0.4), (146, hy + 0.9)], 0.55, 118, 214)
    # blade: back edge (away from handle) nearly straight, beard sweeps toward the handle,
    # cutting edge is a convex arc at the top
    L, R = [], []
    for i in range(25):
        s = i / 24                       # 0 at cheek, 1 at bit
        y = 24.5 - 21.0 * s
        xl = 14.0 - 4.0 * s ** 2.0       # back: slight flare
        xr = 30.0 + 22.0 * s ** 2.6      # beard: strong concave sweep
        L.append((xl, y)); R.append((xr, y))
    arc = []
    for i in range(1, 24):
        a = i / 24
        x = L[-1][0] + (R[-1][0] - L[-1][0]) * a
        arc.append((x, 3.5 - 3.0 * math.sin(math.pi * a) + 1.6 * a))
    R[-1] = (R[-1][0], R[-1][1] + 1.6)
    ic.poly('blade', 'metal', L + arc + R[::-1])
    # cheek/eye block and poll (hammer end)
    ic.rrect('cheek', 'metal', 12.5, 23.5, 31.0, 37.5, 1.6)
    ic.rrect('poll', 'metal', 13.5, 36.0, 26.5, 43.2, 1.2)
    # bevel line parallel to the bit
    bev = []
    for i in range(0, 25):
        a = i / 24
        xl, xr = 10.8, 46.5
        x = xl + (xr - xl) * a
        bev.append((x, 8.4 - 3.0 * math.sin(math.pi * a) + 1.8 * a))
    ic.detail(bev, 0.6, 215, 205)
    return ic


ICONS = [('pickaxe', pickaxe), ('shovel', shovel), ('axe', axe)]


def build():
    sheets = {}
    for sel in (False, True):
        sheet = np.zeros((SHEET_H, SHEET_W), np.uint8)
        for k, (name, fn) in enumerate(ICONS):
            sheet[k * 45:(k + 1) * 45, 0:170] = compose(fn(), sel)
        sheets[sel] = sheet
    pal = []
    for i in range(256):
        pal += [i, i, i]
    for sel, name in ((False, 'sctools'), (True, 'sctools_s')):
        im = Image.fromarray(sheets[sel], 'P'); im.putpalette(pal)
        im.save(name + '.bmp')
        Image.fromarray(sheets[sel]).resize((SHEET_W * 3, SHEET_H * 3), Image.NEAREST).save('_' + name + '_x3.png')
    print('tool sheets written')


if __name__ == '__main__':
    build()
