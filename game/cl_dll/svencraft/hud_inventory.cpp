/*
hud_inventory.cpp - Svencraft: the inventory screens (inventory with its 2x2 grid, crafting table, furnace)

Opened with "sc_inventory" (K) or by the server (E or right-click on a crafting table / furnace). While open,
mouse look moves a cursor instead of the view and the buttons click slots; every click goes to the server
("sc_click <slot> <button> <shift>"), which applies Minecraft's rules and sends the inventory back. On the left,
the recipe book: every recipe, dimmed when the ingredients aren't there; clicking one lays it out in the grid
(shift: as many as possible).
*/
#include "hud.h"
#include "cl_util.h"
#include "keydefs.h"
#include <string.h>
#include <stdio.h>
#include "sc_client.h"

#define UI_SLOT		52	// slot pitch; icons are 48
#define UI_PAD		14
#define UI_W		( UI_PAD * 2 + 9 * UI_SLOT )
#define UI_H		434
#define UI_BOOK_W	( UI_PAD * 2 + 5 * UI_SLOT )
#define UI_GAP		10
#define UI_RECIPE	1000	// hit-test ids of recipe book entries
#define UI_NONE		-2	// inside a window, not on a slot

class CHudSCInventory : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );

	bool m_bOpen;
	float m_flX, m_flY;	// the cursor
	bool m_bShift, m_bCtrl;
	float m_flLastClick;	// a second left click on the same slot soon after: a double click
	int m_iLastClick;
	int m_iDragButton;	// a press with something on the mouse: the slots it is dragged across (-1: none)
	int m_iDrag[64], m_nDrag;
	HSPRITE m_hItems, m_hCounts, m_hCursor, m_hFont;
	screcipe_t m_Recipes[SC_NUM_RECIPES];

	void Origin( int &px, int &py, int &bx, int &by );
	bool SlotRect( int slot, int &x, int &y );
	int HitTest( int mx, int my );
	void Click( int button );
	void ClickAt( int hit, int button );
	void Press( int button );
	bool CanSpread( int slot );
	void DragOver( int slot );
	void EndDrag( void );
	void Hotkey( int n );
	void Throw( bool all );
	bool CanMake( int r );
	const scclslot_t *SlotData( int slot );
	void DrawPanel( int x, int y, int w, int h );
	void DrawSlot( int x, int y, const scclslot_t *s, bool big );
	void DrawItem( int x, int y, const scclslot_t *s );
	void DrawText( int x, int y, const char *s, int r, int g, int b, bool shadow = false );
	void DrawTooltip( int x, int y, const char **lines, int n );
	void DrawArrow( int x, int y, float fill );
};

static CHudSCInventory g_SCUI;

bool SC_UIActive( void )
{
	return g_SCUI.m_bOpen;
}

void SC_UIScreenChanged( void )
{
	bool open = g_SCInv.screen != SCS_NONE;
	if( open && !g_SCUI.m_bOpen )
	{
		g_SCUI.m_flX = ScreenWidth * 0.5f;
		g_SCUI.m_flY = ScreenHeight * 0.5f;
	}
	g_SCUI.m_bOpen = open;
}

void SC_UIMouse( float relyaw, float relpitch )
{
	// look deltas are pixels times m_yaw / m_pitch (0.022)
	g_SCUI.m_flX -= relyaw / 0.022f;
	g_SCUI.m_flY += relpitch / 0.022f;
	g_SCUI.m_flX = Q_max( 0.0f, Q_min( (float)ScreenWidth - 1, g_SCUI.m_flX ));
	g_SCUI.m_flY = Q_max( 0.0f, Q_min( (float)ScreenHeight - 1, g_SCUI.m_flY ));
}

static void SC_CloseUI( void )
{
	g_SCUI.m_bOpen = false;
	g_SCUI.m_iDragButton = -1;
	gEngfuncs.pfnServerCmd( "sc_screen 0" );
}

// K: the inventory
static void SC_Cmd_Inventory( void )
{
	if( g_SCUI.m_bOpen )
		SC_CloseUI();
	else
		gEngfuncs.pfnServerCmd( "sc_screen 1" );
}

