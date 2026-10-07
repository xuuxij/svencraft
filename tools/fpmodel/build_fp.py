"""
build_fp.py - a first-person weapon model from a rigged, animated source (glTF/FBX/.blend) to a GoldSrc .mdl:
    python tools/fpmodel/build_fp.py tools/fpmodel/<weapon>.json [--no-compile]

1. Blender (portable, deps/blender-*/blender.exe, or SVENCRAFT_BLENDER) runs convert_fp.py: decimated mesh, one
   bone per vertex, SMD reference + one SMD per sequence, base-colour textures as PNG.
2. Textures become 8-bit BMPs (256 colours each, the studio model format's).
3. A QC: the sequences in the config's order (the weapon code's animation numbers), their fps, loop and events
   (muzzle flash 5001, sound 5004, Sven's scripted flash 5005...), the muzzle attachment.
4. The Sven Co-op SDK's studiomdl compiles it into run/svencraft/models/svencraft/<name>.mdl.

Config keys: input, name, target_tris, tex_size, transform {rotate, scale, offset}, sequences [{name, range [a, b],
action?, fps, loop, events [[frame, event, "options"]]}], attachments [{bone, offset}], masked [texture names],
credit (kept in the manifest and docs/CREDITS.md).
"""
import json, os, shutil, subprocess, sys, glob
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import sc_paths

cfg_path = os.path.abspath(sys.argv[1])
cfg = json.load(open(cfg_path))
name = cfg['name']
WORK = sc_paths.work(os.path.join('fpmodel', name))
if os.path.isdir(WORK):
    shutil.rmtree(WORK)
os.makedirs(WORK)
here = os.path.dirname(os.path.abspath(__file__))
if not os.path.isabs(cfg['input']):
    cfg['input'] = os.path.normpath(os.path.join(os.path.dirname(cfg_path), cfg['input']))
json.dump(cfg, open(os.path.join(WORK, 'config.json'), 'w'), indent=1)

blender = os.environ.get('SVENCRAFT_BLENDER') or next(iter(sorted(glob.glob(os.path.join(sc_paths.ROOT, 'deps', 'blender-*', 'blender.exe')), reverse=True)), None)
if not blender:
    raise SystemExit('no Blender: put a portable Blender in deps/ or set SVENCRAFT_BLENDER')
r = subprocess.run([blender, '-b', '--factory-startup', '-P', os.path.join(here, 'convert_fp.py'), '--',
                    os.path.join(WORK, 'config.json'), WORK], capture_output=True, text=True)
print('\n'.join(l for l in r.stdout.splitlines() if l.startswith('[convert_fp]')))
if not os.path.exists(os.path.join(WORK, 'manifest.json')):
    print(r.stdout[-4000:], r.stderr[-2000:])
    raise SystemExit('Blender stage failed')
man = json.load(open(os.path.join(WORK, 'manifest.json')))

# ---------------------------------------------------------------- textures: 256 colours each
masked = set(cfg.get('masked', []))
for png in glob.glob(os.path.join(WORK, 'tex_*.png')):
    base = os.path.splitext(os.path.basename(png))[0]
    im = Image.open(png).convert('RGBA')
    rgb = im.convert('RGB')
    if base + '.bmp' in masked:
        # index 255 is see-through (pure blue in the palette): alpha below half
        q = rgb.quantize(255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        px = q.load()
        a = im.getchannel('A').load()
        for y in range(im.height):
            for x in range(im.width):
                if a[x, y] < 128:
                    px[x, y] = 255
        pal = q.getpalette()[:255 * 3] + [0, 0, 255]
        q.putpalette(pal)
    else:
        q = rgb.quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    q.save(os.path.join(WORK, base + '.bmp'))

# ---------------------------------------------------------------- the QC
qc = ['$modelname "%s.mdl"' % name, '$cd "."', '$cdtexture "."', '$scale 1.0', '$cliptotextures',
      '$body "studio" "ref"']
for m in sorted(masked):
    qc.append('$texrendermode "%s" "masked"' % m)
for i, a in enumerate(man['attachments']):
    qc.append('$attachment %d "%s" %.3f %.3f %.3f' % (i, a['bone'], *a['offset']))
for s in cfg.get('sequences', []):
    line = '$sequence "%s" "%s" fps %s%s' % (s['name'], s['name'], s.get('fps', 30), ' loop' if s.get('loop') else '')
    ev = s.get('events', [])
    if ev:
        line += ' {\n' + '\n'.join('\t{ event %d %d "%s" }' % (e[1], e[0], e[2]) for e in ev) + '\n}'
    qc.append(line)
open(os.path.join(WORK, name + '.qc'), 'w', newline='\n').write('\n'.join(qc) + '\n')

if '--no-compile' in sys.argv:
    print('sources in', WORK)
    raise SystemExit(0)
studiomdl = os.path.join(sc_paths.SDK, 'modelling', 'studiomdl.exe')
r = subprocess.run([studiomdl, name + '.qc'], cwd=WORK, capture_output=True, text=True)
out = os.path.join(WORK, name + '.mdl')
if not os.path.exists(out):
    print(r.stdout[-3000:], r.stderr[-1000:])
    raise SystemExit('studiomdl failed')
dst = os.path.join(sc_paths.GAMEDIR, 'models', 'svencraft', name + '.mdl')
os.makedirs(os.path.dirname(dst), exist_ok=True)
shutil.copy2(out, dst)
print('%s: %d -> %d triangles, %d bones, %d textures, %d sequences -> %s' % (
    name, man['triangles_in'], man['triangles_out'], len(man['bones']), len(man['textures']), len(man['sequences']), dst))
