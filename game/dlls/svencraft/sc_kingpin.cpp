/*
sc_kingpin.cpp - Svencraft: Sven Co-op's Kingpin (monster_kingpin), after SevenKewp's CKingpin

A Xen brain on legs (Sven's kingpin.mdl): 450 health (Sven's sk_kingpin_health), its head no weaker than its body.
Its four eyes (front, back, left, right) each charge up and fire a magenta lightning bolt at a hostile in their sight
every four seconds (sk_kingpin_lightning). From a distance it conjures a plasma ball that grows over its head and
then hunts the nearest enemy for eight seconds, bursting on contact (sk_kingpin_plasma_blast, 300 around). It swats
at whoever comes close (sk_kingpin_melee), pushes thrown grenades aside, and when crowded or out of sight it fades out
and reappears up to a thousand units away: a creature where it lands is torn apart (sk_kingpin_telefrag), a player
is thrown clear (sk_kingpin_tele_blast). Dying, its eyes flare and it bursts.
SevenKewp's needs its own monster base and effect helpers: this stands on Half-Life's, the effects written out here.
*/

#include <float.h>
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "weapons.h"
#include "effects.h"
#include "customentity.h"
#include "soundent.h"
#include "studio.h"
#include "skill.h"

#define KINGPIN_AE_SWING_RIGHT	1
#define KINGPIN_AE_SWING_LEFT	2
#define KINGPIN_AE_CONJURE_ORB	3
#define KINGPIN_AE_SHOOT_ORB	4

#define KINGPIN_MELEE_DIST	96
#define KINGPIN_PITCH		160	// the alien grunt's voice, pitched up
#define KINGPIN_EYE_RECHARGE	4.0f

#define KINGPIN_BEAM_SOUND	"debris/beamstart10.wav"
#define KINGPIN_BEAM		"sprites/zbeam2.spr"
#define KINGPIN_FLARE		"sprites/flare1.spr"
#define KINGPIN_EYE		"sprites/boss_glow.spr"
#define KINGPIN_TELE		"sprites/b-tele1.spr"
#define KINGPIN_TELE_SOUND_IN	"ambience/port_suckout1.wav"	// (in.wav would make more sense for out)
#define KINGPIN_TELE_SOUND_OUT	"ambience/port_suckin1.wav"

#define ORB_RADIUS		300
#define ORB_SPRITE		"sprites/nhth1.spr"
#define ORB_TRAIL		"sprites/laserbeam.spr"
#define ORB_GROW_SOUND		"debris/beamstart1.wav"
#define ORB_MOVE_SOUND		"x/x_teleattack1.wav"
#define ORB_EXPLODE_SOUND	"tor/tor-staff-discharge.wav"
#define ORB_SHOCKWAVE		"sprites/shockwave.spr"

static float SC_KingpinValue( const char *name, float sven )
{
	float v = GetSkillCvar( name );
	return v > 0.0f ? v : sven;
}

// a creature or player that is a target (alive, not hidden by notarget)
static bool SC_KingpinTarget( CBaseMonster *self, CBaseEntity *e )
{
	return e != self && ( e->pev->flags & ( FL_MONSTER | FL_CLIENT )) && e->IsAlive() && !( e->pev->flags & FL_NOTARGET )
		&& self->IRelationship( e ) > R_NO;
}

//
// the effects SevenKewp has helpers for
//
static void SC_TE_Cylinder( int type, const Vector &o, float radius, int sprite, int r, int g, int b, int a )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, o );
		WRITE_BYTE( type );	// TE_BEAMCYLINDER or TE_BEAMDISK
		WRITE_COORD( o.x ); WRITE_COORD( o.y ); WRITE_COORD( o.z );
		WRITE_COORD( o.x ); WRITE_COORD( o.y ); WRITE_COORD( o.z + radius );
		WRITE_SHORT( sprite );
		WRITE_BYTE( 0 ); WRITE_BYTE( 0 );	// frame, rate
		WRITE_BYTE( 2 ); WRITE_BYTE( 12 ); WRITE_BYTE( 0 );	// life, width, noise
		WRITE_BYTE( r ); WRITE_BYTE( g ); WRITE_BYTE( b ); WRITE_BYTE( a );
		WRITE_BYTE( 0 );	// speed
	MESSAGE_END();
}

static void SC_TE_ELight( CBaseEntity *e, const Vector &o, float radius, int r, int g, int b, int life, float decay )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, o );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( e->entindex());
		WRITE_COORD( o.x ); WRITE_COORD( o.y ); WRITE_COORD( o.z );
		WRITE_COORD( radius );
		WRITE_BYTE( r ); WRITE_BYTE( g ); WRITE_BYTE( b );
		WRITE_BYTE( life );
		WRITE_COORD( decay );
	MESSAGE_END();
}