int SC_UIKey( int down, int keynum, const char *binding )
{
	if( !g_SCUI.m_bOpen )
		return 0;
	if( keynum == K_SHIFT )
		g_SCUI.m_bShift = down != 0;
	if( keynum == K_CTRL )
		g_SCUI.m_bCtrl = down != 0;
	if( !down )
	{
		if(( keynum == K_MOUSE1 && g_SCUI.m_iDragButton == 0 ) || ( keynum == K_MOUSE2 && g_SCUI.m_iDragButton == 1 ))
			g_SCUI.EndDrag();
		return 0;	// releases go through, so buttons held when the screen opened come back up
	}
	if( binding && ( !strcmp( binding, "toggleconsole" ) || !strcmp( binding, "snapshot" ) || !strcmp( binding, "screenshot" )))
		return 0;
	if( keynum == K_ESCAPE || ( binding && ( !strcmp( binding, "sc_inventory" ) || !strcmp( binding, "+use" ))))
	{
		SC_CloseUI();
		return 1;
	}
	if( keynum >= '1' && keynum <= '9' )
		g_SCUI.Hotkey( keynum - '1' );		// Minecraft's: swap the slot under the cursor with that hotbar slot
	else if( binding && !strcmp( binding, "drop" ))
		g_SCUI.Throw( g_SCUI.m_bShift || g_SCUI.m_bCtrl );	// Minecraft's Q (Ctrl+Q: the stack)
	else if( keynum == K_MOUSE1 )
		g_SCUI.Press( 0 );
	else if( keynum == K_MOUSE2 )
		g_SCUI.Press( 1 );
	return 1;
}

// test helpers: move the cursor to a slot (or x y), and click there
static void SC_Cmd_UIPoint( void )
{
	if( gEngfuncs.Cmd_Argc() >= 3 )
	{
		g_SCUI.m_flX = atof( gEngfuncs.Cmd_Argv( 1 ));
		g_SCUI.m_flY = atof( gEngfuncs.Cmd_Argv( 2 ));
		return;
	}
	int x, y;
	if( gEngfuncs.Cmd_Argc() >= 2 && g_SCUI.SlotRect( atoi( gEngfuncs.Cmd_Argv( 1 )), x, y ))
	{
		g_SCUI.m_flX = x + 24;
		g_SCUI.m_flY = y + 24;
	}
}

// sc_ui_drag <button> <slot> <slot> ...: press over the first slot, move across the others, release (UI tests)
static void SC_Cmd_UIDrag( void )
{
	int x, y, n = gEngfuncs.Cmd_Argc();
	if( !g_SCUI.m_bOpen || n < 3 )
		return;
	int button = atoi( gEngfuncs.Cmd_Argv( 1 ));
	for( int a = 2; a < n; a++ )
	{
		if( !g_SCUI.SlotRect( atoi( gEngfuncs.Cmd_Argv( a )), x, y ))
			continue;
		g_SCUI.m_flX = x + 24;
		g_SCUI.m_flY = y + 24;
		if( a == 2 )
			g_SCUI.Press( button );
		else
			g_SCUI.DragOver( g_SCUI.HitTest( x + 24, y + 24 ));
	}
	if( g_SCUI.m_iDragButton >= 0 )
		g_SCUI.EndDrag();
}

static void SC_Cmd_UIClick( void )
{
	if( !g_SCUI.m_bOpen )
		return;
	bool shift = g_SCUI.m_bShift;
	g_SCUI.m_bShift = gEngfuncs.Cmd_Argc() >= 3 && atoi( gEngfuncs.Cmd_Argv( 2 ));
	g_SCUI.Click( gEngfuncs.Cmd_Argc() >= 2 ? atoi( gEngfuncs.Cmd_Argv( 1 )) : 0 );
	g_SCUI.m_bShift = shift;
}

void SC_UIInit( void )
{
	g_SCUI.Init();
}

void SC_UIVidInit( void )
{
	g_SCUI.VidInit();
}

int CHudSCInventory::Init( void )
{
	m_bOpen = false;
	m_bShift = false;
	m_bCtrl = false;
	m_iDragButton = -1;
	m_nDrag = 0;
	m_iLastClick = UI_NONE;
	m_flLastClick = 0.0f;
	for( int i = 0; i < SC_NUM_RECIPES; i++ )
		SC_ParseRecipe( &g_SCRecipeDefs[i], &m_Recipes[i] );
	gEngfuncs.pfnAddCommand( "sc_inventory", SC_Cmd_Inventory );
	gEngfuncs.pfnAddCommand( "sc_ui_point", SC_Cmd_UIPoint );
	gEngfuncs.pfnAddCommand( "sc_ui_click", SC_Cmd_UIClick );
	gEngfuncs.pfnAddCommand( "sc_ui_drag", SC_Cmd_UIDrag );
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );
	return 1;
}

