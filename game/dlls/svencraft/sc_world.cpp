/*
sc_world.cpp - Svencraft: builds the Minecraft-style block world under svencraft maps

The engine owns the blocks (collision and rendering); this file decides what goes where: stone with
noise caverns and winding tunnels, ores by depth, bedrock at the bottom, and (until the realistic surface
exists) a grass/dirt top so the test map has ground.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "sc_world.h"
#include "sc_game.h"
#include "monsters.h"

#if XASH_WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// world bounds in blocks (VOX_BLOCK_SIZE units each); maxs exclusive
#define SC_WORLD_MINX	-48
#define SC_WORLD_MAXX	48
#define SC_WORLD_MINY	-48
#define SC_WORLD_MAXY	48
#define SC_WORLD_MINZ	-24
#define SC_WORLD_MAXZ	16

static vox_api_t *g_pVox;
static bool g_bVoxLookedUp;

vox_api_t *SC_Vox( void )
{
	if( g_bVoxLookedUp )
		return g_pVox;
	g_bVoxLookedUp = true;

	pfnVox_GetAPI pfnGet = NULL;
#if XASH_WIN32
	HMODULE hEngine = GetModuleHandleA( "xash.dll" );
	if( hEngine )
		pfnGet = (pfnVox_GetAPI)GetProcAddress( hEngine, VOX_GETAPI_NAME );
#else
	pfnGet = (pfnVox_GetAPI)dlsym( RTLD_DEFAULT, VOX_GETAPI_NAME );
#endif
	if( pfnGet )
		g_pVox = pfnGet( VOX_API_VERSION );
	if( !g_pVox )
		ALERT( at_console, "Svencraft: this engine has no block world\n" );
	return g_pVox;
}

//
// noise
//
static unsigned int SC_Hash( int x, int y, int z, int seed )
{
	unsigned int h = (unsigned int)( x * 374761393 + y * 668265263 + z * 2147483647 + seed * 144665 );
	h = ( h ^ ( h >> 13 )) * 1274126177u;
	return h ^ ( h >> 16 );
}

static float SC_Lattice( int x, int y, int z, int seed )
{
	return ( SC_Hash( x, y, z, seed ) & 0xFFFF ) / 32767.5f - 1.0f;
}

static float SC_Smooth( float t )
{
	return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
}

float SC_Noise3( float x, float y, float z, int seed )
{
	int x0 = (int)floor( x ), y0 = (int)floor( y ), z0 = (int)floor( z );
	float fx = SC_Smooth( x - x0 ), fy = SC_Smooth( y - y0 ), fz = SC_Smooth( z - z0 );
	float c[2][2][2];

	for( int i = 0; i < 2; i++ )
		for( int j = 0; j < 2; j++ )
			for( int k = 0; k < 2; k++ )
				c[i][j][k] = SC_Lattice( x0 + i, y0 + j, z0 + k, seed );

	float x00 = c[0][0][0] + ( c[1][0][0] - c[0][0][0] ) * fx;
	float x10 = c[0][1][0] + ( c[1][1][0] - c[0][1][0] ) * fx;
	float x01 = c[0][0][1] + ( c[1][0][1] - c[0][0][1] ) * fx;
	float x11 = c[0][1][1] + ( c[1][1][1] - c[0][1][1] ) * fx;
	float y0v = x00 + ( x10 - x00 ) * fy;
	float y1v = x01 + ( x11 - x01 ) * fy;
	return y0v + ( y1v - y0v ) * fz;
}

static float SC_Fbm( float x, float y, float z, int seed, int octaves )
{
	float sum = 0, amp = 1, norm = 0;
	for( int o = 0; o < octaves; o++ )
	{
		sum += SC_Noise3( x, y, z, seed + o * 1013 ) * amp;
		norm += amp;
		amp *= 0.5f;
		x *= 2.0f; y *= 2.0f; z *= 2.0f;
	}
	return sum / norm;
}

//
// generation
//
int g_iSCBlockTop = 0;	// worldspawn "sc_blocktop": blocks fill z < this; realistic ground sits on top (0 = flat test map)
static int g_iSCWorldTop = 0;	// the current map's, kept after generation

int SC_BlockTop( void )
{
	return g_iSCWorldTop;
}

static bool SC_IsCave( int x, int y, int z, int seed, int roof )
{
	if( z <= SC_WORLD_MINZ + 1 || z > roof )
		return false;

	// caverns: big blobs, squashed vertically
	float cheese = SC_Fbm( x / 22.0f, y / 22.0f, z / 11.0f, seed + 11, 3 );
	if( cheese > 0.42f && z < roof - 1 )
		return true;

	// tunnels: where two noise fields both cross zero
	float a = SC_Noise3( x / 18.0f, y / 18.0f, z / 12.0f, seed + 23 );
	float b = SC_Noise3( x / 18.0f, y / 18.0f, z / 12.0f, seed + 37 );
	float width = z < roof - 2 ? 0.075f : 0.05f;
	return fabs( a ) < width && fabs( b ) < width;
}

static void SC_OreVein( vox_api_t *v, int ore, int count, int size, int zmin, int zmax )
{
	for( int n = 0; n < count; n++ )
	{
		int x = RANDOM_LONG( SC_WORLD_MINX + 1, SC_WORLD_MAXX - 2 );
		int y = RANDOM_LONG( SC_WORLD_MINY + 1, SC_WORLD_MAXY - 2 );
		int z = RANDOM_LONG( zmin, zmax );
		for( int i = 0; i < size; i++ )
		{
			if( v->Get( x, y, z ) == BLOCK_STONE )
				v->Set( x, y, z, ore );
			switch( RANDOM_LONG( 0, 5 ))
			{
			case 0: x++; break;
			case 1: x--; break;
			case 2: y++; break;
			case 3: y--; break;
			case 4: z++; break;
			default: z--; break;
			}
		}
	}
}

static void SC_Tree( vox_api_t *v, int x, int y, int z )
{
	int h = RANDOM_LONG( 4, 6 );
	for( int i = 0; i < h; i++ )
		v->Set( x, y, z + i, BLOCK_LOG );
	for( int dz = h - 3; dz <= h; dz++ )
	{
		int r = dz >= h - 1 ? 1 : 2;
		for( int dx = -r; dx <= r; dx++ )
			for( int dy = -r; dy <= r; dy++ )
			{
				if( abs( dx ) == r && abs( dy ) == r && ( r == 2 || dz == h ))
					continue;
				if( !v->Get( x + dx, y + dy, z + dz ))
					v->Set( x + dx, y + dy, z + dz, BLOCK_LEAVES );
			}
	}
}

static int SC_Cell( float units )
{
	return (int)floor( units / VOX_BLOCK_SIZE );
}

// carve a 2x2x2 worm from (x, y, z) heading down and away, joining the caves below
static void SC_CaveEntrance( vox_api_t *v, int x, int y, int z, int depth )
{
	int dx = RANDOM_LONG( 0, 1 ) ? 1 : -1, dy = RANDOM_LONG( 0, 1 ) ? 1 : -1;
	for( int step = 0; step < depth * 3; step++ )
	{
		for( int i = 0; i < 2; i++ )
			for( int j = 0; j < 2; j++ )
				for( int k = 0; k < 3; k++ )
					if( v->Get( x + i, y + j, z + k ) != BLOCK_BEDROCK )
						v->Set( x + i, y + j, z + k, BLOCK_AIR );
		if( step % 3 == 0 && z > SC_WORLD_MINZ + 4 )
			z--;
		else if( RANDOM_LONG( 0, 1 ))
			x += dx;
		else
			y += dy;
	}
}

// the Minecraft world breaking through: a blocky mound filling the torn crater, a cave going down,
// a tree, and stray blocks hanging in the air above it
static void SC_GenerateRift( vox_api_t *v, CBaseEntity *pRift, int top )
{
	float radius = pRift->pev->fuser1 > 0 ? pRift->pev->fuser1 : 256;
	float height = pRift->pev->fuser2;
	int cx = SC_Cell( pRift->pev->origin.x ), cy = SC_Cell( pRift->pev->origin.y );
	int r = (int)( radius / VOX_BLOCK_SIZE ) + 1;

	for( int x = cx - r; x <= cx + r; x++ )
		for( int y = cy - r; y <= cy + r; y++ )
		{
			float d = sqrt( (float)(( x - cx ) * ( x - cx ) + ( y - cy ) * ( y - cy ))) * VOX_BLOCK_SIZE / radius;
			if( d > 1.15f )
				continue;
			// mound top in blocks; ragged near the torn edge
			float h = height * ( 1.0f - d * d );
			int ztop = SC_Cell( h ) + ( d > 0.8f ? RANDOM_LONG( -1, 1 ) : 0 );
			for( int z = top; z <= ztop; z++ )
				v->Set( x, y, z, z == ztop ? BLOCK_GRASS : ( z > ztop - 3 ? BLOCK_DIRT : BLOCK_STONE ));
		}

	SC_CaveEntrance( v, cx - 1, cy - 1, SC_Cell( height ) - 1, 8 );

	int tx = cx + RANDOM_LONG( 2, 4 ) * ( RANDOM_LONG( 0, 1 ) ? 1 : -1 ), ty = cy + RANDOM_LONG( 2, 4 );
	for( int z = SC_WORLD_MAXZ - 8; z >= top; z-- )
		if( v->Get( tx, ty, z ) == BLOCK_GRASS )
		{
			SC_Tree( v, tx, ty, z + 1 );
			break;
		}

	static const int floaters[] = { BLOCK_DIRT, BLOCK_GRASS, BLOCK_STONE, BLOCK_COBBLE, BLOCK_PLANKS };
	for( int n = 0; n < 9; n++ )
	{
		int x = cx + RANDOM_LONG( -r, r ), y = cy + RANDOM_LONG( -r, r ), z = SC_Cell( height ) + RANDOM_LONG( 2, 6 );
		int id = floaters[RANDOM_LONG( 0, 4 )];
		v->Set( x, y, z, id );
		if( RANDOM_LONG( 0, 2 ) == 0 )
			v->Set( x + 1, y, z, id );
	}
}

static void SC_GenerateWorld( vox_api_t *v, int seed )
{
	int top = g_iSCBlockTop;		// blocks fill z < top
	bool flat = top >= 0;			// test map: no realistic ground, grass on top
	int roof = flat ? -1 : top - 3;	// caves stay below this (keep a roof under the realistic ground)

	v->Fill( SC_WORLD_MINX, SC_WORLD_MINY, SC_WORLD_MINZ, SC_WORLD_MAXX, SC_WORLD_MAXY, top - 2, BLOCK_STONE );
	v->Fill( SC_WORLD_MINX, SC_WORLD_MINY, top - 2, SC_WORLD_MAXX, SC_WORLD_MAXY, top, BLOCK_DIRT );
	if( flat )
		v->Fill( SC_WORLD_MINX, SC_WORLD_MINY, top - 1, SC_WORLD_MAXX, SC_WORLD_MAXY, top, BLOCK_GRASS );
	v->Fill( SC_WORLD_MINX, SC_WORLD_MINY, SC_WORLD_MINZ, SC_WORLD_MAXX, SC_WORLD_MAXY, SC_WORLD_MINZ + 1, BLOCK_BEDROCK );

	for( int x = SC_WORLD_MINX; x < SC_WORLD_MAXX; x++ )
		for( int y = SC_WORLD_MINY; y < SC_WORLD_MAXY; y++ )
		{
			for( int z = SC_WORLD_MINZ + 1; z <= SC_WORLD_MINZ + 3; z++ )
				if( RANDOM_LONG( 0, 3 ) < SC_WORLD_MINZ + 4 - z - 1 )
					v->Set( x, y, z, BLOCK_BEDROCK );

			float pocket = SC_Noise3( x / 7.0f, y / 7.0f, 0.5f, seed + 51 );
			if( pocket > 0.55f )
				v->Set( x, y, top - 3, pocket > 0.7f ? BLOCK_SAND : BLOCK_GRAVEL );
		}

	SC_OreVein( v, BLOCK_COAL_ORE, 70, 8, -14, top - 3 );
	SC_OreVein( v, BLOCK_IRON_ORE, 45, 6, -20, top - 4 );
	SC_OreVein( v, BLOCK_GOLD_ORE, 14, 5, -22, -14 );
	SC_OreVein( v, BLOCK_CRYSTAL_ORE, 10, 4, -22, -10 );
	SC_OreVein( v, BLOCK_DIAMOND_ORE, 8, 3, -22, -18 );

	// caves after ores so cave walls show them
	int carved = 0;
	for( int z = SC_WORLD_MINZ + 1; z < top; z++ )
		for( int y = SC_WORLD_MINY; y < SC_WORLD_MAXY; y++ )
			for( int x = SC_WORLD_MINX; x < SC_WORLD_MAXX; x++ )
			{
				int id = v->Get( x, y, z );
				if( id == BLOCK_BEDROCK || !id )
					continue;
				if( SC_IsCave( x, y, z, seed, roof ))
				{
					v->Set( x, y, z, BLOCK_AIR );
					carved++;
				}
			}

	// rifts: where the block world breaks through the realistic ground
	int rifts = 0;
	CBaseEntity *pRift = NULL;
	while(( pRift = UTIL_FindEntityByClassname( pRift, "info_sc_rift" )) != NULL )
	{
		SC_GenerateRift( v, pRift, top );
		rifts++;
	}

	if( flat )
	{
		for( int t = 0; t < 24; t++ )
		{
			int x = RANDOM_LONG( SC_WORLD_MINX + 3, SC_WORLD_MAXX - 4 );
			int y = RANDOM_LONG( SC_WORLD_MINY + 3, SC_WORLD_MAXY - 4 );
			if( v->Get( x, y, -1 ) == BLOCK_GRASS && !v->Get( x, y, 0 ) && ( abs( x ) > 4 || abs( y ) > 4 ))
				SC_Tree( v, x, y, 0 );
		}
	}

	ALERT( at_console, "Svencraft: block world generated (seed %d, top %d, %d cave blocks, %d rifts)\n", seed, top, carved, rifts );
}

// worldspawn: make an empty block world now (map entities may rest on it), fill it once everything has spawned
// how the Half-Life side treats the block world's creatures:
//   0 = doesn't know what they are (ignores them), 1 = fights them, 2 = fights them alongside the players
static cvar_t sc_alliance = { "sc_alliance", "0", FCVAR_SERVER };
// the block world's seed: 0 = a new world every map load, like Minecraft (a number: the same world, for tests)
static cvar_t sc_seed = { "sc_seed", "0", FCVAR_SERVER };

// Sven Co-op's mp_npckill: 1 (default) anything can hurt the players' allies; 0 nothing can; 2 nothing on the
// players' side can (players and other allies), the enemy still can
static cvar_t mp_npckill = { "mp_npckill", "1", FCVAR_SERVER };

bool SC_BlockFriendlyFire( CBaseEntity *pVictim, entvars_t *pevAttacker )
{
	int mode = (int)mp_npckill.value;
	if( mode == 1 || !pVictim || pVictim->IsPlayer() || !( pVictim->pev->flags & FL_MONSTER ))
		return false;
	CBaseMonster *m = pVictim->MyMonsterPointer();
	CBaseEntity *pl = UTIL_PlayerByIndex( 1 );
	for( int i = 2; !pl && i <= gpGlobals->maxClients; i++ )
		pl = UTIL_PlayerByIndex( i );
	if( !m || !pl || m->IRelationship( pl ) != R_AL )
		return false;		// not one of the players' allies
	if( mode == 0 )
		return true;
	CBaseEntity *a = pevAttacker ? CBaseEntity::Instance( pevAttacker ) : NULL;
	if( !a || a == pVictim )
		return false;
	return a->IsPlayer() || ( a->MyMonsterPointer() && m->IRelationship( a ) == R_AL );
}

void SC_RegisterCvars( void )
{
	CVAR_REGISTER( &mp_npckill );
	CVAR_REGISTER( &sc_alliance );
	CVAR_REGISTER( &sc_seed );
	SC_RegisterStateCommands();
	SC_RegisterPlayerCvars();
	SC_RegisterFogCommands();
	SC_RegisterSaveCommands();
	SC_RegisterRiftCvars();
}

int SC_Relationship( int me, int them, int halfLife )
{
	bool mcMe = me == CLASS_MINECRAFT, mcThem = them == CLASS_MINECRAFT;
	if( mcMe )
	{
		if( mcThem )
			return R_AL;
		// everything alive in this world is a target; things that aren't really creatures are not
		if( them == CLASS_NONE || them == CLASS_MACHINE || them == CLASS_PLAYER_BIOWEAPON || them == CLASS_ALIEN_BIOWEAPON || them >= 99 )
			return R_NO;
		return R_HT;
	}
	if( mcThem )
		return sc_alliance.value >= 1 ? R_HT : R_NO;
	// the alliance: monsters leave the players alone
	if( sc_alliance.value >= 2 && ( them == CLASS_PLAYER || them == CLASS_PLAYER_ALLY || them == CLASS_PLAYER_ALLY_MILITARY ) && halfLife > R_NO )
		return R_NO;
	return halfLife;
}

void SC_WorldSpawn( void )
{
	SC_FogReset();
	SC_CraftingReset();

	const char *pszMap = STRING( gpGlobals->mapname );
	if( strncmp( pszMap, "svencraft", 9 ))
		return;

	vox_api_t *v = SC_Vox();
	if( !v )
		return;

	if( !v->Init( SC_WORLD_MINX, SC_WORLD_MINY, SC_WORLD_MINZ, SC_WORLD_MAXX, SC_WORLD_MAXY, SC_WORLD_MAXZ ))
		return;

	// see-through blocks still collide but don't hide their neighbours' faces
	v->SetFlags( BLOCK_LEAVES, VOXF_SOLID );
	v->SetFlags( BLOCK_GLASS, VOXF_SOLID );
	for( int t = BLOCK_TORCH; t <= BLOCK_WALL_TORCH_NY; t++ )
		v->SetFlags( t, 0 );	// torches: walk through them, see past them
}

// ServerActivate: all map entities exist now (rifts), so the world can be generated
void SC_WorldActivate( void )
{
	// the map's shadow caster for the light compiler (a copy of the diggable geometry): not part of the game
	CBaseEntity *pShadow = NULL;
	while(( pShadow = UTIL_FindEntityByTargetname( pShadow, "sc_shadow" )) != NULL )
		UTIL_Remove( pShadow );

	vox_api_t *v = SC_Vox();
	bool kept = SC_LoadWorld();	// a world kept from before (sc_save.cpp), or
	if( !v || !v->World()->active )
		return;
	if( !kept )
		SC_GenerateWorld( v, sc_seed.value != 0 ? (int)sc_seed.value : RANDOM_LONG( 1, 999999 ));
	g_iSCWorldTop = g_iSCBlockTop;
	g_iSCBlockTop = 0;	// the next map sets its own
}

//
// developer commands (cheats): sc_tp x y z [pitch yaw], sc_cave [n] (teleport into the n-th cave spot)
//
static void SC_Teleport( edict_t *pEntity, const Vector &vecOrigin, const Vector &vecAngles )
{
	entvars_t *pev = &pEntity->v;
	SET_ORIGIN( pEntity, vecOrigin );
	pev->velocity = g_vecZero;
	pev->angles = vecAngles;
	pev->v_angle = vecAngles;
	pev->fixangle = 1;
}

// put a creature that has just been made on free floor near "want" (its feet): the hull that fits its size
// (human 32 wide, large 64), clear of walls and of other creatures; tries rings of spots around it.
// Everything that brings monsters in at run time (sc_summon now, the dungeon master later) goes through here.
bool SC_PlaceCreature( CBaseEntity *pNew, const Vector &want, edict_t *pIgnore )
{
	Vector size = pNew->pev->maxs - pNew->pev->mins;
	bool large = size.x > 34.0f || size.y > 34.0f || size.z > 74.0f;
	int hull = large ? large_hull : human_hull;
	float half = large ? 32.0f : 36.0f;	// from the hull's middle down to its floor
	for( int ring = 0; ring < 5; ring++ )
	{
		int steps = ring ? 8 : 1;
		for( int k = 0; k < steps; k++ )
		{
			float a = k * ( M_PI * 2.0f / steps );
			Vector at = want + Vector( cos( a ), sin( a ), 0 ) * ( ring * ( large ? 72.0f : 48.0f ));
			TraceResult tr;
			// down onto the floor from a little above
			UTIL_TraceHull( at + Vector( 0, 0, 64 ), at - Vector( 0, 0, 256 ), ignore_monsters, hull, pIgnore, &tr );
			if( tr.fAllSolid || tr.fStartSolid || tr.flFraction >= 1.0f )
				continue;
			Vector mid = tr.vecEndPos + Vector( 0, 0, 1 );
			// and nothing standing there already: by the real boxes, with a hand's width between
			Vector feet( mid.x, mid.y, mid.z - half + 1 );
			Vector lo = feet + pNew->pev->mins - Vector( 8, 8, 0 ), hi = feet + pNew->pev->maxs + Vector( 8, 8, 0 );
			bool taken = false;
			CBaseEntity *other = NULL;
			while(( other = UTIL_FindEntityInSphere( other, feet, 384.0f )) != NULL )
			{
				if( other == pNew || !( other->pev->flags & ( FL_MONSTER | FL_CLIENT )) || other->pev->solid == SOLID_NOT )
					continue;
				if( other->pev->absmin.x < hi.x && other->pev->absmax.x > lo.x && other->pev->absmin.y < hi.y
					&& other->pev->absmax.y > lo.y && other->pev->absmin.z < hi.z && other->pev->absmax.z > lo.z )
				{
					taken = true;
					break;
				}
			}
			if( taken )
				continue;
			UTIL_SetOrigin( pNew->pev, Vector( mid.x, mid.y, mid.z - half + 1 ));
			// a turret stands on its origin, its box reaching as far below it as above (Half-Life's): it is set down
			// on the spot the hull found clear, not dropped or walked
			if( pNew->pev->movetype == MOVETYPE_FLY && pNew->pev->mins.z < 0.0f )
				return true;
			// the engine's own test, the one a monster's start makes ("stuck in wall"): its whole footprint on
			// ground it can stand on (a voltigore is 160 wide: a curb under one corner is enough to fail)
			DROP_TO_FLOOR( pNew->edict());
			if( WALK_MOVE( pNew->edict(), 0, 0, WALKMOVE_NORMAL ))
				return true;
		}
	}
	return false;
}

// Sven Co-op's charger keys. An empty charger refills after CustomRechargeTime (or Half-Life's dmdelay), else
// after the rules' time, else Sven's: chargers in co-op come back (Half-Life's multiplayer times, 60 s for health,
// 30 s for the suit); single-player Half-Life never refilled them
bool SC_ChargerKeyValue( scCharger_t *c, KeyValueData *pkvd )
{
	const char *k = pkvd->szKeyName, *v = pkvd->szValue;
	if( FStrEq( k, "CustomJuice" )) c->juice = atoi( v );
	else if( FStrEq( k, "CustomRechargeTime" ) || FStrEq( k, "dmdelay" )) c->recharge = atoi( v );
	else if( FStrEq( k, "TriggerOnEmpty" )) c->onEmpty = ALLOC_STRING( v );
	else if( FStrEq( k, "TriggerOnRecharged" )) c->onRecharged = ALLOC_STRING( v );
	else if( FStrEq( k, "CustomDeniedSound" )) c->denied = ALLOC_STRING( v );
	else if( FStrEq( k, "CustomStartSound" )) c->start = ALLOC_STRING( v );
	else if( FStrEq( k, "CustomLoopSound" )) c->loop = ALLOC_STRING( v );
	else if( FStrEq( k, "soundlist" ) || FStrEq( k, "_minlight" )) ;	// Sven's sound replacement file: not here
	else
		return false;
	pkvd->fHandled = TRUE;
	return true;
}

void SC_ChargerPrecache( scCharger_t *c )
{
	string_t s[] = { c->denied, c->start, c->loop };
	for( int i = 0; i < 3; i++ )
		if( s[i] )
			PRECACHE_SOUND( STRING( s[i] ));
}

const char *SC_ChargerSound( string_t custom, const char *standard )
{
	return custom ? STRING( custom ) : standard;
}

int SC_ChargerRefillTime( scCharger_t *c, float rules, int standard )
{
	if( c->recharge > 0 )
		return c->recharge;
	return rules > 0 ? (int)rules : standard;
}

// a key and value for an entity that is being made, as the map would give it: the fields every entity has here,
// the rest through the entity's own KeyValue (sc_summon now, the dungeon master later)
void SC_SetKeyValue( CBaseEntity *pEnt, const char *key, const char *value )
{
	entvars_t *v = pEnt->pev;
	if( !strcmp( key, "rendercolor" ))
		sscanf( value, "%f %f %f", &v->rendercolor.x, &v->rendercolor.y, &v->rendercolor.z );
	else if( !strcmp( key, "renderamt" )) v->renderamt = atof( value );
	else if( !strcmp( key, "rendermode" )) v->rendermode = atoi( value );
	else if( !strcmp( key, "renderfx" )) v->renderfx = atoi( value );
	else if( !strcmp( key, "scale" )) v->scale = atof( value );
	else if( !strcmp( key, "skin" )) v->skin = atoi( value );
	else if( !strcmp( key, "body" )) v->body = atoi( value );
	else if( !strcmp( key, "spawnflags" )) v->spawnflags = atoi( value );
	else if( !strcmp( key, "health" )) v->health = atof( value );
	else if( !strcmp( key, "targetname" )) v->targetname = ALLOC_STRING( value );
	else if( !strcmp( key, "target" )) v->target = ALLOC_STRING( value );
	else if( !strcmp( key, "model" )) v->model = ALLOC_STRING( value );
	else if( !strcmp( key, "origin" )) sscanf( value, "%f %f %f", &v->origin.x, &v->origin.y, &v->origin.z );
	else if( !strcmp( key, "angles" )) sscanf( value, "%f %f %f", &v->angles.x, &v->angles.y, &v->angles.z );
	else
	{
		KeyValueData kvd;
		kvd.szClassName = (char *)STRING( v->classname );
		kvd.szKeyName = (char *)key;
		kvd.szValue = (char *)value;
		kvd.fHandled = FALSE;
		pEnt->KeyValue( &kvd );
	}
}

bool SC_ClientCommand( edict_t *pEntity, const char *pcmd )
{
	extern cvar_t *g_enable_cheats;

	if( !strcmp( pcmd, "sc_tp" ))
	{
		if( g_enable_cheats->value != 0 && CMD_ARGC() >= 4 )
			SC_Teleport( pEntity, Vector( atof( CMD_ARGV( 1 )), atof( CMD_ARGV( 2 )), atof( CMD_ARGV( 3 ))),
				Vector( CMD_ARGC() >= 5 ? atof( CMD_ARGV( 4 )) : 0, CMD_ARGC() >= 6 ? atof( CMD_ARGV( 5 )) : 0, 0 ));
		return true;
	}

	// the inventory screens (sent by the client's hud_inventory.cpp)
	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pEntity );
	if( !strcmp( pcmd, "sc_screen" ))
	{
		// sc_screen 1: the inventory opens; 0: whatever screen is open closes
		if( pPlayer && CMD_ARGC() >= 2 )
		{
			if( atoi( CMD_ARGV( 1 )))
				SC_OpenScreen( pPlayer, SCS_INVENTORY, -1 );
			else
				SC_CloseScreen( pPlayer );
		}
		return true;
	}
	if( !strcmp( pcmd, "sc_click" ))
	{
		// sc_click <slot> <0 left / 1 right> <shift>
		if( pPlayer && CMD_ARGC() >= 4 )
			SC_ScreenClick( pPlayer, atoi( CMD_ARGV( 1 )), atoi( CMD_ARGV( 2 )), atoi( CMD_ARGV( 3 )) != 0 );
		return true;
	}
	if( !strcmp( pcmd, "sc_hotkey" ))
	{
		// sc_hotkey <slot> <hotbar slot 0-8>: a number key over a slot swaps it with the hotbar's
		if( pPlayer && CMD_ARGC() >= 3 )
			SC_ScreenHotkey( pPlayer, atoi( CMD_ARGV( 1 )), atoi( CMD_ARGV( 2 )));
		return true;
	}
	if( !strcmp( pcmd, "sc_throwslot" ))
	{
		// sc_throwslot <slot> [all]: the drop key over a slot throws one of it (or the stack)
		if( pPlayer && CMD_ARGC() >= 2 )
			SC_ScreenThrow( pPlayer, atoi( CMD_ARGV( 1 )), CMD_ARGC() >= 3 && atoi( CMD_ARGV( 2 )) != 0 );
		return true;
	}
	if( !strcmp( pcmd, "sc_gather" ))
	{
		// sc_gather <slot>: a double click there gathers the item on the mouse from the other slots
		if( pPlayer && CMD_ARGC() >= 2 )
			SC_ScreenGather( pPlayer, atoi( CMD_ARGV( 1 )));
		return true;
	}
	if( !strcmp( pcmd, "sc_spread" ))
	{
		// sc_spread <0 left / 1 right> <slot> <slot> ...: a drag across slots spreads what the mouse carries
		int slots[64], n = 0;
		for( int a = 2; a < CMD_ARGC() && n < 64; a++ )
			slots[n++] = atoi( CMD_ARGV( a ));
		if( pPlayer && CMD_ARGC() >= 3 )
			SC_ScreenSpread( pPlayer, atoi( CMD_ARGV( 1 )), slots, n );
		return true;
	}
	if( !strcmp( pcmd, "sc_recipe" ))
	{
		// sc_recipe <index> [all]: the recipe book lays it out in the grid
		if( pPlayer && CMD_ARGC() >= 2 )
			SC_RecipeFill( pPlayer, atoi( CMD_ARGV( 1 )), CMD_ARGC() >= 3 && atoi( CMD_ARGV( 2 )) != 0 );
		return true;
	}
	if( !strcmp( pcmd, "sc_hotbar" ))
	{
		// sc_hotbar <1..9> | next | prev | last | item <id>   (number keys, the mouse wheel, Q; item: for tests)
		if( pPlayer && CMD_ARGC() >= 2 )
		{
			SCInventory &inv = SC_Inv( pPlayer );
			int was = inv.hotbar;
			if( !strcmp( CMD_ARGV( 1 ), "item" ) && CMD_ARGC() >= 3 )
			{
				for( int i = 0; i < SC_HOTBAR_SLOTS; i++ )
					if( inv.slot[i].count > 0 && inv.slot[i].id == atoi( CMD_ARGV( 2 )))
					{
						inv.hotbar = i;
						break;
					}
				SC_InvSend( pPlayer );
			}
			else if( !strcmp( CMD_ARGV( 1 ), "next" ))
				SC_InvCycle( pPlayer, 1 );
			else if( !strcmp( CMD_ARGV( 1 ), "prev" ))
				SC_InvCycle( pPlayer, -1 );
			else if( !strcmp( CMD_ARGV( 1 ), "last" ))
			{
				inv.hotbar = Q_max( 0, Q_min( SC_HOTBAR_SLOTS - 1, inv.lastHotbar ));
				SC_InvSend( pPlayer );
			}
			else
			{
				inv.hotbar = Q_max( 0, Q_min( SC_HOTBAR_SLOTS - 1, atoi( CMD_ARGV( 1 )) - 1 ));
				SC_InvSend( pPlayer );
			}
			if( inv.hotbar != was )
				inv.lastHotbar = was;
		}
		return true;
	}
	if( !strcmp( pcmd, "lastinv" ))
	{
		// Half-Life's last weapon: the slot held before
		if( pPlayer )
		{
			SCInventory &inv = SC_Inv( pPlayer );
			int was = inv.hotbar;
			inv.hotbar = Q_max( 0, Q_min( SC_HOTBAR_SLOTS - 1, inv.lastHotbar ));
			inv.lastHotbar = was;
			SC_InvSend( pPlayer );
		}
		return true;
	}
	if( !strcmp( pcmd, "drop" ) || !strcmp( pcmd, "sc_drop" ))
	{
		// drop [all]: throws one of what is held (the whole stack with "all"), Minecraft's Q
		if( pPlayer )
			SC_DropHeld( pPlayer, CMD_ARGC() >= 2 && !strcmp( CMD_ARGV( 1 ), "all" ));
		return true;
	}

	if( !strcmp( pcmd, "sc_setblock" ))
	{
		// sc_setblock <x> <y> <z> <id>   (cheats, block coordinates): as if placed or broken there
		vox_api_t *v = SC_Vox();
		if( g_enable_cheats->value == 0 || CMD_ARGC() < 5 || !v )
			return true;
		int c[3] = { atoi( CMD_ARGV( 1 )), atoi( CMD_ARGV( 2 )), atoi( CMD_ARGV( 3 )) };
		int id = atoi( CMD_ARGV( 4 ));
		v->Set( c[0], c[1], c[2], id );
		if( id == BLOCK_AIR )
			SC_BlockRemoved( c );
		else
			SC_CheckFall( c );
		return true;
	}

	if( !strcmp( pcmd, "sc_carve" ))
	{
		// sc_carve <x> <y> <z> [x1 y1 z1]   (cheats, block coordinates): digs the cells out, realistic geometry and blocks
		vox_api_t *v = SC_Vox();
		dyn_api_t *d = SC_Dyn();
		if( g_enable_cheats->value == 0 || CMD_ARGC() < 4 )
			return true;
		int a[3], b[3];
		for( int i = 0; i < 3; i++ )
		{
			a[i] = atoi( CMD_ARGV( 1 + i ));
			b[i] = CMD_ARGC() >= 7 ? atoi( CMD_ARGV( 4 + i )) : a[i];
			if( b[i] < a[i] ) { int t = a[i]; a[i] = b[i]; b[i] = t; }
		}
		for( int z = b[2]; z >= a[2]; z-- )
			for( int y = a[1]; y <= b[1]; y++ )
				for( int x = a[0]; x <= b[0]; x++ )
				{
					int c[3] = { x, y, z };
					Vector mins( x * VOX_BLOCK_SIZE, y * VOX_BLOCK_SIZE, z * VOX_BLOCK_SIZE );
					Vector maxs = mins + Vector( VOX_BLOCK_SIZE, VOX_BLOCK_SIZE, VOX_BLOCK_SIZE );
					if( d )
						d->CarveBox( mins, maxs, NULL, 0 );
					if( v && v->Get( x, y, z ) != BLOCK_AIR )
					{
						v->Set( x, y, z, BLOCK_AIR );
						SC_BlockRemoved( c );
					}
				}
		return true;
	}

	if( !strcmp( pcmd, "sc_clearmonsters" ))
	{
		// sc_clearmonsters   (cheats): removes every monster, for clean tests
		if( g_enable_cheats->value == 0 )
			return true;
		int n = 0;
		for( int i = gpGlobals->maxClients + 1; i < gpGlobals->maxEntities; i++ )
		{
			edict_t *e = INDEXENT( i );
			if( e && !e->free && FBitSet( e->v.flags, FL_MONSTER ))
			{
				UTIL_Remove( CBaseEntity::Instance( e ));
				n++;
			}
		}
		ALERT( at_console, "sc_clearmonsters: %d removed\n", n );
		return true;
	}

	if( !strcmp( pcmd, "sc_summon" ))
	{
		// (the spot is made fit for the creature's size by SC_PlaceCreature: big ones need more room)
		// sc_summon <classname> [distance]   (cheats): spawns it on the ground where the player looks
		if( g_enable_cheats->value == 0 || CMD_ARGC() < 2 )
			return true;
		entvars_t *pev = &pEntity->v;
		UTIL_MakeVectors( Vector( 0, pev->v_angle.y, 0 ));
		float dist = CMD_ARGC() >= 3 ? atof( CMD_ARGV( 2 )) : 160.0f;
		// straight ahead (short of any wall), then down to the floor
		TraceResult tr;
		Vector eye = pev->origin + Vector( 0, 0, 16 );
		UTIL_TraceHull( eye, eye + gpGlobals->v_forward * dist, ignore_monsters, head_hull, pEntity, &tr );
		Vector at = tr.vecEndPos;
		UTIL_TraceHull( at, at - Vector( 0, 0, 512 ), ignore_monsters, head_hull, pEntity, &tr );
		tr.vecEndPos.z -= 18;	// the head hull's bottom
		// sc_summon <classname> [distance] [model] [sequence] [key=value ...]: arguments with "=" are the entity's keys,
		// set before it spawns (a cycler shows any model; an sc_emitter takes kind=1 scale=0.8 ...)
		const char *model = NULL;
		int sequence = -1, positional = 0;
		for( int a = 2; a < CMD_ARGC(); a++ )
		{
			if( strchr( CMD_ARGV( a ), '=' ))
				continue;
			if( positional == 1 )
				model = CMD_ARGV( a );
			else if( positional == 2 )
				sequence = atoi( CMD_ARGV( a ));
			positional++;
		}
		edict_t *e = CREATE_NAMED_ENTITY( ALLOC_STRING( CMD_ARGV( 1 )));
		CBaseEntity *pNew = e ? CBaseEntity::Instance( e ) : NULL;
		if( pNew )
		{
			pNew->pev->origin = tr.vecEndPos + Vector( 0, 0, 4 );
			pNew->pev->angles = Vector( 0, pev->v_angle.y + 180, 0 );
			if( model )
				pNew->pev->model = ALLOC_STRING( model );
			for( int a = 2; a < CMD_ARGC(); a++ )
			{
				char kv[256];
				strncpy( kv, CMD_ARGV( a ), sizeof( kv ) - 1 );
				kv[sizeof( kv ) - 1] = 0;
				char *eq = strchr( kv, '=' );
				if( !eq )
					continue;
				*eq = 0;
				SC_SetKeyValue( pNew, kv, eq + 1 );
			}
			DispatchSpawn( e );
			if( sequence >= 0 && !FNullEnt( e ) && !( e->v.flags & FL_KILLME ))
			{
				pNew->pev->sequence = sequence;
				pNew->pev->frame = 0;
				pNew->pev->framerate = 1.0f;
				CBaseAnimating *anim = (CBaseAnimating *)pNew->MyMonsterPointer();
				if( anim )
					anim->ResetSequenceInfo();
			}
			if( FNullEnt( e ) || ( e->v.flags & FL_KILLME ))
				pNew = NULL;
		}
		if( pNew && ( pNew->pev->flags & FL_MONSTER ) && !SC_PlaceCreature( pNew, tr.vecEndPos, pEntity ))
			ALERT( at_console, "sc_summon: no room for %s here\n", CMD_ARGV( 1 ));
		if( !pNew )
			ALERT( at_console, "sc_summon: can't make %s\n", CMD_ARGV( 1 ));
		else
			ALERT( at_console, "sc_summon: %s at %.0f %.0f %.0f\n", CMD_ARGV( 1 ), pNew->pev->origin.x, pNew->pev->origin.y, pNew->pev->origin.z );
		return true;
	}

	if( !strcmp( pcmd, "sc_bot" ))
	{
		// sc_bot [name]   (cheats): a stand-in player in front of this one (sc_bots.cpp)
		if( g_enable_cheats->value != 0 )
			SC_AddBot( pEntity, CMD_ARGC() >= 2 ? CMD_ARGV( 1 ) : "Bot" );
		return true;
	}

	if( !strcmp( pcmd, "sc_hurt" ))
	{
		// sc_hurt <amount> [self | <classname>]   (cheats): hurts what the player looks at (within 2048), the player,
		// or every living one of a kind (monster_stukabat: what is hard to aim at, a flyer, one round a corner)
		if( g_enable_cheats->value == 0 || CMD_ARGC() < 2 )
			return true;
		if( CMD_ARGC() >= 3 && strcmp( CMD_ARGV( 2 ), "self" ))
		{
			CBaseEntity *pKind = NULL;
			int n = 0;
			while(( pKind = UTIL_FindEntityByClassname( pKind, CMD_ARGV( 2 ))) != NULL )
			{
				if( pKind->pev->takedamage == DAMAGE_NO || !pKind->IsAlive())
					continue;
				pKind->TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), atof( CMD_ARGV( 1 )), DMG_GENERIC );
				n++;
			}
			ALERT( at_console, "sc_hurt: %d %s hurt\n", n, CMD_ARGV( 2 ));
			return true;
		}
		CBaseEntity *pTarget = CBaseEntity::Instance( pEntity );
		if( CMD_ARGC() < 3 )
		{
			UTIL_MakeVectors( pEntity->v.v_angle );
			Vector eye = pEntity->v.origin + pEntity->v.view_ofs;
			TraceResult tr;
			UTIL_TraceLine( eye, eye + gpGlobals->v_forward * 2048, dont_ignore_monsters, pEntity, &tr );
			pTarget = tr.pHit ? CBaseEntity::Instance( tr.pHit ) : NULL;
		}
		if( pTarget && pTarget->pev->takedamage != DAMAGE_NO )
		{
			pTarget->TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), atof( CMD_ARGV( 1 )), DMG_GENERIC );
			ALERT( at_console, "sc_hurt: %s now %.0f/%.0f\n", STRING( pTarget->pev->classname ), pTarget->pev->health, pTarget->pev->max_health );
		}
		return true;
	}

	if( !strcmp( pcmd, "sc_give" ))
	{
		// sc_give <block id> [count]   (cheats)
		if( g_enable_cheats->value != 0 && CMD_ARGC() >= 2 )
			SC_InvAdd( (CBasePlayer *)CBaseEntity::Instance( pEntity ), atoi( CMD_ARGV( 1 )), CMD_ARGC() >= 3 ? atoi( CMD_ARGV( 2 )) : 64 );
		return true;
	}

	if( !strcmp( pcmd, "sc_cave" ))
	{
		vox_api_t *v = SC_Vox();
		if( g_enable_cheats->value == 0 || !v || !v->World()->active )
			return true;

		// the n-th spot (nearest the middle first) with a floor, two blocks of headroom, and open cave around it
		int want = CMD_ARGC() >= 2 ? atoi( CMD_ARGV( 1 )) : 0;
		for( int r = 0; r < 48; r++ )
			for( int x = -r; x <= r; x++ )
				for( int y = -r; y <= r; y++ )
				{
					if( abs( x ) != r && abs( y ) != r )
						continue;
					for( int z = -6; z > SC_WORLD_MINZ + 1; z-- )
					{
						if( v->Get( x, y, z - 1 ) && !v->Get( x, y, z ) && !v->Get( x, y, z + 1 ) && !v->Get( x, y, z + 2 )
							&& !v->Get( x + 2, y, z + 1 ) && !v->Get( x - 2, y, z + 1 ) && !v->Get( x, y + 2, z + 1 ) && !v->Get( x, y - 2, z + 1 ))
						{
							if( want-- > 0 )
								continue;
							Vector o(( x + 0.5f ) * VOX_BLOCK_SIZE, ( y + 0.5f ) * VOX_BLOCK_SIZE, z * VOX_BLOCK_SIZE + 37 );
							SC_Teleport( pEntity, o, Vector( 10, 45, 0 ));
							ALERT( at_console, "sc_cave: block %d %d %d\n", x, y, z );
							return true;
						}
					}
				}
		ALERT( at_console, "sc_cave: no cave found\n" );
		return true;
	}
	return false;
}

static dyn_api_t *g_pDyn;
static bool g_bDynLookedUp;

dyn_api_t *SC_Dyn( void )
{
	if( g_bDynLookedUp )
		return g_pDyn;
	g_bDynLookedUp = true;
	pfnDyn_GetAPI pfnGet = NULL;
#if XASH_WIN32
	HMODULE hEngine = GetModuleHandleA( "xash.dll" );
	if( hEngine )
		pfnGet = (pfnDyn_GetAPI)GetProcAddress( hEngine, DYN_GETAPI_NAME );
#else
	pfnGet = (pfnDyn_GetAPI)dlsym( RTLD_DEFAULT, DYN_GETAPI_NAME );
#endif
	if( pfnGet )
		g_pDyn = pfnGet( DYN_API_VERSION );
	return g_pDyn;
}