// a beam from one of an entity's attachments (1-based) to a point
static void SC_TE_BeamEntPoint( CBaseEntity *e, int attachment, const Vector &end, int sprite, int life, int width, int noise, int r, int g, int b, int a )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, e->pev->origin );
		WRITE_BYTE( TE_BEAMENTPOINT );
		WRITE_SHORT( e->entindex() | ( attachment << 12 ));
		WRITE_COORD( end.x ); WRITE_COORD( end.y ); WRITE_COORD( end.z );
		WRITE_SHORT( sprite );
		WRITE_BYTE( 0 ); WRITE_BYTE( 0 );
		WRITE_BYTE( life ); WRITE_BYTE( width ); WRITE_BYTE( noise );
		WRITE_BYTE( r ); WRITE_BYTE( g ); WRITE_BYTE( b ); WRITE_BYTE( a );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

static void SC_TE_BeamPoints( const Vector &a, const Vector &b, int sprite, int life, int width, int noise, int r, int g, int bl, int br )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, a );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( a.x ); WRITE_COORD( a.y ); WRITE_COORD( a.z );
		WRITE_COORD( b.x ); WRITE_COORD( b.y ); WRITE_COORD( b.z );
		WRITE_SHORT( sprite );
		WRITE_BYTE( 0 ); WRITE_BYTE( 0 );
		WRITE_BYTE( life ); WRITE_BYTE( width ); WRITE_BYTE( noise );
		WRITE_BYTE( r ); WRITE_BYTE( g ); WRITE_BYTE( bl ); WRITE_BYTE( br );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

static void SC_TE_BeamEnts( CBaseEntity *a, CBaseEntity *b, int sprite, int life, int width, int noise )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, a->pev->origin );
		WRITE_BYTE( TE_BEAMENTS );
		WRITE_SHORT( a->entindex());
		WRITE_SHORT( b->entindex());
		WRITE_SHORT( sprite );
		WRITE_BYTE( 0 ); WRITE_BYTE( 0 );
		WRITE_BYTE( life ); WRITE_BYTE( width ); WRITE_BYTE( noise );
		WRITE_BYTE( 255 ); WRITE_BYTE( 255 ); WRITE_BYTE( 255 ); WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

//=========================================================
// the plasma ball (kingpin_plasma_ball): grows over the Kingpin's head, then hunts
//=========================================================
class CKingpinBall : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return m_hOwner != 0 ? m_hOwner->Classify() : CLASS_NONE; }
	void EXPORT HuntThink( void );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );
	void MoveToTarget( const Vector &vecTarget );
	void Activate( void );
	void UpdateOnRemove( void );

	EHANDLE	m_hOwner;
	Vector	m_lastDir;
	bool	m_isActive;
	float	m_lastThink;
	int	m_spriteFrames;
	float	m_power;
	int	m_iShockwave;
	int	m_iTrail;
};

LINK_ENTITY_TO_CLASS( kingpin_plasma_ball, CKingpinBall )

void CKingpinBall::Spawn( void )
{
	m_hOwner = Instance( pev->owner );
	Precache();
	m_flFieldOfView = VIEW_FIELD_FULL;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->gravity = FLT_MIN;	// (0 would be the normal gravity)
	pev->friction = 2.0f;
	SET_MODEL( ENT( pev ), ORB_SPRITE );
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor = Vector( 255, 255, 255 );
	pev->renderamt = 255;
	pev->scale = 1.3f;
	pev->flags |= FL_NOTARGET;
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	UTIL_SetOrigin( pev, pev->origin );
	SetThink( &CKingpinBall::HuntThink );
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->dmgtime = gpGlobals->time;
	pev->framerate = 10.0f;
	m_lastThink = gpGlobals->time;
	m_power = 1.0f;
	m_isActive = false;
	m_lastDir = g_vecZero;
}

void CKingpinBall::Precache( void )
{
	int spr = PRECACHE_MODEL( ORB_SPRITE );
	m_iTrail = PRECACHE_MODEL( ORB_TRAIL );
	m_iShockwave = PRECACHE_MODEL( ORB_SHOCKWAVE );
	PRECACHE_SOUND( ORB_MOVE_SOUND );
	PRECACHE_SOUND( ORB_EXPLODE_SOUND );
	m_spriteFrames = MODEL_FRAMES( spr );
}

void CKingpinBall::Activate( void )
{
	SetTouch( &CKingpinBall::ExplodeTouch );
	MESSAGE_BEGIN( MSG_ALL, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex());
		WRITE_SHORT( m_iTrail );
		WRITE_BYTE( 5 );	// life
		WRITE_BYTE( 10 );	// width
		WRITE_BYTE( 255 ); WRITE_BYTE( 255 ); WRITE_BYTE( 255 ); WRITE_BYTE( 255 );
	MESSAGE_END();
	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, ORB_MOVE_SOUND, 1.0f, ATTN_NORM, 0, 99 );
	m_isActive = true;
	pev->scale = 1.3f;
}

