"""Svencraft crafting-window sprites (all vp_parallel, alphatest: palette index 255 = transparent key).

  craftslots.spr    90 frames of 56x56
                    frame k      (0..44) = normal slot with icon k
                    frame 45 + k (0..44) = selected slot with icon k
                    icon ids: 0..18 block cubes (make_blocks.MATERIALS order), 19..27 tools, 28..35 weapons,
                              36..43 supplies, 44 = empty slot (no icon)  -- see ICON_NAMES
  craftpanel_l.spr  1 frame 256x232 = panel x 0..255   of the 400x232 crafting panel
  craftpanel_r.spr  1 frame 144x232 = panel x 256..399

Panel layout (panel x = screen x + 200, panel y = screen y + 116, screen offsets relative to the screen centre):
  recipe grid   3x3 slots, centres x -164/-108/-52, y -64/-8/+48  -> slot area panel x 8..176,  y 24..192
  ingredients   1x3 slots, centre  x +12,           y -64/-8/+48  -> slot area panel x 184..240, y 24..192
  arrow         centre (+76, -8)   -> panel (276, 108)
  result slot   centre (+132, -8)  -> panel (332, 108)   (no recess, the slot frame is drawn on the plain panel)

Block icons reuse make_blocks.render_cube (imported: make_blocks has no import-time side effects).
slot_base / with_icon / write_sprite are copied from make_hotbar.py (which must not be run or imported: it
rebuilds the installed hotbar sprites at import time). write_sprite here keeps the slot-frame and pixel-art
colours exact in the shared palette and only quantizes the textured cube colours.

Item icons (19..43) are original 16x16 pixel art defined below as character grids, outlined and shaded in
code, scaled x3 (nearest) to 48x48 and centred in the 56x56 slot.
"""
import os, sys, shutil, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import make_blocks as mb
import sprparse

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
SPRGEN = r"C:\Program Files (x86)\Steam\steamapps\common\Sven Co-op SDK\sprites\sprgen.exe"
INSTALL = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\sprites\svencraft"
S = 56          # slot size
PER_SHEET = 16  # 4x4 frames of 56 per source bmp (224x224)
NICON = None   # set from ICON_NAMES below (blocks + 9 tools + 8 weapons + 8 supplies + empty)
EMPTY = None

# ---------------------------------------------------------------- copied from make_hotbar.py

def slot_base(selected):
    rgb = np.zeros((S, S, 3), np.float32)
    op = np.ones((S, S), bool)
    if selected:
        rgb[:] = (20, 20, 20)                 # 1px dark outer edge
        rgb[1:-1, 1:-1] = (235, 235, 235)     # 3px white frame
        rgb[4:-4, 4:-4] = (88, 88, 88)        # lighter fill
    else:
        rgb[:] = (24, 24, 24)
        rgb[1:-1, 1:-1] = (120, 120, 120)     # 2px grey border, bevelled
        rgb[1:3, 1:-1] = (150, 150, 150)
        rgb[1:-1, 1:3] = (150, 150, 150)
        rgb[3:-3, 3:-3] = (58, 58, 58)
        rgb[3:5, 3:-3] = (46, 46, 46)          # inner shadow
        rgb[3:-3, 3:5] = (46, 46, 46)
    return rgb, op


def with_icon(base, icon_rgb, icon_op):
    rgb, op = base[0].copy(), base[1].copy()
    o = (S - mb.FS) // 2
    sub = rgb[o:o + mb.FS, o:o + mb.FS]
    sub[icon_op] = icon_rgb[icon_op]
    return rgb, op


