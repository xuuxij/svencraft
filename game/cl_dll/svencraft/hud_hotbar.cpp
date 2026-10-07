/*
hud_hotbar.cpp - Svencraft: the hotbar (bottom centre of the screen) and the inventory message

The server sends "SCInv" with the whole inventory (see SC_InvSend in dlls/svencraft/sc_inventory.cpp); it is
kept in g_SCInv for the inventory screen too. The hotbar shows the first nine slots like Minecraft's: icon,
stack count, the selected one highlighted, its name popping up above the bar for a moment after it changes.
*/
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include "sc_client.h"
#include "com_model.h"

#define SC_SLOT_SIZE	56

scclinv_t g_SCInv;

class CHudSCHotbar : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_SCInv( const char *pszName, int iSize, void *pbuf );

private:
	HSPRITE m_hBar, m_hCounts, m_hItems;
	int m_iFramesPerSet;	// hotbar.spr: frames [0, n) unselected, [n, 2n) selected; frame 0 is the empty slot
	int m_iLastSlot, m_iLastId;
	float m_flNameTime;
};

static CHudSCHotbar g_SCHotbar;

static int __MsgFunc_SCInv( const char *pszName, int iSize, void *pbuf )
{
	return g_SCHotbar.MsgFunc_SCInv( pszName, iSize, pbuf );
}

int CHudSCHotbar::Init( void )
{
	memset( &g_SCInv, 0, sizeof( g_SCInv ));
	m_iLastSlot = m_iLastId = -1;
	m_flNameTime = 0;
	HOOK_MESSAGE( SCInv );
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );
	return 1;
}

int CHudSCHotbar::VidInit( void )
{
	m_hBar = SPR_Load( "sprites/svencraft/hotbar.spr" );
	m_hCounts = SPR_Load( "sprites/svencraft/counts.spr" );
	m_hItems = SPR_Load( "sprites/svencraft/items.spr" );
	m_iFramesPerSet = m_hBar ? SPR_Frames( m_hBar ) / 2 : 0;
	return 1;
}

static void SC_ReadSlot( scclslot_t &s )
{
	s.id = READ_BYTE();
	s.count = READ_BYTE();
	if( s.count <= 0 )
		s.id = 0;
	const scitem_t *it = SC_Item( s.id );
	s.dur = ( it && it->kind == SCI_TOOL ) ? READ_BYTE() : 255;	// tools carry their wear
}

int CHudSCHotbar::MsgFunc_SCInv( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int screen = g_SCInv.screen;
	g_SCInv.hotbar = READ_BYTE();
	g_SCInv.screen = READ_BYTE();
	for( int i = 0; i < SC_INV_SLOTS; i++ )
		SC_ReadSlot( g_SCInv.slot[i] );
	for( int i = 0; i < 9; i++ )
		SC_ReadSlot( g_SCInv.grid[i] );
	SC_ReadSlot( g_SCInv.cursor );
	SC_ReadSlot( g_SCInv.result );
	for( int i = 0; i < 3; i++ )
		SC_ReadSlot( g_SCInv.furnace[i] );
	g_SCInv.cook = READ_BYTE();
	g_SCInv.burn = READ_BYTE();

	if( g_SCInv.screen != screen )
		SC_UIScreenChanged();
	int id = g_SCInv.slot[g_SCInv.hotbar].id;
	if( g_SCInv.hotbar != m_iLastSlot || id != m_iLastId )
	{
		m_flNameTime = gHUD.m_flTime;
		m_iLastSlot = g_SCInv.hotbar;
		m_iLastId = id;
	}
	return 1;
}

