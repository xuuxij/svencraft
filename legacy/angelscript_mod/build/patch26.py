p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
reps = [
    # mining test: count the drops each tool's breaks produce (drops replace direct inventory adds)
    ("""			for( int k = 0; k < MAT_COUNT; k++ ) inv.count[k] = 0;
			int iMined = 0;""", """			SC_RemoveTestDrops();
			int iMined = 0;"""),
    ("""			szLine += " | " + SC_ToolName( tools[t] ) + "(tier " + inv.toolTier[tools[t]] + ") mined " + iMined + " -> got " + inv.count[g_SCMats[iMat].drop];""",
     """			szLine += " | " + SC_ToolName( tools[t] ) + "(tier " + inv.toolTier[tools[t]] + ") mined " + iMined + " -> drops " + SC_CountTestDrops( g_SCMats[iMat].drop );"""),
    # keep the bot for the pickup test; it disconnects there
    ("""	SC_SelfTestCraftUI( pBot );
	g_PlayerFuncs.BotDisconnect( pBot );
}""", """	SC_SelfTestCraftUI( pBot );
	// Pickup: stand the bot on a fresh brick drop and check it lands in the inventory.
	SC_RemoveTestDrops();
	SCInventory@ binv = SC_Inv( pBot );
	binv.count[MAT_BRICK] = 0;
	pBot.pev.deadflag = DEAD_NO;
	pBot.pev.health = 100;
	Vector vecAt = Vector( 300, 300, 40 );
	g_EntityFuncs.SetOrigin( pBot, vecAt );
	SC_SpawnDrops( vecAt + Vector( 0, 0, 30 ), MAT_BRICK, 3 );
	SC_TestOut( "SCTEST pickup: spawned " + SC_CountTestDrops( MAT_BRICK ) + " brick drops next to the bot\\n" );
	g_Scheduler.SetTimeout( "SC_SelfTestPickup", 1.5, EHandle( pBot ) );
}

void SC_SelfTestPickup( EHandle hBot )
{
	CBasePlayer@ pBot = cast<CBasePlayer@>( hBot.GetEntity() );
	if( pBot !is null )
	{
		SC_TestOut( "SCTEST pickup after 1.5s: bot bricks=" + SC_Inv( pBot ).Whole( MAT_BRICK ) + " drops left=" + SC_CountTestDrops( MAT_BRICK ) + " alive=" + pBot.IsAlive() + "\\n" );
		g_PlayerFuncs.BotDisconnect( pBot );
	}
	SC_TestFlush();
}

void SC_RemoveTestDrops()
{
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_drop" ) ) !is null )
	{
		pEnt.pev.fuser1 = 0;
		g_EntityFuncs.Remove( pEnt );
	}
}

// Total amount carried by live drops of one material (removed ones have amount 0).
string SC_CountTestDrops( int iMat )
{
	float flTotal = 0;
	int n = 0;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_drop" ) ) !is null )
	{
		if( pEnt.pev.iuser1 == iMat && pEnt.pev.fuser1 > 0 )
		{
			flTotal += pEnt.pev.fuser1;
			n++;
		}
	}
	return "" + n + " (" + int( flTotal + 0.001 ) + " blocks)";
}"""),
    ('" frozen=" + ( ( pBot.pev.flags & FL_FROZEN ) != 0 ) + " cursor="', '" maxspeed=" + pBot.pev.maxspeed + " cursor="'),
    ('szLog += " | closed=" + !inv.craftOpen + " frozen=" + ( ( pBot.pev.flags & FL_FROZEN ) != 0 );', 'szLog += " | closed=" + !inv.craftOpen + " maxspeed restored=" + pBot.pev.maxspeed;'),
]
for a, b in reps:
    assert a in s, a[:80]
    s = s.replace(a, b, 1)
open(p, 'w', newline='').write(s)
print('ok')
