# Procedural 16x16 pixel-art block textures (original art), upscaled 4x to 64x64 8-bit BMPs.
import numpy as np
from PIL import Image
rng = np.random.default_rng(1337)
S = 16

def noise(base, var, n=S):
    b = np.array(base, float)
    return np.clip(b + rng.normal(0, var, (n, n, 1)) * np.array([1, 1, 1]), 0, 255)

def speckle(img, color, p):
    m = rng.random((S, S)) < p
    img[m] = color
    return img

def dirt():
    img = noise((134, 96, 67), 14)
    speckle(img, (100, 70, 48), 0.12); speckle(img, (160, 120, 85), 0.06)
    return img

def grass_top():
    img = noise((95, 159, 53), 16); speckle(img, (70, 125, 40), 0.15); speckle(img, (120, 180, 70), 0.08)
    return img

def grass_side():
    img = dirt()
    top = noise((95, 159, 53), 14)
    depth = 3 + (rng.random(S) < 0.5).astype(int)
    for x in range(S):
        img[:depth[x], x] = top[:depth[x], x]
    return img

def stone():
    img = noise((125, 125, 125), 10); speckle(img, (100, 100, 100), 0.12); speckle(img, (145, 145, 145), 0.06)
    return img

def cobble():
    img = noise((115, 115, 115), 8)
    # irregular stones with dark mortar outlines
    for _ in range(9):
        cx, cy, r = rng.integers(0, S), rng.integers(0, S), rng.integers(2, 4)
        shade = rng.integers(95, 150)
        for y in range(S):
            for x in range(S):
                dx = min(abs(x - cx), S - abs(x - cx)); dy = min(abs(y - cy), S - abs(y - cy))
                dd = dx * dx + dy * dy
                if dd <= r * r: img[y, x] = (shade,) * 3
                elif dd <= (r + 1) ** 2: img[y, x] = (70, 70, 70)
    return np.clip(img + rng.normal(0, 5, (S, S, 1)), 0, 255)

def brick():
    img = np.zeros((S, S, 3)); img[:] = (180, 175, 165)  # mortar
    for row in range(4):
        off = 0 if row % 2 == 0 else 4
        for b in range(-1, 3):
            x0 = b * 8 + off
            col = np.array((150, 70, 55)) + rng.normal(0, 10, 3)
            for y in range(row * 4, row * 4 + 3):
                for x in range(x0, x0 + 7):
                    if 0 <= x < S: img[y, x] = col + rng.normal(0, 6, 3)
    return np.clip(img, 0, 255)

def planks():
    img = noise((162, 130, 78), 6)
    for row in range(4):
        y0 = row * 4
        img[y0 + 3, :] = (110, 85, 50)
        seam = (row * 5 + 3) % S
        img[y0:y0 + 3, seam] = (120, 95, 58)
        img[y0:y0 + 3] += rng.normal(0, 8)
    return np.clip(img, 0, 255)

def log_side():
    img = noise((104, 82, 51), 6)
    for x in range(S):
        if rng.random() < 0.35: img[:, x] *= 0.8
    return np.clip(img, 0, 255)

def log_top():
    img = np.zeros((S, S, 3))
    for y in range(S):
        for x in range(S):
            r = np.hypot(x - 7.5, y - 7.5)
            img[y, x] = (104, 82, 51) if r > 6.8 else ((170, 140, 85) if int(r) % 2 else (150, 120, 70))
    return np.clip(img + rng.normal(0, 5, (S, S, 1)), 0, 255)

def leaves():
    img = noise((60, 120, 40), 18); speckle(img, (35, 80, 25), 0.2)
    holes = rng.random((S, S)) < 0.18
    return img, holes

def sand():
    img = noise((219, 207, 160), 7); speckle(img, (200, 185, 135), 0.1)
    return img

def gravel():
    img = noise((130, 124, 122), 22); speckle(img, (90, 85, 85), 0.15); speckle(img, (170, 160, 155), 0.08)
    return img

def snow():
    img = noise((240, 248, 250), 4); speckle(img, (220, 230, 240), 0.08)
    return img

def metal():
    img = noise((200, 200, 205), 4)
    img[0, :] = img[:, 0] = (230, 230, 235); img[S - 1, :] = img[:, S - 1] = (150, 150, 155)
    for (y, x) in [(2, 2), (2, 13), (13, 2), (13, 13)]: img[y, x] = (120, 120, 125)
    return img

def concrete():
    img = noise((160, 160, 155), 5); speckle(img, (140, 140, 135), 0.05)
    return img

def tile():
    img = noise((215, 215, 210), 3)
    img[7, :] = img[:, 7] = img[15, :] = img[:, 15] = (150, 150, 145)
    return img

