/*
sc_emitter.cpp - Svencraft: a source of steam, smoke or fire for the map's ambience (manholes, roof vents,
chimneys, smouldering wrecks)

sc_emitter keys: kind 0 steam (default), 1 smoke, 2 fire; rate (puffs a second, default 6); scale (puff size, default
0.6); rendercolor and renderamt (tint and opacity, defaults by kind); spawnflags 1 = starts off. Triggering it toggles
it. The server only places it: the puffs are the client's (cl_dll/svencraft/sc_effects.cpp SC_EmitterThink), drawn
while the emitter is in view, so a hundred of them across a map cost nothing where nobody looks. The settings ride
in fields every entity sends: skin = kind, body = rate, scale, rendercolor, renderamt; renderfx SC_FX_EMITTER marks it.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "sc_game.h"

#define SF_EMITTER_STARTOFF	1

// steam is drawn blended, not added, so it shows white against a bright street or sky too
static const char *g_szEmitterSprite[3] = { "sprites/steam1.spr", "sprites/black_smoke3.spr", "sprites/fire.spr" };

class CSCEmitter : public CBaseEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if( FStrEq( pkvd->szKeyName, "kind" ))
		{
			pev->skin = Q_max( 0, Q_min( atoi( pkvd->szValue ), 2 ));
			pkvd->fHandled = TRUE;
		}
		else if( FStrEq( pkvd->szKeyName, "rate" ))
		{
			pev->body = Q_max( 1, Q_min( atoi( pkvd->szValue ), 60 ));
			pkvd->fHandled = TRUE;
		}
		else
			CBaseEntity::KeyValue( pkvd );
	}
	void Precache( void )
	{
		for( int i = 0; i < 3; i++ )
			PRECACHE_MODEL( g_szEmitterSprite[i] );
	}
	void Spawn( void )
	{
		Precache();
		if( !pev->body )
			pev->body = pev->skin == 0 ? 5 : 6;
		if( pev->scale <= 0.0f )
			pev->scale = pev->skin == 0 ? 0.9f : 0.5f;
		if( pev->rendercolor == g_vecZero )
			pev->rendercolor = pev->skin == 1 ? Vector( 70, 70, 72 ) : pev->skin == 2 ? Vector( 255, 255, 255 ) : Vector( 205, 208, 212 );
		if( !pev->renderamt )
			pev->renderamt = pev->skin == 1 ? 140 : pev->skin == 2 ? 220 : 150;
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		// a sprite model so the entity is sent to clients; drawn by nobody (the client filters it out)
		SET_MODEL( ENT( pev ), g_szEmitterSprite[pev->skin] );
		UTIL_SetSize( pev, g_vecZero, g_vecZero );
		pev->rendermode = kRenderTransAdd;
		pev->renderfx = SC_FX_EMITTER;
		if( pev->spawnflags & SF_EMITTER_STARTOFF )
			pev->effects |= EF_NODRAW;
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		bool on = !( pev->effects & EF_NODRAW );
		if( useType == USE_TOGGLE )
			on = !on;
		else
			on = useType == USE_ON;
		if( on )
			pev->effects &= ~EF_NODRAW;
		else
			pev->effects |= EF_NODRAW;
	}
};

LINK_ENTITY_TO_CLASS( sc_emitter, CSCEmitter )
