// Minecraft-style hotbar at the bottom of the screen + inventory menu.

const string SC_SPR_HOTBAR = "svencraft/hotbar.spr";   // frame m = slot holding block m, 18 = empty slot; +19 = selected
const string SC_SPR_COUNTS = "svencraft/counts.spr";   // frame n = stack count n (1..99), 100 = "99+"
const int SC_HOTBAR_EMPTY = MAT_COUNT;           // frames: 0..MAT_COUNT-1 blocks, then empty
const int SC_HOTBAR_SELECTED = MAT_COUNT + 1;    // selected variants start here
const int SC_HUD_SLOTS = 8;           // icon channels 0..7, count channels 8..15 (16 HUD sprite channels in total)
const int SC_HUD_CH_COUNT = 8;
const int SC_HUD_TEXT_CHANNEL = 4;
const float SC_HUD_Y = 0.895;         // top edge of the hotbar (fraction of screen height)
const float SC_HUD_SPACING = 56;      // slot width in pixels

void SC_PrecacheHud()
{
	g_Game.PrecacheModel( "sprites/" + SC_SPR_HOTBAR );
	g_Game.PrecacheModel( "sprites/" + SC_SPR_COUNTS );
	g_Game.PrecacheGeneric( "sprites/svencraft/sctools.spr" );
	g_Game.PrecacheGeneric( "sprites/svencraft/sctools_s.spr" );
	for( uint i = 0; i < SC_TOOL_NAMES.length(); i++ )
		g_Game.PrecacheGeneric( "sprites/svencraft/" + SC_TOOL_NAMES[i] + ".txt" );
}

void SC_HudSprite( CBasePlayer@ pPlayer, int iChannel, const string& in szSprite, int iFrame, int iSlot )
{
	HUDSpriteParams sp;
	sp.channel = iChannel;
	sp.flags = HUD_ELEM_ABSOLUTE_X | HUD_ELEM_SCR_CENTER_X | HUD_SPR_MASKED;
	sp.spritename = szSprite;
	sp.frame = iFrame;
	sp.x = ( iSlot - ( SC_HUD_SLOTS - 1 ) * 0.5 ) * SC_HUD_SPACING;
	sp.y = SC_HUD_Y;
	sp.holdTime = 99999;
	sp.color1 = RGBA( 255, 255, 255, 255 );
	g_PlayerFuncs.HudCustomSprite( pPlayer, sp );
}

void SC_UpdateHud( CBasePlayer@ pPlayer )
{
	if( pPlayer is null || !pPlayer.IsConnected() )
		return;
	SCInventory@ inv = SC_Inv( pPlayer );
	if( inv.craftOpen )
		return;   // the crafting window is using the HUD channels; it redraws the hotbar on close

	array<int> owned;
	for( int i = 0; i < MAT_COUNT; i++ )
	{
		if( inv.Has( i ) )
			owned.insertLast( i );
	}
	if( owned.length() > 0 && !inv.Has( inv.selected ) )
		inv.selected = owned[0];

	// Show a window of 9 slots that always contains the selected block.
	int iSel = owned.find( inv.selected );   // -1 when the inventory is empty
	int iStart = 0;
	if( int( owned.length() ) > SC_HUD_SLOTS )
	{
		iStart = iSel - SC_HUD_SLOTS / 2;
		if( iStart < 0 ) iStart = 0;
		if( iStart > int( owned.length() ) - SC_HUD_SLOTS ) iStart = int( owned.length() ) - SC_HUD_SLOTS;
	}
	int iSelSlot = iSel >= 0 ? iSel - iStart : 0;

	for( int i = 0; i < SC_HUD_SLOTS; i++ )
	{
		int k = iStart + i;
		bool bFilled = k < int( owned.length() );
		int iFrame = bFilled ? owned[k] : SC_HOTBAR_EMPTY;
		if( i == iSelSlot )
			iFrame += SC_HOTBAR_SELECTED;
		SC_HudSprite( pPlayer, i, SC_SPR_HOTBAR, iFrame, i );
		// Stack count in the corner of every slot (Minecraft shows it when there's more than one).
		int n = bFilled ? inv.Whole( owned[k] ) : 0;
		if( bFilled && !inv.creative && n > 1 )
			SC_HudSprite( pPlayer, SC_HUD_CH_COUNT + i, SC_SPR_COUNTS, n > 99 ? 100 : n, i );
		else
			g_PlayerFuncs.HudToggleElement( pPlayer, SC_HUD_CH_COUNT + i, false );
	}

	// The block name pops up briefly when the selection changes, like Minecraft.
	int iShown = iSel >= 0 ? inv.selected : -1;
	if( iShown != inv.hudNameShown )
	{
		inv.hudNameShown = iShown;
		if( iShown >= 0 )
			SC_HudText( pPlayer, g_SCMats[iShown].name );
	}
}

