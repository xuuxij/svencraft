import os
p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\sc_props.as"
s = open(p, newline='').read()
old = '''		if( pEnt.pev.solid != SOLID_NOT && pEnt.pev.solid != SOLID_TRIGGER )
			continue;   // solid things are found by the normal trace
		string szClass = pEnt.GetClassname();
		if( !SC_InList( SC_PROP_CLASSES, szClass ) && szClass != "func_illusionary" )
			continue;'''
new = '''		// Prop models often have no collision box (or a zero-size one), so test them all against their
		// model bounds; anything solid in front is still caught by the normal trace distance.
		string szClass = pEnt.GetClassname();
		if( !SC_InList( SC_PROP_CLASSES, szClass ) && szClass != "func_illusionary" )
			continue;'''
assert old in s
s = s.replace(old, new)
s = s.replace('// Picking non-solid decorations (a normal trace passes straight through them)', '// Picking decorations a normal trace passes through (non-solid, or solid with no collision box)')
open(p, 'w', newline='').write(s)
print('ok')
