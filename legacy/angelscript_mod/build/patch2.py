p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
s = s.replace('''		int c = 0;
		kinds.get( key, c );''', '''		int64 c = 0;
		kinds.get( key, c );''')
s = s.replace('''		int c = 0;
		kinds.get( keys[i], c );''', '''		int64 c = 0;
		kinds.get( keys[i], c );''')
anchor = '''	SC_TestOut( "SCTEST item_generic models=" + seen.getSize() + ": " + szMix + "\\n" );'''
probe = anchor + '''

	array<string> probeModels = { "models/snd/palm1.mdl", "models/snd/dunes_straight_l2.mdl", "models/snd/awningm.mdl", "models/snd/tubelight1.mdl" };
	for( uint i = 0; i < probeModels.length(); i++ )
	{
		CBaseEntity@ pProbe = g_EntityFuncs.Create( "info_target", g_vecZero, g_vecZero, true );
		g_EntityFuncs.SetModel( pProbe, probeModels[i] );
		SC_TestOut( "SCTEST setmodel bounds " + probeModels[i] + ": mins " + pProbe.pev.mins.x + "," + pProbe.pev.mins.y + "," + pProbe.pev.mins.z + " maxs " + pProbe.pev.maxs.x + "," + pProbe.pev.maxs.y + "," + pProbe.pev.maxs.z + "\\n" );
		g_EntityFuncs.Remove( pProbe );
	}'''
if 'probeModels' not in s:
    assert anchor in s, 'anchor not found'
    s = s.replace(anchor, probe)
open(p, 'w', newline='').write(s)
print('ok', 'int64 c' in s, 'probeModels' in s)
