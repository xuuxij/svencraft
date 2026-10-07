# Prepare a fresh checkout's runtime folder (run/) for the generators and the game. Safe to re-run: it only
# creates what is missing and never overwrites or deletes anything.
#   - the folders the generators and tools/runtest.sh write into (run/svencraft/maps, models/svencraft, ...,
#     tools/shots)
#   - run/svencoop: a directory junction to Sven Co-op's content folder (sc_paths.SVEN; set SVENCRAFT_SVEN when
#     Sven Co-op is not in Steam's default library)
#   - run/svencraft/extras.pk3 from the engine package's run/valve/extras.pk3
#   - run/svencraft/sprites/640_pain.spr and 320_pain.spr: copies of Sven Co-op's sprites/pain.spr (the HUD's
#     damage direction sprite; Sven Co-op content, so it is copied here and never committed)
#   - run/svencraft/sound/sentences.txt: a copy of Sven Co-op's sound/default_sentences.txt. The engine and the game
#     read the speech sentences (the H.E.V. suit, Barney, the scientists, the grunts' radio, Opposing Force's
#     allies) from sound/sentences.txt, which Sven Co-op doesn't have: without it they are all silent
# Then it lists what is still missing (the engine package, the built DLLs, generated assets): docs/BUILDING.md.
#   python tools/setup_run.py [--check]      (--check: only report)
import os, shutil, subprocess, sys
import sc_paths

CHECK = '--check' in sys.argv
RUN, GAME, SVEN = sc_paths.RUN, sc_paths.GAMEDIR, sc_paths.SVEN
done, todo = [], []


def mkdir(p):
    if not os.path.isdir(p):
        if not CHECK:
            os.makedirs(p)
        done.append('folder ' + os.path.relpath(p, sc_paths.ROOT))


def copy_once(src, dst, what):
    if os.path.exists(dst):
        return
    if not os.path.isfile(src):
        todo.append('%s: %s (source %s missing)' % (what, os.path.relpath(dst, sc_paths.ROOT), src))
        return
    if not CHECK:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    done.append('%s -> %s' % (what, os.path.relpath(dst, sc_paths.ROOT)))


for sub in ('maps', 'models/svencraft', 'sprites/svencraft', 'gfx/blocks', 'scripts', 'sound', 'dlls', 'cl_dlls', 'tests'):
    mkdir(os.path.join(GAME, *sub.split('/')))
mkdir(os.path.join(sc_paths.ROOT, 'tools', 'shots'))

link = os.path.join(RUN, 'svencoop')
if not os.path.exists(link):
    if os.path.isdir(SVEN) and os.path.normcase(os.path.abspath(SVEN)) != os.path.normcase(link):
        if not CHECK:
            subprocess.run(['cmd', '/c', 'mklink', '/J', link, SVEN], check=True, capture_output=True)
        done.append('junction run/svencoop -> ' + SVEN)
    else:
        todo.append('run/svencoop: Sven Co-op content not found at %s (install Sven Co-op, or set SVENCRAFT_SVEN)' % SVEN)

copy_once(os.path.join(RUN, 'valve', 'extras.pk3'), os.path.join(GAME, 'extras.pk3'), 'engine extras')
for size in ('640', '320'):
    copy_once(os.path.join(SVEN, 'sprites', 'pain.spr'), os.path.join(GAME, 'sprites', size + '_pain.spr'), "Sven's pain sprite")
copy_once(os.path.join(SVEN, 'sound', 'default_sentences.txt'), os.path.join(GAME, 'sound', 'sentences.txt'), "Sven's sentences")

for exe in ('xash3d.exe', 'xash.dll', 'ref_gl.dll', 'menu.dll', 'filesystem_stdio.dll', 'SDL2.dll', 'mdldec.exe'):
    if not os.path.isfile(os.path.join(RUN, exe)):
        todo.append('run/%s: setup.bat (builds the engine)' % exe)
for rel in ('dlls/hl_amd64.dll', 'cl_dlls/client_amd64.dll'):
    if not os.path.isfile(os.path.join(GAME, *rel.split('/'))):
        todo.append('run/svencraft/%s: setup.bat (or tools/build.sh game)' % rel)
for rel, tool in (('scripts/blocks.txt', 'make_blocktex.py'), ('models/svencraft/blockitem.mdl', 'make_blockassets.py'),
                  ('sprites/svencraft/items.spr', 'make_items.py'), ('models/svencraft/v_schand.mdl', 'make_hand.py'),
                  ('models/svencraft/v_scpickaxe1.mdl', 'make_tools.py'), ('models/svencraft/creeper.mdl', 'make_creeper.py'),
                  ('sprites/svencraft/font.spr', 'make_font.py'), ('sound/materials.txt', 'make_materials.py'),
                  ('skill.cfg', 'make_skillcfg.py'), ('maps/svencraft_sandbox.bsp', 'maps_src/make_town.py')):
    if not os.path.isfile(os.path.join(GAME, *rel.split('/'))):
        todo.append('run/svencraft/%s: python %s' % (rel, tool if '/' in tool else 'tools/' + tool))

print(('would do' if CHECK else 'done') + ':' + ('' if done else ' nothing'))
for d in done:
    print('  ' + d)
if todo:
    print('still missing (docs/BUILDING.md):')
    for t in todo:
        print('  ' + t)
