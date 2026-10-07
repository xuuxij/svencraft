"""Parse every produced .spr with sprparse, check invariants, render all frames into _preview.png."""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from sprparse import parse, render

SRC = sys.argv[1] if len(sys.argv) > 1 else '.'
REF = 'C:/Program Files (x86)/Steam/steamapps/common/Svencraft Coop/svencoop/sprites'
NAMES = ['sctools', 'sctools_s', 'blockicons', 'slot', 'slot_sel']
MATS = ['grass', 'dirt', 'stone', 'cobble', 'brick', 'planks', 'log', 'leaves', 'sand', 'gravel', 'snow',
        'metal', 'concrete', 'tile', 'rubber', 'glass', 'circuit', 'vent']
font = ImageFont.load_default()
S = {n: parse(os.path.join(SRC, n + '.spr')) for n in NAMES}

# ---- invariants
for n in ('sctools', 'sctools_s'):
    s = S[n]
    assert s['typename'] == 'vp_parallel' and s['texname'] == 'additive' and s['nframes'] == 1
    assert (s['pal'] == np.arange(256)[:, None]).all(), 'greyscale identity palette'
    px = s['frames'][0]['px']
    assert px.shape == (136, 256)
    assert (px[:, 170:] == 0).all() and (px[135:, :] == 0).all(), 'outside cells must be black'
for n in ('blockicons', 'slot', 'slot_sel'):
    s = S[n]
    assert s['typename'] == 'vp_parallel' and s['texname'] == 'alphatest'
    assert tuple(s['pal'][255]) == (0, 0, 255)
    for f in s['frames']:
        t = f['px'] == 255
        assert t[0, :].all() if n == 'blockicons' else True
assert S['blockicons']['nframes'] == 18 and all(f['w'] == 48 and f['h'] == 48 for f in S['blockicons']['frames'])
for n, w in (('slot', 2), ('slot_sel', 3)):
    px = S[n]['frames'][0]['px']
    assert px.shape == (56, 56)
    ring = np.zeros((56, 56), bool); ring[:w] = ring[-w:] = True; ring[:, :w] = ring[:, -w:] = True
    assert (px[ring] != 255).all() and (px[~ring] == 255).all()
    print(n, 'outline colour', tuple(S[n]['pal'][px[0, 0]]), 'width', w)
for i, f in enumerate(S['blockicons']['frames']):
    op = f['px'] != 255
    ys, xs = np.nonzero(op)
    print(f"block {i:2d} {MATS[i]:9s} opaque={op.sum():4d} bbox x{xs.min()}-{xs.max()} y{ys.min()}-{ys.max()}")
print('invariants OK')

# ---- contact sheet
def lab(d, xy, t, c=(230, 230, 230)):
    d.text(xy, t, fill=c, font=font)