int CHudSCInventory::VidInit( void )
{
	m_hItems = SPR_Load( "sprites/svencraft/items.spr" );
	m_hCounts = SPR_Load( "sprites/svencraft/counts.spr" );
	m_hCursor = SPR_Load( "sprites/svencraft/cursor.spr" );
	m_hFont = SPR_Load( "sprites/svencraft/font.spr" );
	m_bOpen = false;
	return 1;
}

//
// layout
//
void CHudSCInventory::Origin( int &px, int &py, int &bx, int &by )
{
	int total = UI_BOOK_W + UI_GAP + UI_W;
	bx = ( ScreenWidth - total ) / 2;
	px = bx + UI_BOOK_W + UI_GAP;
	py = by = ( ScreenHeight - UI_H ) / 2;
}

// top-left of a slot's 48x48 icon area
bool CHudSCInventory::SlotRect( int slot, int &x, int &y )
{
	int px, py, bx, by;
	Origin( px, py, bx, by );
	int top = py + 26;
	if( slot >= 0 && slot < SC_INV_SLOTS )
	{
		int row = slot < SC_HOTBAR_SLOTS ? 4 : 1 + ( slot - SC_HOTBAR_SLOTS ) / 9;	// rows 1..3, hotbar below
		int col = slot % 9;
		x = px + UI_PAD + col * UI_SLOT + 2;
		y = row < 4 ? py + 204 + ( row - 1 ) * UI_SLOT + 2 : py + 368 + 2;
		return true;
	}
	if( slot >= SCSLOT_GRID && slot < SCSLOT_GRID + 9 )
	{
		int cell = slot - SCSLOT_GRID, cx = cell % 3, cy = cell / 3;
		if( g_SCInv.screen == SCS_TABLE )
		{
			x = px + 60 + cx * UI_SLOT + 2;
			y = top + cy * UI_SLOT + 2;
			return true;
		}
		if( g_SCInv.screen == SCS_INVENTORY && cx < 2 && cy < 2 )
		{
			x = px + 230 + cx * UI_SLOT + 2;
			y = top + 26 + cy * UI_SLOT + 2;
			return true;
		}
		return false;
	}
	if( slot == SCSLOT_RESULT )
	{
		if( g_SCInv.screen == SCS_TABLE ) { x = px + 330; y = top + UI_SLOT + 2; return true; }
		if( g_SCInv.screen == SCS_INVENTORY ) { x = px + 410; y = top + 26 + 26 + 2; return true; }
		return false;
	}
	if( slot >= SCSLOT_FURNACE_IN && slot <= SCSLOT_FURNACE_OUT && g_SCInv.screen == SCS_FURNACE )
	{
		if( slot == SCSLOT_FURNACE_IN ) { x = px + 150 + 2; y = top + 2; }
		else if( slot == SCSLOT_FURNACE_FUEL ) { x = px + 150 + 2; y = top + 2 * UI_SLOT + 2; }
		else { x = px + 310 + 2; y = top + UI_SLOT + 2; }
		return true;
	}
	if( slot >= UI_RECIPE && slot < UI_RECIPE + SC_NUM_RECIPES )
	{
		int i = slot - UI_RECIPE;
		x = bx + UI_PAD + ( i % 5 ) * UI_SLOT + 2;
		y = by + 26 + ( i / 5 ) * UI_SLOT + 2;
		return true;
	}
	return false;
}

int CHudSCInventory::HitTest( int mx, int my )
{
	static const int fixed[] = { SCSLOT_RESULT, SCSLOT_FURNACE_IN, SCSLOT_FURNACE_FUEL, SCSLOT_FURNACE_OUT };
	int x, y;
	for( int s = 0; s < SC_INV_SLOTS + 9; s++ )
		if( SlotRect( s, x, y ) && mx >= x - 2 && mx < x + 50 && my >= y - 2 && my < y + 50 )
			return s;
	for( int k = 0; k < 4; k++ )
		if( SlotRect( fixed[k], x, y ) && mx >= x - 2 && mx < x + 50 && my >= y - 2 && my < y + 50 )
			return fixed[k];
	for( int r = 0; r < SC_NUM_RECIPES; r++ )
		if( SlotRect( UI_RECIPE + r, x, y ) && mx >= x - 2 && mx < x + 50 && my >= y - 2 && my < y + 50 )
			return UI_RECIPE + r;
	int px, py, bx, by;
	Origin( px, py, bx, by );
	if(( mx >= px && mx < px + UI_W && my >= py && my < py + UI_H ) || ( mx >= bx && mx < bx + UI_BOOK_W && my >= by && my < by + UI_H ))
		return UI_NONE;
	return SCSLOT_OUTSIDE;
}

