# Survey entity classnames and prop models across all installed Sven Co-op maps.
import os, re, struct, collections, sys
ROOT = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop"
maps = []
for sub in ('svencoop', 'svencoop_addon', 'svencoop_downloads'):
    d = os.path.join(ROOT, sub, 'maps')
    if os.path.isdir(d):
        maps += [os.path.join(d, f) for f in os.listdir(d) if f.lower().endswith('.bsp')]

def entities(path):
    with open(path, 'rb') as f:
        hdr = f.read(4 + 15 * 8)
        ver = struct.unpack_from('<i', hdr, 0)[0]
        off, ln = struct.unpack_from('<ii', hdr, 4)
        if ver != 30:  # some maps swap lump 0/1 (blue-shift style)
            pass
        f.seek(off); data = f.read(ln)
    txt = data.decode('latin-1', 'replace')
    for blk in re.findall(r'\{([^{}]*)\}', txt):
        yield dict(re.findall(r'"([^"]*)"\s+"([^"]*)"', blk))

cls = collections.Counter(); models = collections.Counter(); brushcls = collections.Counter()
per = collections.defaultdict(collections.Counter)
for m in maps:
    try:
        for e in entities(m):
            c = e.get('classname', '?'); cls[c] += 1
            mdl = e.get('model', '')
            if mdl.startswith('*'):
                brushcls[c] += 1
            elif mdl:
                per[c][mdl.lower()] += 1
    except Exception as ex:
        print('fail', m, ex, file=sys.stderr)
print(len(maps), 'maps')
print('\nTop brush-entity classes:', brushcls.most_common(25))
print('\nPoint classes with models:')
for c, cnt in sorted(per.items(), key=lambda kv: -sum(kv[1].values()))[:15]:
    print(' ', c, sum(cnt.values()), 'uses;', cnt.most_common(12))
