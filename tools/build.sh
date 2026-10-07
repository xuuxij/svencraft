#!/bin/sh
# build.sh [game] [engine] [map]: build the parts asked for (default: game) and install them into run/
ROOT=$(cd "$(dirname "$0")/.." && pwd)   # the project root (this script lives in tools/)
[ $# -eq 0 ] && set -- game
if tasklist //FI "IMAGENAME eq xash3d.exe" | grep -qi xash3d; then taskkill //F //IM xash3d.exe > /dev/null; sleep 1; fi
for part in "$@"; do
  case $part in
    game)
      cd $ROOT/game && python waf build > build/last.log 2>&1
      grep -E "error|warning C4(700|701|703|715)|finished" build/last.log | head -20
      if grep -q "Build failed" build/last.log; then echo "GAME BUILD FAILED (game/build/last.log), nothing installed"; else
      cp build/dlls/hl_amd64.dll $ROOT/run/svencraft/dlls/ && cp build/cl_dll/client_amd64.dll $ROOT/run/svencraft/cl_dlls/ && echo "game installed"; fi ;;
    engine)
      cd $ROOT/engine && python waf build > build/last.log 2>&1
      grep -E "error|finished" build/last.log | head -20
      if grep -q "Build failed" build/last.log; then echo "ENGINE BUILD FAILED (engine/build/last.log), nothing installed"; else
      cp build/engine/xash.dll build/ref/gl/ref_gl.dll $ROOT/run/ && echo "engine installed"; fi ;;
    map)
      cd $ROOT/maps_src && python make_town.py 2>&1 | grep -v "^ *$" | tail -4 ;;
  esac
done
