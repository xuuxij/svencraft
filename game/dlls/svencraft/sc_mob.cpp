/*
sc_mob.cpp - Svencraft: what the block world's creatures share (sc_mob.h): senses, walking, getting hurt, dying
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "sc_game.h"
#include "sc_mob.h"

static const char *g_SCMobSteps[] = { "common/npc_step1.wav", "common/npc_step2.wav", "common/npc_step3.wav", "common/npc_step4.wav" };
static const char *g_SCMobFlesh[] = { "debris/flesh1.wav", "debris/flesh2.wav", "debris/flesh3.wav" };

void CSCMob::MobPrecache( void )
{
	PRECACHE_SOUND_ARRAY( g_SCMobSteps );
	PRECACHE_SOUND_ARRAY( g_SCMobFlesh );
	PRECACHE_SOUND( "debris/bustflesh1.wav" );
}

void CSCMob::MobSpawn( const char *model, const Vector &mins, const Vector &maxs, float health, float eyes )
{
	SET_MODEL( ENT( pev ), model );
	UTIL_SetSize( pev, mins, maxs );

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->health = pev->max_health = health;
	pev->view_ofs = Vector( 0, 0, eyes );
	pev->deadflag = DEAD_NO;
	pev->yaw_speed = 360;
	pev->ideal_yaw = pev->angles.y;
	m_bloodColor = DONT_BLEED;
	m_flFieldOfView = -1.0f;	// sees all around
	m_MonsterState = MONSTERSTATE_ALERT;
	m_iCurAnim = -1;
	m_iHurtSkin = 1;
	m_flStepGap = 0.38f;
	m_bDying = false;
	m_flLastThink = gpGlobals->time;
	m_flNextWander = gpGlobals->time + RANDOM_FLOAT( 1, 4 );
	m_vecStuckPos = pev->origin;
	m_flStuckSince = gpGlobals->time;
	SetAnim( "idle", 1.0f );

	// dropped to the floor on the first think: the block world is generated once all entities exist
	m_bLanded = false;
	SetThink( &CSCMob::MobThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1f, 0.3f );
}

void CSCMob::SetAnim( const char *name, float rate )
{
	int seq = LookupSequence( name );
	if( seq < 0 )
		return;
	if( seq != m_iCurAnim )
	{
		pev->sequence = seq;
		pev->frame = 0;
		ResetSequenceInfo();
		m_iCurAnim = seq;
	}
	pev->framerate = rate;
}

void CSCMob::FindTarget( float sight )
{
	// keep a live, visible target; otherwise look around
	CBaseEntity *pEnemy = m_hEnemy;
	if( pEnemy && ( !pEnemy->IsAlive() || FBitSet( pEnemy->pev->flags, FL_NOTARGET ) || ( pEnemy->Center() - Center()).Length() > sight * 1.5f ))
		m_hEnemy = NULL;

	if( gpGlobals->time < m_flNextLook )
		return;
	m_flNextLook = gpGlobals->time + 0.4f;

	Look( (int)sight );
	CBaseEntity *pBest = BestVisibleEnemy();
	// a closer target wins over the one being chased (unless that one is already in reach)
	if( pBest && pBest != m_hEnemy )
	{
		CBaseEntity *pCur = m_hEnemy;
		if( !pCur || ( pBest->Center() - Center()).Length() + 64 < ( pCur->Center() - Center()).Length())
		{
			m_hEnemy = pBest;
			ALERT( at_aiconsole, "%s %d: target %s\n", STRING( pev->classname ), entindex(), STRING( pBest->pev->classname ));
		}
	}
}

void CSCMob::TurnTowards( const Vector &vecGoal, float dt )
{
	Vector d = vecGoal - pev->origin;
	if( d.Length2D() < 1 )
		return;
	float ideal = UTIL_VecToYaw( d );
	float cur = UTIL_AngleMod( pev->angles.y );
	float move = UTIL_AngleDiff( ideal, cur );
	float step = 300.0f * dt;
	if( move > step ) move = step;
	else if( move < -step ) move = -step;
	pev->angles.y = UTIL_AngleMod( cur + move );
	pev->ideal_yaw = ideal;
}

// a block in the way that it could stand on top of: hop
bool CSCMob::TryJump( const Vector &vecDir )
{
	if( !FBitSet( pev->flags, FL_ONGROUND ))
		return false;
	// the head hull is 36 tall around its origin: centred 20 up it covers 2..38 above the feet
	TraceResult low, high;
	Vector start = pev->origin + Vector( 0, 0, 20 );
	UTIL_TraceHull( start, start + vecDir * 24, dont_ignore_monsters, head_hull, ENT( pev ), &low );
	if( low.fStartSolid || low.flFraction >= 1.0f )
		return false;
	if( low.pHit && ( low.pHit->v.flags & ( FL_MONSTER | FL_CLIENT )))
		return false;	// someone in the way, not a step
	// room to rise one block, and to move over the step up there
	Vector up = start + Vector( 0, 0, 44 );
	UTIL_TraceHull( start, up, dont_ignore_monsters, head_hull, ENT( pev ), &high );
	if( high.fStartSolid || high.flFraction < 1.0f )
		return false;
	UTIL_TraceHull( up, up + vecDir * 28, dont_ignore_monsters, head_hull, ENT( pev ), &high );
	if( high.fStartSolid || high.flFraction < 1.0f )
		return false;
	pev->velocity = vecDir * 110 + Vector( 0, 0, 290 );	// clears one block
	ClearBits( pev->flags, FL_ONGROUND );
	return true;
}

// one step towards the goal (facing pFace if given: strafing); false when it got nowhere
bool CSCMob::Walk( const Vector &vecGoal, float flSpeed, float dt, const Vector *pFace )
{
	m_bBlocked = false;
	if( !FBitSet( pev->flags, FL_ONGROUND ))
		return true;	// in the air: the jump carries it

	Vector before = pev->origin;
	float dist = flSpeed * dt;
	Vector goal = vecGoal;

	// going round something for a moment
	if( gpGlobals->time < m_flDetourUntil )
	{
		UTIL_MakeVectors( Vector( 0, m_flDetourYaw, 0 ));
		goal = pev->origin + gpGlobals->v_forward * 64;
	}

	TurnTowards( pFace ? *pFace : goal, dt );
	MOVE_TO_ORIGIN( ENT( pev ), goal, dist, MOVE_STRAFE );

	Vector moved = pev->origin - before;
	if( moved.Length2D() > dist * 0.3f )
		return true;

	// blocked: a block to hop onto, a ledge to drop down (when the goal is below), or a way around
	Vector dir = goal - pev->origin;
	dir.z = 0;
	dir = dir.Normalize();
	if( TryJump( dir ))
		return true;
	if( vecGoal.z < pev->origin.z - 24 )
	{
		TraceResult tr;
		UTIL_TraceHull( pev->origin + Vector( 0, 0, 20 ), pev->origin + Vector( 0, 0, 20 ) + dir * 24, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if( tr.flFraction >= 1.0f )
		{
			pev->velocity = dir * flSpeed;
			ClearBits( pev->flags, FL_ONGROUND );
			return true;
		}
	}
	m_bBlocked = true;
	return false;
}

// after its target, going round for a second whenever it has been stuck a while
bool CSCMob::Chase( CBaseEntity *pEnemy, float flSpeed, float dt )
{
	Walk( pEnemy->pev->origin, flSpeed, dt );
	if(( pev->origin - m_vecStuckPos ).Length2D() > 24 )
	{
		m_vecStuckPos = pev->origin;
		m_flStuckSince = gpGlobals->time;
	}
	else if( gpGlobals->time - m_flStuckSince > 1.2f && gpGlobals->time > m_flDetourUntil )
	{
		m_flDetourYaw = pev->angles.y + ( RANDOM_LONG( 0, 1 ) ? 70 : -70 ) + RANDOM_FLOAT( -20, 20 );
		m_flDetourUntil = gpGlobals->time + 1.0f;
		m_flStuckSince = gpGlobals->time;
	}
	SetAnim( "walk", 1.0f );
	return true;
}

// nothing to do: amble about now and then
bool CSCMob::Wander( float flSpeed, float dt )
{
	if( gpGlobals->time > m_flNextWander )
	{
		UTIL_MakeVectors( Vector( 0, RANDOM_FLOAT( 0, 360 ), 0 ));
		m_vecWander = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( 80, 240 );
		m_flWanderUntil = gpGlobals->time + RANDOM_FLOAT( 2, 4 );
		m_flNextWander = m_flWanderUntil + RANDOM_FLOAT( 3, 8 );
	}
	if( gpGlobals->time < m_flWanderUntil && ( m_vecWander - pev->origin ).Length2D() > 16 )
	{
		if( !Walk( m_vecWander, flSpeed, dt ))
			m_flWanderUntil = 0;
		SetAnim( "walk", 0.45f );
		return true;
	}
	SetAnim( "idle", 1.0f );
	return false;
}

// the head turns to stare at the target, or at a player standing close by
void CSCMob::TurnHead( float dt, float limit )
{
	CBaseEntity *pLook = m_hEnemy;
	if( !pLook )
	{
		CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
		for( ; pPlayer; pPlayer = UTIL_FindEntityByClassname( pPlayer, "player" ))
			if( pPlayer->IsAlive() && ( pPlayer->pev->origin - pev->origin ).Length() < 320 )
			{
				pLook = pPlayer;
				break;
			}
	}
	float want = 0;
	if( pLook )
		want = UTIL_AngleDiff( UTIL_VecToYaw( pLook->pev->origin - pev->origin ), pev->angles.y );
	want = Q_max( -limit, Q_min( limit, want ));
	float step = 240.0f * dt;
	m_flHeadYaw += Q_max( -step, Q_min( step, want - m_flHeadYaw ));
	SetBoneController( 0, m_flHeadYaw );
}

// close enough to hit: the boxes within `reach` of each other, about level, and nothing between
bool CSCMob::InReach( CBaseEntity *pTarget, float reach )
{
	if( !pTarget )
		return false;
	float gap = ( pTarget->Center() - Center()).Length2D() - ( pTarget->pev->size.x + pev->size.x ) * 0.5f;
	if( gap > reach )
		return false;
	if( pTarget->pev->absmin.z > pev->absmax.z + 16 || pTarget->pev->absmax.z < pev->absmin.z - 16 )
		return false;
	return FVisible( pTarget ) != FALSE;
}

// a melee hit: hurt, pushed away a little, a sound
void CSCMob::Strike( CBaseEntity *pTarget, float flDamage, float push, const char *const *sounds, int nsounds )
{
	Vector dir = pTarget->pev->origin - pev->origin;
	dir.z = 0;
	dir = dir.Normalize();
	pTarget->TakeDamage( pev, pev, flDamage, DMG_SLASH );
	if( pTarget->IsAlive() && ( pTarget->pev->flags & ( FL_MONSTER | FL_CLIENT )) && pTarget->pev->movetype != MOVETYPE_FLY
		&& pTarget->pev->movetype != MOVETYPE_NONE && pTarget->pev->size.z < 100 )
	{
		pTarget->pev->velocity = pTarget->pev->velocity + dir * push + Vector( 0, 0, push * 0.5f );
		if( pTarget->IsPlayer())
			pTarget->pev->punchangle.x = -6;
	}
	if( nsounds > 0 )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, sounds[RANDOM_LONG( 0, nsounds - 1 )], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));
}

void CSCMob::MobThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1f;
	float dt = Q_min( gpGlobals->time - m_flLastThink, 0.25f );
	m_flLastThink = gpGlobals->time;
	StudioFrameAdvance();

	if( m_bDying )
		return;
	if( !m_bLanded )
	{
		DROP_TO_FLOOR( ENT( pev ));
		m_bLanded = true;
	}

	bool moving = Behave( dt );
	if( m_bDying )
		return;	// (it went off)

	if( moving && FBitSet( pev->flags, FL_ONGROUND ) && gpGlobals->time > m_flNextStep )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, RANDOM_SOUND_ARRAY( g_SCMobSteps ), StepVolume(), ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 10 ));
		m_flNextStep = gpGlobals->time + ( m_hEnemy != 0 ? m_flStepGap : m_flStepGap * 1.8f );
	}
	UpdateSkin();
}

void CSCMob::UpdateSkin( void )
{
	pev->skin = gpGlobals->time < m_flHurtUntil ? m_iHurtSkin : 0;
}

void CSCMob::HurtSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, RANDOM_SOUND_ARRAY( g_SCMobFlesh ), 0.9f, ATTN_NORM, 0, 120 + RANDOM_LONG( 0, 15 ));
}

void CSCMob::DieSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "debris/bustflesh1.wav", 0.8f, ATTN_NORM, 0, 130 );
}

int CSCMob::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pev->takedamage == DAMAGE_NO || m_bDying )
		return 0;

	// knocked back and flashing red, like back home
	m_flHurtUntil = gpGlobals->time + 0.35f;
	OnHurt( flDamage, bitsDamageType );
	// one push per half second (Minecraft's invulnerability ticks); bullets push less than a swing
	if( pevAttacker && !( bitsDamageType & DMG_BLAST ) && gpGlobals->time > m_flNextKnock )
	{
		Vector dir = pev->origin - pevAttacker->origin;
		dir.z = 0;
		dir = dir.Normalize();
		bool melee = ( bitsDamageType & ( DMG_CLUB | DMG_SLASH )) != 0;
		pev->velocity = dir * ( melee ? 220 : 90 ) + Vector( 0, 0, melee ? 180 : 90 );
		ClearBits( pev->flags, FL_ONGROUND );
		m_flNextKnock = gpGlobals->time + 0.5f;
	}
	HurtSound();

	// whoever hit it is the new target
	CBaseEntity *pAttacker = pevAttacker ? CBaseEntity::Instance( pevAttacker ) : NULL;
	if( pAttacker && pAttacker != this && ( pAttacker->IsPlayer() || pAttacker->MyMonsterPointer()) && IRelationship( pAttacker ) > R_NO )
		m_hEnemy = pAttacker;

	UpdateSkin();

	pev->health -= flDamage;
	ALERT( at_aiconsole, "%s %d: hit for %.0f (type %x), health %.0f\n", STRING( pev->classname ), entindex(), flDamage, bitsDamageType, pev->health );
	if( pev->health <= 0 )
	{
		Killed( pevAttacker, GIB_NORMAL );
		return 0;
	}
	return 1;
}

void CSCMob::Killed( entvars_t *pevAttacker, int iGib )
{
	if( m_bDying )
		return;
	OnKilled();
	DieSound();
	Drops();
	m_bDying = true;
	pev->deadflag = DEAD_DYING;
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;
	pev->scale = 1.0f;
	pev->skin = m_iHurtSkin;
	SetBoneController( 0, 0 );
	SetAnim( "die", 1.0f );
	SetThink( &CSCMob::DyingThink );
	pev->nextthink = gpGlobals->time + 1.1f;
}

void CSCMob::DyingThink( void )
{
	// a puff of smoke and it's gone
	Vector c = pev->origin + Vector( 0, 0, 12 );
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( c.x );
		WRITE_COORD( c.y );
		WRITE_COORD( c.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 12 );	// scale * 10
		WRITE_BYTE( 20 );	// framerate
	MESSAGE_END();
	UTIL_Remove( this );
}
