p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
anchor = '''	SC_SelfTestProps();
	SC_TestOut( "SCTEST done\\n" );
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );'''
new = '''	SC_SelfTestMining();
	SC_SelfTestProps();
	SC_TestOut( "SCTEST done\\n" );
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );'''
func = '''
// Mine every piece of each test wall with a bot player and report what lands in its inventory.
void SC_SelfTestMining()
{
	CBasePlayer@ pBot = g_PlayerFuncs.CreateBot( "sctestbot" );
	if( pBot is null )
	{
		SC_TestOut( "SCTEST mining: could not create bot\\n" );
		return;
	}
	SCInventory@ inv = SC_Inv( pBot );
	array<int> tools = { TOOL_PICKAXE, TOOL_SHOVEL, TOOL_AXE };
	array<CBaseEntity@> walls;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "func_wall" ) ) !is null )
	{
		if( string( pEnt.pev.targetname ) == "" && pEnt.pev.origin.y + pEnt.pev.maxs.y < 400 && pEnt.pev.origin.y + pEnt.pev.mins.y > 300 )
			walls.insertLast( pEnt );   // the test wall row at y 320..352
	}
	for( uint w = 0; w < walls.length(); w++ )
	{
		Vector d = walls[w].pev.maxs - walls[w].pev.mins;
		TraceResult tr;
		Vector c = walls[w].pev.origin + ( walls[w].pev.mins + walls[w].pev.maxs ) * 0.5;
		g_Utility.TraceLine( c - Vector( 0, 200, 0 ), c, ignore_monsters, null, tr );
		int iMat = SC_MatForTexture( g_Utility.TraceTexture( walls[w].edict(), c - Vector( 0, 200, 0 ), c ) );
		if( iMat == MAT_NONE ) iMat = MAT_CONCRETE;
		array<CBaseEntity@> pieces;
		SC_BreakIntoBlocks( walls[w], iMat, pieces );
		string szLine = "SCTEST mine wall " + int( d.x ) + "x" + int( d.y ) + "x" + int( d.z ) + " " + g_SCMats[iMat].id + ": " + pieces.length() + " pieces";
		for( uint t = 0; t < tools.length() && pieces.length() > 0; t++ )
		{
			// each tool mines a third of the pieces (at least one)
			for( int k = 0; k < MAT_COUNT; k++ ) inv.count[k] = 0;
			int iMined = 0;
			for( uint pi = t; pi < pieces.length(); pi += tools.length() )
			{
				for( int swing = 0; swing < 200 && pieces[pi].pev.solid != SOLID_NOT && pieces[pi].GetClassname() == "sc_block" && ( pieces[pi].pev.flags & FL_KILLME ) == 0; swing++ )
					SC_HitBlock( pBot, pieces[pi], tools[t], Vector( 0, -1, 0 ) );
				iMined++;
			}
			szLine += " | " + SC_ToolName( tools[t] ) + "(tier " + inv.toolTier[tools[t]] + ") mined " + iMined + " -> got " + inv.count[g_SCMats[iMat].drop];
		}
		SC_TestOut( szLine + "\\n" );
	}
	g_PlayerFuncs.BotDisconnect( pBot );
}
'''
if 'SC_SelfTestMining' not in s:
    assert anchor in s
    s = s.replace(anchor, new) + func
open(p, 'w', newline='').write(s)
print('ok')
