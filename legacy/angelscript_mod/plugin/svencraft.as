// Svencraft: Minecraft-style mining and building for Sven Co-op.

#include "sc_texmap"
#include "sc_materials"
#include "sc_inventory"
#include "sc_blocks"
#include "sc_sandbox"
#include "sc_modelinfo"
#include "sc_props"
#include "sc_tools"
#include "sc_hud"
#include "sc_feedback"
#include "sc_crafting"
#include "sc_drops"
#include "sc_devtour"
#include "sc_save"

const string SC_BUILD = "2026-10-06 20:20";   // shown by !sc, to confirm which version the game has loaded

void PluginInit()
{
	g_Module.ScriptInfo.SetAuthor( "Svencraft" );
	g_Module.ScriptInfo.SetContactInfo( "local build" );

	g_Hooks.RegisterHook( Hooks::Player::ClientSay, @SC_ClientSay );
	g_Hooks.RegisterHook( Hooks::Player::PlayerSpawn, @SC_PlayerSpawn );
	g_Hooks.RegisterHook( Hooks::Player::PlayerPreThink, @SC_CraftPreThink );
	g_Hooks.RegisterHook( Hooks::Player::PlayerPostThink, @SC_CraftPostThink );
	g_Hooks.RegisterHook( Hooks::Game::MapChange, @SC_MapChange );
	g_Hooks.RegisterHook( Hooks::Player::ClientDisconnect, @SC_ClientDisconnect );

	SC_InitMaterials();
	SC_InitMixRules();
	SC_InitRecipes();

	g_SCSaveEnabled = !SC_FlagFile( "sc_autotest" ) && !SC_FlagFile( "sc_visualtest" );
	SC_LoadPlayers();
}

void MapInit()
{
	SC_RegisterBlock();
	SC_RegisterNode();
	SC_RegisterProp();
	SC_RegisterFeedback();
	SC_RegisterDrops();
	SC_RegisterFalling();
	SC_RegisterTools();

	g_Game.PrecacheModel( SC_BLOCK_MODEL );
	SC_PrecacheDoors();
	SC_PrecacheMaterials();
	SC_PrecacheTools();
	SC_PrecacheHud();
	SC_PrecacheCrafting();

	g_SCBlockCount = 0;
	g_SCDepleted.deleteAll();
	g_SCCellProgress.deleteAll();
	g_SCSandbox = false;
	g_SCTourStarted = false;
}

void MapActivate()
{
	SC_FindTemplates();
	g_SCGone.deleteAll();
	g_SCLastWorldSave = "";
	if( g_SCSandbox && !( g_SCSaveEnabled && SC_LoadWorld() ) )
		SC_GenerateTerrain();
	SC_StartSaveTimer();

	// Automated test hook: runs only when scripts/plugins/store/sc_autotest exists (dev builds).
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_autotest", OpenFile::READ );
	if( f !is null && f.IsOpen() )
	{
		f.Close();
		g_Scheduler.SetTimeout( "SC_SelfTestTimer", 2.0 );
	}
}

// Server-side self test (no player needed): spawns a block above the first spawn point and traces it.
CConCommand g_SCSelfTest( "sc_selftest", "Svencraft self test", @SC_SelfTest );

string g_SCTestLog;
void SC_TestOut( const string& in s )
{
	g_EngineFuncs.ServerPrint( s );
	g_SCTestLog += s;
}

void SC_SelfTestTimer()
{
	g_SCTestLog = "";
	SC_SelfTest( null );
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );
	if( f !is null && f.IsOpen() )
	{
		f.Write( g_SCTestLog );
		f.Close();
	}
}

