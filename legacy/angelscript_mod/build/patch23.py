import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (name, a[:80])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='').write(s)

patch('sc_inventory.as', [
    ("	float craftClosedTime = 0;", "	float craftClosedTime = 0;\n	float cursorX = 0;          // emulated mouse cursor, pixels from the screen centre\n	float cursorY = 0;\n	Vector craftAngles;         // view held here while the window is open\n	float cursorSentTime = 0;"),
])

patch('sc_crafting.as', [
    # constants + precache
    ("const int SC_CRAFT_TEXT_TITLE = 1, SC_CRAFT_TEXT_INFO = 2, SC_CRAFT_TEXT_HINT = 3;",
     """const int SC_CRAFT_TEXT_TITLE = 1, SC_CRAFT_TEXT_INFO = 2, SC_CRAFT_TEXT_HINT = 3;
const string SC_SPR_CURSOR = "svencraft/cursor.spr";               // 32x32, arrow tip at the centre
const int SC_CH_CURSOR = 15;
const float SC_CURSOR_PX_PER_DEGREE = 8;                           // mouse sensitivity of the emulated cursor
// Clickable areas (pixels from the screen centre): result slot and the category arrow buttons.
const int SC_HIT_NONE = -1, SC_HIT_RESULT = -2, SC_HIT_PREV = -3, SC_HIT_NEXT = -4;"""),
    ("""	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_PANEL_R );""",
     """	g_Game.PrecacheModel( "sprites/" + SC_SPR_CRAFT_PANEL_R );
	g_Game.PrecacheModel( "sprites/" + SC_SPR_CURSOR );"""),
    # hint text
    ('''	SC_UIText( pPlayer, SC_CRAFT_TEXT_HINT, 0.8, "W/A/S/D: choose    Left click: craft    Right click / R / O: close", 190, 190, 190 );''',
     '''	SC_UIText( pPlayer, SC_CRAFT_TEXT_HINT, 0.8, "Mouse: point    Left click: select / craft (click result or click again)    < >: category    Right click / R / O: close", 190, 190, 190 );'''),
    # open: remember the view, place the cursor
    ("""	inv.lastButtons = pPlayer.pev.button;      // keys already held don't count as presses
	pPlayer.pev.flags |= FL_FROZEN;""",
     """	inv.lastButtons = pPlayer.pev.button;      // keys already held don't count as presses
	pPlayer.pev.flags |= FL_FROZEN;
	inv.craftAngles = pPlayer.pev.v_angle;
	inv.cursorX = -108;                        // start over the middle of the recipe grid
	inv.cursorY = -8;"""),
    ("""	SC_DrawCraftUI( pPlayer );
	g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/menu1.wav", 0.8, ATTN_NORM );""",
     """	SC_DrawCraftUI( pPlayer );
	SC_DrawCursor( pPlayer );
	g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/menu1.wav", 0.8, ATTN_NORM );"""),
    # close: also hide the cursor
    ("""	for( int ch = 0; ch <= SC_CH_RESULT; ch++ )
		g_PlayerFuncs.HudToggleElement( pPlayer, ch, false );""",
     """	for( int ch = 0; ch <= SC_CH_CURSOR; ch++ )
		g_PlayerFuncs.HudToggleElement( pPlayer, ch, false );"""),
    # left click goes through the cursor
    ("""	if( ( iPressed & IN_ATTACK ) != 0 )
	{
		array<int> list = SC_CategoryRecipes( inv.craftCat );
		if( inv.craftSel < int( list.length() ) )
			SC_Craft( pPlayer, list[inv.craftSel] );
		SC_DrawCraftUI( pPlayer );
		return HOOK_CONTINUE;
	}""",
     """	if( ( iPressed & IN_ATTACK ) != 0 )
	{
		SC_CraftClick( pPlayer );
		return HOOK_CONTINUE;
	}"""),
])

# cursor functions appended
p = os.path.join(D, 'sc_crafting.as')
s = open(p, newline='').read()
s += '''
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

// After the player's own think (which sets pev.angles from the view): turn mouse movement into cursor
// movement, then snap the view back.
HookReturnCode SC_CraftPostThink( CBasePlayer@ pPlayer )
{
	if( pPlayer is null )
		return HOOK_CONTINUE;
	SCInventory@ inv = SC_Inv( pPlayer );
	if( !inv.craftOpen )
		return HOOK_CONTINUE;
	float dYaw = SC_AngleDiff( pPlayer.pev.v_angle.y, inv.craftAngles.y );
	float dPitch = SC_AngleDiff( pPlayer.pev.v_angle.x, inv.craftAngles.x );
	if( SC_Abs( dYaw ) + SC_Abs( dPitch ) < 0.01 )
		return HOOK_CONTINUE;

	inv.cursorX = inv.cursorX - dYaw * SC_CURSOR_PX_PER_DEGREE;
	inv.cursorY = inv.cursorY + dPitch * SC_CURSOR_PX_PER_DEGREE;
	if( inv.cursorX < -200 ) inv.cursorX = -200;
	if( inv.cursorX > 200 ) inv.cursorX = 200;
	if( inv.cursorY < -116 ) inv.cursorY = -116;
	if( inv.cursorY > 116 ) inv.cursorY = 116;

	pPlayer.pev.angles = inv.craftAngles;
	pPlayer.pev.v_angle = inv.craftAngles;
	pPlayer.pev.fixangle = 1;                    // FAM_FORCEVIEWANGLES: client view snaps back

	// Hovering a recipe highlights it.
	int iHit = SC_CraftHit( inv );
	int idx = iHit >= 0 ? SC_GridRecipe( inv, iHit ) : -1;
	if( idx >= 0 && idx != inv.craftSel )
	{
		inv.craftSel = idx;
		SC_DrawCraftUI( pPlayer );
	}
	if( g_Engine.time - inv.cursorSentTime >= 0.02 )
		SC_DrawCursor( pPlayer );
	return HOOK_CONTINUE;
}
'''
open(p, 'w', newline='', encoding='utf-8').write(s)

patch('svencraft.as', [
    ("	g_Hooks.RegisterHook( Hooks::Player::PlayerPreThink, @SC_CraftPreThink );\n",
     "	g_Hooks.RegisterHook( Hooks::Player::PlayerPreThink, @SC_CraftPreThink );\n	g_Hooks.RegisterHook( Hooks::Player::PlayerPostThink, @SC_CraftPostThink );\n"),
])
print('ok')
