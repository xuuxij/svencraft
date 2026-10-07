# Test content for svencraft_sandbox: walls of every thickness, a house with doors and a window,
# breakable/pushable crates, a props yard, a forest and a car park. Terrain top is z = 0, spawn is at (0, 0).
import os, struct, math

ROOT = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop"
BRICK, CONC, WOOD, METAL = 'brick09', 'concrete_01', 'wood_panel_01', 'METALWALL_01'
CRATE, DOOR, GLASS = 'CRATE_04', 'DOOR_GREEN', 'GLASS_BRIGHT'

def model_bounds(rel):
    from mdlbounds import mesh_bounds
    for d in ('svencoop_addon', 'svencoop'):
        p = os.path.join(ROOT, d, rel.replace('/', os.sep))
        if os.path.exists(p):
            return mesh_bounds(p)   # from the vertices: sequence bboxes are often wrong
    raise FileNotFoundError(rel)

def prop(ent, model, x, y, yaw=0, solid=True, skin=0, scale=1.0, z=0):
    mn, mx = model_bounds(model)
    mn = [v * scale for v in mn]; mx = [v * scale for v in mx]
    oz = z - mn[2] if mn[2] > -16 else z             # sit on the ground (models with roots/basements keep their origin)
    kv = {'classname': 'sc_prop', 'model': model, 'origin': '%d %d %d' % (x, y, oz), 'angles': '0 %d 0' % yaw,
          'skin': str(skin), 'scale': '%g' % scale, 'sequence': '0'}
    if solid:
        # blocking hull = the model's box rotated by yaw (axis-aligned, as the engine requires)
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        xs = [mn[0], mx[0]]; ys = [mn[1], mx[1]]
        pts = [(px * c - py * s, px * s + py * c) for px in xs for py in ys]
        hmn = (min(p[0] for p in pts), min(p[1] for p in pts), mn[2])
        hmx = (max(p[0] for p in pts), max(p[1] for p in pts), mx[2])
        kv.update({'solid': '2', 'minhullsize': '%d %d %d' % tuple(round(v) for v in hmn),
                   'maxhullsize': '%d %d %d' % tuple(round(v) for v in hmx)})
    return ent(kv)