void SC_SelfTest( const CCommand@ args )
{
	CBaseEntity@ pStart = g_EntityFuncs.FindEntityByClassname( null, "info_player_start" );
	if( pStart is null )
		@pStart = g_EntityFuncs.FindEntityByClassname( null, "info_player_deathmatch" );
	if( pStart is null )
	{
		SC_TestOut( "SCTEST no spawn point\n" );
		return;
	}
	Vector vecBase = pStart.pev.origin;
	TraceResult tr;
	g_Utility.TraceLine( vecBase, vecBase - Vector( 0, 0, 512 ), ignore_monsters, null, tr );
	Vector vecFloor = tr.vecEndPos;
	string szTex = g_Utility.TraceTexture( g_EntityFuncs.IndexEnt( 0 ), vecBase, vecBase - Vector( 0, 0, 512 ) );
	int iMat = SC_MatForTexture( szTex );
	SC_TestOut( "SCTEST floor z=" + vecFloor.z + " texture='" + szTex + "' -> " + ( iMat >= 0 ? g_SCMats[iMat].name : "none" ) + "\n" );

	Vector c = Vector( SC_Snap( vecFloor.x ), SC_Snap( vecFloor.y ), SC_Floor( vecFloor.z + 0.5 ) + 16 + 128 );
	SC_TestOut( "SCTEST box clear before spawn: " + SC_BoxClear( c, 16 ) + "\n" );
	CBaseEntity@ pBlock = SC_SpawnBlock( c, MAT_BRICK, 32 );
	if( pBlock is null )
	{
		SC_TestOut( "SCTEST spawn FAILED\n" );
		return;
	}
	SC_TestOut( "SCTEST spawned " + pBlock.GetClassname() + " solid=" + pBlock.pev.solid + " skin=" + pBlock.pev.skin
		+ " mins=" + pBlock.pev.mins.z + " maxs=" + pBlock.pev.maxs.z + " model=" + string( pBlock.pev.model ) + " count=" + g_SCBlockCount + "\n" );
	SC_TestOut( "SCTEST box clear after spawn (expect false): " + SC_BoxClear( c, 16 ) + "\n" );
	SC_TestOut( "SCTEST box clear one block up (expect true): " + SC_BoxClear( c + Vector( 0, 0, 32 ), 16 ) + "\n" );

	g_Utility.TraceLine( c + Vector( 0, 0, 100 ), c - Vector( 0, 0, 100 ), dont_ignore_monsters, null, tr );
	CBaseEntity@ pHit = g_EntityFuncs.Instance( tr.pHit );
	SC_TestOut( "SCTEST trace down hits: " + ( pHit is null ? "null" : pHit.GetClassname() ) + " at z=" + tr.vecEndPos.z + " normal z=" + tr.vecPlaneNormal.z + "\n" );

	array<string> texs = { "BRICK1", "C1A0_LABFLRB", "OUT_GRASS1", "SANDGROUND", "-0COMPSCRN", "{GRATE1", "SKY", "!WATERBLUE", "GENERIC48", "CRATE01", "METAL4", "FIFTIES_WALL1" };
	string szMap = "";
	for( uint i = 0; i < texs.length(); i++ )
	{
		int m = SC_MatForTexture( texs[i] );
		szMap += texs[i] + "=" + ( m >= 0 ? g_SCMats[m].id : "none" ) + " ";
	}
	SC_TestOut( "SCTEST texmap " + szMap + "\n" );

	g_EntityFuncs.Remove( pBlock );
	g_SCBlockCount--;

	if( g_SCSandbox )
	{
		array<string>@ keys = g_SCTemplates.getKeys();
		SC_TestOut( "SCTEST sandbox templates=" + keys.length() + " pieces=" + g_SCBlockCount + "\n" );
		g_Utility.TraceLine( Vector( 100, 100, 200 ), Vector( 100, 100, -600 ), dont_ignore_monsters, null, tr );
		@pHit = g_EntityFuncs.Instance( tr.pHit );
		SC_TestOut( "SCTEST terrain at (100,100): " + ( pHit is null ? "null" : pHit.GetClassname() ) + " z=" + tr.vecEndPos.z
			+ ( SC_IsNode( pHit ) ? " mat=" + g_SCMats[pHit.pev.iuser1].id + " size=" + pHit.pev.vuser1.x + "x" + pHit.pev.vuser1.y + "x" + pHit.pev.vuser1.z + " model=" + string( pHit.pev.model ) + " solid=" + pHit.pev.solid : "" ) + "\n" );
		if( SC_IsNode( pHit ) )
		{
			int iBefore = g_SCBlockCount;
			SC_CarveNode( pHit, 2, 2, -1 );
			SC_TestOut( "SCTEST carved grass cell (2,2,-1): pieces " + iBefore + " -> " + g_SCBlockCount + " (expect +8)\n" );
		}
		SC_TestOut( "SCTEST texture mats: sc_grass_top=" + SC_MatForTexture( "sc_grass_top" ) + " sc_bedrock=" + SC_MatForTexture( "sc_bedrock" ) + " sc_log_side=" + SC_MatForTexture( "sc_log_side" ) + "\n" );
		g_Scheduler.SetTimeout( "SC_SelfTestPhase2", 0.5 );
		return;
	}
	SC_SelfTestProps();
	SC_TestOut( "SCTEST done\n" );
	SC_TestFlush();
}

void SC_TestFlush()
{
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );
	if( f !is null && f.IsOpen() )
	{
		f.Write( g_SCTestLog );
		f.Close();
	}
}

