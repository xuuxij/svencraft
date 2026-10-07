/*
voxel.c - Svencraft block world: storage, collision against blocks, and the API for the game DLLs

The block world is part of the world model. SV_ClipMoveToEntity (for the world edict), PM_PlayerTraceExt
(for physent 0) and the point-contents functions merge in the result of a swept box test against the
solid blocks, so everything that collides with the BSP also collides with blocks.
*/
#include "common.h"
#include "xash3d_mathlib.h"
#include "voxel_api.h"
#include "pm_local.h"
#include "voxel.h"

static vox_world_t vox;

const vox_world_t *Vox_World( void )
{
	return &vox;
}

qboolean Vox_Active( void )
{
	return vox.active != 0;
}

static void Vox_Free( void )
{
	if( vox.blocks ) Mem_Free( vox.blocks );
	if( vox.height ) Mem_Free( vox.height );
	if( vox.section_rev ) Mem_Free( vox.section_rev );
	vox.blocks = NULL;
	vox.height = NULL;
	vox.section_rev = NULL;
	vox.active = false;
}

static int GAME_EXPORT Vox_Init( int minx, int miny, int minz, int maxx, int maxy, int maxz )
{
	int i;
	unsigned int rev = vox.world_rev;
	unsigned char flags[VOX_MAX_IDS];

	memcpy( flags, vox.flags, sizeof( flags ));
	Vox_Free();
	memset( &vox, 0, sizeof( vox ));
	memcpy( vox.flags, flags, sizeof( flags ));
	vox.world_rev = rev + 1;

	if( maxx <= minx || maxy <= miny || maxz <= minz )
		return false;

	vox.mins[0] = minx; vox.mins[1] = miny; vox.mins[2] = minz;
	vox.maxs[0] = maxx; vox.maxs[1] = maxy; vox.maxs[2] = maxz;

	for( i = 0; i < 3; i++ )
	{
		vox.size[i] = vox.maxs[i] - vox.mins[i];
		vox.nsections[i] = ( vox.size[i] + VOX_SECTION - 1 ) / VOX_SECTION;
	}

	vox.blocks = Mem_Calloc( host.mempool, sizeof( *vox.blocks ) * vox.size[0] * vox.size[1] * vox.size[2] );
	vox.height = Mem_Malloc( host.mempool, sizeof( *vox.height ) * vox.size[0] * vox.size[1] );
	vox.section_rev = Mem_Calloc( host.mempool, sizeof( *vox.section_rev ) * vox.nsections[0] * vox.nsections[1] * vox.nsections[2] );

	for( i = 0; i < vox.size[0] * vox.size[1]; i++ )
		vox.height[i] = vox.mins[2] - 1;

	if( !vox.flags[1] )
	{
		// default: every block id is a solid, opaque cube until the game says otherwise
		for( i = 1; i < VOX_MAX_IDS; i++ )
			vox.flags[i] = VOXF_SOLID|VOXF_OPAQUE;
	}

	vox.active = true;
	Con_Reportf( "Vox_Init: %d x %d x %d blocks\n", vox.size[0], vox.size[1], vox.size[2] );
	return true;
}

static void GAME_EXPORT Vox_Shutdown( void )
{
	Vox_Free();
	vox.world_rev++;
}

// a new map is starting: the game builds a new block world if it wants one
void Vox_Clear( void )
{
	Vox_Shutdown();
}

static int GAME_EXPORT Vox_Get( int x, int y, int z )
{
	return Vox_At( &vox, x, y, z );
}

static void Vox_TouchSection( int x, int y, int z )
{
	int sx, sy, sz;

	if( !Vox_InBounds( &vox, x, y, z ))
		return;

	sx = ( x - vox.mins[0] ) / VOX_SECTION;
	sy = ( y - vox.mins[1] ) / VOX_SECTION;
	sz = ( z - vox.mins[2] ) / VOX_SECTION;
	vox.section_rev[( sz * vox.nsections[1] + sy ) * vox.nsections[0] + sx]++;
}

static void Vox_UpdateHeight( int x, int y )
{
	int *h = &vox.height[( y - vox.mins[1] ) * vox.size[0] + ( x - vox.mins[0] )];
	int z;

	for( z = vox.maxs[2] - 1; z >= vox.mins[2]; z-- )
	{
		if( Vox_At( &vox, x, y, z ))
			break;
	}
	*h = z;
}

