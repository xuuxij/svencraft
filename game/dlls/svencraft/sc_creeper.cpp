/*
sc_creeper.cpp - Svencraft: the creeper, the first creature from the block world

It walks up to its target (players and, unlike in its own world, the Half-Life monsters too), hops up single
blocks, hisses and swells for a second and a half when it gets close, and blows a blocky crater into whatever is
around: block world and realistic map geometry alike. Backing off in time makes it calm down again.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "sc_game.h"

#define CREEPER_MODEL		"models/svencraft/creeper.mdl"
#define CREEPER_FUSE_SOUND	"hunger/weapons/TNT/zip_flint_burn.wav"
#define CREEPER_HEALTH		100	// 20 in Minecraft, x5 like everything
#define CREEPER_SPEED		115.0f	// a bit slower than a running player
#define CREEPER_WANDER_SPEED	45.0f
#define CREEPER_SIGHT		1200
#define CREEPER_FUSE_TIME	1.5f	// seconds of hissing before it goes off
#define CREEPER_FUSE_RANGE	96.0f	// starts hissing this close (centre to centre)
#define CREEPER_CALM_RANGE	240.0f	// and calms down when the target gets this far away
#define CREEPER_BLAST_DAMAGE	215.0f	// at the centre: Minecraft's power-3 blast (43 health) x5, as 20 health there is 100 here
#define CREEPER_BLAST_RADIUS	240.0f	// six blocks, twice the blast power like back home
#define CREEPER_CRATER		3.0f	// crater radius in blocks
#define CREEPER_JUMP_SPEED	290.0f	// clears one block

enum
{
	CREEPER_IDLE = 0,
	CREEPER_CHASE,
	CREEPER_FUSE,
	CREEPER_DYING
};

class CCreeper : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_MINECRAFT; }
	void SetYawSpeed( void ) { pev->yaw_speed = 360; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void ) {}

	void EXPORT CreeperThink( void );
	void EXPORT DyingThink( void );

private:
	void SetAnim( const char *name, float rate );
	void FindTarget( void );
	bool Walk( const Vector &vecGoal, float flSpeed, float dt );
	void TurnTowards( const Vector &vecGoal, float dt );
	bool TryJump( const Vector &vecDir );
	void Fuse( float dt );
	void Explode( void );
	void UpdateLook( void );
	void TurnHead( float dt );

	int	m_iState;
	float	m_flLastThink;
	float	m_flFuse;		// 0..1
	bool	m_bHissing;
	float	m_flNextLook;
	float	m_flNextWander;
	float	m_flWanderUntil;
	Vector	m_vecWander;
	float	m_flStuckSince;
	Vector	m_vecStuckPos;
	float	m_flDetourUntil;
	float	m_flDetourYaw;
	float	m_flNextStep;
	float	m_flHurtUntil;
	int	m_iCurAnim;
	float	m_flHeadYaw;
	bool	m_bLanded;
	float	m_flNextKnock;
};

LINK_ENTITY_TO_CLASS( monster_creeper, CCreeper )

void CCreeper::Precache( void )
{
	PRECACHE_MODEL( CREEPER_MODEL );
	PRECACHE_SOUND( CREEPER_FUSE_SOUND );
	PRECACHE_SOUND( "debris/flesh1.wav" );
	PRECACHE_SOUND( "debris/flesh2.wav" );
	PRECACHE_SOUND( "debris/flesh3.wav" );
	PRECACHE_SOUND( "debris/bustflesh1.wav" );
	PRECACHE_SOUND( "common/npc_step1.wav" );
	PRECACHE_SOUND( "common/npc_step2.wav" );
	PRECACHE_SOUND( "common/npc_step3.wav" );
	PRECACHE_SOUND( "common/npc_step4.wav" );
	PRECACHE_SOUND( "weapons/explode3.wav" );
	PRECACHE_SOUND( "weapons/explode4.wav" );
	PRECACHE_SOUND( "weapons/explode5.wav" );
}

void CCreeper::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), CREEPER_MODEL );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 64 ));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->health = pev->max_health = CREEPER_HEALTH;
	pev->view_ofs = Vector( 0, 0, 56 );
	pev->deadflag = DEAD_NO;
	pev->yaw_speed = 360;
	pev->ideal_yaw = pev->angles.y;
	m_bloodColor = DONT_BLEED;
	m_flFieldOfView = -1.0f;	// sees all around
	m_MonsterState = MONSTERSTATE_ALERT;
	m_iCurAnim = -1;

	m_iState = CREEPER_IDLE;
	m_flLastThink = gpGlobals->time;
	m_flNextWander = gpGlobals->time + RANDOM_FLOAT( 1, 4 );
	SetAnim( "idle", 1.0f );

	// dropped to the floor on the first think: the block world is generated once all entities exist
	m_bLanded = false;
	SetThink( &CCreeper::CreeperThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.1f, 0.3f );
}

void CCreeper::SetAnim( const char *name, float rate )
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

void CCreeper::FindTarget( void )
{
	// keep a live, visible target; otherwise look around
	CBaseEntity *pEnemy = m_hEnemy;
	if( pEnemy && ( !pEnemy->IsAlive() || FBitSet( pEnemy->pev->flags, FL_NOTARGET ) || ( pEnemy->Center() - Center()).Length() > CREEPER_SIGHT * 1.5f ))
		m_hEnemy = NULL;

	if( gpGlobals->time < m_flNextLook )
		return;
	m_flNextLook = gpGlobals->time + 0.4f;

	Look( CREEPER_SIGHT );
	CBaseEntity *pBest = BestVisibleEnemy();
	// a closer target wins over the one being chased (unless that one is already in reach)
	if( pBest && pBest != m_hEnemy )
	{
		CBaseEntity *pCur = m_hEnemy;
		if( !pCur || ( pBest->Center() - Center()).Length() + 64 < ( pCur->Center() - Center()).Length())
		{
			m_hEnemy = pBest;
			ALERT( at_aiconsole, "creeper %d: target %s\n", entindex(), STRING( pBest->pev->classname ));
		}
	}
}

void CCreeper::TurnTowards( const Vector &vecGoal, float dt )
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
bool CCreeper::TryJump( const Vector &vecDir )
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
	pev->velocity = vecDir * 110 + Vector( 0, 0, CREEPER_JUMP_SPEED );
	ClearBits( pev->flags, FL_ONGROUND );
	return true;
}

// one step towards the goal; false when it got nowhere
bool CCreeper::Walk( const Vector &vecGoal, float flSpeed, float dt )
{
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

	TurnTowards( goal, dt );
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
	return false;
}

void CCreeper::UpdateLook( void )
{
	// skins: 0 normal, 1 white (hissing: flashes faster and faster), 2 red (hurt)
	if( gpGlobals->time < m_flHurtUntil )
		pev->skin = 2;
	else if( m_flFuse > 0 && fmod( gpGlobals->time * ( 2.5f + 8.0f * m_flFuse ), 1.0f ) < 0.5f )
		pev->skin = 1;
	else
		pev->skin = 0;

	// swelling (the client scales studio models by pev->scale)
	pev->scale = 1.0f + 0.22f * m_flFuse * m_flFuse;
}

// the head turns to stare at the target, or at a player standing close by
void CCreeper::TurnHead( float dt )
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
	want = Q_max( -80.0f, Q_min( 80.0f, want ));
	float step = 240.0f * dt;
	m_flHeadYaw += Q_max( -step, Q_min( step, want - m_flHeadYaw ));
	SetBoneController( 0, m_flHeadYaw );
}

void CCreeper::Fuse( float dt )
{
	CBaseEntity *pEnemy = m_hEnemy;
	bool close = false;
	if( pEnemy && pEnemy->IsAlive())
	{
		float d = ( pEnemy->Center() - Center()).Length();
		close = d < CREEPER_CALM_RANGE && FVisible( pEnemy );
		TurnTowards( pEnemy->pev->origin, dt );
	}

	if( close )
		m_flFuse += dt / CREEPER_FUSE_TIME;
	else
		m_flFuse -= dt / CREEPER_FUSE_TIME;

	if( m_flFuse >= 1.0f )
	{
		Explode();
		return;
	}
	if( m_flFuse <= 0.0f )
	{
		// calmed down
		m_flFuse = 0;
		m_iState = CREEPER_CHASE;
		if( m_bHissing )
		{
			STOP_SOUND( ENT( pev ), CHAN_VOICE, CREEPER_FUSE_SOUND );
			m_bHissing = false;
		}
	}
}

void CCreeper::CreeperThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1f;
	float dt = Q_min( gpGlobals->time - m_flLastThink, 0.25f );
	m_flLastThink = gpGlobals->time;
	StudioFrameAdvance();

	if( m_iState == CREEPER_DYING )
		return;
	if( !m_bLanded )
	{
		DROP_TO_FLOOR( ENT( pev ));
		m_bLanded = true;
	}

	FindTarget();
	TurnHead( dt );
	CBaseEntity *pEnemy = m_hEnemy;

	if( m_iState == CREEPER_FUSE )
	{
		Fuse( dt );
		if( m_iState == CREEPER_DYING )
			return;
		SetAnim( "fuse", 1.0f );
		UpdateLook();
		return;
	}

	bool moving = false;
	if( pEnemy )
	{
		m_iState = CREEPER_CHASE;
		float d = ( pEnemy->Center() - Center()).Length();
		if( d < CREEPER_FUSE_RANGE && FVisible( pEnemy ))
		{
			// close enough: stop and hiss
			m_iState = CREEPER_FUSE;
			ALERT( at_aiconsole, "creeper %d: hissing at %s (%.0f away)\n", entindex(), STRING( pEnemy->pev->classname ), d );
			m_flFuse = Q_max( m_flFuse, 0.01f );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, CREEPER_FUSE_SOUND, 1.0f, 0.6f, 0, 105 );
			m_bHissing = true;
			SetAnim( "fuse", 1.0f );
			UpdateLook();
			return;
		}

		moving = Walk( pEnemy->pev->origin, CREEPER_SPEED, dt );
		// stuck for a while: go round for a second
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
		moving = true;
	}
	else
	{
		m_iState = CREEPER_IDLE;
		// amble about now and then
		if( gpGlobals->time > m_flNextWander )
		{
			UTIL_MakeVectors( Vector( 0, RANDOM_FLOAT( 0, 360 ), 0 ));
			m_vecWander = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT( 80, 240 );
			m_flWanderUntil = gpGlobals->time + RANDOM_FLOAT( 2, 4 );
			m_flNextWander = m_flWanderUntil + RANDOM_FLOAT( 3, 8 );
		}
		if( gpGlobals->time < m_flWanderUntil && ( m_vecWander - pev->origin ).Length2D() > 16 )
		{
			if( !Walk( m_vecWander, CREEPER_WANDER_SPEED, dt ))
				m_flWanderUntil = 0;
			SetAnim( "walk", 0.45f );
			moving = true;
		}
		else
			SetAnim( "idle", 1.0f );
	}

	if( moving && FBitSet( pev->flags, FL_ONGROUND ) && gpGlobals->time > m_flNextStep )
	{
		static const char *steps[] = { "common/npc_step1.wav", "common/npc_step2.wav", "common/npc_step3.wav", "common/npc_step4.wav" };
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, steps[RANDOM_LONG( 0, 3 )], 0.35f, ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 10 ));
		m_flNextStep = gpGlobals->time + ( pEnemy ? 0.38f : 0.7f );
	}

	m_flFuse = 0;
	UpdateLook();
}

int CCreeper::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pev->takedamage == DAMAGE_NO || m_iState == CREEPER_DYING )
		return 0;

	// knocked back and flashing red, like back home; a good hit puts the hissing back a bit
	m_flHurtUntil = gpGlobals->time + 0.35f;
	if( m_flFuse > 0 )
		m_flFuse = Q_max( m_flFuse - 0.4f, 0.01f );
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
	static const char *hurt[] = { "debris/flesh1.wav", "debris/flesh2.wav", "debris/flesh3.wav" };
	EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, hurt[RANDOM_LONG( 0, 2 )], 0.9f, ATTN_NORM, 0, 120 + RANDOM_LONG( 0, 15 ));

	// whoever hit it is the new target
	CBaseEntity *pAttacker = pevAttacker ? CBaseEntity::Instance( pevAttacker ) : NULL;
	if( pAttacker && pAttacker != this && ( pAttacker->IsPlayer() || pAttacker->MyMonsterPointer()) && IRelationship( pAttacker ) > R_NO )
		m_hEnemy = pAttacker;

	UpdateLook();

	pev->health -= flDamage;
	ALERT( at_aiconsole, "creeper %d: hit for %.0f (type %x), health %.0f\n", entindex(), flDamage, bitsDamageType, pev->health );
	if( pev->health <= 0 )
	{
		Killed( pevAttacker, GIB_NORMAL );
		return 0;
	}
	return 1;
}

void CCreeper::Killed( entvars_t *pevAttacker, int iGib )
{
	if( m_bHissing )
	{
		STOP_SOUND( ENT( pev ), CHAN_VOICE, CREEPER_FUSE_SOUND );
		m_bHissing = false;
	}
	EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "debris/bustflesh1.wav", 0.8f, ATTN_NORM, 0, 130 );
	// like back home: 0-2 gunpowder (for the ammo it takes to fight the next one)
	int powder = RANDOM_LONG( 0, 2 );
	if( powder )
		SC_SpawnDrop( pev->origin + Vector( 0, 0, 24 ), SCITEM_GUNPOWDER, powder );
	m_iState = CREEPER_DYING;
	m_flFuse = 0;
	pev->deadflag = DEAD_DYING;
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;
	pev->scale = 1.0f;
	pev->skin = 2;
	SetAnim( "die", 1.0f );
	SetThink( &CCreeper::DyingThink );
	pev->nextthink = gpGlobals->time + 1.1f;
}

void CCreeper::DyingThink( void )
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

//
// the blast
//

// RadiusDamage with Minecraft's falloff: impact i = 1 - distance / radius hurts by (i^2 + i) / 2, so it is
// brutal up close and fades fast (Half-Life's is linear)
static void SC_BlastDamage( Vector vecSrc, entvars_t *pevInflictor, float flDamage, float flRadius )
{
	CBaseEntity *pEntity = NULL;
	TraceResult tr;
	vecSrc.z += 1.0f;
	while(( pEntity = UTIL_FindEntityInSphere( pEntity, vecSrc, flRadius )) != NULL )
	{
		if( pEntity->pev->takedamage == DAMAGE_NO )
			continue;
		Vector vecSpot = pEntity->BodyTarget( vecSrc );
		UTIL_TraceLine( vecSrc, vecSpot, dont_ignore_monsters, ENT( pevInflictor ), &tr );
		if( tr.flFraction < 1.0f && tr.pHit != pEntity->edict())
			continue;	// something shields it
		if( tr.fStartSolid )
			tr.vecEndPos = vecSrc;
		float i = 1.0f - ( vecSrc - tr.vecEndPos ).Length() / flRadius;
		if( i <= 0.0f )
			continue;
		float dmg = flDamage * ( i * i + i ) * 0.5f;
		ALERT( at_aiconsole, "blast: %s takes %.0f (health %.0f)\n", STRING( pEntity->pev->classname ), dmg, pEntity->pev->health );
		pEntity->TakeDamage( pevInflictor, pevInflictor, dmg, DMG_BLAST );	// no hitgroups: a blast hits all of you
	}
}

void CCreeper::Explode( void )
{
	ALERT( at_aiconsole, "creeper %d: boom at %.0f %.0f %.0f\n", entindex(), pev->origin.x, pev->origin.y, pev->origin.z );
	if( m_bHissing )
	{
		STOP_SOUND( ENT( pev ), CHAN_VOICE, CREEPER_FUSE_SOUND );
		m_bHissing = false;
	}
	m_iState = CREEPER_DYING;
	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	// a Half-Life fireball in a cloud of the block world's pale puffs, and a flash
	Vector c = pev->origin + Vector( 0, 0, 28 );
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( c.x );
		WRITE_COORD( c.y );
		WRITE_COORD( c.z + 16 );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( 32 );	// scale * 10
		WRITE_BYTE( 15 );	// framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();
	for( int i = 0; i < 9; i++ )
	{
		Vector p = c + Vector( RANDOM_FLOAT( -90, 90 ), RANDOM_FLOAT( -90, 90 ), RANDOM_FLOAT( -10, 70 ));
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, p );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( p.x );
			WRITE_COORD( p.y );
			WRITE_COORD( p.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG( 14, 26 ));	// scale * 10
			WRITE_BYTE( RANDOM_LONG( 120, 200 ));	// brightness
		MESSAGE_END();
	}
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, c );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( c.x );
		WRITE_COORD( c.y );
		WRITE_COORD( c.z );
		WRITE_BYTE( 45 );	// radius * 0.1
		WRITE_BYTE( 255 );
		WRITE_BYTE( 230 );
		WRITE_BYTE( 190 );
		WRITE_BYTE( 6 );	// life * 10
		WRITE_BYTE( 60 );	// decay * 0.1
	MESSAGE_END();

	SC_BlastDamage( c, pev, CREEPER_BLAST_DAMAGE, CREEPER_BLAST_RADIUS );
	SC_BlastProps( c, CREEPER_CRATER * VOX_BLOCK_SIZE + 24 );
	SC_Explosion( pev->origin + Vector( 0, 0, 16 ), 3.0f, 3 );	// Minecraft's creeper: power 3

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1f;
}