void CKingpinBall::HuntThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1f;
	SC_TE_ELight( this, pev->origin, 64 * m_power, 255, 255, 255, 2, 0 );

	float timeLeft = 8.0f - ( gpGlobals->time - pev->dmgtime );
	if( timeLeft < 0 )
	{
		SetTouch( NULL );
		SetThink( NULL );
		UTIL_Remove( this );
		return;
	}
	if( timeLeft < 2.0f )
	{
		// fading out, faster and faster
		float q = 1.0f - timeLeft / 2.0f;
		m_power = 1.0f - q * q;
		pev->renderamt = m_power * 255.0f;
		pev->scale = 0.5f + m_power * 0.8f;
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, ORB_MOVE_SOUND, m_power + 0.2f, ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, (int)( m_power * 20 + 80 ));
	}

	float interval = gpGlobals->time - m_lastThink;
	m_lastThink = gpGlobals->time;
	if( m_spriteFrames > 0 )
		pev->frame = fmod( pev->frame + pev->framerate * interval, (float)m_spriteFrames );

	if( !m_isActive )
	{
		// still growing over the Kingpin's head
		pev->scale = Q_min( 1.3f, pev->scale + 0.05f );
		pev->nextthink = gpGlobals->time + 0.05f;
		return;
	}

	if( m_hEnemy == 0 )
	{
		Look( 1024 );
		m_hEnemy = BestVisibleEnemy();
	}
	if( m_hEnemy != 0 )
	{
		m_lastDir = ( m_hEnemy->Center() - pev->origin ).Normalize();
		MoveToTarget( m_hEnemy->Center());
		if( !m_hEnemy->IsAlive())
			m_hEnemy = NULL;
	}
	else
		MoveToTarget( pev->origin + m_lastDir * 512 );
}

void CKingpinBall::MoveToTarget( const Vector &vecTarget )
{
	Vector ideal = ( vecTarget - pev->origin ).Normalize() * 300 * m_power;
	pev->velocity = pev->velocity * 0.2f + ideal * 0.8f;
}

void CKingpinBall::ExplodeTouch( CBaseEntity *pOther )
{
	float radius = ( ORB_RADIUS + 50 ) / 0.3f;
	SC_TE_Cylinder( TE_BEAMCYLINDER, pev->origin, radius, m_iShockwave, 170, 120, 160, 255 );
	SC_TE_Cylinder( TE_BEAMDISK, pev->origin, radius, m_iShockwave, 170, 120, 160, 255 );
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( pev->origin.x ); WRITE_COORD( pev->origin.y ); WRITE_COORD( pev->origin.z );
		WRITE_BYTE( 40 );	// radius
		WRITE_BYTE( 170 ); WRITE_BYTE( 120 ); WRITE_BYTE( 160 );
		WRITE_BYTE( 10 );	// life
		WRITE_BYTE( 6 );	// decay
	MESSAGE_END();

	entvars_t *pevOwner = pev->owner ? VARS( pev->owner ) : pev;
	::RadiusDamage( pev->origin, pev, pevOwner, SC_KingpinValue( "sk_kingpin_plasma_blast", 80 ), ORB_RADIUS, CLASS_NONE, DMG_ALWAYSGIB | DMG_ENERGYBEAM );
	UTIL_EmitAmbientSound( edict(), pev->origin, ORB_EXPLODE_SOUND, 1.0, ATTN_NORM, 0, RANDOM_LONG( 75, 80 ));
	SetTouch( NULL );
	UTIL_Remove( this );
}

void CKingpinBall::UpdateOnRemove( void )
{
	STOP_SOUND( edict(), CHAN_WEAPON, ORB_MOVE_SOUND );
	CBaseMonster::UpdateOnRemove();
}

//=========================================================
// the Kingpin
//=========================================================
struct kingpin_eye_t
{
	EHANDLE	hSprite;
	EHANDLE	hFlare;
	float	lastAttack;
	int	iAttachment;	// 0-based
	float	angle;		// where it looks, from the Kingpin's facing
};

enum
{
	TASK_KINGPIN_SET_TELEPORT_DEST = LAST_COMMON_TASK + 1,	// a place nearby to go to
	TASK_KINGPIN_TELEPORT,
};

class CKingpin : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void ) { pev->yaw_speed = 100; }
	int Classify( void ) { return CLASS_ALIEN_MONSTER; }
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void ScheduleChange( void );
	void UpdateOnRemove( void );
	void GibMonster( void );
	void EXPORT DieThink( void ) { GibMonster(); }
	void MonsterThink( void );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );
	BOOL CheckRangeAttack1( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2( float flDot, float flDist );
	BOOL CheckMeleeAttack1( float flDot, float flDist ) { return flDist <= KINGPIN_MELEE_DIST; }
	BOOL CheckMeleeAttack2( float flDot, float flDist ) { return FALSE; }
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void SetObjectCollisionBox( void )
	{
		pev->absmin = pev->origin + Vector( -56, -56, 0 );
		pev->absmax = pev->origin + Vector( 56, 56, 116 );
	}
	void PainSound( void );
	void DeathSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );

	CUSTOM_SCHEDULES;