void SC_HudText( CBasePlayer@ pPlayer, const string& in szText )
{
	HUDTextParams tp;
	tp.x = -1;
	tp.y = 0.85;
	tp.effect = 0;
	tp.r1 = 255; tp.g1 = 255; tp.b1 = 255; tp.a1 = 255;
	tp.r2 = 255; tp.g2 = 255; tp.b2 = 255; tp.a2 = 255;
	tp.fadeinTime = 0;
	tp.fadeoutTime = 0.5;
	tp.holdTime = 2.0;
	tp.fxTime = 0;
	tp.channel = SC_HUD_TEXT_CHANNEL;
	g_PlayerFuncs.HudMessage( pPlayer, tp, szText );
}

void SC_UpdateHudDelayed( EHandle hPlayer )
{
	SC_UpdateHud( cast<CBasePlayer@>( hPlayer.GetEntity() ) );
}

//
// Inventory menu
//
void SC_OpenInventory( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	if( inv.menu !is null )
		inv.menu.Unregister();
	@inv.menu = CTextMenu( @SC_InventoryMenuCallback );
	inv.menu.SetTitle( "Inventory" + ( inv.creative ? " (creative)" : "" ) + " - pick a block" );
	inv.menu.AddItem( "Crafting...", any( -2 ) );
	int iAdded = 0;
	for( int i = 0; i < MAT_COUNT; i++ )
	{
		if( !inv.Has( i ) )
			continue;
		string szItem = g_SCMats[i].name + ( inv.creative ? "" : " x" + inv.Whole( i ) ) + ( i == inv.selected ? "  <" : "" );
		inv.menu.AddItem( szItem, any( i ) );
		iAdded++;
	}
	if( iAdded == 0 )
		inv.menu.AddItem( "Empty - mine something first", any( -1 ) );
	inv.menu.Register();
	inv.menu.Open( 0, 0, pPlayer );
}

void SC_InventoryMenuCallback( CTextMenu@ menu, CBasePlayer@ pPlayer, int iSlot, const CTextMenuItem@ pItem )
{
	if( pItem is null || pPlayer is null )
		return;
	int iMat = -1;
	pItem.m_pUserData.retrieve( iMat );
	if( iMat == -2 )
	{
		g_Scheduler.SetTimeout( "SC_OpenCraftingLater", 0.05, EHandle( pPlayer ) );
		return;
	}
	if( iMat < 0 || iMat >= MAT_COUNT )
		return;
	SC_Inv( pPlayer ).selected = iMat;
	SC_UpdateHud( pPlayer );
}

CClientCommand g_SCInventoryCmd( "sc_inv", "Open the Svencraft inventory", @SC_InventoryCmd );

void SC_InventoryCmd( const CCommand@ args )
{
	CBasePlayer@ pPlayer = g_ConCommandSystem.GetCurrentPlayer();
	if( pPlayer !is null )
		SC_OpenInventory( pPlayer );
}
