import struct, sys
d = open(sys.argv[1], 'rb').read()
ints = struct.unpack_from('<i26i', d, 136)
bpi = ints[18]
nm, base, mi = struct.unpack_from('<3i', d, bpi+64)
f = struct.unpack_from('<if10i', d, mi+64)
nummesh, meshindex, numverts, vinfo, vidx, numnorms, ninfo, nidx = f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]
V = [struct.unpack_from('<3f', d, vidx + 12*i) for i in range(numverts)]
N = [struct.unpack_from('<3f', d, nidx + 12*i) for i in range(numnorms)]
print('verts', [tuple(round(c,1) for c in v) for v in V]); print('norms', [tuple(round(c,2) for c in n) for n in N])
def sub(a,b): return [a[i]-b[i] for i in range(3)]
def cross(a,b): return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]
for k in range(nummesh):
    numtris, triindex, skinref, _, _ = struct.unpack_from('<5i', d, meshindex + 20*k)
    o = triindex
    while True:
        c, = struct.unpack_from('<h', d, o); o += 2
        if c == 0: break
        fan = c < 0; c = abs(c)
        pts = [struct.unpack_from('<4h', d, o + 8*i) for i in range(c)]; o += 8*c
        tris = []
        for i in range(2, c):
            if fan: tris.append((pts[0], pts[i-1], pts[i]))
            else: tris.append((pts[i-2], pts[i-1], pts[i]) if i % 2 == 0 else (pts[i-1], pts[i-2], pts[i]))
        for t in tris:
            a, b, cc = (V[p[0]] for p in t)
            n = N[t[0][1]]
            s = sum(x*y for x, y in zip(cross(sub(b,a), sub(cc,a)), n))
            print('fan' if fan else 'strip', 'dot(cross,normal)=%+.0f' % s, 'st', [(p[2],p[3]) for p in t])