def quantize_keep_exact(rgb, op, exact):
    """Like make_blocks.quantize_alphatest, but every colour under `exact` gets its own palette entry and the
    remaining entries are fitted (median cut + weighted k-means, exact entries frozen) to the other pixels."""
    rgb = rgb.astype(np.uint8)
    fixed = np.unique(rgb[op & exact].reshape(-1, 3), axis=0)
    other = op & ~exact
    ocols, ocnt = (np.unique(rgb[other].reshape(-1, 3), axis=0, return_counts=True) if other.any()
                   else (np.zeros((0, 3), np.uint8), np.zeros(0, int)))
    nfree = 255 - len(fixed)
    assert nfree >= 0, len(fixed)
    if len(ocols) <= nfree:
        free = ocols.astype(np.float64)
    else:
        strip = np.repeat(ocols, np.minimum(ocnt, 64), axis=0)[None]       # cap weights for the median-cut seed
        q = Image.fromarray(strip, 'RGB').quantize(colors=nfree, method=Image.Quantize.MEDIANCUT,
                                                   dither=Image.Dither.NONE)
        free = np.array(q.getpalette()[:nfree * 3], np.float64).reshape(-1, 3)
        fx = fixed.astype(np.float64)
        cols = ocols.astype(np.float64)
        for _ in range(40):
            cent = np.concatenate([fx, free])
            d = ((cols[:, None, :] - cent[None, :, :]) ** 2).sum(-1)
            a = d.argmin(1) - len(fx)
            for k in range(len(free)):
                sel = a == k
                if sel.any():
                    free[k] = (cols[sel] * ocnt[sel, None]).sum(0) / ocnt[sel].sum()
                else:
                    worst = d[np.arange(len(cols)), d.argmin(1)].argmax()
                    free[k] = cols[worst]
        free = np.clip(np.round(free), 0, 255)
    pal = np.zeros((256, 3), np.uint8)
    cent = np.concatenate([fixed.astype(np.float64), free])
    pal[:len(cent)] = cent.astype(np.uint8)
    flat = rgb.reshape(-1, 3).astype(np.float64)
    idx = np.zeros(len(flat), np.uint8)
    for s0 in range(0, len(flat), 8192):
        blk = flat[s0:s0 + 8192]
        idx[s0:s0 + 8192] = ((blk[:, None, :] - cent[None, :, :]) ** 2).sum(-1).argmin(1)
    idx = idx.reshape(rgb.shape[:2])
    idx[~op] = 255
    pal[255] = (0, 0, 255)
    assert idx[op].max() <= 254
    assert (pal[idx][op & exact] == rgb[op & exact]).all(), 'exact colours must survive'
    err = np.abs(pal[idx].astype(int) - rgb.astype(int))[other]
    return idx, pal, len(fixed), len(ocols), (err.mean() if err.size else 0.0), (err.max() if err.size else 0)


def write_sprite(name, frames):
    """frames: list of (rgb, opaque, exact). Splits into 224x224 source bmps (sprgen frame limits) sharing one
    palette, then runs sprgen."""
    sheets = [frames[i:i + PER_SHEET] for i in range(0, len(frames), PER_SHEET)]
    atlas_rgb = np.concatenate([f[0] for f in frames], 0)
    atlas_op = np.concatenate([f[1] for f in frames], 0)
    atlas_ex = np.concatenate([f[2] for f in frames], 0)
    idx, pal, nfix, noth, emean, emax = quantize_keep_exact(atlas_rgb, atlas_op, atlas_ex)
    print(f'{name}: {nfix} exact colours, {noth} textured colours -> {255 - nfix} entries '
          f'(mean abs err {emean:.2f}, max {emax})')
    qc = ['$spritename %s' % name, '$type vp_parallel', '$texture alphatest']
    for si, sh in enumerate(sheets):
        sheet = np.full((4 * S, 4 * S), 255, np.uint8)
        for k in range(len(sh)):
            fi = si * PER_SHEET + k
            r, c = divmod(k, 4)
            sheet[r * S:(r + 1) * S, c * S:(c + 1) * S] = idx[fi * S:(fi + 1) * S]
        bmp = '%s_%d.bmp' % (name, si)
        mb.save_bmp(sheet, pal, bmp)
        qc.append('$load %s' % bmp)
        for k in range(len(sh)):
            r, c = divmod(k, 4)
            qc.append('$frame %d %d %d %d' % (c * S, r * S, S, S))
    run_sprgen(name, qc)