// a press: with something on the mouse over a slot that can take it, a drag starts (the click waits for the release);
// otherwise it is a click now
void CHudSCInventory::Press( int button )
{
	int hit = HitTest( (int)m_flX, (int)m_flY );
	if( !m_bShift && g_SCInv.cursor.count > 0 && CanSpread( hit ))
	{
		m_iDragButton = button;
		m_nDrag = 0;
		DragOver( hit );
		return;
	}
	ClickAt( hit, button );
}

// a slot a drag can put the mouse's item in: empty, or the same item with room
bool CHudSCInventory::CanSpread( int slot )
{
	int x, y;
	if( slot < 0 || slot >= UI_RECIPE || slot == SCSLOT_RESULT || slot == SCSLOT_FURNACE_OUT || !SlotRect( slot, x, y ))
		return false;
	const scclslot_t *s = SlotData( slot ), &c = g_SCInv.cursor;
	const scitem_t *it = SC_Item( c.id );
	return s && ( s->count <= 0 || ( s->id == c.id && s->dur == c.dur && s->count < ( it ? it->stack : 64 )));
}

void CHudSCInventory::DragOver( int slot )
{
	if( m_iDragButton < 0 || !CanSpread( slot ) || m_nDrag >= g_SCInv.cursor.count || m_nDrag >= 64 )
		return;
	for( int k = 0; k < m_nDrag; k++ )
		if( m_iDrag[k] == slot )
			return;
	m_iDrag[m_nDrag++] = slot;
}

// the release: over one slot it was a click there, over more the item spreads
void CHudSCInventory::EndDrag( void )
{
	int button = m_iDragButton;
	m_iDragButton = -1;
	if( m_nDrag <= 1 )
	{
		ClickAt( m_nDrag ? m_iDrag[0] : HitTest( (int)m_flX, (int)m_flY ), button );
		return;
	}
	char cmd[512];
	int len = snprintf( cmd, sizeof( cmd ), "sc_spread %d", button );
	for( int k = 0; k < m_nDrag && len < (int)sizeof( cmd ) - 8; k++ )
		len += snprintf( cmd + len, sizeof( cmd ) - len, " %d", m_iDrag[k] );
	m_iLastClick = UI_NONE;
	gEngfuncs.pfnServerCmd( cmd );
}

void CHudSCInventory::Click( int button )
{
	ClickAt( HitTest( (int)m_flX, (int)m_flY ), button );
}

void CHudSCInventory::ClickAt( int hit, int button )
{
	char cmd[64];
	if( hit == UI_NONE )
		return;
	float now = gEngfuncs.GetClientTime();
	bool twice = button == 0 && !m_bShift && hit == m_iLastClick && now - m_flLastClick < 0.3f && hit >= 0 && hit < UI_RECIPE;
	m_iLastClick = button == 0 && !twice ? hit : UI_NONE;
	m_flLastClick = now;
	if( hit >= UI_RECIPE )
		snprintf( cmd, sizeof( cmd ), "sc_recipe %d %d", hit - UI_RECIPE, m_bShift ? 1 : 0 );
	else if( twice )
		snprintf( cmd, sizeof( cmd ), "sc_gather %d", hit );	// the double click: gather this item onto the mouse
	else
		snprintf( cmd, sizeof( cmd ), "sc_click %d %d %d", hit, button, m_bShift ? 1 : 0 );
	gEngfuncs.pfnServerCmd( cmd );
}

void CHudSCInventory::Throw( bool all )
{
	int hit = HitTest( (int)m_flX, (int)m_flY );
	if( hit < 0 || hit >= UI_RECIPE )
		return;
	char cmd[64];
	snprintf( cmd, sizeof( cmd ), "sc_throwslot %d %d", hit, all ? 1 : 0 );
	gEngfuncs.pfnServerCmd( cmd );
}

