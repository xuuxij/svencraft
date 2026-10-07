#!/bin/sh
# In-engine visual test: runtest.sh <name> "<cmd;cmd;...>" [timeout]
# Writes run/svencraft/<name>.cfg (wait aliases + god/notarget + the commands + quit), runs Svencraft windowed,
# then tiles its screenshots into tools/shots/<name>.png (2 per row, half size).
# Env: MAP, SEED, NT ("" = no notarget), ARGS (more launch arguments, e.g. "+maxplayers 2").
NAME=$1; CMDS=$2; TMO=${3:-600}
ROOT=$(cd "$(dirname "$0")/.." && pwd)   # the project root (this script lives in tools/)
cd $ROOT/run || exit 1
if tasklist //FI "IMAGENAME eq xash3d.exe" | grep -qi xash3d; then taskkill //F //IM xash3d.exe > /dev/null; sleep 1; fi
cat > svencraft/tests/$NAME.cfg <<CFG
alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"
alias w100 "w10;w10;w10;w10;w10;w10;w10;w10;w10;w10"
alias w1k "w100;w100;w100;w100;w100;w100;w100;w100;w100;w100"
alias w20 "w10;w10"
alias w5 "wait;wait;wait;wait;wait"
alias w50 "w10;w10;w10;w10;w10"
w100;sv_cheats 1;god;${NT-notarget};w1k;w1k;$(echo "$CMDS" | sed "s/screenshot/screenshot;w10/g");w100;quit
CFG
rm -rf svencraft/scrshots
# the engine saves archived settings (s_show, fps_max ...) on quit: the player's own config.cfg comes back after
cp svencraft/config.cfg svencraft/config.cfg.pretest 2>/dev/null
timeout $TMO ./xash3d.exe -game svencraft -windowed -width 1280 -height 720 -dev 2 -log ${ARGS} +sc_worldsave ${WORLDSAVE:-0} +sc_seed ${SEED:-1717} +map ${MAP:-svencraft_sandbox} +exec tests/$NAME.cfg > /dev/null 2>&1
echo "exit $?"
[ -f svencraft/config.cfg.pretest ] && mv -f svencraft/config.cfg.pretest svencraft/config.cfg
python - "$ROOT" "$NAME" <<'PY'
import sys, glob, os
from PIL import Image
root, name = sys.argv[1], sys.argv[2]
files = sorted(glob.glob(root + '/run/svencraft/scrshots/*.png'))
if not files: print('no shots'); sys.exit()
ims = [Image.open(f).convert('RGB') for f in files]
w, h = ims[0].size[0] // 2, ims[0].size[1] // 2
cols = min(2, len(ims)); rows = (len(ims) + cols - 1) // cols
out = Image.new('RGB', (w * cols, h * rows))
for i, im in enumerate(ims):
    out.paste(im.resize((w, h), Image.LANCZOS), ((i % cols) * w, (i // cols) * h))
out.save(root + '/tools/shots/' + name + '.png')
print(len(ims), 'shots ->', name + '.png')
PY
grep -a -i "error\|warning: \|assert\|crash" $ROOT/run/engine.log 2>/dev/null | grep -v "Couldn't open\|missing\|font" | cut -c1-200 | tail -8
