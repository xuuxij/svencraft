#!/bin/sh
# Generate and compile svencraft_sandbox, then install the BSP into the game.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="/c/Program Files (x86)/Steam/steamapps/common/Sven Co-op SDK/mapping/compilers"
cd "$HERE" && python makesandbox.py
cd "$HERE/map"
"$SDK/SC-CSG_x64.exe" svencraft_sandbox -nowadtextures > /dev/null
"$SDK/SC-BSP_x64.exe" svencraft_sandbox > /dev/null
"$SDK/SC-VIS_x64.exe" svencraft_sandbox > /dev/null
"$SDK/SC-RAD_x64.exe" svencraft_sandbox -bounce 0 -extra > /dev/null
grep -a -i -E "error|warning" svencraft_sandbox.log | grep -v -i "no warnings" | head -5 || true
cp svencraft_sandbox.bsp "/c/Program Files (x86)/Steam/steamapps/common/Svencraft Coop/svencoop_addon/maps/svencraft_sandbox.bsp"
ls -la svencraft_sandbox.bsp
