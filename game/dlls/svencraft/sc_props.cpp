/*
sc_props.cpp - Svencraft: mining props and cars into materials

Studio props (sc_prop: trees, rocks, barrels, ...) and brush-built cars (func_wall "sc_car") take a few tool
hits and then break into the materials they are made of: cars give metal, rubber and glass, trees give
logs and leaves, rocks cobblestone. Studio props glow yellow to red as they weaken; cars show cracks.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "sc_game.h"
#include "decals.h"

extern const char *g_SCFamHit[FAM_COUNT];
extern const char *g_SCFamBreak[FAM_COUNT];

typedef struct
{
	const char	*keys;		// '|'-separated substrings of the model name (or "sc_car" for brush cars)
	int		mats[3];
	int		counts[3];
	float		hits;		// swings with the right tool at tier 1
	int		tool;
	int		family;
	qboolean	trunk;		// hit only near the trunk (trees: their bounds include the whole crown)
} scpropdef_t;

static const scpropdef_t g_SCPropDefs[] =
{
	{ "sc_car|car.mdl",			{ BLOCK_METAL, BLOCK_RUBBER, BLOCK_GLASS }, { 3, 1, 1 }, 8, TOOL_PICKAXE, FAM_METAL, false },
	{ "forklift",				{ BLOCK_METAL, BLOCK_RUBBER, 0 },          { 4, 2, 0 }, 10, TOOL_PICKAXE, FAM_METAL, false },
	{ "tree|oak|palm",			{ BLOCK_LOG, BLOCK_LEAVES, 0 },            { 4, 2, 0 }, 6, TOOL_AXE, FAM_WOOD, true },
	{ "bush|fern|plant",			{ BLOCK_LEAVES, 0, 0 },                     { 2, 0, 0 }, 1, TOOL_HAND, FAM_PLANT, false },
	{ "rock",				{ BLOCK_COBBLE, 0, 0 },                     { 3, 0, 0 }, 5, TOOL_PICKAXE, FAM_STONE, false },
	{ "barrel|drum",			{ BLOCK_METAL, 0, 0 },                      { 2, 0, 0 }, 4, TOOL_PICKAXE, FAM_METAL, false },
	{ "crate",				{ BLOCK_PLANKS, 0, 0 },                     { 2, 0, 0 }, 2, TOOL_AXE, FAM_WOOD, false },
	{ "trashcan",				{ BLOCK_METAL, 0, 0 },                      { 1, 0, 0 }, 3, TOOL_PICKAXE, FAM_METAL, false },
	{ "bench|chair|table",			{ BLOCK_PLANKS, 0, 0 },                     { 2, 0, 0 }, 3, TOOL_AXE, FAM_WOOD, false },
	{ "couch|sofa|bed",			{ BLOCK_PLANKS, BLOCK_RUBBER, 0 },          { 2, 1, 0 }, 4, TOOL_AXE, FAM_WOOD, false },
	{ "cabinet|locker",			{ BLOCK_METAL, 0, 0 },                      { 2, 0, 0 }, 4, TOOL_PICKAXE, FAM_METAL, false },
	{ "sink|toilet",			{ BLOCK_CONCRETE, 0, 0 },                   { 1, 0, 0 }, 3, TOOL_PICKAXE, FAM_STONE, false },
	{ "radio|fan|clock",			{ BLOCK_METAL, 0, 0 },                      { 1, 0, 0 }, 2, TOOL_PICKAXE, FAM_METAL, false },
	{ "pole",				{ BLOCK_LOG, 0, 0 },                        { 2, 0, 0 }, 6, TOOL_AXE, FAM_WOOD, true },
	{ "transformer|substation|elecswitch|gascan|battery|toolbox|tool_box|can_|picnic|flatscrn|securitycam|loudspeaker|pkgsgn|butts|binocs",
						{ BLOCK_METAL, 0, 0 },                      { 1, 0, 0 }, 3, TOOL_PICKAXE, FAM_METAL, false },
	{ "shelves|sawhorse",			{ BLOCK_PLANKS, 0, 0 },                     { 2, 0, 0 }, 3, TOOL_AXE, FAM_WOOD, false },
	{ "awning|stall|umbrella|wb_seat",	{ BLOCK_PLANKS, 0, 0 },                     { 1, 0, 0 }, 2, TOOL_AXE, FAM_WOOD, false },
	{ "barrier",				{ BLOCK_CONCRETE, 0, 0 },                   { 2, 0, 0 }, 5, TOOL_PICKAXE, FAM_STONE, false },
	{ "flower|shrub|cattail|mushroom|leafy|motherinlaw",	{ BLOCK_LEAVES, 0, 0 },     { 1, 0, 0 }, 1, TOOL_HAND, FAM_PLANT, false },
	{ "stones|vase",			{ BLOCK_COBBLE, 0, 0 },                     { 1, 0, 0 }, 3, TOOL_PICKAXE, FAM_STONE, false },
	{ "sandbag",				{ BLOCK_SAND, 0, 0 },                       { 3, 0, 0 }, 3, TOOL_SHOVEL, FAM_EARTH, false },
	{ "lamp|light",				{ BLOCK_GLASS, BLOCK_METAL, 0 },           { 1, 1, 0 }, 2, TOOL_PICKAXE, FAM_GLASS, false },
};
#define SC_NUM_PROPDEFS	( sizeof( g_SCPropDefs ) / sizeof( g_SCPropDefs[0] ))

static bool SC_KeyMatch( const char *keys, const char *name )
{
	char key[64];
	const char *p = keys;
	while( *p )
	{
		int n = 0;
		while( *p && *p != '|' && n < 63 ) key[n++] = *p++;
		key[n] = 0;
		if( *p == '|' ) p++;
		if( n && strstr( name, key ))
			return true;
	}
	return false;
}

static const scpropdef_t *SC_PropDef( CBaseEntity *pEnt )
{
	if( !pEnt )
		return NULL;
	const char *cls = STRING( pEnt->pev->classname );
	const char *name = NULL;
	if( !strcmp( cls, "sc_prop" ))
		name = STRING( pEnt->pev->model );
	else if( !strcmp( cls, "sc_car" ) || ( !strcmp( cls, "func_wall" ) && pEnt->pev->targetname && !strcmp( STRING( pEnt->pev->targetname ), "sc_car" )))
		name = "sc_car";
	if( !name )
		return NULL;
	for( unsigned i = 0; i < SC_NUM_PROPDEFS; i++ )
	{
		if( !SC_KeyMatch( g_SCPropDefs[i].keys, name ))
			continue;
		// scenery-sized models (whole rock formations) are part of the landscape, not something to mine
		Vector size = pEnt->pev->absmax - pEnt->pev->absmin;
		if( !g_SCPropDefs[i].trunk && pEnt->pev->solid != SOLID_BSP && ( size.x > 256 || size.y > 256 ))
			return NULL;
		return &g_SCPropDefs[i];
	}
	return NULL;
}

// a prop's hittable box: studio trees only near the trunk
static void SC_PropBox( CBaseEntity *pEnt, const scpropdef_t *def, Vector &mins, Vector &maxs )
{
	mins = pEnt->pev->absmin;
	maxs = pEnt->pev->absmax;
	if( def->trunk )
	{
		mins = pEnt->pev->origin + Vector( -20, -20, 0 );
		maxs = pEnt->pev->origin + Vector( 20, 20, 160 );
	}
	else if( maxs.x - mins.x < 4 )	// a studio model without bounds: a small box at its feet
	{
		mins = pEnt->pev->origin + Vector( -16, -16, 0 );
		maxs = pEnt->pev->origin + Vector( 16, 16, 48 );
	}
}

static bool SC_RayBox( const Vector &src, const Vector &dir, float maxdist, const Vector &mins, const Vector &maxs, float &t, Vector &normal )
{
	float tmin = 0, tmax = maxdist;
	int axis = -1;
	float sign = 0;
	for( int i = 0; i < 3; i++ )
	{
		if( fabs( dir[i] ) < 1e-6f )
		{
			if( src[i] < mins[i] || src[i] > maxs[i] )
				return false;
			continue;
		}
		float t1 = ( mins[i] - src[i] ) / dir[i], t2 = ( maxs[i] - src[i] ) / dir[i];
		float s = -1;
		if( t1 > t2 ) { float tt = t1; t1 = t2; t2 = tt; s = 1; }
		if( t1 > tmin ) { tmin = t1; axis = i; sign = s; }
		if( t2 < tmax ) tmax = t2;
		if( tmin > tmax )
			return false;
	}
	t = tmin;
	normal = g_vecZero;
	if( axis >= 0 )
		normal[axis] = sign;
	return true;
}

// the nearest minable prop along the line within maxdist (props that aren't solid included)
CBaseEntity *SC_PropTarget( const Vector &src, const Vector &dir, float maxdist, float &dist, Vector &normal )
{
	CBaseEntity *best = NULL;
	dist = maxdist;
	CBaseEntity *pEnt = NULL;
	while(( pEnt = UTIL_FindEntityInSphere( pEnt, src, maxdist + 256.0f )) != NULL )
	{
		const scpropdef_t *def = SC_PropDef( pEnt );
		if( !def )
			continue;
		Vector mins, maxs, n;
		float t;
		SC_PropBox( pEnt, def, mins, maxs );
		if( SC_RayBox( src, dir, dist, mins, maxs, t, n ) && t < dist )
		{
			dist = t;
			normal = n;
			best = pEnt;
		}
	}
	return best;
}

void SC_HitProp( CBasePlayer *pPlayer, int tool, CBaseEntity *pEnt, const Vector &vecHit, const Vector &vecNormal )
{
	const scpropdef_t *def = SC_PropDef( pEnt );
	if( !def )
		return;

	EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_ITEM, g_SCFamHit[def->family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));

	// progress: the right tool (better tiers faster) or slow by hand
	float speed = 1.0f;
	if( tool == def->tool || def->tool == TOOL_HAND )
		speed = 1.0f + SC_HeldTier( pPlayer, tool ) * 0.5f;
	float step = speed / ( Q_max( def->hits, 1.0f ) * 1.5f );
	pEnt->pev->fuser4 += step;
	float p = pEnt->pev->fuser4;

	if( p < 0.999f )
	{
		if( pEnt->IsBSPModel())
		{
			// brush props (cars): dents and holes where the tool lands, sparks off the metal
			Vector src = pPlayer->GetGunPosition();
			Vector dir = ( vecHit - src ).Normalize();
			TraceResult tr;
			UTIL_TraceLine( src, vecHit + dir * 48, dont_ignore_monsters, ENT( pPlayer->pev ), &tr );
			if( tr.flFraction < 1.0f && tr.pHit == pEnt->edict())
			{
				const char *tex = TRACE_TEXTURE( tr.pHit, src, vecHit + dir * 48 );
				bool glass = tex && strstr( tex, "window" );
				UTIL_DecalTrace( &tr, glass ? DECAL_GLASSBREAK1 + RANDOM_LONG( 0, 2 ) : DECAL_BIGSHOT1 + RANDOM_LONG( 0, 4 ));
				if( !glass )
					UTIL_Sparks( tr.vecEndPos );
				SC_Debris( tr.vecEndPos + tr.vecPlaneNormal * 4, glass ? FAM_GLASS : def->family, 1 );
			}
		}
		else
		{
			// glow shell: yellow and thin when scratched, red and thick when about to break
			pEnt->pev->renderfx = kRenderFxGlowShell;
			pEnt->pev->rendercolor = Vector( 255, 230 - 200 * p, 40 - 40 * p );
			pEnt->pev->renderamt = 4 + (int)( p * 14 );
		}
		return;
	}

	SC_HideCrack( pPlayer );
	SC_BreakProp( pEnt, pPlayer, tool );
	SC_WearHeld( pPlayer, 1 );
}

// breaks a prop into its materials; without a player (explosions) everything comes out
void SC_BreakProp( CBaseEntity *pEnt, CBasePlayer *pPlayer, int tool )
{
	const scpropdef_t *def = SC_PropDef( pEnt );
	if( !def )
		return;
	Vector center = ( pEnt->pev->absmin + pEnt->pev->absmax ) * 0.5f;
	if( def->trunk )
		center = pEnt->pev->origin + Vector( 0, 0, 48 );
	EMIT_AMBIENT_SOUND( ENT( pEnt->pev ), center, g_SCFamBreak[def->family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));
	SC_Debris( center, def->family, 6 );
	for( int i = 0; i < 3; i++ )
		if( def->mats[i] == BLOCK_GLASS && def->family != FAM_GLASS )
			SC_Debris( center + Vector( 0, 0, 16 ), FAM_GLASS, 4 );
	for( int i = 0; i < 3; i++ )
	{
		int m = def->mats[i];
		if( m <= BLOCK_AIR || def->counts[i] <= 0 )
			continue;
		if( pPlayer && !SC_CanHarvest( pPlayer, tool, m ))
		{
			char msg[96];
			snprintf( msg, sizeof( msg ), "%s needs a %s pickaxe", g_SCBlocks[m].title, g_SCTierNames[Q_min( g_SCBlocks[m].level, SC_MAX_TIER )] );
			ClientPrint( pPlayer->pev, HUD_PRINTCENTER, msg );
			continue;
		}
		for( int k = 0; k < def->counts[i]; k++ )
			SC_SpawnDrop( center + Vector( RANDOM_FLOAT( -8, 8 ), RANDOM_FLOAT( -8, 8 ), 0 ), m, 1 );
	}
	UTIL_Remove( pEnt );
}

// a blast breaks the props around it (cars included)
void SC_BlastProps( const Vector &center, float radius )
{
	CBaseEntity *pList[64];
	int n = 0;
	CBaseEntity *pEnt = NULL;
	while(( pEnt = UTIL_FindEntityInSphere( pEnt, center, radius + 128 )) != NULL && n < 64 )
	{
		if( !SC_PropDef( pEnt ) || FClassnameIs( pEnt->pev, "sc_car" ))
			continue;	// (cars take the blast's damage instead: sc_car.cpp)
		// the nearest point of its box
		Vector p;
		for( int i = 0; i < 3; i++ )
			p[i] = Q_max( pEnt->pev->absmin[i], Q_min( center[i], pEnt->pev->absmax[i] ));
		if(( p - center ).Length() <= radius )
			pList[n++] = pEnt;
	}
	for( int i = 0; i < n; i++ )
		SC_BreakProp( pList[i], NULL, TOOL_HAND );
}