def run_sprgen(name, qc):
    open(name + '.qc', 'w', newline='\n').write('\n'.join(qc) + '\n')
    if os.path.exists(name + '.spr'):
        os.remove(name + '.spr')
    out = subprocess.run([SPRGEN, name + '.qc'], capture_output=True, text=True)
    assert os.path.exists(name + '.spr'), out.stdout + out.stderr
    print(name, os.path.getsize(name + '.spr'), 'bytes')

# ---------------------------------------------------------------- item pixel art

OUTLINE = (24, 21, 19)
# auto-shaded materials: char -> (highlight, base, shadow). Light comes from the top left.
MAT = {
    'W': ((140, 104, 56), (112, 82, 44), (82, 58, 30)),        # tool handle wood
    'R': ((242, 116, 60), (208, 66, 32), (148, 38, 22)),       # crowbar paint
    'S': ((160, 162, 170), (120, 122, 130), (84, 86, 94)),     # gunmetal
    'P': ((104, 104, 110), (74, 74, 80), (50, 50, 56)),        # black polymer / blued steel
    'C': ((238, 238, 244), (194, 194, 204), (140, 140, 152)),  # chrome / nickel
    'D': ((156, 102, 54), (122, 78, 40), (86, 54, 26)),        # dark wood (grips, stocks)
    'G': ((136, 158, 80), (102, 122, 58), (70, 86, 40)),       # olive drab
    'Y': ((246, 214, 112), (214, 172, 66), (162, 124, 42)),    # brass
    'O': ((228, 152, 98), (190, 112, 64), (138, 76, 42)),      # copper
    'E': ((234, 78, 66), (192, 38, 34), (134, 24, 24)),        # red shell hull
    'B': ((100, 164, 255), (42, 102, 222), (26, 62, 152)),     # HEV battery blue
    'X': ((255, 255, 255), (232, 232, 234), (190, 190, 198)),  # white case
}
TIER = {
    'wood':  ((182, 140, 86), (150, 112, 64), (112, 82, 44)),
    'stone': ((162, 162, 162), (128, 128, 128), (94, 94, 94)),
    'iron':  ((242, 242, 246), (215, 215, 220), (166, 166, 176)),
}
# flat colours
FLAT = {
    'k': OUTLINE,
    'd': (52, 50, 54),        # dark detail inside a part
    'y': (252, 214, 44),      # yellow
    'z': (206, 150, 22),      # dark yellow
    'r': (214, 40, 36),       # red
    'q': (150, 18, 18),       # dark red
    'w': (250, 250, 250),     # white
    's': (226, 220, 196),     # bow string / light line (drawn without an outline)
    'g': (176, 178, 186),     # light grey detail
    'n': (66, 80, 36),        # dark olive groove
    'o': (196, 198, 206),     # thin ring (drawn without an outline)
}
NO_OUTLINE = {'s', 'o'}

TOOL_GRIDS = {
    'pickaxe': [
        "................",
        "....HHHHHHH.....",
        "..HHHHHHHHHHH...",
        ".HH......HHHHH..",
        "..........HHHH..",
        "..........WHHHH.",
        ".........WW.HHH.",
        "........WW...HH.",
        ".......WW....HH.",
        "......WW.....HH.",
        ".....WW......HH.",
        "....WW.......HH.",
        "...WW........H..",
        "..WW........HH..",
        "..W.........H...",
        "................"],
    'shovel': [
        "................",
        "...........HHH..",
        "..........HHHHH.",
        ".........HHHHHH.",
        "........HHHHHHH.",
        "........HHHHHH..",
        ".........HHHH...",
        "........WWHH....",
        ".......WW.......",
        "......WW........",
        ".....WW.........",
        "....WW..........",
        "...WW...........",
        "..WW............",
        "..W.............",
        "................"],
    'axe': [
        "................",
        ".......H........",
        "......HHH.......",
        ".....HHHHH..W...",
        "....HHHHHHHWW...",
        "...HHHHHHHWW....",
        "..HHHHHHHWW.....",
        "....HHHHWW......",
        ".......WW.......",
        "......WW........",
        ".....WW.........",
        "....WW..........",
        "...WW...........",
        "..WW............",
        "..W.............",
        "................"],
}

