import struct, sys, collections
d = open(sys.argv[1], 'rb').read()
ints = struct.unpack_from('<i26i', d, 136)
nb, bi = ints[1], ints[2]; ntex, texi = ints[11], ints[12]; nskinref, nfam, skini = ints[14], ints[15], ints[16]; nbp, bpi = ints[17], ints[18]
bones = [d[bi + 112 * i:bi + 112 * i + 32].split(b'\0')[0].decode() for i in range(nb)]
print('bones:', ', '.join(f'{i}:{b}' for i, b in enumerate(bones)))
texs = [d[texi + 80 * i:texi + 80 * i + 64].split(b'\0')[0].decode() + '(%dx%d)' % struct.unpack_from('<2i', d, texi + 80 * i + 68) for i in range(ntex)]
skins = struct.unpack_from('<%dh' % (nskinref * nfam), d, skini)
print('textures:', texs, 'skin0:', skins[:nskinref])
for b in range(nbp):
    o = bpi + 76 * b
    nm, base, mi = struct.unpack_from('<3i', d, o + 64)
    print('bodypart', d[o:o + 64].split(b'\0')[0].decode(), 'models', nm)
    for m in range(nm):
        mo = mi + 112 * m
        f = struct.unpack_from('<if10i', d, mo + 64)
        nmesh, meshi, nverts, vinfo, vidx = f[2], f[3], f[4], f[5], f[6]
        vbones = d[vinfo:vinfo + nverts]
        print('  model', d[mo:mo + 64].split(b'\0')[0].decode(), 'verts', nverts, 'meshes', nmesh)
        for k in range(nmesh):
            ntris, trii, skinref, nn, ni = struct.unpack_from('<5i', d, meshi + 20 * k)
            # collect vertex indices used by this mesh
            o2 = trii; used = set()
            while True:
                c, = struct.unpack_from('<h', d, o2); o2 += 2
                if c == 0: break
                c = abs(c)
                for i in range(c):
                    used.add(struct.unpack_from('<h', d, o2 + 8 * i)[0])
                o2 += 8 * c
            bc = collections.Counter(bones[vbones[v]] for v in used)
            print('    mesh tris', ntris, 'tex', texs[skins[skinref]], 'bones', dict(bc.most_common(4)))
