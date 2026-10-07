// Saving.
//  * Sandbox world: a snapshot of every terrain node, placed block (with door state), hidden ore cell and
//    mined-away map entity, in scripts/plugins/store/svencraft_<map>.txt. Loaded instead of generating terrain.
//  * Inventories (all maps): block counts, selected block and tool tiers per player, in svencraft_players.txt.
// Both are rewritten every SC_SAVE_INTERVAL seconds when their content changed, on map change and on !save.
// Saving is off during automated tests (sc_autotest / sc_visualtest), which need a freshly generated world.

const string SC_SAVE_DIR = "scripts/plugins/store/";
const string SC_SAVE_PLAYERS = "svencraft_players.txt";
const float SC_SAVE_INTERVAL = 15.0;

bool g_SCSaveEnabled = true;
string g_SCLastWorldSave;
string g_SCLastPlayerSave;
dictionary g_SCGone;              // map entities mined away on this map (id -> true)
CScheduledFunction@ g_SCSaveTimer = null;

string SC_WorldSavePath()
{
	return SC_SAVE_DIR + "svencraft_" + g_Engine.mapname + ".txt";
}

string SC_Fmt( float v )
{
	// whole units are exact on the block grid; 2 decimals elsewhere
	int i = int( SC_Floor( v * 100.0 + 0.5 ) );
	string szSign = i < 0 ? "-" : "";
	if( i < 0 ) i = -i;
	int f = i % 100;
	if( f == 0 )
		return szSign + ( i / 100 );
	return szSign + ( i / 100 ) + "." + ( f < 10 ? "0" : "" ) + f;
}

int SC_Round( float v )
{
	return int( SC_Floor( v + 0.5 ) );
}

// Stable id for a map-placed entity: brush entities by model ("*12"), point entities by class and position.
string SC_MapEntId( CBaseEntity@ pEnt )
{
	string szModel = pEnt.pev.model;
	if( szModel.Length() > 1 && szModel.StartsWith( "*" ) )
		return szModel;
	return pEnt.GetClassname() + "@" + SC_Round( pEnt.pev.origin.x ) + "," + SC_Round( pEnt.pev.origin.y ) + "," + SC_Round( pEnt.pev.origin.z );
}

// Entities the plugin creates itself (never part of the map, never "gone"). Terrain nodes share template models.
bool SC_IsOwnEntity( CBaseEntity@ pEnt )
{
	string szClass = pEnt.GetClassname();
	return szClass == "sc_block" || szClass == "sc_node" || szClass == "sc_drop" || szClass == "sc_falling" || szClass == "sc_crack";
}

// A map entity was mined away: remember it so it stays gone after a reload.
void SC_MarkGone( CBaseEntity@ pEnt )
{
	if( pEnt is null || SC_IsOwnEntity( pEnt ) )
		return;
	g_SCGone.set( SC_MapEntId( pEnt ), true );
}

//
// World snapshot
//
string SC_WorldSnapshot()
{
	string s = "svencraft-world 2\n";
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_node" ) ) !is null )
	{
		if( ( pEnt.pev.flags & FL_KILLME ) != 0 )
			continue;
		Vector sz = pEnt.pev.vuser1;
		int sx = SC_Round( sz.x ), sy = SC_Round( sz.y ), sw = SC_Round( sz.z );
		int x = SC_Round( pEnt.pev.origin.x / SC_BLOCK_SIZE - sx * 0.5 );
		int y = SC_Round( pEnt.pev.origin.y / SC_BLOCK_SIZE - sy * 0.5 );
		int z = SC_Round( pEnt.pev.origin.z / SC_BLOCK_SIZE - sw * 0.5 );
		s += "node " + pEnt.pev.iuser1 + " " + x + " " + y + " " + z + " " + sx + " " + sy + " " + sw + " " + pEnt.pev.iuser2 + "\n";
	}
	@pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_block" ) ) !is null )
	{
		if( ( pEnt.pev.flags & FL_KILLME ) != 0 )
			continue;
		Vector o = pEnt.pev.origin, a = pEnt.pev.angles, h = pEnt.pev.vuser1;
		if( pEnt.pev.iuser1 == MAT_DOOR )
			a = g_vecZero;   // recomputed from the door state
		s += "block " + pEnt.pev.iuser1 + " " + SC_Fmt( o.x ) + " " + SC_Fmt( o.y ) + " " + SC_Fmt( o.z )
			+ " " + SC_Fmt( a.x ) + " " + SC_Fmt( a.y ) + " " + SC_Fmt( a.z ) + " " + pEnt.pev.body + " " + SC_Fmt( pEnt.pev.scale )
			+ " " + SC_Fmt( h.x ) + " " + SC_Fmt( h.y ) + " " + SC_Fmt( h.z ) + " " + pEnt.pev.iuser2 + " " + pEnt.pev.iuser3 + "\n";
	}
	for( uint i = 0; i + 2 < g_SCOreCells.length(); i += 3 )
	{
		int iOre = SC_OreAt( g_SCOreCells[i], g_SCOreCells[i + 1], g_SCOreCells[i + 2] );
		if( iOre != MAT_NONE )
			s += "ore " + g_SCOreCells[i] + " " + g_SCOreCells[i + 1] + " " + g_SCOreCells[i + 2] + " " + iOre + "\n";
	}
	for( uint i = 0; i + 2 < g_SCCaveCells.length(); i += 3 )
	{
		if( SC_CaveAt( g_SCCaveCells[i], g_SCCaveCells[i + 1], g_SCCaveCells[i + 2] ) )
			s += "cave " + g_SCCaveCells[i] + " " + g_SCCaveCells[i + 1] + " " + g_SCCaveCells[i + 2] + "\n";
	}
	array<string>@ gone = g_SCGone.getKeys();
	gone.sortAsc();
	for( uint i = 0; i < gone.length(); i++ )
		s += "gone " + gone[i] + "\n";
	return s;
}