def rubber():
    img = noise((40, 40, 42), 5)
    for y in range(0, S, 4): img[y, :] = (28, 28, 30)
    return img

def glass():
    img = np.zeros((S, S, 3)); img[:] = (200, 230, 240)
    holes = np.ones((S, S), bool)
    holes[0, :] = holes[S - 1, :] = holes[:, 0] = holes[:, S - 1] = False
    for i in range(3, 7): holes[i, i + 2] = False; img[i, i + 2] = (240, 250, 255)
    img[0, :] = img[S - 1, :] = img[:, 0] = img[:, S - 1] = (170, 205, 220)
    return img, holes

def circuit():
    img = noise((40, 90, 50), 6)
    for _ in range(10):
        y, x = rng.integers(0, S, 2); L = rng.integers(3, 8)
        if rng.random() < 0.5: img[y, x:x + L] = (200, 170, 60)
        else: img[y:y + L, x] = (200, 170, 60)
    for _ in range(4):
        y, x = rng.integers(1, S - 3, 2); img[y:y + 3, x:x + 3] = (25, 25, 25)
    return img

def vent():
    img = noise((110, 115, 120), 4)
    for y in range(1, S, 3): img[y, 1:S - 1] = (45, 48, 52)
    return img

TEX = {
    'dirt': dirt(), 'grass_top': grass_top(), 'grass_side': grass_side(), 'stone': stone(),
    'cobble': cobble(), 'brick': brick(), 'planks': planks(), 'log_side': log_side(), 'log_top': log_top(),
    'leaves': leaves(), 'sand': sand(), 'gravel': gravel(), 'snow': snow(), 'metal': metal(),
    'concrete': concrete(), 'tile': tile(), 'rubber': rubber(), 'glass': glass(), 'circuit': circuit(), 'vent': vent(),
}

# Added later: generated after the originals so their random patterns stay identical.
def bench_top():
    img = planks()
    img[0, :] = img[S - 1, :] = img[:, 0] = img[:, S - 1] = (80, 55, 30)       # frame
    for g in (5, 10):
        img[g, 1:S - 1] = (95, 68, 40); img[1:S - 1, g] = (95, 68, 40)         # 3x3 crafting grid
    return np.clip(img, 0, 255)

def bench_side():
    img = planks()
    img[0:2, :] = (120, 88, 50)                                                 # table top edge
    img[3, 3:12] = (150, 150, 155); img[4, 3:11] = (130, 130, 135)              # saw blade
    img[4:7, 2] = (90, 60, 35); img[3:6, 1] = (90, 60, 35)                      # saw handle
    img[9, 9:14] = (140, 140, 145); img[9:14, 11] = (100, 70, 40)               # hammer
    return np.clip(img, 0, 255)

TEX['bench_top'] = bench_top()
TEX['bench_side'] = bench_side()

# Ores (added later; generated after the others so earlier textures keep their random patterns).
def ore(base, fleck, edge, clusters, size):
    img = base.copy()
    centres = [(3, 4), (4, 11), (9, 7), (12, 3), (12, 12)][:clusters]   # spread out like Minecraft ore spots
    for cy0, cx0 in centres:
        cy, cx = cy0 + rng.integers(-1, 2), cx0 + rng.integers(-1, 2)
        for _ in range(size):
            y, x = cy + rng.integers(-1, 2), cx + rng.integers(-1, 2)
            img[y, x] = fleck
            for dy, dx in ((1, 0), (0, 1)):
                if 0 <= y + dy < S and 0 <= x + dx < S and tuple(img[y + dy, x + dx]) != tuple(fleck):
                    img[y + dy, x + dx] = edge
    return np.clip(img, 0, 255)

TEX['iron_ore'] = ore(stone(), np.array((216, 175, 142.0)), np.array((150, 110, 80.0)), 5, 4)
TEX['crystal_ore'] = ore(stone(), np.array((120, 245, 225.0)), np.array((40, 150, 140.0)), 4, 5)

for name, t in TEX.items():
    holes = None
    if isinstance(t, tuple): t, holes = t
    rgb = Image.fromarray(np.clip(t, 0, 255).astype(np.uint8), 'RGB')
    if holes is None:
        pal = rgb.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    else:
        # masked texture: palette index 255 must be the transparent color (blue by convention)
        pal = rgb.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
        idx = np.array(pal); idx[holes] = 255
        p = pal.getpalette()[:765] + [0, 0, 255]
        pal = Image.fromarray(idx.astype(np.uint8), 'P'); pal.putpalette(p)
    pal = pal.resize((64, 64), Image.NEAREST)
    pal.save(f'tex/{name}.bmp')
    rgb.resize((128, 128), Image.NEAREST).save(f'tex/_preview_{name}.png')
print('wrote', len(TEX), 'textures')
