import struct, sys
d = open(sys.argv[1], 'rb').read()
ident, ver = struct.unpack_from('<4si', d, 0)
name = d[8:72].split(b'\0')[0]
length, = struct.unpack_from('<i', d, 72)
v = struct.unpack_from('<15f', d, 76)
ints = struct.unpack_from('<i26i', d, 136)
keys = "flags numbones boneindex numbonecontrollers bonecontrollerindex numhitboxes hitboxindex numseq seqindex numseqgroups seqgroupindex numtextures textureindex texturedataindex numskinref numskinfamilies skinindex numbodyparts bodypartindex numattachments attachmentindex soundtable soundindex soundgroups soundgroupindex numtransitions transitionindex".split()
h = dict(zip(keys, ints))
print(ident, ver, name, 'len', length, 'filesize', len(d))
print('eye/min/max/bbmin/bbmax', [round(x,1) for x in v])
print(h)
for i in range(h['numbones']):
    o = h['boneindex'] + i*112
    print('bone', d[o:o+32].split(b'\0')[0], struct.unpack_from('<2i6i6f6f', d, o+32))
for i in range(h['numseq']):
    o = h['seqindex'] + i*176
    print('seq', d[o:o+32].split(b'\0')[0], 'fps,flags,act,actw,nev,evi,nframes', struct.unpack_from('<f6i', d, o+32), 'numblends,animindex', struct.unpack_from('<2i', d, o+120), 'seqgroup', struct.unpack_from('<i', d, o+156))
for i in range(h['numtextures']):
    o = h['textureindex'] + i*80
    print('tex', d[o:o+64].split(b'\0')[0], struct.unpack_from('<4i', d, o+64))
print('skins', struct.unpack_from('<%dh' % (h['numskinref']*h['numskinfamilies']), d, h['skinindex']))
for i in range(h['numbodyparts']):
    o = h['bodypartindex'] + i*76
    nm, base, mi = struct.unpack_from('<3i', d, o+64)
    print('bodypart', d[o:o+64].split(b'\0')[0], nm, base, mi)
    for m in range(nm):
        mo = mi + m*112
        f = struct.unpack_from('<if10i', d, mo+64)
        print('  model', d[mo:mo+64].split(b'\0')[0], f)
        for k in range(f[2]):
            print('    mesh', struct.unpack_from('<5i', d, f[3] + k*20))
