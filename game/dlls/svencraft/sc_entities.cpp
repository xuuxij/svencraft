/*
sc_entities.cpp - Svencraft map entities

sc_prop       a static studio model (Sven Co-op props: trees, rocks, barrels, ...); "solid" "2" makes it block
info_sc_rift  a place where the Minecraft block world breaks through the Sven world ("radius", "height")
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "sc_world.h"
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

class CSCRift : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void ) { pev->solid = SOLID_NOT; pev->movetype = MOVETYPE_NONE; }
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