static void GAME_EXPORT Vox_Set( int x, int y, int z, int id )
{
	unsigned short *b;
	int oldtop, i;

	if( !vox.active || !Vox_InBounds( &vox, x, y, z ))
		return;

	b = &vox.blocks[((z - vox.mins[2]) * vox.size[1] + ( y - vox.mins[1] )) * vox.size[0] + ( x - vox.mins[0] )];
	if( *b == id )
		return;
	*b = (unsigned short)id;

	oldtop = vox.height[( y - vox.mins[1] ) * vox.size[0] + ( x - vox.mins[0] )];
	if( id && z > oldtop )
		vox.height[( y - vox.mins[1] ) * vox.size[0] + ( x - vox.mins[0] )] = z;
	else if( !id && z == oldtop )
		Vox_UpdateHeight( x, y );

	// this section and the sections of the six neighbours (their faces may appear or vanish)
	Vox_TouchSection( x, y, z );
	Vox_TouchSection( x - 1, y, z ); Vox_TouchSection( x + 1, y, z );
	Vox_TouchSection( x, y - 1, z ); Vox_TouchSection( x, y + 1, z );
	Vox_TouchSection( x, y, z - 1 ); Vox_TouchSection( x, y, z + 1 );

	// sky light reaches down the column: everything below the old/new top may change brightness
	if( z >= oldtop || ( !id && z == oldtop ))
	{
		for( i = vox.mins[2]; i < vox.maxs[2]; i += VOX_SECTION )
		{
			Vox_TouchSection( x, y, i );
			Vox_TouchSection( x - 1, y, i ); Vox_TouchSection( x + 1, y, i );
			Vox_TouchSection( x, y - 1, i ); Vox_TouchSection( x, y + 1, i );
		}
	}
}

static void GAME_EXPORT Vox_Fill( int x0, int y0, int z0, int x1, int y1, int z1, int id )
{
	int x, y, z;

	if( !vox.active )
		return;

	x0 = Q_max( x0, vox.mins[0] ); x1 = Q_min( x1, vox.maxs[0] );
	y0 = Q_max( y0, vox.mins[1] ); y1 = Q_min( y1, vox.maxs[1] );
	z0 = Q_max( z0, vox.mins[2] ); z1 = Q_min( z1, vox.maxs[2] );

	for( z = z0; z < z1; z++ )
		for( y = y0; y < y1; y++ )
			for( x = x0; x < x1; x++ )
				vox.blocks[((z - vox.mins[2]) * vox.size[1] + ( y - vox.mins[1] )) * vox.size[0] + ( x - vox.mins[0] )] = (unsigned short)id;

	for( y = y0; y < y1; y++ )
		for( x = x0; x < x1; x++ )
			Vox_UpdateHeight( x, y );

	// bulk change: rebuild everything
	for( x = 0; x < vox.nsections[0] * vox.nsections[1] * vox.nsections[2]; x++ )
		vox.section_rev[x]++;
}

static void GAME_EXPORT Vox_SetFlags( int id, int flags )
{
	int x;

	if( id <= 0 || id >= VOX_MAX_IDS )
		return;
	vox.flags[id] = (unsigned char)flags;
	if( vox.active )
	{
		for( x = 0; x < vox.nsections[0] * vox.nsections[1] * vox.nsections[2]; x++ )
			vox.section_rev[x]++;
	}
}

static const vox_world_t * GAME_EXPORT Vox_GetWorld( void )
{
	return &vox;
}

static qboolean Vox_SolidAt( int x, int y, int z )
{
	return FBitSet( vox.flags[Vox_At( &vox, x, y, z )], VOXF_SOLID ) != 0;
}

static int Vox_CellOf( float v )
{
	return (int)floorf( v / VOX_BLOCK_SIZE );
}

/*
==================
Vox_SweepBox

Earliest contact of a box (mins/maxs around the moving point) going from start to end with any solid
block. A block is an obstacle when the swept box would pass strictly inside it; merely touching a face
is not a collision, so something resting on a block isn't "inside" it.
==================
*/
#define VOX_MAX_CELLS	4096

