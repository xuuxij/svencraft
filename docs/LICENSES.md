# Licences

This repo combines code under different licences with our own work. It is private; before it is ever made public
or shared, the points marked **decide** need the owner's decision.

## Per part

| Part | Licence | Notes |
|---|---|---|
| `engine/` (Xash3D FWGS fork) | GNU GPL v3 or later | stated in each source file's header (template: `engine/Documentation/gpl_copyright_header.h`); upstream ships no top-level licence file. A few files carry the Half-Life SDK notice (`engine/pm_shared/`, parts of `engine/common/`). Our engine files (`voxel.c`, `dynworld.c`, `gl_voxel.c`, `gl_dynworld.c`, ...) are part of this GPL work. |
| `engine/3rdparty/*` | their own | submodules (mainui, nanogl, gl4es, opus, ogg/vorbis, mpg123, bzip2, mbedtls, libbacktrace, ...): BSD/MIT/zlib-style, LGPL and others; see each project |
| `game/` (hlsdk-portable fork) | Half-Life 1 SDK licence (`game/LICENSE`) | free use and distribution of modified source and binaries, but **only for free** (no sale), the licence file must be included, and the result must run on the Half-Life engine family. GPL code must not be copied into `game/` (the licences are incompatible); `docs/REFERENCES.md` notes each source's licence. |
| `game/freevgui` | its own (BSD-style, see its `LICENSE`) | submodule |
| `game/dlls/gearbox/`, `cl_dll/gearbox/` | Half-Life SDK licence | Opposing Force code from hlsdk-portable's `opforfixed` branch |
| `game/dlls/svencraft/sc_lagcomp.cpp` | Half-Life SDK licence | adapted from SevenKewp (`dlls/util/lagcomp.cpp`, https://github.com/wootguy/SevenKewp) |
| `game/dlls/svencraft/sc_medkit.cpp` | Half-Life SDK licence | adapted from SevenKewp (`dlls/weapon/CMedkit.cpp`); values from SevenKewp_data's `skill.cfg` |
| `game/dlls/svencraft/sc_hwgrunt.cpp`, `sc_robogrunt.cpp` | Half-Life SDK licence | written on Half-Life's `CHGrunt` after SevenKewp's `CHWGrunt` / `CRoboGrunt` |
| `game/dlls/svencraft/sc_tor.cpp`, `sc_kingpin.cpp`, `sc_stukabat.cpp`; `CBabyGarg` in `game/dlls/gargantua.cpp` | Half-Life SDK licence | rewritten on Half-Life's `CBaseMonster` / `CGargantua` after SevenKewp's `CTor`, `CKingpin`, `CStukabat`, `CBabyGarg` |
| `game/dlls/svencraft/`, `game/cl_dll/svencraft/`, `game/common/sc_*.h` (otherwise) | ours, inside an HL-SDK-licensed work | as part of the game DLLs they must follow the HL SDK licence's terms when distributed |
| `tools/`, `maps_src/`, `assets_src/`, `legacy/`, docs, `run/svencraft` text files | ours | **decide**: until then, all rights reserved (private) |
| Generated art (block textures, item icons, the creeper, the font, HUD sprites in `run/svencraft/sprites/svencraft/`) | ours (procedural, drawn in code) | **decide** together with the code |

## Not included, and never to be committed

- **Sven Co-op content** (models, sounds, sprites, textures/WADs, maps) and **Half-Life content**: copyrighted
  by their owners. The game reads it at run time from your own Sven Co-op install through `run/svencoop`, and
  the generators derive files from it locally. These generated files are therefore not in the repo:
  - the realistic block textures (copied from Sven's WADs) in `gfx/blocks/`, `svencraft_dyn.wad`, the compiled
    maps (`.bsp`, `.dyn`) and the map WADs, which embed Sven/Half-Life textures;
  - the hand and tool models (Sven's HEV glove and crowbar models with our geometry);
  - `skill.cfg` and `sound/materials.txt` (built from Sven's files);
  - `sprites/640_pain.spr` / `320_pain.spr` (a copy of Sven's `pain.spr`);
  - the contact sheets `docs/town_v3_*.png` (pictures of Sven/Half-Life textures; regenerate with
    `python tools/wadtex.py sheet <out.png> <TEXTURE> ...`).
- **Sven Co-op SDK** tools (studiomdl, sprgen, compilers): used, not redistributed.
- **Engine runtime package** (`xash3d.exe`, SDL2, FFmpeg DLLs, `menu.dll`, ...): downloaded from Xash3D FWGS'
  releases (GPL and third-party licences); not in the repo.
- Minecraft: no Mojang code or assets are used; block textures, mobs, items and fonts are original, made in code.

## Before any public release (checklist)

- [ ] **decide** the licence for our own code and art (and whether it may differ between `engine/` additions,
      which are GPLv3+, the game DLL code, which follows the HL SDK licence, and the tools/art).
- [ ] add the GPL v3 text (`COPYING`) for `engine/`, keep `game/LICENSE`, add third-party notices.
- [ ] make sure no Sven Co-op or Half-Life content is in the history (`tools/publish/audit.py` checks file types).
- [ ] "Sven Co-op" and "Half-Life" are third-party names; the game needs the user's own copy of Sven Co-op.
