# Point the "Svencraft Coop" non-Steam shortcut at the launcher (Steam must be closed). Binary VDF in/out.
import struct, sys, shutil
VDF = r"C:\Program Files (x86)\Steam\userdata\<steam-user-id>\config\shortcuts.vdf"
GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop"
EXE = '"' + GAME + r'\launcher\Svencraft.exe"'
START = '"' + GAME + '\\launcher\\"'

def parse(b, i):
    obj = []
    while True:
        t = b[i]; i += 1
        if t == 8:
            return obj, i
        j = b.index(0, i); key = b[i:j].decode('utf-8'); i = j + 1
        if t == 0:
            val, i = parse(b, i)
        elif t == 1:
            j = b.index(0, i); val = b[i:j].decode('utf-8'); i = j + 1
        elif t == 2:
            val = struct.unpack_from('<I', b, i)[0]; i += 4
        else:
            raise ValueError('type %d' % t)
        obj.append((t, key, val))

def dump(obj):
    out = b''
    for t, k, v in obj:
        out += bytes([t]) + k.encode('utf-8') + b'\0'
        if t == 0: out += dump(v) + b'\x08'
        elif t == 1: out += v.encode('utf-8') + b'\0'
        else: out += struct.pack('<I', v)
    return out

b = open(VDF, 'rb').read()
root, _ = parse(b, 0)
found = 0
for t, k, shortcuts in root:
    for t2, idx, entry in shortcuts:
        fields = {kk: n for n, (tt, kk, vv) in enumerate(entry)}
        name = entry[fields['AppName']][2] if 'AppName' in fields else entry[fields.get('appname', 0)][2]
        if name == 'Svencraft Coop':
            entry[fields['Exe']] = (1, 'Exe', EXE)
            entry[fields['StartDir']] = (1, 'StartDir', START)
            found += 1
            print('updated', {kk: vv for tt, kk, vv in entry if kk in ('appid', 'AppName', 'Exe', 'StartDir', 'icon')})
assert found == 1, found
shutil.copy(VDF, VDF + '.bak')
open(VDF, 'wb').write(dump(root) + b'\x08')
