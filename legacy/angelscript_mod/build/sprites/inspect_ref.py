import os
import numpy as np
from PIL import Image
from sprparse import parse, describe, render
G = 'C:/Program Files (x86)/Steam/steamapps/common/Svencraft Coop/svencoop/sprites'
for n in ['640hud1', '640hud4', '640hud7', 'crosshairs', '640hudsc']:
    s = describe(os.path.join(G, n + '.spr'))
    pal = s['pal']
    if n in ('640hud1', '640hud4'):
        px = s['frames'][0]['px']
        reg = px[0:45, 0:170]
        used = np.unique(reg)
        print('  crowbar region uses', len(used), 'palette indices; whole frame uses', len(np.unique(px)))
        cols = pal[used]
        grey = np.all(cols[:, 0:1] == cols, axis=1)
        print('  greyscale entries in region:', grey.sum(), '/', len(used))
        print('  sample colours (idx: rgb):', [(int(i), tuple(int(c) for c in pal[i])) for i in used[::max(1, len(used)//12)]])
        print('  pal[0]', pal[0], 'pal[255]', pal[255])
        # which pixels are non-black outside region? bounding boxes of icons
        rgbc = pal[px].max(axis=2)
        ys, xs = np.nonzero(rgbc[0:45, 0:170] > 0)
        print('  crowbar region nonblack bbox: x', xs.min(), xs.max(), 'y', ys.min(), ys.max())
        Image.fromarray(pal[px]).save(f'_ref_{n}_full.png')
        im = Image.fromarray(pal[reg]); im.save(f'_ref_{n}_crowbar.png')
        im.resize((170*3, 45*3), Image.NEAREST).save(f'_ref_{n}_crowbar_x3.png')
        # tinted additive version with HL default hud colour (255,160,0)
        render(s, 0, 'additive', (255, 160, 0)).crop((0, 0, 170, 45)).resize((510, 135), Image.NEAREST).save(f'_ref_{n}_crowbar_tinted.png')
        # luminance histogram
        lum = pal[reg].max(axis=2)
        print('  max channel values in region: min', lum.min(), 'max', lum.max(), 'mean nonzero', lum[lum>0].mean())
