# Generates sc_modelinfo.as: bounds + material mix for every studio model used as a prop by any installed map.
# The server can't read model bounds at runtime (SetModel leaves mins/maxs at 0 for studio models).
import os, re, struct, sys, collections
ROOT = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop"
DIRS = ['svencoop_addon', 'svencoop_downloads', 'svencoop']   # search order, like the game
PROP_CLASSES = {'item_generic', 'monster_furniture', 'cycler', 'cycler_sprite', 'cycler_wreckage', 'sc_prop'}
OUT = sys.argv[1] if len(sys.argv) > 1 else 'sc_modelinfo.as'

# Same keyword rules as SC_MIX_RULES in sc_props.as (kept in sync by hand); texture names are checked too.
RULES = [
    ("tire|tyre|wheel", "rubber:1"),
    ("tree|palm|trunk|stump", "log:2,leaves:1"),
    ("bush|fern|plant|flower|leaf|leaves|grass|vegit|veget|shrub|hedge|ivy|vine|cactus|weed", "leaves:1"),
    ("rock|stone|boulder|cliff|pebble", "cobble:1"),
    ("ground|dirt|mud|soil|terrain", "dirt:1"),
    ("fungus|mushroom|moss", "leaves:1"),
    ("sandbag|dune|sand", "sand:1"),
    ("brick", "brick:1"),
    ("computer|monitor|television|console|phone|radio|keyboard|server|terminal|screen|holo|machine|panel|electr|camera|speaker|arcade|charger|recharge", "circuit:2,glass:1,metal:1"),
    ("lamp|light|bulb|lantern|chandelier", "glass:1,metal:1"),
    ("bottle|glass|window|jar", "glass:1"),
    ("vase|pot|ceramic|toilet|sink|urinal|bathtub|mug|cup|plate|bowl", "tile:1"),
    ("chair|stool|bench|table|desk|shelf|cabinet|crate|pallet|bed|bookcase|book|wood|plank|door|fence|awning|piano|coffin|sign|easel|sofa|couch|carpet|curtain|tent|flag|cloth|fabric", "planks:1"),
    ("car|truck|jeep|bus|taxi|sedan|vehicle|humvee|apc|forklift|tractor|motorbike|heli|osprey|plane|apache|chopper|blackhawk|boat|wagon", "metal:3,rubber:1,glass:1"),
    ("barrel|drum|locker|oxygen|cylinder|pipe|fan|metal|steel|iron|rail|pole|tank|generator|dumpster|mailbox|hydrant|vent|beam|girder|cage|grate|turret|gun|chrome|rust|toolbox|tool|spray|bomb|grenade|ammo|weapon", "metal:1"),
    ("barrier|jersey|concrete|cement|pillar|column|statue|wall", "concrete:1"),
    ("snow|ice", "snow:1"),
    ("tile", "tile:1"),
    ("bone|skull|skeleton|rib|gib|pelvis|corpse|body|flesh", ""),
]

def classify(text):
    for keys, mix in RULES:
        for k in keys.split('|'):
            if k in text:
                return mix
    return None

def find_model(rel):
    for d in DIRS:
        p = os.path.join(ROOT, d, rel.replace('/', os.sep))
        if os.path.exists(p):
            return p
    return None

def model_info(path):
    d = open(path, 'rb').read()
    if d[:4] != b'IDST':
        return None
    ints = struct.unpack_from('<i26i', d, 136)
    numseq, seqindex, numtex, texindex = ints[7], ints[8], ints[11], ints[12]
    mins = maxs = None
    if numseq > 0:
        bb = struct.unpack_from('<6f', d, seqindex + 96)
        if any(abs(v) > 0.01 for v in bb):
            mins, maxs = bb[:3], bb[3:]
    try:
        from mdlbounds import mesh_bounds
        mb = mesh_bounds(path)
        if mb:
            mins, maxs = mb                          # vertex bounds beat the (often wrong) sequence bbox
    except Exception:
        pass
    if mins is None:
        hb = struct.unpack_from('<12f', d, 88)   # min, max, bbmin, bbmax
        for a in (6, 0):
            if any(abs(v) > 0.01 for v in hb[a:a + 6]):
                mins, maxs = hb[a:a + 3], hb[a + 3:a + 6]
                break
    texnames = []
    tpath = path
    if numtex == 0 and os.path.exists(path[:-4] + 'T.mdl'):   # textures stored in a separate xxxT.mdl
        tpath = path[:-4] + 'T.mdl'
        td = open(tpath, 'rb').read()
        tints = struct.unpack_from('<i26i', td, 136)
        numtex, texindex, d = tints[11], tints[12], td
    for i in range(numtex):
        o = texindex + i * 80
        texnames.append(d[o:o + 64].split(b'\0')[0].decode('latin-1').lower())
    return mins, maxs, texnames

# Models used by prop entities in all installed maps
used = collections.Counter()
for sub in DIRS:
    md = os.path.join(ROOT, sub, 'maps')
    if not os.path.isdir(md):
        continue
    for f in os.listdir(md):
        if not f.lower().endswith('.bsp'):
            continue
        b = open(os.path.join(md, f), 'rb').read()
        off, ln = struct.unpack_from('<ii', b, 4)
        txt = b[off:off + ln].decode('latin-1', 'replace')
        for blk in re.findall(r'\{([^{}]*)\}', txt):
            kv = dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', blk))
            m = kv.get('model', '').lower().replace('\\', '/')
            if kv.get('classname') in PROP_CLASSES and m.endswith('.mdl'):
                used[m] += 1

rows, missing, byTex = [], 0, 0
for m in sorted(used):
    p = find_model(m)
    if not p:
        missing += 1
        continue
    info = model_info(p)
    if not info or info[0] is None:
        continue
    mins, maxs, tex = info
    mix = classify(m)
    if mix is None and tex:
        votes = collections.Counter(classify(t) for t in tex)
        votes.pop(None, None)
        if votes:
            mix = votes.most_common(1)[0][0]
            byTex += 1
    rows.append((m, mins, maxs, '' if mix is None else mix, mix is None))

with open(OUT, 'w', newline='\n') as f:
    f.write('// Generated by genmodelinfo.py: bounds and material mix of studio models used as props. Do not edit.\n')
    f.write('// Mix "?" means unknown (use runtime rules); "" means the model yields nothing.\n')
    f.write('const array<string> SC_MODEL_NAMES = {\n')
    for r in rows:
        f.write('\t"%s",\n' % r[0])
    f.write('};\nconst array<float> SC_MODEL_BOUNDS = {\n')
    for r in rows:
        f.write('\t' + ', '.join('%.1f' % v for v in (*r[1], *r[2])) + ',\n')
    f.write('};\nconst array<string> SC_MODEL_MIX = {\n')
    for r in rows:
        f.write('\t"%s",\n' % ('?' if r[4] else r[3]))
    f.write('};\n')
print('%d prop models used, %d written, %d missing on disk, %d classified by texture names, %d unknown' %
      (len(used), len(rows), missing, byTex, sum(1 for r in rows if r[4])))
for r in rows[:6]:
    print(' ', r[0], [round(v) for v in (*r[1], *r[2])], r[3] or '-')
print('unknown examples:', [r[0] for r in rows if r[4]][:15])
