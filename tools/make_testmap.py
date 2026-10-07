# svencraft_world: an empty sky box around the block world (the block world itself comes from the game DLL).
import os, subprocess, shutil
import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_SDK / _SVEN)
ROOT = sc_paths.ROOT
OUT = os.path.join(ROOT, 'maps_src')
SDK = os.path.join(sc_paths.SDK, 'mapping', 'compilers')
SVEN = sc_paths.SVEN
OLDWAD = os.path.join(sc_paths.ASSETS, 'testmap', 'svencraft_old.wad')   # the prototype's block WAD (original art)
os.makedirs(OUT, exist_ok=True)
WAD = os.path.join(OUT, 'svencraft.wad')
if not os.path.exists(WAD):
    shutil.copy2(OLDWAD, WAD)

H = 2048                 # half width (block world is +-1920)
FLOOR, TOP = -1040, 1600  # block world spans z -960 .. 640


def box(mn, mx, tex):
    (x0, y0, z0), (x1, y1, z1) = mn, mx
    faces = [((x0, y0, z1), (x0, y1, z1), (x1, y1, z1), '0 0 -1'),  # top
             ((x0, y1, z0), (x0, y0, z0), (x1, y0, z0), '0 0 -1'),  # bottom
             ((x0, y1, z1), (x0, y0, z1), (x0, y0, z0), '0 -1 0'),  # west
             ((x1, y0, z1), (x1, y1, z1), (x1, y1, z0), '0 -1 0'),  # east
             ((x1, y1, z1), (x0, y1, z1), (x0, y1, z0), '0 0 -1'),  # north
             ((x0, y0, z1), (x1, y0, z1), (x1, y0, z0), '0 0 -1')]  # south
    out = '{\n'
    for (a, b, c, _) in faces:
        u, v = ('1 0 0', '0 -1 0') if a[2] == b[2] == c[2] else (('0 1 0', '0 0 -1') if a[0] == b[0] == c[0] else ('1 0 0', '0 0 -1'))
        out += '( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s [ %s 0 ] [ %s 0 ] 0 1 1\n' % (*a, *b, *c, tex, u, v)
    return out + '}\n'


wads = ';'.join([WAD, os.path.join(SVEN, 'halflife.wad')])
m = '{\n"classname" "worldspawn"\n"mapversion" "220"\n"wad" "%s"\n"skyname" "grassy"\n"message" "Svencraft"\n"maxrange" "8192"\n' % wads
m += box((-H, -H, FLOOR - 32), (H, H, FLOOR), 'sc_bedrock')
m += box((-H - 32, -H - 32, TOP), (H + 32, H + 32, TOP + 32), 'sky')
m += box((-H - 32, -H - 32, FLOOR - 32), (-H, H + 32, TOP), 'sky')
m += box((H, -H - 32, FLOOR - 32), (H + 32, H + 32, TOP), 'sky')
m += box((-H, -H - 32, FLOOR - 32), (H, -H, TOP), 'sky')
m += box((-H, H, FLOOR - 32), (H, H + 32, TOP), 'sky')
m += '}\n'
m += '{\n"classname" "light_environment"\n"origin" "0 0 1400"\n"pitch" "-60"\n"angles" "0 35 0"\n"_light" "255 248 230 280"\n"_diffuse_light" "165 185 225 110"\n}\n'
for x, y in [(20, 20), (100, 20), (20, 100), (-60, 20), (20, -60)]:
    m += '{\n"classname" "info_player_deathmatch"\n"origin" "%d %d 40"\n"angles" "0 90 0"\n}\n' % (x, y)
m += '{\n"classname" "info_player_start"\n"origin" "20 20 40"\n"angles" "0 90 0"\n}\n'
open(os.path.join(OUT, 'svencraft_world.map'), 'w', newline='\n').write(m)

for exe, args in (('SC-CSG_x64.exe', ['-nowadtextures']), ('SC-BSP_x64.exe', []), ('SC-VIS_x64.exe', ['-fast']), ('SC-RAD_x64.exe', ['-bounce', '0', '-fast'])):
    r = subprocess.run([os.path.join(SDK, exe), 'svencraft_world'] + args, cwd=OUT, capture_output=True, text=True)
    if r.returncode != 0:
        print(exe, 'failed'); print(r.stdout[-1500:]); raise SystemExit(1)
dst = os.path.join(ROOT, 'run', 'svencraft', 'maps', 'svencraft_world.bsp')
shutil.copy2(os.path.join(OUT, 'svencraft_world.bsp'), dst)
print('built', dst, os.path.getsize(dst))
