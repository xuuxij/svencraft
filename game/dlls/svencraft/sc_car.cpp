/*
sc_car.cpp - Svencraft: cars that take damage like the ones in Sven Co-op maps

A brush-built car (sc_car) takes damage from bullets, blows and blasts. Hurt, it smokes from the engine; badly
hurt, it catches fire and a few seconds later blows up: a fireball, flying metal, a blast that hurts what is
around it (other cars can go up with it). What's left is a scorched wreck that smokes for a while. Wreck or not,
it can still be taken apart for its metal (sc_props.cpp). Spawnflag 1: it starts out as a wreck. The map builds a
burnt copy of every car (a func_illusionary named by the car's "wreck" key): the car takes its model when it
burns out, and the copy itself goes.

A fuel drum (sc_barrel, the green chemical drum) goes up when it has taken enough: a moment's delay, so a row of
them goes off one after another, then a blast that hurts like a grenade's and leaves a shallow crater (sc_blast.cpp).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "decals.h"
#include "sc_world.h"
#include "sc_game.h"

#define CAR_HEALTH		350.0f
#define CAR_SMOKE_BELOW		0.6f	// of its health
#define CAR_FIRE_BELOW		0.25f
#define CAR_BLAST_DAMAGE	160.0f	// a satchel charge's
#define CAR_BLAST_RADIUS	320.0f
#define SF_CAR_WRECK		1

static int g_sCarFire, g_sCarSmoke, g_sCarGibs;

class CSCCar : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int BloodColor( void ) { return DONT_BLEED; }
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );
	void EXPORT CarThink( void );

private:
	Vector Engine( void );
	void Explode( void );

	float	m_flBurnUntil;		// on fire: it blows up then (0 = not burning)
	float	m_flSmokeUntil;		// a wreck smokes till then
	float	m_flNextPuff;
	bool	m_bWrecked;
	EHANDLE	m_hAttacker;		// who set it off: the blast is theirs
	string_t m_iszWreck;		// the burnt copy's name, then its model
	bool	m_bFoundWreck;
	EHANDLE	m_hSmoke;		// the wreck's column of smoke (an sc_emitter) while it smoulders
	void	BurnOut( void );
};

void CSCCar::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "wreck" ))
	{
		m_iszWreck = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

// the burnt copy's model on this car
void CSCCar::BurnOut( void )
{
	if( m_bFoundWreck && m_iszWreck )
	{
		SET_MODEL( ENT( pev ), STRING( m_iszWreck ));
		UTIL_SetOrigin( pev, pev->origin );
	}
}

LINK_ENTITY_TO_CLASS( sc_car, CSCCar );

void CSCCar::Precache( void )
{
	g_sCarFire = PRECACHE_MODEL( "sprites/fire.spr" );
	g_sCarSmoke = PRECACHE_MODEL( "sprites/black_smoke3.spr" );
	g_sCarGibs = PRECACHE_MODEL( "models/metalplategibs_dark.mdl" );
	PRECACHE_SOUND( "ambience/burning1.wav" );
	PRECACHE_SOUND( "weapons/explode3.wav" );
	PRECACHE_SOUND( "weapons/explode4.wav" );
}

void CSCCar::Spawn( void )
{
	Precache();
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	SET_MODEL( ENT( pev ), STRING( pev->model ));
	UTIL_SetOrigin( pev, pev->origin );
	pev->takedamage = DAMAGE_YES;
	pev->health = CAR_HEALTH;
	m_bWrecked = ( pev->spawnflags & SF_CAR_WRECK ) != 0;
	m_flSmokeUntil = m_bWrecked ? 1e9f : 0.0f;	// an old wreck smolders on
	SetThink( &CSCCar::CarThink );
	pev->nextthink = pev->ltime + RANDOM_FLOAT( 0.2f, 0.6f );
}

// the front of the car, over the engine (cars are built facing +x or +y, the long way)
Vector CSCCar::Engine( void )
{
	Vector c = VecBModelOrigin( pev );
	Vector size = pev->absmax - pev->absmin;
	if( size.y > size.x )
		c.y += size.y * 0.5f - 34.0f;
	else
		c.x += size.x * 0.5f - 34.0f;
	c.z = pev->absmax.z - 20.0f;
	return c;
}

int CSCCar::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( m_bWrecked || !( bitsDamageType & ( DMG_BULLET | DMG_CLUB | DMG_SLASH | DMG_BLAST | DMG_BURN | DMG_ENERGYBEAM | DMG_SHOCK | DMG_GENERIC )) && bitsDamageType )
		return 0;
	pev->health -= flDamage;
	if( pevAttacker )
		m_hAttacker = CBaseEntity::Instance( pevAttacker );
	if( pev->health <= -CAR_HEALTH * CAR_FIRE_BELOW )
		m_flBurnUntil = gpGlobals->time;	// a rocket to the fuel tank: no waiting
	else if( pev->health <= CAR_HEALTH * CAR_FIRE_BELOW && !m_flBurnUntil )
	{
		m_flBurnUntil = gpGlobals->time + RANDOM_FLOAT( 4.0f, 7.0f );
		EMIT_SOUND( ENT( pev ), CHAN_STATIC, "ambience/burning1.wav", 0.9f, ATTN_NORM );
	}
	if( m_flBurnUntil && pev->nextthink > pev->ltime + 0.1f )
		pev->nextthink = pev->ltime + 0.05f;
	return 1;
}

void CSCCar::CarThink( void )
{
	pev->nextthink = pev->ltime + 0.1f;
	if( !m_bFoundWreck && m_iszWreck )
	{
		// every entity is there by the first think: take the burnt copy's model and remove the copy
		CBaseEntity *pCopy = UTIL_FindEntityByTargetname( NULL, STRING( m_iszWreck ));
		m_iszWreck = pCopy ? pCopy->pev->model : 0;
		m_bFoundWreck = true;
		if( pCopy )
			UTIL_Remove( pCopy );
		if( m_bWrecked )
			BurnOut();
	}
	Vector e = Engine();

	if( m_flBurnUntil )
	{
		if( gpGlobals->time >= m_flBurnUntil )
		{
			Explode();
			return;
		}
		// flames out of the engine, black smoke over them; close by, it burns
		if( gpGlobals->time >= m_flNextPuff )
		{
			m_flNextPuff = gpGlobals->time + 0.15f;
			Vector f = e + Vector( RANDOM_FLOAT( -16, 16 ), RANDOM_FLOAT( -16, 16 ), RANDOM_FLOAT( 0, 12 ));
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, f );
				WRITE_BYTE( TE_SPRITE );
				WRITE_COORD( f.x ); WRITE_COORD( f.y ); WRITE_COORD( f.z );
				WRITE_SHORT( g_sCarFire );
				WRITE_BYTE( RANDOM_LONG( 5, 9 ));
				WRITE_BYTE( 220 );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, e );
				WRITE_BYTE( TE_SMOKE );
				WRITE_COORD( e.x ); WRITE_COORD( e.y ); WRITE_COORD( e.z + 30 );
				WRITE_SHORT( g_sCarSmoke );
				WRITE_BYTE( RANDOM_LONG( 15, 25 ));
				WRITE_BYTE( 10 );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, e );
				WRITE_BYTE( TE_DLIGHT );
				WRITE_COORD( e.x ); WRITE_COORD( e.y ); WRITE_COORD( e.z + 8 );
				WRITE_BYTE( RANDOM_LONG( 14, 18 ));	// radius * 0.1
				WRITE_BYTE( 255 ); WRITE_BYTE( 140 ); WRITE_BYTE( 50 );
				WRITE_BYTE( 3 );	// life * 10
				WRITE_BYTE( 0 );
			MESSAGE_END();
			::RadiusDamage( e, pev, pev, 4.0f, 72.0f, CLASS_NONE, DMG_BURN );
		}
		return;
	}

	// the wreck's smoke column dies down
	if( m_bWrecked && gpGlobals->time >= m_flSmokeUntil && m_hSmoke != 0 )
	{
		UTIL_Remove( m_hSmoke );
		m_hSmoke = NULL;
	}

	// hurt or wrecked: smoke from the engine now and then
	if(( pev->health < CAR_HEALTH * CAR_SMOKE_BELOW || gpGlobals->time < m_flSmokeUntil ) && gpGlobals->time >= m_flNextPuff )
	{
		m_flNextPuff = gpGlobals->time + ( m_bWrecked ? 1.2f : 0.6f ) * RANDOM_FLOAT( 0.7f, 1.3f );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, e );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( e.x + RANDOM_FLOAT( -10, 10 )); WRITE_COORD( e.y + RANDOM_FLOAT( -10, 10 )); WRITE_COORD( e.z + 12 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG( 8, 14 ));
			WRITE_BYTE( 12 );
		MESSAGE_END();
	}
	if( !m_bWrecked && pev->health >= CAR_HEALTH * CAR_SMOKE_BELOW )
		pev->nextthink = pev->ltime + 0.5f;
}

void CSCCar::Explode( void )
{
	Vector c = VecBModelOrigin( pev );
	m_bWrecked = true;
	m_flBurnUntil = 0;
	m_flSmokeUntil = gpGlobals->time + 30.0f;
	STOP_SOUND( ENT( pev ), CHAN_STATIC, "ambience/burning1.wav" );
	EMIT_SOUND( ENT( pev ), CHAN_BODY, RANDOM_LONG( 0, 1 ) ? "weapons/explode3.wav" : "weapons/explode4.wav", 1.0f, 0.3f );

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( c.x ); WRITE_COORD( c.y ); WRITE_COORD( c.z + 24 );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( 45 );	// scale * 10
		WRITE_BYTE( 12 );
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	// panels and parts everywhere
	Vector size = pev->absmax - pev->absmin;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( c.x ); WRITE_COORD( c.y ); WRITE_COORD( c.z + 16 );
		WRITE_COORD( size.x ); WRITE_COORD( size.y ); WRITE_COORD( size.z );
		WRITE_COORD( 0 ); WRITE_COORD( 0 ); WRITE_COORD( 260 );
		WRITE_BYTE( 30 );	// random velocity
		WRITE_SHORT( g_sCarGibs );
		WRITE_BYTE( 14 );
		WRITE_BYTE( 40 );	// 4 seconds
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();
	SC_BlastFeel( c, 2.5f );

	// the blast: around it (other cars may go up too; a fuel fire, not a charge: the ground stays)
	CBaseEntity *pAttacker = m_hAttacker;
	::RadiusDamage( c, pev, pAttacker ? pAttacker->pev : pev, CAR_BLAST_DAMAGE, CAR_BLAST_RADIUS, CLASS_NONE, DMG_BLAST );
	BurnOut();

	// a column of black smoke out of the wreck while it smoulders (sc_emitter.cpp)
	edict_t *em = CREATE_NAMED_ENTITY( MAKE_STRING( "sc_emitter" ));
	if( !FNullEnt( em ))
	{
		CBaseEntity *pSmoke = CBaseEntity::Instance( em );
		pSmoke->pev->origin = Engine() + Vector( 0, 0, 20 );
		SC_SetKeyValue( pSmoke, "kind", "1" );
		SC_SetKeyValue( pSmoke, "rate", "5" );
		SC_SetKeyValue( pSmoke, "scale", "0.8" );
		SC_SetKeyValue( pSmoke, "renderamt", "130" );
		DispatchSpawn( em );
		m_hSmoke = pSmoke;
	}

	// and scorched ground around it
	for( int i = 0; i < 3; i++ )
	{
		TraceResult tr;
		Vector from = c + Vector( RANDOM_FLOAT( -0.6f, 0.6f ) * size.x, RANDOM_FLOAT( -0.6f, 0.6f ) * size.y, 0 );
		UTIL_TraceLine( from, from - Vector( 0, 0, size.z + 32 ), ignore_monsters, edict(), &tr );
		UTIL_DecalTrace( &tr, RANDOM_LONG( 0, 1 ) ? DECAL_SCORCH1 : DECAL_SCORCH2 );
	}
	pev->nextthink = pev->ltime + 0.5f;
}

//
// fuel drums
//
#define BARREL_HEALTH		30.0f
#define BARREL_BLAST_DAMAGE	140.0f
#define BARREL_BLAST_RADIUS	260.0f

class CSCBarrel : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int BloodColor( void ) { return DONT_BLEED; }
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void EXPORT Explode( void );

private:
	EHANDLE	m_hAttacker;
};

LINK_ENTITY_TO_CLASS( sc_barrel, CSCBarrel );

void CSCBarrel::Precache( void )
{
	PRECACHE_MODEL( "models/ginsmodels/gins_drums.mdl" );
	g_sCarGibs = PRECACHE_MODEL( "models/metalplategibs_dark.mdl" );
	PRECACHE_SOUND( "weapons/explode3.wav" );
	PRECACHE_SOUND( "weapons/explode4.wav" );
	PRECACHE_SOUND( "debris/metal2.wav" );
}

void CSCBarrel::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/ginsmodels/gins_drums.mdl" );
	pev->skin = 1;	// the green one with the hazard label
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_TOSS;	// stands on whatever is under it
	UTIL_SetSize( pev, Vector( -14, -14, 0 ), Vector( 14, 14, 46 ));
	UTIL_SetOrigin( pev, pev->origin );
	pev->takedamage = DAMAGE_YES;
	pev->health = BARREL_HEALTH;
}

int CSCBarrel::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pev->takedamage == DAMAGE_NO )
		return 0;
	pev->health -= flDamage;
	if( pevAttacker )
		m_hAttacker = CBaseEntity::Instance( pevAttacker );
	if( pev->health <= 0 )
	{
		// a moment, then it goes: rows of them go off one after another
		pev->takedamage = DAMAGE_NO;
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "debris/metal2.wav", 1.0f, ATTN_NORM );
		SetThink( &CSCBarrel::Explode );
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.15f, 0.35f );
	}
	return 1;
}

void CSCBarrel::Explode( void )
{
	Vector c = pev->origin + Vector( 0, 0, 24 );
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, RANDOM_LONG( 0, 1 ) ? "weapons/explode3.wav" : "weapons/explode4.wav", 1.0f, 0.3f );
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( c.x ); WRITE_COORD( c.y ); WRITE_COORD( c.z + 16 );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( 35 );
		WRITE_BYTE( 15 );
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( c.x ); WRITE_COORD( c.y ); WRITE_COORD( c.z );
		WRITE_COORD( 24 ); WRITE_COORD( 24 ); WRITE_COORD( 40 );
		WRITE_COORD( 0 ); WRITE_COORD( 0 ); WRITE_COORD( 200 );
		WRITE_BYTE( 25 );
		WRITE_SHORT( g_sCarGibs );
		WRITE_BYTE( 6 );
		WRITE_BYTE( 30 );
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

	TraceResult tr;
	UTIL_TraceLine( c, c - Vector( 0, 0, 64 ), ignore_monsters, edict(), &tr );
	UTIL_DecalTrace( &tr, RANDOM_LONG( 0, 1 ) ? DECAL_SCORCH1 : DECAL_SCORCH2 );

	CBaseEntity *pAttacker = m_hAttacker;
	pev->solid = SOLID_NOT;
	::RadiusDamage( c, pev, pAttacker ? pAttacker->pev : pev, BARREL_BLAST_DAMAGE, BARREL_BLAST_RADIUS, CLASS_NONE, DMG_BLAST );
	SC_BlastProps( c, 48.0f );
	SC_Explosion( c, 1.5f, 3 );	// a fuel fire more than a charge: a shallow crater
	UTIL_Remove( this );
}
