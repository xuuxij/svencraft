"""Svencraft hotbar sprites (alphatest, 56x56 frames so every element shares the slot's position):
  hotbar.spr  frame m      = slot with block icon m (0..17)
              frame 18     = empty slot
              frame 19 + m = selected slot with block icon m
              frame 37     = selected empty slot
  counts.spr  frame n      = stack count n (1..99) drawn in the slot's bottom-right corner, frame 100 = "99+"
"""
import os, subprocess
import numpy as np
from PIL import Image
import make_blocks as mb

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
SPRGEN = r"C:\Program Files (x86)\Steam\steamapps\common\Sven Co-op SDK\sprites\sprgen.exe"
S = 56          # slot size
PER_SHEET = 16  # 4x4 frames of 56 per source bmp (224x224)

def slot_base(selected):
    rgb = np.zeros((S, S, 3), np.float32)
    op = np.ones((S, S), bool)
    if selected:
        rgb[:] = (20, 20, 20)                 # 1px dark outer edge
        rgb[1:-1, 1:-1] = (235, 235, 235)     # 3px white frame
        rgb[4:-4, 4:-4] = (88, 88, 88)        # lighter fill
    else:
        rgb[:] = (24, 24, 24)
        rgb[1:-1, 1:-1] = (120, 120, 120)     # 2px grey border, bevelled
        rgb[1:3, 1:-1] = (150, 150, 150)
        rgb[1:-1, 1:3] = (150, 150, 150)
        rgb[3:-3, 3:-3] = (58, 58, 58)
        rgb[3:5, 3:-3] = (46, 46, 46)          # inner shadow
        rgb[3:-3, 3:5] = (46, 46, 46)
    return rgb, op

def with_icon(base, icon_rgb, icon_op):
    rgb, op = base[0].copy(), base[1].copy()
    o = (S - mb.FS) // 2
    sub = rgb[o:o + mb.FS, o:o + mb.FS]
    sub[icon_op] = icon_rgb[icon_op]
    return rgb, op

# 3x5 pixel font, scaled x2, white with a dark drop shadow (Minecraft-like stack counts)
FONT = {
    '0': ['111', '101', '101', '101', '111'], '1': ['010', '110', '010', '010', '111'],
    '2': ['111', '001', '111', '100', '111'], '3': ['111', '001', '111', '001', '111'],
    '4': ['101', '101', '111', '001', '001'], '5': ['111', '100', '111', '001', '111'],
    '6': ['111', '100', '111', '101', '111'], '7': ['111', '001', '010', '010', '010'],
    '8': ['111', '101', '111', '101', '111'], '9': ['111', '101', '111', '001', '111'],
    '+': ['000', '010', '111', '010', '000'],
}

def count_frame(text):
    rgb = np.zeros((S, S, 3), np.float32)
    op = np.zeros((S, S), bool)
    sc, gap = 2, 2
    w = len(text) * 3 * sc + (len(text) - 1) * gap
    x0, y0 = S - 5 - w - 2, S - 5 - 5 * sc - 2
    for dx, dy, col in ((2, 2, (40, 40, 40)), (0, 0, (255, 255, 255))):   # shadow first, then glyph
        x = x0
        for ch in text:
            for r, row in enumerate(FONT[ch]):
                for c, bit in enumerate(row):
                    if bit == '1':
                        ys, xs = y0 + r * sc + dy, x + c * sc + dx
                        rgb[ys:ys + sc, xs:xs + sc] = col
                        op[ys:ys + sc, xs:xs + sc] = True
            x += 3 * sc + gap
    return rgb, op

def write_sprite(name, frames):
    """frames: list of (rgb, opaque). Splits into 224x224 source bmps (sprgen frame limits) sharing one palette."""
    sheets = [frames[i:i + PER_SHEET] for i in range(0, len(frames), PER_SHEET)]
    big_rgb = np.concatenate([np.concatenate([f[0] for f in sh] + [np.zeros((S * (PER_SHEET - len(sh)), S, 3), np.float32)] , 0) for sh in sheets], 1) if False else None
    # quantize all frames together so every sheet shares one palette
    atlas_rgb = np.concatenate([f[0] for f in frames], 0).astype(np.uint8)
    atlas_op = np.concatenate([f[1] for f in frames], 0)
    idx, pal, _ = mb.quantize_alphatest(atlas_rgb, atlas_op)
    qc = ['$spritename %s' % name, '$type vp_parallel', '$texture alphatest']
    for si, sh in enumerate(sheets):
        sheet = np.full((4 * S, 4 * S), 255, np.uint8)
        for k in range(len(sh)):
            fi = si * PER_SHEET + k
            r, c = divmod(k, 4)
            sheet[r * S:(r + 1) * S, c * S:(c + 1) * S] = idx[fi * S:(fi + 1) * S]
        bmp = '%s_%d.bmp' % (name, si)
        mb.save_bmp(sheet, pal, bmp)
        qc.append('$load %s' % bmp)
        for k in range(len(sh)):
            r, c = divmod(k, 4)
            qc.append('$frame %d %d %d %d' % (c * S, r * S, S, S))
    open(name + '.qc', 'w', newline='\n').write('\n'.join(qc) + '\n')
    out = subprocess.run([SPRGEN, name + '.qc'], capture_output=True, text=True)
    assert os.path.exists(name + '.spr'), out.stdout + out.stderr
    print(name, len(frames), 'frames', os.path.getsize(name + '.spr'), 'bytes')

icons = [mb.material_icon(n, t, s) for (n, t, s) in mb.MATERIALS]
icons = [(rgb.astype(np.float32), op) for rgb, op in icons]
frames = []
for sel in (False, True):
    base = slot_base(sel)
    frames += [with_icon(base, rgb, op) for rgb, op in icons]
    frames.append(base)
write_sprite('hotbar', frames)

counts = [count_frame(str(n)) for n in range(0, 100)] + [count_frame('99+')]
write_sprite('counts', counts)

# preview: a 9-slot bar with slot 3 selected, count 37 on it, over a grass-coloured background
prev = np.zeros((S + 16, 9 * S + 16, 3), np.uint8); prev[:] = (95, 140, 60)
def blit(rgbop, x, y):
    rgb, op = rgbop
    reg = prev[y:y + S, x:x + S]; reg[op] = rgb[op].astype(np.uint8)
for i, f in enumerate([frames[0], frames[1], frames[3], frames[19 + 2], frames[5], frames[18], frames[18], frames[18], frames[18]]):
    blit(f, 8 + i * S, 8)
blit(counts[37], 8 + 3 * S, 8)
Image.fromarray(prev).resize((prev.shape[1] * 2, prev.shape[0] * 2), Image.NEAREST).save('_hotbar_preview.png')
