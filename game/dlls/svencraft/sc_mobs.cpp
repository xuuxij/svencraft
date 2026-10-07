/*
sc_mobs.cpp - Svencraft: the block world's zombie, skeleton and spider, and the skeleton's arrows

Minecraft's numbers, health and damage x5 like everything here (20 health there is 100 here):
  zombie    100 health, a 15 swipe once a second, arms out; groans. Drops rotten flesh, now and then an iron ingot.
  skeleton  100 health; with a target in sight within 16 blocks it strafes, keeping 4 to 12 blocks off, draws its
            bow for a second and looses an arrow (15-20) every two seconds, aimed for the drop and where you're
            going. Drops bones, sometimes a box of crossbow bolts.
  spider    80 health, a 10 bite, fast; leaps at you from 2 to 6 blocks off and climbs straight up walls.
            Drops string.
Models: tools/make_mobs.py. They walk and see as CSCMob does (sc_mob.cpp).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "sc_game.h"
#include "sc_mob.h"

//=========================================================
// zombie
//=========================================================
#define ZOMBIE_MODEL	"models/svencraft/zombie.mdl"

class CSCZombie : public CSCMob
{
public:
	void Spawn( void );
	void Precache( void );

protected:
	bool Behave( float dt );
	void Drops( void );
	void HurtSound( void );
	void DieSound( void );

	float	m_flNextAttack, m_flAttackEnd, m_flNextGroan;

	static const char *pIdleSounds[];
	static const char *pPainSounds[];
	static const char *pHitSounds[];
};

LINK_ENTITY_TO_CLASS( monster_sc_zombie, CSCZombie )

const char *CSCZombie::pIdleSounds[] = { "zombie/zo_idle1.wav", "zombie/zo_idle2.wav", "zombie/zo_idle3.wav", "zombie/zo_idle4.wav" };
const char *CSCZombie::pPainSounds[] = { "zombie/zo_pain1.wav", "zombie/zo_pain2.wav" };
const char *CSCZombie::pHitSounds[] = { "zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav" };

void CSCZombie::Precache( void )
{
	MobPrecache();
	PRECACHE_MODEL( ZOMBIE_MODEL );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pHitSounds );
}

void CSCZombie::Spawn( void )
{
	Precache();
	MobSpawn( ZOMBIE_MODEL, Vector( -12, -12, 0 ), Vector( 12, 12, 72 ), 100, 64 );
	m_flStepGap = 0.42f;
	m_flNextGroan = gpGlobals->time + RANDOM_FLOAT( 2, 8 );
}

bool CSCZombie::Behave( float dt )
{
	FindTarget( 1400 );
	TurnHead( dt );
	if( gpGlobals->time > m_flNextGroan )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pIdleSounds ), 0.8f, ATTN_NORM, 0, RANDOM_LONG( 60, 72 ));
		m_flNextGroan = gpGlobals->time + RANDOM_FLOAT( 6, 14 );
	}

	CBaseEntity *pEnemy = m_hEnemy;
	if( gpGlobals->time < m_flAttackEnd )
	{
		// mid-swipe
		if( pEnemy )
			TurnTowards( pEnemy->pev->origin, dt );
		return false;
	}
	if( pEnemy )
	{
		if( InReach( pEnemy, 20 ))
		{
			TurnTowards( pEnemy->pev->origin, dt );
			if( gpGlobals->time > m_flNextAttack )
			{
				SetAnim( "attack", 1.0f );
				m_flAttackEnd = gpGlobals->time + 0.5f;
				m_flNextAttack = gpGlobals->time + 1.0f;
				Strike( pEnemy, 15, 180, pHitSounds, ARRAYSIZE( pHitSounds ));
			}
			else
				SetAnim( "idle", 1.0f );
			return false;
		}
		return Chase( pEnemy, 100, dt );
	}
	return Wander( 40, dt );
}

void CSCZombie::HurtSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0f, ATTN_NORM, 0, RANDOM_LONG( 70, 80 ));
}

void CSCZombie::DieSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "zombie/zo_pain2.wav", 1.0f, ATTN_NORM, 0, 58 );
}

void CSCZombie::Drops( void )
{
	int flesh = RANDOM_LONG( 0, 2 );
	if( flesh )
		SC_SpawnDrop( pev->origin + Vector( 0, 0, 24 ), SCITEM_ROTTEN_FLESH, flesh );
	if( RANDOM_LONG( 0, 39 ) == 0 )
		SC_SpawnDrop( pev->origin + Vector( 0, 0, 24 ), SCITEM_IRON_INGOT, 1 );	// Minecraft's rare drop
}

//=========================================================
// skeleton
//=========================================================
#define SKELETON_MODEL	"models/svencraft/skeleton.mdl"
#define SKELETON_RANGE	640.0f	// 16 blocks
#define SKELETON_DRAW	1.0f	// seconds drawing the bow
#define SKELETON_REST	1.0f	// and between shots

class CSCSkeleton : public CSCMob
{
public:
	void Spawn( void );
	void Precache( void );

protected:
	bool Behave( float dt );
	void Drops( void );
	void HurtSound( void );
	void DieSound( void );

	float	m_flSeen;		// how long the target has been in sight
	float	m_flDraw;		// how long the bow has been drawn
	float	m_flNextShot;
	float	m_flNextStrafe;
	int	m_iStrafe;		// +1 left, -1 right
	float	m_flNextRattle;

	static const char *pBoneSounds[];
};

LINK_ENTITY_TO_CLASS( monster_sc_skeleton, CSCSkeleton )

const char *CSCSkeleton::pBoneSounds[] = { "debris/wood1.wav", "debris/wood2.wav", "debris/wood3.wav" };

void CSCSkeleton::Precache( void )
{
	MobPrecache();
	PRECACHE_MODEL( SKELETON_MODEL );
	PRECACHE_SOUND_ARRAY( pBoneSounds );
	PRECACHE_SOUND( "debris/bustcrate1.wav" );
	PRECACHE_SOUND( "weapons/xbow_fire1.wav" );
	UTIL_PrecacheOther( "sc_arrow" );
}

void CSCSkeleton::Spawn( void )
{
	Precache();
	MobSpawn( SKELETON_MODEL, Vector( -10, -10, 0 ), Vector( 10, 10, 72 ), 100, 64 );
	m_flStepGap = 0.42f;
	m_iStrafe = RANDOM_LONG( 0, 1 ) ? 1 : -1;
	m_flNextRattle = gpGlobals->time + RANDOM_FLOAT( 3, 10 );
}

bool CSCSkeleton::Behave( float dt )
{
	FindTarget( 1200 );
	TurnHead( dt );
	if( gpGlobals->time > m_flNextRattle )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pBoneSounds ), 0.4f, ATTN_NORM, 0, RANDOM_LONG( 150, 170 ));
		m_flNextRattle = gpGlobals->time + RANDOM_FLOAT( 6, 14 );
	}

	CBaseEntity *pEnemy = m_hEnemy;
	float dist = pEnemy ? ( pEnemy->Center() - Center()).Length() : 0;
	if( pEnemy && dist < SKELETON_RANGE && FVisible( pEnemy ))
	{
		m_flSeen += dt;
		// Minecraft's bow AI: strafing round its target, closing in from far off, backing off when it's near
		if( gpGlobals->time > m_flNextStrafe )
		{
			if( RANDOM_LONG( 0, 9 ) < 3 )
				m_iStrafe = -m_iStrafe;
			m_flNextStrafe = gpGlobals->time + 1.0f;
		}
		Vector to = pEnemy->pev->origin - pev->origin;
		to.z = 0;
		to = to.Normalize();
		Vector side( -to.y, to.x, 0 );
		float fwd = dist > SKELETON_RANGE * 0.75f ? 1.0f : ( dist < SKELETON_RANGE * 0.25f ? -1.0f : 0.0f );
		Vector dir = ( to * fwd + side * ( m_iStrafe * 0.7f )).Normalize();
		Vector face = pEnemy->pev->origin;
		bool moved = Walk( pev->origin + dir * 64, 70, dt, &face );
		if( m_bBlocked )
			m_iStrafe = -m_iStrafe;

		// the bow: drawn for a second, loosed, a second's rest
		if( gpGlobals->time > m_flNextShot && m_flSeen > 0.5f )
			m_flDraw += dt;
		if( m_flDraw >= SKELETON_DRAW )
		{
			UTIL_MakeVectors( Vector( 0, pev->angles.y, 0 ));
			Vector src = pev->origin + Vector( 0, 0, 54 ) + gpGlobals->v_forward * 14;
			SC_ShootArrow( this, src, pEnemy, 1200, 0.025f );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/xbow_fire1.wav", 0.9f, ATTN_NORM, 0, RANDOM_LONG( 110, 125 ));
			m_flDraw = 0;
			m_flNextShot = gpGlobals->time + SKELETON_REST;
		}
		SetAnim( moved && FBitSet( pev->flags, FL_ONGROUND ) ? "aimwalk" : "aim", 1.0f );
		return moved;
	}
	m_flSeen = 0;
	m_flDraw = 0;
	if( pEnemy )
		return Chase( pEnemy, 95, dt );
	return Wander( 40, dt );
}

void CSCSkeleton::HurtSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pBoneSounds ), 1.0f, ATTN_NORM, 0, RANDOM_LONG( 135, 150 ));
}

void CSCSkeleton::DieSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "debris/bustcrate1.wav", 0.9f, ATTN_NORM, 0, 140 );
}

void CSCSkeleton::Drops( void )
{
	int bones = RANDOM_LONG( 0, 2 );
	if( bones )
		SC_SpawnDrop( pev->origin + Vector( 0, 0, 24 ), SCITEM_BONE, bones );
	if( RANDOM_LONG( 0, 2 ) == 0 )
		CBaseEntity::Create( "ammo_crossbow", pev->origin + Vector( 0, 0, 16 ), Vector( 0, RANDOM_FLOAT( 0, 360 ), 0 ), NULL );	// its arrows
}

//=========================================================
// spider
//=========================================================
#define SPIDER_MODEL	"models/svencraft/spider.mdl"

class CSCSpider : public CSCMob
{
public:
	void Spawn( void );
	void Precache( void );

protected:
	bool Behave( float dt );
	void Drops( void );
	void HurtSound( void );
	void DieSound( void );
	float StepVolume( void ) { return 0.18f; }

	float	m_flNextAttack, m_flAttackEnd, m_flNextLeap, m_flNextHiss;
	bool	m_bClimbing;

	static const char *pIdleSounds[];
	static const char *pPainSounds[];
	static const char *pBiteSounds[];
};

LINK_ENTITY_TO_CLASS( monster_sc_spider, CSCSpider )

const char *CSCSpider::pIdleSounds[] = { "headcrab/hc_idle1.wav", "headcrab/hc_idle2.wav", "headcrab/hc_idle3.wav" };
const char *CSCSpider::pPainSounds[] = { "headcrab/hc_pain1.wav", "headcrab/hc_pain2.wav", "headcrab/hc_pain3.wav" };
const char *CSCSpider::pBiteSounds[] = { "headcrab/hc_headbite.wav" };

void CSCSpider::Precache( void )
{
	MobPrecache();
	PRECACHE_MODEL( SPIDER_MODEL );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pBiteSounds );
	PRECACHE_SOUND( "headcrab/hc_attack1.wav" );
	PRECACHE_SOUND( "headcrab/hc_die1.wav" );
}

void CSCSpider::Spawn( void )
{
	Precache();
	// a 32 wide, 36 tall box: one block high is enough for it (the head hull)
	MobSpawn( SPIDER_MODEL, Vector( -16, -16, 0 ), Vector( 16, 16, 36 ), 80, 24 );
	m_flStepGap = 0.22f;
	m_flNextHiss = gpGlobals->time + RANDOM_FLOAT( 2, 6 );
	m_bClimbing = false;
}

bool CSCSpider::Behave( float dt )
{
	FindTarget( 1000 );
	TurnHead( dt, 40 );
	if( gpGlobals->time > m_flNextHiss )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pIdleSounds ), 0.7f, ATTN_NORM, 0, RANDOM_LONG( 55, 65 ));
		m_flNextHiss = gpGlobals->time + RANDOM_FLOAT( 4, 10 );
	}

	CBaseEntity *pEnemy = m_hEnemy;
	bool onGround = FBitSet( pev->flags, FL_ONGROUND ) != 0;

	// climbing a wall: straight up while it's still in front, then over the top
	if( m_bClimbing )
	{
		if( onGround && !pEnemy )
			m_bClimbing = false;
		else
		{
			UTIL_MakeVectors( Vector( 0, pev->angles.y, 0 ));
			Vector fwd = gpGlobals->v_forward;
			TraceResult tr;
			Vector start = pev->origin + Vector( 0, 0, 20 );
			UTIL_TraceHull( start, start + fwd * 24, ignore_monsters, head_hull, ENT( pev ), &tr );
			if( tr.flFraction < 1.0f && !tr.fStartSolid )
			{
				pev->velocity = fwd * 60 + Vector( 0, 0, 160 );
				ClearBits( pev->flags, FL_ONGROUND );
				SetAnim( "walk", 1.4f );
				return true;
			}
			// the top: onto it
			pev->velocity = fwd * 140 + Vector( 0, 0, 120 );
			m_bClimbing = false;
		}
	}

	if( gpGlobals->time < m_flAttackEnd )
	{
		if( pEnemy )
			TurnTowards( pEnemy->pev->origin, dt );
		return false;
	}
	if( pEnemy )
	{
		if( InReach( pEnemy, 16 ))
		{
			TurnTowards( pEnemy->pev->origin, dt );
			if( gpGlobals->time > m_flNextAttack )
			{
				SetAnim( "attack", 1.0f );
				m_flAttackEnd = gpGlobals->time + 0.5f;
				m_flNextAttack = gpGlobals->time + 1.0f;
				Strike( pEnemy, 10, 120, pBiteSounds, ARRAYSIZE( pBiteSounds ));
			}
			else
				SetAnim( "idle", 1.0f );
			return false;
		}
		// Minecraft's leap: from 2 to 6 blocks off
		float d = ( pEnemy->pev->origin - pev->origin ).Length2D();
		if( onGround && d > 80 && d < 240 && gpGlobals->time > m_flNextLeap && FVisible( pEnemy ))
		{
			Vector dir = pEnemy->pev->origin - pev->origin;
			dir.z = 0;
			dir = dir.Normalize();
			pev->velocity = dir * 330 + Vector( 0, 0, 280 );
			ClearBits( pev->flags, FL_ONGROUND );
			pev->angles.y = UTIL_VecToYaw( dir );
			m_flNextLeap = gpGlobals->time + RANDOM_FLOAT( 1.5f, 3.0f );
			SetAnim( "attack", 1.0f );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "headcrab/hc_attack1.wav", 0.8f, ATTN_NORM, 0, RANDOM_LONG( 65, 75 ));
			return true;
		}
		bool moved = Chase( pEnemy, 140, dt );
		// a wall it can't hop: up it
		if( m_bBlocked && onGround && gpGlobals->time > m_flDetourUntil )
			m_bClimbing = true;
		return moved;
	}
	return Wander( 50, dt );
}

void CSCSpider::HurtSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0f, ATTN_NORM, 0, RANDOM_LONG( 60, 70 ));
}

void CSCSpider::DieSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "headcrab/hc_die1.wav", 1.0f, ATTN_NORM, 0, 55 );
}

void CSCSpider::Drops( void )
{
	int string = RANDOM_LONG( 0, 2 );
	if( string )
		SC_SpawnDrop( pev->origin + Vector( 0, 0, 16 ), SCITEM_STRING, string );
}

//=========================================================
// the arrow: falls like Minecraft's (its gravity is ours), sticks where it lands
//=========================================================
#define ARROW_MODEL	"models/svencraft/arrow.mdl"

class CSCArrow : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_NONE; }
	void EXPORT FlyThink( void );
	void EXPORT ArrowTouch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( sc_arrow, CSCArrow )

void CSCArrow::Precache( void )
{
	PRECACHE_MODEL( ARROW_MODEL );
	PRECACHE_SOUND( "weapons/xbow_hit1.wav" );
	PRECACHE_SOUND( "weapons/xbow_hitbod1.wav" );
	PRECACHE_SOUND( "weapons/xbow_hitbod2.wav" );
}

void CSCArrow::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->gravity = 1.0f;
	SET_MODEL( ENT( pev ), ARROW_MODEL );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( &CSCArrow::ArrowTouch );
	SetThink( &CSCArrow::FlyThink );
	pev->nextthink = gpGlobals->time + 0.02f;
	pev->dmgtime = gpGlobals->time;
}

void CSCArrow::FlyThink( void )
{
	pev->angles = UTIL_VecToAngles( pev->velocity );	// along its flight
	pev->nextthink = gpGlobals->time + 0.05f;
	if( gpGlobals->time - pev->dmgtime > 8.0f )
		UTIL_Remove( this );
}

void CSCArrow::ArrowTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );
	Vector dir = pev->velocity.Normalize();
	entvars_t *pevOwner = pev->owner ? VARS( pev->owner ) : pev;
	if( pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		ClearMultiDamage();
		pOther->TraceAttack( pevOwner, pev->dmg, dir, &tr, DMG_BULLET | DMG_NEVERGIB );
		ApplyMultiDamage( pev, pevOwner );
		if( pOther->IsAlive() && ( pOther->pev->flags & ( FL_MONSTER | FL_CLIENT )) && pOther->pev->movetype != MOVETYPE_FLY
			&& pOther->pev->movetype != MOVETYPE_NONE && pOther->pev->size.z < 100 )
		{
			Vector push = dir;
			push.z = 0;
			pOther->pev->velocity = pOther->pev->velocity + push.Normalize() * 110 + Vector( 0, 0, 60 );
		}
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, RANDOM_LONG( 0, 1 ) ? "weapons/xbow_hitbod1.wav" : "weapons/xbow_hitbod2.wav", 1.0f, ATTN_NORM, 0, 100 );
		pev->effects |= EF_NODRAW;
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1f;
		return;
	}
	// stuck in whatever it hit, for a while
	EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "weapons/xbow_hit1.wav", 0.8f, ATTN_NORM, 0, 110 + RANDOM_LONG( 0, 10 ));
	pev->angles = UTIL_VecToAngles( dir );
	UTIL_SetOrigin( pev, pev->origin - dir * 2 );
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 10.0f;
}

// shoots at where the target will be, high enough to drop onto it (the low arc), a little off like a mob's aim
CBaseEntity *SC_ShootArrow( CBaseEntity *pShooter, const Vector &vecSrc, CBaseEntity *pTarget, float flSpeed, float flSpread )
{
	Vector tgt = pTarget->BodyTarget( vecSrc );
	float t = ( tgt - vecSrc ).Length() / flSpeed;
	tgt = tgt + pTarget->pev->velocity * t * 0.6f;
	Vector d = tgt - vecSrc;
	float x = d.Length2D(), y = d.z, g = CVAR_GET_FLOAT( "sv_gravity" ), v2 = flSpeed * flSpeed;
	float disc = v2 * v2 - g * ( g * x * x + 2 * y * v2 );
	float pitch = ( disc < 0 || x < 1 ) ? (float)( M_PI / 4 ) : atanf(( v2 - sqrtf( disc )) / ( g * x ));
	Vector flat = Vector( d.x, d.y, 0 ).Normalize();
	Vector vel = flat * ( cosf( pitch ) * flSpeed ) + Vector( 0, 0, sinf( pitch ) * flSpeed );
	vel = vel + Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 )) * ( flSpread * flSpeed );

	CBaseEntity *pArrow = CBaseEntity::Create( "sc_arrow", vecSrc, UTIL_VecToAngles( vel ), pShooter->edict());
	if( !pArrow )
		return NULL;
	pArrow->pev->velocity = vel;
	pArrow->pev->dmg = RANDOM_FLOAT( 15, 20 );	// Minecraft's 3-4 x5
	return pArrow;
}
