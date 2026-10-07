import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"
p = os.path.join(D, 'svencraft.as')
s = open(p, newline='', encoding='utf-8').read()
old = '''		SC_SelfTestFallCheck();
		g_PlayerFuncs.BotDisconnect( pBot );
	}
	SC_TestFlush();
}
'''
new = '''		SC_SelfTestFallCheck();
		SC_SelfTestDoors( pBot );
		g_PlayerFuncs.BotDisconnect( pBot );
	}
	SC_TestFlush();
	SC_SelfTestSave();
}

CBaseEntity@ SC_TestFind( int iMat )
{
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_block" ) ) !is null )
	{
		if( pEnt.pev.iuser1 == iMat && ( pEnt.pev.flags & FL_KILLME ) == 0 )
			return pEnt;
	}
	return null;
}

string SC_TestBox( CBaseEntity@ p )
{
	Vector mn = p.pev.origin + p.pev.mins, mx = p.pev.origin + p.pev.maxs;
	return "(" + mn.x + " " + mn.y + " " + mn.z + ")-(" + mx.x + " " + mx.y + " " + mx.z + ") yaw=" + p.pev.angles.y;
}

void SC_SelfTestDoors( CBasePlayer@ pBot )
{
	pBot.pev.v_angle = Vector( 0, 90, 0 );
	g_EntityFuncs.SetOrigin( pBot, Vector( 220, 400, 37 ) );
	bool bDoor = SC_PlaceDoor( pBot, Vector( 220, 460, 20 ) );
	CBaseEntity@ pDoor = SC_TestFind( MAT_DOOR );
	SC_TestOut( "SCTEST door placed=" + bDoor + " found=" + ( pDoor !is null ) + "\\n" );
	if( pDoor is null )
		return;
	SC_TestOut( "SCTEST door closed box " + SC_TestBox( pDoor ) + " (want x 200..240 y 440..446 z 0..80)\\n" );
	SC_ToggleDoor( pDoor );
	SC_TestOut( "SCTEST door open box " + SC_TestBox( pDoor ) + " open=" + pDoor.pev.iuser3 + " (want x 200..206 y 440..480)\\n" );
	g_EntityFuncs.SetOrigin( pBot, Vector( 225, 430, 37 ) );   // stands where the closed panel goes
	SC_ToggleDoor( pDoor );
	SC_TestOut( "SCTEST door close while blocked: open=" + pDoor.pev.iuser3 + " (want 1)\\n" );
	g_EntityFuncs.SetOrigin( pBot, Vector( 220, 380, 37 ) );
	SC_ToggleDoor( pDoor );
	SC_TestOut( "SCTEST door close when clear: open=" + pDoor.pev.iuser3 + " (want 0)\\n" );

	bool bPane = SC_PlacePane( pBot, Vector( 300, 460, 20 ) );
	CBaseEntity@ pPane = SC_TestFind( MAT_GLASS_PANE );
	SC_TestOut( "SCTEST pane placed=" + bPane + ( pPane !is null ? " box " + SC_TestBox( pPane ) + " body=" + pPane.pev.body + " skin=" + pPane.pev.skin : "" ) + " (want y 458..462)\\n" );

	// a second door left open, for the save round trip
	pBot.pev.v_angle = Vector( 0, 0, 0 );
	SC_PlaceDoor( pBot, Vector( 340, 380, 20 ) );

	int iBefore = SC_CountTestDrops( MAT_DOOR );
	int iSwings = 0;
	while( iSwings < 50 && ( pDoor.pev.flags & FL_KILLME ) == 0 )
	{
		SC_HitBlock( pBot, pDoor, TOOL_AXE, Vector( 0, -1, 0 ) );
		iSwings++;
	}
	SC_TestOut( "SCTEST door mined in " + iSwings + " axe swings, door drops " + iBefore + " -> " + SC_CountTestDrops( MAT_DOOR ) + "\\n" );
	CBaseEntity@ pDoor2 = null;
	while( ( @pDoor2 = g_EntityFuncs.FindEntityByClassname( pDoor2, "sc_block" ) ) !is null )
	{
		if( SC_IsDoor( pDoor2 ) && ( pDoor2.pev.flags & FL_KILLME ) == 0 )
		{
			SC_ToggleDoor( pDoor2 );
			SC_TestOut( "SCTEST second door " + SC_TestBox( pDoor2 ) + " open=" + pDoor2.pev.iuser3 + "\\n" );
		}
	}
}

string g_SCTestSnap;
void SC_SelfTestSave()
{
	g_SCTestSnap = SC_WorldSnapshot();
	SC_WriteFile( "scripts/plugins/store/sc_savetest.txt", g_SCTestSnap );
	array<string> classes = { "sc_node", "sc_block" };
	for( uint c = 0; c < classes.length(); c++ )
	{
		CBaseEntity@ pEnt = null;
		while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, classes[c] ) ) !is null )
			g_EntityFuncs.Remove( pEnt );
	}
	g_SCBlockCount = 0;
	SC_ClearOres();
	g_Scheduler.SetTimeout( "SC_SelfTestSave2", 0.3 );
}

void SC_SelfTestSave2()
{
	bool bOk = SC_LoadWorld( "scripts/plugins/store/sc_savetest.txt" );
	string s2 = SC_WorldSnapshot();
	array<string> a = SC_Split( g_SCTestSnap, "\\n" ), b = SC_Split( s2, "\\n" );
	a.sortAsc(); b.sortAsc();
	int iDiff = 0;
	string szFirst = "";
	for( uint i = 0; i < a.length() || i < b.length(); i++ )
	{
		string x = i < a.length() ? a[i] : "<none>", y = i < b.length() ? b[i] : "<none>";
		if( x != y ) { iDiff++; if( szFirst == "" ) szFirst = x + " | " + y; }
	}
	int iDoors = 0, iOpen = 0;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_block" ) ) !is null )
	{
		if( SC_IsDoor( pEnt ) ) { iDoors++; if( pEnt.pev.iuser3 != 0 ) iOpen++; }
	}
	SC_TestOut( "SCTEST save round trip: loaded=" + bOk + " lines " + a.length() + " -> " + b.length() + " differing=" + iDiff
		+ ( szFirst != "" ? " first: " + szFirst : "" ) + " pieces=" + g_SCBlockCount + " doors=" + iDoors + " open=" + iOpen + "\\n" );
	SC_TestFlush();
}
'''
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)
open(p, 'w', newline='', encoding='utf-8').write(s)
print('ok')
