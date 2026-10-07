"""Audit the publishing staging folder for personal data and things that must not be published.

    python tools/publish/audit.py [DEST]          (DEST defaults to <project>/../Svencraft-publish)

Fails (exit 1) on any hit that tools/publish/audit-allow.txt does not allow:
  needle      personal strings, matched case-insensitively in text files and in the strings of binary files.
              Derived on this machine at run time, so this script holds none of them: the Windows user name,
              computer name and profile path, the global git user.email (and its local part), the local time
              zone's names and UTC offsets; plus one string per line from tools/publish/audit-needles.local.txt
              (kept out of the repo: real name, account ids, ...). A missing local file is a warning.
  user-path, appdata, claude-path, claude-link, lan-ip, mac, secret, email
              generic patterns (personal folders, assistant session folders and links, LAN addresses, MAC
              addresses, API tokens and private keys, e-mail addresses in our own files)
  image-meta  PNG text/time/EXIF chunks, JPEG EXIF/XMP/IPTC/comment segments
  file-type   game content or build products (.mdl .spr .wad .bsp .wav .dll .exe .pk3 ...) and files the engine
              writes at run time (config.cfg, .xash_id, ...); allowed only where audit-allow.txt says
  size        files over 50 MB (warning over 20 MB)
  git         when DEST is a git repo: every commit's author and committer must be the noreply identity with UTC
              (+0000) dates, commit messages are scanned like text, .gitmodules URLs must be public https URLs
"""
import fnmatch
import getpass
import os
import re
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
IDENTITY = ('xuuxij', '20761916+xuuxij@users.noreply.github.com')
MAX_MB, WARN_MB = 50, 20

PATTERNS = {
    'user-path': r'[a-z]:[\\/]{1,2}users[\\/]{1,2}(?!public\b|default\b|username\b|you\b|<)[^\\/\s"\'<>]+'
                 r'|/c/users/(?!username\b|you\b|<)[^/\s"\']+|/home/(?!username\b|user\b|you\b|<)[a-z0-9_.-]+/',
    'appdata': r'appdata[\\/]',
    'claude-path': r'\.claude[\\/]|C--WINDOWS|claude[\\/]projects',
    'claude-link': r'claude\.ai/(?:code|chat|share|project)',
    'lan-ip': r'(?<![\d.])(?:192\.168|10\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01]))\.\d{1,3}\.\d{1,3}(?![\d.])',
    'mac': r'(?<![0-9a-f:-])[0-9a-f]{2}(?:[:-][0-9a-f]{2}){5}(?![0-9a-f:-])',
    'secret': r'gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{20,}|sk-ant-[A-Za-z0-9_-]{10,}|AKIA[0-9A-Z]{16}'
              r'|-----BEGIN [A-Z ]*PRIVATE KEY-----|xox[abprs]-[A-Za-z0-9-]{10,}',
}
EMAIL = r'[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}'
# our own files inside the upstream clones (the generic e-mail check covers these and everything outside engine/ game/)
OURS_IN_CLONES = ['game/dlls/svencraft/*', 'game/cl_dll/svencraft/*', 'game/common/sc_*', 'game/common/voxel_api.h',
                  'game/common/dynworld_api.h', 'engine/common/voxel_api.h', 'engine/common/dynworld_api.h',
                  'engine/engine/common/voxel.*', 'engine/engine/common/dynworld.*', 'engine/ref/gl/gl_voxel.c',
                  'engine/ref/gl/gl_dynworld.c']
BAD_EXT = {'.mdl', '.spr', '.wad', '.bsp', '.dyn', '.wav', '.ogg', '.mp3', '.dll', '.exe', '.pdb', '.lib', '.obj', '.exp',
           '.7z', '.zip', '.pk3', '.pak', '.dem', '.sav', '.nod', '.nrp'}
RUNTIME_NAMES = {'config.cfg', 'video.cfg', 'opengl.cfg', 'vfs.cfg', '.xash_id', 'voice_ban.dt', 'engine.log',
                 'cdaudio.txt', 'userconfig.cfg'}
TEXT_EXT = {'.c', '.h', '.cpp', '.hpp', '.cc', '.py', '.sh', '.ps1', '.bat', '.cmd', '.md', '.txt', '.cfg', '.qc', '.smd',
            '.as', '.cs', '.json', '.yml', '.yaml', '.xml', '.html', '.css', '.js', '.in', '.mk', '.cmake', '.def', '.rc',
            '.gitignore', '.gitattributes', '.gitmodules', '.lst', '.inc', '.m', '.mm', '.java', '.kt', '.gradle', '.pro'}


