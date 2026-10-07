/*
sc_lagcomp.cpp - Svencraft: shoot the monster you see, not where it is on the server by the time your shot arrives

The engine's sv_unlag rewinds only players. In co-op the targets are monsters, and every client sees them in the
past: by its ping, plus its interpolation delay (cl_interp / ex_interp, the usercmd's lerp_msec). So the server
keeps the last second of every monster's position, angles and animation frame (50 snapshots, 20 ms apart), and
while a player's rounds are traced (FireBulletsPlayer, penetration included) it puts the monsters back where that
player saw them, then restores them.

Adapted from SevenKewp's dlls/util/lagcomp.cpp (https://github.com/wootguy/SevenKewp, Half-Life SDK licence):
here only monsters are rewound (not doors, platforms or cars), the rewind includes the interpolation delay, and the
cvar is our own: sc_unlag_monsters 1 (0 turns it off), sc_unlag_max 0.5 (seconds, the most it rewinds).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "sc_game.h"

#define LAG_STATES	50		// one second of history
#define LAG_INTERVAL	0.02f
#define LAG_EDICTS	2048
#define LAG_PING_SMOOTH	32		// snapshots of each player's ping averaged

static cvar_t sc_unlag_monsters = { "sc_unlag_monsters", "1", FCVAR_SERVER };
static cvar_t sc_unlag_max = { "sc_unlag_max", "0.5", FCVAR_SERVER };

typedef struct
{
	Vector origin, angles;
	int sequence;
	float frame;
	bool kept;
} lagent_t;

typedef struct
{
	float time;
	lagent_t ents[LAG_EDICTS];
} lagstate_t;

static lagstate_t g_lagHistory[LAG_STATES];
static int g_lagHead, g_lagWritten;
static float g_lagNext;
static float g_lagPing[LAG_PING_SMOOTH][SC_MAX_PLAYERS + 1];
static int g_lagPingHead;
static int g_lagLerpMsec[SC_MAX_PLAYERS + 1];

static struct
{
	bool active, moved[LAG_EDICTS];
	lagent_t saved[LAG_EDICTS];
	int state;
} g_lagRewind;

void SC_RegisterLagCompCvars( void )
{
	CVAR_REGISTER( &sc_unlag_monsters );
	CVAR_REGISTER( &sc_unlag_max );
}

// CmdStart: how far behind this player's screen draws the world (its interpolation)
void SC_LagCompCmd( CBasePlayer *pPlayer, int lerpMsec )
{
	int i = pPlayer->entindex();
	if( i >= 1 && i <= SC_MAX_PLAYERS )
		g_lagLerpMsec[i] = lerpMsec;
}

static bool SC_LagCompensated( edict_t *e )
{
	if( e->free || !e->v.model || ( e->v.effects & EF_NODRAW ) || e->v.solid == SOLID_NOT )
		return false;
	return ( e->v.flags & FL_MONSTER ) && e->v.deadflag == DEAD_NO;
}

// StartFrame: one snapshot every LAG_INTERVAL
void SC_LagCompFrame( void )
{
	float now = gpGlobals->time;
	if( g_lagNext - now > 1.0f )
		g_lagNext = g_lagWritten = 0;	// a new map's clock
	if( now < g_lagNext || !sc_unlag_monsters.value )
		return;
	g_lagNext = now + LAG_INTERVAL;

	for( int i = 1; i <= gpGlobals->maxClients && i <= SC_MAX_PLAYERS; i++ )
	{
		edict_t *pl = INDEXENT( i );
		int ping = 0, loss = 0;
		if( pl && !pl->free && pl->pvPrivateData )
			g_engfuncs.pfnGetPlayerStats( pl, &ping, &loss );
		g_lagPing[g_lagPingHead][i] = ping / 1000.0f;
	}
	g_lagPingHead = ( g_lagPingHead + 1 ) % LAG_PING_SMOOTH;

	lagstate_t &st = g_lagHistory[g_lagHead];
	st.time = now;
	edict_t *edicts = INDEXENT( 0 );
	int maxi = Q_min( LAG_EDICTS, gpGlobals->maxEntities );
	for( int i = gpGlobals->maxClients + 1; i < maxi; i++ )
	{
		lagent_t &le = st.ents[i];
		le.kept = SC_LagCompensated( &edicts[i] );
		if( !le.kept )
			continue;
		le.origin = edicts[i].v.origin;
		le.angles = edicts[i].v.angles;
		le.sequence = edicts[i].v.sequence;
		le.frame = edicts[i].v.frame;
	}
	g_lagHead = ( g_lagHead + 1 ) % LAG_STATES;
	g_lagWritten = Q_min( LAG_STATES, g_lagWritten + 1 );
}

// before tracing this player's shots: the monsters where the player saw them
void SC_LagCompBegin( CBasePlayer *pPlayer )
{
	g_lagRewind.active = false;
	int p = pPlayer->entindex();
	if( !sc_unlag_monsters.value || !g_lagWritten || p < 1 || p > SC_MAX_PLAYERS || ( pPlayer->pev->flags & FL_FAKECLIENT ))
		return;
	float ping = 0.0f;
	for( int k = 0; k < LAG_PING_SMOOTH; k++ )
		ping += g_lagPing[k][p];
	ping /= LAG_PING_SMOOTH;
	float back = Q_min( ping + g_lagLerpMsec[p] / 1000.0f, Q_max( sc_unlag_max.value, 0.0f ));
	if( back < LAG_INTERVAL * 0.5f )
		return;
	float target = gpGlobals->time - back, best = 1e9f;
	int idx = -1;
	for( int k = 0; k < g_lagWritten; k++ )
	{
		float d = fabs( g_lagHistory[k].time - target );
		if( d < best )
		{
			best = d;
			idx = k;
		}
	}
	if( idx < 0 )
		return;

	lagstate_t &st = g_lagHistory[idx];
	edict_t *edicts = INDEXENT( 0 );
	int maxi = Q_min( LAG_EDICTS, gpGlobals->maxEntities );
	for( int i = gpGlobals->maxClients + 1; i < maxi; i++ )
	{
		g_lagRewind.saved[i].kept = false;
		g_lagRewind.moved[i] = false;
		if( !st.ents[i].kept || !SC_LagCompensated( &edicts[i] ))
			continue;	// gone, dead or new since then: as it is
		entvars_t &v = edicts[i].v;
		lagent_t &sv = g_lagRewind.saved[i];
		sv.kept = true;
		sv.origin = v.origin;
		sv.angles = v.angles;
		sv.sequence = v.sequence;
		sv.frame = v.frame;
		v.sequence = st.ents[i].sequence;
		v.frame = st.ents[i].frame;
		v.angles = st.ents[i].angles;
		if(( st.ents[i].origin - v.origin ).Length() > 1.0f )
		{
			UTIL_SetOrigin( &v, st.ents[i].origin );	// relinks it for traces
			g_lagRewind.moved[i] = true;
		}
	}
	g_lagRewind.state = idx;
	g_lagRewind.active = true;
}

// after the shots: everything back where it is now
void SC_LagCompEnd( void )
{
	if( !g_lagRewind.active )
		return;
	g_lagRewind.active = false;
	lagstate_t &st = g_lagHistory[g_lagRewind.state];
	edict_t *edicts = INDEXENT( 0 );
	int maxi = Q_min( LAG_EDICTS, gpGlobals->maxEntities );
	for( int i = gpGlobals->maxClients + 1; i < maxi; i++ )
	{
		lagent_t &sv = g_lagRewind.saved[i];
		if( !sv.kept )
			continue;
		entvars_t &v = edicts[i].v;
		if( v.sequence == st.ents[i].sequence )		// unless the hit changed its animation
		{
			v.sequence = sv.sequence;
			v.frame = sv.frame;
		}
		v.angles = sv.angles;
		if( g_lagRewind.moved[i] && !edicts[i].free )
			UTIL_SetOrigin( &v, sv.origin );
	}
}
