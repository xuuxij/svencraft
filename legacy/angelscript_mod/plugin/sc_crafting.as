// Crafting: recipes turn inventory blocks into blocks, better tools, Half-Life weapons, ammo and items.
// Basic recipes work anywhere; the rest need a placed Workbench nearby (or creative mode).
// Recipe ideas follow LambdaCraft's HL-gear-from-materials design (MIT), using Svencraft's own materials.

enum SCCraftCategory
{
	CRAFT_BASIC = 0,
	CRAFT_BLOCKS,
	CRAFT_TOOLS,
	CRAFT_WEAPONS,
	CRAFT_SUPPLIES,
	CRAFT_CATEGORY_COUNT
}
const array<string> SC_CRAFT_CATEGORY_NAMES = { "Basic", "Blocks", "Tools", "Weapons", "Ammo & Items" };
const float SC_BENCH_RANGE = 160;   // ~4 blocks

class SCRecipe
{
	string name;
	int category;
	bool bench;
	array<int> mats;
	array<int> counts;
	int outMat = MAT_NONE;   // block output
	int outCount = 0;
	int tool = -1;           // tool upgrade output
	int tier = 0;
	string item;             // entity output (weapon_*, ammo_*, item_*)
	int itemCount = 0;
	int icon = 0;            // craftslots.spr icon id (SC_AssignRecipeIcons)

	string Ingredients()
	{
		string s = "";
		for( uint i = 0; i < mats.length(); i++ )
			s += ( i > 0 ? ", " : "" ) + counts[i] + " " + g_SCMats[mats[i]].name;
		return s;
	}
}

array<SCRecipe@> g_SCRecipes;

SCRecipe@ SC_Recipe( int iCategory, const string& in szName, bool bBench, const string& in szIngredients )
{
	SCRecipe r;
	r.name = szName;
	r.category = iCategory;
	r.bench = bBench;
	array<string> parts = SC_Split( szIngredients, "," );
	for( uint i = 0; i < parts.length(); i++ )
	{
		array<string> nm = SC_Split( parts[i], ":" );
		int iMat = SC_MatByName( nm[0] );
		if( iMat == MAT_NONE )
		{
			g_Game.AlertMessage( at_console, "Svencraft: bad recipe ingredient '" + nm[0] + "' in " + szName + "\n" );
			continue;
		}
		r.mats.insertLast( iMat );
		r.counts.insertLast( nm.length() > 1 ? atoi( nm[1] ) : 1 );
	}
	g_SCRecipes.insertLast( r );
	return r;
}

void SC_BlockRecipe( int iCat, bool bBench, const string& in szIn, int iOut, int iCount )
{
	SCRecipe@ r = SC_Recipe( iCat, g_SCMats[iOut].name + ( iCount > 1 ? " x" + iCount : "" ), bBench, szIn );
	r.outMat = iOut;
	r.outCount = iCount;
}

void SC_ToolRecipe( int iTool, int iTier, const string& in szIn )
{
	SCRecipe@ r = SC_Recipe( CRAFT_TOOLS, SC_TIER_NAMES[iTier] + " " + SC_ToolName( iTool ), true, szIn );
	r.tool = iTool;
	r.tier = iTier;
}

void SC_ItemRecipe( int iCat, const string& in szName, const string& in szIn, const string& in szItem, int iCount = 1 )
{
	SCRecipe@ r = SC_Recipe( iCat, szName, true, szIn );
	r.item = szItem;
	r.itemCount = iCount;
}

