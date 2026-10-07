/*
sc_tor.cpp - Svencraft: Sven Co-op's Tor (monster_alien_tor), an alien grunt's commander, after SevenKewp's CTor

A big Xen soldier with a staff (Sven's tor.mdl): 800 health (Sven's sk_tor_health), armored where the model's hit group
10 is (bullets and blows lose 20 there, and ricochet). Far off he fires his staff in bursts of three green beams
(sk_tor_energybeam each, throwing the hit up a little), five bursts and then a rest; while resting he opens a portal
and calls in an alien grunt (three at most, each counted off when it dies). Close by he swings and stabs with the staff
(sk_tor_punch); with two or more enemies around him he slams the ground: a shock ring that throws everyone near up
into the air (sk_tor_sonicblast, less further out).
SevenKewp's needs its own monster base (knockback, display names, barnacle offsets): this stands on Half-Life's.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "weapons.h"
#include "effects.h"
#include "customentity.h"
#include "soundent.h"
#include "animation.h"
#include "studio.h"
#include "skill.h"

#define TOR_AE_SLAM		1
#define TOR_AE_STAFF_SWING	2
#define TOR_AE_SHOOT		3
#define TOR_AE_SUMMON		4
#define TOR_AE_STAFF_STAB	7
#define TOR_AE_STEP_RIGHT	10
#define TOR_AE_STEP_LEFT	11

#define TOR_MELEE_DIST		96
#define TOR_MELEE_CHASE		300	// nearer than this he'd rather close in than shoot
#define TOR_SLAM_CHECK		150	// enemies this near count toward a slam
#define TOR_SLAM_RADIUS		300
#define TOR_SLAM_ENEMIES	2
#define TOR_BURSTS		5	// beam bursts before a rest
#define TOR_SHOOT_RANGE		4096
#define TOR_SUMMON_DIST		256
#define TOR_SUMMON_HEIGHT	80
#define TOR_MAX_CHILDREN	3

#define TOR_SHOCKWAVE		"sprites/shockwave.spr"
#define TOR_SHOOT_SOUND		"tor/tor-staff-discharge.wav"
#define TOR_BEAM		"sprites/xenobeam.spr"
#define TOR_PORTAL		"sprites/exit1.spr"
#define TOR_PORTAL_SOUND	"debris/beamstart8.wav"
#define TOR_PORTAL_SOUND2	"debris/beamstart7.wav"
#define TOR_SUMMON_SOUND	"tor/tor-summon.wav"
#define TOR_SUMMONS		"monster_alien_grunt"

// Sven's values when the skill cvars aren't there
static float SC_TorValue( const char *name, float sven )
{
	float v = GetSkillCvar( name );
	return v > 0.0f ? v : sven;
}

// what a blow can throw: something that walks or flies about (not a turret, a corpse or a wall)
static bool SC_CanKnockback( CBaseEntity *e )
{
	if( !e || !e->IsAlive() || !( e->pev->flags & ( FL_MONSTER | FL_CLIENT )))
		return false;
	int mt = e->pev->movetype;
	return mt == MOVETYPE_WALK || mt == MOVETYPE_STEP || mt == MOVETYPE_TOSS || ( mt == MOVETYPE_FLY && e->pev->mins.z >= 0.0f );
}

class CTor : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void ) { pev->yaw_speed = 180; }
	int Classify( void ) { return CLASS_ALIEN_MILITARY; }
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void SetActivity( Activity NewActivity );
	void MonsterThink( void );
	BOOL CheckRangeAttack1( float flDot, float flDist );
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckMeleeAttack1( float flDot, float flDist );
	BOOL CheckMeleeAttack2( float flDot, float flDist );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void DeathNotice( entvars_t *pevChild );
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -24, -24, 0 );
		pev->absmax = pev->origin + Vector( 24, 24, 88 );
	}
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void DeathSound( void );

	CUSTOM_SCHEDULES;

private:
	void SlamAttack( void );
	bool GetSummonPos( Vector &pos );
	void StartSummon( void );
	void SpawnGrunt( void );

	int	m_iShots;		// beam bursts since the last rest
	float	m_flNextShoot;		// the rest is over
	float	m_flNextBurst;		// a burst is under way (its first beam's time)
	float	m_flNextBeam;
	int	m_iBurstBeams;
	float	m_flNextSummon;
	float	m_flSummonSpawn;	// the grunt comes through the portal
	bool	m_bSummoning;
	int	m_iChildren;
	Vector	m_vecSummon;
	int	m_iFailedMelees;	// don't keep swinging at someone who steps back each time
	int	m_iShockwave;

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];
	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDieSounds[];
	static const char *pRunSounds[];
	static const char *pSlamSounds[];
};

LINK_ENTITY_TO_CLASS( monster_alien_tor, CTor )

const char *CTor::pAttackHitSounds[] = { "zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav" };
const char *CTor::pAttackMissSounds[] = { "zombie/claw_miss1.wav", "zombie/claw_miss2.wav" };
const char *CTor::pAttackSounds[] = { "tor/tor-attack1.wav", "tor/tor-attack2.wav" };
const char *CTor::pIdleSounds[] = { "tor/tor-idle.wav", "tor/tor-idle2.wav", "tor/tor-idle3.wav" };
const char *CTor::pAlertSounds[] = { "tor/tor-alerted.wav" };
const char *CTor::pPainSounds[] = { "tor/tor-pain.wav", "tor/tor-pain2.wav" };
const char *CTor::pDieSounds[] = { "tor/tor-die.wav", "tor/tor-die2.wav" };
const char *CTor::pRunSounds[] = { "tor/tor-foot.wav" };
const char *CTor::pSlamSounds[] = { "houndeye/he_blast1.wav", "houndeye/he_blast2.wav", "houndeye/he_blast3.wav" };

// the slam: not broken off by a flinch
Task_t tlTorSlam[] =
{
	{ TASK_STOP_MOVING,	0 },
	{ TASK_MELEE_ATTACK1,	0 },
};

Schedule_t slTorSlam[] =
{
	{ tlTorSlam, ARRAYSIZE( tlTorSlam ), 0, 0, "TOR_SLAM_ATTACK" },
};

DEFINE_CUSTOM_SCHEDULES( CTor )
{
	slTorSlam,
};

IMPLEMENT_CUSTOM_SCHEDULES( CTor, CBaseMonster )

void CTor::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/tor.mdl" );
	UTIL_SetSize( pev, Vector( -24, -24, 0 ), Vector( 24, 24, 72 ));

	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->health		= SC_TorValue( "sk_tor_health", 800 );
	pev->view_ofs		= Vector( 0, 0, 64 );
	m_flFieldOfView		= 0.0;
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_RANGE_ATTACK1 | bits_CAP_RANGE_ATTACK2 | bits_CAP_MELEE_ATTACK1 | bits_CAP_MELEE_ATTACK2;

	m_iShots = m_iBurstBeams = m_iChildren = m_iFailedMelees = 0;
	m_flNextShoot = m_flNextBurst = m_flNextBeam = m_flNextSummon = m_flSummonSpawn = 0;
	m_bSummoning = false;

	MonsterInit();
}

void CTor::Precache( void )
{
	PRECACHE_MODEL( "models/tor.mdl" );
	PRECACHE_MODEL( TOR_BEAM );
	PRECACHE_MODEL( TOR_PORTAL );
	m_iShockwave = PRECACHE_MODEL( TOR_SHOCKWAVE );

	PRECACHE_SOUND_ARRAY( pAttackHitSounds );
	PRECACHE_SOUND_ARRAY( pAttackMissSounds );
	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDieSounds );
	PRECACHE_SOUND_ARRAY( pRunSounds );
	PRECACHE_SOUND_ARRAY( pSlamSounds );
	PRECACHE_SOUND( TOR_SHOOT_SOUND );
	PRECACHE_SOUND( TOR_SUMMON_SOUND );
	PRECACHE_SOUND( TOR_PORTAL_SOUND );
	PRECACHE_SOUND( TOR_PORTAL_SOUND2 );

	UTIL_PrecacheOther( TOR_SUMMONS );
}

void CTor::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case TOR_AE_SLAM:
		SlamAttack();
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3f );
		break;
	case TOR_AE_SHOOT:
		// the burst is fired from MonsterThink; the sequence waits for it (and fires no more of these)
		m_flNextBeam = m_flNextBurst = gpGlobals->time;
		pev->framerate = 0.001f;
		m_fSequenceFinished = FALSE;
		if( ++m_iShots >= TOR_BURSTS )
		{
			m_iShots = 0;
			m_flNextShoot = gpGlobals->time + 3.0f;
		}
		m_iFailedMelees = 0;
		break;
	case TOR_AE_SUMMON:
		break;
	case TOR_AE_STAFF_SWING:
	case TOR_AE_STAFF_STAB:
	{
		CBaseEntity *pHurt = CheckTraceHullAttack( TOR_MELEE_DIST, (int)SC_TorValue( "sk_tor_punch", 55 ), DMG_SLASH );
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3f );
		if( pHurt )
		{
			m_iFailedMelees = 0;
			if( SC_CanKnockback( pHurt ))
			{
				if( pEvent->event == TOR_AE_STAFF_SWING )
				{
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->velocity = pHurt->pev->velocity + ( gpGlobals->v_right + gpGlobals->v_forward + gpGlobals->v_up ) * 200;
				}
				else
				{
					pHurt->pev->punchangle.x = 18;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 100;
				}
			}
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));
		}
		else
		{
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));
			m_iFailedMelees++;
		}
		break;
	}
	case TOR_AE_STEP_LEFT:
	case TOR_AE_STEP_RIGHT:
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, RANDOM_SOUND_ARRAY( pRunSounds ), 1.0, ATTN_NORM, 0, pEvent->event == TOR_AE_STEP_RIGHT ? 100 : 120 );
		break;
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

Schedule_t *CTor::GetSchedule( void )
{
	// a heavy hit makes him flinch; light ones he doesn't notice
	if( HasConditions( bits_COND_HEAVY_DAMAGE ))
	{
		ClearConditions( bits_COND_HEAVY_DAMAGE );
		return GetScheduleOfType( SCHED_SMALL_FLINCH );
	}
	ClearConditions( bits_COND_LIGHT_DAMAGE );
	return CBaseMonster::GetSchedule();
}

Schedule_t *CTor::GetScheduleOfType( int Type )
{
	switch( Type )
	{
	case SCHED_MELEE_ATTACK1:
		return &slTorSlam[0];
	case SCHED_MELEE_ATTACK2:
		AttackSound();
		break;
	}
	return CBaseMonster::GetScheduleOfType( Type );
}

// shooting is the short "attack_idle" (the second of the model's three), as in SevenKewp: its one event fires a burst
void CTor::SetActivity( Activity NewActivity )
{
	CBaseMonster::SetActivity( NewActivity );
	if( NewActivity != ACT_RANGE_ATTACK1 )
		return;
	studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR( ENT( pev ));
	if( !hdr )
		return;
	mstudioseqdesc_t *seq = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex );
	for( int i = 0, n = 0; i < hdr->numseq; i++ )
	{
		if( seq[i].activity != ACT_RANGE_ATTACK1 || n++ != 1 )
			continue;
		pev->sequence = i;
		pev->frame = 0;
		ResetSequenceInfo();
		break;
	}
}

void CTor::MonsterThink( void )
{
	if( m_flNextBurst && m_flNextBurst < gpGlobals->time )
	{
		pev->framerate = 0.001f;
		if( m_flNextBeam < gpGlobals->time )
		{
			m_flNextBeam = gpGlobals->time + 0.05f;
			m_iBurstBeams++;
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, TOR_SHOOT_SOUND, 1.0, ATTN_NORM, 0, 100 );
			CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3f );

			Vector vecSrc, angles;
			GetAttachment( 0, vecSrc, angles );
			CBaseEntity *pTarget = m_hEnemy;
			if( pTarget )
			{
				Vector vecDir = ( pTarget->BodyTarget( pev->origin ) - vecSrc ).Normalize();
				TraceResult tr;
				UTIL_TraceLine( vecSrc, vecSrc + vecDir * TOR_SHOOT_RANGE, dont_ignore_monsters, edict(), &tr );

				// two beams twisting round each other, the alien green
				for( int b = 0; b < 2; b++ )
				{
					CBeam *pBeam = CBeam::BeamCreate( TOR_BEAM, 50 );
					if( !pBeam )
						continue;
					pBeam->PointsInit( vecSrc, tr.vecEndPos );
					pBeam->SetFlags( b ? BEAM_FSINE : 0 );
					pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
					pBeam->SetColor( 96, b ? 255 : 128, 16 );
					pBeam->SetBrightness( 150 );
					pBeam->SetNoise( b ? 15 : 10 );
					pBeam->SetScrollRate( 150 );
					pBeam->LiveForTime( 0.5f );
				}

				CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
				if( pHit && pHit->pev->takedamage )
				{
					pHit->TakeDamage( pev, pev, SC_TorValue( "sk_tor_energybeam", 3 ), DMG_ENERGYBEAM );
					if( SC_CanKnockback( pHit ))
						pHit->pev->velocity.z += 200;
				}
			}

			if( m_iBurstBeams >= 3 )
			{
				// the burst is over: the shooting sequence finishes
				m_flNextBeam = m_flNextBurst = 0;
				m_iBurstBeams = 0;
				pev->framerate = 1.0f;
				m_fSequenceFinished = TRUE;
			}
		}
	}

	if( m_Activity == ACT_RANGE_ATTACK2 )
	{
		if( m_flSummonSpawn && m_flSummonSpawn < gpGlobals->time )
			SpawnGrunt();
		if( !m_bSummoning && m_flNextSummon < gpGlobals->time )
			StartSummon();
		if( m_fSequenceFinished )
			m_bSummoning = false;
	}
	else
		m_flSummonSpawn = 0;

	CBaseMonster::MonsterThink();
}

BOOL CTor::CheckRangeAttack1( float flDot, float flDist )
{
	bool closeIn = flDist < TOR_MELEE_CHASE && m_iFailedMelees < 2;
	return !m_bSummoning && !closeIn && flDist < TOR_SHOOT_RANGE && gpGlobals->time > m_flNextShoot;
}

BOOL CTor::CheckRangeAttack2( float flDot, float flDist )
{
	if( m_bSummoning )
		return TRUE;
	Vector spot;
	return m_iChildren < TOR_MAX_CHILDREN && m_flNextSummon < gpGlobals->time && GetSummonPos( spot );
}

BOOL CTor::CheckMeleeAttack1( float flDot, float flDist )
{
	int nearby = 0;
	CBaseEntity *pEntity = NULL;
	while(( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, TOR_SLAM_CHECK )) != NULL )
	{
		if(( pEntity->pev->flags & ( FL_MONSTER | FL_CLIENT )) && pEntity->IsAlive() && pEntity != this && IRelationship( pEntity ) >= R_DL )
		{
			if( ++nearby >= TOR_SLAM_ENEMIES )
				return TRUE;
		}
	}
	return FALSE;
}

BOOL CTor::CheckMeleeAttack2( float flDot, float flDist )
{
	return flDist <= TOR_MELEE_DIST;
}

void CTor::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( ptr->iHitgroup == 10 && ( bitsDamageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB )))
	{
		// his armor
		if( pev->dmgtime != gpGlobals->time || RANDOM_LONG( 0, 10 ) < 1 )
		{
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT( 1, 2 ));
			pev->dmgtime = gpGlobals->time;
		}
		if(( bitsDamageType & DMG_BULLET ) && RANDOM_LONG( 0, 1 ) == 0 )
		{
			Vector vecTracerDir = vecDir;
			vecTracerDir.x += RANDOM_FLOAT( -0.3f, 0.3f );
			vecTracerDir.y += RANDOM_FLOAT( -0.3f, 0.3f );
			vecTracerDir.z += RANDOM_FLOAT( -0.3f, 0.3f );
			Vector vecEnd = ptr->vecEndPos + vecTracerDir * -512;
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos );
				WRITE_BYTE( TE_TRACER );	// the round glancing off
				WRITE_COORD( ptr->vecEndPos.x );
				WRITE_COORD( ptr->vecEndPos.y );
				WRITE_COORD( ptr->vecEndPos.z );
				WRITE_COORD( vecEnd.x );
				WRITE_COORD( vecEnd.y );
				WRITE_COORD( vecEnd.z );
			MESSAGE_END();
		}
		flDamage -= 20;
		if( flDamage <= 0 )
			flDamage = 0.1f;	// barely, but he notices
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
		return;
	}
	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

void CTor::DeathNotice( entvars_t *pevChild )
{
	if( m_iChildren > 0 )
		m_iChildren--;
}

void CTor::PainSound( void )
{
	if( RANDOM_LONG( 0, 5 ) < 2 )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( 0, 9 ));
}

void CTor::AlertSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAlertSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( 0, 9 ));
}

void CTor::IdleSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pIdleSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));
}

void CTor::AttackSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAttackSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));
}

void CTor::DeathSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pDieSounds ), 1.0, ATTN_NORM, 0, 100 );
}

// the ground slam: everyone near is thrown up (the nearer, the higher) and hurt
void CTor::SlamAttack( void )
{
	CBaseEntity *pEntity = NULL;
	while(( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, TOR_SLAM_RADIUS )) != NULL )
	{
		if( pEntity == this || !( pEntity->pev->flags & ( FL_MONSTER | FL_CLIENT )) || !pEntity->IsAlive() || IRelationship( pEntity ) < R_DL )
			continue;
		Vector delta = pEntity->pev->origin - pev->origin;
		Vector push = delta.Normalize();
		push.z = 0;
		float launch = Q_max( 0.7f, 1.0f - (( delta.Length() - 64 ) * 0.5f / TOR_SLAM_RADIUS ));
		if( SC_CanKnockback( pEntity ))
			pEntity->pev->velocity = pEntity->pev->velocity + Vector( 0, 0, 1000 * launch ) + push * 1000 * ( 1.0f - launch );
		pEntity->TakeDamage( pev, pev, SC_TorValue( "sk_tor_sonicblast", 15 ) * launch, DMG_SONIC );
		if( pEntity->IsPlayer())
			pEntity->pev->punchangle.x = 10;
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pSlamSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));

	// the shock ring
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 16 );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 16 + ( TOR_SLAM_RADIUS + 50 ) / 0.3f );
		WRITE_SHORT( m_iShockwave );
		WRITE_BYTE( 0 );	// start frame
		WRITE_BYTE( 0 );	// frame rate
		WRITE_BYTE( 2 );	// life
		WRITE_BYTE( 12 );	// width
		WRITE_BYTE( 0 );	// noise
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 0 );	// speed
	MESSAGE_END();
	UTIL_ScreenShake( pev->origin, 6.0f, 100.0f, 1.0f, TOR_SLAM_RADIUS * 2 );
}

// a spot up and out from him with room for a grunt
bool CTor::GetSummonPos( Vector &pos )
{
	Vector start = pev->origin + Vector( 0, 0, ( pev->maxs.z - pev->mins.z ) * 0.5f );
	for( int i = 0; i < 8; i++ )
	{
		float c = RANDOM_FLOAT( 0, 2 * M_PI );
		Vector check = pev->origin + Vector( cos( c ) * TOR_SUMMON_DIST, sin( c ) * TOR_SUMMON_DIST, start.z - pev->origin.z + TOR_SUMMON_HEIGHT );
		TraceResult tr;
		UTIL_TraceHull( start, check, dont_ignore_monsters, large_hull, ENT( pev ), &tr );
		if( tr.fStartSolid || tr.flFraction < 0.5f )
			continue;
		pos = tr.vecEndPos - ( check - pev->origin ).Normalize() * 40;	// a little in from any wall
		return true;
	}
	return false;
}

void CTor::StartSummon( void )
{
	if( !GetSummonPos( m_vecSummon ))
		return;
	m_bSummoning = true;
	m_flNextSummon = gpGlobals->time + RANDOM_FLOAT( 5.0f, 10.0f );
	m_flSummonSpawn = gpGlobals->time + 2.0f;
	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, TOR_SUMMON_SOUND, 1.0, ATTN_NORM, 0, 100 );

	Vector start = pev->origin + Vector( 0, 0, ( pev->maxs.z - pev->mins.z ) * 0.5f );
	for( int i = 0; i < 3; i++ )
	{
		CBeam *pBeam = CBeam::BeamCreate( TOR_BEAM, 30 );
		if( !pBeam )
			continue;
		pBeam->PointsInit( start, m_vecSummon );
		pBeam->SetFlags( BEAM_FSHADEOUT );
		pBeam->SetColor( 96, 255, 32 );
		pBeam->SetBrightness( 80 );
		pBeam->SetNoise( 80 );
		pBeam->LiveForTime( 2.0f );
	}
	CSprite *pPortal = CSprite::SpriteCreate( TOR_PORTAL, m_vecSummon, TRUE );
	if( pPortal )
	{
		pPortal->SetScale( 2.0f );
		pPortal->SetTransparency( kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone );
		pPortal->AnimateAndDie( 10 );
		EMIT_SOUND_DYN( ENT( pPortal->pev ), CHAN_ITEM, TOR_PORTAL_SOUND, 1.0, ATTN_NORM, 0, 100 );
	}
	m_vecSummon.z -= 40;
}

void CTor::SpawnGrunt( void )
{
	m_flSummonSpawn = 0;
	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( TOR_SUMMONS ));
	if( FNullEnt( pent ))
		return;
	entvars_t *pevNew = VARS( pent );
	pevNew->origin = m_vecSummon;
	pevNew->angles = pev->angles;
	SetBits( pevNew->spawnflags, SF_MONSTER_FALL_TO_GROUND );
	DispatchSpawn( pent );
	pevNew->owner = edict();	// (its death is counted off: DeathNotice)
	CBaseEntity *pNew = CBaseEntity::Instance( pent );
	EMIT_SOUND_DYN( pent, CHAN_ITEM, TOR_PORTAL_SOUND2, 1.0, ATTN_NORM, 0, 100 );

	// whatever creature stood where it came through is gone (not a player: they're pushed clear)
	CBaseEntity *pList[64];
	int count = UTIL_EntitiesInBox( pList, 64, pevNew->absmin, pevNew->absmax, FL_MONSTER | FL_CLIENT );
	for( int i = 0; i < count; i++ )
	{
		if( pList[i] == pNew || pList[i] == this || pList[i]->pev->takedamage == DAMAGE_NO )
			continue;
		if( pList[i]->IsPlayer())
			pList[i]->pev->velocity = pList[i]->pev->velocity + ( pList[i]->pev->origin - pevNew->origin ).Normalize() * 300 + Vector( 0, 0, 200 );
		else
			pList[i]->Killed( pev, GIB_ALWAYS );
	}
	m_iChildren++;
}
