# A studio model's sequences: tools/mdlinfo.py <model.mdl> [...]
# Prints each sequence's index, name, activity (number and ACT_ name), weight, frames, fps, looping and its events
# (frame, event number, options): what a monster's code has to handle and which sequence an activity picks.
import struct, sys, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def activity_names():
    # the SDK's enum: values in order, some given explicitly
    names, n = {}, 0
    try:
        for line in open(os.path.join(ROOT, 'game', 'dlls', 'activity.h')):
            m = re.match(r'\s*(ACT_\w+)\s*(?:=\s*(\d+))?\s*,', line)
            if m:
                if m.group(2):
                    n = int(m.group(2))
                names[n] = m.group(1)
                n += 1
    except OSError:
        pass
    return names

def info(path):
    data = open(path, 'rb').read()
    if data[:4] != b'IDST':
        print(path, ': not a studio model (%r)' % data[:4])
        return
    acts = activity_names()
    name = data[8:72].split(b'\0')[0].decode('latin-1')
    # studiohdr_t: id, version, name[64], length, eyeposition[3], min[3], max[3], bbmin[3], bbmax[3], flags,
    # numbones, boneindex, numbonecontrollers, bonecontrollerindex, numhitboxes, hitboxindex, numseq, seqindex
    off = 8 + 64 + 4 + 12 * 5 + 4
    numbones, boneindex, numbc, bcindex, numhb, hbindex, numseq, seqindex = struct.unpack_from('<8i', data, off)
    print('%s (%s): %d sequences' % (os.path.basename(path), name, numseq))
    SEQ = 176	# mstudioseqdesc_t
    for i in range(numseq):
        o = seqindex + i * SEQ
        label = data[o:o + 32].split(b'\0')[0].decode('latin-1')
        fps, flags, activity, actweight, numevents, eventindex, numframes = struct.unpack_from('<f6i', data, o + 32)
        evs = []
        for e in range(numevents):
            eo = eventindex + e * 76	# mstudioevent_t: frame, event, type, options[64]
            frame, event, typ = struct.unpack_from('<3i', data, eo)
            opts = data[eo + 12:eo + 76].split(b'\0')[0].decode('latin-1')
            evs.append('%d@%d%s' % (event, frame, (' "%s"' % opts) if opts else ''))
        print('  %3d %-24s act %3d %-24s w%-3d %3d fr %5.1f fps%s%s' % (i, label, activity, acts.get(activity, ''), actweight,
              numframes, fps, ' loop' if flags & 1 else '', ('  events: ' + ', '.join(evs)) if evs else ''))

for p in sys.argv[1:]:
    info(p)
