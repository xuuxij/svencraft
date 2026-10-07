//
// Crafting window (Minecraft style), drawn with HUD sprites and driven by the movement/attack keys:
//   W/A/S/D move the highlight (across categories at the edges), left click crafts, right click / R / E / O close.
// Layout (pixels from the screen centre): 400x232 panel; 3x3 recipe grid on the left, ingredient column,
// arrow, result slot. HUD sprite channels 0..14 are borrowed from the hotbar while the window is open.
//
const string SC_SPR_CRAFT_SLOTS = "svencraft/craftslots.spr";   // frame k = slot with icon k, 45+k = selected
const string SC_SPR_CRAFT_PANEL_L = "svencraft/craftpanel_l.spr"; // 256x232, panel x -200..56
const string SC_SPR_CRAFT_PANEL_R = "svencraft/craftpanel_r.spr"; // 144x232, panel x 56..200
const int SC_ICON_EMPTY = 44;
const int SC_ICON_SELECTED = 45;
const int SC_CH_PANEL_L = 0, SC_CH_PANEL_R = 1, SC_CH_GRID = 2, SC_CH_INGREDIENT = 11, SC_CH_RESULT = 14;
const int SC_CRAFT_TEXT_TITLE = 1, SC_CRAFT_TEXT_INFO = 2, SC_CRAFT_TEXT_HINT = 3;

void SC_PrecacheCraftUI()
{
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_SLOTS );
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_PANEL_L );
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_PANEL_R );
	g_SoundSystem.PrecacheSound( "common/menu1.wav" );
	g_SoundSystem.PrecacheSound( "common/menu2.wav" );
}

// Icon id for a recipe's result (see craftslots.spr frame order).
void SC_AssignRecipeIcons()
{
	int iWeapon = 0, iSupply = 0;
	for( uint i = 0; i < g_SCRecipes.length(); i++ )
	{
		SCRecipe@ r = g_SCRecipes[i];
		if( r.outMat != MAT_NONE )
			r.icon = r.outMat;
		else if( r.tool >= 0 )
			r.icon = 19 + ( r.tool - TOOL_PICKAXE ) * 3 + ( r.tier - 1 );
		else if( r.category == CRAFT_WEAPONS )
			r.icon = 28 + iWeapon++;
		else
			r.icon = 36 + iSupply++;
	}
}

array<int> SC_CategoryRecipes( int iCat )
{
	array<int> list;
	for( uint i = 0; i < g_SCRecipes.length(); i++ )
	{
		if( g_SCRecipes[i].category == iCat )
			list.insertLast( int( i ) );
	}
	return list;
}

void SC_UISprite( CBasePlayer@ pPlayer, int iChannel, const string& in szSprite, int iFrame, int x, int y, bool bDim = false )
{
	HUDSpriteParams sp;
	sp.channel = iChannel;
	sp.flags = HUD_ELEM_ABSOLUTE_X | HUD_ELEM_SCR_CENTER_X | HUD_ELEM_ABSOLUTE_Y | HUD_ELEM_SCR_CENTER_Y | HUD_SPR_MASKED;
	sp.spritename = szSprite;
	sp.frame = iFrame;
	sp.x = x;
	sp.y = y;
	sp.holdTime = 99999;
	sp.color1 = bDim ? RGBA( 110, 110, 110, 255 ) : RGBA( 255, 255, 255, 255 );
	g_PlayerFuncs.HudCustomSprite( pPlayer, sp );
}

void SC_UIText( CBasePlayer@ pPlayer, int iChannel, float y, const string& in szText, int r = 255, int g = 255, int b = 255 )
{
	HUDTextParams tp;
	tp.x = -1;
	tp.y = y;
	tp.effect = 0;
	tp.r1 = r; tp.g1 = g; tp.b1 = b; tp.a1 = 255;
	tp.r2 = r; tp.g2 = g; tp.b2 = b; tp.a2 = 255;
	tp.fadeinTime = 0;
	tp.fadeoutTime = 0;
	tp.holdTime = szText == "" ? 0.05 : 99999;
	tp.fxTime = 0;
	tp.channel = iChannel;
	g_PlayerFuncs.HudMessage( pPlayer, tp, szText == "" ? " " : szText );
}