bool SC_WriteFile( const string& in szPath, const string& in szData )
{
	File@ f = g_FileSystem.OpenFile( szPath, OpenFile::WRITE );
	if( f is null || !f.IsOpen() )
	{
		g_Game.AlertMessage( at_console, "Svencraft: can't write " + szPath + "\n" );
		return false;
	}
	f.Write( szData );
	f.Close();
	return true;
}

array<string> SC_ReadLines( const string& in szPath )
{
	array<string> lines;
	File@ f = g_FileSystem.OpenFile( szPath, OpenFile::READ );
	if( f is null || !f.IsOpen() )
		return lines;
	string szLine;
	while( !f.EOFReached() )
	{
		f.ReadLine( szLine );
		szLine.Trim();
		if( szLine.Length() > 0 )
			lines.insertLast( szLine );
	}
	f.Close();
	return lines;
}

void SC_SaveWorld( bool bForce = false )
{
	if( !g_SCSaveEnabled || !g_SCSandbox )
		return;
	string s = SC_WorldSnapshot();
	if( !bForce && s == g_SCLastWorldSave )
		return;
	if( SC_WriteFile( SC_WorldSavePath(), s ) )
		g_SCLastWorldSave = s;
}

// Rebuilds the sandbox from the save file. False when there is no usable save (generate a new world instead).
bool SC_LoadWorld( const string& in szPath = "" )
{
	array<string> lines = SC_ReadLines( szPath == "" ? SC_WorldSavePath() : szPath );
	if( lines.length() < 2 || !lines[0].StartsWith( "svencraft-world" ) )
		return false;
	int iNodes = 0;
	for( uint i = 1; i < lines.length(); i++ )
	{
		if( lines[i].StartsWith( "node " ) ) iNodes++;
	}
	if( iNodes == 0 )
		return false;   // reset marker (!resetworld) or empty file
	int iVersion = atoi( SC_Split( lines[0], " " )[1] );

	SC_ClearOres();
	SC_ClearCaves();
	g_SCGone.deleteAll();
	int iBlocks = 0;
	for( uint i = 1; i < lines.length(); i++ )
	{
		array<string> t = SC_Split( lines[i], " " );
		if( t[0] == "node" && t.length() >= 8 )
		{
			int iMat = atoi( t[1] );
			if( iMat >= 0 && iMat < MAT_COUNT )
				SC_SpawnNode( iMat, atoi( t[2] ), atoi( t[3] ), atoi( t[4] ), atoi( t[5] ), atoi( t[6] ), atoi( t[7] ), true, t.length() >= 9 && atoi( t[8] ) != 0 );
		}
		else if( t[0] == "block" && t.length() >= 15 )
		{
			int iMat = atoi( t[1] );
			if( iMat < 0 || iMat >= MAT_COUNT )
				continue;
			CBaseEntity@ pEnt = g_EntityFuncs.Create( "sc_block", Vector( atof( t[2] ), atof( t[3] ), atof( t[4] ) ),
				Vector( atof( t[5] ), atof( t[6] ), atof( t[7] ) ), true );
			if( pEnt is null )
				continue;
			pEnt.pev.iuser1 = iMat;
			pEnt.pev.body = atoi( t[8] );
			pEnt.pev.scale = atof( t[9] );
			pEnt.pev.vuser1 = Vector( atof( t[10] ), atof( t[11] ), atof( t[12] ) );
			pEnt.pev.iuser2 = atoi( t[13] );
			pEnt.pev.iuser3 = atoi( t[14] );
			g_EntityFuncs.DispatchSpawn( pEnt.edict() );
			g_SCBlockCount++;
			iBlocks++;
		}
		else if( t[0] == "cave" && t.length() >= 4 )
			SC_AddCave( atoi( t[1] ), atoi( t[2] ), atoi( t[3] ) );
		else if( t[0] == "ore" && t.length() >= 5 )
			SC_AddOre( atoi( t[1] ), atoi( t[2] ), atoi( t[3] ), atoi( t[4] ) );
		else if( t[0] == "gone" && t.length() >= 2 )
			g_SCGone.set( t[1], true );
	}

	if( iVersion < 2 )
		SC_AddCavesToWorld();

	// Map entities that were mined away stay gone.
	int iGone = 0;
	for( int i = 1; i < g_Engine.maxEntities; i++ )
	{
		CBaseEntity@ pEnt = g_EntityFuncs.Instance( i );
		if( pEnt is null || pEnt.IsPlayer() || SC_IsOwnEntity( pEnt ) )
			continue;
		if( g_SCGone.exists( SC_MapEntId( pEnt ) ) )
		{
			g_EntityFuncs.Remove( pEnt );
			iGone++;
		}
	}
	g_SCLastWorldSave = SC_WorldSnapshot();
	g_EngineFuncs.ServerPrint( "Svencraft: loaded world save (" + iNodes + " terrain pieces, " + iBlocks + " blocks, " + iGone + " mined map objects)\n" );
	return true;
}

