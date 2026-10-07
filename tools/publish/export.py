"""Build or refresh the publishing staging folder from tools/publish/manifest.txt.

    python tools/publish/export.py [DEST] [--list]

DEST defaults to <project>/../Svencraft-publish. The staging folder mirrors the project's layout but holds only
the files the manifest selects, copied byte for byte (timestamps kept). It also gets:
  .gitmodules                 the engine's and game's submodules, re-rooted (engine/3rdparty/..., game/freevgui)
  <submodule paths>/          empty folders where the gitlinks go (publish.sh pins them to .export/submodules.txt)
  .export/                    bookkeeping, never committed: files.txt (sha256 size path), submodules.txt
                              (sha path url), warnings.txt, and the marker that makes the folder safe to refresh
Refreshing removes staging files the manifest no longer selects, but only inside DEST (never .git/), and only when
DEST is empty, new, or carries the .export marker. Nothing outside DEST is written or deleted.
--list prints the selected files and writes nothing.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
MANIFEST = os.path.join(HERE, 'manifest.txt')
MARKER = 'svencraft-export-staging'


def glob_re(g):
    out, i = '', 0
    while i < len(g):
        if g.startswith('**/', i):
            out += '(?:.*/)?'; i += 3; continue
        if g.startswith('**', i):
            out += '.*'; i += 2; continue
        c = g[i]
        out += '[^/]*' if c == '*' else '[^/]' if c == '?' else re.escape(c)
        i += 1
    return re.compile(out + r'\Z')


def read_manifest():
    inc, exc, gits = [], [], []
    for n, line in enumerate(open(MANIFEST, encoding='utf-8'), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        kind, _, arg = line.partition(' ')
        arg = arg.strip()
        if kind == '+':
            inc.append(arg)
        elif kind == '-':
            exc.append(glob_re(arg))
        elif kind == 'git':
            gits.append(arg.strip('/'))
        else:
            raise SystemExit('manifest line %d: unknown rule %r' % (n, line))
    return inc, exc, gits


def walk_glob(pattern):
    """project files matching an include glob (walks only the folder before the first wildcard)"""
    parts = pattern.split('/')
    lit = []
    for p in parts:
        if any(ch in p for ch in '*?['):
            break
        lit.append(p)
    if len(lit) == len(parts):
        return [pattern] if os.path.isfile(os.path.join(ROOT, *parts)) else []
    rx = glob_re(pattern)
    base = os.path.join(ROOT, *lit)
    found = []
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames[:] = [d for d in dirnames if d != '.git' and not os.path.islink(os.path.join(dirpath, d))]
        for f in filenames:
            rel = os.path.relpath(os.path.join(dirpath, f), ROOT).replace(os.sep, '/')
            if rx.match(rel):
                found.append(rel)
    return found


def git(repo, *args):
    r = subprocess.run(['git', '-C', repo] + list(args), capture_output=True)
    if r.returncode != 0:
        raise SystemExit('git %s in %s failed: %s' % (' '.join(args), repo, r.stderr.decode(errors='replace')))
    return r.stdout.decode('utf-8', 'surrogateescape')


def clone_files(d, warnings):
    """(files, submodules) of the nested clone ROOT/d: files relative to ROOT; submodules (path, sha, url)"""
    repo = os.path.join(ROOT, d)
    files, subs, gitlinks = [], [], set()
    for rec in git(repo, 'ls-files', '-s', '-z').split('\0'):
        if not rec:
            continue
        meta, path = rec.split('\t', 1)
        mode = meta.split()[0]
        if mode == '160000':
            gitlinks.add(path)
            continue
        if os.path.isfile(os.path.join(repo, path)):
            files.append(d + '/' + path)
        elif not os.path.exists(os.path.join(repo, path)):
            warnings.append('%s/%s: tracked upstream but deleted here (not exported)' % (d, path))
    for path in git(repo, 'ls-files', '--others', '--exclude-standard', '-z').split('\0'):
        if path and os.path.isfile(os.path.join(repo, path)):
            files.append(d + '/' + path)
    urls = {}
    if os.path.isfile(os.path.join(repo, '.gitmodules')):
        cfg = subprocess.run(['git', 'config', '-f', os.path.join(repo, '.gitmodules'), '--get-regexp', r'^submodule\..*\.(path|url)$'],
                             capture_output=True).stdout.decode()
        tmp = {}
        for line in cfg.splitlines():
            key, val = line.split(' ', 1)
            name, field = key[len('submodule.'):].rsplit('.', 1)
            tmp.setdefault(name, {})[field] = val
        urls = {v['path']: v.get('url', '') for v in tmp.values() if 'path' in v}
    for line in git(repo, 'submodule', 'status').splitlines():
        flag, rest = line[:1], line[1:].split()
        sha, path = rest[0], rest[1]
        if flag == '-':
            warnings.append('%s/%s: submodule not checked out; pinned to the recorded commit' % (d, path))
        elif flag == '+':
            warnings.append('%s/%s: checked-out commit differs from the one the clone records; pinned to the checked-out one' % (d, path))
        elif flag == 'U':
            warnings.append('%s/%s: submodule has merge conflicts' % (d, path))
        if flag != '-':
            dirty = git(os.path.join(repo, path), 'status', '--porcelain', '--untracked-files=no')
            if dirty.strip():
                warnings.append('%s/%s: submodule has local changes that are NOT exported' % (d, path))
        subs.append((d + '/' + path, sha, urls.get(path, '')))
        gitlinks.discard(path)
    for path in sorted(gitlinks):
        warnings.append('%s/%s: gitlink without a submodule entry (skipped)' % (d, path))
    return files, subs


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def safe_dest(dest):
    dest = os.path.abspath(dest)
    root = os.path.abspath(ROOT)
    norm = lambda p: os.path.normcase(p.rstrip('\\/')) + os.sep
    if norm(dest).startswith(norm(root)) or norm(root).startswith(norm(dest)):
        raise SystemExit('refusing: %s overlaps the project folder %s' % (dest, root))
    if os.path.splitdrive(dest)[1] in ('', '\\', '/'):
        raise SystemExit('refusing: %s is a drive root' % dest)
    if os.path.isdir(dest) and os.listdir(dest):
        marker = os.path.join(dest, '.export', 'MARKER')
        if not (os.path.isfile(marker) and open(marker).read().strip() == MARKER):
            raise SystemExit('refusing: %s is not empty and has no .export/MARKER (not a staging folder made by this script)' % dest)
    return dest


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    listing = '--list' in sys.argv
    dest = args[0] if args else os.path.join(os.path.dirname(ROOT), 'Svencraft-publish')

    inc, exc, gits = read_manifest()
    warnings = []
    selected = set()
    for g in inc:
        hits = walk_glob(g)
        if not hits:
            warnings.append('manifest: %r matched nothing' % g)
        selected.update(hits)
    subs = []
    for d in gits:
        f, s = clone_files(d, warnings)
        selected.update(f)
        subs += s
    excluded = {p for p in selected if any(rx.match(p) for rx in exc)}
    files = sorted(selected - excluded)
    sub_paths = {p for p, _, _ in subs}
    files = [p for p in files if not any(p.startswith(s + '/') for s in sub_paths)]

    if listing:
        for p in files:
            print(p)
        print('%d files (%d excluded by rules), %d submodules' % (len(files), len(excluded), len(subs)), file=sys.stderr)
        return

    dest = safe_dest(dest)
    os.makedirs(os.path.join(dest, '.export'), exist_ok=True)
    open(os.path.join(dest, '.export', 'MARKER'), 'w').write(MARKER + '\n')

    copied = 0
    total = 0
    want = set(files)
    for p in files:
        src = os.path.join(ROOT, *p.split('/'))
        dst = os.path.join(dest, *p.split('/'))
        st = os.stat(src)
        total += st.st_size
        if os.path.isfile(dst):
            dt = os.stat(dst)
            if dt.st_size == st.st_size and int(dt.st_mtime) == int(st.st_mtime):
                continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1

    # .gitmodules for the re-rooted submodules, and their (empty) folders
    gm = ''.join('[submodule "%s"]\n\tpath = %s\n\turl = %s\n' % (p, p, u) for p, _, u in subs)
    open(os.path.join(dest, '.gitmodules'), 'w', newline='\n').write(gm)
    want.add('.gitmodules')
    for p, _, _ in subs:
        os.makedirs(os.path.join(dest, *p.split('/')), exist_ok=True)

    # remove what the manifest no longer selects (inside dest only; .git and .export are left alone)
    removed = 0
    keep_dirs = {os.path.normcase(os.path.join(dest, *p.split('/'))) for p, _, _ in subs}
    for dirpath, dirnames, filenames in os.walk(dest, topdown=False):
        rel_dir = os.path.relpath(dirpath, dest).replace(os.sep, '/')
        top = rel_dir.split('/')[0]
        if top in ('.git', '.export'):
            continue
        for f in filenames:
            rel = (f if rel_dir == '.' else rel_dir + '/' + f)
            if rel not in want:
                os.remove(os.path.join(dirpath, f))
                removed += 1
        if rel_dir != '.' and os.path.normcase(dirpath) not in keep_dirs and not os.listdir(dirpath):
            os.rmdir(dirpath)

    # checks the repo's attributes would undo: engine/.gitattributes normalizes *.c *.h wscript to LF
    for p in files:
        if p.startswith('engine/') and (p.endswith(('.c', '.h')) or p.endswith('/wscript')):
            if b'\r\n' in open(os.path.join(dest, *p.split('/')), 'rb').read():
                warnings.append('%s: has CRLF but engine/.gitattributes stores it as LF (clones will differ)' % p)
    big = [(os.path.getsize(os.path.join(dest, *p.split('/'))), p) for p in files]
    for size, p in sorted(big, reverse=True):
        if size > 10 * 1024 * 1024:
            warnings.append('%s: %.1f MB' % (p, size / 1048576))

    with open(os.path.join(dest, '.export', 'files.txt'), 'w', newline='\n') as f:
        for p in files:
            fp = os.path.join(dest, *p.split('/'))
            f.write('%s %d %s\n' % (sha256(fp), os.path.getsize(fp), p))
    with open(os.path.join(dest, '.export', 'submodules.txt'), 'w', newline='\n') as f:
        for p, sha, url in subs:
            f.write('%s %s %s\n' % (sha, p, url))
    with open(os.path.join(dest, '.export', 'warnings.txt'), 'w', newline='\n') as f:
        f.write(''.join(w + '\n' for w in warnings))

    print('export -> %s' % dest)
    print('  %d files, %.1f MB (%d copied or updated, %d removed), %d submodules as gitlinks' %
          (len(files), total / 1048576, copied, removed, len(subs)))
    for w in warnings:
        print('  warning: ' + w)


if __name__ == '__main__':
    main()