// Props/structures on a normal map: classification, splitting walls into blocks, prop material mixes, decor picking.
void SC_SelfTestProps()
{
	dictionary kinds;
	array<string> kindNames = { "none", "block", "node", "creature", "prop", "breakable", "wall", "surface" };
	array<CBaseEntity@> walls;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityInSphere( pEnt, g_vecZero, 65536, "*", "classname" ) ) !is null )
	{
		int k = SC_KindOf( pEnt );
		if( !pEnt.IsBSPModel() && k != KIND_PROP )
			continue;
		string key = pEnt.GetClassname() + "->" + kindNames[k];
		int64 c = 0;
		kinds.get( key, c );
		kinds.set( key, c + 1 );
		if( k == KIND_WALL && walls.length() < 40 )
			walls.insertLast( pEnt );
	}
	array<string>@ keys = kinds.getKeys();
	keys.sortAsc();
	string szKinds = "";
	for( uint i = 0; i < keys.length(); i++ )
	{
		int64 c = 0;
		kinds.get( keys[i], c );
		szKinds += keys[i] + "=" + c + " ";
	}
	SC_TestOut( "SCTEST kinds: " + szKinds + "\n" );
	string szBad = "";
	int iItems = 0;
	for( uint i = 0; i < g_SCRecipes.length(); i++ )
	{
		if( g_SCRecipes[i].item == "" )
			continue;
		iItems++;
		CBaseEntity@ pTest = g_EntityFuncs.Create( g_SCRecipes[i].item, Vector( 0, 0, -4000 ), g_vecZero, true );
		if( pTest is null )
			szBad += g_SCRecipes[i].item + " ";
		else
			g_EntityFuncs.Remove( pTest );
	}
	SC_TestOut( "SCTEST recipes=" + g_SCRecipes.length() + " item recipes=" + iItems + " bad classes: " + ( szBad == "" ? "none" : szBad ) + "\n" );

	// Doors first: they must split like walls.
	array<string> doorClasses = { "func_door", "func_door_rotating" };
	for( uint dc = 0; dc < doorClasses.length(); dc++ )
	{
		CBaseEntity@ pDoor = g_EntityFuncs.FindEntityByClassname( null, doorClasses[dc] );
		if( pDoor is null )
			continue;
		array<CBaseEntity@> dp;
		Vector dd = pDoor.pev.maxs - pDoor.pev.mins;
		int r = SC_BreakIntoBlocks( pDoor, MAT_PLANKS, dp );
		SC_TestOut( "SCTEST door " + doorClasses[dc] + " size " + int( dd.x ) + "x" + int( dd.y ) + "x" + int( dd.z ) + " origin " + pDoor.pev.origin.x + "," + pDoor.pev.origin.y + "," + pDoor.pev.origin.z
			+ " angles " + pDoor.pev.angles.y + " takedamage " + pDoor.pev.takedamage + " kind " + kindNames[SC_KindOf( pDoor )] + " -> result " + kindNames[r] + ", " + dp.length() + " pieces\n" );
	}

	int iSplit = 0, iProp = 0, iBig = 0, iPieces = 0;
	for( uint i = 0; i < walls.length() && iSplit < 6; i++ )
	{
		Vector d = walls[i].pev.maxs - walls[i].pev.mins;
		array<CBaseEntity@> pieces;
		int r = SC_BreakIntoBlocks( walls[i], MAT_CONCRETE, pieces );
		if( r == KIND_WALL )
		{
			iSplit++;
			iPieces += pieces.length();
			SC_TestOut( "SCTEST split " + walls[i].GetClassname() + " size " + int( d.x ) + "x" + int( d.y ) + "x" + int( d.z ) + " -> " + pieces.length()
				+ " pieces, shape " + pieces[0].pev.body + ", piece box " + int( pieces[0].pev.size.x ) + "x" + int( pieces[0].pev.size.y ) + "x" + int( pieces[0].pev.size.z )
				+ " scale " + pieces[0].pev.scale + " angles " + pieces[0].pev.angles.x + "," + pieces[0].pev.angles.z + "\n" );
		}
		else if( r == KIND_PROP ) iProp++;
		else if( r == KIND_SURFACE ) iBig++;
	}
	SC_TestOut( "SCTEST walls tried: split=" + iSplit + " (" + iPieces + " pieces) wholeprop=" + iProp + " toobig=" + iBig + "\n" );

	dictionary seen;
	string szMix = "";
	@pEnt = null;
	CBaseEntity@ pDecorTarget = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, g_SCSandbox ? "sc_prop" : "item_generic" ) ) !is null )
	{
		string szModel = string( pEnt.pev.model );
		if( seen.exists( szModel ) )
			continue;
		seen.set( szModel, true );
		array<int> mats;
		array<float> weights;
		SC_PropMix( pEnt, MAT_NONE, mats, weights );
		Vector mn, mx;
		SC_EntBox( pEnt, mn, mx );
		string szM = "";
		for( uint i = 0; i < mats.length(); i++ )
			szM += g_SCMats[mats[i]].id + ( i + 1 < mats.length() ? "+" : "" );
		if( seen.getSize() <= 24 )
			szMix += szModel + "=" + ( szM == "" ? "nothing" : szM ) + "(" + int( SC_BoxBlocks( mx - mn ) * 10 ) / 10.0 + "blk,solid" + pEnt.pev.solid + ( SC_IsHugeProp( mats.length() > 0 ? mats[0] : MAT_METAL, SC_BoxBlocks( mx - mn ) ) ? ",HARVEST-ONLY" : "" ) + ") ";
		if( pDecorTarget is null && ( mx - mn ).Length() > 8 && pEnt.pev.solid == SOLID_NOT )
			@pDecorTarget = pEnt;
	}
	SC_TestOut( "SCTEST item_generic models=" + seen.getSize() + ": " + szMix + "\n" );

	if( pDecorTarget !is null )
	{
		Vector mn, mx;
		SC_EntBox( pDecorTarget, mn, mx );
		Vector vecCenter = ( mn + mx ) * 0.5;
		Vector vecFrom = vecCenter + Vector( 100, 0, 0 );
		float flDist;
		CBaseEntity@ pFound = SC_FindDecor( vecFrom, Vector( -1, 0, 0 ), SC_REACH, flDist );
		SC_TestOut( "SCTEST decor pick of " + string( pDecorTarget.pev.model ) + ": " + ( pFound is pDecorTarget ? "OK" : ( pFound is null ? "missed" : "got " + string( pFound.pev.model ) ) ) + " dist=" + flDist + "\n" );
	}
	SC_TestOut( "SCTEST entities in use: " + g_EngineFuncs.NumberOfEntities() + " pieces=" + g_SCBlockCount + "\n" );
}