ITEM_GRIDS = {
    'crowbar': [
        "................",
        "................",
        "................",
        "...........RR...",
        "..........RRRR..",
        ".........RR..RR.",
        "........RR....R.",
        ".......RR.....R.",
        "......RR.....R..",
        ".....RR.........",
        "....RR..........",
        "...RR...........",
        "..RR............",
        ".RR.............",
        ".R..............",
        "................"],
    'pistol': [
        "................",
        "................",
        "................",
        "..S.........S...",
        ".SSSSSSSSSSSSS..",
        ".SSSSSdddSSSSS..",
        ".SSSSSSSSSSSSS..",
        ".PPPPPPPPPPPPP..",
        "..PPPP.P..P.....",
        "..PPPP.PPPP.....",
        ".PPPP...........",
        ".PPPP...........",
        ".PPPP...........",
        ".PPPP...........",
        "................",
        "................"],
    'revolver': [
        "................",
        "................",
        "................",
        "..C..CCCC....C..",
        "..CCCCCCCCCCCCC.",
        "...CCdCdCCCCCCC.",
        "...CCdCdC.......",
        "..DDDCCCC.......",
        "..DDD.CC........",
        ".DDDD...........",
        ".DDD............",
        ".DDD............",
        "................",
        "................",
        "................",
        "................"],
    'shotgun': [
        "................",
        "................",
        "................",
        "................",
        ".....PPPP.......",
        ".....PPPPSSSSSS.",
        ".DDDDPPPPDDDDSS.",
        ".DDDDPPPPDDDDS..",
        ".DDD..P.P.......",
        ".DD...PP........",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................"],
    'mp5': [
        "................",
        "................",
        "................",
        "...........P....",
        "..SSSSSSSSSSS...",
        ".PSSSSSSSSSSSPP.",
        ".P..PPPPPPPP....",
        ".P..PP..PP......",
        ".PP.PP...PP.....",
        ".........PP.....",
        "..........P.....",
        "................",
        "................",
        "................",
        "................",
        "................"],
    'crossbow': [
        "................",
        ".........SS.....",
        "........s.SS....",
        ".......s...S....",
        "......s....SS...",
        ".....s......S...",
        "....s.......S...",
        ".DDDDDDDDDDDSCC.",
        ".DDDDDDDDDDDSCC.",
        "....s.......S...",
        ".....s......S...",
        "......s....SS...",
        ".......s...S....",
        "........s.SS....",
        ".........SS.....",
        "................"],
    'grenade': [
        "................",
        ".....oo.........",
        "....o..o........",
        ".....oo.SS......",
        "......SSSSSS....",
        "......SSS..S....",
        ".....GGGGG.S....",
        "....GGGGGGGS....",
        "...GGGGGGGGG....",
        "...GnGGnGGnG....",
        "...GGGGGGGGG....",
        "...GnGGnGGnG....",
        "....GGGGGGG.....",
        ".....GGGGG......",
        "................",
        "................"],
    'rpg': [
        "................",
        "................",
        "................",
        "................",
        "....gg..........",
        ".S..g......GG...",
        ".SSSSDDDSSGGGG..",
        ".SSSSDDDSSGGGGG.",
        ".SSSSDDDSSGGGG..",
        ".S..PP..PP.GG...",
        "....PP...P......",
        "................",
        "................",
        "................",
        "................",
        "................"],
    'ammo_9mm': [
        "................",
        "................",
        ".....YYYYOO.....",
        ".....YYYYOO.....",
        "....PPPPPP......",
        "....PPPPPP......",
        "....PPkPPP......",
        "....PPPPPP......",
        "....PPkPPP......",
        "....PPPPPP......",
        "....PPkPPP......",
        "....PPPPPP......",
        "...PPPPPPPP.....",
        "................",
        "................",
        "................"],
    'ammo_357': [
        "................",
        "................",
        ".......O........",
        "......OOO.......",
        "...O..YYY..O....",
        "..OOO.YYY.OOO...",
        "..YYY.YYY.YYY...",
        "..YYY.YYY.YYY...",
        "..YYY.YYY.YYY...",
        "..YYY.YYY.YYY...",
        "..YYY.YYY.YYY...",
        "..YYY.YYY.YYY...",
        "..YYY.YYY.YYY...",
        "................",
        "................",
        "................"],
    'shells': [
        "................",
        "................",
        "......EEEE......",
        ".EEEE.EEEE......",
        ".EEEE.EEEE.EEEE.",
        ".EEEE.EEEE.EEEE.",
        ".EEEE.EEEE.EEEE.",
        ".EEEE.EEEE.EEEE.",
        ".EEEE.EEEE.EEEE.",
        ".EEEE.YYYY.EEEE.",
        ".YYYY.YYYY.EEEE.",
        ".YYYY.YYYY.YYYY.",
        "...........YYYY.",
        "................",
        "................",
        "................"],
    'ammo_mp5': [
        "................",
        "................",
        ".........YYYO...",
        "........SSSSS...",
        "........SSSSS...",
        "........SSSSS...",
        ".......SSSSS....",
        ".......SSSSS....",
        "......SSSSS.....",
        ".....SSSSS......",
        "....SSSSS.......",
        "...SSSSS........",
        "..SSSSS.........",
        "................",
        "................",
        "................"],
    'bolts': [
        "................",
        "..........CC....",
        "..........SC....",
        ".........S......",
        "........S....CC.",
        ".......S.....SC.",
        "......S.....S...",
        ".....S.....S....",
        "....S.....S.....",
        "..rS.....S......",
        ".rSr....S.......",
        "..r....S........",
        ".....rS.........",
        "....rSr.........",
        ".....r..........",
        "................"],
    'rocket': [
        "................",
        ".............G..",
        "..........GGGG..",
        ".........GGGGG..",
        "........GGGGGG..",
        "........GGGGGG..",
        "........GGGGG...",
        ".......SSGGG....",
        "......SSS.......",
        ".....SSS........",
        "....SSS.........",
        ".S.SSS..........",
        ".SSSS...........",
        "..SSS...........",
        "...S............",
        "................"],
    'medkit': [
        "................",
        "................",
        "......dddd......",
        ".....d....d.....",
        ".XXXXXXXXXXXXXX.",
        ".XXXXXXrrXXXXXX.",
        ".XXXXXXrrXXXXXX.",
        ".XXXXrrrrrrXXXX.",
        ".XXXXrrrrrrXXXX.",
        ".XXXXXXrrXXXXXX.",
        ".XXXXXXrrXXXXXX.",
        ".XXXXXXXXXXXXXX.",
        "................",
        "................",
        "................",
        "................"],
    'battery': [
        "................",
        "......gggg......",
        "....yyyyyyyy....",
        "....BBBBBBBB....",
        "....BBBBByBB....",
        "....BBBByyBB....",
        "....BBByyBBB....",
        "....BByyyyBB....",
        "....BBByyBBB....",
        "....BBBByBBB....",
        "....BBByBBBB....",
        "....BBBBBBBB....",
        "....yyyyyyyy....",
        "................",
        "................",
        "................"],
}

TOOLS = [('pickaxe', 'wood'), ('pickaxe', 'stone'), ('pickaxe', 'iron'),
         ('shovel', 'wood'), ('shovel', 'stone'), ('shovel', 'iron'),
         ('axe', 'wood'), ('axe', 'stone'), ('axe', 'iron')]
WEAPONS = ['crowbar', 'pistol', 'revolver', 'shotgun', 'mp5', 'crossbow', 'grenade', 'rpg']
SUPPLIES = ['ammo_9mm', 'ammo_357', 'shells', 'ammo_mp5', 'bolts', 'rocket', 'medkit', 'battery']

ICON_NAMES = ([m[0] for m in mb.MATERIALS] + ['%s_%s' % (t, k) for k, t in TOOLS] + WEAPONS + SUPPLIES + ['empty'])
NICON = len(ICON_NAMES)
EMPTY = NICON - 1


def pixel_icon(grid, mats=None):
    """16x16 character grid -> (rgb 16x16, opaque 16x16): auto shading for MAT letters, 1px outline around the
    silhouette (4-neighbour), art centred in the 16x16 cell by whole pixels."""
    mats = dict(MAT, **(mats or {}))
    assert len(grid) == 16 and all(len(r) == 16 for r in grid), grid
    g = np.array([list(r) for r in grid])
    filled = g != '.'
    rgb = np.zeros((16, 16, 3), np.float32)
    pad = np.pad(g, 1, constant_values='.')
    for y in range(16):
        for x in range(16):
            c = g[y, x]
            if c == '.':
                continue
            if c in FLAT:
                rgb[y, x] = FLAT[c]
                continue
            hi, base, lo = mats[c]
            up, lf = pad[y, x + 1] != c, pad[y + 1, x] != c
            dn, rt = pad[y + 2, x + 1] != c, pad[y + 1, x + 2] != c
            score = int(up) + int(lf) - int(dn) - int(rt)
            rgb[y, x] = hi if score > 0 else lo if score < 0 else base
    sil = filled & ~np.isin(g, list(NO_OUTLINE))
    p = np.pad(sil, 1)
    ring = (p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]) & ~filled
    rgb[ring] = OUTLINE
    op = filled | ring
    # centre the art (including its outline) in the 16x16 cell
    ys, xs = np.nonzero(op)
    dy = (16 - (ys.max() + 1) - ys.min()) // 2
    dx = (16 - (xs.max() + 1) - xs.min()) // 2
    rgb = np.roll(rgb, (dy, dx), (0, 1)); op = np.roll(op, (dy, dx), (0, 1))
    assert op.sum() == (filled | ring).sum()
    return rgb, op