private:
	void CancelOrb( void );
	void EndTeleport( void );
	void LaserEyesThink( void );
	void DeflectThink( void );

	float	m_flNextOrb;
	float	m_flNextTele;
	EHANDLE	m_hOrb;
	Vector	m_vecTeleSrc, m_vecTeleDst;
	float	m_flTeleTime;		// the teleport began (or last ended: the eyes wait a second after)
	int	m_iTelePhase;		// 0 none, 1 fading out, 2 the portals are open
	int	m_iOldRenderFx, m_iOldRenderMode;
	Vector	m_vecOldRenderColor;
	float	m_flOldRenderAmt;
	kingpin_eye_t m_eyes[4];
	int	m_iAttachments;
	int	m_iBeam, m_iSpit;

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];
	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDieSounds[];
};

LINK_ENTITY_TO_CLASS( monster_kingpin, CKingpin )

const char *CKingpin::pAttackHitSounds[] = { "zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav" };
const char *CKingpin::pAttackMissSounds[] = { "zombie/claw_miss1.wav", "zombie/claw_miss2.wav" };
const char *CKingpin::pAttackSounds[] = { "agrunt/ag_attack2.wav", "agrunt/ag_attack3.wav" };
const char *CKingpin::pDieSounds[] = { "agrunt/ag_die2.wav" };
const char *CKingpin::pPainSounds[] = { "agrunt/ag_pain1.wav", "agrunt/ag_pain2.wav", "agrunt/ag_pain3.wav" };
const char *CKingpin::pIdleSounds[] = { "agrunt/ag_die1.wav", "agrunt/ag_idle1.wav" };
const char *CKingpin::pAlertSounds[] = { "agrunt/ag_alert2.wav", "agrunt/ag_alert3.wav", "agrunt/ag_alert4.wav" };

Task_t tlKingpinOrb[] =
{
	{ TASK_STOP_MOVING,	0 },
	{ TASK_RANGE_ATTACK2,	0 },
};

Schedule_t slKingpinOrb[] =
{
	{ tlKingpinOrb, ARRAYSIZE( tlKingpinOrb ), 0, 0, "KINGPIN_ORB_ATTACK" },
};

Task_t tlKingpinTeleport[] =
{
	{ TASK_STOP_MOVING,			0 },
	{ TASK_KINGPIN_SET_TELEPORT_DEST,	0 },
	{ TASK_KINGPIN_TELEPORT,		0 },
};

Schedule_t slKingpinTeleport[] =
{
	{ tlKingpinTeleport, ARRAYSIZE( tlKingpinTeleport ), 0, 0, "KINGPIN_TELEPORT" },
};

DEFINE_CUSTOM_SCHEDULES( CKingpin )
{
	slKingpinOrb,
	slKingpinTeleport,
};

IMPLEMENT_CUSTOM_SCHEDULES( CKingpin, CBaseMonster )

void CKingpin::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/kingpin.mdl" );
	UTIL_SetSize( pev, Vector( -24, -24, 0 ), Vector( 24, 24, 72 ));

	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->health		= SC_KingpinValue( "sk_kingpin_health", 450 );
	pev->view_ofs		= Vector( 0, 0, 80 );
	m_flFieldOfView		= VIEW_FIELD_FULL;	// (it has eyes all round)
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_RANGE_ATTACK2 | bits_CAP_MELEE_ATTACK1;
	m_flNextOrb = m_flNextTele = m_flTeleTime = 0;
	m_iTelePhase = 0;

	MonsterInit();

	studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR( ENT( pev ));
	m_iAttachments = hdr ? hdr->numattachments : 0;
	static const float angles[4] = { 0, 180, -90, 90 };	// front, back, right, left
	for( int i = 0; i < 4; i++ )
	{
		m_eyes[i].iAttachment = i;
		m_eyes[i].angle = angles[i];
		m_eyes[i].lastAttack = 0;
		CSprite *pSpr = CSprite::SpriteCreate( KINGPIN_EYE, pev->origin, FALSE );
		if( !pSpr )
			continue;
		pSpr->SetAttachment( edict(), i + 1 );
		pSpr->pev->rendermode = kRenderTransAdd;
		pSpr->pev->renderamt = 255;
		pSpr->pev->scale = 0.2f;
		pSpr->pev->framerate = 20.0f;
		pSpr->TurnOn();
		m_eyes[i].hSprite = pSpr;
	}
}

void CKingpin::Precache( void )
{
	PRECACHE_MODEL( "models/kingpin.mdl" );
	m_iBeam = PRECACHE_MODEL( KINGPIN_BEAM );
	m_iSpit = PRECACHE_MODEL( "sprites/tinyspit.spr" );
	PRECACHE_MODEL( KINGPIN_TELE );
	PRECACHE_MODEL( KINGPIN_FLARE );
	PRECACHE_MODEL( KINGPIN_EYE );
	PRECACHE_SOUND_ARRAY( pAttackHitSounds );
	PRECACHE_SOUND_ARRAY( pAttackMissSounds );
	PRECACHE_SOUND_ARRAY( pAttackSounds );
	PRECACHE_SOUND_ARRAY( pIdleSounds );
	PRECACHE_SOUND_ARRAY( pAlertSounds );
	PRECACHE_SOUND_ARRAY( pPainSounds );
	PRECACHE_SOUND_ARRAY( pDieSounds );
	PRECACHE_SOUND( KINGPIN_BEAM_SOUND );
	PRECACHE_SOUND( KINGPIN_TELE_SOUND_IN );
	PRECACHE_SOUND( KINGPIN_TELE_SOUND_OUT );
	PRECACHE_SOUND( ORB_GROW_SOUND );
	UTIL_PrecacheOther( "kingpin_plasma_ball" );
}

