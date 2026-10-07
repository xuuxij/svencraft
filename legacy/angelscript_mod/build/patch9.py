p = r".\makesandbox.py"
s = open(p).read()
rep = [
    ("SCALE = 0.5   # 64px texture covers one 32-unit block", "BLOCK = 40     # world units per block (must match SC_BLOCK_SIZE in sc_blocks.as)\nSCALE = BLOCK / 64.0   # 64px texture covers one block"),
    ("H, TOP, CEIL = 1024, 1536, 1552     # half-size of the play area, sky height\nFLOOR = -1040                       # bottom of the hidden template room",
     "H, TOP, CEIL = 32 * BLOCK, 1536, 1552   # half-size of the play area (64x64 blocks), sky height\nBEDROCK = -11 * BLOCK                   # top of the bedrock = bottom of the terrain (11 blocks deep)\nFLOOR = -1056                           # bottom of the hidden template room"),
    ("box((-H, -H, -416), (H, H, -352), {'top': 'sc_bedrock'", "box((-H, -H, BEDROCK - 64), (H, H, BEDROCK), {'top': 'sc_bedrock'"),
    ("out += ent({'classname': 'info_player_start', 'origin': '0 0 40', 'angles': '0 90 0'})", "out += ent({'classname': 'info_player_start', 'origin': '0 0 48', 'angles': '0 90 0'})"),
    ("'origin': '%d %d 40' % (x, y), 'angles': '0 90 0'})", "'origin': '%d %d 48' % (x, y), 'angles': '0 90 0'})"),
    ("x, y, rowd = -H + 64, -H + 64, 0\nzb = FLOOR + 16 + 96                         # template bottoms float above the room floor (multiple of 32)",
     "x, y, rowd = -H + 2 * BLOCK, -H + 2 * BLOCK, 0\nzb = -24 * BLOCK                             # template bottoms float above the room floor (grid aligned)"),
    ("    w, d = sx * 32, sy * 32\n    if x + w > H - 64:\n        x, y, rowd = -H + 64, y + rowd + 96, 0\n    mn = (x, y, zb); mx = (x + w, y + d, zb + sz * 32)",
     "    w, d = sx * BLOCK, sy * BLOCK\n    if x + w > H - 2 * BLOCK:\n        x, y, rowd = -H + 2 * BLOCK, y + rowd + 3 * BLOCK, 0\n    mn = (x, y, zb); mx = (x + w, y + d, zb + sz * BLOCK)"),
    ("    x += w + 96; rowd = max(rowd, d)", "    x += w + 3 * BLOCK; rowd = max(rowd, d)"),
    ("assert zb % 32 == 0", "assert zb % BLOCK == 0 and zb > FLOOR + 16 and zb + 8 * BLOCK < BEDROCK - 64"),
]
for a, b in rep:
    assert a in s, a[:60]
    s = s.replace(a, b)
open(p, 'w').write(s)
print('ok')
