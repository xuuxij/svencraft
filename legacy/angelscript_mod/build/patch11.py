import os
p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\sc_props.as"
s = open(p, newline='').read()
reps = [
    ("		self.pev.solid = m_bSolid ? SOLID_BBOX : SOLID_NOT;",
     "		// The engine may consume the \"solid\" key itself (it's an entvars field), so honour both.\n		self.pev.solid = ( m_bSolid || self.pev.solid == SOLID_BBOX ) ? SOLID_BBOX : SOLID_NOT;"),
    ("	bool bEarth = iMain == MAT_COBBLE || iMain == MAT_STONE || iMain == MAT_SAND || iMain == MAT_DIRT",
     "	bool bEarth = iMain == MAT_COBBLE || iMain == MAT_STONE || iMain == MAT_DIRT"),
]
for a, b in reps:
    assert a in s, a[:60]
    s = s.replace(a, b)
open(p, 'w', newline='').write(s)
print('ok')
