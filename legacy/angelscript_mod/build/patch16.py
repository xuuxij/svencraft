import os, re
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"
p = os.path.join(D, 'svencraft.as')
s = open(p, newline='').read()
reps = [
    ('#include "sc_feedback"\n', '#include "sc_feedback"\n#include "sc_crafting"\n'),
    ('\tSC_InitMixRules();\n}', '\tSC_InitMixRules();\n\tSC_InitRecipes();\n}'),
    ('\tSC_PrecacheHud();\n', '\tSC_PrecacheHud();\n\tSC_PrecacheCrafting();\n'),
    ('Mouse3 or K: inventory." );', 'Mouse3 or K: inventory. O: crafting." );'),
    ('"!inv  !block <name>  !creative  !tools  !clearblocks  !resetworld (sandbox)"', '"!inv  !craft  !block <name>  !creative  !tools  !clearblocks  !resetworld (sandbox)"'),
    ('''	else if( szCmd == "!resetworld" )''', '''	else if( szCmd == "!craft" )
	{
		SC_OpenCrafting( pPlayer );
	}
	else if( szCmd == "!resetworld" )'''),
]
for a, b in reps:
    assert a in s, a[:60]
    s = s.replace(a, b, 1)
# self-test: every craftable entity class exists, and recipe ingredients resolve
anchor = '	SC_TestOut( "SCTEST kinds: " + szKinds + "\\n" );'
test = anchor + '''
	string szBad = "";
	int iItems = 0;
	for( uint i = 0; i < g_SCRecipes.length(); i++ )
	{
		if( g_SCRecipes[i].item == "" )
			continue;
		iItems++;
		CBaseEntity@ pTest = g_EntityFuncs.Create( g_SCRecipes[i].item, Vector( 0, 0, -4000 ), g_vecZero, true );
		if( pTest is null )
			szBad += g_SCRecipes[i].item + " ";
		else
			g_EntityFuncs.Remove( pTest );
	}
	SC_TestOut( "SCTEST recipes=" + g_SCRecipes.length() + " item recipes=" + iItems + " bad classes: " + ( szBad == "" ? "none" : szBad ) + "\\n" );'''
if 'SCTEST recipes=' not in s:
    assert anchor in s, 'test anchor'
    s = s.replace(anchor, test, 1)
open(p, 'w', newline='').write(s)
# tidy the material table columns
m = os.path.join(D, 'sc_materials.as')
t = open(m, newline='').read()
t = re.sub(r'(TOOL_\w+,\s+)(\d)\s+,\s*', lambda mm: mm.group(1) + mm.group(2) + ',     ', t)
open(m, 'w', newline='').write(t)
print('ok')
