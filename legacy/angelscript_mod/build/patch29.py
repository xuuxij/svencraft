p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\sc_devtour.as"
s = open(p, newline='', encoding='utf-8').read()
a = s.index('	case 1:   // crack overlay')
b = s.index('	case 2:   // crafting window')
s = s[:a] + '''	case 1:   // crack overlays: side face of a placed brick block and the grass top beside it; glowing car
	{
		Vector vecBlock( -300, -180, 20 );
		SC_PlaceBlockAt( vecBlock, MAT_BRICK );
		SC_TourPlace( pPlayer, Vector( -330, -290, 90 ), Vector( 30, 75, 0 ) );
		SC_ShowCrack( pPlayer, vecBlock + Vector( 0, -20, 0 ), Vector( 0, -1, 0 ), SC_BLOCK_SIZE, 0.8 );
		CBaseEntity@ pCar = null;
		while( ( @pCar = g_EntityFuncs.FindEntityByClassname( pCar, "sc_prop" ) ) !is null )
		{
			if( string( pCar.pev.model ).Find( "car" ) != String::INVALID_INDEX )
			{
				SC_ShowGlow( pPlayer, pCar, 0.7 );
				break;
			}
		}
		inv.lastHitTime = g_Engine.time + 100;   // keep the feedback up for the screenshot
		break;
	}
''' + s[b:]
a = s.index('	case 4:   // an exposed ore vein')
b = s.index('	case 5:   // falling sand')
s = s[:a] + '''	case 4:   // an exposed ore vein: open a 3x3 pit down to just above the first ore cell, look straight down
	{
		if( g_SCOreCells.length() >= 3 )
		{
			int ox = g_SCOreCells[0], oy = g_SCOreCells[1], oz = g_SCOreCells[2];
			for( int z = -1; z > oz; z-- )
				for( int dx = -1; dx <= 1; dx++ )
					for( int dy = -1; dy <= 1; dy++ )
					{
						Vector v( ( ox + dx + 0.5 ) * SC_BLOCK_SIZE, ( oy + dy + 0.5 ) * SC_BLOCK_SIZE, ( z + 0.5 ) * SC_BLOCK_SIZE );
						CBaseEntity@ pNode = SC_BlockAt( v );
						if( SC_IsNode( pNode ) ) SC_CarveNode( pNode, ox + dx, oy + dy, z );
					}
			SC_TourPlace( pPlayer, Vector( ( ox + 0.5 ) * SC_BLOCK_SIZE, ( oy - 0.5 ) * SC_BLOCK_SIZE, 60 ), Vector( 80, 90, 0 ) );
			g_EngineFuncs.ServerPrint( "SCTOUR ore pit over " + ox + "," + oy + "," + oz + " pieces=" + g_SCBlockCount + "\\n" );
		}
		break;
	}
''' + s[b:]
open(p, 'w', newline='', encoding='utf-8').write(s)
print('ok')
