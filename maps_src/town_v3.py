# The town, version 3 (docs/town_v3_assets.md): Elm St's buildings (diner, police station, church and graveyard),
# the apartment block with its fire escape, the park, the gas station with its service bays, street furniture
# (power lines, hydrants, dumpsters, a phone booth, a bus stop, signs), and the town's sounds.
# make_town.py calls textures(TEX) before it writes its WAD, and build(globals()) once its helpers exist.
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFont


# ---------------------------------------------------------------- generated textures (signs, the police livery)
def _font(size):
    for name in ('arialbd.ttf', 'arial.ttf'):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def _board(w, h, bg, fg, lines, border=None, octagon=False):
    im = Image.new('RGB', (w, h), (90, 96, 90) if octagon else bg)
    d = ImageDraw.Draw(im)
    if octagon:
        c, r = w / 2, w / 2 - 1
        pts = [(c + r * math.cos(math.pi / 8 + k * math.pi / 4), c + r * math.sin(math.pi / 8 + k * math.pi / 4)) for k in range(8)]
        d.polygon(pts, fill=(250, 250, 250))
        r -= 3
        pts = [(c + r * math.cos(math.pi / 8 + k * math.pi / 4), c + r * math.sin(math.pi / 8 + k * math.pi / 4)) for k in range(8)]
        d.polygon(pts, fill=bg)
    elif border:
        d.rectangle((1, 1, w - 2, h - 2), outline=border, width=2)
    y = (h - sum(s for _, s in lines) - 2 * (len(lines) - 1)) / 2
    for text, size in lines:
        f = _font(size)
        tw = d.textlength(text, font=f)
        d.text(((w - tw) / 2, y - 1), text, font=f, fill=fg)
        y += size + 2
    a = np.array(im, float)
    a += np.random.default_rng(len(lines) * 31 + w).normal(0, 3, a.shape)     # a little grime
    return np.clip(a * 0.92, 0, 255)


def textures(TEX, car_side, paint):
    TEX['sign_stop'] = _board(64, 64, (176, 26, 26), (250, 250, 250), [('STOP', 20)], octagon=True)
    TEX['sign_speed'] = _board(48, 64, (232, 232, 226), (20, 20, 20), [('SPEED', 9), ('LIMIT', 9), ('35', 22)], border=(20, 20, 20))
    TEX['sign_bus'] = _board(48, 64, (34, 70, 140), (240, 240, 240), [('BUS', 14), ('STOP', 14)], border=(240, 240, 240))
    TEX['sign_gas'] = _board(96, 64, (196, 30, 30), (250, 244, 220), [('ROCKWELL', 13), ('GAS   .89', 13), ('DIESEL .79', 13)], border=(250, 244, 220))
    TEX['sign_police'] = _board(128, 32, (24, 34, 72), (226, 190, 80), [('POLICE', 22)], border=(226, 190, 80))
    TEX['sign_mail'] = _board(32, 16, (34, 60, 130), (240, 240, 240), [('US MAIL', 6)])
    # the police car: black and white, a star on the doors
    side = car_side((214, 214, 208), 77)
    side[:20] = side[:20] * 0.15 + 10
    side[46:] = side[46:] * 0.15 + 10
    im = Image.fromarray(side.astype(np.uint8))
    d = ImageDraw.Draw(im)
    for cx in (124, 176):
        pts = [(cx + (8 if k % 2 == 0 else 3.5) * math.sin(k * math.pi / 5), 33 - (8 if k % 2 == 0 else 3.5) * math.cos(k * math.pi / 5)) for k in range(10)]
        d.polygon(pts, fill=(210, 170, 60))
    TEX['carside_police'] = np.array(im, float)
    TEX['car_police'] = paint((26, 26, 28), 64, 64, 91)
    bar = np.zeros((16, 32, 3)) + 30
    bar[3:13, 1:15] = (220, 30, 30)
    bar[3:13, 17:31] = (40, 60, 230)
    TEX['lightbar'] = bar


