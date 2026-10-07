/*
sc_bots.cpp - Svencraft: stand-in players for trying co-op things alone

`sc_bot [name]` (cheats) brings in a player that does nothing but stand where it was put: something to heal,
revive, shoot by mistake or look at. The engine's fake clients only live through the moves the game gives them, so
every server frame gives each one an empty move (it stands, falls, dies and waits like anyone).
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "sc_game.h"

extern BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] );
extern void ClientPutInServer( edict_t *pEntity );

static float g_flBotLastMove;

// a fake client named so, put in front of pBy (or wherever the game spawns it)
void SC_AddBot( edict_t *pBy, const char *name )
{
	edict_t *e = g_engfuncs.pfnCreateFakeClient( name );
	if( FNullEnt( e ))
	{
		ALERT( at_console, "sc_bot: no free player slot (maxplayers)\n" );
		return;
	}
	char reject[128] = "";
	if( !ClientConnect( e, name, "127.0.0.1", reject ))
	{
		ALERT( at_console, "sc_bot: refused (%s)\n", reject );
		return;
	}
	ClientPutInServer( e );
	e->v.flags |= FL_FAKECLIENT;
	if( pBy )
	{
		UTIL_MakeVectors( Vector( 0, pBy->v.v_angle.y, 0 ));
		TraceResult tr;
		Vector from = pBy->v.origin, to = from + gpGlobals->v_forward * 96.0f;
		UTIL_TraceHull( from, to, dont_ignore_monsters, human_hull, pBy, &tr );
		if( !tr.fStartSolid && tr.flFraction > 0.6f )
		{
			UTIL_SetOrigin( &e->v, tr.vecEndPos );
			e->v.angles = e->v.v_angle = Vector( 0, pBy->v.v_angle.y + 180.0f, 0 );
			e->v.fixangle = 1;
		}
	}
	ALERT( at_console, "sc_bot: %s at %.0f %.0f %.0f\n", name, e->v.origin.x, e->v.origin.y, e->v.origin.z );
}

// every server frame: each stand-in makes an empty move, facing the way it faces
void SC_BotsFrame( void )
{
	float msec = ( gpGlobals->time - g_flBotLastMove ) * 1000.0f;
	g_flBotLastMove = gpGlobals->time;
	if( msec <= 0.0f || msec > 100.0f )
		msec = 10.0f;
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		edict_t *e = INDEXENT( i );
		if( FNullEnt( e ) || !( e->v.flags & FL_FAKECLIENT ) || FStringNull( e->v.netname ))
			continue;
		g_engfuncs.pfnRunPlayerMove( e, e->v.v_angle, 0.0f, 0.0f, 0.0f, 0, 0, (byte)msec );
	}
}