void CKingpin::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case KINGPIN_AE_CONJURE_ORB:
		if( m_hOrb == 0 )
		{
			CBaseEntity *pOrb = CBaseEntity::Create( "kingpin_plasma_ball", pev->origin + Vector( 0, 0, 64 ), g_vecZero, edict());
			if( pOrb )
			{
				pOrb->pev->scale = 0.1f;
				m_hOrb = pOrb;
				pev->framerate = 0.3f;	// slowed, while the ball grows
				EMIT_SOUND_DYN( pOrb->edict(), CHAN_WEAPON, ORB_GROW_SOUND, 1.0, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ));
			}
		}
		break;
	case KINGPIN_AE_SHOOT_ORB:
		if( m_hOrb != 0 )
		{
			UTIL_MakeVectors( pev->angles );
			CKingpinBall *pOrb = (CKingpinBall *)(CBaseEntity *)m_hOrb;
			pOrb->m_lastDir = gpGlobals->v_forward;
			pOrb->Activate();
			m_hOrb = NULL;	// another can be conjured
		}
		m_flNextOrb = gpGlobals->time + 8;
		pev->framerate = 1.0f;
		break;
	case KINGPIN_AE_SWING_LEFT:
	case KINGPIN_AE_SWING_RIGHT:
	{
		bool left = pEvent->event == KINGPIN_AE_SWING_LEFT;
		CBaseEntity *pHurt = CheckTraceHullAttack( KINGPIN_MELEE_DIST, (int)SC_KingpinValue( "sk_kingpin_melee", 40 ), DMG_SLASH );
		if( pHurt )
		{
			if( pHurt->pev->flags & ( FL_MONSTER | FL_CLIENT ))
			{
				pHurt->pev->punchangle.x = 5;
				pHurt->pev->punchangle.z = left ? -18 : 18;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * ( left ? -100 : 100 );
			}
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackHitSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));
		}
		else
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, RANDOM_SOUND_ARRAY( pAttackMissSounds ), 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ));
		break;
	}
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

Schedule_t *CKingpin::GetSchedule( void )
{
	// it never flinches or backs off
	ClearConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE );

	if( m_MonsterState == MONSTERSTATE_COMBAT && m_flNextTele < gpGlobals->time )
	{
		// crowded, or the enemy out of sight: somewhere else
		int nearby = 0;
		CBaseEntity *pOther = NULL;
		while(( pOther = UTIL_FindEntityInSphere( pOther, pev->origin, 256 )) != NULL )
			if( SC_KingpinTarget( this, pOther ))
				nearby++;
		if( nearby > 1 || HasConditions( bits_COND_ENEMY_OCCLUDED ))
			return slKingpinTeleport;
	}
	return CBaseMonster::GetSchedule();
}

Schedule_t *CKingpin::GetScheduleOfType( int Type )
{
	switch( Type )
	{
	case SCHED_CHASE_ENEMY_FAILED:
		if( m_flNextOrb < gpGlobals->time && HasConditions( bits_COND_SEE_ENEMY ))
			return slKingpinOrb;
		if( m_flNextTele < gpGlobals->time )
			return slKingpinTeleport;
		break;
	case SCHED_MELEE_ATTACK1:
		AttackSound();
		break;
	case SCHED_RANGE_ATTACK2:
		return slKingpinOrb;
	}
	return CBaseMonster::GetScheduleOfType( Type );
}

void CKingpin::CancelOrb( void )
{
	CKingpinBall *pOrb = (CKingpinBall *)(CBaseEntity *)m_hOrb;
	if( pOrb && !pOrb->m_isActive )
		UTIL_Remove( pOrb );
	m_hOrb = NULL;
}

void CKingpin::ScheduleChange( void )
{
	CancelOrb();
	if( m_iTelePhase )
		EndTeleport();	// (broken off: back where it was, not stuck between)
	m_iTelePhase = 0;
}

void CKingpin::UpdateOnRemove( void )
{
	CancelOrb();
	for( int i = 0; i < 4; i++ )
	{
		if( m_eyes[i].hSprite != 0 )
			UTIL_Remove( m_eyes[i].hSprite );
		if( m_eyes[i].hFlare != 0 )
			UTIL_Remove( m_eyes[i].hFlare );
	}
	CBaseMonster::UpdateOnRemove();
}

