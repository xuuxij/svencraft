import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='', encoding='utf-8').read()
    for a, b in reps:
        assert a in s, (name, a[:90])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='', encoding='utf-8').write(s)

# self-test: ore reveal + falling sand
p = os.path.join(D, 'svencraft.as')
s = open(p, newline='', encoding='utf-8').read()
s = s.replace('''	SC_SelfTestMining();
	SC_SelfTestProps();''', '''	SC_SelfTestWorld();
	SC_SelfTestMining();
	SC_SelfTestProps();''', 1)
s = s.replace('''		SC_TestOut( "SCTEST pickup after 1.5s: bot bricks=" + SC_Inv( pBot ).Whole( MAT_BRICK ) + " drops left=" + SC_CountTestDrops( MAT_BRICK ) + " alive=" + pBot.IsAlive() + "\\\\n" );''',
'''		SC_TestOut( "SCTEST pickup after 1.5s: bot bricks=" + SC_Inv( pBot ).Whole( MAT_BRICK ) + " drops left=" + SC_CountTestDrops( MAT_BRICK ) + " alive=" + pBot.IsAlive() + "\\\\n" );
		SC_SelfTestFallCheck();''', 1)
assert 'SC_SelfTestFallCheck();' in s and 'SC_SelfTestWorld();' in s
s += '''
string SC_DescribeCell( int x, int y, int z )
{
	CBaseEntity@ pEnt = SC_BlockAt( Vector( ( x + 0.5 ) * SC_BLOCK_SIZE, ( y + 0.5 ) * SC_BLOCK_SIZE, ( z + 0.5 ) * SC_BLOCK_SIZE ) );
	if( pEnt is null )
		return "air";
	return g_SCMats[pEnt.pev.iuser1].id + "(" + int( pEnt.pev.vuser1.x ) + "x" + int( pEnt.pev.vuser1.y ) + "x" + int( pEnt.pev.vuser1.z ) + ")";
}

// Ore reveal and the setup for the falling-sand check (checked 1.5 s later in SC_SelfTestFallCheck).
void SC_SelfTestWorld()
{
	SC_TestOut( "SCTEST ores: " + g_SCOreCount + " ore cells generated\\n" );
	if( g_SCOreCells.length() >= 3 )
	{
		int ox = g_SCOreCells[0], oy = g_SCOreCells[1], oz = g_SCOreCells[2];
		string szBefore = SC_DescribeCell( ox, oy, oz );
		// dig the cell next to it (x+1 or x-1, whichever is still stone)
		int nx = ox + 1 < SC_TERRAIN_HALF ? ox + 1 : ox - 1;
		CBaseEntity@ pNode = SC_BlockAt( Vector( ( nx + 0.5 ) * SC_BLOCK_SIZE, ( oy + 0.5 ) * SC_BLOCK_SIZE, ( oz + 0.5 ) * SC_BLOCK_SIZE ) );
		if( pNode !is null && SC_IsNode( pNode ) )
			SC_CarveNode( pNode, nx, oy, oz );
		SC_TestOut( "SCTEST ore at " + ox + "," + oy + "," + oz + " (" + g_SCMats[SC_OreAt( ox, oy, oz )].id + "): before dig " + szBefore + ", after digging next to it " + SC_DescribeCell( ox, oy, oz ) + " pieces=" + g_SCBlockCount + "\\n" );
	}
	// falling sand: dig out the bottom sand cell of a beach column; the two sand cells above must fall into place
	CBaseEntity@ pSand = SC_BlockAt( Vector( 18.5 * SC_BLOCK_SIZE, -19.5 * SC_BLOCK_SIZE, -2.5 * SC_BLOCK_SIZE ) );
	SC_TestOut( "SCTEST fall setup: column (18,-20) z=-1.." + SC_DescribeCell( 18, -20, -1 ) + " z=-2 " + SC_DescribeCell( 18, -20, -2 ) + " z=-3 " + SC_DescribeCell( 18, -20, -3 ) + "\\n" );
	if( pSand !is null && SC_IsNode( pSand ) )
	{
		SC_CarveNode( pSand, 18, -20, -3 );
		SC_CheckFall( 18, -20, -2 );
	}
	int iFalling = 0;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_falling" ) ) !is null )
		iFalling++;
	SC_TestOut( "SCTEST fall: dug (18,-20,-3); falling blocks now " + iFalling + "\\n" );
}

void SC_SelfTestFallCheck()
{
	SC_TestOut( "SCTEST fall result: z=-1 " + SC_DescribeCell( 18, -20, -1 ) + " z=-2 " + SC_DescribeCell( 18, -20, -2 ) + " z=-3 " + SC_DescribeCell( 18, -20, -3 ) + " (expect air, sand, sand)\\n" );
}
'''
open(p, 'w', newline='', encoding='utf-8').write(s)
print('ok')