void CHudSCInventory::Hotkey( int n )
{
	int hit = HitTest( (int)m_flX, (int)m_flY );
	if( hit < 0 || hit >= UI_RECIPE )
		return;
	char cmd[64];
	snprintf( cmd, sizeof( cmd ), "sc_hotkey %d %d", hit, n );
	gEngfuncs.pfnServerCmd( cmd );
}

bool CHudSCInventory::CanMake( int r )
{
	int ids[9], need[9];
	int n = SC_RecipeNeeds( &m_Recipes[r], ids, need );
	for( int k = 0; k < n; k++ )
	{
		int have = 0;
		for( int i = 0; i < SC_INV_SLOTS; i++ )
			if( g_SCInv.slot[i].id == ids[k] )
				have += g_SCInv.slot[i].count;
		for( int i = 0; i < 9; i++ )
			if( g_SCInv.grid[i].id == ids[k] )
				have += g_SCInv.grid[i].count;
		if( have < need[k] )
			return false;
	}
	return true;
}

const scclslot_t *CHudSCInventory::SlotData( int slot )
{
	if( slot >= 0 && slot < SC_INV_SLOTS ) return &g_SCInv.slot[slot];
	if( slot >= SCSLOT_GRID && slot < SCSLOT_GRID + 9 ) return &g_SCInv.grid[slot - SCSLOT_GRID];
	if( slot == SCSLOT_RESULT ) return &g_SCInv.result;
	if( slot >= SCSLOT_FURNACE_IN && slot <= SCSLOT_FURNACE_OUT ) return &g_SCInv.furnace[slot - SCSLOT_FURNACE_IN];
	return NULL;
}

//
// drawing: Minecraft's light grey windows and inset slots
//
void CHudSCInventory::DrawPanel( int x, int y, int w, int h )
{
	gEngfuncs.pfnFillRGBABlend( x, y, w, h, 198, 198, 198, 255 );
	gEngfuncs.pfnFillRGBABlend( x, y, w - 3, 3, 255, 255, 255, 255 );
	gEngfuncs.pfnFillRGBABlend( x, y, 3, h - 3, 255, 255, 255, 255 );
	gEngfuncs.pfnFillRGBABlend( x + 3, y + h - 3, w - 3, 3, 85, 85, 85, 255 );
	gEngfuncs.pfnFillRGBABlend( x + w - 3, y + 3, 3, h - 3, 85, 85, 85, 255 );
	gEngfuncs.pfnFillRGBABlend( x - 2, y - 2, w + 4, 2, 0, 0, 0, 255 );
	gEngfuncs.pfnFillRGBABlend( x - 2, y + h, w + 4, 2, 0, 0, 0, 255 );
	gEngfuncs.pfnFillRGBABlend( x - 2, y, 2, h, 0, 0, 0, 255 );
	gEngfuncs.pfnFillRGBABlend( x + w, y, 2, h, 0, 0, 0, 255 );
}

void CHudSCInventory::DrawItem( int x, int y, const scclslot_t *s )
{
	int id = s->id, count = s->count;
	if( count <= 0 || !m_hItems || id <= 0 || id >= SPR_Frames( m_hItems ))
		return;
	SPR_Set( m_hItems, 255, 255, 255 );
	SPR_DrawHoles( id, x, y, NULL );
	if( count > 1 && m_hCounts )
	{
		SPR_Set( m_hCounts, 255, 255, 255 );
		SPR_DrawHoles( count > 99 ? 100 : count, x - 4, y - 4, NULL );	// counts.spr is laid out for 56-pixel slots
	}
	SC_DrawDurability( x, y, s );
}

void CHudSCInventory::DrawSlot( int x, int y, const scclslot_t *s, bool big )
{
	int b = big ? 6 : 0;
	gEngfuncs.pfnFillRGBABlend( x - 2 - b, y - 2 - b, 52 + 2 * b, 2, 55, 55, 55, 255 );
	gEngfuncs.pfnFillRGBABlend( x - 2 - b, y - 2 - b, 2, 52 + 2 * b, 55, 55, 55, 255 );
	gEngfuncs.pfnFillRGBABlend( x - b, y + 48 + b, 50 + 2 * b, 2, 255, 255, 255, 255 );
	gEngfuncs.pfnFillRGBABlend( x + 48 + b, y - b, 2, 50 + 2 * b, 255, 255, 255, 255 );
	gEngfuncs.pfnFillRGBABlend( x - b, y - b, 48 + 2 * b, 48 + 2 * b, 139, 139, 139, 255 );
	if( s )
		DrawItem( x, y, s );
}

