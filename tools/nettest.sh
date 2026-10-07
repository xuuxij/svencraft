#!/bin/sh
# Co-op over the network on one PC: nettest.sh <name> "<host cmds>" "<client cmds>" [timeout]
# The host runs a co-op listen server (maxplayers 2, coop 1); a second copy of the engine connects to it at
# 127.0.0.1, like a friend's PC (its own engine: none of the host's memory). Each runs its commands (the wait
# aliases of runtest.sh; "screenshot scrshots/h_01.png" / "scrshots/c_01.png" name the shots) and quits.
# Host shots (h_*) and client shots (c_*) are tiled side by side into tools/shots/<name>.png; the logs are
# run/nettest_host.log and run/nettest_client.log. The player's config.cfg is put back afterwards.
NAME=$1; HOSTCMDS=$2; CLIENTCMDS=$3; TMO=${4:-300}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd $ROOT/run || exit 1
if tasklist //FI "IMAGENAME eq xash3d.exe" | grep -qi xash3d; then taskkill //F //IM xash3d.exe > /dev/null; sleep 1; fi
WAITS='alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"
alias w100 "w10;w10;w10;w10;w10;w10;w10;w10;w10;w10"
alias w1k "w100;w100;w100;w100;w100;w100;w100;w100;w100;w100"
alias w5 "wait;wait;wait;wait;wait"
alias w20 "w10;w10"
alias w50 "w10;w10;w10;w10;w10"'
mkdir -p svencraft/tests svencraft/scrshots
printf '%s\n%s\n' "$WAITS" "w100;sv_cheats 1;fps_max 100;$HOSTCMDS;w100;quit" > svencraft/tests/${NAME}_host.cfg
printf '%s\n%s\n' "$WAITS" "fps_max 100;$CLIENTCMDS;w100;quit" > svencraft/tests/${NAME}_client.cfg
rm -f svencraft/scrshots/h_*.png svencraft/scrshots/c_*.png nettest_host.log nettest_client.log
cp svencraft/config.cfg svencraft/config.cfg.pretest 2>/dev/null
timeout $TMO ./xash3d.exe -game svencraft -windowed -width 960 -height 540 -dev 2 -log nettest_host.log \
  +maxplayers 2 +coop 1 +sv_cheats 1 +sc_worldsave ${WORLDSAVE:-0} +sc_seed ${SEED:-1717} +map ${MAP:-svencraft_sandbox} +exec tests/${NAME}_host.cfg > /dev/null 2>&1 &
HOSTPID=$!
sleep ${JOIN:-12}
timeout $TMO ./xash3d.exe -game svencraft -windowed -width 960 -height 540 -dev 2 -log nettest_client.log \
  +clientport 27006 +connect 127.0.0.1 +exec tests/${NAME}_client.cfg > /dev/null 2>&1
echo "client exit $?"
wait $HOSTPID
echo "host exit $?"
[ -f svencraft/config.cfg.pretest ] && mv -f svencraft/config.cfg.pretest svencraft/config.cfg
python - "$ROOT" "$NAME" <<'PY'
import sys, glob
from PIL import Image
root, name = sys.argv[1], sys.argv[2]
hs = sorted(glob.glob(root + '/run/svencraft/scrshots/h_*.png'))
cs = sorted(glob.glob(root + '/run/svencraft/scrshots/c_*.png'))
rows = max(len(hs), len(cs))
if not rows: print('no shots'); sys.exit()
w, h = 640, 360
out = Image.new('RGB', (w * 2, h * rows))
for col, files in ((0, hs), (1, cs)):
    for r, f in enumerate(files):
        out.paste(Image.open(f).convert('RGB').resize((w, h), Image.LANCZOS), (col * w, r * h))
out.save(root + '/tools/shots/' + name + '.png')
print(len(hs), 'host and', len(cs), 'client shots ->', name + '.png')
PY
for f in nettest_host.log nettest_client.log; do
  echo "== $f"; grep -a -i "error\|world snapshot\|svc_scworld\|Dyn_LoadForMap\|Vox_Init\|connected\|disconnect" $f 2>/dev/null | grep -v "Couldn't open\|font" | cut -c1-200 | tail -12
done
