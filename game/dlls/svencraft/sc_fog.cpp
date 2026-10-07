/*
sc_fog.cpp - Svencraft: fog and haze, Sven Co-op's env_fog

env_fog keys as in Sven Co-op / Spirit of Half-Life: rendercolor (the fog's colour), iuser2 (where it starts, units
from the eye), iuser3 (where everything has disappeared into it), spawnflags 1 = starts off, 2 = fogs the sky too.
Triggering it toggles it. The one switched on last is the fog: every client gets it (message SCFog, applied each
frame by cl_dll/svencraft/sc_effects.cpp through the triangle API), and so does each player who joins.
Server command sc_fog <r g b start end [sky]> sets it from the console (and later the dungeon master); sc_fog 0
clears it.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "sc_game.h"

extern int gmsgSCFog;

#define SF_FOG_STARTOFF	1
#define SF_FOG_SKY	2

static struct
{
	bool on;
	int r, g, b, start, end;
	bool sky;
} g_SCFog;

void SC_FogSend( CBasePlayer *pPlayer )
{
	if( !gmsgSCFog )
		return;
	if( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgSCFog, NULL, pPlayer->edict());
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgSCFog );
		WRITE_BYTE( g_SCFog.on ? 1 : 0 );
		WRITE_BYTE( g_SCFog.r );
		WRITE_BYTE( g_SCFog.g );
		WRITE_BYTE( g_SCFog.b );
		WRITE_SHORT( g_SCFog.start );
		WRITE_SHORT( g_SCFog.end );
		WRITE_BYTE( g_SCFog.sky ? 1 : 0 );
	MESSAGE_END();
}

static void SC_FogSet( bool on, int r, int g, int b, int start, int end, bool sky )
{
	g_SCFog.on = on && end > start;
	g_SCFog.r = r; g_SCFog.g = g; g_SCFog.b = b;
	g_SCFog.start = Q_max( start, 0 );
	g_SCFog.end = Q_min( end, 32767 );
	g_SCFog.sky = sky;
	SC_FogSend( NULL );
}

class CEnvFog : public CPointEntity
{
public:
	void Spawn( void )
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
		if( !( pev->spawnflags & SF_FOG_STARTOFF ))
		{
			SetThink( &CEnvFog::TurnOnThink );
			pev->nextthink = gpGlobals->time + 0.1f;
		}
	}
	void EXPORT TurnOnThink( void )
	{
		Apply( true );
	}
	void Apply( bool on )
	{
		m_bOn = on;
		SC_FogSet( on, (int)pev->rendercolor.x, (int)pev->rendercolor.y, (int)pev->rendercolor.z, pev->iuser2, pev->iuser3,
			( pev->spawnflags & SF_FOG_SKY ) != 0 );
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		bool on = useType == USE_TOGGLE ? !m_bOn : useType == USE_ON;
		Apply( on );
	}

private:
	bool m_bOn;
};

LINK_ENTITY_TO_CLASS( env_fog, CEnvFog )

// a new map starts clear until its env_fog speaks
void SC_FogReset( void )
{
	g_SCFog.on = false;
}

static void SC_FogCommand( void )
{
	if( CMD_ARGC() < 6 )
	{
		if( CMD_ARGC() == 2 && !atoi( CMD_ARGV( 1 )))
			SC_FogSet( false, 0, 0, 0, 0, 0, false );
		else
			g_engfuncs.pfnServerPrint( "sc_fog <r g b start end [sky 0/1]>  |  sc_fog 0\n" );
		return;
	}
	SC_FogSet( true, atoi( CMD_ARGV( 1 )), atoi( CMD_ARGV( 2 )), atoi( CMD_ARGV( 3 )), atoi( CMD_ARGV( 4 )), atoi( CMD_ARGV( 5 )),
		CMD_ARGC() >= 7 && atoi( CMD_ARGV( 6 )) != 0 );
}

void SC_RegisterFogCommands( void )
{
	g_engfuncs.pfnAddServerCommand( "sc_fog", SC_FogCommand );
}