// the pixel font (sprites/svencraft/font.spr: ASCII from 32, 12 pixels a character, 22 high)
#define UI_CHAR_W	12
#define UI_CHAR_H	22

void CHudSCInventory::DrawText( int x, int y, const char *s, int r, int g, int b, bool shadow )
{
	if( !m_hFont )
		return;
	if( shadow )
		DrawText( x + 2, y + 2, s, r / 4, g / 4, b / 4 );
	SPR_Set( m_hFont, r, g, b );
	for( ; *s; s++, x += UI_CHAR_W )
	{
		int c = (unsigned char)*s;
		if( c > 32 && c < 128 )
			SPR_DrawHoles( c - 32, x, y, NULL );
	}
}

void CHudSCInventory::DrawTooltip( int x, int y, const char **lines, int n )
{
	int w = 0, h = 0, lh = UI_CHAR_H + 2;
	for( int i = 0; i < n; i++ )
		w = Q_max( w, (int)strlen( lines[i] ) * UI_CHAR_W );
	h = n * lh;
	x += 16;
	y -= 8;
	if( x + w + 12 > ScreenWidth ) x = ScreenWidth - w - 12;
	gEngfuncs.pfnFillRGBABlend( x - 6, y - 4, w + 12, h + 8, 16, 0, 16, 235 );
	gEngfuncs.pfnFillRGBABlend( x - 5, y - 3, w + 10, 1, 80, 0, 255, 255 );
	gEngfuncs.pfnFillRGBABlend( x - 5, y + h + 2, w + 10, 1, 40, 0, 127, 255 );
	for( int i = 0; i < n; i++ )
		DrawText( x, y + i * lh, lines[i], i ? 170 : 255, i ? 170 : 255, i ? 170 : 255, true );
}

// Minecraft's crafting arrow; fill (0..1) whitens it from the left (furnace progress)
void CHudSCInventory::DrawArrow( int x, int y, float fill )
{
	static const int rows[] = { 2, 4, 6, 8, 10, 12, 14, 12, 10, 8, 6, 4, 2 };	// head widths
	int len = 44;
	for( int pass = 0; pass < 2; pass++ )
	{
		int r = pass ? 255 : 139, g = pass ? 255 : 139, b = pass ? 255 : 139;
		int limit = pass ? (int)( fill * len ) : len;
		if( limit <= 0 )
			continue;
		int shaft = Q_min( limit, 30 );
		gEngfuncs.pfnFillRGBABlend( x, y + 10, shaft, 6, r, g, b, 255 );
		for( int i = 0; i < 13; i++ )
		{
			int w = Q_min( rows[i], limit - 30 );
			if( w > 0 )
				gEngfuncs.pfnFillRGBABlend( x + 30, y + i * 2, w, 2, r, g, b, 255 );
		}
	}
}