def x3(rgb, op):
    return np.repeat(np.repeat(rgb, 3, 0), 3, 1), np.repeat(np.repeat(op, 3, 0), 3, 1)


def build_icons():
    """-> list of 45 (rgb 48x48, opaque 48x48, exact bool) ; icon 44 is None (empty slot)."""
    icons = []
    for name, top, side in mb.MATERIALS:
        rgb, op = mb.material_icon(name, top, side)
        icons.append((rgb.astype(np.float32), op, name in mb.FLAT_ICONS))
    for tool, tier in TOOLS:
        icons.append(x3(*pixel_icon(TOOL_GRIDS[tool], {'H': TIER[tier]})) + (True,))
    for n in WEAPONS + SUPPLIES:
        icons.append(x3(*pixel_icon(ITEM_GRIDS[n])) + (True,))
    icons.append(None)
    assert len(icons) == NICON
    return icons


def slot_frames(icons):
    frames = []
    for sel in (False, True):
        base = slot_base(sel)
        for ic in icons:
            if ic is None:
                frames.append((base[0], base[1], np.ones((S, S), bool)))
                continue
            rgb, op = with_icon(base, ic[0], ic[1])
            exact = np.ones((S, S), bool)
            if not ic[2]:
                o = (S - mb.FS) // 2
                exact[o:o + mb.FS, o:o + mb.FS][ic[1]] = False
            frames.append((rgb, op, exact))
    return frames