void SC_InitRecipes()
{
	g_SCRecipes.resize( 0 );
	// Basic: hand crafting, no workbench needed
	SC_BlockRecipe( CRAFT_BASIC, false, "log:1", MAT_PLANKS, 4 );
	SC_BlockRecipe( CRAFT_BASIC, false, "planks:4", MAT_WORKBENCH, 1 );

	// Blocks (smelting and mixing stand-ins)
	SC_BlockRecipe( CRAFT_BLOCKS, true, "cobble:1", MAT_STONE, 1 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "sand:1", MAT_GLASS, 1 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "cobble:1,dirt:1", MAT_BRICK, 2 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "gravel:2,sand:2", MAT_CONCRETE, 4 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "concrete:2", MAT_TILE, 2 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "cobble:1", MAT_GRAVEL, 2 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "leaves:4", MAT_RUBBER, 1 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "metal:1", MAT_VENT, 2 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "metal:1,glass:1", MAT_CIRCUIT, 1 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "glass:6", MAT_GLASS_PANE, 16 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "planks:6", MAT_DOOR, 3 );

	// Tool upgrades (head material + plank handle)
	SC_ToolRecipe( TOOL_PICKAXE, 2, "cobble:3,planks:2" );
	SC_ToolRecipe( TOOL_SHOVEL, 2, "cobble:1,planks:2" );
	SC_ToolRecipe( TOOL_AXE, 2, "cobble:3,planks:2" );
	SC_ToolRecipe( TOOL_PICKAXE, 3, "metal:3,planks:2" );
	SC_ToolRecipe( TOOL_SHOVEL, 3, "metal:1,planks:2" );
	SC_ToolRecipe( TOOL_AXE, 3, "metal:3,planks:2" );

	// Half-Life weapons
	SC_ItemRecipe( CRAFT_WEAPONS, "Crowbar", "metal:2", "weapon_crowbar" );
	SC_ItemRecipe( CRAFT_WEAPONS, "9mm Handgun", "metal:3,circuit:1", "weapon_9mmhandgun" );
	SC_ItemRecipe( CRAFT_WEAPONS, ".357 Magnum", "metal:4,circuit:1", "weapon_357" );
	SC_ItemRecipe( CRAFT_WEAPONS, "Shotgun", "metal:4,planks:2", "weapon_shotgun" );
	SC_ItemRecipe( CRAFT_WEAPONS, "MP5", "metal:5,circuit:2", "weapon_9mmAR" );
	SC_ItemRecipe( CRAFT_WEAPONS, "Crossbow", "planks:4,metal:2,rubber:2", "weapon_crossbow" );
	SC_ItemRecipe( CRAFT_WEAPONS, "Hand Grenade", "metal:1,sand:2", "weapon_handgrenade" );
	SC_ItemRecipe( CRAFT_WEAPONS, "RPG Launcher", "metal:8,circuit:4", "weapon_rpg" );

	// Ammo and supplies
	SC_ItemRecipe( CRAFT_SUPPLIES, "9mm Clip", "metal:1", "ammo_9mmclip" );
	SC_ItemRecipe( CRAFT_SUPPLIES, ".357 Rounds", "metal:1", "ammo_357" );
	SC_ItemRecipe( CRAFT_SUPPLIES, "Shotgun Shells", "metal:1,sand:1", "ammo_buckshot" );
	SC_ItemRecipe( CRAFT_SUPPLIES, "MP5 Magazine", "metal:2", "ammo_9mmAR" );
	SC_ItemRecipe( CRAFT_SUPPLIES, "Crossbow Bolts", "planks:2,metal:1", "ammo_crossbow" );
	SC_ItemRecipe( CRAFT_SUPPLIES, "RPG Rocket", "metal:2,sand:2,circuit:1", "ammo_rpgclip" );
	SC_ItemRecipe( CRAFT_SUPPLIES, "Health Kit", "leaves:3,rubber:1", "item_healthkit" );
	SC_ItemRecipe( CRAFT_SUPPLIES, "Armor Battery", "circuit:1,metal:1", "item_battery" );

	SC_AssignRecipeIcons();
}

// Weapons/items must be precached at map start or giving them later crashes the server.
void SC_PrecacheCrafting()
{
	for( uint i = 0; i < g_SCRecipes.length(); i++ )
	{
		if( g_SCRecipes[i].item != "" )
			g_Game.PrecacheOther( g_SCRecipes[i].item );
	}
	g_SoundSystem.PrecacheSound( "items/gunpickup2.wav" );
	g_SoundSystem.PrecacheSound( "common/wpn_denyselect.wav" );
	SC_PrecacheCraftUI();
}