static qboolean Vox_SweepSegment( const vec3_t start, const vec3_t delta, const vec3_t mins, const vec3_t maxs, float f0, float f1, vox_trace_t *tr )
{
	vec3_t lo, hi;
	int c0[3], c1[3];
	int x, y, z, i;
	qboolean hit = false;

	for( i = 0; i < 3; i++ )
	{
		float a = start[i] + delta[i] * f0, b = start[i] + delta[i] * f1;
		lo[i] = Q_min( a, b ) + mins[i] - 0.1f;
		hi[i] = Q_max( a, b ) + maxs[i] + 0.1f;
		c0[i] = Q_max( Vox_CellOf( lo[i] ), vox.mins[i] );
		c1[i] = Q_min( Vox_CellOf( hi[i] ), vox.maxs[i] - 1 );
		if( c0[i] > c1[i] )
			return false;
	}

	for( z = c0[2]; z <= c1[2]; z++ )
	for( y = c0[1]; y <= c1[1]; y++ )
	for( x = c0[0]; x <= c1[0]; x++ )
	{
		float tenter = -1e30f, texit = 1e30f;
		int axis = -1, inside = true;
		vec3_t bmin, bmax;
		int cell[3] = { x, y, z };

		if( !Vox_SolidAt( x, y, z ))
			continue;

		for( i = 0; i < 3; i++ )
		{
			bmin[i] = cell[i] * VOX_BLOCK_SIZE - maxs[i];
			bmax[i] = ( cell[i] + 1 ) * VOX_BLOCK_SIZE - mins[i];
			if( start[i] <= bmin[i] || start[i] >= bmax[i] )
				inside = false;
		}

		if( inside )
		{
			tr->startsolid = true;
			tr->fraction = 0.0f;
			tr->block = Vox_At( &vox, x, y, z );
			VectorCopy( cell, tr->cell );
			hit = true;
			continue;
		}

		for( i = 0; i < 3; i++ )
		{
			float t1, t2;

			if( fabsf( delta[i] ) < 1e-6f )
			{
				// moving parallel to this axis: a path exactly on a block face still touches the block
				// (a box sliding along a wall is DIST_EPSILON away from it, so it doesn't count)
				if( start[i] < bmin[i] - 0.001f || start[i] > bmax[i] + 0.001f )
					break;
				continue;
			}

			t1 = ( bmin[i] - start[i] ) / delta[i];
			t2 = ( bmax[i] - start[i] ) / delta[i];
			if( t1 > t2 ) { float t = t1; t1 = t2; t2 = t; }
			if( t1 > tenter ) { tenter = t1; axis = i; }
			if( t2 < texit ) texit = t2;
		}

		if( i < 3 || axis < 0 || tenter >= texit || texit <= 0.0f || tenter < 0.0f || tenter >= tr->fraction )
			continue;

		tr->fraction = tenter;
		VectorClear( tr->normal );
		tr->normal[axis] = delta[axis] > 0.0f ? -1.0f : 1.0f;
		tr->block = Vox_At( &vox, x, y, z );
		tr->cell[0] = x; tr->cell[1] = y; tr->cell[2] = z;
		hit = true;
	}

	return hit;
}

qboolean Vox_TraceBox( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vox_trace_t *tr )
{
	vec3_t delta;
	float len, step, f;
	qboolean hit = false;

	memset( tr, 0, sizeof( *tr ));
	tr->fraction = 1.0f;

	if( !vox.active )
		return false;

	VectorSubtract( end, start, delta );
	len = VectorLength( delta );

	// long traces (bullets) are swept in pieces so each piece only looks at a few cells
	step = len > VOX_BLOCK_SIZE * 2 ? ( VOX_BLOCK_SIZE * 2 ) / len : 1.0f;
	for( f = 0.0f; f < 1.0f && !tr->startsolid; f += step )
	{
		float f1 = Q_min( f + step, 1.0f );

		if( Vox_SweepSegment( start, delta, mins, maxs, f, f1, tr ))
			hit = true;
		if( hit && tr->fraction <= f1 )
			break;
	}

	if( !hit )
		return false;

	if( tr->startsolid )
	{
		vox_trace_t te;

		// allsolid when the end position is stuck too
		memset( &te, 0, sizeof( te ));
		te.fraction = 1.0f;
		Vox_SweepSegment( end, vec3_origin, mins, maxs, 0.0f, 1.0f, &te );
		tr->allsolid = te.startsolid;
		VectorCopy( start, tr->endpos );
		return true;
	}

	// stop just short of the face, like the BSP hull checks do
	if( len > 0.0f )
		tr->fraction = Q_max( 0.0f, tr->fraction - DIST_EPSILON / len );
	VectorMA( start, tr->fraction, delta, tr->endpos );
	return true;
}