def local_needles(warnings):
    n = set()
    for v in (os.environ.get('USERNAME'), os.environ.get('USER'), getpass.getuser(), os.environ.get('COMPUTERNAME'),
              socket.gethostname()):
        if v and len(v) >= 3:
            n.add(v)
    prof = os.environ.get('USERPROFILE') or os.path.expanduser('~')
    if prof:
        n.add(prof)
        n.add(prof.replace('\\', '/'))
        if len(prof) > 2 and prof[1] == ':':
            n.add('/' + prof[0].lower() + prof[2:].replace('\\', '/'))
    r = subprocess.run(['git', 'config', '--global', 'user.email'], capture_output=True, text=True)
    email = r.stdout.strip()
    if email and not email.endswith('@users.noreply.github.com'):
        n.add(email)
        local = email.split('@')[0]
        if len(local) >= 5:
            n.add(local)
    r = subprocess.run(['git', 'config', '--global', 'user.name'], capture_output=True, text=True)
    if r.stdout.strip() and r.stdout.strip() != IDENTITY[0]:
        n.add(r.stdout.strip())
    # the local time zone: its names and UTC offsets (standard and daylight)
    for name in time.tzname:
        if name and len(name) > 4:
            n.add(name)
    for secs in {time.timezone, time.altzone}:
        if secs:
            off = -secs
            sign = '+' if off >= 0 else '-'
            hh, mm = divmod(abs(off) // 60, 60)
            n.add('%s%02d%02d' % (sign, hh, mm))
            n.add('%s%02d:%02d' % (sign, hh, mm))
    path = os.path.join(HERE, 'audit-needles.local.txt')
    if os.path.isfile(path):
        for line in open(path, encoding='utf-8'):
            line = line.strip()
            if line and not line.startswith('#'):
                n.add(line)
    else:
        warnings.append('no %s: only the needles derived on this machine are checked' % os.path.relpath(path, ROOT))
    return sorted(n, key=len, reverse=True)


def read_allow():
    allow = []
    path = os.path.join(HERE, 'audit-allow.txt')
    if os.path.isfile(path):
        for line in open(path, encoding='utf-8'):
            line = line.split('#', 1)[0].strip()
            if line:
                check, glob = line.split(None, 1)
                allow.append((check, glob.strip()))
    return allow


def allowed(allow, check, rel):
    return any((c == check or c == '*') and fnmatch.fnmatchcase(rel, g) for c, g in allow)


def binary_strings(data):
    """printable ASCII runs and UTF-16LE runs of 4+ characters"""
    out = [m.group().decode('ascii') for m in re.finditer(rb'[\x20-\x7e]{4,}', data)]
    out += [m.group().decode('utf-16le') for m in re.finditer(rb'(?:[\x20-\x7e]\x00){4,}', data)]
    return '\n'.join(out)


def image_meta(data):
    found = []
    if data[:8] == b'\x89PNG\r\n\x1a\n':
        i = 8
        while i + 8 <= len(data):
            ln = int.from_bytes(data[i:i + 4], 'big')
            typ = data[i + 4:i + 8].decode('latin-1')
            if typ in ('tEXt', 'zTXt', 'iTXt', 'eXIf', 'tIME'):
                found.append('PNG %s chunk: %r' % (typ, data[i + 8:i + 8 + min(ln, 60)]))
            i += 12 + ln
            if typ == 'IEND':
                break
    elif data[:2] == b'\xff\xd8':
        i = 2
        while i + 4 <= len(data) and data[i] == 0xFF:
            marker = data[i + 1]
            if marker in (0xD9, 0xDA):
                break
            ln = int.from_bytes(data[i + 2:i + 4], 'big')
            if marker == 0xE1 or marker == 0xED or marker == 0xFE:
                found.append('JPEG segment 0x%02X: %r' % (marker, data[i + 4:i + 4 + min(ln - 2, 40)]))
            i += 2 + ln
    return found


def is_text(rel, data):
    ext = os.path.splitext(rel)[1].lower()
    base = os.path.basename(rel)
    if b'\0' in data[:8192]:
        return False
    return ext in TEXT_EXT or base in ('wscript', 'Makefile', 'waf', 'LICENSE', 'COPYING', 'README', 'CMakeLists.txt') or \
        not ext or all(32 <= b < 127 or b in (9, 10, 13) for b in data[:4096])


def main():
    dest = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(ROOT), 'Svencraft-publish'))
    if not os.path.isdir(dest):
        raise SystemExit('no staging folder at %s (run tools/publish/export.sh first)' % dest)
    warnings, fails = [], []
    needles = local_needles(warnings)
    # a needle of only letters (a name) or only digits (an account id) must stand alone, so "colinear" or a longer
    # number does not count; anything else matches as a plain substring
    def needle_re(n):
        if n.isalpha():
            return r'(?<![A-Za-z])%s(?![A-Za-z])' % re.escape(n)
        if n.isdigit():
            return r'(?<!\d)%s(?!\d)' % re.escape(n)
        return re.escape(n)
    needle_rx = re.compile('|'.join(needle_re(n) for n in needles), re.I) if needles else None
    pats = {k: re.compile(v, re.I) for k, v in PATTERNS.items()}
    email_rx = re.compile(EMAIL)
    allow = read_allow()

    def hit(check, rel, where, text):
        if allowed(allow, check, rel):
            return
        fails.append('%-11s %s%s  %s' % (check, rel, where, text))

    def scan_text(rel, text, check_email):
        checks = [('needle', needle_rx)] if needle_rx else []
        checks += list(pats.items())
        if check_email:
            checks.append(('email', email_rx))
        for name, rx in checks:
            for m in rx.finditer(text):
                if name == 'email' and m.group().lower().endswith(('@users.noreply.github.com', '@example.com', '@noreply.anthropic.com')):
                    continue
                line = text.count('\n', 0, m.start()) + 1
                s = text.rfind('\n', 0, m.start()) + 1
                e = text.find('\n', m.end())
                snippet = text[s:e if e >= 0 else len(text)].strip()
                hit(name, rel, ':%d' % line, snippet[:160])

    nfiles = nbytes = 0
    for dirpath, dirnames, filenames in os.walk(dest):
        rel_dir = os.path.relpath(dirpath, dest).replace(os.sep, '/')
        if rel_dir.split('/')[0] in ('.git', '.export'):
            dirnames[:] = []
            continue
        for f in filenames:
            rel = f if rel_dir == '.' else rel_dir + '/' + f
            path = os.path.join(dirpath, f)
            size = os.path.getsize(path)
            nfiles += 1
            nbytes += size
            if size > MAX_MB * 1048576:
                hit('size', rel, '', '%.1f MB (limit %d MB)' % (size / 1048576, MAX_MB))
            elif size > WARN_MB * 1048576:
                warnings.append('%s: %.1f MB' % (rel, size / 1048576))
            ext = os.path.splitext(f)[1].lower()
            if ext in BAD_EXT or f.lower() in RUNTIME_NAMES:
                hit('file-type', rel, '', 'game content, build product or runtime file')
            data = open(path, 'rb').read()
            for meta in image_meta(data):
                hit('image-meta', rel, '', meta)
            ours = not rel.startswith(('engine/', 'game/')) or any(fnmatch.fnmatchcase(rel, g) for g in OURS_IN_CLONES)
            if is_text(rel, data):
                scan_text(rel, data.decode('utf-8', 'replace'), ours)
            else:
                scan_text(rel + ' (binary strings)', binary_strings(data), False)

    # .gitmodules: public https URLs only
    gm = os.path.join(dest, '.gitmodules')
    if os.path.isfile(gm):
        for line in open(gm, encoding='utf-8'):
            if line.strip().startswith('url') and not re.match(r'\s*url = https://(github\.com|gitlab\.com)/', line):
                hit('git', '.gitmodules', '', 'non-public submodule URL: ' + line.strip())

    # the staging repo's history, when there is one
    if os.path.isdir(os.path.join(dest, '.git')):
        r = subprocess.run(['git', '-C', dest, 'log', '--all', '--format=%H%x1f%an%x1f%ae%x1f%ad%x1f%cn%x1f%ce%x1f%cd%x1f%B%x1e',
                            '--date=raw'], capture_output=True)
        if r.returncode == 0:
            for rec in r.stdout.decode('utf-8', 'replace').split('\x1e'):
                rec = rec.strip()
                if not rec:
                    continue
                h, an, ae, ad, cn, ce, cd, body = rec.split('\x1f')
                if (an, ae) != IDENTITY or (cn, ce) != IDENTITY:
                    hit('git', 'commit ' + h[:10], '', 'identity %s <%s> / %s <%s>' % (an, ae, cn, ce))
                for d in (ad, cd):
                    if not d.endswith(' +0000'):
                        hit('git', 'commit ' + h[:10], '', 'date not UTC: ' + d)
                scan_text('commit %s message' % h[:10], body, True)
        for key in ('user.name', 'user.email'):
            v = subprocess.run(['git', '-C', dest, 'config', '--local', key], capture_output=True, text=True).stdout.strip()
            if v and v not in IDENTITY:
                hit('git', '.git/config', '', '%s = %s' % (key, v))

    print('audit %s: %d files, %.1f MB, %d needles + %d patterns' % (dest, nfiles, nbytes / 1048576, len(needles), len(pats) + 1))
    for w in warnings:
        print('  warning: ' + w)
    for f in fails:
        print('  FAIL ' + f)
    print('RESULT: %s (%d hits)' % ('FAIL' if fails else 'PASS', len(fails)))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