bool SC_NearBench( CBasePlayer@ pPlayer )
{
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityInSphere( pEnt, pPlayer.pev.origin, SC_BENCH_RANGE, "*", "classname" ) ) !is null )
	{
		string szClass = pEnt.GetClassname();
		if( ( szClass == "sc_node" || szClass == "sc_block" ) && pEnt.pev.iuser1 == MAT_WORKBENCH )
			return true;
	}
	return false;
}

bool SC_HasIngredients( SCInventory@ inv, SCRecipe@ r )
{
	if( inv.creative )
		return true;
	for( uint i = 0; i < r.mats.length(); i++ )
	{
		if( inv.Whole( r.mats[i] ) < r.counts[i] )
			return false;
	}
	return true;
}

bool SC_RecipeUseful( SCInventory@ inv, SCRecipe@ r )
{
	return r.tool < 0 || inv.toolTier[r.tool] < r.tier;
}

bool SC_CanCraft( CBasePlayer@ pPlayer, SCRecipe@ r, bool bBench )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	return ( !r.bench || bBench ) && SC_HasIngredients( inv, r ) && SC_RecipeUseful( inv, r );
}

void SC_Craft( CBasePlayer@ pPlayer, int iRecipe )
{
	SCRecipe@ r = g_SCRecipes[iRecipe];
	SCInventory@ inv = SC_Inv( pPlayer );
	bool bBench = inv.creative || SC_NearBench( pPlayer );

	string szProblem = "";
	if( r.bench && !bBench )
		szProblem = "Needs a Workbench nearby";
	else if( !SC_RecipeUseful( inv, r ) )
		szProblem = "You already have a " + SC_TIER_NAMES[inv.toolTier[r.tool]] + " " + SC_ToolName( r.tool );
	else if( !SC_HasIngredients( inv, r ) )
		szProblem = "Needs " + r.Ingredients();
	if( szProblem != "" )
	{
		SC_Print( pPlayer, szProblem );
		g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/wpn_denyselect.wav", 1, ATTN_NORM );
		return;
	}

	if( !inv.creative )
	{
		for( uint i = 0; i < r.mats.length(); i++ )
		{
			inv.count[r.mats[i]] -= r.counts[i];
			if( inv.count[r.mats[i]] < 0.001 )
				inv.count[r.mats[i]] = 0;
		}
	}

	if( r.outMat != MAT_NONE )
		inv.Add( r.outMat, r.outCount );
	if( r.tool >= 0 )
	{
		inv.toolTier[r.tool] = r.tier;
		SC_GiveTools( EHandle( pPlayer ) );
		SC_RefreshHeldTool( pPlayer );
	}
	for( int i = 0; i < r.itemCount; i++ )
		pPlayer.GiveNamedItem( r.item );

	g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	SC_Print( pPlayer, "Crafted " + r.name );
	SC_UpdateHud( pPlayer );
}

//
// Crafting window (Minecraft style), drawn with HUD sprites and driven by the movement/attack keys:
//   W/A/S/D move the highlight (across categories at the edges), left click crafts, right click / R / E / O close.
// Layout (pixels from the screen centre): 400x232 panel; 3x3 recipe grid on the left, ingredient column,
// arrow, result slot. HUD sprite channels 0..14 are borrowed from the hotbar while the window is open.
//
const string SC_SPR_CRAFT_SLOTS = "svencraft/craftslots.spr";   // frame k = slot with icon k, 45+k = selected
const string SC_SPR_CRAFT_PANEL_L = "svencraft/craftpanel_l.spr"; // 256x232, panel x -200..56
const string SC_SPR_CRAFT_PANEL_R = "svencraft/craftpanel_r.spr"; // 144x232, panel x 56..200
// craftslots.spr icons: blocks 0..MAT_COUNT-1, then 9 tools, 8 weapons, 8 supplies, then the empty slot.
const int SC_ICON_TOOLS = MAT_COUNT;
const int SC_ICON_WEAPONS = MAT_COUNT + 9;
const int SC_ICON_SUPPLIES = MAT_COUNT + 17;
const int SC_ICON_EMPTY = MAT_COUNT + 25;
const int SC_ICON_SELECTED = MAT_COUNT + 26;   // selected-slot variants start here
const int SC_CH_PANEL_L = 0, SC_CH_PANEL_R = 1, SC_CH_GRID = 2, SC_CH_INGREDIENT = 11, SC_CH_RESULT = 14;
const int SC_CRAFT_TEXT_TITLE = 1, SC_CRAFT_TEXT_INFO = 2, SC_CRAFT_TEXT_HINT = 3;
const string SC_SPR_CURSOR = "svencraft/cursor.spr";               // 32x32, arrow tip at the centre
const int SC_CH_CURSOR = 15;
const float SC_CURSOR_PX_PER_DEGREE = 8;                           // mouse sensitivity of the emulated cursor
// Clickable areas (pixels from the screen centre): result slot and the category arrow buttons.
const int SC_HIT_NONE = -1, SC_HIT_RESULT = -2, SC_HIT_PREV = -3, SC_HIT_NEXT = -4;