// each eye charges for four seconds, then strikes a hostile in front of it with lightning (one eye a think)
void CKingpin::LaserEyesThink( void )
{
	CBaseEntity *targets[32];
	int n = 0;
	CBaseEntity *pOther = NULL;
	while(( pOther = UTIL_FindEntityInSphere( pOther, pev->origin, 2048 )) != NULL && n < 32 )
		if( SC_KingpinTarget( this, pOther ))
			targets[n++] = pOther;

	bool fired = false;
	for( int i = 0; i < 4 && !fired; i++ )
	{
		kingpin_eye_t &eye = m_eyes[i];
		CBaseEntity *pSpr = eye.hSprite;
		if( !pSpr )
			continue;
		Vector eyePos, eyeAng;
		if( eye.iAttachment < m_iAttachments )
			GetAttachment( eye.iAttachment, eyePos, eyeAng );
		else
			eyePos = Center();
		UTIL_MakeVectors( Vector( 0, pev->angles.y + eye.angle, 0 ));
		Vector eyeDir = gpGlobals->v_forward;

		float timeLeft = KINGPIN_EYE_RECHARGE - ( gpGlobals->time - eye.lastAttack );
		if( timeLeft < 0 )
		{
			bool canShoot = gpGlobals->time - m_flTeleTime > 1.0f && m_iTelePhase == 0;
			for( int k = 0; k < n && canShoot; k++ )
			{
				CBaseEntity *pTarget = targets[k];
				TraceResult tr;
				UTIL_TraceLine( eyePos, pTarget->Center(), dont_ignore_monsters, edict(), &tr );
				if( CBaseEntity::Instance( tr.pHit ) != pTarget || DotProduct( eyeDir, pTarget->Center() - eyePos ) < 0 )
					continue;	// hidden, or behind this eye

				EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, KINGPIN_BEAM_SOUND, 1.0, ATTN_NORM, 0, RANDOM_LONG( 95, 105 ));
				CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3f );
				SC_TE_BeamEntPoint( this, eye.iAttachment + 1, pTarget->Center(), m_iBeam, 2, 50, 32, 255, 0, 255, 150 );

				CSprite *pFlare = CSprite::SpriteCreate( KINGPIN_FLARE, pev->origin, FALSE );
				if( pFlare )
				{
					pFlare->SetAttachment( edict(), eye.iAttachment + 1 );
					pFlare->pev->rendermode = kRenderTransAdd;
					pFlare->pev->renderamt = 255;
					pFlare->pev->scale = 0.8f;
					if( eye.hFlare != 0 )
						UTIL_Remove( eye.hFlare );
					eye.hFlare = pFlare;
				}
				pTarget->TakeDamage( pev, pev, SC_KingpinValue( "sk_kingpin_lightning", 25 ), DMG_ENERGYBEAM );
				eye.lastAttack = gpGlobals->time;
				fired = true;
				break;
			}
			pSpr->pev->renderamt = 255;
			pSpr->pev->scale = 0.2f;
		}
		else
		{
			// charging: the eye's glow swells
			float power = ( KINGPIN_EYE_RECHARGE - timeLeft ) / KINGPIN_EYE_RECHARGE;
			power = power * power * power * power;
			pSpr->pev->renderamt = power * 128.0f + 128.0f;
			pSpr->pev->scale = power * 0.15f + 0.05f;
			pSpr->pev->renderfx = 0;
			CBaseEntity *pFlare = eye.hFlare;
			if( pFlare )
			{
				pFlare->pev->scale *= 0.5f;
				if( pFlare->pev->scale < 0.02f )
				{
					UTIL_Remove( pFlare );
					eye.hFlare = NULL;
				}
			}
		}
	}
}

// grenades thrown at it by its enemies swerve away
void CKingpin::DeflectThink( void )
{
	CBaseEntity *pOther = NULL;
	while(( pOther = UTIL_FindEntityInSphere( pOther, pev->origin, 300 )) != NULL )
	{
		if(( pOther->pev->effects & EF_NODRAW ) || pOther->pev->solid == SOLID_NOT || pOther->pev->movetype != MOVETYPE_BOUNCE || !pOther->pev->owner)
			continue;
		CBaseEntity *pOwner = Instance( pOther->pev->owner );
		if( !pOwner || pOwner == this || !( pOwner->pev->flags & ( FL_MONSTER | FL_CLIENT )) || IRelationship( pOwner ) == R_AL )
			continue;
		Vector away = ( pOther->pev->origin - pev->origin ).Normalize();
		if( DotProduct( away, pOther->pev->velocity.Normalize()) < -0.2f )
		{
			// coming straight in: aside, not back
			UTIL_MakeVectors( UTIL_VecToAngles( pOther->pev->velocity ));
			away = DotProduct( away, gpGlobals->v_right ) > 0 ? gpGlobals->v_right : gpGlobals->v_right * -1;
		}
		pOther->pev->velocity = pOther->pev->velocity + away * 300;
		pOther->pev->angles = UTIL_VecToAngles( pOther->pev->velocity.Normalize());
		SC_TE_BeamEnts( this, pOther, m_iBeam, 1, 16, 32 );
	}
}

void CKingpin::MonsterThink( void )
{
	if( IsAlive())
	{
		LaserEyesThink();
		DeflectThink();
	}
	CBaseMonster::MonsterThink();
}

