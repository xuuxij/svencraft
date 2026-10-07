# Text patching that copes with CRLF files: patterns are written with \n and matched either way.
def _read(path):
    raw = open(path, 'rb').read().decode('utf-8', 'surrogateescape')
    return raw.replace('\r\n', '\n'), '\r\n' in raw


def _write(path, s, crlf):
    if crlf:
        s = s.replace('\n', '\r\n')
    open(path, 'wb').write(s.encode('utf-8', 'surrogateescape'))


def add_include(s, include):
    """insert after the last top-level #include (outside #if blocks) among the file's first 80 lines"""
    if include in s:
        return s
    lines = s.split('\n')
    depth, last = 0, None
    for i, l in enumerate(lines[:80]):
        t = l.strip()
        if t.startswith('#if'):
            depth += 1
        elif t.startswith('#endif'):
            depth -= 1
        elif t.startswith('#include') and depth == 0:
            last = i
    lines.insert(last + 1, include)
    return '\n'.join(lines)


def patch(path, pairs, include=None):
    s, crlf = _read(path)
    for old, new in pairs:
        if new in s and (old not in s or old in new):
            continue   # already applied (also when the new text keeps the old, as an insertion does)
        assert s.count(old) == 1, (path, old[:70], s.count(old))
        s = s.replace(old, new)
    if include:
        s = add_include(s, include)
    _write(path, s, crlf)
    print('patched', path)


def move_include(path, include):
    """remove a misplaced include and put it after the other includes"""
    s, crlf = _read(path)
    s = s.replace(include + '\n', '', 1)
    _write(path, add_include(s, include), crlf)
    print('moved include in', path)