# ---------------------------------------------------------------- panel

PW, PH = 400, 232
SPLIT = 256
GREY, LIGHT, DARK, BLACK, RECESS, RECESS_DARK = (198,) * 3, (255,) * 3, (85,) * 3, (0,) * 3, (139,) * 3, (55,) * 3
RECIPE_AREA = (8, 24, 176, 192)        # x0, y0, x1, y1 (exclusive), slot area
INGRED_AREA = (184, 24, 240, 192)
ARROW_C = (276, 108)


def build_panel():
    rgb = np.zeros((PH, PW, 3), np.uint8)
    op = np.ones((PH, PW), bool)
    rgb[:] = GREY
    # 2px bevel inside a 1px black outline, light on top/left, dark on bottom/right
    rgb[1:3, 1:-1] = LIGHT
    rgb[1:-1, 1:3] = LIGHT
    rgb[-3:-1, 1:-1] = DARK
    rgb[1:-1, -3:-1] = DARK
    # where the bevels meet (top-right, bottom-left) split them along the 45 degree line
    for y in range(1, 3):
        for x in range(PW - 3, PW - 1):
            d = (PW - 1 - x) - y
            rgb[y, x] = LIGHT if d > 0 else DARK if d < 0 else GREY
    for y in range(PH - 3, PH - 1):
        for x in range(1, 3):
            d = x - (PH - 1 - y)
            rgb[y, x] = LIGHT if d < 0 else DARK if d > 0 else GREY
    rgb[0, :] = BLACK; rgb[-1, :] = BLACK; rgb[:, 0] = BLACK; rgb[:, -1] = BLACK
    # rounded outer corners (Minecraft GUI style): corner pixel cut, black pixel moved inward
    for cy, cx, sy, sx in ((0, 0, 1, 1), (0, PW - 1, 1, -1), (PH - 1, 0, -1, 1), (PH - 1, PW - 1, -1, -1)):
        op[cy, cx] = False
        rgb[cy + sy, cx + sx] = BLACK
    # recessed areas, 4px larger than the slot areas, with a 1px inner bevel (dark top/left, white bottom/right)
    for x0, y0, x1, y1 in (RECIPE_AREA, INGRED_AREA):
        x0, y0, x1, y1 = x0 - 4, y0 - 4, x1 + 4, y1 + 4
        rgb[y0:y1, x0:x1] = RECESS
        rgb[y0, x0:x1 - 1] = RECESS_DARK
        rgb[y0:y1 - 1, x0] = RECESS_DARK
        rgb[y1 - 1, x0 + 1:x1] = LIGHT
        rgb[y0 + 1:y1, x1 - 1] = LIGHT
    # crafting arrow: 16x12 cell grid, 2px per cell, grey with a dark outline, pointing right
    arrow = arrow_cells()
    h, w = arrow.shape
    ax0, ay0 = ARROW_C[0] - w, ARROW_C[1] - h          # 2px cells -> pixel size (2w, 2h)
    for r in range(h):
        for c in range(w):
            if arrow[r, c]:
                rgb[ay0 + 2 * r:ay0 + 2 * r + 2, ax0 + 2 * c:ax0 + 2 * c + 2] = DARK_ARROW if arrow[r, c] == 1 else RECESS
    return rgb, op