// !resetworld: drop the save so the next load generates a fresh world too.
void SC_ClearWorldSave()
{
	g_SCGone.deleteAll();
	if( g_SCSaveEnabled && g_SCSandbox )
	{
		SC_WriteFile( SC_WorldSavePath(), "svencraft-world 1\n" );
		g_SCLastWorldSave = "";
	}
}

//
// Inventories
//
string SC_PlayersSnapshot()
{
	string s = "svencraft-players 1\n";
	array<string>@ keys = g_SCInventories.getKeys();
	keys.sortAsc();
	for( uint k = 0; k < keys.length(); k++ )
	{
		SCInventory@ inv;
		if( !g_SCInventories.get( keys[k], @inv ) || inv is null )
			continue;
		string szCounts = "", szTiers = "";
		for( uint i = 0; i < inv.count.length(); i++ )
			szCounts += ( i > 0 ? "," : "" ) + SC_Fmt( inv.count[i] );
		for( uint i = 0; i < inv.toolTier.length(); i++ )
			szTiers += ( i > 0 ? "," : "" ) + inv.toolTier[i];
		s += "p\t" + keys[k] + "\t" + inv.selected + "\t" + szTiers + "\t" + szCounts + "\n";
	}
	return s;
}

void SC_SavePlayers( bool bForce = false )
{
	if( !g_SCSaveEnabled )
		return;
	string s = SC_PlayersSnapshot();
	if( !bForce && s == g_SCLastPlayerSave )
		return;
	if( SC_WriteFile( SC_SAVE_DIR + SC_SAVE_PLAYERS, s ) )
		g_SCLastPlayerSave = s;
}

void SC_LoadPlayers( const string& in szPath = "" )
{
	if( !g_SCSaveEnabled && szPath == "" )
		return;
	array<string> lines = SC_ReadLines( szPath == "" ? SC_SAVE_DIR + SC_SAVE_PLAYERS : szPath );
	for( uint i = 1; i < lines.length(); i++ )
	{
		array<string> t = SC_Split( lines[i], "\t" );
		if( t.length() < 5 || t[0] != "p" )
			continue;
		SCInventory inv;
		inv.selected = atoi( t[2] );
		if( inv.selected < 0 || inv.selected >= MAT_COUNT )
			inv.selected = MAT_DIRT;
		array<string> tiers = SC_Split( t[3], "," );
		for( uint j = 0; j < tiers.length() && j < inv.toolTier.length(); j++ )
			inv.toolTier[j] = atoi( tiers[j] );
		array<string> counts = SC_Split( t[4], "," );
		for( uint j = 0; j < counts.length() && j < inv.count.length(); j++ )
			inv.count[j] = atof( counts[j] );
		g_SCInventories.set( t[1], @inv );
	}
	g_SCLastPlayerSave = SC_PlayersSnapshot();
}

//
// Scheduling
//
void SC_SaveTick()
{
	SC_SaveWorld();
	SC_SavePlayers();
}

void SC_StartSaveTimer()
{
	if( g_SCSaveTimer !is null )
		g_Scheduler.RemoveTimer( g_SCSaveTimer );
	@g_SCSaveTimer = null;
	if( g_SCSaveEnabled )
		@g_SCSaveTimer = g_Scheduler.SetInterval( "SC_SaveTick", SC_SAVE_INTERVAL, g_Scheduler.REPEAT_INFINITE_TIMES );
}

HookReturnCode SC_MapChange( const string& in szNextMap )
{
	SC_SaveTick();
	return HOOK_CONTINUE;
}

HookReturnCode SC_ClientDisconnect( CBasePlayer@ pPlayer )
{
	SC_SavePlayers();
	return HOOK_CONTINUE;
}

bool SC_FlagFile( const string& in szName )
{
	File@ f = g_FileSystem.OpenFile( SC_SAVE_DIR + szName, OpenFile::READ );
	if( f is null || !f.IsOpen() )
		return false;
	f.Close();
	return true;
}
