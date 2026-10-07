# Publishing to GitHub

Target: one **private** repo, `xuuxij/svencraft`, whose layout mirrors the project folder. Nothing is pushed until
the owner asks for it; then one command does it (below). Every push must be free of personal data, and every
commit is made as `xuuxij <20761916+xuuxij@users.noreply.github.com>` with UTC dates.

## How it works

The project folder itself is not a git repo: `engine/` and `game/` are their own upstream clones (with our changes
uncommitted, see docs/UPSTREAM.md) and cannot be nested in a parent repo as plain folders. So publishing is an
**export**: `tools/publish/export.py` copies exactly what `tools/publish/manifest.txt` selects into a staging
folder next to the project, `../Svencraft-publish`, and that folder is the repo.

```
project/                          ../Svencraft-publish/  (staging = the GitHub repo)
  README.md DEVLOG.md DESIGN.md  ->  same
  docs/ tools/ maps_src/*.py     ->  same (minus generated files and one-off scripts)
  assets_src/ legacy/            ->  same
  run/svencraft/<hand-kept>      ->  same (gameinfo.txt, userconfig.d, weapon_sc_*.txt, 10 prebuilt sprites)
  engine/ (clone, + our changes) ->  engine/: tracked files as on disk + our new files; .git not copied
  engine/3rdparty/* (submodules) ->  gitlinks, pinned to the commits checked out now (.gitmodules generated)
  game/ (clone, + our changes)   ->  game/: likewise; game/freevgui as a gitlink
                                     .export/  bookkeeping, never committed (file list with sha256, pins, warnings)
```

- Files are copied byte for byte (timestamps kept). `.gitattributes` (`* -text`) stops git from converting line
  endings, so the repo matches the working tree; the one exception is `engine/.gitattributes` (`*.c *.h wscript`
  stored as LF), which affects only engine files that are CRLF on disk (export.py warns about them: currently
  `engine/engine/common/dynworld.c`, which a clone gets with LF endings; harmless to the build).
- Refreshing the export removes staging files the manifest no longer selects. It only ever writes and deletes
  inside the staging folder, and refuses to touch a non-empty folder without its `.export/MARKER`.
- `tools/publish/audit.py` checks the staging folder (and its git history once there is one) and fails on any
  hit: personal strings derived on this machine (Windows user and computer name, profile path, global git e-mail,
  local time zone) plus the local-only list `tools/publish/audit-needles.local.txt`; personal folders, assistant
  session folders and links, LAN/MAC addresses, tokens and keys, e-mail addresses in our files; PNG/JPEG
  metadata; game content and build products by file type (`.mdl .spr .wad .bsp .wav .dll .exe .pk3 ...`) and
  engine runtime files (`config.cfg`, `.xash_id`, ...); files over 50 MB; commit identities and dates.
  `tools/publish/audit-allow.txt` lists the vetted exceptions (the prototype's prebuilt sprites and WAD, a few
  unchanged upstream files with example IPs or image metadata). Fix hits **at the source in the project**, never
  by editing the staging copy, then export again.

## Dry run (any time; no git, nothing leaves the machine)

```sh
sh tools/publish/export.sh            # -> ../Svencraft-publish   (add a folder to export elsewhere; --list to just list)
python tools/publish/audit.py         # exit 0 = clean
```

## Publishing (only when the owner says so)

```sh
sh tools/publish/publish.sh --yes -m "Svencraft snapshot 2026-10-07"
```

It refuses to run without `--yes`. Steps: checks that `gh` is logged in as `xuuxij`; export; audit; `git init -b
main` on the first run; local git identity = the noreply address (the global one is personal), `core.autocrlf
false`; stages everything except `.export/` (forced, so upstream `.gitignore` files cannot drop tracked upstream
files), pins the 18 submodule gitlinks from `.export/submodules.txt`; commits with `TZ=UTC` and explicit
`+0000` author/committer dates (no commit when nothing changed); audits again, now including the history; then on
the first run `gh repo create xuuxij/svencraft --private --source . --remote origin --push`, afterwards
`git push origin HEAD:main`.

Cloning it: `git clone --recursive https://github.com/xuuxij/svencraft.git` (the submodules come from their
public upstreams), then docs/BUILDING.md.

## Checklist before each publish

- [ ] `DEVLOG.md` has an entry for what changed; README "Status" still true.
- [ ] New tools or hand-kept files are covered by `tools/publish/manifest.txt` (export prints rules that match
      nothing; `export.sh --list` shows the selection).
- [ ] No absolute paths in new scripts: use `tools/sc_paths.py` (Python) or the script's own location.
- [ ] Export warnings read (`../Svencraft-publish/.export/warnings.txt`): deleted upstream files, submodules with
      local changes (not exported!), CRLF engine sources, big files.
- [ ] Audit passes; any new `audit-allow.txt` entry is a real false positive, not personal data.
- [ ] Nothing from Sven Co-op / Half-Life slipped in (docs/LICENSES.md).
- [ ] Screenshots or images added to docs: no metadata (the audit checks PNG/JPEG), nothing personal on screen.

## Verifying a fresh checkout

The export doubles as a clean checkout for testing the build pipeline without touching the live game folder:

```sh
python tools/publish/export.py <some folder outside the project>
cd <that folder> && python tools/setup_run.py      # junction to Sven Co-op, folders, pain sprite
SVENCRAFT_MDLDEC=<project>/run/mdldec.exe python tools/make_blocktex.py   # ... and the other generators
```

and compare its `run/svencraft` with the live one (sha256). Done on 2026-10-07 for every generator except the
two map builders (`make_town.py`, `make_testmap.py`: they need minutes and the map compilers; their paths were
checked): all 96 files the check tree had (generated, hand-kept, prebuilt, set up) and `game/common/sc_items.h`
were byte-identical to the live ones.

## Alternatives considered

- **Make the project folder the repo and turn `engine/` and `game/` into submodules pointing at our own forks**
  (`xuuxij/xash3d-fwgs`, `xuuxij/hlsdk-portable`, with our changes committed on a branch there). Cleanest git
  history and upstream merges with real git merges, but three repos to keep in step, GitHub forks of public repos
  cannot be private (separate private mirror repos would be needed), and our changes would have to be committed
  in the clones (today they are kept uncommitted on purpose). Possible later.
- **git subtree** of the upstreams inside one repo: upstream history in our repo (hundreds of MB), awkward
  merges with uncommitted local changes.
- **Patches instead of trees** (store `git diff` of each clone plus a base commit): small repo, but a clone does
  not build without applying patches, and binary/new files are clumsy. The export keeps the full trees.
- **Commit straight from the project folder with a top-level `.gitignore`**: impossible without removing the
  nested `.git` folders, and it would expose whatever is lying around to `git add`; the manifest-driven export
  is an allowlist and can be audited before anything leaves.
- **Including generated assets** so a clone runs without the generators: most are derived from Sven Co-op
  content (not ours to publish); the original-art ones (creeper, font, icons) could be added later if wanted.
