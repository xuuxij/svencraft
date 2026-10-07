# Cave self-test, generation piece count, tour scenes.
import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"


def patch(name, pairs):
    p = os.path.join(D, name)
    s = open(p, newline='', encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (name, old[:90], s.count(old))
        s = s.replace(old, new)
    open(p, 'w', newline='', encoding='utf-8').write(s)
    print('patched', name)


patch('sc_sandbox.as', [
    ('''	SC_BlockTree( -14, -12 );
	SC_BlockTree( 12, -16 );
}''', '''	SC_BlockTree( -14, -12 );
	SC_BlockTree( 12, -16 );
	g_EngineFuncs.ServerPrint( "Svencraft: generated terrain, " + g_SCBlockCount + " pieces\\n" );
}'''),
])

patch('svencraft.as', [
    ('''void SC_SelfTestWorld()
{
	string szOreTpl = "";''', '''void SC_SelfTestCaves()
{
	int x0 = SC_CAVE_ENTRANCE_X, y0 = SC_CAVE_ENTRANCE_Y;
	SC_TestOut( "SCTEST caves: " + g_SCCaveCount + " cells, dark stone template=" + g_SCTemplatesDark.exists( SC_TplKey( MAT_STONE, 1, 1, 1 ) )
		+ " | stairs (x0+1,y0,-1) " + SC_DescribeCell( x0 + 1, y0, -1 ) + ", under it " + SC_DescribeCell( x0 + 1, y0, -2 )
		+ " | step 9 floor " + SC_DescribeCell( x0 + 9, y0, -10 ) + " | tunnel (x0+10,y0,-9) " + SC_DescribeCell( x0 + 10, y0, -9 )
		+ " (-8) " + SC_DescribeCell( x0 + 10, y0, -8 ) + " ceiling (-7) " + SC_DescribeCell( x0 + 10, y0, -7 ) + "\\n" );
	// a hidden cave cell: dig into the raw chunk around it and check the cave opens
	for( uint i = 0; i + 2 < g_SCCaveCells.length(); i += 3 )
	{
		int cx = g_SCCaveCells[i], cy = g_SCCaveCells[i + 1], cz = g_SCCaveCells[i + 2];
		if( !SC_CaveAt( cx, cy, cz ) )
			continue;
		CBaseEntity@ pNode = SC_BlockAt( Vector( ( cx + 0.5 ) * SC_BLOCK_SIZE, ( cy + 0.5 ) * SC_BLOCK_SIZE, ( cz + 0.5 ) * SC_BLOCK_SIZE ) );
		if( !SC_IsNode( pNode ) || pNode.pev.vuser1.x < 8 )
			continue;
		string szBefore = SC_DescribeCell( cx, cy, cz );
		int iPieces = g_SCBlockCount;
		// carve the chunk's top corner cell (stone, not cave: caves stay below z -5)
		int nx = int( SC_Floor( pNode.pev.origin.x / SC_BLOCK_SIZE - 4 + 0.5 ) ), ny = int( SC_Floor( pNode.pev.origin.y / SC_BLOCK_SIZE - 4 + 0.5 ) );
		SC_CarveNode( pNode, nx, ny, -4 );
		SC_TestOut( "SCTEST hidden cave cell " + cx + "," + cy + "," + cz + ": before " + szBefore + ", after digging chunk corner " + nx + "," + ny + ",-4: "
			+ SC_DescribeCell( cx, cy, cz ) + " stillHidden=" + SC_CaveAt( cx, cy, cz ) + " pieces " + iPieces + " -> " + g_SCBlockCount + "\\n" );
		break;
	}
}

void SC_SelfTestWorld()
{
	SC_SelfTestCaves();
	string szOreTpl = "";'''),
])

patch('sc_devtour.as', [
    ('''	case 8:   // the same from the side, to see the open door's swing
		SC_TourPlace( pPlayer, Vector( 520, 840, 60 ), Vector( 10, -40, 0 ) );
		break;''', '''	case 8:   // the same from the side, to see the open door's swing
		SC_TourPlace( pPlayer, Vector( 520, 840, 60 ), Vector( 10, -40, 0 ) );
		break;
	case 9:   // cave entrance: top of the staircase, looking down it
		SC_TourPlace( pPlayer, Vector( ( SC_CAVE_ENTRANCE_X - 0.5 ) * SC_BLOCK_SIZE, ( SC_CAVE_ENTRANCE_Y + 1 ) * SC_BLOCK_SIZE, 60 ), Vector( 25, 0, 0 ) );
		break;
	case 10:  // inside the tunnel at the bottom of the stairs, flashlight on
		SC_TourPlace( pPlayer, Vector( ( SC_CAVE_ENTRANCE_X + 9.5 ) * SC_BLOCK_SIZE, ( SC_CAVE_ENTRANCE_Y + 1 ) * SC_BLOCK_SIZE, -9 * SC_BLOCK_SIZE + 40 ), Vector( 5, 0, 0 ) );
		pPlayer.pev.impulse = 100;
		break;
	case 11:  // looking back up the stairs from the tunnel
		SC_TourPlace( pPlayer, Vector( ( SC_CAVE_ENTRANCE_X + 11 ) * SC_BLOCK_SIZE, ( SC_CAVE_ENTRANCE_Y + 1 ) * SC_BLOCK_SIZE, -9 * SC_BLOCK_SIZE + 40 ), Vector( -25, 180, 0 ) );
		break;'''),
])
