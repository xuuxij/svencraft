p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
start = s.index('	array<string> probeModels = {')
end = s.index('	}', s.index('g_EntityFuncs.Remove( pProbe );')) + 3
s = s[:start].rstrip('\n') + '\n' + s[end:]
open(p, 'w', newline='').write(s)
print('probeModels' in s)
