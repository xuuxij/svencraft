# Svencraft setup: one command from a fresh clone to a playable sandbox (Windows x64).
#
#   setup.bat                   build and generate whatever is missing (safe to run again)
#   setup.bat --rebuild         build the engine and game again and regenerate every asset and the map
#   setup.bat --check           only check the prerequisites
#   (or: py -3 tools/setup.py [...])
#
# Install first (README.md, "Playing it"): Visual Studio Build Tools with "Desktop development with C++",
# Python 3.8+, Git for Windows, and on Steam: Sven Co-op and the Sven Co-op SDK (Library > Tools).
# The rest it does itself:
#   1. Python's numpy and Pillow (pip), the engine's and game's git submodules
#   2. finds Sven Co-op and its SDK in any Steam library (or SVENCRAFT_SVEN / SVENCRAFT_SDK)
#   3. SDL2's development package (downloaded into deps/SDL2_VC)
#   4. builds the engine (launcher, engine, renderer, menu, filesystem, model decompiler) and the game, and puts
#      them in run/
#   5. tools/setup_run.py (folders, the run/svencoop junction, files copied from Sven Co-op)
#   6. the generated assets and the sandbox map (docs/BUILDING.md, "Generated assets")
# Everything the steps print goes to setup.log in the project folder as well.
import os, sys, subprocess, shutil, re, time, zipfile, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUN = os.path.join(ROOT, 'run')
GAME = os.path.join(RUN, 'svencraft')
LOG = os.path.join(ROOT, 'setup.log')
SDL2_URL = 'https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip'
SDL2_DIR = os.path.join(ROOT, 'deps', 'SDL2_VC')
REBUILD = '--rebuild' in sys.argv
CHECK = '--check' in sys.argv
_log = None


def say(msg=''):
    print(msg, flush=True)
    if _log:
        _log.write(msg + '\n')
        _log.flush()


def fail(msg):
    say()
    say('SETUP STOPPED: ' + msg)
    say('(the full output is in setup.log)')
    sys.exit(1)


def run(cmd, cwd=ROOT, what=None, env=None):
    """runs a command, its output into setup.log; stops setup when it fails"""
    say('  > ' + ' '.join(cmd) + ('   [in %s]' % os.path.relpath(cwd, ROOT) if cwd != ROOT else ''))
    t = time.time()
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    out = p.stdout.decode('utf-8', 'replace')
    if _log:
        _log.write(out + '\n')
        _log.flush()
    if p.returncode != 0:
        tail = '\n'.join(out.splitlines()[-25:])
        say(tail)
        fail('%s failed (exit %d)' % (what or cmd[0], p.returncode))
    say('    done in %.0f s' % (time.time() - t))
    return out


# ---------------------------------------------------------------- prerequisites
def check_python():
    if sys.version_info < (3, 8):
        fail('Python 3.8 or newer is needed (this is %d.%d)' % sys.version_info[:2])
    missing = []
    for mod, pkg in (('numpy', 'numpy'), ('PIL', 'pillow')):
        try:
            __import__(mod)
        except ImportError:
            missing.append(pkg)
    if missing:
        if CHECK:
            say('  Python packages missing: ' + ' '.join(missing))
            return
        say('  installing Python packages: ' + ' '.join(missing))
        run([sys.executable, '-m', 'pip', 'install', '--user'] + missing, what='pip install')
    say('  Python %d.%d with numpy and Pillow' % sys.version_info[:2])


def check_git_submodules():
    git = shutil.which('git')
    if not git:
        fail('Git is not installed (https://git-scm.com/download/win), or not on PATH')
    if not os.path.isdir(os.path.join(ROOT, '.git')):
        say('  not a git clone: skipping the submodules (they must be in place already)')
        return
    st = subprocess.run([git, 'submodule', 'status', '--recursive'], cwd=ROOT, capture_output=True, text=True).stdout
    todo = [l for l in st.splitlines() if l.startswith('-')]
    if todo and not CHECK:
        say('  fetching %d submodules (the engine\'s and game\'s third-party code)' % len(todo))
        run([git, 'submodule', 'update', '--init', '--recursive'], what='git submodule update')
    elif todo:
        say('  %d submodules not fetched yet' % len(todo))
    else:
        say('  submodules in place')