void CKingpin::GibMonster( void )
{
	CBaseMonster::GibMonster();
	Vector o = pev->origin;
	for( int i = 0; i < 2; i++ )
	{
		Vector at = o + Vector( 0, 0, 40 + 48 * i );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, at );
			WRITE_BYTE( TE_SPRITE_SPRAY );
			WRITE_COORD( at.x ); WRITE_COORD( at.y ); WRITE_COORD( at.z );
			WRITE_COORD( 0 ); WRITE_COORD( 0 ); WRITE_COORD( 1 );
			WRITE_SHORT( m_iSpit );
			WRITE_BYTE( 20 );	// count
			WRITE_BYTE( 100 - 50 * i );	// speed
			WRITE_BYTE( 40 + 40 * i );	// noise
		MESSAGE_END();
	}
	float height = pev->maxs.z * 2;
	for( int i = 0; i < 4; i++ )
		SpawnBlood( o + Vector( 0, 0, 32 + height * 0.1f + height * 0.2f * i ), BloodColor(), 255 );
}

void CKingpin::EndTeleport( void )
{
	pev->renderfx = m_iOldRenderFx;
	pev->renderamt = m_flOldRenderAmt;
	pev->rendermode = m_iOldRenderMode;
	pev->rendercolor = m_vecOldRenderColor;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	UTIL_SetOrigin( pev, pev->origin );
}

