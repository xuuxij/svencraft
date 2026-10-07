// Per-player block inventory. Keyed by SteamID so it survives deaths and map changes.

class SCInventory
{
	array<float> count;     // fractional so sub-blocks (smaller than a full block) add up
	int selected = MAT_DIRT;
	bool creative = false;

	// Progress on the world surface currently being mined.
	string mineKey;
	float mineProgress = 0;

	CTextMenu@ menu = null;
	int hudNameShown = -1;  // block whose name was last popped up on the HUD

	// Mining feedback (sc_feedback.as)
	EHandle crack;
	EHandle glow;
	int glowFx = 0;
	float glowAmt = 0;
	Vector glowColor;
	float lastHitTime = 0;

	array<int> toolTier = { 0, 1, 1, 1 };   // by SCTool: hand, pickaxe, shovel, axe (start wooden)
	bool craftOpen = false;     // crafting window (sc_crafting.as)
	int craftCat = 0;
	int craftSel = 0;
	int lastButtons = 0;
	float craftClosedTime = 0;
	float cursorX = 0;          // emulated mouse cursor, pixels from the screen centre
	float cursorY = 0;
	Vector craftAngles;         // view held here while the window is open
	float cursorSentTime = 0;
	float savedMaxspeed = 0;
	bool snapPending = false;

	SCInventory()
	{
		count.resize( MAT_COUNT );
		for( uint i = 0; i < count.length(); i++ )
			count[i] = 0;
	}

	int Whole( int iMat ) { return int( count[iMat] + 0.001 ); }
	bool Has( int iMat ) { return creative || count[iMat] >= 0.999; }
	void Add( int iMat, float flAmount ) { count[iMat] += flAmount; }

	bool Take( int iMat )
	{
		if( creative )
			return true;
		if( count[iMat] < 0.999 )
			return false;
		count[iMat] -= 1.0;
		if( count[iMat] < 0.001 )
			count[iMat] = 0;
		return true;
	}
}

dictionary g_SCInventories;

string SC_PlayerKey( CBasePlayer@ pPlayer )
{
	string szId = g_EngineFuncs.GetPlayerAuthId( pPlayer.edict() );
	if( szId == "" || szId == "BOT" || szId == "STEAM_ID_LAN" || szId == "STEAM_ID_PENDING" )
		szId = "name:" + string( pPlayer.pev.netname );
	return szId;
}

SCInventory@ SC_Inv( CBasePlayer@ pPlayer )
{
	return SC_Inv_Key( SC_PlayerKey( pPlayer ) );
}

SCInventory@ SC_Inv_Key( const string& in szKey )
{
	SCInventory@ inv;
	if( !g_SCInventories.get( szKey, @inv ) || inv is null )
	{
		@inv = SCInventory();
		g_SCInventories.set( szKey, @inv );
	}
	return inv;
}

void SC_Print( CBasePlayer@ pPlayer, const string& in szMsg )
{
	g_PlayerFuncs.ClientPrint( pPlayer, HUD_PRINTCENTER, szMsg );
}

void SC_Chat( CBasePlayer@ pPlayer, const string& in szMsg )
{
	g_PlayerFuncs.ClientPrint( pPlayer, HUD_PRINTTALK, "[Svencraft] " + szMsg + "\n" );
}

void SC_ShowSelection( CBasePlayer@ pPlayer )
{
	SC_UpdateHud( pPlayer );
}

// Select the next (iDir = 1) or previous (iDir = -1) block type the player can place.
void SC_CycleBlock( CBasePlayer@ pPlayer, int iDir )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	for( int i = 1; i <= MAT_COUNT; i++ )
	{
		int iMat = ( inv.selected + iDir * i + MAT_COUNT * 2 ) % MAT_COUNT;
		if( inv.Has( iMat ) )
		{
			inv.selected = iMat;
			SC_ShowSelection( pPlayer );
			return;
		}
	}
	SC_Print( pPlayer, "No blocks yet - mine something first\n(or say !creative)" );
}

void SC_ShowInventory( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	string szList = "";
	for( int i = 0; i < MAT_COUNT; i++ )
	{
		if( inv.count[i] < 0.01 )
			continue;
		if( szList != "" )
			szList += ", ";
		szList += g_SCMats[i].name + " " + inv.Whole( i );
		float flFrac = inv.count[i] - float( inv.Whole( i ) );
		if( flFrac >= 0.01 )
			szList += " (+" + int( flFrac * 100 ) + "%)";
	}
	if( szList == "" )
		szList = "empty";
	SC_Chat( pPlayer, "Inventory: " + szList + ( inv.creative ? "  [creative: infinite blocks]" : "" ) );
}