int CHudSCInventory::Draw( float flTime )
{
	if( !m_bOpen )
		return 1;
	int px, py, bx, by, x, y;
	Origin( px, py, bx, by );

	// dim the game behind, like Minecraft
	gEngfuncs.pfnFillRGBABlend( 0, 0, ScreenWidth, ScreenHeight, 16, 16, 16, 150 );

	// the window
	DrawPanel( px, py, UI_W, UI_H );
	const char *title = g_SCInv.screen == SCS_TABLE ? "Crafting Table" : g_SCInv.screen == SCS_FURNACE ? "Furnace" : "Crafting";
	DrawText( px + UI_PAD, py + 4, title, 64, 64, 64 );
	DrawText( px + UI_PAD, py + 180, "Inventory", 64, 64, 64 );
	for( int s = 0; s < SC_INV_SLOTS + 9; s++ )
		if( SlotRect( s, x, y ))
			DrawSlot( x, y, SlotData( s ), false );
	// a drag in progress: the slots it has crossed light up
	if( m_iDragButton >= 0 )
	{
		DragOver( HitTest( (int)m_flX, (int)m_flY ));
		for( int k = 0; k < m_nDrag; k++ )
			if( SlotRect( m_iDrag[k], x, y ))
				gEngfuncs.pfnFillRGBABlend( x, y, 48, 48, 255, 255, 255, 90 );
	}
	if( g_SCInv.screen == SCS_INVENTORY || g_SCInv.screen == SCS_TABLE )
	{
		SlotRect( SCSLOT_RESULT, x, y );
		DrawArrow( x - 70, y + 10, 0 );
		DrawSlot( x, y, &g_SCInv.result, true );
	}
	else if( g_SCInv.screen == SCS_FURNACE )
	{
		for( int s = SCSLOT_FURNACE_IN; s <= SCSLOT_FURNACE_OUT; s++ )
			if( SlotRect( s, x, y ))
				DrawSlot( x, y, SlotData( s ), s == SCSLOT_FURNACE_OUT );
		// the flame between input and fuel burns down with the fuel
		SlotRect( SCSLOT_FURNACE_IN, x, y );
		int fh = 26 * g_SCInv.burn / 255;
		gEngfuncs.pfnFillRGBABlend( x + 12, y + 60, 24, 26, 139, 139, 139, 255 );
		if( fh > 0 )
		{
			gEngfuncs.pfnFillRGBABlend( x + 12, y + 60 + 26 - fh, 24, fh, 255, 150, 30, 255 );
			gEngfuncs.pfnFillRGBABlend( x + 18, y + 60 + 26 - fh + fh / 3, 12, fh - fh / 3, 255, 230, 90, 255 );
		}
		SlotRect( SCSLOT_FURNACE_OUT, x, y );
		DrawArrow( x - 90, y + 10, g_SCInv.cook / 255.0f );
	}

	// the recipe book
	DrawPanel( bx, by, UI_BOOK_W, UI_H );
	DrawText( bx + UI_PAD, by + 4, "Recipes", 64, 64, 64 );
	for( int r = 0; r < SC_NUM_RECIPES; r++ )
	{
		SlotRect( UI_RECIPE + r, x, y );
		scclslot_t out = { m_Recipes[r].out, m_Recipes[r].count, 255 };
		DrawSlot( x, y, &out, false );
		bool usable = CanMake( r ) && ( g_SCInv.screen == SCS_TABLE || !SC_RecipeNeedsTable( &m_Recipes[r] ));
		if( !usable || g_SCInv.screen == SCS_FURNACE )
			gEngfuncs.pfnFillRGBABlend( x - 2, y - 2, 52, 52, 60, 30, 30, 150 );
	}

	// what the mouse carries, the cursor, and a tooltip for what's under it
	int mx = (int)m_flX, my = (int)m_flY;
	if( g_SCInv.cursor.count > 0 )
		DrawItem( mx - 24, my - 24, &g_SCInv.cursor );
	if( m_hCursor )
	{
		SPR_Set( m_hCursor, 255, 255, 255 );
		SPR_DrawHoles( 0, mx, my, NULL );
	}
	else
		gEngfuncs.pfnFillRGBABlend( mx - 1, my - 1, 3, 3, 255, 255, 255, 255 );

	if( g_SCInv.cursor.count <= 0 )
	{
		int hit = HitTest( mx, my );
		const char *lines[12];
		char buf[12][64];
		int n = 0;
		if( hit >= UI_RECIPE )
		{
			const screcipe_t &rc = m_Recipes[hit - UI_RECIPE];
			const scitem_t *it = SC_Item( rc.out );
			if( it )
			{
				lines[n++] = it->name;
				int ids[9], need[9];
				int k = SC_RecipeNeeds( &rc, ids, need );
				for( int i = 0; i < k && n < 10; i++ )
				{
					const scitem_t *in = SC_Item( ids[i] );
					snprintf( buf[n], 64, "  %d x %s", need[i], in ? in->name : "?" );
					lines[n] = buf[n];
					n++;
				}
				if( SC_RecipeNeedsTable( &rc ))
					lines[n++] = "  (at a crafting table)";
			}
		}
		else if( hit >= 0 || hit == SCSLOT_RESULT )
		{
			const scclslot_t *s = SlotData( hit );
			const scitem_t *it = s && s->count > 0 ? SC_Item( s->id ) : NULL;
			if( it )
				lines[n++] = it->name;
		}
		if( n )
			DrawTooltip( mx, my, lines, n );
	}
	return 1;
}