int CHudSCHotbar::Draw( float flTime )
{
	if( !m_hBar || ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL ) || !( gHUD.m_iWeaponBits & ( 1 << WEAPON_SUIT )))
		return 1;
	if( SC_UIActive())
		return 1;	// the inventory screen has its own

	int x0 = ( ScreenWidth - SC_HOTBAR_SLOTS * SC_SLOT_SIZE ) / 2;
	int y = ScreenHeight - SC_SLOT_SIZE - 8;

	for( int s = 0; s < SC_HOTBAR_SLOTS; s++ )
	{
		const scclslot_t &sl = g_SCInv.slot[s];
		int x = x0 + s * SC_SLOT_SIZE;
		SPR_Set( m_hBar, 255, 255, 255 );
		SPR_DrawHoles( s == g_SCInv.hotbar ? m_iFramesPerSet : 0, x, y, NULL );
		if( sl.count <= 0 )
			continue;
		if( m_hItems && sl.id < SPR_Frames( m_hItems ))
		{
			SPR_Set( m_hItems, 255, 255, 255 );
			SPR_DrawHoles( sl.id, x + 4, y + 4, NULL );
		}
		if( sl.count > 1 && m_hCounts )
		{
			SPR_Set( m_hCounts, 255, 255, 255 );
			SPR_DrawHoles( sl.count > 99 ? 100 : sl.count, x, y, NULL );
		}
		SC_DrawDurability( x + 4, y + 4, &sl );
	}

	// the selected slot's name, for a couple of seconds after it changes
	const scitem_t *it = SC_Item( g_SCInv.slot[g_SCInv.hotbar].id );
	if( it && g_SCInv.slot[g_SCInv.hotbar].count > 0 && gHUD.m_flTime - m_flNameTime < 2.0f )
	{
		int w = gHUD.DrawHudStringLen( it->name );
		gHUD.DrawHudString(( ScreenWidth - w ) / 2, y - 22, ScreenWidth, it->name, 255, 255, 255 );
	}
	return 1;
}

// Minecraft's wear bar: under a damaged tool's icon, green when new through yellow to red
void SC_DrawDurability( int x, int y, const scclslot_t *s )
{
	if( !s || s->count <= 0 || s->dur >= 255 )
		return;
	const scitem_t *it = SC_Item( s->id );
	if( !it || it->kind != SCI_TOOL )
		return;
	float f = s->dur / 255.0f;
	int w = (int)( 39 * f + 0.5f );
	// hue from 120 (green) at full to 0 (red) when worn out, full saturation and value
	float h = f * 120.0f / 60.0f;
	int r = h < 1 ? 255 : (int)( 255 * ( 2 - h ));
	int g = h < 1 ? (int)( 255 * h ) : 255;
	gEngfuncs.pfnFillRGBABlend( x + 3, y + 39, 42, 6, 0, 0, 0, 255 );
	gEngfuncs.pfnFillRGBABlend( x + 3, y + 39, w, 3, r, g, 0, 255 );
}

void SC_HotbarSelect( const char *how )
{
	if( gHUD.m_fPlayerDead )
		return;
	// shown straight away; the server's answer confirms it
	if( !strcmp( how, "next" ))
		g_SCInv.hotbar = ( g_SCInv.hotbar + 1 ) % SC_HOTBAR_SLOTS;
	else if( !strcmp( how, "prev" ))
		g_SCInv.hotbar = ( g_SCInv.hotbar + SC_HOTBAR_SLOTS - 1 ) % SC_HOTBAR_SLOTS;
	else
	{
		int n = atoi( how ) - 1;
		if( n < 0 || n >= SC_HOTBAR_SLOTS )
			return;
		g_SCInv.hotbar = n;
	}
	char cmd[32];
	snprintf( cmd, sizeof( cmd ), "sc_hotbar %s", how );
	ServerCmd( cmd );
}

void SC_ViewModelLook( struct cl_entity_s *view )
{
	if( !view || !view->model || !strstr( view->model->name, "v_schand" ))
		return;
	const scclslot_t &s = g_SCInv.slot[g_SCInv.hotbar];
	int id = s.count > 0 ? s.id : 0;
	const scitem_t *it = SC_Item( id );
	if( !it || it->kind == SCI_TOOL || it->kind == SCI_WEAPON )
		view->curstate.body = 0;	// empty hand
	else if( g_SCFlatSkin[id] >= 0 )
	{
		view->curstate.body = 2;	// flat: an item, a torch
		view->curstate.skin = SCI_NUM_BLOCK_IDS + g_SCFlatSkin[id];
	}
	else
	{
		view->curstate.body = 1;	// a block
		view->curstate.skin = id;
	}
}

void SC_TargetHudInit( void );	// hud_target.cpp

void SC_HudInit( void )
{
	g_SCHotbar.Init();
	SC_UIInit();
	SC_TargetHudInit();
}

void SC_HudVidInit( void )
{
	g_SCHotbar.VidInit();
	SC_UIVidInit();
}
