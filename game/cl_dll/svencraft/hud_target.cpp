/*
hud_target.cpp - Svencraft: who is under the crosshair, Sven Co-op style

The server sends "SCTarget" (dlls/svencraft/sc_status.cpp) when the player's crosshair target or its health
changes: the name goes under the crosshair, "Friend: ..." in green, "Enemy: ..." in red, others in yellow, with its
health below (and armour for players). cl_targetinfo 0 hides it.
*/
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>

class CHudSCTarget : public CHudBase
{
public:
	int Init( void );
	int VidInit( void ) { return 1; }
	int Draw( float flTime );
	int MsgFunc_SCTarget( const char *pszName, int iSize, void *pbuf );

private:
	int m_iIndex, m_iRel, m_iHealth, m_iArmor;
	char m_szName[48];
	cvar_t *m_pShow;
};

static CHudSCTarget g_SCTarget;

static int __MsgFunc_SCTarget( const char *pszName, int iSize, void *pbuf )
{
	return g_SCTarget.MsgFunc_SCTarget( pszName, iSize, pbuf );
}

int CHudSCTarget::Init( void )
{
	m_iIndex = 0;
	m_szName[0] = 0;
	m_pShow = gEngfuncs.pfnRegisterVariable( "cl_targetinfo", "1", FCVAR_ARCHIVE );
	HOOK_MESSAGE( SCTarget );
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );
	return 1;
}

int CHudSCTarget::MsgFunc_SCTarget( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iIndex = READ_SHORT();
	m_iRel = READ_CHAR();
	m_iHealth = READ_SHORT();
	m_iArmor = READ_SHORT();
	strncpy( m_szName, READ_STRING(), sizeof( m_szName ) - 1 );
	m_szName[sizeof( m_szName ) - 1] = 0;
	return 1;
}

int CHudSCTarget::Draw( float flTime )
{
	if( !m_iIndex || !m_szName[0] || ( m_pShow && !m_pShow->value ) || ( gHUD.m_iHideHUDDisplay & HIDEHUD_ALL ))
		return 1;
	// R_AL -2, R_FR -1: friends; R_DL 1, R_HT 2, R_NM 3: enemies
	int r = 230, g = 200, b = 80;
	const char *who = "";
	if( m_iRel < 0 )
	{
		r = 90; g = 220; b = 90;
		who = "Friend: ";
	}
	else if( m_iRel > 0 )
	{
		r = 235; g = 70; b = 55;
		who = "Enemy: ";
	}
	char line1[96], line2[64];
	snprintf( line1, sizeof( line1 ), "%s%s", who, m_szName );
	if( m_iArmor >= 0 )
		snprintf( line2, sizeof( line2 ), "Health: %d   Armor: %d", m_iHealth, m_iArmor );
	else
		snprintf( line2, sizeof( line2 ), "Health: %d", m_iHealth );
	int y = ScreenHeight / 2 + 36;
	int w = gHUD.DrawHudStringLen( line1 );
	gHUD.DrawHudString(( ScreenWidth - w ) / 2, y, ScreenWidth, line1, r, g, b );
	w = gHUD.DrawHudStringLen( line2 );
	gHUD.DrawHudString(( ScreenWidth - w ) / 2, y + gHUD.m_iFontHeight, ScreenWidth, line2, 220, 220, 220 );
	return 1;
}

void SC_TargetHudInit( void )
{
	g_SCTarget.Init();
}
