"""cracks.spr: 10-stage block-breaking overlay (original pixel art), oriented sprite, alphatest, 64x64 frames.
Each stage adds more cracks to the previous one. Drawn on 16x16 then scaled x4 (pixel-art look)."""
import os, subprocess
import numpy as np
from PIL import Image
import make_blocks as mb

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
SPRGEN = r"C:\Program Files (x86)\Steam\steamapps\common\Sven Co-op SDK\sprites\sprgen.exe"
rng = np.random.default_rng(11)
N, S = 16, 64

# Jagged rays from the centre: stage k shows more rays, each grown to a fraction of its full length,
# plus short side branches near the end. 1-pixel lines on a 16x16 grid.
import math
rays = []
for i in range(9):
    ang = i * 2 * math.pi / 9 + rng.uniform(-0.25, 0.25)
    pts, x, y = [], 7.5, 7.5
    for step in range(9):
        a = ang + rng.uniform(-0.6, 0.6)
        x += math.cos(a); y += math.sin(a)
        pts.append((int(round(y)), int(round(x))))
    br_at = rng.integers(3, 6)
    ba = ang + rng.choice([-1, 1]) * 1.1
    bx, by = pts[br_at][1], pts[br_at][0]
    branch = [(int(round(by + math.sin(ba) * t)), int(round(bx + math.cos(ba) * t))) for t in (1, 2, 3)]
    rays.append((pts, br_at, branch))
order = [0, 4, 7, 2, 5, 8, 1, 3, 6]
frames = []
for stage in range(10):
    crack = np.zeros((N, N), bool)
    nrays = min(9, 1 + stage)
    frac = (stage + 1) / 10.0
    for r in order[:nrays]:
        pts, br_at, branch = rays[r]
        L = max(2, int(round(len(pts) * min(1.0, frac * 1.6))))
        for (yy, xx) in pts[:L]:
            if 0 <= yy < N and 0 <= xx < N: crack[yy, xx] = True
        if L > br_at + 1 and stage >= 4:
            for (yy, xx) in branch[:1 + (stage - 4) // 2]:
                if 0 <= yy < N and 0 <= xx < N: crack[yy, xx] = True
    crack[7:9, 7:9] = True
    big = np.kron(crack, np.ones((4, 4), bool))
    rgb = np.zeros((S, S, 3), np.float32)
    rgb[big] = (25, 25, 25)
    rim = np.zeros_like(big)
    rim[1:, :] |= big[:-1, :]; rim[:, 1:] |= big[:, :-1]
    rim &= ~big
    rgb[rim] = (140, 140, 140)
    frames.append((rgb, big | rim))

atlas_rgb = np.concatenate([f[0] for f in frames], 0).astype(np.uint8)
atlas_op = np.concatenate([f[1] for f in frames], 0)
idx, pal, _ = mb.quantize_alphatest(atlas_rgb, atlas_op)
qc = ['$spritename cracks', '$type oriented', '$texture alphatest']
for si in range(0, 10, 4):
    sheet = np.full((4 * S, 4 * S), 255, np.uint8)
    chunk = list(range(si, min(si + 4, 10)))
    for k, fi in enumerate(chunk):
        sheet[0:S, k * S:(k + 1) * S] = idx[fi * S:(fi + 1) * S]
    bmp = 'cracks_%d.bmp' % (si // 4)
    mb.save_bmp(sheet, pal, bmp)
    qc.append('$load ' + bmp)
    for k in range(len(chunk)):
        qc.append('$frame %d 0 %d %d' % (k * S, S, S))
open('cracks.qc', 'w', newline='\n').write('\n'.join(qc) + '\n')
subprocess.run([SPRGEN, 'cracks.qc'], capture_output=True)
assert os.path.exists('cracks.spr')
prev = np.zeros((S + 8, 10 * (S + 8), 3), np.uint8); prev[:] = (160, 120, 80)
for i, (rgb, op) in enumerate(frames):
    reg = prev[4:4 + S, 4 + i * (S + 8):4 + i * (S + 8) + S]; reg[op] = rgb[op].astype(np.uint8)
Image.fromarray(prev).resize((prev.shape[1] * 2, prev.shape[0] * 2), Image.NEAREST).save('_cracks_preview.png')
print('cracks.spr', os.path.getsize('cracks.spr'), 'bytes')
