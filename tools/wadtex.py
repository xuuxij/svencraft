# WAD3 texture access: list, decode to RGB, contact sheets.   python wadtex.py sheet out.png NAME1 NAME2 ...
import struct, os, sys
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sc_paths   # project paths (tools/sc_paths.py; env override SVENCRAFT_SVEN)
SVEN = sc_paths.SVEN
WADS = ['halflife.wad', 'Opfor.wad', 'tfc.WAD', 'tfc2.wad', 'bridge3.wad', 'cs_bdog.wad', 'snd.wad', 'nw.wad', 'op4ctf.wad',
        'deathmission.wad', 'barney.wad', 'decay.wad', 'sandstone.wad', 'neilm4.wad', 'neilm5.wad', 'scrpg2.wad', 'liquids.wad',
        'neilm2.wad']
_index = None


def index():
    global _index
    if _index is None:
        _index = {}
        for w in WADS:
            p = os.path.join(SVEN, w)
            if not os.path.exists(p):
                continue
            d = open(p, 'rb').read()
            n, off = struct.unpack_from('<ii', d, 4)
            for i in range(n):
                fo, dsz, sz, typ, cmp = struct.unpack_from('<iiibb', d, off + 32 * i)
                name = d[off + 32 * i + 16: off + 32 * i + 32].split(b'\0')[0].decode('latin-1')
                _index.setdefault(name.upper(), (w, p, fo))
    return _index


def load(name):
    w, p, fo = index()[name.upper()]
    d = open(p, 'rb').read()
    nm, wdt, hgt, o0, o1, o2, o3 = struct.unpack_from('<16sII4I', d, fo)
    pix = d[fo + o0: fo + o0 + wdt * hgt]
    palo = fo + o3 + (wdt // 8) * (hgt // 8) + 2
    pal = d[palo: palo + 768]
    im = Image.frombytes('P', (wdt, hgt), pix)
    im.putpalette(pal)
    return im.convert('RGB'), w


def sheet(out, names, cell=128):
    cols = 6
    rows = (len(names) + cols - 1) // cols
    img = Image.new('RGB', (cols * (cell + 8), rows * (cell + 22)), (30, 30, 30))
    dr = ImageDraw.Draw(img)
    for i, n in enumerate(names):
        r, c = divmod(i, cols)
        x, y = c * (cell + 8), r * (cell + 22)
        try:
            im, w = load(n)
            img.paste(im.resize((cell, cell)), (x, y))
            dr.text((x, y + cell + 2), '%s %dx%d' % (n, *im.size), fill=(255, 255, 255))
        except KeyError:
            dr.text((x, y + 40), 'missing ' + n, fill=(255, 80, 80))
    img.save(out)


if __name__ == '__main__':
    if sys.argv[1] == 'sheet':
        sheet(sys.argv[2], sys.argv[3:])


def raw_lump(name):
    """the texture's miptex lump bytes, exactly as stored in its WAD"""
    w, p, fo = index()[name.upper()]
    d = open(p, 'rb').read()
    n, off = struct.unpack_from('<ii', d, 4)
    for i in range(n):
        lfo, dsz, sz, typ, cmp = struct.unpack_from('<iiibb', d, off + 32 * i)
        if lfo == fo:
            return d[fo: fo + dsz]
    raise KeyError(name)


def write_raw_wad(path, names):
    """a WAD3 with the named textures copied byte for byte from Sven Co-op's WADs"""
    blob, dirents, pos = b'', b'', 12
    for name in names:
        body = raw_lump(name)
        dirents += struct.pack('<iiibbh16s', pos, len(body), len(body), 0x43, 0, 0, name.encode().ljust(16, b'\0'))
        blob += body
        pos += len(body)
    open(path, 'wb').write(b'WAD3' + struct.pack('<ii', len(names), pos) + blob + dirents)