def build(box, ent):
    out = ''
    def wall(mn, mx, tex, cls='func_wall', extra=None, brushes=None):
        kv = {'classname': cls, '_minlight': '0.3'}
        kv.update(extra or {})
        return ent(kv, brushes or [box(mn, mx, tex, 1)])

    # --- North: wall row, one per thickness (32 cube / 16 slab / 8 plate / 4 sheet) + a pillar
    out += wall((-448, 320, 0), (-320, 352, 96), BRICK)
    out += wall((-256, 328, 0), (-128, 344, 96), CONC)
    out += wall((-64, 332, 0), (64, 340, 96), WOOD)
    out += wall((128, 334, 0), (256, 338, 96), METAL)
    out += wall((320, 320, 0), (352, 352, 128), BRICK)

    # --- North: small brick house (16-thick walls) at x -128..128, y 512..704, 128 tall
    out += wall((-128, 512, 0), (-32, 528, 128), BRICK)          # south wall, left of door
    out += wall((32, 512, 0), (128, 528, 128), BRICK)            # south wall, right of door
    out += wall((-32, 512, 112), (32, 528, 128), BRICK)          # lintel
    out += wall((-128, 688, 0), (128, 704, 128), BRICK)          # north wall
    out += wall((112, 528, 0), (128, 576, 128), BRICK)           # east wall around the window
    out += wall((112, 640, 0), (128, 688, 128), BRICK)
    out += wall((112, 576, 0), (128, 640, 48), BRICK)
    out += wall((112, 576, 96), (128, 640, 128), BRICK)
    out += wall((-128, 528, 0), (-112, 576, 128), BRICK)         # west wall around the side door
    out += wall((-128, 640, 0), (-112, 688, 128), BRICK)
    out += wall((-128, 576, 112), (-112, 640, 128), BRICK)
    out += wall((-128, 512, 128), (128, 704, 136), WOOD)         # roof (8 thick -> plates)
    # sliding front door (opens sideways on touch)
    out += wall(None, None, None, 'func_door', {'speed': '100', 'wait': '4', 'lip': '8', 'angles': '0 0 0', 'movesnd': '1', 'stopsnd': '1'},
                [box((-32, 516, 0), (32, 524, 112), DOOR, 1)])
    # hinged side door with an origin brush at the hinge
    out += wall(None, None, None, 'func_door_rotating', {'speed': '100', 'wait': '4', 'distance': '90', 'movesnd': '1', 'stopsnd': '1'},
                [box((-124, 578, 0), (-116, 638, 112), DOOR, 1), box((-124, 578, 52), (-116, 586, 60), 'ORIGIN', 1)])
    # breakable glass window
    out += wall(None, None, None, 'func_breakable', {'material': '0', 'health': '15', 'rendermode': '2', 'renderamt': '140'},
                [box((118, 576, 48), (122, 640, 96), GLASS, 1)])
    # furniture inside
    out += prop(ent, 'models/chair.mdl', 40, 600, 180)
    out += prop(ent, 'models/ginsmodels/gins_table20.mdl', -20, 620)
    out += prop(ent, 'models/adamr/ceiling_lamp.mdl', 0, 600, solid=False, z=90)

    # --- Crates beside the house: breakable wood crates (stacked) and a pushable crate
    for x, y, z in ((192, 544, 0), (232, 544, 0), (192, 584, 0), (212, 564, 32)):
        out += wall(None, None, None, 'func_breakable', {'material': '1', 'health': '30'}, [box((x, y, z), (x + 32, y + 32, z + 32), CRATE, 1)])
    out += wall(None, None, None, 'func_pushable', {'friction': '50', 'buoyancy': '20'}, [box((272, 600, 0), (320, 648, 48), CRATE, 1)])

    # --- East: props yard
    yard = ['models/chair.mdl', 'models/ginsmodels/gins_table20.mdl', 'models/snd/woodbarrel.mdl', 'models/mbarrel.mdl',
            'models/snd/vase1.mdl', 'models/sandbags.mdl', 'models/mil_crate.mdl', 'models/adamr/toolbox.mdl',
            'models/ginsmodels/bench3.mdl', 'models/ginsmodels/gins_deskchair.mdl', 'models/big_rock.mdl',
            'models/forklift_static.mdl', 'models/snd/tank_body.mdl']
    for i, m in enumerate(yard[:10]):
        out += prop(ent, m, 448 + (i % 5) * 96, -96 + (i // 5) * 128)
    out += prop(ent, 'models/big_rock.mdl', 512, 224)
    out += prop(ent, 'models/forklift_static.mdl', 720, 224, 90)
    out += prop(ent, 'models/snd/tank_body.mdl', 640, -352)

    # --- South: forest of model trees and bushes (non-solid canopy, like real maps)
    trees = [('models/hunger/vegitation/tree1.mdl', -96, -448), ('models/hunger/vegitation/tree2.mdl', 96, -480),
             ('models/ginsmodels/lag_oaktree.mdl', 256, -608), ('models/ginsmodels/palmtree.mdl', -256, -640),
             ('models/hunger/vegitation/bush1.mdl', 0, -360), ('models/hunger/vegitation/fern1.mdl', 160, -380),
             ('models/hunger/vegitation/bush2.mdl', -192, -400)]
    for m, x, y in trees:
        out += prop(ent, m, x, y, solid=False)

    # --- West: car park
    out += prop(ent, 'models/svencraft/car.mdl', -480, -96, 0, skin=0)
    out += prop(ent, 'models/svencraft/car.mdl', -480, 64, 0, skin=1)
    out += prop(ent, 'models/svencraft/car.mdl', -672, -16, 90, skin=2)
    return out
