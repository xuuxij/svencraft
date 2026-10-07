# Offline estimate of how many terrain node entities a cave design costs (same octree split as sc_sandbox.as).
import random
SIZES = {'grass': {(8, 8, 1), (4, 4, 1), (2, 2, 1), (1, 1, 1)}, 'dirt': {(8, 8, 2), (4, 4, 1), (2, 2, 1), (1, 1, 1)},
         'stone': {(8, 8, 8), (4, 4, 4), (2, 2, 2), (1, 1, 1)}}


def pieces(mat, x, y, z, sx, sy, sz, air):
    """count of nodes for box minus air cells"""
    cells = [(x + i, y + j, z + k) for i in range(sx) for j in range(sy) for k in range(sz)]
    n_air = sum(c in air for c in cells)
    if n_air == len(cells):
        return 0
    if n_air == 0 and (sx, sy, sz) in SIZES[mat]:
        return 1
    if sx == sy == sz == 1:
        return 1
    hx, hy, hz = max(sx // 2, 1), max(sy // 2, 1), max(sz // 2, 1)
    return sum(pieces(mat, x + i, y + j, z + k, hx, hy, hz, air)
               for i in range(0, sx, hx) for j in range(0, sy, hy) for k in range(0, sz, hz))


def world(air):
    total = 0
    for x in range(-32, 32, 8):
        for y in range(-32, 32, 8):
            total += pieces('grass', x, y, -1, 8, 8, 1, air)
            total += pieces('dirt', x, y, -3, 8, 8, 2, air)
            total += pieces('stone', x, y, -11, 8, 8, 8, air)
    return total


def macro(air, x, y, z, s=2):
    for i in range(s):
        for j in range(s):
            for k in range(s):
                air.add((x + i, y + j, z + k))


def tunnel(air, rng, x, y, z, steps):
    """worm of 2x2x2 macro cells on the even/odd lattice (x,y even; z in -11,-9,-7)"""
    d = rng.choice([(2, 0), (-2, 0), (0, 2), (0, -2)])
    for _ in range(steps):
        macro(air, x, y, z)
        if rng.random() < 0.3:
            d = rng.choice([(2, 0), (-2, 0), (0, 2), (0, -2)])
        if rng.random() < 0.15:
            z = max(-11, min(-7, z + rng.choice([-2, 2])))
        x = max(-30, min(28, x + d[0])); y = max(-30, min(28, y + d[1]))
    return x, y, z


def stairs(air, x, y, dx, dy, depth, width=2, head=3):
    """staircase from the surface going down one block per step along (dx,dy)"""
    px, py = -dy, dx
    for k in range(1, depth + 1):
        cx, cy = x + dx * k, y + dy * k
        for w in range(width):
            for h in range(head):
                z = -k + h
                if z <= -1:
                    air.add((cx + px * w, cy + py * w, z))


base = world(set())
print('flat world:', base)
rng = random.Random(7)
for trial in range(5):
    air = set()
    # entrance stairs from (4,4) heading +x down to the tunnel floor at -9
    stairs(air, 2, 4, 1, 0, 9)
    ex, ey = 2 + 9, 4
    ex -= ex % 2; ey -= ey % 2
    end = tunnel(air, rng, ex, ey, -9, 14)
    macro(air, (end[0] // 4) * 4, (end[1] // 4) * 4, -11, 4)   # room at the end
    n = len(air)
    print('trial', trial, 'air cells', n, 'pieces', world(air) - base, '(stairs only %d)' % (world({c for c in air if c[0] <= 11}) - base))
