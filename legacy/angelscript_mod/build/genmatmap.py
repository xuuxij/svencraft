import re, sys
src = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop\sound\materials.txt"
out = sys.argv[1]
names, chars, seen = [], [], set()
for line in open(src, encoding='latin-1'):
    line = line.split('//')[0].strip()
    m = re.match(r'^([A-Za-z])\s+(\S+)', line)
    if not m: continue
    c, n = m.group(1).upper(), m.group(2).upper()[:12]
    if n in seen: continue
    seen.add(n); names.append(n); chars.append(c)
with open(out, 'w', newline='\n') as f:
    f.write('// Generated from sound/materials.txt (texture name prefix -> Half-Life material char). Do not edit.\n')
    f.write('const string SC_TEXCHARS = "%s";\n' % ''.join(chars))
    f.write('const array<string> SC_TEXNAMES = {\n')
    for i in range(0, len(names), 8):
        f.write('\t' + ', '.join('"%s"' % n.replace('"', '') for n in names[i:i+8]) + ',\n')
    f.write('};\n')
print(len(names), 'texture entries')