def find_msvc():
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'), 'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    if os.path.isfile(vswhere):
        out = subprocess.run([vswhere, '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                              '-property', 'installationPath'], capture_output=True, text=True).stdout.strip()
        if out:
            say('  C++ build tools: ' + out.splitlines()[0])
            return
    fail('the Visual Studio C++ build tools are not installed: install "Build Tools for Visual Studio"\n'
         '  (https://visualstudio.microsoft.com/downloads/, under "Tools for Visual Studio") with the\n'
         '  "Desktop development with C++" workload, then run setup again')


def steam_libraries():
    libs = []
    try:
        import winreg
        for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
            for key in (r'Software\Valve\Steam', r'Software\WOW6432Node\Valve\Steam'):
                try:
                    with winreg.OpenKey(hive, key) as k:
                        for name in ('SteamPath', 'InstallPath'):
                            try:
                                libs.append(os.path.normpath(winreg.QueryValueEx(k, name)[0]))
                            except OSError:
                                pass
                except OSError:
                    pass
    except ImportError:
        pass
    libs.append(r'C:\Program Files (x86)\Steam')
    found = []
    for steam in libs:
        vdf = os.path.join(steam, 'steamapps', 'libraryfolders.vdf')
        if os.path.isfile(vdf):
            for m in re.finditer(r'"path"\s+"([^"]+)"', open(vdf, encoding='utf-8', errors='replace').read()):
                found.append(os.path.normpath(m.group(1).replace('\\\\', '\\')))
        found.append(steam)
    out = []
    for f in found:
        if f.lower() not in [o.lower() for o in out]:
            out.append(f)
    return out


def find_sven():
    """Sven Co-op's content folder and the SDK: SVENCRAFT_SVEN / SVENCRAFT_SDK, else any Steam library"""
    sven, sdk = os.environ.get('SVENCRAFT_SVEN'), os.environ.get('SVENCRAFT_SDK')
    for lib in steam_libraries():
        common = os.path.join(lib, 'steamapps', 'common')
        if not sven and os.path.isfile(os.path.join(common, 'Sven Co-op', 'svencoop', 'halflife.wad')):
            sven = os.path.join(common, 'Sven Co-op', 'svencoop')
        if not sdk and os.path.isfile(os.path.join(common, 'Sven Co-op SDK', 'modelling', 'studiomdl.exe')):
            sdk = os.path.join(common, 'Sven Co-op SDK')
    if not sven or not os.path.isdir(sven):
        fail('Sven Co-op was not found: install it from Steam (free), or set SVENCRAFT_SVEN to its "svencoop" folder')
    for rel in (r'modelling\studiomdl.exe', r'sprites\sprgen.exe', r'mapping\compilers\SC-CSG_x64.exe'):
        if not sdk or not os.path.isfile(os.path.join(sdk, rel)):
            fail('the Sven Co-op SDK was not found (or is incomplete: %s): install it from Steam, Library > Tools >\n'
                 '  "Sven Co-op SDK", or set SVENCRAFT_SDK to its folder' % rel)
    # the tools and the game's junction take these
    os.environ['SVENCRAFT_SVEN'] = sven
    os.environ['SVENCRAFT_SDK'] = sdk
    say('  Sven Co-op: ' + sven)
    say('  Sven Co-op SDK: ' + sdk)


def get_sdl2():
    if os.path.isfile(os.path.join(SDL2_DIR, 'include', 'SDL.h')) and os.path.isfile(os.path.join(SDL2_DIR, 'lib', 'x64', 'SDL2.dll')):
        say('  SDL2: ' + os.path.relpath(SDL2_DIR, ROOT))
        return
    if CHECK:
        say('  SDL2 development package: not downloaded yet')
        return
    os.makedirs(os.path.dirname(SDL2_DIR), exist_ok=True)
    zpath = os.path.join(os.path.dirname(SDL2_DIR), 'SDL2.zip')
    say('  downloading SDL2 (' + SDL2_URL + ')')
    urllib.request.urlretrieve(SDL2_URL, zpath)
    with zipfile.ZipFile(zpath) as z:
        top = z.namelist()[0].split('/')[0]
        z.extractall(os.path.dirname(SDL2_DIR))
    if os.path.isdir(SDL2_DIR):
        shutil.rmtree(SDL2_DIR)
    os.rename(os.path.join(os.path.dirname(SDL2_DIR), top), SDL2_DIR)
    os.remove(zpath)
    say('  SDL2 unpacked into ' + os.path.relpath(SDL2_DIR, ROOT))


# ---------------------------------------------------------------- building
ENGINE_FILES = [  # what the engine build makes -> run/
    ('build/game_launch/xash3d.exe', 'xash3d.exe'), ('build/engine/xash.dll', 'xash.dll'),
    ('build/ref/gl/ref_gl.dll', 'ref_gl.dll'), ('build/3rdparty/mainui/menu.dll', 'menu.dll'),
    ('build/filesystem/filesystem_stdio.dll', 'filesystem_stdio.dll'), ('build/utils/mdldec/mdldec.exe', 'mdldec.exe'),
]
GAME_FILES = [('build/dlls/hl_amd64.dll', 'svencraft/dlls/hl_amd64.dll'),
              ('build/cl_dll/client_amd64.dll', 'svencraft/cl_dlls/client_amd64.dll')]


def waf(tree, configure_args):
    cwd = os.path.join(ROOT, tree)
    if REBUILD or not os.path.isfile(os.path.join(cwd, 'build', 'c4che', '_cache.py')):
        run([sys.executable, 'waf', 'configure', '-T', 'release', '-8', '--msvc_targets=x64'] + configure_args, cwd=cwd,
            what=tree + ' configure')
    run([sys.executable, 'waf', 'build'], cwd=cwd, what=tree + ' build')


def install(tree, files):
    for src, dst in files:
        s, d = os.path.join(ROOT, tree, *src.split('/')), os.path.join(RUN, *dst.split('/'))
        if not os.path.isfile(s):
            fail('the %s build did not make %s' % (tree, src))
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copy2(s, d)


def build_engine():
    if not REBUILD and all(os.path.isfile(os.path.join(RUN, d)) for s, d in ENGINE_FILES):
        say('  engine already built (setup.bat --rebuild builds it again)')
        return
    waf('engine', ['-s', SDL2_DIR, '--enable-utils'])
    install('engine', ENGINE_FILES)
    shutil.copy2(os.path.join(SDL2_DIR, 'lib', 'x64', 'SDL2.dll'), os.path.join(RUN, 'SDL2.dll'))
    os.makedirs(GAME, exist_ok=True)
    shutil.copy2(os.path.join(ROOT, 'engine', 'build', '3rdparty', 'extras', 'extras.pk3'), os.path.join(GAME, 'extras.pk3'))
    say('  engine installed in run/')


def build_game():
    if not REBUILD and all(os.path.isfile(os.path.join(RUN, *d.split('/'))) for s, d in GAME_FILES):
        say('  game already built (setup.bat --rebuild builds it again)')
        return
    waf('game', ['--prefix=' + RUN])
    install('game', GAME_FILES)
    say('  game installed in run/svencraft/')


# ---------------------------------------------------------------- assets
GENERATORS = [  # (script, a file it makes): in this order (docs/BUILDING.md)
    ('tools/make_blocktex.py', 'scripts/blocks.txt'),
    ('tools/make_blockassets.py', 'models/svencraft/blockitem.mdl'),
    ('tools/make_items.py', 'models/svencraft/itemflat.mdl'),
    ('tools/make_hand.py', 'models/svencraft/v_schand.mdl'),
    ('tools/make_tools.py', 'models/svencraft/v_scpickaxe1.mdl'),
    ('tools/make_creeper.py', 'models/svencraft/creeper.mdl'),
    ('tools/make_font.py', 'sprites/svencraft/font.spr'),
    ('tools/make_materials.py', 'sound/materials.txt'),
    ('tools/make_skillcfg.py', 'skill.cfg'),
    ('maps_src/make_town.py', 'maps/svencraft_sandbox.bsp'),
]


def generate():
    for script, made in GENERATORS:
        if not REBUILD and os.path.isfile(os.path.join(GAME, *made.split('/'))):
            say('  %-28s already made' % script)
            continue
        if script.endswith('make_town.py'):
            say('  compiling the sandbox map: this takes a few minutes')
        run([sys.executable, os.path.join(ROOT, *script.split('/'))], cwd=os.path.dirname(os.path.join(ROOT, script)),
            what=script)
        if not os.path.isfile(os.path.join(GAME, *made.split('/'))):
            fail('%s ran but %s is missing' % (script, made))


# ---------------------------------------------------------------- main
def main():
    global _log
    _log = open(LOG, 'w', encoding='utf-8')
    say('Svencraft setup (%s)' % time.strftime('%Y-%m-%d %H:%M'))
    if os.name != 'nt':
        fail('setup runs on Windows (x64) only for now')
    say()
    say('[1/6] Python, Git and the submodules')
    check_python()
    check_git_submodules()
    say('[2/6] Visual Studio C++ build tools, Sven Co-op and its SDK')
    find_msvc()
    find_sven()
    say('[3/6] SDL2')
    get_sdl2()
    if CHECK:
        say()
        say('Prerequisites look fine. Run setup.bat without --check to build.')
        return
    say('[4/6] building the engine and the game (the first time takes a few minutes)')
    tasks = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq xash3d.exe'], capture_output=True, text=True).stdout
    if 'xash3d.exe' in tasks.lower():
        fail('Svencraft (xash3d.exe) is running: close the game, then run setup again')
    build_engine()
    build_game()
    say('[5/6] the run folder')
    run([sys.executable, os.path.join(ROOT, 'tools', 'setup_run.py')], what='setup_run.py')
    say('[6/6] generated assets and the map')
    generate()
    out = run([sys.executable, os.path.join(ROOT, 'tools', 'setup_run.py'), '--check'], what='setup_run.py --check')
    if 'missing' in out.lower() or 'todo' in out.lower():
        say(out)
    say()
    say('Svencraft is ready: double-click "Play Svencraft.bat".')


if __name__ == '__main__':
    try:
        main()
    except SystemExit:
        raise
    except BaseException as e:		# anything unexpected: say what, keep the details in the log
        import traceback
        if _log:
            _log.write(traceback.format_exc())
        fail('%s: %s' % (type(e).__name__, e))
