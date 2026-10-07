# Where Svencraft's tools find things. Paths come from this file's location (not a fixed drive path), so the
# generators run from any checkout; each one can be overridden with an environment variable.
#
#   ROOT     the project root (holds tools/, maps_src/, run/, engine/, game/)        - this file's parent folder
#   RUN      the runtime folder (xash3d.exe and the game folders)                    SVENCRAFT_RUN      (ROOT/run)
#   GAMEDIR  the game folder the generators write into                               SVENCRAFT_GAMEDIR  (RUN/svencraft)
#   SVEN     Sven Co-op's content folder (".../Sven Co-op/svencoop")                  SVENCRAFT_SVEN
#            default: RUN/svencoop (the junction the game reads) if present, else Steam's standard install path
#   SDK      the Sven Co-op SDK folder (modelling/studiomdl.exe, sprites/sprgen.exe,  SVENCRAFT_SDK
#            mapping/compilers/SC-*.exe); default: Steam's standard install path
#   BUILD    where generators keep their work folders (build_<name>)                 SVENCRAFT_BUILD    (ROOT/tools)
#   MDLDEC   Xash3D's model decompiler (ships with the engine)                        SVENCRAFT_MDLDEC   (RUN/mdldec.exe)
#   ASSETS   hand-kept source art (block textures, the old test map's WAD)           - ROOT/assets_src
#
#   python tools/sc_paths.py     prints them (and whether each exists)
import os

STEAM_COMMON = r"C:\Program Files (x86)\Steam\steamapps\common"   # Steam's default library (generic, not per user)


def _env(name, default):
    v = os.environ.get(name)
    return os.path.abspath(v) if v else default


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUN = _env('SVENCRAFT_RUN', os.path.join(ROOT, 'run'))
GAMEDIR = _env('SVENCRAFT_GAMEDIR', os.path.join(RUN, 'svencraft'))
_sven_link = os.path.join(RUN, 'svencoop')
SVEN = _env('SVENCRAFT_SVEN', _sven_link if os.path.isdir(_sven_link) else os.path.join(STEAM_COMMON, 'Sven Co-op', 'svencoop'))
SDK = _env('SVENCRAFT_SDK', os.path.join(STEAM_COMMON, 'Sven Co-op SDK'))
BUILD = _env('SVENCRAFT_BUILD', os.path.join(ROOT, 'tools'))
MDLDEC = _env('SVENCRAFT_MDLDEC', os.path.join(RUN, 'mdldec.exe'))
ASSETS = os.path.join(ROOT, 'assets_src')


def work(name):
    """a generator's work folder: BUILD/build_<name>"""
    return os.path.join(BUILD, 'build_' + name)


if __name__ == '__main__':
    for k in ('ROOT', 'RUN', 'GAMEDIR', 'SVEN', 'SDK', 'BUILD', 'MDLDEC', 'ASSETS'):
        p = globals()[k]
        print('%-8s %s%s' % (k, p, '' if os.path.exists(p) else '   (missing)'))
