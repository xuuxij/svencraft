/*
sc_hwgrunt.cpp - Svencraft: Sven Co-op's heavy weapons grunt (monster_hwgrunt), on Half-Life's grunt AI

A grunt with a minigun (Sven's hwgrunt.mdl): 200 health (Sven's sk_hwgrunt_health), no grenades, no kicks, never
crouches. The barrels spin up before his first round (the spinup sequence and sound) and keep turning while he
fights, winding down a couple of seconds after his last burst; he fires 5.56 rounds about fifteen a second for as
long as his attack lasts (the model's attack sequence only flashes the muzzle: the rounds come from here). Dying,
he drops the minigun, which any player can take (hotbar item 147). SevenKewp's CHWGrunt was the reference; it
needs its own grunt base, so this one stands on Half-Life's CHGrunt instead.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadmonster.h"
#include "weapons.h"
#include "soundent.h"
#include "talkmonster.h"
#include "hgrunt.h"

#define HWGRUNT_HEALTH		200	// Sven's sk_hwgrunt_health
#define HWGRUNT_GUN_GROUP	1
#define HWGRUNT_GUN_MINIGUN	0
#define HWGRUNT_GUN_NONE	4
#define HWGRUNT_AE_DROP_GUN	11	// the death sequences' event, as Half-Life's grunt
#define HWGRUNT_SPINUP		0.7f	// the spinup sequence
#define HWGRUNT_ROUND		0.066f	// about fifteen rounds a second

static const char *g_HWShoot[] = { "hassault/hw_shoot1.wav", "hassault/hw_shoot2.wav", "hassault/hw_shoot3.wav" };

class CHWGrunt : public CHGrunt
{
public:
	void Spawn( void );
	void Precache( void );
	void SetActivity( Activity NewActivity );
	void RunTask( Task_t *pTask );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CheckMeleeAttack1( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2( float flDot, float flDist ) { return FALSE; }
	void CheckAmmo( void ) { }
	Vector GetGunPosition( void );
	void PrescheduleThink( void );
	void GibMonster( void );
	void Killed( entvars_t *pevAttacker, int iGib );

private:
	void FireRound( void );
	void SpinDown( void );
	void DropMinigun( void );

	float m_flSpunUp;	// when the barrels are (or were) up to speed; 0: still
	float m_flNextRound;
	float m_flLastFire;
};

LINK_ENTITY_TO_CLASS( monster_hwgrunt, CHWGrunt )

void CHWGrunt::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/hwgrunt.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	pev->health		= HWGRUNT_HEALTH;
	m_flFieldOfView		= 0.2f;
	m_MonsterState		= MONSTERSTATE_NONE;
	m_flNextGrenadeCheck	= gpGlobals->time + 1;
	m_flNextPainTime	= gpGlobals->time;
	m_iSentence		= -1;	// HGRUNT_SENT_NONE
	m_afCapability		= bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;
	m_fEnemyEluded		= FALSE;
	m_fFirstEncounter	= TRUE;
	m_fStanding		= TRUE;
	m_HackedGunPos		= Vector( 0, 0, 48 );
	pev->weapons		= 0;	// none of the grunt's: no grenades
	m_cClipSize = m_cAmmoLoaded = 9999;	// fed from a belt: never reloads
	m_flSpunUp = m_flNextRound = m_flLastFire = 0.0f;
	SetBodygroup( HWGRUNT_GUN_GROUP, HWGRUNT_GUN_MINIGUN );

	CTalkMonster::g_talkWaitTime = 0;
	MonsterInit();
}

void CHWGrunt::Precache( void )
{
	CHGrunt::Precache();	// the grunt's voice, pains, deaths and shells
	m_voicePitch = 85 + RANDOM_LONG( 0, 10 );	// a heavier man
	PRECACHE_MODEL( "models/hwgrunt.mdl" );
	PRECACHE_SOUND( "hassault/hw_spinup.wav" );
	PRECACHE_SOUND( "hassault/hw_spindown.wav" );
	PRECACHE_SOUND( "hassault/hw_spin.wav" );
	for( int i = 0; i < 3; i++ )
		PRECACHE_SOUND( g_HWShoot[i] );
	UTIL_PrecacheOther( "weapon_minigun" );	// (registered with the other weapons in W_Precache)
}

Vector CHWGrunt::GetGunPosition( void )
{
	return pev->origin + Vector( 0, 0, 48 );	// at the hip, where he holds it
}

// shooting: the first attack after the barrels stopped is the spin-up, the ones after it the firing. The grunt's
// moments before firing (the aimed pause, the hand signals) are the spin-up too, or more firing once the barrels
// turn; the crouches he has no animation for are the standing idle
void CHWGrunt::SetActivity( Activity NewActivity )
{
	if( NewActivity == ACT_CROUCH || NewActivity == ACT_CROUCHIDLE )
		NewActivity = ACT_IDLE;
	if( NewActivity == ACT_IDLE )
	{
		// (the grunt's idle in a fight is the angry one, which this model hasn't got: the plain idle)
		CBaseMonster::SetActivity( ACT_IDLE );
		return;
	}
	bool shooting = NewActivity == ACT_RANGE_ATTACK1 || NewActivity == ACT_IDLE_ANGRY || NewActivity == ACT_SIGNAL1
		|| NewActivity == ACT_SIGNAL2 || NewActivity == ACT_SIGNAL3;
	if( !shooting )
	{
		CHGrunt::SetActivity( NewActivity );
		return;
	}
	int seq;
	if( m_flSpunUp <= 0.0f )
	{
		m_flSpunUp = gpGlobals->time + HWGRUNT_SPINUP;
		m_flNextRound = m_flSpunUp;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "hassault/hw_spinup.wav", 0.9f, ATTN_NORM );
		seq = LookupSequence( "spinup" );
	}
	else
		seq = LookupSequence( "attack" );
	if( seq < 0 )
	{
		CHGrunt::SetActivity( NewActivity );
		return;
	}
	m_Activity = NewActivity;
	m_IdealActivity = NewActivity;
	if( pev->sequence != seq || !m_fSequenceLoops )
		pev->frame = 0;
	pev->sequence = seq;
	ResetSequenceInfo();
	SetYawSpeed();
}

void CHWGrunt::RunTask( Task_t *pTask )
{
	CHGrunt::RunTask( pTask );
	// rounds come while the attack sequence plays at an enemy in sight, the barrels up to speed
	if( pev->sequence != LookupSequence( "attack" ) || m_hEnemy == 0 || !HasConditions( bits_COND_SEE_ENEMY )
		|| m_flSpunUp <= 0.0f || m_flSpunUp > gpGlobals->time )
		return;
	if( m_flNextRound < gpGlobals->time - 0.2f )
		m_flNextRound = gpGlobals->time;
	for( int n = 0; n < 3 && m_flNextRound <= gpGlobals->time; n++ )
	{
		FireRound();
		m_flNextRound += HWGRUNT_ROUND;
	}
}

void CHWGrunt::FireRound( void )
{
	if( GetBodygroup( HWGRUNT_GUN_GROUP ) == HWGRUNT_GUN_NONE )
		return;
	Vector src = GetGunPosition(), dir = ShootAtEnemy( src );
	UTIL_MakeVectors( pev->angles );
	Vector shell = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
	EjectBrass( src - dir * 24, shell, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );
	FireBullets( 1, src, dir, VECTOR_CONE_6DEGREES, 2048, BULLET_MONSTER_556 );
	pev->effects |= EF_MUZZLEFLASH;
	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, g_HWShoot[RANDOM_LONG( 0, 2 )], 1.0f, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 12 ));
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 384, 0.3f );
	if( gpGlobals->time - m_flLastFire > 0.5f )
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "hassault/hw_spin.wav", 0.6f, ATTN_NORM );	// the barrels' hum while he fights
	m_flLastFire = gpGlobals->time;
	SetBlending( 0, UTIL_VecToAngles( dir ).x );
}

void CHWGrunt::SpinDown( void )
{
	if( m_flSpunUp <= 0.0f )
		return;
	m_flSpunUp = 0.0f;
	STOP_SOUND( ENT( pev ), CHAN_ITEM, "hassault/hw_spin.wav" );
	EMIT_SOUND( ENT( pev ), CHAN_ITEM, "hassault/hw_spindown.wav", 0.8f, ATTN_NORM );
}

// the barrels wind down a while after the last burst
void CHWGrunt::PrescheduleThink( void )
{
	CHGrunt::PrescheduleThink();
	if( m_flSpunUp > 0.0f && pev->sequence != LookupSequence( "attack" ) && pev->sequence != LookupSequence( "spinup" )
		&& gpGlobals->time - Q_max( m_flLastFire, m_flSpunUp ) > 2.0f )
		SpinDown();
}

void CHWGrunt::DropMinigun( void )
{
	if( GetBodygroup( HWGRUNT_GUN_GROUP ) == HWGRUNT_GUN_NONE )
		return;
	Vector pos, ang;
	GetAttachment( 0, pos, ang );
	if(( pos - pev->origin ).Length() > 128.0f )
		pos = pev->origin + Vector( 0, 0, 40 );
	DropItem( "weapon_minigun", pos, Vector( 0, pev->angles.y, 0 ));
	SetBodygroup( HWGRUNT_GUN_GROUP, HWGRUNT_GUN_NONE );
}

void CHWGrunt::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	if( pEvent->event == HWGRUNT_AE_DROP_GUN )
	{
		DropMinigun();
		return;
	}
	CHGrunt::HandleAnimEvent( pEvent );
}

void CHWGrunt::GibMonster( void )
{
	DropMinigun();	// not the grunt's rifle
	CBaseMonster::GibMonster();
}

void CHWGrunt::Killed( entvars_t *pevAttacker, int iGib )
{
	SpinDown();
	CHGrunt::Killed( pevAttacker, iGib );
}