void SC_SelfTestPhase2()
{
	TraceResult tr;
	g_Utility.TraceLine( Vector( 100, 100, 200 ), Vector( 100, 100, -600 ), dont_ignore_monsters, null, tr );
	CBaseEntity@ pHit = g_EntityFuncs.Instance( tr.pHit );
	SC_TestOut( "SCTEST after carve, (100,100) hits: " + ( pHit is null ? "null" : pHit.GetClassname() ) + " z=" + tr.vecEndPos.z
		+ ( SC_IsNode( pHit ) ? " mat=" + g_SCMats[pHit.pev.iuser1].id : "" ) + " (expect dirt at z=-40)\n" );
	g_Utility.TraceLine( Vector( 140, 100, 200 ), Vector( 140, 100, -600 ), dont_ignore_monsters, null, tr );
	@pHit = g_EntityFuncs.Instance( tr.pHit );
	SC_TestOut( "SCTEST neighbour (140,100) hits: " + ( pHit is null ? "null" : pHit.GetClassname() ) + " z=" + tr.vecEndPos.z
		+ ( SC_IsNode( pHit ) ? " mat=" + g_SCMats[pHit.pev.iuser1].id + " size=" + pHit.pev.vuser1.x : "" ) + " (expect grass size 1 at z=0)\n" );
	bool bClear = SC_BoxClear( Vector( 20, 20, 20 ), 20 );
	bool bPlaced = SC_PlaceBlockAt( Vector( 20, 20, 20 ), MAT_BRICK );
	g_Utility.TraceLine( Vector( 20, 20, 200 ), Vector( 20, 20, -600 ), dont_ignore_monsters, null, tr );
	@pHit = g_EntityFuncs.Instance( tr.pHit );
	SC_TestOut( "SCTEST place brick at (20,20,20): clear=" + bClear + " placed=" + bPlaced + " now hits " + ( pHit is null ? "null" : pHit.GetClassname() ) + " z=" + tr.vecEndPos.z
		+ ( SC_IsNode( pHit ) ? " mat=" + g_SCMats[pHit.pev.iuser1].id : "" ) + " (expect brick node, z=40)\n" );
	SC_TestOut( "SCTEST entities in use: " + g_EngineFuncs.NumberOfEntities() + " pieces=" + g_SCBlockCount + "\n" );
	SC_SelfTestWorld();
	SC_SelfTestMining();
	SC_SelfTestProps();
	SC_TestOut( "SCTEST done\n" );
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );
	if( f !is null && f.IsOpen() )
	{
		f.Write( g_SCTestLog );
		f.Close();
	}
}

HookReturnCode SC_PlayerSpawn( CBasePlayer@ pPlayer )
{
	g_Scheduler.SetTimeout( "SC_GiveTools", 0.5, EHandle( pPlayer ) );
	g_Scheduler.SetTimeout( "SC_UpdateHudDelayed", 1.0, EHandle( pPlayer ) );
	SC_MaybeStartTour( pPlayer );
	return HOOK_CONTINUE;
}

