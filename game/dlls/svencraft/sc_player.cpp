/*
sc_player.cpp - Svencraft: what a fall does to a player

sc_gunfeel 1 turns on recoil and spread bloom for the Half-Life guns (sc_gunfeel.h), sc_penetration 1 rounds through
thin walls (sc_ballistics.h); each player's physinfo ("scgf", "scpn") carries them to the client.

sc_falldamage picks the rule, for single and multiplayer alike:
  1 (default) Minecraft's, x5 like every number here: from the fall's height (v^2 / 2g), 5 per block beyond three
    (Minecraft's 1 per block): a 4-block drop costs 5, a 10-block one 35, 23 blocks kill from full health;
  2 a "realistic" curve: nothing below 3 blocks, then steeply worse: 4 m 2, 6 m 11, 10 m 42, 15 m fatal;
  0 Half-Life's own (mp_falldamage in multiplayer).
Landing in snow halves it (water already cancels it).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "pm_materials.h"
#include "sc_world.h"
#include "sc_game.h"
#include "game.h"

static cvar_t sc_falldamage = { "sc_falldamage", "1", FCVAR_SERVER };
static cvar_t sc_gunfeel = { "sc_gunfeel", "1", FCVAR_SERVER };	// recoil and spread bloom (sc_gunfeel.h)
static cvar_t sc_respawn = { "sc_respawn", "1", FCVAR_SERVER };	// respawn at a spawn point after death (client.cpp respawn)
static cvar_t sc_penetration = { "sc_penetration", "1", FCVAR_SERVER };	// rounds through thin walls (sc_ballistics.h)

extern "C" char PM_FindTextureType( char *name );

void SC_RegisterPlayerCvars( void )
{
	CVAR_REGISTER( &sc_falldamage );
	CVAR_REGISTER( &sc_gunfeel );
	CVAR_REGISTER( &sc_respawn );
	CVAR_REGISTER( &sc_penetration );
	SC_RegisterBallisticsCvars();
	SC_RegisterLagCompCvars();
}

int SC_Penetration( void )
{
	return sc_penetration.value != 0.0f;
}

int SC_GunFeel( void )
{
	return sc_gunfeel.value != 0.0f;
}

// the client weapons and effects follow the server's settings: they ride in each player's physinfo
static void SC_SyncPhysKey( CBasePlayer *pPlayer, const char *key, bool on )
{
	const char *want = on ? "1" : "0";
	const char *has = g_engfuncs.pfnGetPhysicsKeyValue( pPlayer->edict(), key );
	if( !has || strcmp( has, want ))
		g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), key, want );
}

void SC_SyncGunFeel( CBasePlayer *pPlayer )
{
	SC_SyncPhysKey( pPlayer, "scgf", sc_gunfeel.value != 0.0f );
	SC_SyncPhysKey( pPlayer, "scpn", sc_penetration.value != 0.0f );
}

// In co-op (Half-Life's rules) a dead player's own entity is the body, and the engine turns a player to their view
// after every move (sv_pmove.c): the body keeps the way it fell instead
static Vector g_vecBodyAngles[33];

void SC_BodyFell( CBasePlayer *pPlayer )
{
	int i = pPlayer->entindex();
	if( i > 0 && i <= 32 )
		g_vecBodyAngles[i] = Vector( 0, pPlayer->pev->angles.y, 0 );
}

void SC_HoldBody( CBasePlayer *pPlayer )
{
	int i = pPlayer->entindex();
	if( i > 0 && i <= 32 && pPlayer->pev->deadflag != DEAD_NO )
		pPlayer->pev->angles = g_vecBodyAngles[i];
}

// Sven's co-op: players don't hurt each other, with bullets or blasts (a rocket's or grenade's attacker is the one
// who fired it); their own blasts still hurt them. mp_friendlyfire 1 lets them; deathmatch is everyone for himself
bool SC_PlayerShielded( CBasePlayer *pVictim, CBaseEntity *pAttacker )
{
	return !gpGlobals->deathmatch && friendlyfire.value == 0.0f && pAttacker && pAttacker != pVictim && pAttacker->IsPlayer();
}

// Sven's antiblock: use held for a moment (0.3 s) on a teammate in the way changes places with them, once a second
// at most; whoever lands where the other crouched crouches too (SevenKewp's TryAntiBlock, for players)
static float g_flUseHeld[33], g_flNextSwap[33];

void SC_AntiBlock( CBasePlayer *pPlayer )
{
	int i = pPlayer->entindex();
	if( i < 1 || i > 32 )
		return;
	if( !( pPlayer->pev->button & IN_USE ))
	{
		g_flUseHeld[i] = 0.0f;
		return;
	}
	if( g_flUseHeld[i] == 0.0f )
		g_flUseHeld[i] = gpGlobals->time;
	if( g_flUseHeld[i] < 0.0f || gpGlobals->time - g_flUseHeld[i] < 0.3f )
		return;
	g_flUseHeld[i] = -1.0f;	// once a press

	UTIL_MakeVectors( pPlayer->pev->v_angle );
	Vector eye = pPlayer->pev->origin + pPlayer->pev->view_ofs;
	TraceResult tr;
	UTIL_TraceLine( eye, eye + gpGlobals->v_forward * 64.0f, dont_ignore_monsters, pPlayer->edict(), &tr );
	CBaseEntity *pHit = tr.pHit ? CBaseEntity::Instance( tr.pHit ) : NULL;
	if( !pHit || !pHit->IsPlayer() || !pHit->IsAlive() || pHit == pPlayer )
		return;
	CBasePlayer *pOther = (CBasePlayer *)pHit;
	if( g_flNextSwap[i] > gpGlobals->time )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, UTIL_VarArgs( "Wait %.1fs", g_flNextSwap[i] - gpGlobals->time + 0.05f ));
		return;
	}
	g_flNextSwap[i] = gpGlobals->time + 1.0f;

	Vector a = pPlayer->pev->origin, b = pOther->pev->origin;
	bool aDuck = ( pPlayer->pev->flags & FL_DUCKING ) != 0, bDuck = ( pOther->pev->flags & FL_DUCKING ) != 0;
	CBasePlayer *who[2] = { pPlayer, pOther };
	bool duck[2] = { bDuck, aDuck };	// each takes the other's place and stance
	Vector to[2] = { b, a };
	for( int k = 0; k < 2; k++ )
	{
		CBasePlayer *p = who[k];
		if( duck[k] )
		{
			p->pev->flags |= FL_DUCKING;
			p->pev->flDuckTime = 26;
			p->pev->view_ofs = VEC_DUCK_VIEW;
			UTIL_SetSize( p->pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		}
		p->pev->velocity = g_vecZero;
		UTIL_SetOrigin( p->pev, to[k] );
	}
}

// Sven's way (sc_respawn): the dead wait by their bodies, where a medic can bring them back, until they respawn
bool SC_StayWithBody( void )
{
	return sc_respawn.value != 0.0f;
}

bool SC_OwnFallDamage( void )
{
	return sc_falldamage.value >= 1.0f;
}

// the speed from which a fall starts to hurt: three blocks' drop (about 438 at sv_gravity 800)
float SC_FallHurtSpeed( void )
{
	float g = Q_max( CVAR_GET_FLOAT( "sv_gravity" ), 1.0f );
	return sqrt( 2.0f * g * 3.0f * VOX_BLOCK_SIZE );
}

float SC_FallDamage( CBasePlayer *pPlayer )
{
	float v = pPlayer->m_flFallVelocity, g = Q_max( CVAR_GET_FLOAT( "sv_gravity" ), 1.0f );
	float h = v * v / ( 2.0f * g ), dmg;
	if( sc_falldamage.value >= 2.0f )
		dmg = h <= 3.0f * VOX_BLOCK_SIZE ? 0.0f : 100.0f * powf(( h - 3.0f * VOX_BLOCK_SIZE ) / 480.0f, 1.6f );
	else
		dmg = 5.0f * Q_max( 0.0f, ceilf( h / VOX_BLOCK_SIZE - 0.05f ) - 3.0f );

	// what it landed on
	Vector from = pPlayer->pev->origin, to = from - Vector( 0, 0, 64 );
	const char *tex = TRACE_TEXTURE( INDEXENT( 0 ), from, to );
	if( tex )
	{
		char name[16];
		strncpy( name, ( *tex == '-' || *tex == '+' ) ? tex + 2 : ( *tex == '{' || *tex == '!' || *tex == '~' || *tex == ' ' ) ? tex + 1 : tex, 15 );
		name[15] = 0;
		char t = PM_FindTextureType( name );
		if( t == CHAR_TEX_SNOW || t == CHAR_TEX_SNOW_OPFOR )
			dmg *= 0.5f;
	}
	return dmg;
}
