/*
sc_minigun.cpp - Svencraft: Sven Co-op's minigun

Hold +attack: the barrels wind up for half a second, then it fires a 5.56 round every 0.06 s for as long as the
button and the ammo last; +attack2 keeps them spinning without firing, ready to shoot at once. Let go and they wind
down. Carrying it slows the player (210), spinning it more (150). Recoil and spread go through sc_gunfeel.h.
Predicted like Half-Life's guns: the barrel state is m_fInSpecialReload, the time to the next state pev->fuser1,
both in weapon_data_t; rounds and sounds of each shot go through the "events/minigun.sc" event (cl_dll/ev_hldm.cpp
EV_FireMinigun). Sven Co-op's models (v_minigun, p_minigunspin/idle, w_minigun) and heavy-weapons-grunt sounds.
Compiled into both DLLs.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "sc_gunfeel.h"
#include "sc_minigun.h"

enum minigun_e
{
	MINIGUN_IDLE = 0,
	MINIGUN_IDLE2,
	MINIGUN_GENTLEIDLE,
	MINIGUN_STILLIDLE,
	MINIGUN_DRAW,
	MINIGUN_HOLSTER,
	MINIGUN_SPINUP,
	MINIGUN_SPINDOWN,
	MINIGUN_SPINIDLE,
	MINIGUN_SPINFIRE,
	MINIGUN_SPINIDLEDOWN,
};

#define MINIGUN_SPINUP_TIME	0.5f
#define MINIGUN_SPINDOWN_TIME	0.9f
#define MINIGUN_CYCLE		0.06f
#define MINIGUN_SPEED_CARRY	210.0f
#define MINIGUN_SPEED_SPIN	150.0f

static const scgunfeel_t g_SCFeelMinigun = { 0.035f, 0.040f, 0.12f, 0.70f, 0.002f, 0.030f, 0.25f, 0.35f };

LINK_ENTITY_TO_CLASS( weapon_minigun, CMinigun )

void CMinigun::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), "models/w_minigun.mdl" );
	m_iId = WEAPON_MINIGUN;
	m_iDefaultAmmo = MINIGUN_DEFAULT_GIVE;
	FallInit();
}

void CMinigun::Precache( void )
{
	PRECACHE_MODEL( "models/v_minigun.mdl" );
	PRECACHE_MODEL( "models/w_minigun.mdl" );
	PRECACHE_MODEL( "models/p_minigunidle.mdl" );
	PRECACHE_MODEL( "models/p_minigunspin.mdl" );
	PRECACHE_MODEL( "models/saw_shell.mdl" );
	PRECACHE_SOUND( "hassault/hw_spinup.wav" );
	PRECACHE_SOUND( "hassault/hw_spindown.wav" );
	PRECACHE_SOUND( "hassault/hw_spin.wav" );
	PRECACHE_SOUND( "hassault/hw_shoot1.wav" );
	PRECACHE_SOUND( "hassault/hw_shoot2.wav" );
	PRECACHE_SOUND( "hassault/hw_shoot3.wav" );
	m_usMinigun = PRECACHE_EVENT( 1, "events/minigun.sc" );
}

int CMinigun::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556";
	p->iMaxAmmo1 = _556_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;	// fed straight from the belt
	p->iSlot = 5;
	p->iPosition = 3;		// after the M249, displacer and sniper rifle
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_MINIGUN;
	p->iWeight = MINIGUN_WEIGHT;
	return 1;
}

int CMinigun::AddToPlayer( CBasePlayer *pPlayer )
{
	if( CBasePlayerWeapon::AddToPlayer( pPlayer ))
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CMinigun::Deploy( void )
{
	m_fInSpecialReload = SC_SPIN_STILL;
	pev->fuser1 = 0.0f;
	return DefaultDeploy( "models/v_minigun.mdl", "models/p_minigunidle.mdl", MINIGUN_DRAW, "saw" );
}

void CMinigun::Holster( int skiplocal )
{
	if( m_fInSpecialReload != SC_SPIN_STILL )
	{
#if !CLIENT_DLL
		STOP_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "hassault/hw_spin.wav" );
#endif
		m_fInSpecialReload = SC_SPIN_STILL;
	}
	m_pPlayer->pev->maxspeed = 0.0f;	// the server's own again
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;
	SendWeaponAnim( MINIGUN_HOLSTER );
}

// the player is slowed by the weight, more while it spins
void CMinigun::SetSpeed( void )
{
	m_pPlayer->pev->maxspeed = m_fInSpecialReload == SC_SPIN_STILL ? MINIGUN_SPEED_CARRY : MINIGUN_SPEED_SPIN;
}

void CMinigun::ItemPostFrame( void )
{
	// the barrels move on by themselves: up to speed, or down to rest
	if( m_fInSpecialReload == SC_SPIN_UP && pev->fuser1 <= 0.0f )
		m_fInSpecialReload = SC_SPIN_ON;
	else if( m_fInSpecialReload == SC_SPIN_DOWN && pev->fuser1 <= 0.0f )
	{
		m_fInSpecialReload = SC_SPIN_STILL;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
	}
	SetSpeed();
	CBasePlayerWeapon::ItemPostFrame();
}

// +attack and +attack2 both spin the barrels; only +attack fires once they are up to speed
void CMinigun::Spin( bool fire )
{
	if( m_pPlayer->pev->waterlevel == 3 )
	{
		SpinDown();
		return;
	}
	if( m_fInSpecialReload == SC_SPIN_STILL || m_fInSpecialReload == SC_SPIN_DOWN )
	{
		m_fInSpecialReload = SC_SPIN_UP;
		pev->fuser1 = MINIGUN_SPINUP_TIME;
		SendWeaponAnim( MINIGUN_SPINUP );
#if !CLIENT_DLL
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "hassault/hw_spinup.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
		m_pPlayer->pev->weaponmodel = MAKE_STRING( "models/p_minigunspin.mdl" );
#endif
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.05f;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + MINIGUN_SPINUP_TIME;
		return;
	}
	if( m_fInSpecialReload == SC_SPIN_UP )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.05f;
		return;
	}

	// up to speed
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.2f;
	if( !fire || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		if( fire )
			PlayEmptySound();
		if( m_pPlayer->pev->weaponanim != MINIGUN_SPINIDLE )
			SendWeaponAnim( MINIGUN_SPINIDLE );
#if !CLIENT_DLL
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "hassault/hw_spin.wav", 0.7f, ATTN_NORM, SND_CHANGE_VOL, PITCH_NORM );
#endif
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1f;
		return;
	}

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	m_pPlayer->pev->effects = (int)( m_pPlayer->pev->effects ) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	float s = SC_GunFeel() ? SC_GunSpread( this, g_SCFeelMinigun ) : VECTOR_CONE_4DEGREES.x;
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( s, s, s ), 8192, BULLET_PLAYER_556, 3, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
#if CLIENT_WEAPONS
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif
	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usMinigun, 0.0f, g_vecZero, g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );
	if( SC_GunFeel())
		SC_GunKick( this, g_SCFeelMinigun );

	if( m_pPlayer->pev->weaponanim != MINIGUN_SPINFIRE )
		SendWeaponAnim( MINIGUN_SPINFIRE );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( MINIGUN_CYCLE );
	if( m_flNextPrimaryAttack < UTIL_WeaponTimeBase())
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + MINIGUN_CYCLE;

	if( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
}

void CMinigun::PrimaryAttack( void )
{
	Spin( true );
}

void CMinigun::SecondaryAttack( void )
{
	Spin( false );
}

void CMinigun::SpinDown( void )
{
	if( m_fInSpecialReload != SC_SPIN_UP && m_fInSpecialReload != SC_SPIN_ON )
		return;
	m_fInSpecialReload = SC_SPIN_DOWN;
	pev->fuser1 = MINIGUN_SPINDOWN_TIME;
	SendWeaponAnim( MINIGUN_SPINDOWN );
#if !CLIENT_DLL
	STOP_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "hassault/hw_spin.wav" );
	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "hassault/hw_spindown.wav", 0.9f, ATTN_NORM, 0, PITCH_NORM );
	m_pPlayer->pev->weaponmodel = MAKE_STRING( "models/p_minigunidle.mdl" );
#endif
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + MINIGUN_SPINDOWN_TIME;
}

void CMinigun::WeaponIdle( void )
{
	// no button held: the barrels wind down
	SpinDown();
	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() || m_fInSpecialReload != SC_SPIN_STILL )
		return;
	int r = UTIL_SharedRandomLong( m_pPlayer->random_seed, 0, 2 );
	SendWeaponAnim( r == 0 ? MINIGUN_IDLE : r == 1 ? MINIGUN_IDLE2 : MINIGUN_GENTLEIDLE );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 6.0f, 12.0f );
}
