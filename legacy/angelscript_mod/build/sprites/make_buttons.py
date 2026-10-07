# Adds the category arrow buttons to the crafting panel (run after make_craftui.py, which redraws the panel).
import numpy as np, subprocess, os, shutil
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); os.chdir(HERE)
SPRGEN = r"C:\Program Files (x86)\Steam\steamapps\common\Sven Co-op SDK\sprites\sprgen.exe"
DST = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\sprites\svencraft"
with open('craftpanel_l.bmp', 'rb') as fh:
    im = Image.open(fh); im.load(); a = np.array(im); palette = im.getpalette()
pal = np.array(palette[:768]).reshape(-1, 3)
def idx(c):
    return int(np.argmin(((pal - np.array(c)) ** 2).sum(1)))
MID, DARK, WHITE, BLACK = idx((139, 139, 139)), idx((55, 55, 55)), idx((255, 255, 255)), idx((0, 0, 0))
def button(x0, y0, x1, y1, left):
    a[y0:y1, x0:x1] = BLACK
    a[y0 + 1:y1 - 1, x0 + 1:x1 - 1] = MID
    a[y0 + 1, x0 + 1:x1 - 1] = WHITE; a[y0 + 1:y1 - 1, x0 + 1] = WHITE
    a[y1 - 2, x0 + 1:x1 - 1] = DARK; a[y0 + 1:y1 - 1, x1 - 2] = DARK
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    for i in range(7):
        x = cx - 3 + i if left else cx + 3 - i
        a[cy - i:cy + i + 1, x] = BLACK
button(10, 202, 38, 226, True)      # previous category, screen centre (-176, +98)
button(146, 202, 174, 226, False)   # next category,     screen centre (-40, +98)
out = Image.fromarray(a, 'P'); out.putpalette(palette); out.save('craftpanel_btn.bmp')
open('craftpanel_btn.qc', 'w', newline='\n').write('$spritename craftpanel_l\n$type vp_parallel\n$texture alphatest\n$load craftpanel_btn.bmp\n$frame 0 0 256 232\n')
subprocess.run([SPRGEN, 'craftpanel_btn.qc'], capture_output=True)
shutil.copy('craftpanel_l.spr', os.path.join(DST, 'craftpanel_l.spr'))
print('buttons added')
