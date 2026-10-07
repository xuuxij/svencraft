# Svencraft item pixel art: 16x16 character grids, outlined and shaded in code (original art).
# Ported from the Sven Co-op prototype's crafting UI builder; used by tools/make_items.py.
import numpy as np

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
    'diamond': ((176, 255, 248), (92, 220, 214), (36, 150, 156)),
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

