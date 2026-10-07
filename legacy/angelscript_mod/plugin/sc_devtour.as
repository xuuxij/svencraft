// Developer visual tour: when scripts/plugins/store/sc_visualtest exists, the first player to spawn on the
// sandbox is walked through scripted scenes (one every few seconds) so screenshots can be captured.

bool g_SCTourStarted = false;
int g_SCTourFirstStep = 0;   // first scene: the number on the flag file's first line (empty = 0)

bool SC_VisualTestEnabled()
{
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_visualtest", OpenFile::READ );
	if( f is null || !f.IsOpen() )
		return false;
	f.Close();
	return true;
}

void SC_TourPlace( CBasePlayer@ pPlayer, const Vector& in vecPos, const Vector& in vecAngles )
{
	g_EntityFuncs.SetOrigin( pPlayer, vecPos );
	pPlayer.pev.velocity = g_vecZero;
	pPlayer.pev.angles = vecAngles;
	pPlayer.pev.v_angle = vecAngles;
	pPlayer.pev.fixangle = 1;
}

void SC_MaybeStartTour( CBasePlayer@ pPlayer )
{
	if( g_SCTourStarted || !g_SCSandbox || !SC_VisualTestEnabled() )
		return;
	g_SCTourStarted = true;
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_visualtest", OpenFile::READ );
	if( f !is null && f.IsOpen() )
	{
		string szLine;
		f.ReadLine( szLine );
		f.Close();
		g_SCTourFirstStep = atoi( szLine );
	}
	pPlayer.pev.takedamage = DAMAGE_NO;
	pPlayer.pev.flags |= FL_GODMODE;
	SC_Inv( pPlayer ).creative = false;
	g_Scheduler.SetTimeout( "SC_TourStep", 4.0, EHandle( pPlayer ), g_SCTourFirstStep );
}

