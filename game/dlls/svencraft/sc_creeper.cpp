/*
sc_creeper.cpp - Svencraft: the creeper, the first creature from the block world

It walks up to its target (players and, unlike in its own world, the Half-Life monsters too), hops up single
blocks, hisses and swells for a second and a half when it gets close, and blows a blocky crater into whatever is
around: block world and realistic map geometry alike. Backing off in time makes it calm down again.
(Walking, senses, getting hurt and dying: CSCMob, sc_mob.cpp.)
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "sc_game.h"
#include "sc_mob.h"

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

class CCreeper : public CSCMob
{
public:
	void Spawn( void );
	void Precache( void );

protected:
	bool Behave( float dt );
	void Drops( void );
	void OnHurt( float flDamage, int bitsDamageType );
	void OnKilled( void ) { StopHiss(); m_flFuse = 0; }
	void UpdateSkin( void );

private:
	void Fuse( float dt );
	void Explode( void );
	void StopHiss( void );

	bool	m_bFusing;
	float	m_flFuse;		// 0..1
	bool	m_bHissing;
};

LINK_ENTITY_TO_CLASS( monster_creeper, CCreeper )

void CCreeper::Precache( void )
{
	MobPrecache();
	PRECACHE_MODEL( CREEPER_MODEL );
	PRECACHE_SOUND( CREEPER_FUSE_SOUND );
	PRECACHE_SOUND( "weapons/explode3.wav" );
	PRECACHE_SOUND( "weapons/explode4.wav" );
	PRECACHE_SOUND( "weapons/explode5.wav" );
}

void CCreeper::Spawn( void )
{
	Precache();
	MobSpawn( CREEPER_MODEL, Vector( -12, -12, 0 ), Vector( 12, 12, 64 ), CREEPER_HEALTH, 56 );
	m_iHurtSkin = 2;	// (1 is the white flash of the fuse)
	m_bFusing = false;
}

void CCreeper::UpdateSkin( void )
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

void CCreeper::StopHiss( void )
{
	if( m_bHissing )
	{
		STOP_SOUND( ENT( pev ), CHAN_VOICE, CREEPER_FUSE_SOUND );
		m_bHissing = false;
	}
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
		m_bFusing = false;
		StopHiss();
	}
}

bool CCreeper::Behave( float dt )
{
	FindTarget( CREEPER_SIGHT );
	TurnHead( dt );
	CBaseEntity *pEnemy = m_hEnemy;

	if( m_bFusing )
	{
		Fuse( dt );
		if( !m_bDying )
			SetAnim( "fuse", 1.0f );
		return false;
	}

	if( pEnemy )
	{
		float d = ( pEnemy->Center() - Center()).Length();
		if( d < CREEPER_FUSE_RANGE && FVisible( pEnemy ))
		{
			// close enough: stop and hiss
			m_bFusing = true;
			ALERT( at_aiconsole, "creeper %d: hissing at %s (%.0f away)\n", entindex(), STRING( pEnemy->pev->classname ), d );
			m_flFuse = Q_max( m_flFuse, 0.01f );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, CREEPER_FUSE_SOUND, 1.0f, 0.6f, 0, 105 );
			m_bHissing = true;
			SetAnim( "fuse", 1.0f );
			return false;
		}
		m_flFuse = 0;
		return Chase( pEnemy, CREEPER_SPEED, dt );
	}
	m_flFuse = 0;
	return Wander( CREEPER_WANDER_SPEED, dt );
}

void CCreeper::OnHurt( float flDamage, int bitsDamageType )
{
	// a good hit puts the hissing back a bit
	if( m_flFuse > 0 )
		m_flFuse = Q_max( m_flFuse - 0.4f, 0.01f );
}

void CCreeper::Drops( void )
{
	// like back home: 0-2 gunpowder (for the ammo it takes to fight the next one)
	int powder = RANDOM_LONG( 0, 2 );
	if( powder )
		SC_SpawnDrop( pev->origin + Vector( 0, 0, 24 ), SCITEM_GUNPOWDER, powder );
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
	StopHiss();
	m_bDying = true;
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
