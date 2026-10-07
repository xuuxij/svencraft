/*
sc_stukabat.cpp - Svencraft: Sven Co-op's Stukabat (monster_stukabat), after SevenKewp's CStukabat

A Xen bat (Sven's stukabat.mdl): 123 health (Sven's sk_stukabat), so delicate that anything heavier than a pistol
round makes it flinch. It flies up and away from its enemy to somewhere open, then folds its wings and dives on
them, gaining speed to a thousand units a second, aiming where they're going; a hit bites (sk_stukabat_dmg_bite) and
knocks the view up. Then it hovers, faces them, and climbs away for the next dive. Killed, it drops out of the air
and dies on the ground. Flying is the alien controller's way (Half-Life's), with the dive added.
SevenKewp's needs its own monster base: this stands on Half-Life's.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "weapons.h"
#include "effects.h"
#include "soundent.h"
#include "studio.h"
#include "skill.h"

#define STUKABAT_AE_FLAP	9	// the model's wingbeat (the flying and hovering cycles)
#define STUKABAT_FLAP_SOUND	"stukabat/stukabat_flap1.wav"
#define STUKABAT_DIVE_MAX	1000	// the fastest dive
#define STUKABAT_DIVE_FLY	400	// flapping toward the target
#define STUKABAT_CIRCLE		300	// climbing away, or roaming
#define STUKABAT_CHECK_DIST	200

static float SC_StukabatValue( const char *name, float sven )
{
	float v = GetSkillCvar( name );
	return v > 0.0f ? v : sven;
}

class CStukabat : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void ) { pev->yaw_speed = 120; }
	int Classify( void ) { return CLASS_ALIEN_MONSTER; }
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void RunAI( void );
	BOOL CheckRangeAttack1( float flDot, float flDist ) { return flDist > 256 && flDist <= 4096; }	// a dive
	BOOL CheckRangeAttack2( float flDot, float flDist ) { return FALSE; }
	BOOL CheckMeleeAttack1( float flDot, float flDist ) { return m_iDiving == 2 && flDist < 350; }	// claws out
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	void EXPORT DiveTouch( CBaseEntity *pOther );
	void Move( float flInterval );
	int CheckLocalMove( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist );
	void MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval );
	void SetActivity( Activity NewActivity );
	BOOL ShouldAdvanceRoute( float flWaypointDist ) { return flWaypointDist <= 32; }
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void DeathSound( void );

	CUSTOM_SCHEDULES;

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];
	static const char *pAttackHitSounds[];

	Vector	m_velocity;	// (a flyer's own: pev->velocity isn't how it moves)
	int	m_iDiving;	// 0 not, 1 wings folded, 2 flapping toward the target
};

LINK_ENTITY_TO_CLASS( monster_stukabat, CStukabat )

const char *CStukabat::pAttackSounds[] = { "controller/con_attack1.wav", "controller/con_attack2.wav", "controller/con_attack3.wav" };
const char *CStukabat::pIdleSounds[] = { "controller/con_idle1.wav", "controller/con_idle2.wav", "controller/con_idle3.wav", "controller/con_idle4.wav", "controller/con_idle5.wav" };
const char *CStukabat::pAlertSounds[] = { "controller/con_alert1.wav", "controller/con_alert2.wav", "controller/con_alert3.wav" };
const char *CStukabat::pPainSounds[] = { "controller/con_pain1.wav", "controller/con_pain2.wav", "controller/con_pain3.wav" };
const char *CStukabat::pDeathSounds[] = { "controller/con_die1.wav", "controller/con_die2.wav" };
const char *CStukabat::pAttackHitSounds[] = { "zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav" };

enum
{
	TASK_STUKABAT_PATH_ABOVE_ENEMY = LAST_COMMON_TASK + 1,	// up and away from the enemy, for a dive
};

Task_t tlStukabatRetreat[] =
{
	{ TASK_STUKABAT_PATH_ABOVE_ENEMY,	0 },
	{ TASK_FACE_IDEAL,			0 },
	{ TASK_SET_ACTIVITY,			(float)ACT_FLY },
	{ TASK_WAIT_FOR_MOVEMENT,		0 },
};

Schedule_t slStukabatRetreat[] =
{
	{ tlStukabatRetreat, ARRAYSIZE( tlStukabatRetreat ), bits_COND_HEAVY_DAMAGE | bits_COND_TASK_FAILED, 0, "STUKABAT_RETREAT" },
};

Task_t tlStukabatDive[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY },
	{ TASK_GET_PATH_TO_ENEMY,	128 },
	{ TASK_RANGE_ATTACK1,		0 },
	{ TASK_WAIT_FOR_MOVEMENT,	0 },
	{ TASK_SET_ACTIVITY,		(float)ACT_HOVER },
	{ TASK_WAIT_FACE_ENEMY,		0.5f },
};

Schedule_t slStukabatDive[] =
{
	{ tlStukabatDive, ARRAYSIZE( tlStukabatDive ), bits_COND_HEAVY_DAMAGE, 0, "STUKABAT_DIVE_ATTACK" },
};

Task_t tlStukabatFlinch[] =
{
	{ TASK_REMEMBER,	(float)bits_MEMORY_FLINCHED },
	{ TASK_SMALL_FLINCH,	0 },
};

Schedule_t slStukabatFlinch[] =
{
	{ tlStukabatFlinch, ARRAYSIZE( tlStukabatFlinch ), bits_COND_HEAVY_DAMAGE, 0, "STUKABAT_FLINCH" },
};

Task_t tlStukabatFail[] =
{
	{ TASK_STOP_MOVING,	0 },
	{ TASK_SET_ACTIVITY,	(float)ACT_HOVER },
	{ TASK_WAIT,		0.5f },
	{ TASK_WAIT_PVS,	0 },
};

Schedule_t slStukabatFail[] =
{
	{ tlStukabatFail, ARRAYSIZE( tlStukabatFail ), 0, 0, "STUKABAT_FAIL" },
};

DEFINE_CUSTOM_SCHEDULES( CStukabat )
{
	slStukabatRetreat,
	slStukabatDive,
	slStukabatFlinch,
	slStukabatFail,
};

IMPLEMENT_CUSTOM_SCHEDULES( CStukabat, CBaseMonster )

void CStukabat::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/stukabat.mdl" );
	UTIL_SetSize( pev, Vector( -32, -32, 0 ), Vector( 32, 32, 32 ));

	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	pev->flags		|= FL_FLY;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->health		= SC_StukabatValue( "sk_stukabat", 123 );
	pev->view_ofs		= Vector( 0, 0, -2 );
	m_flFieldOfView		= VIEW_FIELD_WIDE;
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_RANGE_ATTACK1 | bits_CAP_MELEE_ATTACK1;
	m_iDiving = 0;
	m_velocity = g_vecZero;

	MonsterInit();
}

void CStukabat::Precache( void )
{
	PRECACHE_MODEL( "models/stukabat.mdl" );
	PRECACHE_SOUND( STUKABAT_FLAP_SOUND );
	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDeathSounds );
	PRECACHE_SOUND_ARRAY( pAttackHitSounds );
}

void CStukabat::PainSound( void )
{
	if( RANDOM_LONG( 0, 3 ) < 2 )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( 150, 160 ));
}

void CStukabat::AlertSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAlertSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( 150, 160 ));
}

void CStukabat::IdleSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pIdleSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( 150, 160 ));
}

void CStukabat::AttackSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAttackSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( 150, 160 ));
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3f );
}

void CStukabat::DeathSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pDeathSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( 150, 160 ));
}

void CStukabat::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case STUKABAT_AE_FLAP:
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, STUKABAT_FLAP_SOUND, 0.6, ATTN_STATIC, 0, RANDOM_LONG( 120, 130 ));
		break;
	case 8:		// (the flying cycle's other mark: nothing to do)
		break;
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

// the activities the model's sequences stand for: claws (the first "range attack 2"), the folded-wing dive (the
// second), falling dead (the first "die simple"), hovering when idle; a hard flinch while diving
void CStukabat::SetActivity( Activity NewActivity )
{
	int act = NewActivity, nth = 0;
	switch( NewActivity )
	{
	case ACT_MELEE_ATTACK1:	act = ACT_RANGE_ATTACK2; nth = 0; break;
	case ACT_RANGE_ATTACK2:	act = ACT_RANGE_ATTACK2; nth = 1; break;
	case ACT_DIESIMPLE:	act = ACT_DIESIMPLE; nth = 0; break;
	case ACT_IDLE:
	case ACT_WALK:		act = ACT_HOVER; nth = -1; break;
	case ACT_SMALL_FLINCH:	act = m_iDiving ? ACT_BIG_FLINCH : ACT_SMALL_FLINCH; nth = -1; break;
	default:		nth = -1; break;
	}
	int iSequence = -1;
	studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR( ENT( pev ));
	if( hdr && nth >= 0 )
	{
		mstudioseqdesc_t *seq = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex );
		for( int i = 0, n = 0; i < hdr->numseq; i++ )
		{
			if( seq[i].activity == act && n++ == nth )
			{
				iSequence = i;
				break;
			}
		}
	}
	else
		iSequence = LookupActivity( act );

	Activity old = m_Activity;
	m_Activity = m_IdealActivity = NewActivity;
	if( iSequence < 0 )
	{
		pev->sequence = 0;
		return;
	}
	if( pev->sequence != iSequence || !m_fSequenceLoops )
	{
		if( !( old == ACT_WALK || old == ACT_RUN ) || !( NewActivity == ACT_WALK || NewActivity == ACT_RUN ))
			pev->frame = 0;
	}
	pev->sequence = iSequence;
	ResetSequenceInfo();
	SetYawSpeed();
}

void CStukabat::DiveTouch( CBaseEntity *pOther )
{
	if( pOther && pOther->pev->takedamage )
	{
		Vector start = BodyTarget( pev->origin );
		Vector end = pOther->BodyTarget( pev->origin );
		TraceResult tr;
		UTIL_TraceLine( start, end, dont_ignore_monsters, edict(), &tr );
		ClearMultiDamage();
		pOther->TraceAttack( pev, SC_StukabatValue( "sk_stukabat_dmg_bite", 12 ), ( end - start ).Normalize(), &tr, DMG_CLUB );
		ApplyMultiDamage( pev, pev );
		pOther->pev->punchangle.x = 18;
		EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));
	}
	m_iDiving = 0;
	m_velocity = g_vecZero;
	m_flGroundSpeed = 1;	// (not 0: that would make a NaN origin)
	TaskComplete();
	SetTouch( NULL );
}

void CStukabat::StartTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_DIE:
		// it drops out of the air
		CBaseMonster::StartTask( pTask );
		SetActivity( ACT_FALL );
		pev->angles.x = 0;
		pev->movetype = MOVETYPE_TOSS;
		pev->velocity = m_velocity;
		SetTouch( NULL );
		break;
	case TASK_RANGE_ATTACK1:
		m_iDiving = 2;
		SetTouch( &CStukabat::DiveTouch );
		TaskComplete();
		AttackSound();
		break;
	case TASK_SMALL_FLINCH:
		CBaseMonster::StartTask( pTask );
		m_movementActivity = ACT_SMALL_FLINCH;
		SetTouch( NULL );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( 150, 160 ));
		break;
	case TASK_STUKABAT_PATH_ABOVE_ENEMY:
	{
		// the furthest open point from here (looking up first, then level, then down a little), for the next dive
		if( m_hEnemy == 0 )
		{
			TaskFail();
			break;
		}
		int oldSolid = pev->solid;
		pev->solid = SOLID_NOT;	// (its own box out of the traces)
		float bestDist = 0;
		Vector best = g_vecZero;
		bool found = false;
		static const float pitches[4] = { -45, 0, 10, 45 };
		for( int k = 0; k < 4 && !found; k++ )
		{
			float angle = RANDOM_FLOAT( 0, 360 );
			for( int i = 0; i < 18; i++ )
			{
				angle += 20;
				UTIL_MakeVectors( Vector( pitches[k], angle, 0 ));
				TraceResult tr;
				TRACE_MONSTER_HULL( edict(), pev->origin, pev->origin + gpGlobals->v_forward * 1024, dont_ignore_monsters, m_hEnemy->edict(), &tr );
				float dist = ( tr.vecEndPos - pev->origin ).Length();
				if( dist < 256 )
					continue;
				if( dist > bestDist )
				{
					bestDist = dist;
					best = tr.vecEndPos - gpGlobals->v_forward * 32;	// a little off any wall
					found = true;
				}
			}
		}
		pev->solid = oldSolid;
		if( !found )
		{
			TaskFail();
			break;
		}
		AttackSound();
		if( BuildRoute( best, bits_MF_TO_LOCATION, NULL ) || BuildNearestRoute( best, pev->view_ofs, 0, ( best - pev->origin ).Length()))
			TaskComplete();
		else
			TaskFail();
		break;
	}
	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

void CStukabat::RunTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_DIE:
		if( pev->flags & FL_ONGROUND )
		{
			pev->angles.x = 0;
			if( m_Activity != ACT_DIESIMPLE && m_Activity != ACT_DIEVIOLENT )
			{
				SetActivity( RANDOM_LONG( 0, 2 ) == 0 ? ACT_DIEVIOLENT : ACT_DIESIMPLE );
				pev->framerate = 1.5f;
			}
			CBaseMonster::RunTask( pTask );
		}
		else
		{
			// nose down, the wings slowing the fall a little
			pev->angles.x = UTIL_VecToAngles( pev->velocity ).x + 60;
			pev->velocity = pev->velocity * 0.95f;
		}
		break;
	case TASK_WAIT_FACE_ENEMY:
		m_iDiving = 0;
		m_IdealActivity = ACT_HOVER;
		m_movementActivity = ACT_HOVER;
		m_flGroundSpeed = 0;
		CBaseMonster::RunTask( pTask );
		break;
	case TASK_SMALL_FLINCH:
		// the big flinch cut short, so it doesn't hang waiting for the schedule to change
		if( m_fSequenceFinished || ( pev->sequence == 8 && pev->frame > 200 ))
		{
			TaskComplete();
			m_flGroundSpeed = 100;
			m_velocity = g_vecZero;
			m_iDiving = 0;
		}
		break;
	case TASK_WAIT_FOR_MOVEMENT:
	case TASK_WAIT:
	case TASK_MOVE_TO_TARGET_RANGE:
	case TASK_WAIT_PVS:
	{
		if( m_velocity.Length())
			MakeIdealYaw( pev->origin + m_velocity );
		ChangeYaw( pev->yaw_speed );
		if( m_iDiving )
		{
			// steep enough, or fast enough, and the wings fold
			UTIL_MakeVectors( pev->angles );
			m_iDiving = ( gpGlobals->v_forward.z > 0.7f || m_velocity.Length() > STUKABAT_DIVE_MAX - 100 ) ? 1 : 2;
		}
		if( m_fSequenceFinished )
			pev->framerate = 1.0f;

		CBaseMonster::RunTask( pTask );

		if( m_Activity != m_IdealActivity )
		{
			SetActivity( m_IdealActivity );
			if( m_Activity == ACT_SMALL_FLINCH )
				m_iDiving = 0;
		}
		pev->framerate = 1.0f;

		if( m_iDiving )
		{
			m_flGroundSpeed = Q_min( (float)STUKABAT_DIVE_MAX, Q_max( (float)STUKABAT_DIVE_FLY, m_flGroundSpeed + 100 ));
			if( m_iDiving == 1 )
				m_movementActivity = ACT_RANGE_ATTACK2;
			else if( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ))
			{
				if( m_Activity == ACT_MELEE_ATTACK1 && m_fSequenceFinished )
					TaskFail();
				m_movementActivity = ACT_MELEE_ATTACK1;
			}
			else
				m_movementActivity = ACT_FLY;
		}
		else
		{
			m_movementActivity = m_velocity.Length() > 50 ? ACT_FLY : ACT_HOVER;
			m_flGroundSpeed = Q_min( m_flGroundSpeed + 100, (float)STUKABAT_CIRCLE );
		}
		if( m_Activity == ACT_FLY )
			pev->framerate = m_iDiving ? 2.0f : 1.5f;
		break;
	}
	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}

Schedule_t *CStukabat::GetSchedule( void )
{
	if( HasConditions( bits_COND_HEAVY_DAMAGE ))
		return GetScheduleOfType( SCHED_SMALL_FLINCH );
	return CBaseMonster::GetSchedule();
}

Schedule_t *CStukabat::GetScheduleOfType( int Type )
{
	switch( Type )
	{
	case SCHED_CHASE_ENEMY:
	case SCHED_MELEE_ATTACK1:
		return slStukabatRetreat;	// up and away, for the next dive
	case SCHED_COMBAT_FACE:
	case SCHED_RANGE_ATTACK1:
		return slStukabatDive;
	case SCHED_SMALL_FLINCH:
		return slStukabatFlinch;
	case SCHED_FAIL:
		return slStukabatFail;
	}
	return CBaseMonster::GetScheduleOfType( Type );
}

void CStukabat::RunAI( void )
{
	CBaseMonster::RunAI();
	// the body pitched along its flight, level when hovering
	if( IsAlive())
		pev->angles.x = ( m_Activity != ACT_HOVER && m_Activity != ACT_IDLE ) ? UTIL_VecToAngles( m_velocity.Normalize()).x : 0;
}

// the controller's way of flying (controller.cpp), with the dive: aimed where the enemy will be, and not stopped by
// the creature it's diving at
void CStukabat::Move( float flInterval )
{
	float flWaypointDist, flCheckDist, flDist, flMoveDist;
	Vector vecDir, vecApex;
	CBaseEntity *pTargetEnt;

	if( FRouteClear())
	{
		TaskFail();
		return;
	}
	if( m_flMoveWaitFinished > gpGlobals->time )
		return;

	pTargetEnt = NULL;
	if( m_flGroundSpeed == 0 )
		m_flGroundSpeed = 200;
	flMoveDist = m_flGroundSpeed * flInterval;

	do
	{
		if(( m_Route[m_iRouteIndex].iType & ~bits_MF_NOT_TO_MASK ) == bits_MF_TO_ENEMY )
		{
			pTargetEnt = m_hEnemy;
			if( m_hEnemy != 0 )
				m_Route[m_iRouteIndex].vecLocation = m_hEnemy->Center() + m_hEnemy->pev->velocity * 0.25f;	// where they'll be
		}
		else if(( m_Route[m_iRouteIndex].iType & ~bits_MF_NOT_TO_MASK ) == bits_MF_TO_TARGETENT )
			pTargetEnt = m_hTargetEnt;

		vecDir = ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Normalize();
		flWaypointDist = ( m_Route[m_iRouteIndex].vecLocation - pev->origin ).Length();
		flCheckDist = flWaypointDist < STUKABAT_CHECK_DIST ? flWaypointDist : STUKABAT_CHECK_DIST;

		flDist = 0;
		if( CheckLocalMove( pev->origin, pev->origin + vecDir * flCheckDist, pTargetEnt, &flDist ) != LOCALMOVE_VALID )
		{
			CBaseEntity *pBlocker = CBaseEntity::Instance( gpGlobals->trace_ent );
			bool expected = m_iDiving && pBlocker && ( pBlocker->pev->flags & ( FL_MONSTER | FL_CLIENT ));
			if( !expected )
			{
				if( pBlocker )
					DispatchBlocked( edict(), pBlocker->edict());
				Stop();
				if( !m_iDiving && pBlocker && m_moveWaitTime > 0 && pBlocker->IsMoving() && !pBlocker->IsPlayer() && ( gpGlobals->time - m_flMoveWaitFinished ) > 3.0f )
				{
					if( flDist < m_flGroundSpeed )
					{
						m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime;
						return;
					}
				}
				else if( FTriangulate( pev->origin, m_Route[m_iRouteIndex].vecLocation, flDist, pTargetEnt, &vecApex ))
				{
					InsertWaypoint( vecApex, bits_MF_TO_DETOUR );
					RouteSimplify( pTargetEnt );
				}
				else if( !m_iDiving )
				{
					Stop();
					if( m_moveWaitTime > 0 )
					{
						FRefreshRoute();
						m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime * 0.5f;
					}
					else
						TaskFail();
					return;
				}
			}
		}

		if( flCheckDist < flMoveDist )
		{
			MoveExecute( pTargetEnt, vecDir, flCheckDist / m_flGroundSpeed );
			AdvanceRoute( flWaypointDist );
			flMoveDist -= flCheckDist;
		}
		else
		{
			MoveExecute( pTargetEnt, vecDir, flMoveDist / m_flGroundSpeed );
			if( ShouldAdvanceRoute( flWaypointDist - flMoveDist ))
				AdvanceRoute( flWaypointDist );
			flMoveDist = 0;
		}

		if( MovementIsComplete())
		{
			Stop();
			RouteClear();
		}
	} while( flMoveDist > 0 && flCheckDist > 0 );

	if( m_movementGoal == MOVEGOAL_LOCATION )
	{
		if( flWaypointDist < 32 )
			RouteClear();	// (it's where it climbed to)
	}
	else if( flWaypointDist < 128 )
	{
		RouteSimplify( m_movementGoal == MOVEGOAL_ENEMY ? (CBaseEntity *)m_hEnemy : (CBaseEntity *)m_hTargetEnt );
		FRefreshRoute();
		if( m_flGroundSpeed > 200 )
			m_flGroundSpeed -= 40;
	}
	else if( m_flGroundSpeed < 400 )
		m_flGroundSpeed += 10;
}

int CStukabat::CheckLocalMove( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist )
{
	TraceResult tr;
	UTIL_TraceHull( vecStart + Vector( 0, 0, 32 ), vecEnd + Vector( 0, 0, 32 ), dont_ignore_monsters, large_hull, edict(), &tr );
	if( pflDist )
		*pflDist = (( tr.vecEndPos - Vector( 0, 0, 32 )) - vecStart ).Length();
	if( tr.fStartSolid || tr.flFraction < 1.0f )
	{
		if( pTarget && pTarget->edict() == gpGlobals->trace_ent )
			return LOCALMOVE_VALID;
		return LOCALMOVE_INVALID;
	}
	return LOCALMOVE_VALID;
}

void CStukabat::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval )
{
	if( m_IdealActivity != m_movementActivity )
		m_IdealActivity = m_movementActivity;

	// diving turns harder than gliding
	if( m_iDiving )
		m_velocity = m_velocity * 0.4f + vecDir * m_flGroundSpeed * 0.5f;
	else
		m_velocity = m_velocity * 0.8f + vecDir * m_flGroundSpeed * 0.2f;

	Vector expected = pev->origin + m_velocity * flInterval;
	UTIL_MoveToOrigin( ENT( pev ), pev->origin + m_velocity, m_velocity.Length() * flInterval, MOVE_STRAFE );

	// a dive that ran into something: as far as it gets, and it touches what it hit (moving doesn't)
	if( m_iDiving && ( pev->origin - expected ).Length() > 1 )
	{
		TraceResult tr;
		TRACE_MONSTER_HULL( edict(), pev->origin, expected, dont_ignore_monsters, edict(), &tr );
		UTIL_MoveToOrigin( ENT( pev ), tr.vecEndPos, ( tr.vecEndPos - pev->origin ).Length() * 0.99f, MOVE_STRAFE );
		if( tr.pHit )
			DispatchTouch( edict(), tr.pHit );
	}
}
