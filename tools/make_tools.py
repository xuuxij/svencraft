# The tool models: models/svencraft/{v,p,w}_sc<tool><tier>.mdl, Sven's crowbar models with a blocky tool in place
# of the crowbar (tools/toolmdl.py), for every tool and tier (1 wood, 2 stone, 3 iron, 4 diamond)
import os, subprocess, sys

import sc_paths   # project paths (tools/sc_paths.py; env overrides SVENCRAFT_GAMEDIR / _SVEN)
ROOT = sc_paths.ROOT
SRC = os.path.join(sc_paths.SVEN, 'models')
DST = os.path.join(sc_paths.GAMEDIR, 'models', 'svencraft')
TIERS = [int(t) for t in sys.argv[1:]] or [1, 2, 3, 4]
for tool in ('pickaxe', 'shovel', 'axe'):
    for tier in TIERS:
        for kind in 'vpw':
            subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'toolmdl.py'), os.path.join(SRC, '%s_crowbar.mdl' % kind),
                            os.path.join(DST, '%s_sc%s%d.mdl' % (kind, tool, tier)), tool, str(tier)], check=True)