void SC_PrecacheCraftUI()
{
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_SLOTS );
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_PANEL_L );
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_PANEL_R );
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CURSOR );
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
			r.icon = SC_ICON_TOOLS + ( r.tool - TOOL_PICKAXE ) * 3 + ( r.tier - 1 );
		else if( r.category == CRAFT_WEAPONS )
			r.icon = SC_ICON_WEAPONS + iWeapon++;
		else
			r.icon = SC_ICON_SUPPLIES + iSupply++;
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
	SC_UIText( pPlayer, SC_CRAFT_TEXT_HINT, 0.8, "Mouse: point    Left click: select / craft (click result or click again)    < >: category    Right click / R / O: close", 190, 190, 190 );
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
	// Hold the player still by speed, not FL_FROZEN: a frozen player's view angles stop reaching the
	// server, and the emulated cursor is driven by them.
	inv.savedMaxspeed = pPlayer.pev.maxspeed;
	inv.craftAngles = pPlayer.pev.v_angle;
	inv.cursorX = -108;                        // start over the middle of the recipe grid
	inv.cursorY = -8;
	for( int ch = 0; ch < 16; ch++ )
		g_PlayerFuncs.HudToggleElement( pPlayer, ch, false );
	SC_DrawCraftUI( pPlayer );
	SC_DrawCursor( pPlayer );
	g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/menu1.wav", 0.8, ATTN_NORM );
}

