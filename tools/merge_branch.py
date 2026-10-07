"""Merge an upstream branch into a fork's *uncommitted* working tree, file by file (no commits, no index changes).

    python tools/merge_branch.py <repo dir> <ref> [--base <commit>] [--exclude <glob> ...] [--dry]

For every path that differs between <base> (default: HEAD) and <ref>:
  added    -> written from <ref> (reported as a conflict if a different file already exists)
  modified -> if our file is still <base>'s, take <ref>'s; otherwise a 3-way `git merge-file` (ours, base, theirs),
              leaving conflict markers where both changed the same lines
  deleted  -> removed only if our file is still <base>'s, otherwise reported
Line endings follow our file (or CRLF for new files when core.autocrlf is true), so a CRLF working copy does not
conflict on every line with LF blobs. Prints a summary; exit code 1 if any conflicts are left.
"""
import fnmatch
import os
import subprocess
import sys


def git(repo, *args, binary=False):
    r = subprocess.run(['git', '-C', repo] + list(args), capture_output=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.decode(errors='replace'))
    return r.stdout if binary else r.stdout.decode('utf-8', 'surrogateescape')


def blob(repo, ref, path):
    try:
        return git(repo, 'show', '%s:%s' % (ref, path), binary=True)
    except RuntimeError:
        return None


def to_eol(data, crlf):
    data = data.replace(b'\r\n', b'\n')
    return data.replace(b'\n', b'\r\n') if crlf else data


def main():
    a = sys.argv[1:]
    repo, ref = a[0], a[1]
    base, excludes, dry = 'HEAD', [], '--dry' in a
    for i, x in enumerate(a):
        if x == '--base':
            base = a[i + 1]
        if x == '--exclude':
            excludes.append(a[i + 1])
    try:
        autocrlf = git(repo, 'config', '--get', 'core.autocrlf').strip() == 'true'
    except RuntimeError:
        autocrlf = False
    status = git(repo, 'diff', '--name-status', '--no-renames', base, ref).splitlines()
    tmp = os.path.join(repo, '.git', 'merge_branch_tmp')
    os.makedirs(tmp, exist_ok=True)
    taken, merged, conflicts, skipped = [], [], [], []
    for line in status:
        st, path = line.split('\t', 1)
        if any(fnmatch.fnmatch(path, e) for e in excludes):
            skipped.append(path)
            continue
        full = os.path.join(repo, path)
        ours = open(full, 'rb').read() if os.path.isfile(full) else None
        b, t = blob(repo, base, path), blob(repo, ref, path)
        crlf = (b'\r\n' in ours) if ours is not None else autocrlf
        binary = t is not None and b'\0' in t[:8000]
        if st == 'A':
            if ours is not None and to_eol(ours, False) != to_eol(t, False):
                conflicts.append(path + ' (added upstream, a different file exists here)')
                continue
            data = t if binary else to_eol(t, crlf)
            if not dry:
                os.makedirs(os.path.dirname(full) or '.', exist_ok=True)
                open(full, 'wb').write(data)
            taken.append(path)
        elif st == 'D':
            if ours is None:
                continue
            if to_eol(ours, False) == to_eol(b, False):
                if not dry:
                    os.remove(full)
                taken.append(path + ' (deleted)')
            else:
                conflicts.append(path + ' (deleted upstream, changed here: kept)')
        else:  # M
            if ours is None:
                conflicts.append(path + ' (changed upstream, missing here)')
                continue
            if binary or to_eol(ours, False) == to_eol(b, False):
                if not dry:
                    open(full, 'wb').write(t if binary else to_eol(t, crlf))
                taken.append(path)
                continue
            fo, fb, ft = (os.path.join(tmp, n) for n in ('ours', 'base', 'theirs'))
            open(fo, 'wb').write(to_eol(ours, False))
            open(fb, 'wb').write(to_eol(b, False))
            open(ft, 'wb').write(to_eol(t, False))
            r = subprocess.run(['git', 'merge-file', '-L', 'ours', '-L', 'base', '-L', ref, fo, fb, ft], capture_output=True)
            res = open(fo, 'rb').read()
            if not dry:
                open(full, 'wb').write(to_eol(res, crlf))
            (conflicts if r.returncode != 0 else merged).append(path + (' (%d conflicts)' % r.returncode if r.returncode > 0 else ''))
    for name, lst in (('taken from ' + ref, taken), ('merged', merged), ('excluded', skipped), ('CONFLICTS', conflicts)):
        if lst:
            print('%s: %d' % (name, len(lst)))
            for p in lst:
                print('   ', p)
    sys.exit(1 if conflicts else 0)


main()