HookReturnCode SC_ClientSay( SayParameters@ pParams )
{
	CBasePlayer@ pPlayer = pParams.GetPlayer();
	const CCommand@ args = pParams.GetArguments();
	if( pPlayer is null || args.ArgC() < 1 )
		return HOOK_CONTINUE;

	string szCmd = args.Arg( 0 );
	szCmd = szCmd.ToLowercase();
	SCInventory@ inv = SC_Inv( pPlayer );

	if( szCmd == "!sc" || szCmd == "!svencraft" )
	{
		SC_Chat( pPlayer, "Tools are in weapon slot 1. Left click: mine. Right click: place, or open doors / workbenches (crouch to place on a workbench). R: next block. Mouse3 or K: inventory. O: crafting. F: flashlight (caves are dark). The sandbox saves itself." );
		SC_Chat( pPlayer, "!inv  !craft  !block <name>  !creative  !tools  !save  !clearblocks  !resetworld (sandbox)" );
		SC_Chat( pPlayer, "Build " + SC_BUILD );
	}
	else if( szCmd == "!save" )
	{
		SC_SaveWorld( true );
		SC_SavePlayers( true );
		SC_Chat( pPlayer, g_SCSaveEnabled ? ( g_SCSandbox ? "World and inventories saved" : "Inventories saved" ) : "Saving is off (test mode)" );
	}
	else if( szCmd == "!inv" || szCmd == "!i" )
	{
		SC_ShowInventory( pPlayer );
		SC_OpenInventory( pPlayer );
	}
	else if( szCmd == "!craft" )
	{
		SC_OpenCrafting( pPlayer );
	}
	else if( szCmd == "!resetworld" )
	{
		if( g_PlayerFuncs.AdminLevel( pPlayer ) < ADMIN_YES )
			SC_Chat( pPlayer, "Admins only" );
		else
		{
			SC_ResetWorld();
			SC_Chat( pPlayer, "World reset" );
		}
	}
	else if( szCmd == "!creative" )
	{
		inv.creative = !inv.creative;
		SC_Chat( pPlayer, "Creative mode " + ( inv.creative ? "ON - infinite blocks of every type" : "OFF" ) );
		SC_UpdateHud( pPlayer );
	}
	else if( szCmd == "!block" )
	{
		int iMat = args.ArgC() > 1 ? SC_MatByName( args.Arg( 1 ) ) : MAT_NONE;
		if( iMat == MAT_NONE )
		{
			string szNames = "";
			for( uint i = 0; i < g_SCMats.length(); i++ )
				szNames += ( i > 0 ? ", " : "" ) + g_SCMats[i].id;
			SC_Chat( pPlayer, "Blocks: " + szNames );
		}
		else
		{
			inv.selected = iMat;
			SC_ShowSelection( pPlayer );
		}
	}
	else if( szCmd == "!tools" )
	{
		SC_GiveTools( EHandle( pPlayer ) );
	}
	else if( szCmd == "!clearblocks" )
	{
		if( g_PlayerFuncs.AdminLevel( pPlayer ) < ADMIN_YES )
		{
			SC_Chat( pPlayer, "Admins only" );
		}
		else
		{
			SC_ClearBlocks();
			SC_Chat( pPlayer, "All placed blocks removed" );
		}
	}
	else
	{
		return HOOK_CONTINUE;
	}

	pParams.ShouldHide = true;
	return HOOK_HANDLED;
}

// Mine every piece of each test wall with a bot player and report what lands in its inventory.
void SC_SelfTestMining()
{
	CBasePlayer@ pBot = g_PlayerFuncs.CreateBot( "sctestbot" );
	if( pBot is null )
	{
		SC_TestOut( "SCTEST mining: could not create bot\n" );
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
			SC_RemoveTestDrops();
			int iMined = 0;
			for( uint pi = t; pi < pieces.length(); pi += tools.length() )
			{
				for( int swing = 0; swing < 200 && pieces[pi].pev.solid != SOLID_NOT && pieces[pi].GetClassname() == "sc_block" && ( pieces[pi].pev.flags & FL_KILLME ) == 0; swing++ )
					SC_HitBlock( pBot, pieces[pi], tools[t], Vector( 0, -1, 0 ) );
				iMined++;
			}
			szLine += " | " + SC_ToolName( tools[t] ) + "(tier " + inv.toolTier[tools[t]] + ") mined " + iMined + " -> drops " + SC_CountTestDrops( g_SCMats[iMat].drop );
		}
		SC_TestOut( szLine + "\n" );
	}
	SC_SelfTestCraftUI( pBot );
	// Pickup: stand the bot on a fresh brick drop and check it lands in the inventory.
	SC_RemoveTestDrops();
	SCInventory@ binv = SC_Inv( pBot );
	binv.count[MAT_BRICK] = 0;
	pBot.pev.deadflag = DEAD_NO;
	pBot.pev.health = 100;
	Vector vecAt = Vector( 300, 300, 40 );
	g_EntityFuncs.SetOrigin( pBot, vecAt );
	SC_SpawnDrops( vecAt + Vector( 0, 0, 30 ), MAT_BRICK, 3 );
	SC_TestOut( "SCTEST pickup: spawned " + SC_CountTestDrops( MAT_BRICK ) + " brick drops next to the bot\n" );
	g_Scheduler.SetTimeout( "SC_SelfTestPickup", 1.5, EHandle( pBot ) );
}