DARK_ARROW = (55, 55, 55)


def arrow_cells():
    """16x12 cells: 0 empty, 1 outline, 2 fill. Shaft 6 cells high, head 12 cells high."""
    m = np.zeros((12, 16), int)
    m[3:9, 0:9] = 2                       # shaft
    for r in range(12):                   # triangular head, apex at the right
        half = min(r, 11 - r)
        m[r, 8:8 + half + 2] = 2
    out = np.zeros_like(m)
    p = np.pad(m > 0, 1)
    for r in range(12):
        for c in range(16):
            if m[r, c]:
                nb = p[r, c + 1] & p[r + 2, c + 1] & p[r + 1, c] & p[r + 1, c + 2]
                out[r, c] = 2 if nb else 1
    return out


def write_panel():
    rgb, op = build_panel()
    for name, x0, x1 in (('craftpanel_l', 0, SPLIT), ('craftpanel_r', SPLIT, PW)):
        sub, sop = rgb[:, x0:x1], op[:, x0:x1]
        idx, pal, nuniq = mb.quantize_alphatest(sub, sop)
        assert nuniq <= 255
        assert (pal[idx][sop] == sub[sop]).all()
        mb.save_bmp(idx, pal, name + '.bmp')
        run_sprgen(name, ['$spritename ' + name, '$type vp_parallel', '$texture alphatest',
                          '$load %s.bmp' % name, '$frame 0 0 %d %d' % (x1 - x0, PH)])
    return rgb, op

# ---------------------------------------------------------------- previews


def icon_sheet(icons, path, scale=2):
    font = ImageFont.load_default()
    cols, cw, ch = 9, S * scale + 12, S * scale + 26
    rows = (NICON + cols - 1) // cols
    img = Image.new('RGB', (cols * cw + 12, rows * ch + 12), (36, 38, 42))
    d = ImageDraw.Draw(img)
    for i in range(NICON):
        rgb, op = slot_base(False)
        if icons[i] is not None:
            rgb, op = with_icon((rgb, op), icons[i][0], icons[i][1])
        im = Image.fromarray(rgb.astype(np.uint8)).resize((S * scale, S * scale), Image.NEAREST)
        r, c = divmod(i, cols)
        img.paste(im, (12 + c * cw, 12 + r * ch))
        d.text((12 + c * cw, 12 + r * ch + S * scale + 3), '%d %s' % (i, ICON_NAMES[i]), fill=(230, 230, 230), font=font)
    img.save(path)


