/*
sc_blast.cpp - Svencraft: explosions break the world, Minecraft's way

Rays leave the blast in every direction (Minecraft's 16x16x16 grid of them), each with the explosion's power
give or take 30%. A ray loses strength with distance and with every solid step it takes, by that material's blast
resistance: dirt gives way easily, stone and brick hold. Every block cell a ray reaches with strength left breaks.
The town's geometry works the same way: a thin wall only costs the rays the steps that are inside it, so a
grenade at a wall blows a hole through it while one in the street just scars the ground. The creeper and every
Half-Life explosion (grenades, rockets, satchels, tripmines) go through here.

Bullets break what bullets would: glass shatters at once, leaves go in a couple of hits, wood (planks, logs, the
town's desks and counters) splinters after a magazine or so. Stone, brick, earth and metal only chip.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "sc_world.h"
#include "sc_game.h"
#include "player.h"
#include "weapons.h"
#include "pm_materials.h"
#include "sc_ballistics.h"

// Minecraft's blast resistance per block (-1: indestructible)
static const float g_SCBlastResist[BLOCK_COUNT] =
{
	0.0f,			// air
	0.6f, 0.5f,		// grass, dirt
	6.0f, 6.0f,		// stone, cobblestone
	0.6f, 0.5f,		// gravel, sand
	-1.0f,			// bedrock
	3.0f, 3.0f, 3.0f, 3.0f, 3.0f,	// ores
	2.0f, 0.2f, 3.0f,	// log, leaves, planks
	0.3f,			// glass
	6.0f,			// mossy cobblestone
	6.0f, 6.0f,		// bricks
	1.8f, 1.8f,		// concrete, asphalt (Minecraft's concrete)
	6.0f,			// scrap metal (a block of iron)
	0.8f,			// rubber
	2.5f, 3.5f,		// crafting table, furnace
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f,	// torches
	3.5f, 3.5f, 3.5f, 3.5f, 3.5f, 3.5f, 3.5f,	// furnaces
};

#define BLAST_STEP	0.3f		// blocks per ray step, Minecraft's
#define BLAST_CELLS	4096		// cells one blast can look at (power 8 reaches ~10 blocks out)

typedef struct
{
	int	c[3];
	int	used;
	float	resist;		// of the cell's material; < -1.5: not looked at yet
	int	material;	// block id (the town's: its main material)
	bool	town;		// the town's geometry, not a block
	bool	broken;
} blastcell_t;

static blastcell_t g_blastCells[BLAST_CELLS];
static int g_blastStamp;

static blastcell_t *SC_BlastCell( int x, int y, int z )
{
	unsigned int h = ( (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ (unsigned int)z * 83492791u ) & ( BLAST_CELLS - 1 );
	for( int probe = 0; probe < 64; probe++ )
	{
		blastcell_t *b = &g_blastCells[( h + probe ) & ( BLAST_CELLS - 1 )];
		if( b->used != g_blastStamp )
		{
			b->used = g_blastStamp;
			b->c[0] = x; b->c[1] = y; b->c[2] = z;
			b->resist = -2.0f;
			b->material = BLOCK_AIR;
			b->town = b->broken = false;
			return b;
		}
		if( b->c[0] == x && b->c[1] == y && b->c[2] == z )
			return b;
	}
	return NULL;
}

// what a ray stepping to p goes through: the cell's resistance and whether p is inside something (-1: the ray
// stops, at something indestructible or past what one blast keeps track of)
static float SC_BlastResistAt( const Vector &p, blastcell_t **cell, bool *solid )
{
	int x = (int)floor( p.x / VOX_BLOCK_SIZE ), y = (int)floor( p.y / VOX_BLOCK_SIZE ), z = (int)floor( p.z / VOX_BLOCK_SIZE );
	blastcell_t *b = SC_BlastCell( x, y, z );
	*cell = b;
	*solid = false;
	if( !b )
		return -1.0f;
	if( b->resist < -1.5f )
	{
		vox_api_t *v = SC_Vox();
		dyn_api_t *d = SC_Dyn();
		int id = v && v->World()->active ? v->Get( x, y, z ) : BLOCK_AIR;
		b->resist = 0.0f;
		if( id > BLOCK_AIR && id < BLOCK_COUNT )
		{
			b->material = id;
			b->resist = g_SCBlastResist[id];
		}
		else if( d && d->World()->active )
		{
			// the town's geometry in this cell: its main material
			float mins[3] = { x * VOX_BLOCK_SIZE, y * VOX_BLOCK_SIZE, z * VOX_BLOCK_SIZE };
			float maxs[3] = { mins[0] + VOX_BLOCK_SIZE, mins[1] + VOX_BLOCK_SIZE, mins[2] + VOX_BLOCK_SIZE };
			float vol[BLOCK_COUNT];
			if( d->BoxVolume( mins, maxs, vol, BLOCK_COUNT ) > 1.0f )
			{
				int best = BLOCK_CONCRETE;
				for( int m = 1; m < BLOCK_COUNT; m++ )
					if( vol[m] > vol[best] )
						best = m;
				b->material = best;
				b->resist = g_SCBlastResist[best];
				b->town = true;
			}
		}
	}
	// the town's walls are thinner than a cell: only steps inside them count
	*solid = b->material != BLOCK_AIR && ( !b->town || SC_Dyn()->PointSolid( p ));
	return b->resist;
}

// how a blast feels to those near it (cl_dll/svencraft/sc_effects.cpp): its place and power, to everyone in earshot
extern int gmsgSCBlast;
void SC_BlastFeel( const Vector &c, float power )
{
	if( !gmsgSCBlast )
		return;
	MESSAGE_BEGIN( MSG_PAS, gmsgSCBlast, c );
		WRITE_COORD( c.x );
		WRITE_COORD( c.y );
		WRITE_COORD( c.z );
		WRITE_BYTE( (int)Q_min( power * 10.0f, 255.0f ));
	MESSAGE_END();
}

void SC_Explosion( const Vector &center, float power, int dropOneIn )
{
	SC_BlastFeel( center, power );
	vox_api_t *v = SC_Vox();
	dyn_api_t *d = SC_Dyn();
	bool vox = v && v->World()->active, dyn = d && d->World()->active;
	if(( !vox && !dyn ) || power <= 0.0f )
		return;
	g_blastStamp++;

	// the rays: from the middle out to every cell on the surface of a 16x16x16 cube
	for( int i = 0; i < 16; i++ )
		for( int j = 0; j < 16; j++ )
			for( int k = 0; k < 16; k++ )
			{
				if( i != 0 && i != 15 && j != 0 && j != 15 && k != 0 && k != 15 )
					continue;
				Vector dir( i / 15.0f * 2.0f - 1.0f, j / 15.0f * 2.0f - 1.0f, k / 15.0f * 2.0f - 1.0f );
				dir = dir.Normalize() * ( BLAST_STEP * VOX_BLOCK_SIZE );
				float f = power * RANDOM_FLOAT( 0.7f, 1.3f );
				Vector p = center;
				for( ; f > 0.0f; f -= 0.225f, p = p + dir )
				{
					blastcell_t *b;
					bool solid;
					float r = SC_BlastResistAt( p, &b, &solid );
					if( r < 0.0f )
						break;
					if( !solid )
						continue;
					f -= ( r + 0.3f ) * 0.3f;
					if( f > 0.0f )
						b->broken = true;
				}
			}

	// break what the rays reached; one in `dropOneIn` comes back as an item, the rest flies apart
	float got[SCI_COUNT];
	memset( got, 0, sizeof( got ));
	for( int i = 0; i < BLAST_CELLS; i++ )
	{
		blastcell_t *b = &g_blastCells[i];
		if( b->used != g_blastStamp || !b->broken )
			continue;
		if( !b->town )
		{
			int id = v->Get( b->c[0], b->c[1], b->c[2] );
			if( id <= BLOCK_AIR || id >= BLOCK_COUNT || g_SCBlocks[id].hardness < 0 )
				continue;
			v->Set( b->c[0], b->c[1], b->c[2], BLOCK_AIR );
			got[g_SCBlocks[id].drop] += 1.0f;
			SC_BlockRemoved( b->c );
		}
		else
		{
			float mins[3] = { b->c[0] * VOX_BLOCK_SIZE, b->c[1] * VOX_BLOCK_SIZE, b->c[2] * VOX_BLOCK_SIZE };
			float maxs[3] = { mins[0] + VOX_BLOCK_SIZE, mins[1] + VOX_BLOCK_SIZE, mins[2] + VOX_BLOCK_SIZE };
			float vol[BLOCK_COUNT];
			if( d->CarveBox( mins, maxs, vol, BLOCK_COUNT ) > 0 )
				for( int m = 1; m < BLOCK_COUNT; m++ )
					got[g_SCBlocks[m].drop] += vol[m] / ( VOX_BLOCK_SIZE * VOX_BLOCK_SIZE * VOX_BLOCK_SIZE );
		}
	}

	for( int m = 1; m < SCI_COUNT; m++ )
	{
		if( got[m] <= 0.0f )
			continue;
		if( got[m] >= 0.5f )
			SC_Debris( center + Vector( 0, 0, 16 ), m < BLOCK_COUNT ? g_SCBlocks[m].family : FAM_STONE, Q_min( 2 + (int)( got[m] / 3 ), 10 ), 300 );
		int n = (int)( got[m] / Q_max( dropOneIn, 1 ) + RANDOM_FLOAT( 0, 1 ));
		if( n > 0 )
			SC_SpawnDrop( center + Vector( RANDOM_FLOAT( -40, 40 ), RANDOM_FLOAT( -40, 40 ), 8 ), m, Q_min( n, 64 ));
	}
}

// a Half-Life explosion (CGrenade::Explode: grenades, rockets, satchels, tripmines): its damage as Minecraft
// power (a hand grenade's 100 is 1.7, a rocket's 150 2.5, a satchel's 160 a little less than the creeper's 3);
// one in three blocks drops
void SC_HLExplosion( const Vector &center, float damage )
{
	float power = damage / 60.0f;
	SC_BlastProps( center, power * 24.0f );
	SC_Explosion( center, power, 3 );
}

//
// bullets
//
#define BULLET_CELLS	32
#define BULLET_FORGET	8.0f	// seconds before a cell's bullet damage is forgotten

extern const char *g_SCFamBreak[FAM_COUNT];

static struct { int c[3]; float dmg, time; bool used; } g_bulletCells[BULLET_CELLS];

// how much bullet damage a material takes before it gives way (0: bullets only chip it)
static float SC_BulletToughness( int material )
{
	switch( material )
	{
	case BLOCK_GLASS:		return 1.0f;
	case BLOCK_LEAVES:		return 12.0f;
	case BLOCK_PLANKS:
	case BLOCK_CRAFTING_TABLE:	return 110.0f;
	case BLOCK_LOG:			return 160.0f;
	}
	return 0.0f;
}

// a round from the air into water splashes where it goes in, like Sven's water impacts: droplets and a plip. The
// client does it for players' shots (cl_dll/svencraft/sc_effects.cpp); for monsters' this finds the place and the
// clients that can see it draw the same splash
extern int gmsgSCSplash;
static bool SC_Liquid( int contents )
{
	return contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA;
}

// the water's surface on the way from a (in the air) to b (in it); false if the way doesn't go into water
static bool SC_FindSurface( Vector a, Vector b, Vector &at )
{
	if( SC_Liquid( UTIL_PointContents( a )) || !SC_Liquid( UTIL_PointContents( b )))
		return false;
	for( int i = 0; i < 12; i++ )		// where the air ends
	{
		Vector mid = ( a + b ) * 0.5f;
		if( SC_Liquid( UTIL_PointContents( mid )))
			b = mid;
		else
			a = mid;
	}
	at = a;
	return true;
}

// size 0: a round; 1-6: a body, by how fast it fell in
static void SC_SendSplash( const Vector &at, int size )
{
	if( !gmsgSCSplash )
		return;
	MESSAGE_BEGIN( MSG_PVS, gmsgSCSplash, at );
		WRITE_COORD( at.x );
		WRITE_COORD( at.y );
		WRITE_COORD( at.z );
		WRITE_BYTE( size );
	MESSAGE_END();
}

void SC_BulletSplash( const Vector &src, const Vector &end )
{
	Vector at;
	if( SC_FindSurface( src, end - ( end - src ).Normalize() * 2.0f, at ))
		SC_SendSplash( at, 0 );
}

// someone falling into water throws it up, as much as the fall was fast (Sven's splash)
void SC_WaterEntry( CBaseEntity *pEnt, float speed )
{
	Vector at, o = pEnt->pev->origin;
	if( speed >= 250.0f && SC_FindSurface( o + Vector( 0, 0, 64 ), o + Vector( 0, 0, pEnt->pev->mins.z + 2.0f ), at ))
		SC_SendSplash( at, (int)Q_min( speed / 150.0f, 6.0f ));
}

void SC_BulletHitWorld( const Vector &end, const Vector &normal, float damage )
{
	vox_api_t *v = SC_Vox();
	dyn_api_t *d = SC_Dyn();
	Vector p = end - normal * 2.0f;
	int c[3] = { (int)floor( p.x / VOX_BLOCK_SIZE ), (int)floor( p.y / VOX_BLOCK_SIZE ), (int)floor( p.z / VOX_BLOCK_SIZE ) };
	float mins[3] = { c[0] * VOX_BLOCK_SIZE, c[1] * VOX_BLOCK_SIZE, c[2] * VOX_BLOCK_SIZE };
	float maxs[3] = { mins[0] + VOX_BLOCK_SIZE, mins[1] + VOX_BLOCK_SIZE, mins[2] + VOX_BLOCK_SIZE };
	int material = BLOCK_AIR;
	bool town = false;

	if( v && v->World()->active )
		material = v->Get( c[0], c[1], c[2] );
	if(( material <= BLOCK_AIR || material >= BLOCK_COUNT ) && d && d->World()->active && d->PointSolid( p ))
	{
		// the town's geometry: what the cell is mostly made of
		float vol[BLOCK_COUNT];
		material = BLOCK_AIR;
		if( d->BoxVolume( mins, maxs, vol, BLOCK_COUNT ) > 1.0f )
			for( int m = 1; m < BLOCK_COUNT; m++ )
				if( material == BLOCK_AIR || vol[m] > vol[material] )
					material = m;
		town = true;
	}
	if( material <= BLOCK_AIR || material >= BLOCK_COUNT )
		return;
	float tough = SC_BulletToughness( material );
	if( tough <= 0.0f )
		return;

	// this cell's damage so far (the oldest record makes room)
	int slot = 0;
	for( int i = 0; i < BULLET_CELLS; i++ )
	{
		if( g_bulletCells[i].used && g_bulletCells[i].c[0] == c[0] && g_bulletCells[i].c[1] == c[1] && g_bulletCells[i].c[2] == c[2] )
		{
			slot = i;
			break;
		}
		if( !g_bulletCells[i].used || g_bulletCells[i].time < g_bulletCells[slot].time )
			slot = i;
	}
	if( !g_bulletCells[slot].used || g_bulletCells[slot].c[0] != c[0] || g_bulletCells[slot].c[1] != c[1] || g_bulletCells[slot].c[2] != c[2]
		|| gpGlobals->time - g_bulletCells[slot].time > BULLET_FORGET )
	{
		g_bulletCells[slot].used = true;
		g_bulletCells[slot].c[0] = c[0]; g_bulletCells[slot].c[1] = c[1]; g_bulletCells[slot].c[2] = c[2];
		g_bulletCells[slot].dmg = 0.0f;
	}
	g_bulletCells[slot].time = gpGlobals->time;
	g_bulletCells[slot].dmg += damage;

	int family = g_SCBlocks[material].family;
	if( g_bulletCells[slot].dmg < tough )
		return;		// (the splinters of each hit are the client's: cl_dll/ev_hldm.cpp)

	// it gives way: a block drops itself (glass and leaves don't, like Minecraft's); the town's wood by volume
	g_bulletCells[slot].used = false;
	Vector center( mins[0] + VOX_BLOCK_SIZE * 0.5f, mins[1] + VOX_BLOCK_SIZE * 0.5f, mins[2] + VOX_BLOCK_SIZE * 0.5f );
	if( !town )
	{
		v->Set( c[0], c[1], c[2], BLOCK_AIR );
		SC_BlockRemoved( c );
		if( material != BLOCK_GLASS && material != BLOCK_LEAVES )
			SC_SpawnDrop( center, g_SCBlocks[material].drop, 1 );
	}
	else
	{
		float vol[BLOCK_COUNT];
		if( d->CarveBox( mins, maxs, vol, BLOCK_COUNT ) > 0 )
		{
			int n = (int)( vol[material] / ( VOX_BLOCK_SIZE * VOX_BLOCK_SIZE * VOX_BLOCK_SIZE ) * 2.0f + RANDOM_FLOAT( 0, 1 ));
			if( n > 0 )
				SC_SpawnDrop( center, g_SCBlocks[material].drop, n );
		}
	}
	SC_Debris( center, family, 6 );
	EMIT_AMBIENT_SOUND( INDEXENT( 0 ), center, g_SCFamBreak[family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));
}

//
// rounds through walls (sc_ballistics.h)
//
extern "C" char PM_FindTextureType( char *name );

// what the face a trace from a to b meets on this entity is made of
static char SC_SurfaceMaterial( edict_t *hit, const Vector &a, const Vector &b )
{
	const char *tex = TRACE_TEXTURE( hit, a, b );
	if( !tex )
		return CHAR_TEX_CONCRETE;
	if( *tex == '-' || *tex == '+' )
		tex += 2;
	if( *tex == '{' || *tex == '!' || *tex == '~' || *tex == ' ' )
		tex++;
	char name[CBTEXTURENAMEMAX];
	strncpy( name, tex, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	return PM_FindTextureType( name );
}

bool SC_BulletPenetrate( CBaseEntity *pShooter, TraceResult &tr, Vector &from, const Vector &dir, float &power, float distance )
{
	if( power <= 0.0f || tr.flFraction >= 1.0f || tr.fAllSolid || FNullEnt( tr.pHit ) && tr.pHit != INDEXENT( 0 ))
		return false;
	edict_t *hit = tr.pHit;
	if( hit != INDEXENT( 0 ))
	{
		CBaseEntity *ent = CBaseEntity::Instance( hit );
		if( !ent || ent->MyMonsterPointer() || ent->IsPlayer() || ent->pev->solid != SOLID_BSP )
			return false;		// flesh, or a model: the round stops
	}
	Vector in = tr.vecEndPos, n = tr.vecPlaneNormal;
	float cost = SC_PenCost( SC_SurfaceMaterial( hit, in + n * 4.0f, in - n * 4.0f ));
	if( cost <= 0.0f )
		return false;

	// the far side, found from outside
	Vector beyond = in + dir * Q_min( power / cost, SC_PEN_REACH );
	if( UTIL_PointContents( beyond ) == CONTENTS_SOLID )
		return false;
	TraceResult back;
	UTIL_TraceLine( beyond, in, ignore_monsters, ENT( pShooter->pev ), &back );
	if( back.fStartSolid || back.flFraction >= 1.0f || back.pHit != hit )
		return false;
	float exitcost = SC_PenCost( SC_SurfaceMaterial( hit, back.vecEndPos + back.vecPlaneNormal * 4.0f, back.vecEndPos - back.vecPlaneNormal * 4.0f ));
	if( exitcost <= 0.0f )
		return false;
	power -= ( back.vecEndPos - in ).Length() * Q_max( cost, exitcost );
	if( power <= 0.0f )
		return false;

	from = back.vecEndPos + dir * 0.5f;
	UTIL_TraceLine( from, from + dir * distance, dont_ignore_monsters, ENT( pShooter->pev ), &tr );
	return true;
}

// sc_bulletlog 1: each surface a player's round meets, to compare with the client's (cl_bulletlog 1)
static cvar_t sc_bulletlog = { "sc_bulletlog", "0", FCVAR_SERVER };
void SC_RegisterBallisticsCvars( void )
{
	CVAR_REGISTER( &sc_bulletlog );
}

void SC_BulletLog( const char *side, int surf, float fraction, const Vector &end )
{
	if( sc_bulletlog.value != 0.0f && fraction < 1.0f )
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "bullet %s %d %.1f %.1f %.1f\n", side, surf, end.x, end.y, end.z ));
}