void SC_CloseCrafting( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	if( !inv.craftOpen )
		return;
	inv.craftOpen = false;
	inv.craftClosedTime = g_Engine.time;
	pPlayer.pev.maxspeed = inv.savedMaxspeed;
	inv.snapPending = false;
	for( int ch = 0; ch <= SC_CH_CURSOR; ch++ )
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
	pPlayer.pev.maxspeed = 1;                                          // effectively can't walk
	pPlayer.pev.velocity.x = 0;
	pPlayer.pev.velocity.y = 0;
	SC_CraftMouse( pPlayer );

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
		SC_CraftClick( pPlayer );
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

//
// Emulated mouse cursor: the view is held still while the window is open, and the mouse movement that
// would have turned it moves the cursor instead.
//
float SC_AngleDiff( float a, float b )
{
	float d = a - b;
	while( d > 180 ) d -= 360;
	while( d < -180 ) d += 360;
	return d;
}

void SC_DrawCursor( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	SC_UISprite( pPlayer, SC_CH_CURSOR, SC_SPR_CURSOR, 0, int( inv.cursorX ), int( inv.cursorY ) );
	inv.cursorSentTime = g_Engine.time;
}

// What is under the cursor: 0..8 = recipe grid slot, or one of SC_HIT_*.
int SC_CraftHit( SCInventory@ inv )
{
	float x = inv.cursorX, y = inv.cursorY;
	for( int k = 0; k < 9; k++ )
	{
		float sx = -164 + 56 * ( k % 3 ), sy = -64 + 56 * ( k / 3 );
		if( SC_Abs( x - sx ) <= 28 && SC_Abs( y - sy ) <= 28 )
			return k;
	}
	if( SC_Abs( x - 132 ) <= 28 && SC_Abs( y + 8 ) <= 28 ) return SC_HIT_RESULT;
	if( SC_Abs( x + 176 ) <= 15 && SC_Abs( y - 98 ) <= 13 ) return SC_HIT_PREV;
	if( SC_Abs( x + 40 ) <= 15 && SC_Abs( y - 98 ) <= 13 ) return SC_HIT_NEXT;
	return SC_HIT_NONE;
}

// Recipe index (within the current category) for a grid slot on the current page, or -1.
int SC_GridRecipe( SCInventory@ inv, int iSlot )
{
	int n = SC_CategoryRecipes( inv.craftCat ).length();
	int idx = ( inv.craftSel / 9 ) * 9 + iSlot;
	return idx < n ? idx : -1;
}

void SC_CraftClick( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	int iHit = SC_CraftHit( inv );
	if( iHit >= 0 )
	{
		int idx = SC_GridRecipe( inv, iHit );
		if( idx < 0 )
			return;
		if( idx != inv.craftSel )
		{
			inv.craftSel = idx;                  // first click selects ...
			g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/menu2.wav", 0.5, ATTN_NORM );
			SC_DrawCraftUI( pPlayer );
			return;
		}
		iHit = SC_HIT_RESULT;                    // ... clicking the selected recipe again crafts it
	}
	if( iHit == SC_HIT_RESULT )
	{
		array<int> list = SC_CategoryRecipes( inv.craftCat );
		if( inv.craftSel < int( list.length() ) )
			SC_Craft( pPlayer, list[inv.craftSel] );
		SC_DrawCraftUI( pPlayer );
	}
	else if( iHit == SC_HIT_PREV || iHit == SC_HIT_NEXT )
	{
		inv.craftCat = ( inv.craftCat + ( iHit == SC_HIT_NEXT ? 1 : CRAFT_CATEGORY_COUNT - 1 ) ) % CRAFT_CATEGORY_COUNT;
		inv.craftSel = 0;
		g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/menu2.wav", 0.5, ATTN_NORM );
		SC_DrawCraftUI( pPlayer );
	}
}

// Mouse movement that would have turned the view moves the cursor instead; the view is snapped back.
void SC_CraftMouse( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	float dYaw = SC_AngleDiff( pPlayer.pev.v_angle.y, inv.craftAngles.y );
	float dPitch = SC_AngleDiff( pPlayer.pev.v_angle.x, inv.craftAngles.x );
	if( SC_Abs( dYaw ) + SC_Abs( dPitch ) < 0.01 )
		return;

	inv.cursorX = inv.cursorX - dYaw * SC_CURSOR_PX_PER_DEGREE;
	inv.cursorY = inv.cursorY + dPitch * SC_CURSOR_PX_PER_DEGREE;
	if( inv.cursorX < -200 ) inv.cursorX = -200;
	if( inv.cursorX > 200 ) inv.cursorX = 200;
	if( inv.cursorY < -116 ) inv.cursorY = -116;
	if( inv.cursorY > 116 ) inv.cursorY = 116;

	pPlayer.pev.angles = inv.craftAngles;
	pPlayer.pev.v_angle = inv.craftAngles;
	pPlayer.pev.fixangle = 1;                    // FAM_FORCEVIEWANGLES: client view snaps back
	inv.snapPending = true;

	int iHit = SC_CraftHit( inv );
	int idx = iHit >= 0 ? SC_GridRecipe( inv, iHit ) : -1;
	if( idx >= 0 && idx != inv.craftSel )
	{
		inv.craftSel = idx;                      // hovering a recipe highlights it
		SC_DrawCraftUI( pPlayer );
	}
	if( g_Engine.time - inv.cursorSentTime >= 0.02 )
		SC_DrawCursor( pPlayer );
}

// The player's own think rewrites pev.angles from the view; restore the held view before it's sent.
HookReturnCode SC_CraftPostThink( CBasePlayer@ pPlayer )
{
	if( pPlayer is null )
		return HOOK_CONTINUE;
	SCInventory@ inv = SC_Inv( pPlayer );
	if( !inv.craftOpen )
		return HOOK_CONTINUE;
	SC_CraftMouse( pPlayer );
	if( inv.snapPending )
	{
		pPlayer.pev.angles = inv.craftAngles;
		pPlayer.pev.v_angle = inv.craftAngles;
		pPlayer.pev.fixangle = 1;
		inv.snapPending = false;
	}
	return HOOK_CONTINUE;
}
