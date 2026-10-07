p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='', encoding='utf-8').read()
old = '''" pieces=" + g_SCBlockCount + " doors=" + iDoors + " open=" + iOpen + "\\n" );
	SC_TestFlush();
}
'''
new = '''" pieces=" + g_SCBlockCount + " doors=" + iDoors + " open=" + iOpen + "\\n" );
	SC_TestFlush();
	if( SC_FlagFile( "sc_upgradetest.txt" ) )
	{
		SC_ClearWorldEntities();
		g_Scheduler.SetTimeout( "SC_SelfTestUpgrade", 0.3 );
	}
}

void SC_ClearWorldEntities()
{
	array<string> classes = { "sc_node", "sc_block" };
	for( uint c = 0; c < classes.length(); c++ )
	{
		CBaseEntity@ pEnt = null;
		while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, classes[c] ) ) !is null )
			g_EntityFuncs.Remove( pEnt );
	}
	g_SCBlockCount = 0;
	SC_ClearOres();
	SC_ClearCaves();
}

// Loads a pre-cave (v1) save and checks caves were added: entrance open, player blocks intact.
void SC_SelfTestUpgrade()
{
	bool bOk = SC_LoadWorld( "scripts/plugins/store/sc_upgradetest.txt" );
	int x0 = SC_CAVE_ENTRANCE_X, y0 = SC_CAVE_ENTRANCE_Y;
	int iPlaced = 0;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_node" ) ) !is null )
	{
		if( !SC_IsTerrainMat( pEnt.pev.iuser1 ) && pEnt.pev.iuser1 != MAT_LOG && pEnt.pev.iuser1 != MAT_LEAVES ) iPlaced++;
	}
	string s2 = SC_WorldSnapshot();
	int iCaveLines = 0;
	array<string> lines = SC_Split( s2, "\\n" );
	for( uint i = 0; i < lines.length(); i++ ) { if( lines[i].StartsWith( "cave " ) ) iCaveLines++; }
	SC_TestOut( "SCTEST upgrade v1 save: loaded=" + bOk + " stairs " + SC_DescribeCell( x0 + 1, y0, -1 ) + " tunnel " + SC_DescribeCell( x0 + 10, y0, -9 )
		+ " floor " + SC_DescribeCell( x0 + 10, y0, -10 ) + " | non-terrain nodes kept=" + iPlaced + " | snapshot header " + lines[0] + ", hidden cave cells " + iCaveLines + " pieces=" + g_SCBlockCount + "\\n" );
	SC_TestFlush();
}
'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(p, 'w', newline='', encoding='utf-8').write(s)
print('ok')