# ---------------------------------------------------------------- the town
def build(g):
    box, prism, entity, Brush = g['box'], g['prism'], g['entity'], g['Brush']
    dyn, ents = g['dyn'], g['ents']
    dbox, side_box, prop, fit = g['dbox'], g['side_box'], g['prop'], g['fit']
    desk, counter, shelf, crate, building, car = g['desk'], g['counter'], g['shelf'], g['crate'], g['building'], g['car']
    wall, glass, door = g['wall'], g['glass'], g['door']
    FURNITURE, BUILDINGS, DOORS = g['FURNITURE'], g['BUILDINGS'], g['DOORS']
    DIRT, COBBLE, LEAVES, PLANKS, BRICK, TAN, CONCRETE, ASPHALT, METAL = 2, 4, 14, 15, 18, 19, 20, 21, 22

    g['MATERIAL'].update({'Brick05': BRICK, 'WALL_BRICK2': BRICK, 'WALL_BRICK5': BRICK, 'PRXMSBRICK1A': TAN, 'nm_farm05': PLANKS,
                          'OUT_GALV1': METAL})
    g['KINDS'].update({
        'diner':     {'floor': 'FIFTIES_FLR03', 'ceil': 'FIFTIES_CEIL01', 'wall_in': 'FIFTIES_WALL14'},
        'police':    {'floor': 'C1A1_LINO', 'ceil': 'CEILING_TILE', 'wall_in': 'FIFTIES_WALL14'},
        'church':    {'floor': 'IN_FLOOR2', 'ceil': 'M_CEILING3', 'wall_in': 'WALL_PLASTER'},
        'apartment': {'floor': 'IN_FLOOR3', 'ceil': 'CEILING_TILE', 'wall_in': 'EASTPLSTR2B'},
        'kiosk':     {'floor': 'FIFTIES_FLR02', 'ceil': 'CEILING_TILE', 'wall_in': 'E_WALL_WHITE1'},
    })

    def light(x, y, z, rgbi):
        ents.append(entity({'classname': 'light', 'origin': '%d %d %d' % (x, y, z), '_light': '%d %d %d %d' % rgbi}))

    def masked(brushes, solid=True, kv=None):
        """see-through textures ({ fences, bars, cables) only work on brush entities, drawn alpha-tested"""
        d = {'classname': 'func_wall' if solid else 'func_illusionary', 'rendermode': '4', 'renderamt': '255'}
        d.update(kv or {})
        ents.append(entity(d, brushes))

    def board(x0, y0, z0, x1, y1, z1, tex, faces, tw, th):
        """a sign: the texture stretched over the board's faces named in `faces` (a func_wall: not diggable)"""
        b = box((x0, y0, z0), (x1, y1, z1), {'all': 'BA_STEEL_01', **{f: tex for f in faces}},
                **{f: (fit((1, 0, 0), (0, 0, -1), x0, z1, x1 - x0, z1 - z0, tw, th) if f[0] == 'y' else
                       fit((0, 1, 0), (0, 0, -1), y0, z1, y1 - y0, z1 - z0, tw, th)) for f in faces})
        ents.append(entity({'classname': 'func_wall'}, [b]))

    def post(cx, cy, r, z0, z1, tex, mat=METAL, sides=8):
        pts = [(cx + r * math.cos(2 * math.pi * (k + 0.5) / sides), cy + r * math.sin(2 * math.pi * (k + 0.5) / sides)) for k in range(sides)]
        dyn.append((prism(pts, z0, [z1] * sides, tex, tex, tex), mat, tex))

    def ambient(x, y, z, wav, vol, radius):
        flags = {'everywhere': 1, 'small': 2, 'medium': 4, 'large': 8}[radius]
        ents.append(entity({'classname': 'ambient_generic', 'origin': '%d %d %d' % (x, y, z), 'message': wav, 'health': str(vol),
                            'spawnflags': str(flags)}))

    def random_sounds(x, y, z, sounds, mind, maxd, vol, attn, spread=0):
        """sc_ambient_random (game/dlls/svencraft/sc_acoustics.cpp): one of the sounds now and then"""
        ents.append(entity({'classname': 'sc_ambient_random', 'origin': '%d %d %d' % (x, y, z), 'sounds': ';'.join(sounds),
                            'mindelay': str(mind), 'maxdelay': str(maxd), 'volume': '%.2f' % vol, 'attenuation': '%.2f' % attn,
                            'spread': str(spread)}))

    def slab(x0, y0, x1, y1, tex, mat=DIRT, z0=0, z1=1, cut=None):
        dbox((x0, y0, z0), (x1, y1, z1), {'top': tex, 'side': cut or tex}, mat, cut=cut or tex)

    # ------------------------------------------------ the diner, Big Tony's Pizza (north side, west of Elm St)
    x0, y0, x1, y1 = -264, 296, -8, 536
    ix0, iy0, ix1, iy1 = building(x0, y0, x1, y1, 160, 'Brick05', 'diner', 'y-', windows=3, roof='DOUBLE_ASPHALT',
                                  door_tex='OFF_DR3', door_offset=96, light=(255, 230, 190, 150))
    dbox((x0, y0, 160), (x1, y0 + 16, 204), {'top': 'OUT_CONCRETE1', 'side': 'Brick05'}, BRICK)             # false front
    board(-196, y0 - 4, 132, -100, y0, 204, 'sign_pizza_01', ['y-'], 128, 128)
    board(x1, 380, 64, x1 + 4, 460, 144, 'sign_taco_01', ['x+'], 128, 128)
    slab(x0, 192, x1, y0, 'OUT_SIDEWALK6', CONCRETE, 0, 8, cut='OUT_CONCRETE1')                             # patio
    for cx in (-200, -136):
        prop('models/snd/awningL.mdl', cx, y0 - 1, yaw=270, z=136, solid=False)
    for cx in (-216, -96):
        prop('models/ginsmodels/wb_umbrella.mdl', cx, 244, z=8, solid=False)
        prop('models/ginsmodels/wb_table01.mdl', cx, 244, z=8)
        prop('models/ginsmodels/wb_seat.mdl', cx - 30, 244, yaw=0, z=8)
        prop('models/ginsmodels/wb_seat.mdl', cx + 30, 244, yaw=180, z=8)
    # inside: a counter with stools, booths by the windows, the kitchen behind
    counter(-236, 456, -60, 480)
    for k in range(5):
        sx = -220 + 36 * k
        post(sx, 440, 4, 8, 34, 'BA_STEEL_01')
        post(sx, 440, 10, 34, 38, 'barrel_red_01', PLANKS)
        FURNITURE.append((sx - 10, 430, sx + 10, 450))
    for bx in (-236, -170):
        dbox((bx, 330, 8), (bx + 48, 362, 36), {'top': 'M_WOOD1', 'all': 'DESK_GEN'}, PLANKS)               # table
        for by0, by1 in ((314, 330), (362, 378)):
            dbox((bx, by0, 8), (bx + 48, by1, 26), 'couch_main_t', PLANKS)                                  # seats
        dbox((bx, 312, 8), (bx + 48, 316, 56), 'couch_main_t', PLANKS)                                      # back
        FURNITURE.append((bx, 312, bx + 48, 378))
    dbox((ix0 + 4, iy1 - 24, 8), (ix0 + 68, iy1, 104), {'all': 'BA_STEEL_01', 'y-': 'ba_freezer_01'}, METAL)  # fridge
    board(-180, iy1 - 2, 112, -116, iy1, 144, 'ba_pizzamenu', ['y-'], 128, 64)
    prop('models/revil/Furnature/sink.mdl', ix1 - 20, iy1 - 12, yaw=270, z=8)
    prop('models/ginsmodels/lag_radio.mdl', -100, 468, yaw=270, z=48)
    for fx in (-200, -100):
        prop('models/ginsmodels/gins_ceilfan.mdl', fx, 400, z=160, solid=False)
    prop('models/ginsmodels/gins_creteytrashcan.mdl', -24, 212, z=8)
    ambient(-100, 468, 64, 'hunger/thambs/radiomusic.wav', 3, 'small')
    ambient(-150, 400, 140, 'fans/fan1.wav', 2, 'small')

    # ------------------------------------------------ the police station (east of Elm St)
    x0, y0, x1, y1 = 264, 296, 616, 616
    ix0, iy0, ix1, iy1 = building(x0, y0, x1, y1, 232, 'WALL_BRICK2', 'police', 'y-', windows=3, floors=2,
                                  door_tex='OFF_DR2', light=(230, 236, 255, 150))
    board(384, y0 - 4, 128, 496, y0, 156, 'sign_police', ['y-'], 128, 32)
    post(300, 240, 3, 8, 330, 'BA_STEEL_01')                                                                 # flag
    ents.append(entity({'classname': 'func_illusionary'}, [box((303, 239, 250), (351, 241, 322), 'POSTER11',
                **{f: fit((1, 0, 0), (0, 0, -1), 303, 322, 48, 72, 64, 96) for f in ('y-', 'y+')})]))
    slab(x0, 640, x1, 840, 'DOUBLE_ASPHALT', ASPHALT, 0, 2, cut='OUT_ASPHALT1')                              # the lot
    for k in range(5):
        lx = 296 + 64 * k
        dbox((lx, 760, 2), (lx + 4, 840, 3), 'WHITE', ASPHALT, cut='OUT_ASPHALT1')
    car(360, 720, True, 'police', z=2, lightbar=True)
    car(520, 720, True, 'police', z=2, lightbar=True)
    # inside: a front desk, a bench, two cells on the east side, desks upstairs
    counter(392, 380, 488, 404)
    prop('models/ginsmodels/bench3.mdl', 300, 430, yaw=0, z=8)
    prop('models/ginsmodels/lag_radio.mdl', 440, 392, yaw=270, z=48)
    for cy0 in (344, 456):
        cy1 = cy0 + 96
        bars = [box((504, cy0, 8), (506, cy0 + 24, 120), '{GATE_01'), box((504, cy1 - 24, 8), (506, cy1, 120), '{GATE_01'),
                box((504, cy0 + 24, 104), (506, cy1 - 24, 120), '{GATE_01')]
        masked(bars)
        gate = box((504, cy0 + 24, 8), (506, cy1 - 24, 104), '{GATE_01')
        origin = box((504, cy0 + 24, 8), (506, cy0 + 26, 104), 'ORIGIN')
        ents.append(entity({'classname': 'func_door_rotating', 'speed': '100', 'distance': '90', 'wait': '-1', 'rendermode': '4',
                            'renderamt': '255', 'spawnflags': '256'}, [gate, origin]))                       # use to open
        dbox((ix1 - 72, cy0 + 8, 8), (ix1, cy0 + 40, 28), {'top': 'BEDDING', 'all': 'BA_STEEL_01'}, METAL)
        prop('models/revil/Furnature/toilet.mdl', ix1 - 24, cy1 - 24, yaw=180, z=8)
        FURNITURE.append((504, cy0, ix1, cy1))
    for k in range(3):
        dbox((ix0 + 4 + 34 * k, iy0 + 4, 8), (ix0 + 36 + 34 * k, iy0 + 28, 104), {'all': 'BA_STEEL_01', 'y+': 'PRXLOCKER1A'}, METAL)
    desk(340, 440, 'y-', z=128); desk(440, 440, 'y-', z=128); desk(540, 520, 'x-', z=128)
    prop('models/filecabinet.mdl', ix1 - 14, 340, yaw=180, z=128)
    ambient(440, 392, 64, 'hunger/thambs/radio_static1.wav', 2, 'small')
    ambient(560, 450, 100, 'ambience/fluorescent.wav', 2, 'small')

    # ------------------------------------------------ the church at the end of Elm St, its graveyard
    x0, y0, x1, y1 = 40, 792, 200, 1016
    ix0, iy0, ix1, iy1 = building(x0, y0, x1, y1, 160, 'nm_farm05', 'church', 'y-', windows=3, roof='nm_farm07', gable=96,
                                  door_tex='EASTDOOR1', light=(255, 220, 170, 130))
    # the steeple over the door: tower, belfry, spire
    dbox((88, 792, 168), (152, 856, 268), {'top': 'OUT_CONCRETE1', 'side': 'nm_farm05'}, PLANKS)
    dbox((88, 792, 268), (152, 856, 340), {'top': 'OUT_CONCRETE1', 'side': 'EASTWNDWBTALL'}, PLANKS)
    dbox((84, 788, 340), (156, 860, 352), 'M_WOOD1', PLANKS)
    spire = Brush()
    a = (120, 824, 448)
    base = [(84, 788, 352), (156, 788, 352), (156, 860, 352), (84, 860, 352)]
    spire.face(list(reversed(base)), 'nm_farm07')
    for k in range(4):
        spire.face([base[k], base[(k + 1) % 4], a], 'nm_farm07')
    dyn.append((spire, PLANKS, 'nm_farm07'))
    prop('models/ginsmodels/gins_clock_tower.mdl', 120, 789, yaw=270, z=300, solid=False)
    for row in range(6):                                                                                   # pews
        py = 870 + 22 * row
        for px0, px1 in ((ix0 + 8, 100), (140, ix1 - 8)):
            dbox((px0, py, 8), (px1, py + 12, 24), 'M_WOOD1', PLANKS)
            dbox((px0, py + 12, 8), (px1, py + 16, 44), 'M_WOOD1', PLANKS)
        FURNITURE.append((ix0, py, ix1, py + 16))
    dbox((88, iy1 - 40, 8), (152, iy1 - 16, 44), {'top': 'M_WOOD1', 'all': 'WOOD_PANEL_01'}, PLANKS)       # altar
    FURNITURE.append((88, iy1 - 40, 152, iy1 - 16))
    prop('models/adamr/vase.mdl', 72, iy1 - 30, z=8)
    random_sounds(120, 824, 320, ['hunger/thambs/bell2.wav'], 150, 300, 1.0, 0.4)
    random_sounds(-120, 900, 120, ['hunger/thambs/crow.wav', 'hunger/thambs/owl1.wav'], 20, 60, 0.7, 0.9, spread=200)
    # the graveyard beside it: dark grass, rows of headstones, a wooden fence with a gate to the forecourt
    gx0, gy0, gx1, gy1 = -240, 800, -16, 1010
    slab(gx0, gy0, gx1, gy1, 'jungle_floor_02')
    for j in range(3):
        for i in range(4):
            hx, hy = gx0 + 36 + 52 * i, gy0 + 40 + 64 * j
            if (i + j) % 4 == 3:
                dbox((hx - 3, hy - 3, 1), (hx + 3, hy + 3, 52), 'ROCK_GREY', COBBLE)                       # a cross
                dbox((hx - 14, hy - 3, 34), (hx + 14, hy + 3, 40), 'ROCK_GREY', COBBLE)
            else:
                dbox((hx - 12, hy - 3, 1), (hx + 12, hy + 3, 40), {'all': 'ROCK_GREY', 'top': 'castlestone1'}, COBBLE)
            FURNITURE.append((hx - 14, hy - 4, hx + 14, hy + 4))
    for fx0, fy0, fx1, fy1 in ((gx0, gy0, gx1, gy0 + 4), (gx0, gy1 - 4, gx1, gy1), (gx0, gy0, gx0 + 4, gy1),
                               (gx1 - 4, gy0, gx1, 860), (gx1 - 4, 924, gx1, gy1)):
        dbox((fx0, fy0, 0), (fx1, fy1, 56), 'OUT_FENCE3', PLANKS,
             **{f: {'scale': (0.5, 0.5)} for f in ('x+', 'x-', 'y+', 'y-')})
    prop('models/sc_robination/dead_tree08.mdl', -200, 980, solid=False)
    prop('models/ginsmodels/gins_oak2.mdl', -60, 830, z=132, solid=False)

    # ------------------------------------------------ the apartment block, three storeys, a fire escape on the east
    x0, y0, x1, y1 = 96, -704, 480, -368
    ix0, iy0, ix1, iy1 = building(x0, y0, x1, y1, 352, 'Brick05', 'apartment', 'y+', windows=3, floors=3, roof='DOUBLE_ASPHALT',
                                  door_tex='FIFTIES_DR6', light=(255, 228, 180, 130))
    # fire escape: two flights against the wall (each 32 wide), landings at the floors, a railing
    for k in range(8):
        dbox((512, -432 - 18 * (k + 1), 12 + 16 * k), (544, -432 - 18 * k, 16 + 16 * k), 'OUT_GRATING1', METAL)
    dbox((480, -640, 124), (544, -576, 128), 'OUT_GRATING1', METAL)                                         # landing 2nd floor
    for k in range(8):
        dbox((480, -576 + 18 * k, 139 + 15 * k), (512, -576 + 18 * (k + 1), 143 + 15 * k), 'OUT_GRATING1', METAL)
    dbox((480, -432, 244), (544, -368, 248), 'OUT_GRATING1', METAL)                                         # landing 3rd floor
    masked([box((544, -640, 128), (546, -576, 168), '{GRATE1A'), box((544, -432, 248), (546, -368, 288), '{GRATE1A')])
    FURNITURE.append((480, -640, 548, -368))
    # a steel ladder from the top landing up the wall and over the parapet onto the roof (func_ladder: the climbable
    # volume, invisible; the rungs are a see-through brush in front of it)
    masked([box((482, -416, 248), (484, -384, 404), '{LADDER2',
                **{f: fit((0, 1, 0), (0, 0, -1), -416, 404, 32, 32, 32, 32) for f in ('x+', 'x-')})], solid=False)
    ents.append(entity({'classname': 'func_ladder'}, [box((480, -416, 248), (490, -384, 402), 'AAATRIGGER')]))
    # a water tank on the roof
    for lx, ly in ((352, -600), (416, -600), (352, -536), (416, -536)):
        post(lx, ly, 3, 368, 404, 'BA_STEEL_01')
    post(384, -568, 40, 404, 468, 'OUT_WD', PLANKS, sides=12)
    # inside: the flats' furniture on each floor
    for f, z in enumerate((8, 128, 248)):
        prop('models/ginsmodels/couch.mdl', ix1 - 4, -520, yaw=180, z=z)
        prop('models/ginsmodels/gins_table20.mdl', ix1 - 80, -520, z=z)
        prop('models/chair.mdl', ix1 - 60, -600, yaw=90, z=z)
        prop('models/uplant3.mdl', ix0 + 20, -420, z=z)
    ambient(288, -420, 60, 'ambience/fluorescent.wav', 2, 'small')
    ambient(200, -600, 170, 'hunger/thambs/radiomusic.wav', 2, 'small')
    ambient(400, -500, 290, 'ambience/crtnoise.wav', 1, 'small')
    # the yard behind it: a fence, a washing line, a dumpster
    slab(80, -900, 560, -720, 'BOOT_GRASS_06')
    for fx0, fy0, fx1, fy1 in ((80, -900, 560, -896), (80, -900, 84, -720), (556, -900, 560, -720)):
        dbox((fx0, fy0, 0), (fx1, fy1, 96), 'OUT_FENCE1', PLANKS, **{f: {'scale': (0.75, 0.75)} for f in ('x+', 'x-', 'y+', 'y-')})
    for lx in (160, 416):
        post(lx, -800, 3, 1, 128, 'TELEPHONEPOLE', PLANKS)
    masked([box((160, -801, 64), (416, -799, 128), '{EASTLINE1', **{f: fit((1, 0, 0), (0, 0, -1), 160, 128, 256, 64, 256, 128)
                                                                   for f in ('y-', 'y+')})], solid=False)

    # ------------------------------------------------ the park between the warehouse lot and the house
    px0, py0, px1, py1 = -540, -560, -300, -260
    slab(px0, py0, px1, py1, 'OUT_GROUND5')
    slab(-428, py0, -412, py1, 'OUT_SIDEWALK6', CONCRETE, 1, 2)
    slab(px0, -418, px1, -402, 'OUT_SIDEWALK6', CONCRETE, 1, 2)
    prop('models/ginsmodels/bench3.mdl', -470, -380, yaw=270, z=1)
    prop('models/ginsmodels/bench3.mdl', -360, -440, yaw=90, z=1)
    prop('models/ginsmodels/iw_picnic.mdl', -360, -320, z=1)
    prop('models/hunger/vegitation/tree2.mdl', -500, -520, solid=False)
    prop('models/ginsmodels/gins_oak2.mdl', -340, -500, z=133, solid=False)
    for fx, fy in ((-480, -300), (-340, -380)):
        for a0, b0, a1, b1 in ((-20, -20, 20, -16), (-20, 16, 20, 20), (-20, -16, -16, 16), (16, -16, 20, 16)):
            dbox((fx + a0, fy + b0, 1), (fx + a1, fy + b1, 9), 'castlestone1', COBBLE)
        prop('models/hunger/vegitation/arc_flower.mdl', fx, fy, z=1, solid=False)
    dbox((-444, -434, 1), (-396, -386, 81), {'all': 'castlestone1', 'x+': 'I_CHURCH1A'}, COBBLE)              # memorial
    FURNITURE.append((-444, -434, -396, -386))
    post(-310, -270, 3, 1, 40, 'BA_STEEL_01')
    prop('models/ginsmodels/gins_coinop_binocs70.mdl', -310, -270, yaw=45, z=104, solid=False)            # looking at the rift
    random_sounds(-420, -410, 120, ['ambience/wren1.wav', 'hunger/thambs/bird1.wav', 'ambience/quail1.wav', 'hunger/thambs/leaves1.wav'],
                  5, 15, 0.6, 1.0, spread=200)

    # ------------------------------------------------ the pond in the field behind the back alley (its hollow: make_town.py)
    px, py, rx, ry, wl = g['POND']
    ground_z = g['ground_z']
    # Half-Life's water: a func_water (swimming, drowning, the murky underwater view), see-through like Sven's. It goes
    # down to the block world's top so whatever is dug out under the pond floods; the banks hide the rest of the box
    ents.append(entity({'classname': 'func_water', 'skin': '-3', 'WaveHeight': '2', 'rendermode': '2', 'renderamt': '150',
                        'spawnflags': '256'}, [box((px - 224, py - 176, g['SLAB']), (px + 224, py + 176, wl), '!pondwater')]))
    # a plank jetty from the south bank on posts, deep water off its end
    jx0, jy0, jx1, jy1 = -724, 808, -676, 936
    dbox((jx0, jy0, -2), (jx1, jy1, 4), {'top': 'DTGLWOODW101', 'side': 'GREYWOOD'}, PLANKS,
         top={'u': (0, 1, 0), 'v': (1, 0, 0), 'scale': (0.4, 0.4)})          # planks across
    for jy in (852, 894, 930):
        for jx in (jx0 + 5, jx1 - 5):
            post(jx, jy, 4, int(ground_z(jx, jy)) - 4, -2, 'GREYWOOD', PLANKS)
    FURNITURE.append((jx0, jy0, jx1, jy1))
    # reeds along the shore (none by the jetty), ferns on the bank, a tree over the north-west side
    prng = g['random'].Random(11)
    for k in range(30):
        a = 2 * math.pi * (k + prng.uniform(-0.3, 0.3)) / 30
        if abs(math.atan2(math.sin(a - 5.03), math.cos(a - 5.03))) < 0.3 or prng.random() < 0.3:
            continue
        r = 0.2
        while ground_z(px + r * rx * math.cos(a), py + r * ry * math.sin(a)) < wl - 6 and r < 1.3:
            r += 0.02
        sx, sy = px + r * rx * math.cos(a), py + r * ry * math.sin(a)
        for n in range(prng.randint(2, 4)):
            cx, cy = sx + prng.uniform(-14, 14), sy + prng.uniform(-14, 14)
            prop('models/svencooprpg2/cattail.mdl', cx, cy, solid=False, z=int(ground_z(cx, cy)) - 2)
        if prng.random() < 0.35:
            bx, by = px + (r + 0.18) * rx * math.cos(a), py + (r + 0.18) * ry * math.sin(a)
            prop(prng.choice(['models/hunger/vegitation/fern1.mdl', 'models/hunger/vegitation/fern2.mdl']), bx, by, solid=False,
                 z=int(ground_z(bx, by)) - 1)
    prop('models/hunger/vegitation/tree1.mdl', px - 250, py + 150, solid=False, z=int(ground_z(px - 250, py + 150)) - 2)
    prop('models/hunger/vegitation/bush1.mdl', px + 240, py + 120, solid=False, z=int(ground_z(px + 240, py + 120)) - 2)
    prop('models/ginsmodels/bench3.mdl', px - 120, jy0 - 24, yaw=90, z=int(ground_z(px - 120, jy0 - 24)))
    FURNITURE.append((px - 150, jy0 - 44, px - 90, jy0 - 4))
    random_sounds(px, py, 0, ['hunger/thambs/frog.wav', 'hunger/thambs/frog.wav', 'hunger/thambs/splash.wav', 'ambience/wren1.wav'],
                  5, 16, 0.6, 1.1, spread=160)
    # Half-Life's leeches in the deep water: they nip at swimmers
    for lx, ly in ((px - 40, py + 30), (px + 60, py - 10)):
        ents.append(entity({'classname': 'monster_leech', 'origin': '%d %d %d' % (lx, ly, int(ground_z(lx, ly)) + 30),
                            'angles': '0 %d 0' % prng.randint(0, 359)}))

    # ------------------------------------------------ the gas station: forecourt, canopy and pumps, kiosk, service bays
    slab(720, -600, 1360, -128, 'OUT_SIDEWALK6', CONCRETE, 0, 2, cut='OUT_CONCRETE1')
    dbox((816, -480, 184), (1120, -256, 208), {'top': 'OUT_ROOF2', 'bottom': 'E_WALL_WHITE1', 'side': 'barrel_red_01'}, METAL)
    for cx, cy in ((848, -448), (1088, -448), (848, -288), (1088, -288)):
        dbox((cx - 8, cy - 8, 2), (cx + 8, cy + 8, 184), 'BA_STEEL_01', METAL)
        FURNITURE.append((cx - 8, cy - 8, cx + 8, cy + 8))
    for lx, ly in ((900, -420), (1040, -420), (900, -316), (1040, -316)):
        light(lx, ly, 176, (255, 250, 235, 120))
    for ix in (896, 1040):
        dbox((ix - 20, -440, 2), (ix + 20, -296, 8), {'top': 'OUT_CONCRETE1', 'side': 'IN_CURB1'}, CONCRETE)
        for py in (-400, -336):
            dbox((ix - 14, py - 10, 8), (ix + 14, py + 10, 68), {'all': 'barrel_red_01', 'top': 'BA_STEEL_01', 'x+': 'H2OTANK_FRONT',
                                                                'x-': 'H2OTANK_FRONT'}, METAL)
        FURNITURE.append((ix - 20, -440, ix + 20, -296))
    ambient(968, -368, 170, 'ambience/electrical_hum1.wav', 1, 'small')
    # the price pylon by the road
    for px in (708, 780):
        dbox((px - 3, -183, 2), (px + 3, -177, 200), 'BA_STEEL_01', METAL)
    board(696, -184, 136, 792, -176, 200, 'sign_gas', ['y-', 'y+'], 96, 64)
    # the kiosk
    building(1040, -840, 1184, -600, 176, 'PRXMSBRICK1A', 'kiosk', 'y+', windows=2, roof='DOUBLE_ASPHALT', door_tex='OFF_DR3',
             light=(255, 248, 230, 140))
    shelf(1060, -760, 1072, -680, h=72, tex='M_WOOD1', mat=PLANKS)
    dbox((1150, -830, 8), (1168, -740, 104), {'all': 'BA_STEEL_01', 'x-': 'ba_freezer_01'}, METAL)            # cooler
    counter(1080, -660, 1150, -640)
    for vx, tex in ((1048, 'GEN_VEND1'), (1146, 'pepsifront')):
        dbox((vx, -598, 2), (vx + 28, -578, 90), {'all': 'BA_STEEL_01', 'y+': tex}, METAL,
             **{'y+': fit((1, 0, 0), (0, 0, -1), vx, 90, 28, 88, 64, 96) if tex == 'GEN_VEND1' else fit((1, 0, 0), (0, 0, -1), vx, 90, 28, 88, 128, 256)})
        FURNITURE.append((vx, -598, vx + 28, -578))
    ambient(1062, -588, 50, 'ambience/vendmachine.wav', 2, 'small')
    ambient(1158, -785, 60, 'ambience/freezer_fan.wav', 1, 'small')
    # the service bays: galvanised walls, two roll-up doors facing the forecourt
    bx0, by0, bx1, by1, bh = 1184, -840, 1360, -600, 176
    BUILDINGS.append((bx0, by0, bx1, by1))
    dbox((bx0, by0, 0), (bx1, by1, 8), {'top': 'CONCRETETILE', 'side': 'OUT_GALV1'}, CONCRETE, cut='OUT_CONCRETE1')
    dbox((bx0, by0, bh), (bx1, by1, bh + 16), {'top': 'DOUBLE_ASPHALT', 'bottom': 'OUT_GALV1', 'side': 'OUT_GALV1'}, METAL)
    for b in wall('x', by0, by0 + 16, bx0, bx1, bh, [], 'OUT_GALV1', 'IN_WALL16', '-') + \
             wall('y', bx1 - 16, bx1, by0 + 16, by1 - 16, bh, [], 'OUT_GALV1', 'IN_WALL16', '+') + \
             wall('x', by1 - 16, by1, bx0, bx1, bh, [(1192, 1272, 8, 104), (1280, 1352, 8, 104)], 'PRXMSBRICK1A', 'IN_WALL16', '+'):
        dyn.append((b, METAL, 'OUT_GALV1'))
    for dx0, dx1 in ((1192, 1272), (1280, 1352)):
        panel = box((dx0, by1 - 10, 8), (dx1, by1 - 6, 104), 'DOOR_GARAGE_02',
                    **{f: fit((1, 0, 0), (0, 0, -1), dx0, 104, dx1 - dx0, 96, 192, 144) for f in ('y-', 'y+')})
        ents.append(entity({'classname': 'func_door', 'angles': '-90 0 0', 'speed': '60', 'wait': '8', 'lip': '12', 'movesnd': '1',
                            'stopsnd': '1'}, [panel]))
        DOORS.append(((dx0 + dx1) // 2, by1, 0, 1))
    light(1272, -720, bh - 20, (255, 244, 220, 140))
    dbox((bx0 + 8, by0 + 16, 8), (bx0 + 72, by0 + 40, 44), {'top': 'M_WOOD1', 'all': 'WORKBENCH'}, PLANKS)    # bench
    board(bx0 + 8, by0 + 16, 52, bx0 + 136, by0 + 18, 116, 'TOOLS', ['y+'], 256, 128)
    FURNITURE.append((bx0 + 8, by0 + 16, bx0 + 72, by0 + 40))
    for tx, ty in ((1240, -700), (1240, -660)):
        for k in range(3):
            post(tx, ty, 14, 8 + 10 * k, 18 + 10 * k, 'TIRE_01', 23, sides=10)
        FURNITURE.append((tx - 14, ty - 14, tx + 14, ty + 14))
    car(1316, -720, True, 'white', z=16)                                                                     # up on blocks
    for cx, cy in ((1294, -790), (1338, -790), (1294, -650), (1338, -650)):
        dbox((cx - 8, cy - 8, 8), (cx + 8, cy + 8, 16), 'ROCK_GREY', COBBLE)
    prop('models/hunger/item_gascan.mdl', 1200, -640, z=8)
    prop('models/ginsmodels/gins_drums.mdl', 1376, -580, yaw=30, z=2)
    prop('models/tool_box.mdl', bx0 + 40, by0 + 28, yaw=90, z=44)
    ambient(1272, -760, 90, 'ambience/hammer.wav', 1, 'small')
    slab(1040, -1000, 1400, -840, 'Gravel01')                                                                # the back lot
    masked([box((1040, -1000, 0), (1400, -998, 96), '{HLXFENCE1', **{f: {'scale': (0.5, 0.5)} for f in ('y-', 'y+')}),
            box((1398, -998, 0), (1400, -840, 96), '{HLXFENCE1', **{f: {'scale': (0.5, 0.5)} for f in ('x-', 'x+')})])

    # ------------------------------------------------ street furniture
    # power poles along the south verge, cables between their crossarms
    poles = [-1344, -896, -448, 0, 448, 740, 1380]
    for px in poles:
        prop('models/snd/pole.mdl', px, -232, yaw=90, z=0)
    for pa, pb in zip(poles, poles[1:]):
        for cy in (-312, -152):
            masked([box((pa, cy - 1, 420), (pb, cy + 1, 470), '{PRXCABLES1',
                        **{f: fit((1, 0, 0), (0, 0, -1), pa, 470, pb - pa, 50, 128, 64) for f in ('y-', 'y+')})], solid=False)
    ambient(0, -232, 400, 'ambience/electrical_hum1.wav', 1, 'small')
    # fire hydrants
    for hx, hy, hz in ((-760, 172, 8), (-240, -172, 8), (232, 172, 8), (660, -172, 8), (1120, -150, 2)):
        post(hx, hy, 6, hz, hz + 26, 'barrel_red_01')
        post(hx, hy, 8, hz + 26, hz + 30, 'barrel_red_01')
        dbox((hx - 9, hy - 2, hz + 14), (hx + 9, hy + 2, hz + 20), 'barrel_red_01', METAL)
        FURNITURE.append((hx - 9, hy - 9, hx + 9, hy + 9))
    # dumpsters, with their flies
    for dx, dy in ((-560, 744), (-60, 620), (440, -760), (1300, -870)):
        dbox((dx - 48, dy - 24, 1), (dx + 48, dy + 24, 50), {'all': 'IN_DUMPSTER1', 'x+': 'OUT_DMP1C', 'x-': 'OUT_DMP1C', 'top': 'OUT_DMPLID'},
             METAL, **{'top': {'scale': (1.5, 1)}})
        FURNITURE.append((dx - 48, dy - 24, dx + 48, dy + 24))
        ambient(dx, dy, 60, 'ambience/flies.wav', 2, 'small')
    # the back alley behind the office and the shop
    slab(-1400, 712, -272, 776, 'OUT_ASPHALT1', ASPHALT, 0, 2)
    # a phone booth at the gas station, glass you can break
    bx, by = 1320, -200
    dbox((bx - 20, by - 20, 2), (bx + 20, by + 20, 4), 'BA_STEEL_01', METAL)
    dbox((bx - 20, by - 20, 96), (bx + 20, by + 20, 100), 'BA_STEEL_01', METAL)
    for cx, cy in ((-20, -20), (16, -20), (-20, 16), (16, 16)):
        dbox((bx + cx, by + cy, 4), (bx + cx + 4, by + cy + 4, 96), 'BA_STEEL_01', METAL)
    dbox((bx - 16, by + 16, 4), (bx + 16, by + 20, 96), {'all': 'BA_STEEL_01', 'y-': 'nm_tely2'}, METAL)
    for gb in (box((bx - 16, by - 20, 4), (bx + 16, by - 18, 96), 'GLASS_MED'), box((bx - 20, by - 16, 4), (bx - 18, by + 16, 96), 'GLASS_MED')):
        ents.append(entity({'classname': 'func_breakable', 'material': '0', 'health': '20', 'rendermode': '2', 'renderamt': '120'}, [gb]))
    FURNITURE.append((bx - 20, by - 20, bx + 20, by + 20))
    random_sounds(bx, by, 60, ['hunger/thambs/telephone.wav'], 60, 180, 0.5, 1.5)
    # a bus stop on the south side
    dbox((-200, -236, 96), (-72, -164, 100), 'OUT_GALV1', METAL)
    for cx in (-200, -76):
        dbox((cx, -236, 0), (cx + 4, -232, 96), 'BA_STEEL_01', METAL)
    ents.append(entity({'classname': 'func_breakable', 'material': '0', 'health': '20', 'rendermode': '2', 'renderamt': '120'},
                       [box((-196, -236, 30), (-76, -233, 92), 'GLASS_MED')]))
    prop('models/ginsmodels/bench3.mdl', -136, -220, yaw=90, z=0)
    dbox((-61, -201, 8), (-59, -199, 120), 'BA_STEEL_01', METAL)
    board(-72, -202, 120, -48, -198, 152, 'sign_bus', ['y+', 'y-'], 48, 64)
    # signs: stop at the end of Elm St, speed limits at the town's ends, a mailbox by the police station
    for sx, sy, tex, faces, tw, th, sw, shh in ((32, 212, 'sign_stop', ['y+'], 64, 64, 32, 32), (-1380, -220, 'sign_speed', ['x-'], 48, 64, 24, 32),
                                                (1380, 220, 'sign_speed', ['x+'], 48, 64, 24, 32)):
        dbox((sx - 1, sy - 1, 0 if abs(sx) > 1000 else 8), (sx + 1, sy + 1, 104), 'BA_STEEL_01', METAL)
        if faces[0][0] == 'y':
            board(sx - sw // 2, sy + 1, 104, sx + sw // 2, sy + 3, 104 + shh, tex, faces, tw, th)
        else:
            board(sx + (1 if faces[0] == 'x+' else -3), sy - sw // 2, 104, sx + (3 if faces[0] == 'x+' else -1), sy + sw // 2, 104 + shh,
                  tex, faces, tw, th)
    dbox((514, 166, 8), (526, 178, 28), 'BA_STEEL_01', METAL)
    dbox((510, 164, 28), (530, 180, 60), {'all': 'barrel_blu_01', 'y-': 'barrel_blu_01'}, METAL)
    board(513, 162, 44, 527, 164, 51, 'sign_mail', ['y-'], 32, 16)
    # manholes
    for mx, my in ((-600, 40), (120, 600), (900, -40)):
        post(mx, my, 24, 2, 3, 'OUT_MANHOLE2', ASPHALT, sides=12)
        # steam from the sewer (sc_emitter, game/dlls/svencraft/sc_emitter.cpp)
        ents.append(entity({'classname': 'sc_emitter', 'origin': '%d %d 6' % (mx, my), 'kind': '0', 'rate': '4'}))
    # Elm St: lamps, street trees
    for ly in (420, 700):
        g['world'].append(box((228, ly - 4, 8), (236, ly + 4, 200), 'car_chrome'))           # like the main road's lamps
        g['world'].append(box((216, ly - 10, 200), (248, ly + 10, 210), {'all': 'car_chrome', 'bottom': 'streetlight1'}))
        light(232, ly, 190, (255, 220, 160, 110))
    for ty in (360, 600):
        prop('models/hunger/vegitation/zalec_tree1.mdl', 220, ty, z=8, solid=False)
    # the checkpoint by the rift (for the alliance phase): a line of barriers
    for k in range(5):
        prop('models/snd/barrier.mdl', 630, 340 + 96 * k, yaw=0, z=None)

    # ------------------------------------------------ the town's air: birds, wind on the hills, a dog somewhere
    for x, y in ((-900, 200), (200, -300), (900, 600)):
        ambient(x, y, 160, 'tu3sday/forest_noise.wav', 2, 'large')
    for x, y in ((-1800, 0), (1800, 300), (0, 1800), (0, -1800)):
        ambient(x, y, 400, 'ambience/wind2.wav', 3, 'large')
    for x, y in ((-900, 400), (400, 300), (0, -800)):
        random_sounds(x, y, 200, ['ambience/wren1.wav', 'hunger/thambs/bird1.wav', 'ambience/hawk1.wav', 'ambience/quail1.wav',
                                  'hunger/thambs/crow.wav'], 6, 20, 0.6, 0.9, spread=500)
    random_sounds(-1700, 1500, 200, ['hunger/thambs/dogamb1.wav'], 40, 100, 0.8, 0.5)
    random_sounds(1500, -1500, 300, ['ambience/jetflyby1.wav', 'misc/truck_stop.wav'], 90, 200, 0.7, 0.4)
    # the office's and the shop's hum
    ambient(-1000, 560, 200, 'ambience/fluorescent.wav', 2, 'small')
    ambient(-480, 520, 160, 'hunger/thambs/radiomusic.wav', 2, 'small')
    ambient(-944, -600, 200, 'ambience/industrial2.wav', 1, 'medium')
