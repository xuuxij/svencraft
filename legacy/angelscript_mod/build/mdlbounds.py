# Exact bounds of a GoldSrc studio model in its default pose, from the mesh vertices
# (sequence/header bounding boxes are often wrong or empty).
import struct, math

def _quat(ax, ay, az):
    sy, cy = math.sin(az * 0.5), math.cos(az * 0.5)
    sp, cp = math.sin(ay * 0.5), math.cos(ay * 0.5)
    sr, cr = math.sin(ax * 0.5), math.cos(ax * 0.5)
    return (sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy, cr * cp * cy + sr * sp * sy)

def _matrix(q, t):
    x, y, z, w = q
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y), t[0]],
            [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x), t[1]],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y), t[2]]]

def _concat(a, b):
    out = [[0.0] * 4 for _ in range(3)]
    for i in range(3):
        for j in range(4):
            out[i][j] = sum(a[i][k] * b[k][j] for k in range(3)) + (a[i][3] if j == 3 else 0)
    return out

def mesh_bounds(path):
    """(mins, maxs) of all body-part vertices (first sub-model of each body part), or None."""
    d = open(path, 'rb').read()
    if d[:4] != b'IDST':
        return None
    ints = struct.unpack_from('<i26i', d, 136)
    nb, bi, nbp, bpi = ints[1], ints[2], ints[17], ints[18]
    mats = []
    for i in range(nb):
        o = bi + 112 * i
        parent = struct.unpack_from('<i', d, o + 32)[0]
        val = struct.unpack_from('<6f', d, o + 64)
        m = _matrix(_quat(val[3], val[4], val[5]), val[:3])
        mats.append(_concat(mats[parent], m) if parent >= 0 else m)
    lo, hi = [1e9] * 3, [-1e9] * 3
    for b in range(nbp):
        nm, base, mi = struct.unpack_from('<3i', d, bpi + 76 * b + 64)
        if nm < 1:
            continue
        f = struct.unpack_from('<if10i', d, mi + 64)   # first sub-model of the body part
        nverts, vinfo, vidx = f[4], f[5], f[6]
        for v in range(nverts):
            bone = d[vinfo + v]
            x, y, z = struct.unpack_from('<3f', d, vidx + 12 * v)
            m = mats[bone]
            w = [m[i][0] * x + m[i][1] * y + m[i][2] * z + m[i][3] for i in range(3)]
            for i in range(3):
                lo[i] = min(lo[i], w[i]); hi[i] = max(hi[i], w[i])
    return (tuple(lo), tuple(hi)) if lo[0] < 1e8 else None

if __name__ == '__main__':
    import sys
    for p in sys.argv[1:]:
        print(p, mesh_bounds(p))