void SC_DrawCraftUI( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	bool bBench = inv.creative || SC_NearBench( pPlayer );
	array<int> list = SC_CategoryRecipes( inv.craftCat );
	if( inv.craftSel >= int( list.length() ) ) inv.craftSel = list.length() - 1;
	if( inv.craftSel < 0 ) inv.craftSel = 0;
	int iPage = inv.craftSel / 9;
	int iPages = ( list.length() + 8 ) / 9;

	SC_UISprite( pPlayer, SC_CH_PANEL_L, SC_SPR_CRAFT_PANEL_L, 0, -72, 0 );
	SC_UISprite( pPlayer, SC_CH_PANEL_R, SC_SPR_CRAFT_PANEL_R, 0, 128, 0 );
	for( int k = 0; k < 9; k++ )
	{
		int idx = iPage * 9 + k;
		int x = -164 + 56 * ( k % 3 ), y = -64 + 56 * ( k / 3 );
		if( idx >= int( list.length() ) )
		{
			SC_UISprite( pPlayer, SC_CH_GRID + k, SC_SPR_CRAFT_SLOTS, SC_ICON_EMPTY, x, y );
			continue;
		}
		SCRecipe@ r = g_SCRecipes[list[idx]];
		bool bSel = idx == inv.craftSel;
		SC_UISprite( pPlayer, SC_CH_GRID + k, SC_SPR_CRAFT_SLOTS, r.icon + ( bSel ? SC_ICON_SELECTED : 0 ), x, y, !SC_CanCraft( pPlayer, r, bBench ) );
	}

	SCRecipe@ sel = list.length() > 0 ? g_SCRecipes[list[inv.craftSel]] : null;
	string szNeeds = "";
	for( int i = 0; i < 3; i++ )
	{
		int y = -64 + 56 * i;
		if( sel is null || i >= int( sel.mats.length() ) )
		{
			SC_UISprite( pPlayer, SC_CH_INGREDIENT + i, SC_SPR_CRAFT_SLOTS, SC_ICON_EMPTY, 12, y );
			continue;
		}
		int iMat = sel.mats[i];
		bool bEnough = inv.creative || inv.Whole( iMat ) >= sel.counts[i];
		SC_UISprite( pPlayer, SC_CH_INGREDIENT + i, SC_SPR_CRAFT_SLOTS, iMat, 12, y, !bEnough );
		szNeeds += ( i > 0 ? "    " : "" ) + sel.counts[i] + " " + g_SCMats[iMat].name + ( inv.creative ? "" : " (have " + inv.Whole( iMat ) + ")" );
	}
	if( sel !is null )
		SC_UISprite( pPlayer, SC_CH_RESULT, SC_SPR_CRAFT_SLOTS, sel.icon, 132, -8, !SC_CanCraft( pPlayer, sel, bBench ) );
	else
		SC_UISprite( pPlayer, SC_CH_RESULT, SC_SPR_CRAFT_SLOTS, SC_ICON_EMPTY, 132, -8 );

	SC_UIText( pPlayer, SC_CRAFT_TEXT_TITLE, 0.28, "Crafting: " + SC_CRAFT_CATEGORY_NAMES[inv.craftCat] + "  (" + ( inv.craftCat + 1 ) + "/" + CRAFT_CATEGORY_COUNT + ")"
		+ ( iPages > 1 ? "  page " + ( iPage + 1 ) + "/" + iPages : "" ) + ( bBench ? "" : "\nNo workbench nearby - only Basic recipes" ) );
	if( sel is null )
	{
		SC_UIText( pPlayer, SC_CRAFT_TEXT_INFO, 0.7, "" );
	}
	else
	{
		string szState;
		int r = 120, g = 255, b = 120;
		if( sel.bench && !bBench ) { szState = "Needs a workbench nearby"; r = 255; g = 170; b = 80; }
		else if( !SC_RecipeUseful( inv, sel ) ) { szState = "You already have this tool tier"; r = 200; g = 200; b = 200; }
		else if( !SC_HasIngredients( inv, sel ) ) { szState = "Missing materials"; r = 255; g = 110; b = 110; }
		else szState = "Ready - left click to craft";
		SC_UIText( pPlayer, SC_CRAFT_TEXT_INFO, 0.7, sel.name + "\n" + szNeeds + "\n" + szState, r, g, b );
	}
	SC_UIText( pPlayer, SC_CRAFT_TEXT_HINT, 0.8, "W/A/S/D: choose    Left click: craft    Right click / R / O: close", 190, 190, 190 );
}

void SC_OpenCrafting( CBasePlayer@ pPlayer )
{
	if( pPlayer is null || !pPlayer.IsAlive() )
		return;
	SCInventory@ inv = SC_Inv( pPlayer );
	if( inv.craftOpen )
		return;
	inv.craftOpen = true;
	inv.lastButtons = pPlayer.pev.button;      // keys already held don't count as presses
	pPlayer.pev.flags |= FL_FROZEN;
	for( int ch = 0; ch < 16; ch++ )
		g_PlayerFuncs.HudToggleElement( pPlayer, ch, false );
	SC_DrawCraftUI( pPlayer );
	g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/menu1.wav", 0.8, ATTN_NORM );
}