def blit(canvas, spr, fi, cx, cy):
    """Draw sprite frame fi centred at (cx, cy), alphatest."""
    f = spr['frames'][fi]
    x0, y0 = cx - f['w'] // 2, cy - f['h'] // 2
    reg = canvas[y0:y0 + f['h'], x0:x0 + f['w']]
    m = f['px'] != 255
    reg[m] = spr['pal'][f['px']][m]


def verify_and_preview():
    sl = sprparse.parse('craftslots.spr')
    pl, pr = sprparse.parse('craftpanel_l.spr'), sprparse.parse('craftpanel_r.spr')
    for n, s in (('craftslots', sl), ('craftpanel_l', pl), ('craftpanel_r', pr)):
        assert s['typename'] == 'vp_parallel' and s['texname'] == 'alphatest', n
        assert tuple(s['pal'][255]) == (0, 0, 255), n
        print(f"{n}.spr: {s['nframes']} frames, {s['frames'][0]['w']}x{s['frames'][0]['h']}, {s['size']} bytes")
    assert sl['nframes'] == 2 * NICON and all(f['w'] == 56 and f['h'] == 56 for f in sl['frames'])
    assert pl['nframes'] == 1 and (pl['frames'][0]['w'], pl['frames'][0]['h']) == (256, 232)
    assert pr['nframes'] == 1 and (pr['frames'][0]['w'], pr['frames'][0]['h']) == (144, 232)
    # mock-up of the whole window over a dim backdrop (panel coords = screen offset + (200, 116))
    M = 24
    can = np.zeros((PH + 2 * M, PW + 2 * M, 3), np.uint8); can[:] = (52, 70, 44)
    blit(can, pl, 0, M + 128, M + 116)
    blit(can, pr, 0, M + 256 + 72, M + 116)
    recipe = [0, 5, 20, 29, 38, 27, 42, 35, 18]          # stone pickaxe (20) selected
    for k, ic in enumerate(recipe):
        r, c = divmod(k, 3)
        sx, sy = (-164, -108, -52)[c], (-64, -8, 48)[r]
        blit(can, sl, ic + (45 if ic == 20 else 0), M + sx + 200, M + sy + 116)
    for ic, sy in zip((5, 3, EMPTY), (-64, -8, 48)):
        blit(can, sl, ic, M + 12 + 200, M + sy + 116)
    blit(can, sl, 20, M + 132 + 200, M + -8 + 116)
    Image.fromarray(can).resize((can.shape[1] * 2, can.shape[0] * 2), Image.NEAREST).save('_craftui_preview.png')
    # contact sheet of all 45 icons, from the compiled sprite
    icons = []
    for i in range(NICON):
        f = sl['frames'][i]
        icons.append(None if i == EMPTY else (sl['pal'][f['px']][4:52, 4:52].astype(np.float32),
                                              (f['px'] != 255)[4:52, 4:52]))
    icon_sheet(icons, '_craftui_icons.png', 2)


def install():
    os.makedirs(INSTALL, exist_ok=True)
    for n in ('craftslots', 'craftpanel_l', 'craftpanel_r'):
        shutil.copy2(n + '.spr', os.path.join(INSTALL, n + '.spr'))
        print('installed', os.path.join(INSTALL, n + '.spr'), os.path.getsize(os.path.join(INSTALL, n + '.spr')))


if __name__ == '__main__':
    icons = build_icons()
    if 'icons' in sys.argv[1:]:
        icon_sheet(icons, sys.argv[2] if len(sys.argv) > 2 else '_craftui_icons.png', 3)
        sys.exit()
    write_sprite('craftslots', slot_frames(icons))
    write_panel()
    verify_and_preview()
    if 'noinstall' not in sys.argv[1:]:
        install()