void SC_SelfTestPickup( EHandle hBot )
{
	CBasePlayer@ pBot = cast<CBasePlayer@>( hBot.GetEntity() );
	if( pBot !is null )
	{
		SC_TestOut( "SCTEST pickup after 1.5s: bot bricks=" + SC_Inv( pBot ).Whole( MAT_BRICK ) + " drops left=" + SC_CountTestDrops( MAT_BRICK ) + " alive=" + pBot.IsAlive() + "\n" );
		SC_SelfTestFallCheck();
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
	SC_TestOut( "SCTEST door placed=" + bDoor + " found=" + ( pDoor !is null ) + "\n" );
	if( pDoor is null )
		return;
	SC_TestOut( "SCTEST door closed box " + SC_TestBox( pDoor ) + " (want x 200..240 y 440..446 z 0..80)\n" );
	SC_ToggleDoor( pDoor );
	SC_TestOut( "SCTEST door open box " + SC_TestBox( pDoor ) + " open=" + pDoor.pev.iuser3 + " (want x 200..206 y 440..480)\n" );
	g_EntityFuncs.SetOrigin( pBot, Vector( 225, 430, 37 ) );   // stands where the closed panel goes
	SC_ToggleDoor( pDoor );
	SC_TestOut( "SCTEST door close while blocked: open=" + pDoor.pev.iuser3 + " (want 1)\n" );
	g_EntityFuncs.SetOrigin( pBot, Vector( 220, 380, 37 ) );
	SC_ToggleDoor( pDoor );
	SC_TestOut( "SCTEST door close when clear: open=" + pDoor.pev.iuser3 + " (want 0)\n" );

	bool bPane = SC_PlacePane( pBot, Vector( 300, 460, 20 ) );
	CBaseEntity@ pPane = SC_TestFind( MAT_GLASS_PANE );
	SC_TestOut( "SCTEST pane placed=" + bPane + ( pPane !is null ? " box " + SC_TestBox( pPane ) + " body=" + pPane.pev.body + " skin=" + pPane.pev.skin : "" ) + " (want y 458..462)\n" );

	// a second door left open, for the save round trip
	pBot.pev.v_angle = Vector( 0, 0, 0 );
	SC_PlaceDoor( pBot, Vector( 340, 380, 20 ) );

	string szBefore = SC_CountTestDrops( MAT_DOOR );
	int iSwings = 0;
	while( iSwings < 50 && ( pDoor.pev.flags & FL_KILLME ) == 0 )
	{
		SC_HitBlock( pBot, pDoor, TOOL_AXE, Vector( 0, -1, 0 ) );
		iSwings++;
	}
	SC_TestOut( "SCTEST door mined in " + iSwings + " axe swings, door drops " + szBefore + " -> " + SC_CountTestDrops( MAT_DOOR ) + "\n" );
	CBaseEntity@ pDoor2 = null;
	while( ( @pDoor2 = g_EntityFuncs.FindEntityByClassname( pDoor2, "sc_block" ) ) !is null )
	{
		if( SC_IsDoor( pDoor2 ) && ( pDoor2.pev.flags & FL_KILLME ) == 0 )
		{
			SC_ToggleDoor( pDoor2 );
			SC_TestOut( "SCTEST second door " + SC_TestBox( pDoor2 ) + " open=" + pDoor2.pev.iuser3 + "\n" );
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
	array<string> a = SC_Split( g_SCTestSnap, "\n" ), b = SC_Split( s2, "\n" );
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
	// inventories: give a test player some items, round-trip them through a file
	SCInventory@ inv = SC_Inv_Key( "STEAM_0:1:TEST" );
	inv.count[MAT_DOOR] = 3; inv.count[MAT_GLASS_PANE] = 16; inv.count[MAT_BRICK] = 2.5; inv.toolTier[TOOL_PICKAXE] = 3; inv.selected = MAT_DOOR;
	string p1 = SC_PlayersSnapshot();
	SC_WriteFile( "scripts/plugins/store/sc_playertest.txt", p1 );
	g_SCInventories.deleteAll();
	SC_LoadPlayers( "scripts/plugins/store/sc_playertest.txt" );
	string p2 = SC_PlayersSnapshot();
	SC_TestOut( "SCTEST players round trip: equal=" + ( p1 == p2 ) + " test player doors=" + SC_Inv_Key( "STEAM_0:1:TEST" ).Whole( MAT_DOOR ) + " pickaxe tier=" + SC_Inv_Key( "STEAM_0:1:TEST" ).toolTier[TOOL_PICKAXE] + "
" );
	SC_TestOut( "SCTEST save round trip: loaded=" + bOk + " lines " + a.length() + " -> " + b.length() + " differing=" + iDiff
		+ ( szFirst != "" ? " first: " + szFirst : "" ) + " pieces=" + g_SCBlockCount + " doors=" + iDoors + " open=" + iOpen + "\n" );
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
	array<string> lines = SC_Split( s2, "\n" );
	for( uint i = 0; i < lines.length(); i++ ) { if( lines[i].StartsWith( "cave " ) ) iCaveLines++; }
	SC_TestOut( "SCTEST upgrade v1 save: loaded=" + bOk + " stairs " + SC_DescribeCell( x0 + 1, y0, -1 ) + " tunnel " + SC_DescribeCell( x0 + 10, y0, -9 )
		+ " floor " + SC_DescribeCell( x0 + 10, y0, -10 ) + " | non-terrain nodes kept=" + iPlaced + " | snapshot header " + lines[0] + ", hidden cave cells " + iCaveLines + " pieces=" + g_SCBlockCount + "\n" );
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
}

// Simulate one key press for the crafting window.
void SC_TestPress( CBasePlayer@ pBot, int iButton )
{
	uint uiFlags;
	pBot.pev.button = iButton;
	SC_CraftPreThink( pBot, uiFlags );
	pBot.pev.button = 0;
	SC_CraftPreThink( pBot, uiFlags );
}

void SC_SelfTestCraftUI( CBasePlayer@ pBot )
{
	SCInventory@ inv = SC_Inv( pBot );
	for( int k = 0; k < MAT_COUNT; k++ ) inv.count[k] = 0;
	inv.count[MAT_LOG] = 2;
	inv.craftCat = CRAFT_BASIC;
	inv.craftSel = 0;
	inv.craftOpen = false;
	inv.craftClosedTime = 0;
	pBot.pev.deadflag = DEAD_NO;
	pBot.pev.health = 100;
	pBot.pev.v_angle = Vector( 10, 50, 0 );
	SC_OpenCrafting( pBot );
	string szLog = "SCTEST craftui open=" + inv.craftOpen + " maxspeed=" + pBot.pev.maxspeed + " cursor=" + inv.cursorX + "," + inv.cursorY;
	// mouse: turning right 7 degrees and down 7 degrees should move the cursor +56,+56 (grid slot 4 -> slot 8) and snap the view back
	pBot.pev.v_angle = Vector( 17, 43, 0 );
	SC_CraftPostThink( pBot );
	szLog += " | moved cursor=" + inv.cursorX + "," + inv.cursorY + " view=" + pBot.pev.v_angle.x + "," + pBot.pev.v_angle.y + " fix=" + pBot.pev.fixangle;
	inv.cursorX = -164; inv.cursorY = -64;          // over grid slot 0 (planks recipe, already selected)
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | click slot0 logs=" + inv.Whole( MAT_LOG ) + " planks=" + inv.Whole( MAT_PLANKS );
	inv.cursorX = -108; inv.cursorY = -64;          // slot 1 (workbench): first click selects
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | click slot1 sel=" + inv.craftSel + " workbench=" + inv.Whole( MAT_WORKBENCH );
	inv.cursorX = 132; inv.cursorY = -8;            // result slot crafts the selected recipe
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | click result workbench=" + inv.Whole( MAT_WORKBENCH ) + " planks=" + inv.Whole( MAT_PLANKS );
	inv.cursorX = -40; inv.cursorY = 98;            // next-category button
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | next button cat=" + inv.craftCat;
	inv.cursorX = -176; inv.cursorY = 98;           // previous-category button
	SC_TestPress( pBot, IN_ATTACK );
	szLog += " | prev button cat=" + inv.craftCat;
	SC_TestPress( pBot, IN_ATTACK2 );               // close
	szLog += " | closed=" + !inv.craftOpen + " maxspeed restored=" + pBot.pev.maxspeed;
	SC_TestOut( szLog + "\n" );
}

string SC_DescribeCell( int x, int y, int z )
{
	CBaseEntity@ pEnt = SC_BlockAt( Vector( ( x + 0.5 ) * SC_BLOCK_SIZE, ( y + 0.5 ) * SC_BLOCK_SIZE, ( z + 0.5 ) * SC_BLOCK_SIZE ) );
	if( pEnt is null )
		return "air";
	return g_SCMats[pEnt.pev.iuser1].id + "(" + int( pEnt.pev.vuser1.x ) + "x" + int( pEnt.pev.vuser1.y ) + "x" + int( pEnt.pev.vuser1.z ) + ")";
}

// Ore reveal now; falling sand is set up here and checked 1.5 s later (SC_SelfTestFallCheck).
void SC_SelfTestCaves()
{
	int x0 = SC_CAVE_ENTRANCE_X, y0 = SC_CAVE_ENTRANCE_Y;
	SC_TestOut( "SCTEST caves: " + g_SCCaveCount + " cells, dark stone template=" + g_SCTemplatesDark.exists( SC_TplKey( MAT_STONE, 1, 1, 1 ) )
		+ " | stairs (x0+1,y0,-1) " + SC_DescribeCell( x0 + 1, y0, -1 ) + ", under it " + SC_DescribeCell( x0 + 1, y0, -2 )
		+ " | step 9 floor " + SC_DescribeCell( x0 + 9, y0, -10 ) + " | tunnel (x0+10,y0,-9) " + SC_DescribeCell( x0 + 10, y0, -9 )
		+ " (-8) " + SC_DescribeCell( x0 + 10, y0, -8 ) + " ceiling (-7) " + SC_DescribeCell( x0 + 10, y0, -7 ) + "\n" );
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
			+ SC_DescribeCell( cx, cy, cz ) + " stillHidden=" + SC_CaveAt( cx, cy, cz ) + " pieces " + iPieces + " -> " + g_SCBlockCount + "\n" );
		break;
	}
}

void SC_SelfTestWorld()
{
	SC_SelfTestCaves();
	string szOreTpl = "";
	array<string>@ keys = g_SCTemplates.getKeys();
	SC_TestOut( "SCTEST templates now " + keys.length() + "; ore templates: iron=" + SC_HasTemplate( MAT_IRON_ORE, 1, 1, 1 ) + " crystal=" + SC_HasTemplate( MAT_CRYSTAL_ORE, 1, 1, 1 ) + "\n" );
	SC_TestOut( "SCTEST ores: " + g_SCOreCount + " ore cells generated\n" );
	if( g_SCOreCells.length() >= 3 )
	{
		int ox = g_SCOreCells[0], oy = g_SCOreCells[1], oz = g_SCOreCells[2];
		string szBefore = SC_DescribeCell( ox, oy, oz );
		int nx = ox + 1 < SC_TERRAIN_HALF ? ox + 1 : ox - 1;
		CBaseEntity@ pNode = SC_BlockAt( Vector( ( nx + 0.5 ) * SC_BLOCK_SIZE, ( oy + 0.5 ) * SC_BLOCK_SIZE, ( oz + 0.5 ) * SC_BLOCK_SIZE ) );
		if( pNode !is null && SC_IsNode( pNode ) )
			SC_CarveNode( pNode, nx, oy, oz );
		SC_TestOut( "SCTEST ore at " + ox + "," + oy + "," + oz + " (" + g_SCMats[SC_OreAt( ox, oy, oz )].id + "): before dig " + szBefore + ", after digging next to it " + SC_DescribeCell( ox, oy, oz ) + " pieces=" + g_SCBlockCount + "\n" );
	}
	SC_TestOut( "SCTEST fall setup: column (18,-20) z=-1 " + SC_DescribeCell( 18, -20, -1 ) + " z=-2 " + SC_DescribeCell( 18, -20, -2 ) + " z=-3 " + SC_DescribeCell( 18, -20, -3 ) + "\n" );
	CBaseEntity@ pSand = SC_BlockAt( Vector( 18.5 * SC_BLOCK_SIZE, -19.5 * SC_BLOCK_SIZE, -2.5 * SC_BLOCK_SIZE ) );
	if( pSand !is null && SC_IsNode( pSand ) )
	{
		SC_CarveNode( pSand, 18, -20, -3 );
		SC_CheckFall( 18, -20, -2 );
	}
	int iFalling = 0;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_falling" ) ) !is null )
		iFalling++;
	SC_TestOut( "SCTEST fall: dug (18,-20,-3); falling blocks now " + iFalling + "\n" );
}

void SC_SelfTestFallCheck()
{
	SC_TestOut( "SCTEST fall result: z=-1 " + SC_DescribeCell( 18, -20, -1 ) + " z=-2 " + SC_DescribeCell( 18, -20, -2 ) + " z=-3 " + SC_DescribeCell( 18, -20, -3 ) + " (expect air, sand, sand)\n" );
}
