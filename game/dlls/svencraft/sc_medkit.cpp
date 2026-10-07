/*
sc_medkit.cpp - Svencraft: Sven Co-op's medkit (weapon_medkit), adapted from SevenKewp's CMedkit

Primary: heals the player or friendly creature in front of it, 10 a use (Sven's sk_plr_HpMedic) from its own
// charge, which comes back by itself (1 point every 0.6 s, up to 100; a new one holds 50). Secondary, held for two
// seconds by a fallen player or ally: brings them back for 50 charge, a player with 50 health, an ally whole (Sven's
// revive). Fallen players show a red cross to whoever holds a medkit.

Dead players stay with their bodies while they wait to respawn (no death camera), and a revive in progress holds
off their respawn (player.cpp PlayerDeathThink).
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "sc_game.h"

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;

#define MEDKIT_HEAL		10	// Sven's sk_plr_HpMedic
#define MEDKIT_START		50	// a new medkit's charge (SevenKewp's sk_plr_medkit_start_ammo)
#define MEDKIT_MAX		100	// sk_ammo_max_medkit
#define MEDKIT_RECHARGE		0.6f	// seconds a point (sk_plr_medkit_recharge_delay)
#define MEDKIT_REVIVE_COST	50	// sk_plr_medkit_revive_cost
#define MEDKIT_REVIVE_HEALTH	50
#define MEDKIT_REVIVE_RADIUS	64.0f
#define MEDKIT_REVIVE_TIME	2.0f
#define MEDKIT_REACH		32.0f

enum medkit_e
{
	MEDKIT_IDLE = 0,
	MEDKIT_LONGIDLE,
	MEDKIT_LONGUSE,
	MEDKIT_SHORTUSE,
	MEDKIT_HOLSTER,
	MEDKIT_DRAW
};

// until when a dead player's respawn is held off by someone reviving them
static float g_flReviveHold[33];	// by player index (32 players at most)

bool SC_BeingRevived( CBasePlayer *pPlayer )
{
	int i = pPlayer->entindex();
	return i > 0 && i <= 32 && g_flReviveHold[i] > gpGlobals->time;
}

// the copy of a dead player's body in the body queue (world.cpp CopyToBodyQue marks it with the player's index)
static CBaseEntity *SC_PlayerCorpse( CBasePlayer *pPlayer )
{
	CBaseEntity *pBody = NULL;
	while(( pBody = UTIL_FindEntityByClassname( pBody, "bodyque" )) != NULL )
		if( pBody->pev->modelindex && pBody->pev->renderfx == kRenderFxDeadPlayer && (int)pBody->pev->renderamt == pPlayer->entindex())
			return pBody;
	return NULL;
}

// a dead player up again where they lie, crouched in case it is tight there; the hotbar hands back the bare hand and
// the tools (what they carried still lies around them)
void SC_RevivePlayer( CBasePlayer *pPlayer, int health )
{
	CBaseEntity *pBody = SC_PlayerCorpse( pPlayer );
	if( pBody )
	{
		pBody->pev->modelindex = 0;
		pBody->pev->effects |= EF_NODRAW;
	}
	g_flReviveHold[pPlayer->entindex()] = 0.0f;

	pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
	pPlayer->pev->iuser1 = pPlayer->pev->iuser2 = 0;
	pPlayer->pev->modelindex = g_ulModelIndexPlayer;
	pPlayer->pev->effects &= ~EF_NODRAW;
	pPlayer->pev->deadflag = DEAD_NO;
	pPlayer->pev->health = health;
	pPlayer->pev->takedamage = DAMAGE_AIM;
	pPlayer->pev->solid = SOLID_SLIDEBOX;
	pPlayer->pev->movetype = MOVETYPE_WALK;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles.x = pPlayer->pev->angles.z = 0.0f;
	pPlayer->pev->framerate = 1.0f;
	UTIL_SetSize( pPlayer->pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
	pPlayer->pev->flags |= FL_DUCKING;
	pPlayer->m_afPhysicsFlags |= PFLAG_DUCKING;
	pPlayer->pev->view_ofs = VEC_DUCK_VIEW;
	pPlayer->m_flRespawnTimer = 0.0f;
	pPlayer->m_iClientHealth = -1;
	pPlayer->m_flNextAttack = UTIL_WeaponTimeBase();
	pPlayer->pev->nextthink = -1;
	pPlayer->SetAnimation( PLAYER_IDLE );
	SC_PlayerSpawn( pPlayer );
	EMIT_SOUND( ENT( pPlayer->pev ), CHAN_VOICE, "player/pl_jumpland2.wav", 1.0f, ATTN_NORM );
}

class CMedkit : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 1; }
	int GetItemInfo( ItemInfo *p );
	int AddToPlayer( CBasePlayer *pPlayer );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );
	void ItemPostFrame( void );
	BOOL UseDecrement( void ) { return FALSE; }

private:
	void Recharge( void );
	void CancelRevive( void );
	CBaseEntity *ReviveTarget( void );

	float m_flRecharge;		// when the next point of charge comes
	float m_flReviveDone;		// when the revive being held completes (0: none)
	float m_flNextHint;		// the red crosses over the dead
	float m_flNextMessage;
	EHANDLE m_hReviving;
	int m_iCross;
};

LINK_ENTITY_TO_CLASS( weapon_medkit, CMedkit )

void CMedkit::Spawn( void )
{
	Precache();
	m_iId = WEAPON_MEDKIT;
	SET_MODEL( ENT( pev ), "models/w_pmedkit.mdl" );
	m_iClip = -1;
	m_iDefaultAmmo = MEDKIT_START;
	FallInit();
}

void CMedkit::Precache( void )
{
	PRECACHE_MODEL( "models/v_medkit.mdl" );
	PRECACHE_MODEL( "models/p_medkit.mdl" );
	PRECACHE_MODEL( "models/w_pmedkit.mdl" );
	PRECACHE_SOUND( "items/medshot4.wav" );
	PRECACHE_SOUND( "items/medshotno1.wav" );
	PRECACHE_SOUND( "items/suitchargeok1.wav" );
	PRECACHE_SOUND( "weapons/electro4.wav" );
	PRECACHE_SOUND( "player/pl_jumpland2.wav" );
	m_iCross = PRECACHE_MODEL( "sprites/saveme.spr" );
}

int CMedkit::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "health";
	p->iMaxAmmo1 = MEDKIT_MAX;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 8;
	p->iId = WEAPON_MEDKIT;
	p->iWeight = 0;
	p->iFlags = ITEM_FLAG_NOAUTORELOAD | ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_SELECTONEMPTY;
	return 1;
}

int CMedkit::AddToPlayer( CBasePlayer *pPlayer )
{
	if( !CBasePlayerWeapon::AddToPlayer( pPlayer ))
		return FALSE;
	m_flRecharge = gpGlobals->time + MEDKIT_RECHARGE;
	return TRUE;
}

// the charge comes back a point at a time, also while the medkit is put away (caught up when it is next used)
void CMedkit::Recharge( void )
{
	if( !m_pPlayer || m_iPrimaryAmmoType < 0 )
		return;
	int &ammo = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];
	if( m_flRecharge <= 0.0f || ammo >= MEDKIT_MAX )
	{
		m_flRecharge = gpGlobals->time + MEDKIT_RECHARGE;
		return;
	}
	while( m_flRecharge < gpGlobals->time && ammo < MEDKIT_MAX )
	{
		ammo++;
		m_flRecharge += MEDKIT_RECHARGE;
	}
}

BOOL CMedkit::Deploy( void )
{
	Recharge();
	m_flReviveDone = 0.0f;
	m_hReviving = NULL;
	return DefaultDeploy( "models/v_medkit.mdl", "models/p_medkit.mdl", MEDKIT_DRAW, "trip" );
}

void CMedkit::Holster( int skiplocal )
{
	CancelRevive();
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;
	SendWeaponAnim( MEDKIT_HOLSTER );
}

void CMedkit::WeaponIdle( void )
{
	Recharge();
	CancelRevive();	// let go of +attack2
	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;
	if( RANDOM_FLOAT( 0.0f, 1.0f ) <= 0.2f )
	{
		SendWeaponAnim( MEDKIT_IDLE );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.4f;
	}
	else
	{
		SendWeaponAnim( MEDKIT_LONGIDLE );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.8f;
	}
}

// a red cross over each fallen player in sight, for the one holding the medkit
void CMedkit::ItemPostFrame( void )
{
	CBasePlayerWeapon::ItemPostFrame();
	if( m_flNextHint > gpGlobals->time )
		return;
	m_flNextHint = gpGlobals->time + 0.2f;
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pDead = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !pDead || pDead == m_pPlayer || pDead->IsAlive() || FStringNull( pDead->pev->netname ) || pDead->pev->iuser1
			|| ( pDead->pev->effects & EF_NODRAW ))
			continue;
		Vector at = pDead->pev->origin + Vector( 0, 0, 24 );
		if( !m_pPlayer->FVisible( at ))
			continue;
		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, m_pPlayer->pev );
			WRITE_BYTE( TE_EXPLOSION );
			WRITE_COORD( at.x );
			WRITE_COORD( at.y );
			WRITE_COORD( at.z - 10.0f );	// (the client raises explosions by 10)
			WRITE_SHORT( m_iCross );
			WRITE_BYTE( 8 );		// scale 0.8
			WRITE_BYTE( 10 );		// frames a second
			WRITE_BYTE( TE_EXPLFLAG_NOADDITIVE | TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES );
		MESSAGE_END();
	}
}

void CMedkit::PrimaryAttack( void )
{
	Recharge();
	int ammo = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector src = m_pPlayer->GetGunPosition(), end = src + gpGlobals->v_forward * MEDKIT_REACH;
	TraceResult tr;
	UTIL_TraceLine( src, end, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );
	if( tr.flFraction >= 1.0f )
		UTIL_TraceHull( src, end, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
	CBaseEntity *pHit = tr.pHit ? CBaseEntity::Instance( tr.pHit ) : NULL;
	CBaseMonster *pMon = pHit ? pHit->MyMonsterPointer() : NULL;

	// players and the creatures on their side, alive and hurt
	if( !pMon || !pMon->IsAlive() || pMon->pev->health >= pMon->pev->max_health
		|| ( !pMon->IsPlayer() && m_pPlayer->IRelationship( pMon ) > R_NO ))
		return;
	if( ammo <= 0 )
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "items/medshotno1.wav", 1.0f, ATTN_NORM );
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
		return;
	}

	// a running-down medkit gives less a use, and sounds it
	float heal = Q_min( (float)MEDKIT_HEAL, pMon->pev->max_health - pMon->pev->health );
	if( ammo < MEDKIT_HEAL )
		heal = Q_min( MEDKIT_HEAL * 0.2f, heal );
	else if( ammo < MEDKIT_HEAL * 2 )
		heal = Q_min( MEDKIT_HEAL * 0.5f, heal );
	int give = (int)Q_min( (float)ammo, ceilf( heal ));
	if( give <= 0 )
		return;

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	SendWeaponAnim( MEDKIT_SHORTUSE );
	pMon->TakeHealth( give, DMG_GENERIC );
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= give;
	int pitch = ammo < MEDKIT_HEAL * 2 ? (int)( ammo / ( MEDKIT_HEAL * 2.0f ) * 20.5f + 80.0f ) : 100;
	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "items/medshot4.wav", 1.0f, ATTN_NORM, 0, pitch );
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0f;
}

// the nearest fallen player within reach, still waiting (not respawned, not watching as a spectator); else the
// nearest fallen ally (a body that has finished falling, not a machine, not gibbed)
CBaseEntity *CMedkit::ReviveTarget( void )
{
	CBaseEntity *pBest = NULL;
	float best = MEDKIT_REVIVE_RADIUS;
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pDead = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !pDead || pDead == m_pPlayer || pDead->IsAlive() || FStringNull( pDead->pev->netname ) || pDead->pev->iuser1
			|| pDead->pev->deadflag < DEAD_DYING || ( pDead->pev->effects & EF_NODRAW ))
			continue;	// (EF_NODRAW: gibbed, nothing left to bring back)
		float d = ( pDead->pev->origin - m_pPlayer->pev->origin ).Length();
		if( d < best )
		{
			best = d;
			pBest = pDead;
		}
	}
	if( pBest )
		return pBest;
	CBaseEntity *pEnt = NULL;
	best = MEDKIT_REVIVE_RADIUS;
	while(( pEnt = UTIL_FindEntityInSphere( pEnt, m_pPlayer->pev->origin, MEDKIT_REVIVE_RADIUS )) != NULL )
	{
		CBaseMonster *pMon = pEnt->MyMonsterPointer();
		if( !pMon || pMon->IsPlayer() || pMon->pev->deadflag != DEAD_DEAD || ( pMon->pev->effects & EF_NODRAW )
			|| ( pMon->pev->flags & FL_KILLME ) || pMon->Classify() == CLASS_MACHINE || m_pPlayer->IRelationship( pMon ) > R_NO )
			continue;
		float d = ( pMon->pev->origin - m_pPlayer->pev->origin ).Length();
		if( d < best )
		{
			best = d;
			pBest = pMon;
		}
	}
	return pBest;
}

// a fallen ally up again where it lies: its own spawn again (model, size, full health, its AI from the start)
static void SC_ReviveMonster( CBaseMonster *pMon )
{
	Vector origin = pMon->pev->origin, yaw = Vector( 0, pMon->pev->angles.y, 0 );
	pMon->Forget( bits_MEMORY_KILLED );
	pMon->pev->velocity = g_vecZero;
	pMon->Spawn();
	pMon->pev->angles = yaw;
	UTIL_SetOrigin( pMon->pev, origin + Vector( 0, 0, 1 ));
}

void CMedkit::SecondaryAttack( void )
{
	Recharge();
	CBaseEntity *pDead = ReviveTarget();
	if( !pDead || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < MEDKIT_REVIVE_COST
		|| ( m_flReviveDone > 0.0f && (CBaseEntity *)m_hReviving != pDead ))
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "items/medshotno1.wav", 1.0f, ATTN_NORM );
		m_flNextSecondaryAttack = GetNextAttackDelay( 0.5f );
		m_flReviveDone = 0.0f;
		m_hReviving = NULL;
		return;
	}
	if( pDead->IsPlayer())
		g_flReviveHold[pDead->entindex()] = gpGlobals->time + 0.5f;	// they don't respawn under the medic's hands

	if( m_flReviveDone <= 0.0f )
	{
		m_hReviving = pDead;
		m_flReviveDone = gpGlobals->time + MEDKIT_REVIVE_TIME;
		SendWeaponAnim( MEDKIT_LONGUSE );
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "items/suitchargeok1.wav", 1.0f, ATTN_NORM );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + MEDKIT_REVIVE_TIME + 1.0f;
		return;
	}

	if( m_flNextMessage < gpGlobals->time )
	{
		m_flNextMessage = gpGlobals->time + 0.1f;
		char bar[40];
		int done = (int)( 30.0f * ( 1.0f - ( m_flReviveDone - gpGlobals->time ) / MEDKIT_REVIVE_TIME ));
		for( int i = 0; i < 30; i++ )
			bar[i] = i <= done ? '|' : '.';
		bar[30] = 0;
		char name[64];
		if( pDead->IsPlayer())
		{
			strncpy( name, STRING( pDead->pev->netname ), sizeof( name ) - 1 );
			name[sizeof( name ) - 1] = 0;
			ClientPrint( pDead->pev, HUD_PRINTCENTER, UTIL_VarArgs( "%s is reviving you\n[%s]", STRING( m_pPlayer->pev->netname ), bar ));
		}
		else
			SC_DisplayName( pDead, name, sizeof( name ));
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, UTIL_VarArgs( "Reviving %s\n[%s]", name, bar ));
	}

	if( m_flReviveDone > gpGlobals->time )
		return;
	m_flReviveDone = 0.0f;
	m_hReviving = NULL;
	ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "" );
	if( pDead->IsPlayer())
		ClientPrint( pDead->pev, HUD_PRINTCENTER, "" );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	SendWeaponAnim( MEDKIT_SHORTUSE );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/electro4.wav", 1.0f, ATTN_NORM );
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= MEDKIT_REVIVE_COST;
	m_flNextSecondaryAttack = GetNextAttackDelay( 2.0f );
	if( pDead->IsPlayer())
		SC_RevivePlayer( (CBasePlayer *)pDead, MEDKIT_REVIVE_HEALTH );
	else
		SC_ReviveMonster( pDead->MyMonsterPointer());
}

void CMedkit::CancelRevive( void )
{
	if( m_flReviveDone <= 0.0f )
		return;
	m_flReviveDone = 0.0f;
	ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "Revive cancelled" );
	CBaseEntity *pDead = m_hReviving;
	if( pDead && pDead->IsPlayer() && !pDead->IsAlive())
		ClientPrint( pDead->pev, HUD_PRINTCENTER, UTIL_VarArgs( "%s stopped reviving you", STRING( m_pPlayer->pev->netname )));
	m_hReviving = NULL;
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.5f );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "items/medshotno1.wav", 1.0f, ATTN_NORM );
}