// the first block a line hits (0: none) and how far along
int Vox_TraceBlock( const vec3_t start, const vec3_t end, float *fraction )
{
	vox_trace_t tr;
	if( !Vox_TraceBox( start, vec3_origin, vec3_origin, end, &tr ) || tr.startsolid )
		return 0;
	if( fraction )
		*fraction = tr.fraction;
	return Vox_Get( tr.cell[0], tr.cell[1], tr.cell[2] );
}

qboolean Vox_BoxSolid( const vec3_t absmin, const vec3_t absmax )
{
	int c0[3], c1[3], x, y, z, i;

	if( !vox.active )
		return false;

	for( i = 0; i < 3; i++ )
	{
		// strict overlap: a box resting on a face doesn't count
		c0[i] = Q_max( Vox_CellOf( absmin[i] + 0.01f ), vox.mins[i] );
		c1[i] = Q_min( Vox_CellOf( absmax[i] - 0.01f ), vox.maxs[i] - 1 );
		if( c0[i] > c1[i] )
			return false;
	}

	for( z = c0[2]; z <= c1[2]; z++ )
		for( y = c0[1]; y <= c1[1]; y++ )
			for( x = c0[0]; x <= c1[0]; x++ )
				if( Vox_SolidAt( x, y, z ))
					return true;
	return false;
}

qboolean Vox_PointSolid( const vec3_t p )
{
	if( !vox.active )
		return false;
	return Vox_SolidAt( Vox_CellOf( p[0] ), Vox_CellOf( p[1] ), Vox_CellOf( p[2] ));
}

// fold the block collision into a world trace (server traces)
void Vox_ClipTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *trace, edict_t *world )
{
	vox_trace_t vt;

	if( !vox.active || !Vox_TraceBox( start, mins, maxs, end, &vt ))
		return;

	if( vt.startsolid )
	{
		trace->startsolid = true;
		if( vt.allsolid ) trace->allsolid = true;
		trace->fraction = 0.0f;
		VectorCopy( start, trace->endpos );
		trace->ent = world;
		return;
	}

	if( vt.fraction < trace->fraction )
	{
		trace->fraction = vt.fraction;
		VectorCopy( vt.endpos, trace->endpos );
		VectorCopy( vt.normal, trace->plane.normal );
		trace->plane.dist = DotProduct( vt.endpos, vt.normal );
		trace->ent = world;
	}
}

// same for player movement traces (physent 0 is the world)
void Vox_ClipPMTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, pmtrace_t *trace )
{
	vox_trace_t vt;

	if( !vox.active || !Vox_TraceBox( start, mins, maxs, end, &vt ))
		return;

	if( vt.startsolid )
	{
		trace->startsolid = true;
		if( vt.allsolid ) trace->allsolid = true;
		trace->fraction = 0.0f;
		VectorCopy( start, trace->endpos );
		return;
	}

	if( vt.fraction < trace->fraction )
	{
		trace->fraction = vt.fraction;
		VectorCopy( vt.endpos, trace->endpos );
		VectorCopy( vt.normal, trace->plane.normal );
		trace->plane.dist = DotProduct( vt.endpos, vt.normal );
	}
}

static int GAME_EXPORT Vox_TraceLineAPI( const float *start, const float *end, float *fraction, int *cell, float *normal )
{
	vox_trace_t vt;

	if( !Vox_TraceBox( start, vec3_origin, vec3_origin, end, &vt ))
	{
		if( fraction ) *fraction = 1.0f;
		return 0;
	}
	if( fraction ) *fraction = vt.fraction;
	if( cell ) { cell[0] = vt.cell[0]; cell[1] = vt.cell[1]; cell[2] = vt.cell[2]; }
	if( normal ) VectorCopy( vt.normal, normal );
	return vt.block;
}

static int GAME_EXPORT Vox_BoxSolidAPI( const float *absmin, const float *absmax )
{
	return Vox_BoxSolid( absmin, absmax );
}

static vox_api_t vox_api =
{
	VOX_API_VERSION,
	Vox_Init,
	Vox_Shutdown,
	Vox_Get,
	Vox_Set,
	Vox_Fill,
	Vox_SetFlags,
	Vox_GetWorld,
	Vox_TraceLineAPI,
	Vox_BoxSolidAPI,
};

EXPORT vox_api_t *Vox_GetAPI( int version );
EXPORT vox_api_t *Vox_GetAPI( int version )
{
	if( version != VOX_API_VERSION )
	{
		Con_Printf( S_ERROR "Vox_GetAPI: game wants version %d, engine has %d\n", version, VOX_API_VERSION );
		return NULL;
	}
	return &vox_api;
}
