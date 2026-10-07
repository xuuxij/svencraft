# sprites/svencraft/font.spr: a pixel font for Svencraft's screens, ASCII 32..127 (frame = char - 32), each glyph
# 12x22 (Pillow's built-in 6x11 bitmap font doubled) in a 16x24 frame; text advances 12 pixels a character. Glyph pixels are palette white, so the HUD tints them any
# colour; index 255 is transparent.
import os, shutil, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_SDK / _BUILD)
ROOT = sc_paths.ROOT
WORK = sc_paths.work('font')
SPRGEN = os.path.join(sc_paths.SDK, 'sprites', 'sprgen.exe')
os.makedirs(WORK, exist_ok=True)
f = ImageFont.load_default_imagefont()
GW, GH, S = 6, 11, 2
frames = []
for c in range(32, 128):
    im = Image.new('L', (GW, GH), 0)
    ImageDraw.Draw(im).text((0, 0), chr(c) if c < 127 else ' ', font=f, fill=255)
    a = np.array(im) > 127
    frames.append(np.repeat(np.repeat(a, S, 0), S, 1))
W, H = 16, 24   # frames must be multiples of 8: the 12x22 glyph sits top-left, the rest is transparent
cols = 16
sheet = np.full((6 * H, cols * W), 255, np.uint8)
for i, g in enumerate(frames):
    r, c = divmod(i, cols)
    sheet[r * H:r * H + GH * S, c * W:c * W + GW * S][g] = 0
pal = [255, 255, 255] + [0, 0, 0] * 254 + [0, 0, 255]
im = Image.fromarray(sheet, 'P'); im.putpalette(pal)
im.save(os.path.join(WORK, 'font.bmp'))
qc = ['$spritename font', '$type vp_parallel', '$texture alphatest', '$load font.bmp']
for i in range(len(frames)):
    r, c = divmod(i, cols)
    qc.append('$frame %d %d %d %d' % (c * W, r * H, W, H))
open(os.path.join(WORK, 'font.qc'), 'w', newline='\n').write('\n'.join(qc) + '\n')
if os.path.exists(os.path.join(WORK, 'font.spr')):
    os.remove(os.path.join(WORK, 'font.spr'))
r = subprocess.run([SPRGEN, 'font.qc'], cwd=WORK, capture_output=True, text=True)
assert os.path.exists(os.path.join(WORK, 'font.spr')), r.stdout + r.stderr
shutil.copy2(os.path.join(WORK, 'font.spr'), os.path.join(ROOT, 'run', 'svencraft', 'sprites', 'svencraft', 'font.spr'))
print('font.spr', len(frames), 'glyphs of', W, 'x', H)
