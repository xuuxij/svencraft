/*
sc_tools.cpp - Svencraft: pickaxe, shovel, axe and the bare hand

Left click swings: blocks get mined, creatures get hit like with the crowbar. Right click opens a crafting table or
furnace, or places the block in hand. The hotbar slot decides which of these is out (sc_hotbar.cpp); the tool's
model follows the held tool's tier (wood / stone / iron), the hand's holds the slot's block or item (drawn by the
client from the inventory, cl_dll/svencraft/hud_hotbar.cpp).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "sc_game.h"

enum sctool_anim_e	// same sequences as the crowbar the tool models were made from
{
	SCTOOL_IDLE = 0,
	SCTOOL_DRAW,
	SCTOOL_HOLSTER,
	SCTOOL_ATTACK1HIT,
	SCTOOL_ATTACK1MISS,
	SCTOOL_ATTACK2MISS,
	SCTOOL_ATTACK2HIT,
	SCTOOL_ATTACK3MISS,
	SCTOOL_ATTACK3HIT
};

// the crowbar's three swings, each with a hit and a miss version
static const int g_SCHitAnims[3] = { SCTOOL_ATTACK1HIT, SCTOOL_ATTACK2HIT, SCTOOL_ATTACK3HIT };
static const int g_SCMissAnims[3] = { SCTOOL_ATTACK1MISS, SCTOOL_ATTACK2MISS, SCTOOL_ATTACK3MISS };

static const char *g_SCToolModel[TOOL_COUNT] = { "", "scpickaxe", "scshovel", "scaxe" };

class CSCTool : public CBasePlayerWeapon
{
public:
	virtual int ToolType( void ) = 0;
	virtual int WeaponId( void ) = 0;

	void Spawn( void );
	void Precache( void );
	int GetItemInfo( ItemInfo *p );
	int AddToPlayer( CBasePlayer *pPlayer );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void PrimaryAttack( void );
	int Strike( bool apply );
	void EXPORT Impact( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );
	BOOL UseDecrement( void ) { return FALSE; }

	const char *Model( char kind, int tier );
	int m_iSwing;
};

#define SC_HAND_MODEL	"models/svencraft/v_schand.mdl"

const char *CSCTool::Model( char kind, int tier )
{
	static char buf[4][64];
	static int n;
	if( ToolType() == TOOL_HAND )
		return SC_HAND_MODEL;	// only a view model (the world model is never seen: the hand isn't dropped)
	n = ( n + 1 ) & 3;
	snprintf( buf[n], sizeof( buf[n] ), "models/svencraft/%c_%s%d.mdl", kind, g_SCToolModel[ToolType()], Q_min( Q_max( tier, 1 ), SC_MAX_TIER ));
	return buf[n];
}

void CSCTool::Spawn( void )
{
	Precache();
	m_iId = WeaponId();
	SET_MODEL( ENT( pev ), Model( 'w', 1 ));
	m_iClip = -1;
	FallInit();
}

void CSCTool::Precache( void )
{
	PRECACHE_MODEL( SC_HAND_MODEL );
	for( int t = 1; t <= SC_MAX_TIER; t++ )
	{
		PRECACHE_MODEL( (char *)Model( 'v', t ));
		PRECACHE_MODEL( (char *)Model( 'p', t ));
		PRECACHE_MODEL( (char *)Model( 'w', t ));
	}
	PRECACHE_SOUND( "weapons/cbar_hitbod1.wav" );
	PRECACHE_SOUND( "weapons/cbar_hitbod2.wav" );
	PRECACHE_SOUND( "weapons/cbar_hit1.wav" );
	PRECACHE_SOUND( "weapons/cbar_miss1.wav" );
	SC_Precache();
}

int CSCTool::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = WeaponId() - WEAPON_SC_PICKAXE + 4;	// after the crowbar and Opposing Force's wrench, knife, grapple
	p->iId = WeaponId();
	p->iWeight = 0;
	return 1;
}

int CSCTool::AddToPlayer( CBasePlayer *pPlayer )
{
	return CBasePlayerWeapon::AddToPlayer( pPlayer );	// no pickup icon: the hotbar shows what is held
}

BOOL CSCTool::Deploy( void )
{
	int tier = SC_HeldTier( m_pPlayer, ToolType());
	if( ToolType() == TOOL_HAND )
	{
		BOOL ok = DefaultDeploy( (char *)SC_HAND_MODEL, (char *)"", SCTOOL_DRAW, "crowbar" );
		m_pPlayer->pev->weaponmodel = 0;	// nothing in the hand to show from outside (yet)
		return ok;
	}
	return DefaultDeploy( (char *)Model( 'v', tier ), (char *)Model( 'p', tier ), SCTOOL_DRAW, "crowbar" );
}

void CSCTool::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;
	SendWeaponAnim( SCTOOL_HOLSTER );
	SC_HideCrack( m_pPlayer );
}

// A swing: the animation starts now (hit or miss version, from what is under the crosshair), and the
// blow lands at the animation's impact frame, with a fresh look at what is there.
#define SC_IMPACT_DELAY	0.18f	// when the tool head lands in the swing

void CSCTool::PrimaryAttack( void )
{
	int target = Strike( false );
	m_iSwing++;
	SendWeaponAnim( target ? g_SCHitAnims[m_iSwing % 3] : g_SCMissAnims[m_iSwing % 3] );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	if( !target )
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 15 ));

	SetThink( &CSCTool::Impact );
	pev->nextthink = gpGlobals->time + SC_IMPACT_DELAY;
	m_flNextPrimaryAttack = gpGlobals->time + SC_SWING_TIME;
	m_flTimeWeaponIdle = gpGlobals->time + 2.0f;
}

void CSCTool::Impact( void )
{
	SetThink( NULL );
	if( m_pPlayer && m_pPlayer->m_pActiveItem == this && m_pPlayer->IsAlive())
		Strike( true );
}

// what the swing hits: 0 = nothing; with apply, the hit happens (mining, digging, damage)
int CSCTool::Strike( bool apply )
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecDir = gpGlobals->v_forward;
	Vector vecEnd = vecSrc + vecDir * SC_REACH;

	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	// a torch in the way (not solid: the engine's trace went through it)
	int tcell[3];
	if( SC_TorchTarget( vecSrc, vecDir, tr.flFraction * SC_REACH, tcell ))
	{
		if( apply )
			SC_BreakTorch( m_pPlayer, tcell );
		return 1;
	}

	// a block, if it is the first thing along the line (engine traces include blocks too)
	vox_api_t *v = SC_Vox();
	float vfrac = 1.0f;
	int cell[3];
	float normal[3];
	int block = v ? v->TraceLine( vecSrc, vecEnd, &vfrac, cell, normal ) : 0;
	if( block && vfrac <= tr.flFraction + 0.001f )
	{
		if( apply )
			SC_HitBlock( m_pPlayer, ToolType(), cell, normal );
		return 1;
	}

	// props and cars (trees aren't solid, so look for them along the line too)
	float pdist;
	Vector pnormal;
	CBaseEntity *prop = SC_PropTarget( vecSrc, vecDir, SC_REACH, pdist, pnormal );
	if( prop && pdist <= tr.flFraction * SC_REACH + 2.0f )
	{
		if( apply )
			SC_HitProp( m_pPlayer, ToolType(), prop, vecSrc + vecDir * pdist, pnormal );
		return 1;
	}

	// the realistic ground and walls
	int ground;
	float gfrac = 1.0f, gnormal[3];
	if( tr.flFraction < 1.0f && SC_Dyn() && tr.pHit == INDEXENT( 0 )
		&& ( ground = SC_Dyn()->TraceLine( vecSrc, vecEnd, &gfrac, gnormal )) > 0 && fabs( gfrac - tr.flFraction ) < 0.01f )
	{
		if( apply )
			SC_HitGround( m_pPlayer, ToolType(), ground, tr.vecEndPos, Vector( gnormal[0], gnormal[1], gnormal[2] ));
		return 1;
	}

	// creatures and breakables: hit like the crowbar, with a Minecraft-ish reach (a creeper hisses from further)
	CBaseEntity *pHit = tr.flFraction < 1.0f ? CBaseEntity::Instance( tr.pHit ) : NULL;
	float dist = ( tr.vecEndPos - vecSrc ).Length();
	if( !pHit || dist > 96.0f )
		return 0;
	if( !apply )
		return 1;
	// Minecraft's tool damage (by tier: none, wood, stone, iron, diamond) x5, as 20 health there is 100 here
	static const float dmg[TOOL_COUNT][SC_MAX_TIER + 1] =
	{
		{ 5, 5, 5, 5, 5 },		// hand
		{ 5, 10, 15, 20, 25 },	// pickaxe
		{ 5, 12.5f, 17.5f, 22.5f, 27.5f },	// shovel
		{ 5, 35, 45, 45, 45 },	// axe
	};
	int tier = Q_min( SC_HeldTier( m_pPlayer, ToolType()), SC_MAX_TIER );
	float damage = dmg[ToolType()][tier];
	// Minecraft's critical hit: coming down from a jump, half as hard again, with a burst of sparks
	if( !FBitSet( m_pPlayer->pev->flags, FL_ONGROUND ) && m_pPlayer->pev->velocity.z < 0 && m_pPlayer->pev->waterlevel < 2
		&& !m_pPlayer->IsOnLadder())
	{
		damage *= 1.5f;
		UTIL_Sparks( tr.vecEndPos );
	}
	ClearMultiDamage();
	pHit->TraceAttack( m_pPlayer->pev, damage, vecDir, &tr, DMG_CLUB );
	ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
	if( pHit->IsAlive() || pHit->pev->takedamage != DAMAGE_NO )
		SC_WearHeld( m_pPlayer, 2 );	// Minecraft: a tool used as a weapon wears twice as fast
	if( pHit->Classify() != CLASS_NONE && pHit->Classify() != CLASS_MACHINE )
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, RANDOM_LONG( 0, 1 ) ? "weapons/cbar_hitbod1.wav" : "weapons/cbar_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	else
	{
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/cbar_hit1.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ));
		DecalGunshot( &tr, BULLET_PLAYER_CROWBAR );
	}
	return 1;
}

void CSCTool::SecondaryAttack( void )
{
	m_flNextSecondaryAttack = gpGlobals->time + 0.25f;
	if( SC_PlayerUse( m_pPlayer ))	// right-click on a crafting table or furnace opens it, like Minecraft
	{
		m_flNextSecondaryAttack = gpGlobals->time + 0.5f;
		return;
	}
	if( SC_EatHeld( m_pPlayer ))
	{
		m_flNextSecondaryAttack = gpGlobals->time + 1.6f;	// Minecraft's eating time
		return;
	}
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	if( SC_PlaceBlock( m_pPlayer, m_pPlayer->GetGunPosition(), gpGlobals->v_forward ))
	{
		// a quick swing, like Minecraft's place animation
		m_iSwing++;
		SendWeaponAnim( g_SCMissAnims[m_iSwing % 3] );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_flTimeWeaponIdle = gpGlobals->time + 1.0f;
	}
	m_flNextSecondaryAttack = gpGlobals->time + 0.25f;
}

void CSCTool::WeaponIdle( void )
{
	SCInventory &inv = SC_Inv( m_pPlayer );
	if( inv.lastHit > 0 && gpGlobals->time - inv.lastHit > 1.5f )
	{
		SC_HideCrack( m_pPlayer );
		inv.lastHit = 0;
	}
	if( m_flTimeWeaponIdle > gpGlobals->time )
		return;
	SendWeaponAnim( SCTOOL_IDLE );
	m_flTimeWeaponIdle = gpGlobals->time + 10.0f;
}

class CSCPickaxe : public CSCTool
{
public:
	int ToolType( void ) { return TOOL_PICKAXE; }
	int WeaponId( void ) { return WEAPON_SC_PICKAXE; }
};
class CSCShovel : public CSCTool
{
public:
	int ToolType( void ) { return TOOL_SHOVEL; }
	int WeaponId( void ) { return WEAPON_SC_SHOVEL; }
};
class CSCAxe : public CSCTool
{
public:
	int ToolType( void ) { return TOOL_AXE; }
	int WeaponId( void ) { return WEAPON_SC_AXE; }
};
class CSCHand : public CSCTool
{
public:
	int ToolType( void ) { return TOOL_HAND; }
	int WeaponId( void ) { return WEAPON_SC_HAND; }
};

LINK_ENTITY_TO_CLASS( weapon_sc_pickaxe, CSCPickaxe )
LINK_ENTITY_TO_CLASS( weapon_sc_shovel, CSCShovel )
LINK_ENTITY_TO_CLASS( weapon_sc_axe, CSCAxe )
LINK_ENTITY_TO_CLASS( weapon_sc_hand, CSCHand )