// Each step sets up one scene and schedules the next ~5 s later. Scene times: 4, 9, 14, ... seconds after spawn.
void SC_TourStep( EHandle hPlayer, int iStep )
{
	CBasePlayer@ pPlayer = cast<CBasePlayer@>( hPlayer.GetEntity() );
	if( pPlayer is null || !pPlayer.IsConnected() )
		return;
	SCInventory@ inv = SC_Inv( pPlayer );
	g_EngineFuncs.ServerPrint( "SCTOUR step " + iStep + " at " + g_Engine.time + "\n" );
	switch( iStep )
	{
	case 0:   // overview: test wall row and the house, from the spawn side
		SC_TourPlace( pPlayer, Vector( 0, 120, 60 ), Vector( 8, 90, 0 ) );
		break;
	case 1:   // crack overlays: side face of a placed brick block and the grass top beside it; glowing car
	{
		Vector vecBlock( -300, -180, 20 );
		SC_PlaceBlockAt( vecBlock, MAT_BRICK );
		SC_TourPlace( pPlayer, Vector( -330, -290, 90 ), Vector( 30, 75, 0 ) );
		SC_ShowCrack( pPlayer, vecBlock + Vector( 0, -20, 0 ), Vector( 0, -1, 0 ), SC_BLOCK_SIZE, 0.8 );
		g_Scheduler.SetTimeout( "SC_TourSecondCrack", 2.5, EHandle( pPlayer ) );
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
	case 2:   // crafting window
		SC_ClearFeedback( pPlayer );
		inv.count[MAT_LOG] = 3; inv.count[MAT_PLANKS] = 6; inv.count[MAT_COBBLE] = 5;
		SC_TourPlace( pPlayer, Vector( 0, 0, 60 ), Vector( 5, 90, 0 ) );
		SC_OpenCrafting( pPlayer );
		break;
	case 3:   // close crafting; drops popping out of a broken block in front of the player
		SC_CloseCrafting( pPlayer );
		SC_TourPlace( pPlayer, Vector( 200, -200, 60 ), Vector( 10, 0, 0 ) );
		SC_SpawnDrops( Vector( 400, -200, 30 ), MAT_BRICK, 3 );
		SC_SpawnDrops( Vector( 420, -160, 30 ), MAT_PLANKS, 2 );
		break;
	case 4:   // an exposed ore vein: open a 3x3 pit down to just above the first ore cell, look straight down
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
			g_EngineFuncs.ServerPrint( "SCTOUR ore pit over " + ox + "," + oy + "," + oz + " pieces=" + g_SCBlockCount + "\n" );
		}
		break;
	}
	case 5:   // falling sand: dig under the beach, watch the column drop
	{
		SC_TourPlace( pPlayer, Vector( 18.5 * SC_BLOCK_SIZE, -15 * SC_BLOCK_SIZE, 70 ), Vector( 35, -90, 0 ) );
		CBaseEntity@ pSand = SC_BlockAt( Vector( 18.5 * SC_BLOCK_SIZE, -19.5 * SC_BLOCK_SIZE, -2.5 * SC_BLOCK_SIZE ) );
		if( SC_IsNode( pSand ) )
		{
			SC_CarveNode( pSand, 18, -20, -3 );
			SC_CheckFall( 18, -20, -2 );
		}
		break;
	}
	case 6:   // hotbar with counts after collecting
		inv.count[MAT_DIRT] = 12; inv.count[MAT_STONE] = 3; inv.count[MAT_BRICK] = 7;
		SC_UpdateHud( pPlayer );
		SC_TourPlace( pPlayer, Vector( -200, 300, 60 ), Vector( 0, 60, 0 ) );
		break;
	case 7:   // open ground: a closed door, an open door and a 2x2 glass pane window
	{
		SC_TourPlace( pPlayer, Vector( 650, 540, 60 ), Vector( 8, 90, 0 ) );
		pPlayer.pev.v_angle = Vector( 0, 90, 0 );
		SC_PlaceDoor( pPlayer, Vector( 660, 700, 20 ) );
		SC_PlaceDoor( pPlayer, Vector( 740, 700, 20 ) );
		for( int px = 560; px <= 600; px += 40 )
			for( int pz = 20; pz <= 60; pz += 40 )
				SC_PlacePane( pPlayer, Vector( px, 700, pz ) );
		CBaseEntity@ pEnt = null;
		while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_block" ) ) !is null )
		{
			if( SC_IsDoor( pEnt ) && pEnt.pev.origin.x > 700 )
				SC_ToggleDoor( pEnt );
			if( SC_IsDoor( pEnt ) || pEnt.pev.iuser1 == MAT_GLASS_PANE )
				g_EngineFuncs.ServerPrint( "SCTOUR " + g_SCMats[pEnt.pev.iuser1].id + " at " + pEnt.pev.origin.x + "," + pEnt.pev.origin.y + " box " + ( pEnt.pev.origin + pEnt.pev.mins ).ToString() + " - " + ( pEnt.pev.origin + pEnt.pev.maxs ).ToString() + "
" );
		}
		pPlayer.pev.v_angle = Vector( 8, 90, 0 );
		SC_SpawnDrops( Vector( 610, 600, 30 ), MAT_DOOR, 1 );
		SC_SpawnDrops( Vector( 690, 600, 30 ), MAT_GLASS_PANE, 1 );
		break;
	}
	case 8:   // the same from the side, to see the open door's swing
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
		break;
	default:
		return;
	}
	g_Scheduler.SetTimeout( "SC_TourStep", 5.0, hPlayer, iStep + 1 );
}

void SC_TourSecondCrack( EHandle hPlayer )
{
	CBasePlayer@ pPlayer = cast<CBasePlayer@>( hPlayer.GetEntity() );
	if( pPlayer !is null )
		SC_ShowCrack( pPlayer, Vector( -260, -180, 0 ), Vector( 0, 0, 1 ), SC_BLOCK_SIZE, 0.5 );   // grass top beside the block
}