def checker(w, h, a=(58, 62, 70), b=(78, 82, 90), cs=8):
    y, x = np.mgrid[0:h, 0:w]
    m = ((x // cs + y // cs) % 2 == 0)[..., None]
    return Image.fromarray(np.where(m, np.array(a, np.uint8), np.array(b, np.uint8)).astype(np.uint8), 'RGB')

def masked(spr, fi, bg):
    f = spr['frames'][fi]
    rgb = Image.fromarray(spr['pal'][f['px']], 'RGB')
    out = bg.copy(); out.paste(rgb, (0, 0), Image.fromarray(((f['px'] != 255) * 255).astype(np.uint8)))
    return out

W = 1100
sheet = Image.new('RGB', (W, 1340), (24, 26, 30))
d = ImageDraw.Draw(sheet)
y = 8
lab(d, (8, y), 'sctools.spr / sctools_s.spr  (additive, 256x136, 1 frame; cells 170x45 at y=0,45,90)  -- raw greyscale, 2x'); y += 16
ref1, ref4 = parse(REF + '/640hud1.spr'), parse(REF + '/640hud4.spr')
for k, nm in enumerate(['pickaxe y=0', 'shovel y=45', 'axe y=90']):
    a = render(S['sctools'], 0, 'additive').crop((0, k * 45, 170, k * 45 + 45)).resize((340, 90), Image.NEAREST)
    b = render(S['sctools_s'], 0, 'additive').crop((0, k * 45, 170, k * 45 + 45)).resize((340, 90), Image.NEAREST)
    sheet.paste(a, (8, y)); sheet.paste(b, (358, y)); lab(d, (710, y + 40), nm); y += 94
lab(d, (8, y), 'stock crowbar for comparison (640hud1 / 640hud4)'); y += 14
sheet.paste(render(ref1, 0, 'additive').crop((0, 0, 170, 45)).resize((340, 90), Image.NEAREST), (8, y))
sheet.paste(render(ref4, 0, 'additive').crop((0, 0, 170, 45)).resize((340, 90), Image.NEAREST), (358, y)); y += 98

lab(d, (8, y), 'as drawn in-game: additive, tinted by HUD colour (HL orange 255,160,0 and blue 100,130,200), 1x; full sheet incl. padding'); y += 16
x = 8
for tint in ((255, 160, 0), (100, 130, 200)):
    for n in ('sctools', 'sctools_s'):
        im = render(S[n], 0, 'additive', tint, bg=(10, 12, 16))
        sheet.paste(im, (x, y)); x += 262
y += 142

lab(d, (8, y), 'blockicons.spr  (alphatest, 18 frames 48x48)  -- 2x over checker (transparent = index 255)'); y += 16
for i in range(18):
    r, c = divmod(i, 9)
    bx, by = 8 + c * 120, y + r * 122
    im = masked(S['blockicons'], i, checker(48, 48, cs=4)).resize((96, 96), Image.NEAREST)
    sheet.paste(im, (bx, by)); lab(d, (bx, by + 98), f'{i} {MATS[i]}')
y += 2 * 122 + 6

lab(d, (8, y), 'blockicons 1x over light and dark backgrounds'); y += 14
for j, bgc in enumerate(((200, 205, 210), (20, 22, 26))):
    for i in range(18):
        im = masked(S['blockicons'], i, Image.new('RGB', (48, 48), bgc))
        sheet.paste(im, (8 + i * 52, y))
    y += 52
y += 6

lab(d, (8, y), 'slot.spr / slot_sel.spr (alphatest, 56x56) 2x, then a mock 9-slot hotbar at 1x and 2x (icon centred, 4px inset)'); y += 16
sheet.paste(masked(S['slot'], 0, checker(56, 56, cs=4)).resize((112, 112), Image.NEAREST), (8, y))
sheet.paste(masked(S['slot_sel'], 0, checker(56, 56, cs=4)).resize((112, 112), Image.NEAREST), (130, y))
# mock hotbar
scene = Image.fromarray(np.tile(np.linspace(40, 140, 9 * 58, dtype=np.uint8)[None, :, None], (60, 1, 3)), 'RGB')
for i in range(9):
    sx = i * 58 + 1
    tile = scene.crop((sx, 2, sx + 56, 58))
    base = tile.copy()
    icon = S['blockicons']['frames'][[0, 2, 3, 5, 6, 7, 15, 11, 16][i]]
    ic = Image.fromarray(S['blockicons']['pal'][icon['px']], 'RGB')
    base.paste(ic, (4, 4), Image.fromarray(((icon['px'] != 255) * 255).astype(np.uint8)))
    sl = S['slot_sel' if i == 2 else 'slot']['frames'][0]
    base.paste(Image.fromarray(S['slot_sel' if i == 2 else 'slot']['pal'][sl['px']], 'RGB'), (0, 0),
               Image.fromarray(((sl['px'] != 255) * 255).astype(np.uint8)))
    scene.paste(base, (sx, 2))
sheet.paste(scene, (260, y)); y += 128
big = scene.resize((scene.width * 2, 120), Image.NEAREST).crop((0, 0, min(W - 16, scene.width * 2), 120))
sheet.paste(big, (8, y)); y += 126
sheet = sheet.crop((0, 0, W, y))
sheet.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), '_preview.png'))
print('preview', sheet.size)
for n in NAMES:
    s = S[n]
    print(f"{n}.spr  {s['size']} bytes  {s['typename']}/{s['texname']}  {s['nframes']} frame(s) "
          f"{s['frames'][0]['w']}x{s['frames'][0]['h']}")
