/*
sc_entities.cpp - Svencraft map entities

sc_prop       a static studio model (Sven Co-op props: trees, rocks, barrels, ...); "solid" "2" makes it block
info_sc_rift  a place where the Minecraft block world breaks through the Sven world ("radius", "height"); its
              creatures come out of it (sc_rift_mobs, sc_rift_interval)
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "sc_world.h"
#include "sc_game.h"
#include "animation.h"

class CSCProp : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	int m_iSolid;
};

LINK_ENTITY_TO_CLASS( sc_prop, CSCProp )

void CSCProp::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "solid" ))
	{
		m_iSolid = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CSCProp::Precache( void )
{
	PRECACHE_MODEL( STRING( pev->model ));
}

void CSCProp::Spawn( void )
{
	Precache();
	SET_MODEL( ENT( pev ), STRING( pev->model ));
	pev->movetype = MOVETYPE_NONE;
	pev->solid = m_iSolid == 2 ? SOLID_BBOX : SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->sequence = 0;
	pev->frame = 0;
	pev->framerate = 0;
	// size from the first sequence's box turned by the yaw (tools aim at it; solid props collide with it)
	float bmin[3], bmax[3];
	void *pmodel = GET_MODEL_PTR( ENT( pev ));
	if( pmodel && ExtractBbox( pmodel, 0, bmin, bmax ))
	{
		float yaw = pev->angles.y * ( M_PI / 180.0f ), c = fabs( cos( yaw )), s = fabs( sin( yaw ));
		float cx = ( bmin[0] + bmax[0] ) * 0.5f, cy = ( bmin[1] + bmax[1] ) * 0.5f;
		float ex = ( bmax[0] - bmin[0] ) * 0.5f, ey = ( bmax[1] - bmin[1] ) * 0.5f;
		float rx = cx * cos( yaw ) - cy * sin( yaw ), ry = cx * sin( yaw ) + cy * cos( yaw );
		float hx = ex * c + ey * s, hy = ex * s + ey * c;
		UTIL_SetSize( pev, Vector( rx - hx, ry - hy, bmin[2] ), Vector( rx + hx, ry + hy, bmax[2] ));
	}
	UTIL_SetOrigin( pev, pev->origin );
}

// the block world's creatures come through: one every sc_rift_interval seconds while fewer than sc_rift_mobs of them
// are about the rift (0: none), Minecraft's night mix of zombies, skeletons, spiders and creepers, out on the mound
static cvar_t sc_rift_mobs = { "sc_rift_mobs", "4", FCVAR_SERVER };
static cvar_t sc_rift_interval = { "sc_rift_interval", "25", FCVAR_SERVER };

void SC_RegisterRiftCvars( void )
{
	CVAR_REGISTER( &sc_rift_mobs );
	CVAR_REGISTER( &sc_rift_interval );
}

class CSCRift : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void )
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		SetThink( &CSCRift::RiftThink );
		pev->nextthink = gpGlobals->time + 15.0f;	// (the block world exists by then)
	}
	void EXPORT RiftThink( void );
};

LINK_ENTITY_TO_CLASS( info_sc_rift, CSCRift )

void CSCRift::KeyValue( KeyValueData *pkvd )
{
	// stored in spare fields: fuser1 = radius, fuser2 = mound height (world units)
	if( FStrEq( pkvd->szKeyName, "radius" ))
	{
		pev->fuser1 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "height" ))
	{
		pev->fuser2 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CSCRift::RiftThink( void )
{
	pev->nextthink = gpGlobals->time + Q_max( 3.0f, sc_rift_interval.value );
	int cap = (int)sc_rift_mobs.value;
	if( cap <= 0 )
		return;
	float radius = pev->fuser1 > 0 ? pev->fuser1 : 256;

	// how many of them are about already
	int n = 0;
	CBaseEntity *pOther = NULL;
	while(( pOther = UTIL_FindEntityInSphere( pOther, pev->origin, radius * 4 )) != NULL )
		if(( pOther->pev->flags & FL_MONSTER ) && pOther->IsAlive() && pOther->Classify() == CLASS_MINECRAFT )
			n++;
	if( n >= cap )
		return;

	static const struct { const char *cls; int weight; } kinds[] =
	{
		{ "monster_sc_zombie", 35 }, { "monster_sc_skeleton", 25 }, { "monster_sc_spider", 25 }, { "monster_creeper", 15 }
	};
	int r = RANDOM_LONG( 0, 99 ), k = 0;
	for( ; k < (int)ARRAYSIZE( kinds ) - 1 && r >= kinds[k].weight; k++ )
		r -= kinds[k].weight;

	// on the mound, clear of the shaft in its middle and under the floating blocks
	for( int tries = 0; tries < 6; tries++ )
	{
		float a = RANDOM_FLOAT( 0, 2 * M_PI ), d = RANDOM_FLOAT( 100, Q_max( 120.0f, radius * 0.6f ));
		Vector from = pev->origin + Vector( cos( a ) * d, sin( a ) * d, pev->fuser2 + 60 );
		TraceResult tr;
		UTIL_TraceLine( from, from - Vector( 0, 0, 600 ), ignore_monsters, NULL, &tr );
		if( tr.flFraction >= 1.0f || tr.fStartSolid || tr.fAllSolid )
			continue;
		CBaseEntity *pMob = CBaseEntity::Create( kinds[k].cls, tr.vecEndPos + Vector( 0, 0, 4 ), Vector( 0, RANDOM_FLOAT( 0, 360 ), 0 ), NULL );
		if( !pMob )
			return;
		if( !SC_PlaceCreature( pMob, tr.vecEndPos, NULL ))
		{
			UTIL_Remove( pMob );
			continue;
		}
		Vector c = pMob->pev->origin + Vector( 0, 0, 16 );
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, c );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( c.x );
			WRITE_COORD( c.y );
			WRITE_COORD( c.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 14 );	// scale * 10
			WRITE_BYTE( 16 );	// framerate
		MESSAGE_END();
		ALERT( at_aiconsole, "rift: a %s comes through (%d about)\n", kinds[k].cls, n + 1 );
		return;
	}
}
