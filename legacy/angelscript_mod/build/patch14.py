p = r".\makesandbox.py"
s = open(p).read()
reps = [
    # 1. shaded texture variants in the WAD (palette scaled; masked index 255 untouched)
    ("""lumps = []
for wname, src in WADTEX.items():
    img = bedrock() if src is None else Image.open(os.path.join(TEX, src + '.bmp'))
    lumps.append((wname, miptex(wname, img)))""",
     """# Minecraft-style face shading baked into textures: _x = sides facing X, _y = sides facing Y, _b = bottom.
SHADES = {'_x': 0.70, '_y': 0.85, '_b': 0.55}

def shaded(img, k):
    out = img.copy()
    pal = (img.getpalette() + [0] * 768)[:768]
    pal = [int(v * k) if i < 255 * 3 else v for i, v in enumerate(pal)]
    out.putpalette(pal)
    return out

lumps = []
for wname, src in WADTEX.items():
    img = bedrock() if src is None else Image.open(os.path.join(TEX, src + '.bmp'))
    lumps.append((wname, miptex(wname, img)))
    if src is not None:
        for suf, k in SHADES.items():
            lumps.append((wname + suf, miptex(wname + suf, shaded(img, k))))"""),
    # 2. per-axis side textures in box()
    ("""        t = tex['top'] if n[2] > 0 else tex['bottom'] if n[2] < 0 else tex['side']""",
     """        t = tex['top'] if n[2] > 0 else tex['bottom'] if n[2] < 0 else (tex.get('sx', tex.get('side')) if n[0] else tex.get('sy', tex.get('side')))"""),
    # 3. template room sealed from the sky: lit only by _minlight -> every face uniform
    ("""    box((-H, -H, BEDROCK - 64), (H, H, BEDROCK), {'top': 'sc_bedrock', 'side': 'sc_bedrock', 'bottom': 'sky'}),  # bedrock; sky below lights templates""",
     """    box((-H, -H, BEDROCK - 64), (H, H, BEDROCK), 'sc_bedrock'),                                     # bedrock (seals the template room)
    box((-H, -H, FLOOR + 16), (-H + 16, H, BEDROCK - 64), 'sc_bedrock'),                           # template room walls: no sky,
    box((H - 16, -H, FLOOR + 16), (H, H, BEDROCK - 64), 'sc_bedrock'),                             # so templates get only uniform
    box((-H + 16, -H, FLOOR + 16), (H - 16, -H + 16, BEDROCK - 64), 'sc_bedrock'),                 # _minlight and tile seamlessly
    box((-H + 16, H - 16, FLOOR + 16), (H - 16, H, BEDROCK - 64), 'sc_bedrock'),"""),
    ("""    kv = {'classname': 'func_wall', 'targetname': 'sctpl_%s_%d_%d_%d' % (m, sx, sy, sz), '_minlight': '0.55'}""",
     """    kv = {'classname': 'func_wall', 'targetname': 'sctpl_%s_%d_%d_%d' % (m, sx, sy, sz), '_minlight': '0.6'}"""),
    ("""    out += ent(kv, [box(mn, mx, {'top': top, 'side': side, 'bottom': bot}),""",
     """    out += ent(kv, [box(mn, mx, {'top': top, 'sx': side + '_x', 'sy': side + '_y', 'bottom': bot + '_b'}),"""),
]
for a, b in reps:
    assert a in s, a[:70]
    s = s.replace(a, b)
open(p, 'w').write(s)
print('ok')
