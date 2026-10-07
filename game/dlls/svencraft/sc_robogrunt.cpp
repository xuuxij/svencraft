/*
sc_robogrunt.cpp - Svencraft: Sven Co-op's robot grunt (monster_robogrunt), on Half-Life's grunt AI

Half-Life's grunt in Sven's rgrunt.mdl, a machine: it doesn't bleed (bullets strike sparks, a shot to the head
throws a burst of them), takes double from energy weapons and four times from shock, and talks in the VOX voice
(Sven's RB_ sentences, through CHGrunt::Voice). Dead, it smokes and blows up a few seconds later (Sven's
sk_rgrunt_explode, 100), sooner if it is hit again (a third of the time); the blast leaves metal parts.
SevenKewp's CRoboGrunt was the reference.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "weapons.h"
#include "explode.h"
#include "talkmonster.h"
#include "hgrunt.h"

#define RGRUNT_EXPLODE		100	// Sven's sk_rgrunt_explode

static const char *g_RGDie[] = { "turret/tu_die.wav", "turret/tu_die2.wav", "turret/tu_die3.wav" };
static const char *g_RGSpark[] = { "buttons/spark1.wav", "buttons/spark2.wav", "buttons/spark3.wav", "buttons/spark4.wav",
	"buttons/spark5.wav", "buttons/spark6.wav" };

class CRoboGrunt : public CHGrunt
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MACHINE; }
	const char *Voice( const char *group );
	void PainSound( void );
	void DeathSound( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );
	void EXPORT SmokeThink( void );
	void RunTask( Task_t *pTask );

private:
	float m_flExplode;	// when the wreck blows (0: not dead yet)
	bool m_bBlown;
	int m_iSpark, m_iGibs;
};

LINK_ENTITY_TO_CLASS( monster_robogrunt, CRoboGrunt )

void CRoboGrunt::Spawn( void )
{
	CHGrunt::Spawn();	// Half-Life's grunt, its gun and squad
	SET_MODEL( ENT( pev ), "models/rgrunt.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	m_bloodColor = DONT_BLEED;
	m_voicePitch = RANDOM_LONG( 0, 1 ) ? 120 + RANDOM_LONG( 0, 9 ) : 115;
	m_flExplode = 0.0f;
	m_bBlown = false;
}

void CRoboGrunt::Precache( void )
{
	CHGrunt::Precache();
	PRECACHE_MODEL( "models/rgrunt.mdl" );
	m_iGibs = PRECACHE_MODEL( "models/computergibs.mdl" );
	m_iSpark = PRECACHE_MODEL( "sprites/xspark2.spr" );
	for( int i = 0; i < 3; i++ )
		PRECACHE_SOUND( g_RGDie[i] );
	for( int i = 0; i < 6; i++ )
		PRECACHE_SOUND( g_RGSpark[i] );
}

// the grunt's lines in Sven's robot voice (RB_ groups); it has no pain line
const char *CRoboGrunt::Voice( const char *group )
{
	static char buf[32];
	if( !group || strncmp( group, "HG_", 3 ) || !strcmp( group, "HG_PAIN" ))
		return NULL;
	snprintf( buf, sizeof( buf ), "RB_%s", strcmp( group, "HG_MONSTER" ) ? group + 3 : "MONST" );
	return buf;
}

void CRoboGrunt::PainSound( void )
{
	if( gpGlobals->time > m_flNextPainTime )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, g_RGSpark[RANDOM_LONG( 0, 5 )], 1.0f, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ));
		m_flNextPainTime = gpGlobals->time + 1.0f;
	}
}

void CRoboGrunt::DeathSound( void )
{
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, g_RGDie[RANDOM_LONG( 0, 2 )], 1.0f, ATTN_NORM );
}

void CRoboGrunt::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( flDamage > 0.0f )
	{
		if( pev->deadflag == DEAD_NO )
		{
			if( bitsDamageType & DMG_ENERGYBEAM )
				flDamage *= 2.0f;
			if( bitsDamageType & DMG_SHOCK )
				flDamage *= 4.0f;
		}
		else if( m_flExplode > 0.0f && RANDOM_LONG( 0, 2 ) == 0 )
			m_flExplode = gpGlobals->time;	// a hit to the wreck sets it off
		// metal: sparks where it is hit, a burst of them off the head
		UTIL_Sparks( ptr->vecEndPos );
		if( ptr->iHitgroup == HITGROUP_HEAD )
		{
			EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, g_RGSpark[RANDOM_LONG( 0, 5 )], 1.0f, ATTN_NORM, 0, RANDOM_LONG( 90, 110 ));
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
				WRITE_BYTE( TE_EXPLOSION );
				WRITE_COORD( ptr->vecEndPos.x );
				WRITE_COORD( ptr->vecEndPos.y );
				WRITE_COORD( ptr->vecEndPos.z - 20.0f );
				WRITE_SHORT( m_iSpark );
				WRITE_BYTE( RANDOM_LONG( 6, 8 ));
				WRITE_BYTE( 50 );
				WRITE_BYTE( TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES );
			MESSAGE_END();
		}
	}
	CHGrunt::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );	// (no blood: DONT_BLEED)
}

void CRoboGrunt::Killed( entvars_t *pevAttacker, int iGib )
{
	CHGrunt::Killed( pevAttacker, iGib );
	if( m_bBlown || m_flExplode > 0.0f )
		return;
	m_flExplode = gpGlobals->time + 3.0f + RANDOM_FLOAT( 0.0f, 3.0f );
	SetThink( &CRoboGrunt::SmokeThink );
	pev->nextthink = gpGlobals->time + 0.1f;
}

// the end of the death animation stops a monster's thinking (TASK_DIE): the wreck keeps smoking toward its blast
void CRoboGrunt::RunTask( Task_t *pTask )
{
	CHGrunt::RunTask( pTask );
	if( m_flExplode > 0.0f && !m_bBlown )
		SetThink( &CRoboGrunt::SmokeThink );
}

// the wreck smokes (and finishes falling) until it blows
void CRoboGrunt::SmokeThink( void )
{
	CBaseMonster::MonsterThink();
	if( gpGlobals->time >= m_flExplode )
	{
		GibMonster();
		return;
	}
	Vector at( RANDOM_FLOAT( pev->absmin.x, pev->absmax.x ), RANDOM_FLOAT( pev->absmin.y, pev->absmax.y ), pev->origin.z + 16.0f );
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, at );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( at.x );
		WRITE_COORD( at.y );
		WRITE_COORD( at.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 25 );
		WRITE_BYTE( 10 );
	MESSAGE_END();
	pev->nextthink = gpGlobals->time + 0.2f;
}

// blowing up: the blast, and metal parts instead of a body
void CRoboGrunt::GibMonster( void )
{
	if( m_bBlown )
		return;
	m_bBlown = true;
	pev->takedamage = DAMAGE_NO;	// not set off again by its own blast
	ExplosionCreate( pev->origin + Vector( 0, 0, 8 ), pev->angles, edict(), RGRUNT_EXPLODE, TRUE );
	Vector c = pev->origin + Vector( 0, 0, 36 );
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( c.x );
		WRITE_COORD( c.y );
		WRITE_COORD( c.z );
		WRITE_COORD( 32 );		// size
		WRITE_COORD( 32 );
		WRITE_COORD( 72 );
		WRITE_COORD( 0 );		// velocity
		WRITE_COORD( 0 );
		WRITE_COORD( 200 );
		WRITE_BYTE( 30 );		// random velocity
		WRITE_SHORT( m_iGibs );
		WRITE_BYTE( 12 );		// count
		WRITE_BYTE( 100 );		// life (0.1 s)
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();
	CHGrunt::GibMonster();		// its gun drops (a machine leaves no body gibs)
	pev->effects |= EF_NODRAW;
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1f;
}