void CKingpin::StartTask( Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_DIE:
		CBaseMonster::StartTask( pTask );
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 1;
		pev->rendercolor = Vector( 128, 0, 128 );
		for( int i = 0; i < 4; i++ )
		{
			if( m_eyes[i].hFlare != 0 )
				UTIL_Remove( m_eyes[i].hFlare );
		}
		break;
	case TASK_KINGPIN_TELEPORT:
		m_iOldRenderFx = pev->renderfx;
		m_iOldRenderMode = pev->rendermode;
		m_vecOldRenderColor = pev->rendercolor;
		m_flOldRenderAmt = pev->renderamt;
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 230;
		pev->rendermode = kRenderTransTexture;
		pev->rendercolor = Vector( 0, 128, 64 );
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NOCLIP;
		pev->velocity = g_vecZero;
		pev->takedamage = DAMAGE_NO;
		m_flTeleTime = gpGlobals->time;
		m_iTelePhase = 1;
		SetActivity( ACT_DIESIMPLE );	// (sinking down: the way out)
		UTIL_EmitAmbientSound( edict(), m_vecTeleDst, KINGPIN_TELE_SOUND_IN, 1.0f, ATTN_NORM, 0, 100 );
		break;
	case TASK_KINGPIN_SET_TELEPORT_DEST:
	{
		// the furthest open spot around, out to 1024 (level first, then a little upward), on the floor
		const float up = 64;
		Vector start = pev->origin + Vector( 0, 0, up ), best = g_vecZero;
		float bestDist = 0, angle = RANDOM_FLOAT( 0, 360 );
		static const float pitches[2] = { -40, 0 };
		bool found = false;
		TraceResult tr;
		TRACE_MONSTER_HULL( edict(), pev->origin, start, ignore_monsters, NULL, &tr );
		if( tr.flFraction < 1.0f )
		{
			TaskFail();
			break;
		}
		for( int k = 0; k < 2 && !found; k++ )
		{
			for( int i = 0; i < 18; i++ )
			{
				angle += 20;
				UTIL_MakeVectors( Vector( pitches[k], angle, 0 ));
				TRACE_MONSTER_HULL( edict(), start, start + gpGlobals->v_forward * 1024, ignore_monsters, NULL, &tr );
				float dist = ( tr.vecEndPos - start ).Length();
				if( dist < 256 )
					continue;	// too near where it is
				Vector from = tr.vecEndPos;
				TRACE_MONSTER_HULL( edict(), from, from - Vector( 0, 0, 1024 ), ignore_monsters, NULL, &tr );
				Vector tele = tr.vecEndPos + Vector( 0, 0, Q_min( up, ( tr.vecEndPos - from ).Length()));
				if( POINT_CONTENTS( tele ) != CONTENTS_EMPTY && found )
					continue;	// out of water and lava, if it can
				if( dist > bestDist )
				{
					bestDist = dist;
					best = tele;
					found = true;
				}
			}
		}
		if( found )
		{
			m_vecTeleSrc = start;
			m_vecTeleDst = best;
			TaskComplete();
		}
		else
			TaskFail();
		break;
	}
	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

void CKingpin::RunTask( Task_t *pTask )
{
	const Vector offset( 0, 0, 40 );
	switch( pTask->iTask )
	{
	case TASK_DIE:
	{
		CBaseMonster::RunTask( pTask );
		pev->framerate = 1.5f;
		// the eyes flare and strike the ground around it
		for( int i = 0; i < 4; i++ )
		{
			kingpin_eye_t &eye = m_eyes[i];
			Vector at, ang;
			GetAttachment( eye.iAttachment, at, ang );
			CBaseEntity *pSpr = eye.hSprite;
			if( pSpr )
			{
				pSpr->pev->scale = Q_max( 0.2f, pSpr->pev->scale + 0.03f );
				pSpr->pev->renderamt = 255;
			}
			TraceResult tr;
			UTIL_TraceLine( at, at - Vector( RANDOM_FLOAT( -0.5f, 0.5f ) * 256, RANDOM_FLOAT( -0.5f, 0.5f ) * 256, 256 ), ignore_monsters, NULL, &tr );
			SC_TE_BeamEntPoint( this, eye.iAttachment + 1, tr.vecEndPos, m_iBeam, 2, 20, 128, 255, 0, 255, 150 );
		}
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, KINGPIN_BEAM_SOUND, 0.5, ATTN_NORM, 0, RANDOM_LONG( 80, 120 ));
		if( pev->frame > 200 )
		{
			// and it bursts
			pev->renderfx = kRenderFxExplode;
			SetThink( &CKingpin::DieThink );
			pev->nextthink = gpGlobals->time + 0.15f;
		}
		break;
	}
	case TASK_KINGPIN_TELEPORT:
		pev->renderamt = Q_max( 1.0f, pev->renderamt - 10 );
		if( m_iTelePhase == 1 && gpGlobals->time - m_flTeleTime > 1.0f )
		{
			for( int i = 0; i < 2; i++ )
			{
				CSprite *pSpr = CSprite::SpriteCreate( KINGPIN_TELE, ( i ? m_vecTeleDst : m_vecTeleSrc ) + offset, FALSE );
				if( !pSpr )
					continue;
				pSpr->pev->rendermode = kRenderTransAdd;
				pSpr->pev->renderamt = 255;
				pSpr->pev->scale = 1.5f;
				pSpr->AnimateAndDie( 15.0f );
				if( i )
					UTIL_EmitAmbientSound( pSpr->edict(), m_vecTeleSrc, KINGPIN_TELE_SOUND_OUT, 1.0f, ATTN_NORM, 0, 100 );
			}
			m_iTelePhase = 2;
		}
		if( gpGlobals->time - m_flTeleTime > 2.0f )
		{
			SC_TE_BeamPoints( m_vecTeleSrc + offset, m_vecTeleDst + offset, m_iBeam, 2, 80, 64, 255, 255, 255, 150 );
			UTIL_SetOrigin( pev, m_vecTeleDst );
			EndTeleport();
			m_flNextTele = gpGlobals->time + 5.0f;

			// whatever stands where it lands: a creature is torn apart, a player thrown clear
			CBaseEntity *pList[32];
			int count = UTIL_EntitiesInBox( pList, 32, pev->origin + pev->mins, pev->origin + pev->maxs, FL_MONSTER | FL_CLIENT );
			for( int i = 0; i < count; i++ )
			{
				if( pList[i] == this || !pList[i]->pev->takedamage )
					continue;
				if( pList[i]->IsPlayer())
				{
					pList[i]->pev->velocity = pList[i]->pev->velocity + ( pList[i]->pev->origin - pev->origin ).Normalize() * 400 + Vector( 0, 0, 250 );
					pList[i]->TakeDamage( pev, pev, SC_KingpinValue( "sk_kingpin_tele_blast", 15 ), DMG_ENERGYBEAM );
				}
				else
					pList[i]->TakeDamage( pev, pev, SC_KingpinValue( "sk_kingpin_telefrag", 500 ), DMG_ALWAYSGIB | DMG_ENERGYBEAM );
			}
			SetActivity( ACT_IDLE );
			m_flTeleTime = gpGlobals->time;	// (the eyes wait a moment)
			m_iTelePhase = 0;
			TaskComplete();
		}
		break;
	default:
		CBaseMonster::RunTask( pTask );
		break;
	}
}

BOOL CKingpin::CheckRangeAttack2( float flDot, float flDist )
{
	return m_flNextOrb < gpGlobals->time && flDist > ORB_RADIUS + 128;
}

void CKingpin::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	// its head is most of it: a head hit counts as the body's
	if( ptr->iHitgroup == HITGROUP_HEAD )
		ptr->iHitgroup = HITGROUP_CHEST;
	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

void CKingpin::PainSound( void )
{
	if( RANDOM_LONG( 0, 5 ) < 2 )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pPainSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( KINGPIN_PITCH, KINGPIN_PITCH + 10 ));
}

void CKingpin::DeathSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pDieSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( KINGPIN_PITCH, KINGPIN_PITCH + 10 ));
}

void CKingpin::AlertSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAlertSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( KINGPIN_PITCH, KINGPIN_PITCH + 10 ));
}

void CKingpin::IdleSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pIdleSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( KINGPIN_PITCH, KINGPIN_PITCH + 10 ));
}

void CKingpin::AttackSound( void )
{
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, RANDOM_SOUND_ARRAY( pAttackSounds ), 1.0, ATTN_NORM, 0, RANDOM_LONG( 150, 160 ));
}