void SC_CloseCrafting( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	if( !inv.craftOpen )
		return;
	inv.craftOpen = false;
	inv.craftClosedTime = g_Engine.time;
	pPlayer.pev.flags &= ~FL_FROZEN;
	for( int ch = 0; ch <= SC_CH_RESULT; ch++ )
		g_PlayerFuncs.HudToggleElement( pPlayer, ch, false );
	SC_UIText( pPlayer, SC_CRAFT_TEXT_TITLE, 0.28, "" );
	SC_UIText( pPlayer, SC_CRAFT_TEXT_INFO, 0.7, "" );
	SC_UIText( pPlayer, SC_CRAFT_TEXT_HINT, 0.8, "" );
	SC_UpdateHud( pPlayer );
}

void SC_CraftingMove( CBasePlayer@ pPlayer, int iStep )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	int n = SC_CategoryRecipes( inv.craftCat ).length();
	int iNew = inv.craftSel + iStep;
	if( iNew < 0 )
	{
		inv.craftCat = ( inv.craftCat + CRAFT_CATEGORY_COUNT - 1 ) % CRAFT_CATEGORY_COUNT;   // previous category, last item
		inv.craftSel = SC_CategoryRecipes( inv.craftCat ).length() - 1;
	}
	else if( iNew >= n )
	{
		inv.craftCat = ( inv.craftCat + 1 ) % CRAFT_CATEGORY_COUNT;                         // next category, first item
		inv.craftSel = 0;
	}
	else
		inv.craftSel = iNew;
	g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/menu2.wav", 0.5, ATTN_NORM );
	SC_DrawCraftUI( pPlayer );
}

// Runs every frame for every player: drives the crafting window while it is open.
HookReturnCode SC_CraftPreThink( CBasePlayer@ pPlayer, uint& out uiFlags )
{
	uiFlags = 0;
	if( pPlayer is null )
		return HOOK_CONTINUE;
	SCInventory@ inv = SC_Inv( pPlayer );
	if( !inv.craftOpen )
		return HOOK_CONTINUE;
	if( !pPlayer.IsAlive() )
	{
		SC_CloseCrafting( pPlayer );
		return HOOK_CONTINUE;
	}
	pPlayer.m_flNextAttack = g_WeaponFuncs.WeaponTimeBase() + 0.3;   // no shooting/mining while the window is open
	pPlayer.pev.velocity = g_vecZero;

	int iButtons = pPlayer.pev.button;
	int iPressed = iButtons & ~inv.lastButtons;
	inv.lastButtons = iButtons;
	if( iPressed == 0 )
		return HOOK_CONTINUE;

	if( ( iPressed & ( IN_ATTACK2 | IN_RELOAD | IN_USE ) ) != 0 )
	{
		SC_CloseCrafting( pPlayer );
		return HOOK_CONTINUE;
	}
	if( ( iPressed & IN_ATTACK ) != 0 )
	{
		array<int> list = SC_CategoryRecipes( inv.craftCat );
		if( inv.craftSel < int( list.length() ) )
			SC_Craft( pPlayer, list[inv.craftSel] );
		SC_DrawCraftUI( pPlayer );
		return HOOK_CONTINUE;
	}
	if( ( iPressed & IN_MOVELEFT ) != 0 ) SC_CraftingMove( pPlayer, -1 );
	else if( ( iPressed & IN_MOVERIGHT ) != 0 ) SC_CraftingMove( pPlayer, 1 );
	else if( ( iPressed & IN_FORWARD ) != 0 ) SC_CraftingMove( pPlayer, -3 );
	else if( ( iPressed & IN_BACK ) != 0 ) SC_CraftingMove( pPlayer, 3 );
	return HOOK_CONTINUE;
}

void SC_OpenCraftingLater( EHandle hPlayer )
{
	CBasePlayer@ pPlayer = cast<CBasePlayer@>( hPlayer.GetEntity() );
	if( pPlayer is null || !pPlayer.IsConnected() )
		return;
	if( g_Engine.time - SC_Inv( pPlayer ).craftClosedTime < 0.4 )
		return;   // the same key press that closed the window (e.g. +use on the workbench) must not reopen it
	SC_OpenCrafting( pPlayer );
}

CClientCommand g_SCCraftCmd( "sc_craft", "Open/close the Svencraft crafting window", @SC_CraftCmd );

void SC_CraftCmd( const CCommand@ args )
{
	CBasePlayer@ pPlayer = g_ConCommandSystem.GetCurrentPlayer();
	if( pPlayer is null )
		return;
	if( SC_Inv( pPlayer ).craftOpen )
		SC_CloseCrafting( pPlayer );
	else
		SC_OpenCrafting( pPlayer );
}
