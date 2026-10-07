/*
dynworld.c - Svencraft destructible map geometry (see common/dynworld_api.h)

Brushes are convex: a list of outward planes. Collision is the Quake 3 way (each plane pushed out by the
moving box's support distance, axial bevel planes added), so boxes slide on slopes like on BSP hulls.
Carving a box out of a brush splits it into up to six convex pieces. Faces are rebuilt per 256-unit cell and
lit like Half-Life's: a lightmap texel every 16 units (sun with shadows, sky light with occlusion, the map's
point lights), each face's part of the cell packed into the cell's lightmap.
*/
#include "common.h"
#include "xash3d_mathlib.h"
#include "pm_local.h"
#include "dynworld_api.h"
#include "dynworld.h"

#define DYN_MAX_SIDES		40
#define DYN_MAX_WINDING		64
#define DYN_EPS			0.01f
#define DYN_CLIP_EPS		( 1.0f / 32.0f )
#define DYN_MAX_LIGHTS		256

typedef struct
{
	vec3_t	normal;
	float	dist;
	short	tex;		// -1: bevel (collision only)
	float	vecs[2][4];	// texture s/t in texels
} dynside_t;

typedef struct
{
	int		numsides;
	dynside_t	sides[DYN_MAX_SIDES];
	vec3_t		mins, maxs;
	int		material;
	short		cuttex;
	qboolean	alive;
	int		owner;		// render cell
	int		check;		// trace stamp
} dynbrush_t;

typedef struct
{
	int	num, max;
	int	*list;
} dynlist_t;

typedef struct
{
	vec3_t	origin;
	vec3_t	color;		// 0..1 scaled by intensity
	float	intensity;
} dynlight_t;

static dynworld_t	dw;
static poolhandle_t	dynpool;
static dynbrush_t	*brushes;
static int		numbrushes, maxbrushes;
static dynlist_t	*gridlists;	// collision lists per cell
static int		texsize[DYN_MAX_TEXTURES][2];
static int		checkcount;
static unsigned int	carves;		// bumped by every carve: cached model light goes stale
static model_t		*dynmodel;	// the BSP world, for light rays

// lighting from the map's entities
static qboolean	has_sun;
static vec3_t	sun_dir;	// towards the sun
static vec3_t	sun_color, sky_color;
static dynlight_t	lights[DYN_MAX_LIGHTS];
static int		numlights;

const dynworld_t *Dyn_World( void ) { return &dw; }
qboolean Dyn_Active( void ) { return dw.active; }

//
// windings
//
typedef struct { int num; vec3_t p[DYN_MAX_WINDING]; } dynwinding_t;

static void Dyn_BaseWinding( const vec3_t n, float d, dynwinding_t *w )
{
	vec3_t org, vup, vright;
	int ax = 0;
	if( fabs( n[1] ) > fabs( n[ax] )) ax = 1;
	if( fabs( n[2] ) > fabs( n[ax] )) ax = 2;
	VectorClear( vup );
	if( ax == 2 ) vup[0] = 1; else vup[2] = 1;
	float v = DotProduct( vup, n );
	VectorMA( vup, -v, n, vup );
	VectorNormalize( vup );
	VectorScale( n, d, org );
	CrossProduct( vup, n, vright );
	VectorScale( vup, 65536.0f, vup );
	VectorScale( vright, 65536.0f, vright );
	w->num = 4;
	VectorSubtract( org, vright, w->p[0] ); VectorAdd( w->p[0], vup, w->p[0] );
	VectorAdd( org, vright, w->p[1] );      VectorAdd( w->p[1], vup, w->p[1] );
	VectorAdd( org, vright, w->p[2] );      VectorSubtract( w->p[2], vup, w->p[2] );
	VectorSubtract( org, vright, w->p[3] ); VectorSubtract( w->p[3], vup, w->p[3] );
}

// keep the part of w behind the plane (dot <= d)
static void Dyn_ChopWinding( dynwinding_t *w, const vec3_t n, float d )
{
	dynwinding_t out;
	float dists[DYN_MAX_WINDING + 1];
	int sides[DYN_MAX_WINDING + 1], i, front = 0;

	for( i = 0; i < w->num; i++ )
	{
		dists[i] = DotProduct( w->p[i], n ) - d;
		sides[i] = dists[i] > DYN_EPS ? 1 : ( dists[i] < -DYN_EPS ? -1 : 0 );
		if( sides[i] > 0 ) front++;
	}
	if( !front )
		return;
	if( front == w->num ) { w->num = 0; return; }
	dists[i] = dists[0]; sides[i] = sides[0];
	out.num = 0;
	for( i = 0; i < w->num && out.num < DYN_MAX_WINDING - 1; i++ )
	{
		if( sides[i] <= 0 )
		{
			VectorCopy( w->p[i], out.p[out.num] );	// (a macro: no ++ inside)
			out.num++;
		}
		if( sides[i] == 0 || sides[i + 1] == 0 || sides[i] == sides[i + 1] )
			continue;
		const float *p1 = w->p[i], *p2 = w->p[( i + 1 ) % w->num];
		float t = dists[i] / ( dists[i] - dists[i + 1] );
		for( int j = 0; j < 3; j++ )
			out.p[out.num][j] = p1[j] + t * ( p2[j] - p1[j] );
		out.num++;
	}
	*w = out;
}

static qboolean Dyn_SideWinding( const dynbrush_t *b, int s, dynwinding_t *w )
{
	Dyn_BaseWinding( b->sides[s].normal, b->sides[s].dist, w );
	for( int j = 0; j < b->numsides && w->num >= 3; j++ )
	{
		if( j == s ) continue;
		Dyn_ChopWinding( w, b->sides[j].normal, b->sides[j].dist );
	}
	return w->num >= 3;
}

// bounds from the windings; false when the brush has no volume
static qboolean Dyn_FinishBrush( dynbrush_t *b )
{
	dynwinding_t w;
	int faces = 0;
	ClearBounds( b->mins, b->maxs );
	for( int s = 0; s < b->numsides; s++ )
	{
		if( !Dyn_SideWinding( b, s, &w ))
			continue;
		faces++;
		for( int i = 0; i < w.num; i++ )
			AddPointToBounds( w.p[i], b->mins, b->maxs );
	}
	if( faces < 4 )
		return false;
	for( int i = 0; i < 3; i++ )
		if( b->maxs[i] - b->mins[i] < 0.25f )
			return false;
	return true;
}

// axial bevels so boxes don't catch on edges (collision only)
static void Dyn_AddBevels( dynbrush_t *b )
{
	for( int axis = 0; axis < 3; axis++ )
		for( int dir = -1; dir <= 1; dir += 2 )
		{
			qboolean found = false;
			for( int s = 0; s < b->numsides; s++ )
				if( b->sides[s].normal[axis] == (float)dir ) { found = true; break; }
			if( found || b->numsides >= DYN_MAX_SIDES )
				continue;
			dynside_t *side = &b->sides[b->numsides++];
			memset( side, 0, sizeof( *side ));
			side->normal[axis] = dir;
			side->dist = dir > 0 ? b->maxs[axis] : -b->mins[axis];
			side->tex = -1;
		}
}

//
// grid
//
static int Dyn_CellIndex( int x, int y, int z )
{
	return ( z * dw.gridsize[1] + y ) * dw.gridsize[0] + x;
}

static void Dyn_CellRange( const vec3_t mins, const vec3_t maxs, int *c0, int *c1 )
{
	for( int i = 0; i < 3; i++ )
	{
		c0[i] = (int)floor(( mins[i] - dw.gridmins[i] ) / DYN_CELL_SIZE );
		c1[i] = (int)floor(( maxs[i] - dw.gridmins[i] ) / DYN_CELL_SIZE );
		c0[i] = bound( 0, c0[i], dw.gridsize[i] - 1 );
		c1[i] = bound( 0, c1[i], dw.gridsize[i] - 1 );
	}
}

static void Dyn_ListAdd( dynlist_t *l, int v )
{
	if( l->num >= l->max )
	{
		l->max = l->max ? l->max * 2 : 16;
		int *n = Mem_Malloc( dynpool, sizeof( int ) * l->max );
		if( l->list ) { memcpy( n, l->list, sizeof( int ) * l->num ); Mem_Free( l->list ); }
		l->list = n;
	}
	l->list[l->num++] = v;
}

static void Dyn_ListRemove( dynlist_t *l, int v )
{
	for( int i = 0; i < l->num; i++ )
		if( l->list[i] == v ) { l->list[i] = l->list[--l->num]; return; }
}

static void Dyn_Link( int bi )
{
	dynbrush_t *b = &brushes[bi];
	int c0[3], c1[3];
	Dyn_CellRange( b->mins, b->maxs, c0, c1 );
	for( int z = c0[2]; z <= c1[2]; z++ )
		for( int y = c0[1]; y <= c1[1]; y++ )
			for( int x = c0[0]; x <= c1[0]; x++ )
			{
				int ci = Dyn_CellIndex( x, y, z );
				Dyn_ListAdd( &gridlists[ci], bi );
				dw.cells[ci].dirty = true;	// faces of neighbours may hide/show
			}
	vec3_t c;
	VectorAverage( b->mins, b->maxs, c );
	int o[3], oo[3];
	Dyn_CellRange( c, c, o, oo );
	b->owner = Dyn_CellIndex( o[0], o[1], o[2] );
	dw.cells[b->owner].dirty = true;
}

static void Dyn_Unlink( int bi )
{
	dynbrush_t *b = &brushes[bi];
	int c0[3], c1[3];
	Dyn_CellRange( b->mins, b->maxs, c0, c1 );
	for( int z = c0[2]; z <= c1[2]; z++ )
		for( int y = c0[1]; y <= c1[1]; y++ )
			for( int x = c0[0]; x <= c1[0]; x++ )
			{
				int ci = Dyn_CellIndex( x, y, z );
				Dyn_ListRemove( &gridlists[ci], bi );
				dw.cells[ci].dirty = true;
			}
	dw.cells[b->owner].dirty = true;
}

static int Dyn_NewBrush( void )
{
	for( int i = 0; i < numbrushes; i++ )
		if( !brushes[i].alive ) return i;	// reuse a dead slot
	if( numbrushes >= maxbrushes )
	{
		int nmax = maxbrushes ? maxbrushes * 2 : 1024;
		dynbrush_t *n = Mem_Calloc( dynpool, sizeof( dynbrush_t ) * nmax );
		if( brushes ) { memcpy( n, brushes, sizeof( dynbrush_t ) * numbrushes ); Mem_Free( brushes ); }
		brushes = n;
		maxbrushes = nmax;
	}
	return numbrushes++;
}

//
// loading
//
void Dyn_Clear( void )
{
	if( dynpool )
		Mem_FreePool( &dynpool );
	brushes = NULL;
	gridlists = NULL;
	numbrushes = maxbrushes = 0;
	unsigned int rev = dw.world_rev;
	memset( &dw, 0, sizeof( dw ));
	dw.world_rev = rev + 1;
	numlights = 0;
	has_sun = false;
	dynmodel = NULL;
}

static const char *Dyn_Value( const char *ents, const char *start, const char *end, const char *key, char *out, size_t size )
{
	char pat[64];
	Q_snprintf( pat, sizeof( pat ), "\"%s\"", key );
	const char *p = Q_strstr( start, pat );
	if( !p || p > end )
		return NULL;
	p += Q_strlen( pat );
	while( *p == ' ' || *p == '\t' ) p++;
	if( *p != '"' ) return NULL;
	p++;
	size_t n = 0;
	while( *p && *p != '"' && n + 1 < size ) out[n++] = *p++;
	out[n] = 0;
	return out;
}

static void Dyn_ParseLights( const char *ents )
{
	const char *p = ents;
	char val[256];
	sky_color[0] = sky_color[1] = sky_color[2] = 0.35f;
	while( p && ( p = Q_strchr( p, '{' )))
	{
		const char *e = Q_strchr( p, '}' );
		if( !e ) break;
		char cls[64];
		if( Dyn_Value( ents, p, e, "classname", cls, sizeof( cls )))
		{
			if( !Q_strcmp( cls, "light_environment" ))
			{
				float r = 255, g = 255, b = 255, i = 200, pitch = -45, yaw = 0;
				vec3_t ang;
				if( Dyn_Value( ents, p, e, "_light", val, sizeof( val ))) sscanf( val, "%f %f %f %f", &r, &g, &b, &i );
				// the compilers fold "pitch" into "angles" (pitch yaw roll): an explicit pitch wins
				if( Dyn_Value( ents, p, e, "angles", val, sizeof( val ))) sscanf( val, "%f %f", &pitch, &yaw );
				if( Dyn_Value( ents, p, e, "pitch", val, sizeof( val ))) pitch = Q_atof( val );
				VectorSet( sun_color, r / 255.0f * i / 255.0f, g / 255.0f * i / 255.0f, b / 255.0f * i / 255.0f );
				// light "pitch" is negative downwards (opposite of view angles): the light travels along
				// AngleVectors( -pitch, yaw ); the sun is the other way
				VectorSet( ang, -pitch, yaw, 0 );
				AngleVectors( ang, sun_dir, NULL, NULL );
				VectorNegate( sun_dir, sun_dir );
				has_sun = true;
				if( Dyn_Value( ents, p, e, "_diffuse_light", val, sizeof( val )))
				{
					r = g = b = 128; i = 100;
					sscanf( val, "%f %f %f %f", &r, &g, &b, &i );
					VectorSet( sky_color, r / 255.0f * i / 255.0f, g / 255.0f * i / 255.0f, b / 255.0f * i / 255.0f );
				}
			}
			else if( !Q_strncmp( cls, "light", 5 ) && numlights < DYN_MAX_LIGHTS )
			{
				dynlight_t *l = &lights[numlights];
				float r = 255, g = 255, b = 255, i = 200;
				if( !Dyn_Value( ents, p, e, "origin", val, sizeof( val )))
				{
					p = e + 1;
					continue;
				}
				sscanf( val, "%f %f %f", &l->origin[0], &l->origin[1], &l->origin[2] );
				if( Dyn_Value( ents, p, e, "_light", val, sizeof( val ))) sscanf( val, "%f %f %f %f", &r, &g, &b, &i );
				VectorSet( l->color, r / 255.0f, g / 255.0f, b / 255.0f );
				l->intensity = i;
				numlights++;
			}
		}
		p = e + 1;
	}
}

static void Dyn_TexVecsForNormal( const vec3_t n, float vecs[2][4] )
{
	int ax = 0;
	if( fabs( n[1] ) > fabs( n[ax] )) ax = 1;
	if( fabs( n[2] ) > fabs( n[ax] )) ax = 2;
	memset( vecs, 0, sizeof( float ) * 8 );
	if( ax == 2 ) { vecs[0][0] = 1; vecs[1][1] = -1; }
	else if( ax == 0 ) { vecs[0][1] = 1; vecs[1][2] = -1; }
	else { vecs[0][0] = 1; vecs[1][2] = -1; }
}

// maps/<map>.dyn:
//   dynworld 1
//   tex <name> <width> <height>                 (index = order of appearance)
//   brush <material> <cut texture index> <numsides>
//   side <nx> <ny> <nz> <dist> <tex> <s0 s1 s2 s3> <t0 t1 t2 t3>
qboolean Dyn_LoadForMap( const char *mapname, model_t *world )
{
	char path[MAX_QPATH];
	Dyn_Clear();
	Q_snprintf( path, sizeof( path ), "maps/%s.dyn", mapname );
	byte *file = FS_LoadFile( path, NULL, false );
	if( !file )
		return false;

	dynpool = Mem_AllocPool( "Dynamic World" );
	dynmodel = world;
	char *p = (char *)file, *line;
	dynbrush_t *cur = NULL;
	int cursides = 0, count = 0;

	for( line = p; line && *line; line = p )
	{
		p = Q_strchr( line, '\n' );
		if( p ) *p++ = 0;
		if( !Q_strncmp( line, "tex ", 4 ) && dw.numtextures < DYN_MAX_TEXTURES )
		{
			char name[64];
			int w = 64, h = 64;
			if( sscanf( line + 4, "%63s %d %d", name, &w, &h ) >= 1 )
			{
				Q_strncpy( dw.texnames[dw.numtextures], name, sizeof( dw.texnames[0] ));
				texsize[dw.numtextures][0] = Q_max( w, 1 );
				texsize[dw.numtextures][1] = Q_max( h, 1 );
				dw.numtextures++;
			}
		}
		else if( !Q_strncmp( line, "brush ", 6 ))
		{
			int mat = 0, cut = 0, ns = 0;
			sscanf( line + 6, "%d %d %d", &mat, &cut, &ns );
			int bi = Dyn_NewBrush();
			cur = &brushes[bi];
			memset( cur, 0, sizeof( *cur ));
			cur->material = mat;
			cur->cuttex = cut;
			cursides = ns;
		}
		else if( !Q_strncmp( line, "side ", 5 ) && cur && cur->numsides < DYN_MAX_SIDES - 6 )
		{
			dynside_t *s = &cur->sides[cur->numsides];
			int tex = 0;
			sscanf( line + 5, "%f %f %f %f %d %f %f %f %f %f %f %f %f", &s->normal[0], &s->normal[1], &s->normal[2], &s->dist, &tex,
				&s->vecs[0][0], &s->vecs[0][1], &s->vecs[0][2], &s->vecs[0][3], &s->vecs[1][0], &s->vecs[1][1], &s->vecs[1][2], &s->vecs[1][3] );
			s->tex = (short)tex;
			cur->numsides++;
			if( cur->numsides == cursides )
			{
				if( Dyn_FinishBrush( cur ))
				{
					cur->alive = true;
					count++;
				}
				cur = NULL;
			}
		}
	}
	Mem_Free( file );

	if( !count )
	{
		Dyn_Clear();
		return false;
	}

	// grid over everything, with room to grow
	vec3_t mins, maxs;
	ClearBounds( mins, maxs );
	for( int i = 0; i < numbrushes; i++ )
		if( brushes[i].alive ) { AddPointToBounds( brushes[i].mins, mins, maxs ); AddPointToBounds( brushes[i].maxs, mins, maxs ); }
	for( int i = 0; i < 3; i++ )
	{
		dw.gridmins[i] = floor( mins[i] / DYN_CELL_SIZE ) * DYN_CELL_SIZE - DYN_CELL_SIZE;
		dw.gridsize[i] = (int)ceil(( maxs[i] - dw.gridmins[i] ) / DYN_CELL_SIZE ) + 1;
	}
	int ncells = dw.gridsize[0] * dw.gridsize[1] * dw.gridsize[2];
	Con_Printf( "Dyn_LoadForMap: %d brushes, bounds %.0f %.0f %.0f - %.0f %.0f %.0f, first brush %.0f %.0f %.0f - %.0f %.0f %.0f (%d sides)\n", count,
		mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], brushes[0].mins[0], brushes[0].mins[1], brushes[0].mins[2],
		brushes[0].maxs[0], brushes[0].maxs[1], brushes[0].maxs[2], brushes[0].numsides );
	if( ncells <= 0 || ncells > 4000000 )
	{
		Con_Printf( S_ERROR "Dyn_LoadForMap: bad grid %d x %d x %d\n", dw.gridsize[0], dw.gridsize[1], dw.gridsize[2] );
		Dyn_Clear();
		return false;
	}
	dw.cells = Mem_Calloc( dynpool, sizeof( dyncell_t ) * ncells );
	gridlists = Mem_Calloc( dynpool, sizeof( dynlist_t ) * ncells );
	for( int z = 0; z < dw.gridsize[2]; z++ )
		for( int y = 0; y < dw.gridsize[1]; y++ )
			for( int x = 0; x < dw.gridsize[0]; x++ )
			{
				dyncell_t *c = &dw.cells[Dyn_CellIndex( x, y, z )];
				VectorSet( c->mins, dw.gridmins[0] + x * DYN_CELL_SIZE, dw.gridmins[1] + y * DYN_CELL_SIZE, dw.gridmins[2] + z * DYN_CELL_SIZE );
				VectorSet( c->maxs, c->mins[0] + DYN_CELL_SIZE, c->mins[1] + DYN_CELL_SIZE, c->mins[2] + DYN_CELL_SIZE );
			}
	for( int i = 0; i < numbrushes; i++ )
		if( brushes[i].alive )
		{
			Dyn_AddBevels( &brushes[i] );
			Dyn_Link( i );
		}

	if( world && world->entities )
		Dyn_ParseLights( world->entities );
	dw.active = true;
	Con_Reportf( "Dyn_LoadForMap: %d brushes, %d textures, %d lights, grid %dx%dx%d\n", count, dw.numtextures, numlights,
		dw.gridsize[0], dw.gridsize[1], dw.gridsize[2] );
	return true;
}

//
// collision
//
typedef struct
{
	vec3_t	start, end, mins, maxs;
	float	fraction;
	vec3_t	normal;
	int	brush;
	qboolean startsolid, allsolid;
	qboolean point;
} dyntw_t;

static void Dyn_TraceThroughBrush( dyntw_t *tw, int bi )
{
	const dynbrush_t *b = &brushes[bi];
	float enter = -1.0f, leave = 1.0f;
	qboolean startout = false, getout = false;
	const dynside_t *clip = NULL;

	for( int i = 0; i < b->numsides; i++ )
	{
		const dynside_t *s = &b->sides[i];
		float dist = s->dist;
		if( !tw->point )
		{
			vec3_t ofs;
			for( int j = 0; j < 3; j++ )
				ofs[j] = s->normal[j] < 0 ? tw->maxs[j] : tw->mins[j];
			dist -= DotProduct( ofs, s->normal );
		}
		float d1 = DotProduct( tw->start, s->normal ) - dist;
		float d2 = DotProduct( tw->end, s->normal ) - dist;
		if( d2 > 0 ) getout = true;
		if( d1 > 0 ) startout = true;
		if( d1 > 0 && ( d2 >= DYN_CLIP_EPS || d2 >= d1 ))
			return;	// completely in front of this face
		if( d1 <= 0 && d2 <= 0 )
			continue;
		if( d1 > d2 )
		{
			float f = ( d1 - DYN_CLIP_EPS ) / ( d1 - d2 );
			if( f < 0 ) f = 0;
			if( f > enter ) { enter = f; clip = s; }
		}
		else
		{
			float f = ( d1 + DYN_CLIP_EPS ) / ( d1 - d2 );
			if( f > 1 ) f = 1;
			if( f < leave ) leave = f;
		}
	}

	if( !startout )
	{
		tw->startsolid = true;
		if( !getout ) { tw->allsolid = true; tw->fraction = 0; tw->brush = bi; }
		return;
	}
	if( enter < leave && enter > -1 && enter < tw->fraction && clip )
	{
		tw->fraction = Q_max( enter, 0.0f );
		VectorCopy( clip->normal, tw->normal );
		tw->brush = bi;
	}
}

static void Dyn_Trace( dyntw_t *tw )
{
	vec3_t lo, hi, delta;
	float len, step, f;
	int c0[3], c1[3];

	tw->fraction = 1.0f;
	tw->brush = -1;
	tw->startsolid = tw->allsolid = false;
	if( !dw.active )
		return;
	tw->point = VectorIsNull( tw->mins ) && VectorIsNull( tw->maxs );
	checkcount++;

	VectorSubtract( tw->end, tw->start, delta );
	len = VectorLength( delta );
	step = len > DYN_CELL_SIZE ? DYN_CELL_SIZE / len : 1.0f;
	for( f = 0.0f; f < 1.0f; f += step )
	{
		float f1 = Q_min( f + step, 1.0f );
		if( tw->fraction < f )
			break;
		for( int i = 0; i < 3; i++ )
		{
			float a = tw->start[i] + delta[i] * f, b = tw->start[i] + delta[i] * f1;
			lo[i] = Q_min( a, b ) + tw->mins[i] - 1;
			hi[i] = Q_max( a, b ) + tw->maxs[i] + 1;
		}
		Dyn_CellRange( lo, hi, c0, c1 );
		for( int z = c0[2]; z <= c1[2]; z++ )
			for( int y = c0[1]; y <= c1[1]; y++ )
				for( int x = c0[0]; x <= c1[0]; x++ )
				{
					dynlist_t *l = &gridlists[Dyn_CellIndex( x, y, z )];
					for( int k = 0; k < l->num; k++ )
					{
						dynbrush_t *b = &brushes[l->list[k]];
						if( b->check == checkcount )
							continue;
						b->check = checkcount;
						if( b->maxs[0] < lo[0] || b->mins[0] > hi[0] || b->maxs[1] < lo[1] || b->mins[1] > hi[1] || b->maxs[2] < lo[2] || b->mins[2] > hi[2] )
						{
							b->check = 0;	// may still matter for a later piece of a long trace
							continue;
						}
						Dyn_TraceThroughBrush( tw, l->list[k] );
					}
				}
	}
}

void Dyn_ClipTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *trace, edict_t *world )
{
	dyntw_t tw;
	if( !dw.active ) return;
	VectorCopy( start, tw.start ); VectorCopy( end, tw.end ); VectorCopy( mins, tw.mins ); VectorCopy( maxs, tw.maxs );
	Dyn_Trace( &tw );
	if( tw.startsolid )
	{
		trace->startsolid = true;
		if( tw.allsolid ) { trace->allsolid = true; trace->fraction = 0; VectorCopy( start, trace->endpos ); trace->ent = world; }
	}
	if( tw.brush >= 0 && tw.fraction < trace->fraction )
	{
		trace->fraction = tw.fraction;
		VectorLerp( start, tw.fraction, end, trace->endpos );
		VectorCopy( tw.normal, trace->plane.normal );
		trace->plane.dist = DotProduct( trace->endpos, tw.normal );
		trace->ent = world;
	}
}

void Dyn_ClipPMTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, pmtrace_t *trace )
{
	dyntw_t tw;
	if( !dw.active ) return;
	VectorCopy( start, tw.start ); VectorCopy( end, tw.end ); VectorCopy( mins, tw.mins ); VectorCopy( maxs, tw.maxs );
	Dyn_Trace( &tw );
	if( tw.startsolid )
	{
		trace->startsolid = true;
		if( tw.allsolid ) { trace->allsolid = true; trace->fraction = 0; VectorCopy( start, trace->endpos ); }
	}
	if( tw.brush >= 0 && tw.fraction < trace->fraction )
	{
		trace->fraction = tw.fraction;
		VectorLerp( start, tw.fraction, end, trace->endpos );
		VectorCopy( tw.normal, trace->plane.normal );
		trace->plane.dist = DotProduct( trace->endpos, tw.normal );
	}
}

qboolean Dyn_BoxSolid( const vec3_t absmin, const vec3_t absmax )
{
	dyntw_t tw;
	if( !dw.active ) return false;
	VectorAverage( absmin, absmax, tw.start );
	VectorCopy( tw.start, tw.end );
	VectorSubtract( absmin, tw.start, tw.mins );
	VectorSubtract( absmax, tw.start, tw.maxs );
	Dyn_Trace( &tw );
	return tw.startsolid;
}

// a point on a brush's surface counts as inside it: where two brushes meet (the ground's triangles share their
// diagonals) there is no gap to see or light through
qboolean Dyn_PointSolid( const vec3_t p )
{
	if( !dw.active ) return false;
	int c0[3], c1[3];
	Dyn_CellRange( p, p, c0, c1 );
	dynlist_t *l = &gridlists[Dyn_CellIndex( c0[0], c0[1], c0[2] )];
	for( int k = 0; k < l->num; k++ )
	{
		const dynbrush_t *b = &brushes[l->list[k]];
		int s;
		for( s = 0; s < b->numsides; s++ )
			if( DotProduct( p, b->sides[s].normal ) - b->sides[s].dist > DYN_EPS )
				break;
		if( s == b->numsides )
			return true;
	}
	return false;
}

//
// carving
//
static float Dyn_BrushVolume( const dynbrush_t *b )
{
	// sum of pyramids from the first vertex
	dynwinding_t w;
	vec3_t corner;
	qboolean have = false;
	float vol = 0;
	for( int s = 0; s < b->numsides; s++ )
	{
		if( b->sides[s].tex == -1 && 0 ) continue;
		if( !Dyn_SideWinding( b, s, &w )) continue;
		if( !have ) { VectorCopy( w.p[0], corner ); have = true; }
		float d = -( DotProduct( corner, b->sides[s].normal ) - b->sides[s].dist );
		float area = 0;
		for( int i = 2; i < w.num; i++ )
		{
			vec3_t e1, e2, c;
			VectorSubtract( w.p[i - 1], w.p[0], e1 );
			VectorSubtract( w.p[i], w.p[0], e2 );
			CrossProduct( e1, e2, c );
			area += VectorLength( c ) * 0.5f;
		}
		vol += d * area / 3.0f;
	}
	return vol;
}

static void Dyn_RemoveRedundantSides( dynbrush_t *b )
{
	dynwinding_t w;

	// coplanar duplicates first (carving next to an earlier hole re-adds the same planes)
	for( int s = 0; s < b->numsides; s++ )
		for( int t = s + 1; t < b->numsides; )
		{
			if( fabs( b->sides[s].dist - b->sides[t].dist ) < 0.01f && DotProduct( b->sides[s].normal, b->sides[t].normal ) > 0.9999f )
				b->sides[t] = b->sides[--b->numsides];
			else
				t++;
		}

	for( int s = 0; s < b->numsides; )
	{
		if( b->sides[s].tex != -1 && !Dyn_SideWinding( b, s, &w ))
			b->sides[s] = b->sides[--b->numsides];
		else
			s++;
	}
}

// how much diggable material is inside a box (per material too), without changing anything
float Dyn_BoxVolume( const vec3_t mins, const vec3_t maxs, float *volume, int maxmaterials )
{
	float total = 0;
	if( !dw.active ) return 0;
	if( volume ) memset( volume, 0, sizeof( float ) * maxmaterials );

	int c0[3], c1[3];
	Dyn_CellRange( mins, maxs, c0, c1 );
	checkcount++;
	for( int z = c0[2]; z <= c1[2]; z++ )
		for( int y = c0[1]; y <= c1[1]; y++ )
			for( int x = c0[0]; x <= c1[0]; x++ )
			{
				dynlist_t *l = &gridlists[Dyn_CellIndex( x, y, z )];
				for( int k = 0; k < l->num; k++ )
				{
					dynbrush_t *b = &brushes[l->list[k]];
					if( b->check == checkcount ) continue;
					b->check = checkcount;
					if( b->maxs[0] <= mins[0] || b->mins[0] >= maxs[0] || b->maxs[1] <= mins[1] || b->mins[1] >= maxs[1] || b->maxs[2] <= mins[2] || b->mins[2] >= maxs[2] )
						continue;
					dynbrush_t inside = *b;
					for( int s = 0; s < inside.numsides; )
						if( inside.sides[s].tex == -1 ) inside.sides[s] = inside.sides[--inside.numsides]; else s++;
					for( int axis = 0; axis < 3 && inside.numsides < DYN_MAX_SIDES; axis++ )
						for( int dir = -1; dir <= 1 && inside.numsides < DYN_MAX_SIDES; dir += 2 )
						{
							dynside_t *s = &inside.sides[inside.numsides++];
							memset( s, 0, sizeof( *s ));
							s->normal[axis] = (float)dir;
							s->dist = dir > 0 ? maxs[axis] : -mins[axis];
							s->tex = inside.cuttex;
						}
					// empty intersection first: cleaning up an empty brush would leave a bogus one
					if( !Dyn_FinishBrush( &inside ))
						continue;
					Dyn_RemoveRedundantSides( &inside );
					if( !Dyn_FinishBrush( &inside ))
						continue;
					float v = Dyn_BrushVolume( &inside );
					total += v;
					if( volume && inside.material >= 0 && inside.material < maxmaterials )
						volume[inside.material] += v;
				}
			}
	return total;
}

// a carve lets light through: faces whose sun or sky rays pass the dug box see more sky now. They lie below it
// and down-sun of it: rebuild (relight) the cells the box swept down those rays touches
static void Dyn_DirtyLightAround( const vec3_t mins, const vec3_t maxs )
{
	vec3_t lo, hi, p;
	int c0[3], c1[3];
	static const float sweeps[6][4] = { { 0, 0, -1, 400 }, { 0.7f, 0, -0.7f, 256 }, { -0.7f, 0, -0.7f, 256 },
		{ 0, 0.7f, -0.7f, 256 }, { 0, -0.7f, -0.7f, 256 }, { 0, 0, 0, 0 } };

	for( int i = 0; i < 3; i++ )
	{
		lo[i] = mins[i] - 64.0f;
		hi[i] = maxs[i] + 64.0f;
	}
	for( int k = 0; k < 6; k++ )
	{
		vec3_t dir = { sweeps[k][0], sweeps[k][1], sweeps[k][2] };
		float len = sweeps[k][3];
		if( k == 5 )
		{
			if( !has_sun )
				continue;
			VectorNegate( sun_dir, dir );	// down the sun's rays
			len = 640.0f;
		}
		VectorMA( mins, len, dir, p ); AddPointToBounds( p, lo, hi );
		VectorMA( maxs, len, dir, p ); AddPointToBounds( p, lo, hi );
	}
	Dyn_CellRange( lo, hi, c0, c1 );
	for( int z = c0[2]; z <= c1[2]; z++ )
		for( int y = c0[1]; y <= c1[1]; y++ )
			for( int x = c0[0]; x <= c1[0]; x++ )
				dw.cells[Dyn_CellIndex( x, y, z )].dirty = true;
}

float Dyn_CarveBox( const vec3_t mins, const vec3_t maxs, float *volume, int maxmaterials )
{
	float total = 0;
	if( !dw.active ) return 0;
	if( volume ) memset( volume, 0, sizeof( float ) * maxmaterials );

	int c0[3], c1[3];
	Dyn_CellRange( mins, maxs, c0, c1 );
	// collect the brushes first: the lists change while carving
	int cand[512], ncand = 0;
	checkcount++;
	for( int z = c0[2]; z <= c1[2]; z++ )
		for( int y = c0[1]; y <= c1[1]; y++ )
			for( int x = c0[0]; x <= c1[0]; x++ )
			{
				dynlist_t *l = &gridlists[Dyn_CellIndex( x, y, z )];
				for( int k = 0; k < l->num && ncand < 512; k++ )
				{
					dynbrush_t *b = &brushes[l->list[k]];
					if( b->check == checkcount ) continue;
					b->check = checkcount;
					if( b->maxs[0] <= mins[0] || b->mins[0] >= maxs[0] || b->maxs[1] <= mins[1] || b->mins[1] >= maxs[1] || b->maxs[2] <= mins[2] || b->mins[2] >= maxs[2] )
						continue;
					cand[ncand++] = l->list[k];
				}
			}

	for( int c = 0; c < ncand; c++ )
	{
		int bi = cand[c];
		dynbrush_t orig = brushes[bi];	// copy: the array may be reallocated
		dynbrush_t inside = orig;
		qboolean touched = false;

		// strip bevels: they are recomputed for each piece
		for( int s = 0; s < inside.numsides; )
			if( inside.sides[s].tex == -1 ) inside.sides[s] = inside.sides[--inside.numsides]; else s++;

		Dyn_Unlink( bi );
		brushes[bi].alive = false;

		for( int axis = 0; axis < 3; axis++ )
			for( int dir = -1; dir <= 1; dir += 2 )
			{
				// the carve box's face plane, outward
				vec3_t n = { 0, 0, 0 };
				n[axis] = (float)dir;
				float d = dir > 0 ? maxs[axis] : -mins[axis];

				// the piece outside this plane is kept
				dynbrush_t piece = inside;
				if( piece.numsides < DYN_MAX_SIDES - 7 )
				{
					dynside_t *s = &piece.sides[piece.numsides++];
					VectorNegate( n, s->normal );
					s->dist = -d;
					s->tex = inside.cuttex;
					Dyn_TexVecsForNormal( s->normal, s->vecs );
					if( Dyn_FinishBrush( &piece ))
					{
						Dyn_RemoveRedundantSides( &piece );
						Dyn_FinishBrush( &piece );
						int ni = Dyn_NewBrush();
						brushes[ni] = piece;
						brushes[ni].alive = true;
						Dyn_AddBevels( &brushes[ni] );
						Dyn_Link( ni );
					}
				}
				// and carving continues with the part inside it
				if( inside.numsides < DYN_MAX_SIDES - 7 )
				{
					dynside_t *s = &inside.sides[inside.numsides++];
					VectorCopy( n, s->normal );
					s->dist = d;
					s->tex = inside.cuttex;
					Dyn_TexVecsForNormal( s->normal, s->vecs );
				}
				touched = true;
			}

		carves++;
		if( touched && Dyn_FinishBrush( &inside ))
		{
			Dyn_RemoveRedundantSides( &inside );
			Dyn_FinishBrush( &inside );
			float v = Dyn_BrushVolume( &inside );
			total += v;
			if( volume && inside.material >= 0 && inside.material < maxmaterials )
				volume[inside.material] += v;
		}
	}
	if( total > 0.0f )
	{
		float *log = dw.carvelog[dw.numcarves++ % DYN_CARVE_LOG];
		VectorCopy( mins, log );
		VectorCopy( maxs, log + 3 );
		Dyn_DirtyLightAround( mins, maxs );
	}
	return total;
}

//
// lighting and meshes
//
static qboolean Dyn_LineBlocked( const vec3_t a, const vec3_t b )
{
	vec3_t dynend;
	VectorCopy( b, dynend );
	if( dynmodel )
	{
		pmtrace_t tr;
		memset( &tr, 0, sizeof( tr ));
		tr.fraction = 1.0f;
		tr.allsolid = true;
		VectorCopy( b, tr.endpos );
		vec3_t p1, p2;
		VectorCopy( a, p1 ); VectorCopy( b, p2 );
		PM_RecursiveHullCheck( &dynmodel->hulls[0], dynmodel->hulls[0].firstclipnode, 0, 1, p1, p2, &tr );
		if( tr.startsolid )
			return true;
		if( tr.fraction < 1.0f )
		{
			// the void behind the sky box is solid too: a ray that got into the sky reached it
			vec3_t dir, back;
			VectorSubtract( p2, p1, dir );
			VectorNormalize( dir );
			VectorMA( tr.endpos, -2.0f, dir, back );
			if( PM_HullPointContents( &dynmodel->hulls[0], dynmodel->hulls[0].firstclipnode, back ) != CONTENTS_SKY )
				return true;
			VectorCopy( tr.endpos, dynend );	// sky: only the diggable geometry before it can still be in the way
		}
	}
	dyntw_t tw;
	VectorCopy( a, tw.start ); VectorCopy( dynend, tw.end );
	VectorClear( tw.mins ); VectorClear( tw.maxs );
	Dyn_Trace( &tw );
	return tw.brush >= 0;
}

static void Dyn_LightPoint( const vec3_t p, const vec3_t n, byte *rgba )
{
	vec3_t c = { 0, 0, 0 }, start, end, dir;

	// bounce light: nothing the player can see is ever pitch black
	VectorScale( sky_color, 0.55f, c );

	// rays leave a little off the surface; a vertex tucked into a corner gets only the bounce light
	VectorMA( p, 4.0f, n, start );
	if( Dyn_PointSolid( start ))
	{
		VectorMA( p, 10.0f, n, start );
		if( Dyn_PointSolid( start ))
		{
			for( int i = 0; i < 3; i++ )
				rgba[i] = (byte)bound( 40.0f, c[i] * 200.0f, 255.0f );
			rgba[3] = 255;
			return;
		}
	}

	// sky light: fraction of a few upward rays that reach the sky
	static const float skydirs[5][3] = { { 0, 0, 1 }, { 0.7f, 0, 0.7f }, { -0.7f, 0, 0.7f }, { 0, 0.7f, 0.7f }, { 0, -0.7f, 0.7f }};
	float vis = 0;
	for( int i = 0; i < 5; i++ )
	{
		VectorMA( start, 2048.0f, skydirs[i], end );
		if( !Dyn_LineBlocked( start, end ))
			vis += 0.2f * ( 0.6f + 0.4f * Q_max( 0.0f, DotProduct( n, skydirs[i] )));
	}
	VectorMA( c, vis * 2.2f, sky_color, c );

	// sun: four slightly spread rays, so shadow edges are soft instead of following the triangles
	if( has_sun )
	{
		float lambert = DotProduct( n, sun_dir );
		if( lambert > 0 )
		{
			vec3_t right, up, d;
			static const float jitter[4][2] = { { 0.03f, 0.03f }, { -0.03f, 0.03f }, { 0.03f, -0.03f }, { -0.03f, -0.03f }};
			float lit = 0;
			vec3_t axis = { 0, 0, 1 };
			if( fabs( sun_dir[2] ) > 0.9f ) VectorSet( axis, 1, 0, 0 );
			CrossProduct( sun_dir, axis, right );
			VectorNormalize( right );
			CrossProduct( sun_dir, right, up );
			for( int i = 0; i < 4; i++ )
			{
				VectorMA( sun_dir, jitter[i][0], right, d );
				VectorMA( d, jitter[i][1], up, d );
				VectorMA( start, 4096.0f, d, end );
				if( !Dyn_LineBlocked( start, end ))
					lit += 0.25f;
			}
			VectorMA( c, lit * lambert * 0.85f, sun_color, c );
		}
	}

	for( int i = 0; i < numlights; i++ )
	{
		VectorSubtract( lights[i].origin, p, dir );
		float dist = VectorNormalizeLength( dir );
		float lambert = DotProduct( n, dir );
		if( lambert <= 0 || dist > lights[i].intensity * 4.0f )
			continue;
		if( Dyn_LineBlocked( start, lights[i].origin ))
			continue;
		float add = lights[i].intensity / Q_max( dist, 24.0f ) * lambert * 0.5f;
		VectorMA( c, Q_min( add, 1.5f ), lights[i].color, c );
	}

	for( int i = 0; i < 3; i++ )
		rgba[i] = (byte)bound( 40.0f, c[i] * 200.0f, 255.0f );
	rgba[3] = 255;
}

// lightmaps
#define DYN_LUXEL		16.0f	// world units per lightmap texel, Half-Life's
#define DYN_LM_WIDTH		256	// a cell's lightmap atlas
#define DYN_LM_MAX		1.3f	// brightest light kept (1 = the texture as painted)

typedef struct
{
	vec3_t	base;		// bounce, sky and lamp light
	vec3_t	pos;		// the point on the face it was sampled at
	float	sun, sunaa;	// sun reaching it: the middle ray, and averaged over the texel at shadow edges
	int	valid;		// false: inside other geometry (filled from the neighbours)
} dynluxel_t;

typedef struct
{
	int		first, count;	// the face's triangles in the build
	const dynside_t	*side;
	vec3_t		u, v;		// lightmap axes on the face's plane
	float		s0, t0;		// the first texel's place on them
	int		w, h, x, y;	// size, and where in the atlas
	int		npts;
	float		pts[DYN_MAX_WINDING][2];	// the whole face's outline on the axes
	dynluxel_t	*lux;
	vec3_t		*col;
} dynlmgroup_t;

typedef struct
{
	int		num, max;
	dynvert_t	*v;
	int		*tex;
	int		cell, cellc[3];	// the cell being built: it gets the triangles whose middles are inside it
	dynlmgroup_t	*groups;
	int		numgroups, maxgroups;
} dynbuild_t;

// triangles share corners: light each (position, normal) once per cell build
#define DYN_LIGHTCACHE	16384
static struct { int key[4]; byte rgba[4]; qboolean used; } lightcache[DYN_LIGHTCACHE];

static void Dyn_ClearLightCache( void )
{
	for( int i = 0; i < DYN_LIGHTCACHE; i++ )
		lightcache[i].used = false;
}

static void Dyn_CachedLight( const vec3_t p, const vec3_t n, byte *rgba )
{
	int key[4] = { (int)floor( p[0] * 4 + 0.5f ), (int)floor( p[1] * 4 + 0.5f ), (int)floor( p[2] * 4 + 0.5f ),
		(int)floor( n[0] * 50 + 50.5f ) * 10201 + (int)floor( n[1] * 50 + 50.5f ) * 101 + (int)floor( n[2] * 50 + 50.5f ) };
	unsigned int h = (unsigned int)( key[0] * 73856093 ^ key[1] * 19349663 ^ key[2] * 83492791 ^ key[3] * 2654435761u );
	for( int probe = 0; probe < 16; probe++ )
	{
		int i = ( h + probe ) & ( DYN_LIGHTCACHE - 1 );
		if( !lightcache[i].used )
		{
			Dyn_LightPoint( p, n, rgba );
			memcpy( lightcache[i].key, key, sizeof( key ));
			memcpy( lightcache[i].rgba, rgba, 4 );
			lightcache[i].used = true;
			return;
		}
		if( !memcmp( lightcache[i].key, key, sizeof( key )))
		{
			memcpy( rgba, lightcache[i].rgba, 4 );
			return;
		}
	}
	Dyn_LightPoint( p, n, rgba );
}

// light for a model standing at p (sky, sun and lamps, regardless of facing) and the way it travels;
// cached per 16 units until the geometry changes
void Dyn_ModelLight( const float *p, byte *rgb, float *lightdir )
{
	static struct { int key[3]; unsigned int rev; byte rgb[3]; vec3_t dir; qboolean used; } cache[1024];
	int key[3] = { (int)floor( p[0] / 16.0f ), (int)floor( p[1] / 16.0f ), (int)floor( p[2] / 16.0f ) };
	unsigned int h = (unsigned int)( key[0] * 73856093 ^ key[1] * 19349663 ^ key[2] * 83492791 ) & 1023;

	unsigned int rev = dw.world_rev * 65536u + carves;
	if( cache[h].used && cache[h].rev == rev && !memcmp( cache[h].key, key, sizeof( key )))
	{
		memcpy( rgb, cache[h].rgb, 3 );
		VectorCopy( cache[h].dir, lightdir );
		return;
	}

	vec3_t c, start, end, dir;
	VectorScale( sky_color, 0.55f, c );
	VectorCopy( p, start );
	VectorSet( lightdir, 0.0f, 0.0f, -1.0f );

	if( !Dyn_PointSolid( start ))
	{
		static const float skydirs[5][3] = { { 0, 0, 1 }, { 0.7f, 0, 0.7f }, { -0.7f, 0, 0.7f }, { 0, 0.7f, 0.7f }, { 0, -0.7f, 0.7f }};
		float vis = 0;
		for( int i = 0; i < 5; i++ )
		{
			VectorMA( start, 2048.0f, skydirs[i], end );
			if( !Dyn_LineBlocked( start, end ))
				vis += 0.2f;
		}
		VectorMA( c, vis * 1.6f, sky_color, c );

		if( has_sun )
		{
			static const float jitter[4][2] = { { 0.03f, 0.03f }, { -0.03f, 0.03f }, { 0.03f, -0.03f }, { -0.03f, -0.03f }};
			vec3_t right, up, d, axis = { 0, 0, 1 };
			float lit = 0;
			if( fabs( sun_dir[2] ) > 0.9f ) VectorSet( axis, 1, 0, 0 );
			CrossProduct( sun_dir, axis, right );
			VectorNormalize( right );
			CrossProduct( sun_dir, right, up );
			for( int i = 0; i < 4; i++ )
			{
				VectorMA( sun_dir, jitter[i][0], right, d );
				VectorMA( d, jitter[i][1], up, d );
				VectorMA( start, 4096.0f, d, end );
				if( !Dyn_LineBlocked( start, end ))
					lit += 0.25f;
			}
			VectorMA( c, lit * 0.7f, sun_color, c );
			if( lit > 0 )
				VectorNegate( sun_dir, lightdir );
		}

		for( int i = 0; i < numlights; i++ )
		{
			VectorSubtract( lights[i].origin, p, dir );
			float dist = VectorNormalizeLength( dir );
			if( dist > lights[i].intensity * 4.0f || Dyn_LineBlocked( start, lights[i].origin ))
				continue;
			VectorMA( c, Q_min( lights[i].intensity / Q_max( dist, 24.0f ) * 0.4f, 1.2f ), lights[i].color, c );
		}
	}

	for( int i = 0; i < 3; i++ )
		rgb[i] = (byte)bound( 30.0f, c[i] * 200.0f, 255.0f );

	memcpy( cache[h].key, key, sizeof( key ));
	cache[h].rev = rev;
	memcpy( cache[h].rgb, rgb, 3 );
	VectorCopy( lightdir, cache[h].dir );
	cache[h].used = true;
}

// a face polygon is hidden when a neighbouring brush covers all of it: its middle and (nearly) its corners,
// tested just in front of the face
static qboolean Dyn_Covered( const vec3_t *pts, int num, const vec3_t normal )
{
	vec3_t mid = { 0, 0, 0 }, q;
	for( int i = 0; i < num; i++ ) VectorAdd( mid, pts[i], mid );
	VectorScale( mid, 1.0f / num, mid );
	VectorMA( mid, 0.5f, normal, q );
	if( !Dyn_PointSolid( q ))
		return false;
	for( int i = 0; i < num; i++ )
	{
		VectorLerp( pts[i], 0.1f, mid, q );
		VectorMA( q, 0.5f, normal, q );
		if( !Dyn_PointSolid( q ))
			return false;
	}
	return true;
}

static void Dyn_Emit( dynbuild_t *bld, const vec3_t a, const vec3_t b, const vec3_t c, const dynside_t *s )
{
	if( bld->num + 3 > bld->max )
	{
		int nmax = bld->max ? bld->max * 2 : 4096;
		dynvert_t *nv = Mem_Malloc( dynpool, sizeof( dynvert_t ) * nmax );
		int *nt = Mem_Malloc( dynpool, sizeof( int ) * nmax );
		if( bld->v ) { memcpy( nv, bld->v, sizeof( dynvert_t ) * bld->num ); memcpy( nt, bld->tex, sizeof( int ) * bld->num ); Mem_Free( bld->v ); Mem_Free( bld->tex ); }
		bld->v = nv; bld->tex = nt; bld->max = nmax;
	}
	const float *pts[3] = { a, b, c };
	int tw = texsize[s->tex][0], th = texsize[s->tex][1];
	for( int i = 0; i < 3; i++ )
	{
		dynvert_t *v = &bld->v[bld->num];
		VectorCopy( pts[i], v->xyz );
		v->st[0] = ( DotProduct( pts[i], s->vecs[0] ) + s->vecs[0][3] ) / tw;
		v->st[1] = ( DotProduct( pts[i], s->vecs[1] ) + s->vecs[1][3] ) / th;
		bld->tex[bld->num] = s->tex;
		bld->num++;
	}
}

#define DYN_PIECE	40.0f	// faces are cut along the block grid (VOX_BLOCK_SIZE)

// a piece within one block cell: drawn by the render cell holding its middle, unless something covers it
static void Dyn_EmitPiece( dynbuild_t *bld, const dynwinding_t *w, const dynside_t *s )
{
	vec3_t mid = { 0, 0, 0 };
	int c0[3], c1[3];
	for( int i = 0; i < w->num; i++ )
		VectorAdd( mid, w->p[i], mid );
	VectorScale( mid, 1.0f / w->num, mid );
	Dyn_CellRange( mid, mid, c0, c1 );
	if( Dyn_CellIndex( c0[0], c0[1], c0[2] ) != bld->cell )
		return;
	if( Dyn_Covered( (const vec3_t *)w->p, w->num, s->normal ))
		return;
	for( int i = 2; i < w->num; i++ )
		Dyn_Emit( bld, w->p[0], w->p[i - 1], w->p[i], s );
}

static void Dyn_SplitFace( dynbuild_t *bld, const dynwinding_t *w, const dynside_t *s, int axis )
{
	for( ; axis < 3; axis++ )
	{
		float lo = 1e30f, hi = -1e30f;
		for( int i = 0; i < w->num; i++ )
		{
			lo = Q_min( lo, w->p[i][axis] );
			hi = Q_max( hi, w->p[i][axis] );
		}
		float plane = ( floor(( lo + 0.05f ) / DYN_PIECE ) + 1.0f ) * DYN_PIECE;
		if( plane >= hi - 0.05f )
			continue;
		dynwinding_t below = *w, above = *w;
		vec3_t n = { 0, 0, 0 };
		n[axis] = 1.0f;
		Dyn_ChopWinding( &below, n, plane );
		n[axis] = -1.0f;
		Dyn_ChopWinding( &above, n, -plane );
		if( below.num >= 3 )
			Dyn_SplitFace( bld, &below, s, axis + 1 );	// one block slab along this axis now
		if( above.num >= 3 )
			Dyn_SplitFace( bld, &above, s, axis );
		return;
	}
	Dyn_EmitPiece( bld, w, s );
}

static void Dyn_FaceAxes( const vec3_t n, vec3_t u, vec3_t v )
{
	vec3_t a = { 0, 0, 1 };
	if( fabs( n[2] ) > 0.9f )
		VectorSet( a, 1, 0, 0 );
	CrossProduct( a, n, u );
	VectorNormalize( u );
	CrossProduct( n, u, v );
}

static qboolean Dyn_LightStartBad( const vec3_t p )
{
	if( Dyn_PointSolid( p ))
		return true;
	return dynmodel && PM_HullPointContents( &dynmodel->hulls[0], dynmodel->hulls[0].firstclipnode, (float *)p ) == CONTENTS_SOLID;
}

static float Dyn_SunRay( const vec3_t start )
{
	vec3_t end;
	VectorMA( start, 4096.0f, sun_dir, end );
	return Dyn_LineBlocked( start, end ) ? 0.0f : 1.0f;
}

// everything but the sun's strength: bounce, sky, lamps; the sun's middle ray
static void Dyn_LuxelSample( dynluxel_t *l, const vec3_t p, const vec3_t n )
{
	static const float skydirs[5][3] = { { 0, 0, 1 }, { 0.7f, 0, 0.7f }, { -0.7f, 0, 0.7f }, { 0, 0.7f, 0.7f }, { 0, -0.7f, 0.7f }};
	vec3_t start, end, dir;
	float vis = 0;

	VectorCopy( p, l->pos );
	VectorScale( sky_color, 0.55f, l->base );	// bounce light: nothing the player can see is ever pitch black
	l->sun = l->sunaa = 0.0f;
	VectorMA( p, 2.0f, n, start );
	l->valid = !Dyn_LightStartBad( start );
	if( !l->valid )
		return;

	for( int i = 0; i < 5; i++ )
	{
		VectorMA( start, 2048.0f, skydirs[i], end );
		if( !Dyn_LineBlocked( start, end ))
			vis += 0.2f * ( 0.6f + 0.4f * Q_max( 0.0f, DotProduct( n, skydirs[i] )));
	}
	VectorMA( l->base, vis * 2.2f, sky_color, l->base );

	if( has_sun && DotProduct( n, sun_dir ) > 0 )
		l->sun = l->sunaa = Dyn_SunRay( start );

	for( int i = 0; i < numlights; i++ )
	{
		VectorSubtract( lights[i].origin, p, dir );
		float dist = VectorNormalizeLength( dir );
		float lambert = DotProduct( n, dir );
		if( lambert <= 0 || dist > lights[i].intensity * 4.0f )
			continue;
		if( Dyn_LineBlocked( start, lights[i].origin ))
			continue;
		float add = lights[i].intensity / Q_max( dist, 24.0f ) * lambert * 0.5f;
		VectorMA( l->base, Q_min( add, 1.5f ), lights[i].color, l->base );
	}
}

// a point of the face's plane pulled inside its outline (texels at the edges light what is just inside)
static void Dyn_ClampToFace( const dynlmgroup_t *g, float *s, float *t )
{
	float area = 0, cs = 0, ct = 0;
	for( int i = 0; i < g->npts; i++ )
	{
		const float *a = g->pts[i], *b = g->pts[( i + 1 ) % g->npts];
		area += a[0] * b[1] - a[1] * b[0];
		cs += a[0]; ct += a[1];
	}
	cs /= g->npts; ct /= g->npts;
	float sign = area >= 0 ? 1.0f : -1.0f, best = 1e30f, bs = *s, bt = *t;
	qboolean inside = true;
	for( int i = 0; i < g->npts; i++ )
	{
		const float *a = g->pts[i], *b = g->pts[( i + 1 ) % g->npts];
		float ex = b[0] - a[0], ey = b[1] - a[1], px = *s - a[0], py = *t - a[1];
		float len = ex * ex + ey * ey;
		if( sign * ( ex * py - ey * px ) < 0 )
			inside = false;
		float k = len > 0 ? bound( 0.0f, ( px * ex + py * ey ) / len, 1.0f ) : 0.0f;
		float qs = a[0] + ex * k, qt = a[1] + ey * k;
		float d = ( *s - qs ) * ( *s - qs ) + ( *t - qt ) * ( *t - qt );
		if( d < best ) { best = d; bs = qs; bt = qt; }
	}
	if( inside && best > 1.0f )
		return;
	if( !inside )
		*s = bs, *t = bt;
	float dx = cs - *s, dy = ct - *t, dl = sqrt( dx * dx + dy * dy );
	if( dl > 1.0f )
		*s += dx / dl, *t += dy / dl;
}

static void Dyn_LightGroup( dynlmgroup_t *g )
{
	const dynside_t *side = g->side;
	int n = g->w * g->h;
	g->lux = Mem_Calloc( dynpool, sizeof( dynluxel_t ) * n );
	g->col = Mem_Calloc( dynpool, sizeof( vec3_t ) * n );

	for( int j = 0; j < g->h; j++ )
		for( int i = 0; i < g->w; i++ )
		{
			float s = g->s0 + i * DYN_LUXEL, t = g->t0 + j * DYN_LUXEL;
			vec3_t p;
			Dyn_ClampToFace( g, &s, &t );
			VectorScale( side->normal, side->dist, p );
			VectorMA( p, s, g->u, p );
			VectorMA( p, t, g->v, p );
			Dyn_LuxelSample( &g->lux[j * g->w + i], p, side->normal );
		}

	// shadow edges: texels whose neighbours disagree about the sun average four more rays across themselves
	float lambert = has_sun ? DotProduct( side->normal, sun_dir ) : 0.0f;
	if( lambert > 0 )
	{
		for( int j = 0; j < g->h; j++ )
			for( int i = 0; i < g->w; i++ )
			{
				dynluxel_t *l = &g->lux[j * g->w + i];
				static const int nb[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }};
				qboolean edge = false;
				if( !l->valid )
					continue;
				for( int k = 0; k < 4 && !edge; k++ )
				{
					int x = i + nb[k][0], y = j + nb[k][1];
					if( x >= 0 && y >= 0 && x < g->w && y < g->h && g->lux[y * g->w + x].valid && g->lux[y * g->w + x].sun != l->sun )
						edge = true;
				}
				if( !edge )
					continue;
				float sum = l->sun;
				int cnt = 1;
				for( int k = 0; k < 4; k++ )
				{
					vec3_t q;
					VectorMA( l->pos, ( k & 1 ? 0.25f : -0.25f ) * DYN_LUXEL, g->u, q );
					VectorMA( q, ( k & 2 ? 0.25f : -0.25f ) * DYN_LUXEL, g->v, q );
					VectorMA( q, 2.0f, side->normal, q );
					if( Dyn_LightStartBad( q ))
						continue;
					sum += Dyn_SunRay( q );
					cnt++;
				}
				l->sunaa = sum / cnt;
			}
	}

	for( int k = 0; k < n; k++ )
		VectorMA( g->lux[k].base, g->lux[k].sunaa * lambert * 0.85f, sun_color, g->col[k] );

	// texels inside other geometry take their neighbours' light
	for( int pass = 0; pass < 4; pass++ )
	{
		qboolean any = false;
		for( int j = 0; j < g->h; j++ )
			for( int i = 0; i < g->w; i++ )
			{
				dynluxel_t *l = &g->lux[j * g->w + i];
				vec3_t sum = { 0, 0, 0 };
				int cnt = 0;
				if( l->valid )
					continue;
				for( int y = j - 1; y <= j + 1; y++ )
					for( int x = i - 1; x <= i + 1; x++ )
						if( x >= 0 && y >= 0 && x < g->w && y < g->h && g->lux[y * g->w + x].valid == 1 )
						{
							VectorAdd( sum, g->col[y * g->w + x], sum );
							cnt++;
						}
				if( cnt )
				{
					VectorScale( sum, 1.0f / cnt, g->col[j * g->w + i] );
					l->valid = 2;	// filled this pass: feeds the next one
					any = true;
				}
			}
		for( int k = 0; k < n; k++ )
			if( g->lux[k].valid == 2 )
				g->lux[k].valid = 1;
		if( !any )
			break;
	}
	for( int k = 0; k < n; k++ )
		if( !g->lux[k].valid )
			VectorScale( sky_color, 0.55f, g->col[k] );
}

static void Dyn_AddLightGroup( dynbuild_t *bld, int first, const dynside_t *side, const dynwinding_t *w )
{
	if( bld->numgroups >= bld->maxgroups )
	{
		int nmax = bld->maxgroups ? bld->maxgroups * 2 : 64;
		dynlmgroup_t *ng = Mem_Calloc( dynpool, sizeof( dynlmgroup_t ) * nmax );
		if( bld->groups ) { memcpy( ng, bld->groups, sizeof( dynlmgroup_t ) * bld->numgroups ); Mem_Free( bld->groups ); }
		bld->groups = ng;
		bld->maxgroups = nmax;
	}
	dynlmgroup_t *g = &bld->groups[bld->numgroups++];
	float smin = 1e30f, smax = -1e30f, tmin = 1e30f, tmax = -1e30f;
	memset( g, 0, sizeof( *g ));
	g->first = first;
	g->count = bld->num - first;
	g->side = side;
	Dyn_FaceAxes( side->normal, g->u, g->v );
	for( int i = first; i < bld->num; i++ )
	{
		float s = DotProduct( bld->v[i].xyz, g->u ), t = DotProduct( bld->v[i].xyz, g->v );
		smin = Q_min( smin, s ); smax = Q_max( smax, s );
		tmin = Q_min( tmin, t ); tmax = Q_max( tmax, t );
	}
	g->s0 = floor( smin / DYN_LUXEL ) * DYN_LUXEL;
	g->t0 = floor( tmin / DYN_LUXEL ) * DYN_LUXEL;
	g->w = Q_min( (int)(( smax - g->s0 ) / DYN_LUXEL ) + 2, DYN_LM_WIDTH );
	g->h = Q_min( (int)(( tmax - g->t0 ) / DYN_LUXEL ) + 2, DYN_LM_WIDTH );
	g->npts = w->num;
	for( int i = 0; i < w->num; i++ )
	{
		g->pts[i][0] = DotProduct( w->p[i], g->u );
		g->pts[i][1] = DotProduct( w->p[i], g->v );
	}
	Dyn_LightGroup( g );
}

static int Dyn_GroupCmp( const void *a, const void *b )
{
	return ( *(const dynlmgroup_t * const *)b )->h - ( *(const dynlmgroup_t * const *)a )->h;
}

// pack the faces' lightmaps into the cell's atlas (shelves, tallest first) and point the vertices at them
static void Dyn_PackLightmaps( dynbuild_t *bld, dyncell_t *cell )
{
	dynlmgroup_t **order = Mem_Malloc( dynpool, sizeof( dynlmgroup_t * ) * bld->numgroups );
	int x = 0, y = 0, shelf = 0, H = 16;

	for( int i = 0; i < bld->numgroups; i++ )
		order[i] = &bld->groups[i];
	qsort( order, bld->numgroups, sizeof( order[0] ), Dyn_GroupCmp );
	for( int i = 0; i < bld->numgroups; i++ )
	{
		dynlmgroup_t *g = order[i];
		if( x + g->w > DYN_LM_WIDTH )
		{
			x = 0;
			y += shelf;
			shelf = 0;
		}
		g->x = x;
		g->y = y;
		x += g->w;
		shelf = Q_max( shelf, g->h );
	}
	while( H < y + shelf )
		H *= 2;
	Mem_Free( order );

	cell->lmsize[0] = DYN_LM_WIDTH;
	cell->lmsize[1] = H;
	cell->lightmap = Mem_Calloc( dynpool, DYN_LM_WIDTH * H * 4 );
	for( int gi = 0; gi < bld->numgroups; gi++ )
	{
		dynlmgroup_t *g = &bld->groups[gi];
		for( int j = 0; j < g->h; j++ )
			for( int i = 0; i < g->w; i++ )
			{
				byte *out = &cell->lightmap[(( g->y + j ) * DYN_LM_WIDTH + g->x + i ) * 4];
				for( int c = 0; c < 3; c++ )
					out[c] = (byte)bound( 20.0f, Q_min( g->col[j * g->w + i][c], DYN_LM_MAX ) * 100.0f, 255.0f );
				out[3] = 255;
			}
		for( int k = g->first; k < g->first + g->count; k++ )
		{
			dynvert_t *v = &bld->v[k];
			float fs = ( DotProduct( v->xyz, g->u ) - g->s0 ) / DYN_LUXEL, ft = ( DotProduct( v->xyz, g->v ) - g->t0 ) / DYN_LUXEL;
			v->lm[0] = ( g->x + 0.5f + fs ) / DYN_LM_WIDTH;
			v->lm[1] = ( g->y + 0.5f + ft ) / H;
			// the light at the vertex too (bilinear, full scale), for the renderer's torch light
			int i0 = bound( 0, (int)fs, g->w - 1 ), j0 = bound( 0, (int)ft, g->h - 1 );
			int i1 = Q_min( i0 + 1, g->w - 1 ), j1 = Q_min( j0 + 1, g->h - 1 );
			float a = bound( 0.0f, fs - i0, 1.0f ), b = bound( 0.0f, ft - j0, 1.0f );
			for( int c = 0; c < 3; c++ )
			{
				float l = ( g->col[j0 * g->w + i0][c] * ( 1 - a ) + g->col[j0 * g->w + i1][c] * a ) * ( 1 - b )
					+ ( g->col[j1 * g->w + i0][c] * ( 1 - a ) + g->col[j1 * g->w + i1][c] * a ) * b;
				v->rgba[c] = (byte)bound( 40.0f, Q_min( l, DYN_LM_MAX ) * 200.0f, 255.0f );
			}
			v->rgba[3] = 255;
		}
		Mem_Free( g->lux );
		Mem_Free( g->col );
	}
	if( bld->groups )
		Mem_Free( bld->groups );
	bld->groups = NULL;
	bld->numgroups = bld->maxgroups = 0;
}

static void Dyn_BuildCell( int ci )
{
	dyncell_t *cell = &dw.cells[ci];
	dynbuild_t bld = { 0 };
	dynwinding_t w;

	Dyn_ClearLightCache();
	bld.cell = ci;
	bld.cellc[0] = ci % dw.gridsize[0];
	bld.cellc[1] = ( ci / dw.gridsize[0] ) % dw.gridsize[1];
	bld.cellc[2] = ci / ( dw.gridsize[0] * dw.gridsize[1] );

	const dynlist_t *l = &gridlists[ci];
	for( int k = 0; k < l->num; k++ )
	{
		const dynbrush_t *b = &brushes[l->list[k]];
		if( !b->alive )
			continue;
		for( int s = 0; s < b->numsides; s++ )
		{
			const dynside_t *side = &b->sides[s];
			if( side->tex < 0 || side->tex >= dw.numtextures || !Dyn_SideWinding( b, s, &w ))
				continue;
			// only the part that can have pieces in this cell (a piece is at most a block across), then the
			// pieces; hidden ones are left out one by one (a whole face can't be: a dug hole may be anywhere on it)
			dynwinding_t cw = w;
			for( int axis = 0; axis < 3 && cw.num >= 3; axis++ )
			{
				vec3_t n = { 0, 0, 0 };
				n[axis] = 1.0f;
				Dyn_ChopWinding( &cw, n, cell->maxs[axis] + DYN_PIECE );
				n[axis] = -1.0f;
				Dyn_ChopWinding( &cw, n, -( cell->mins[axis] - DYN_PIECE ));
			}
			if( cw.num < 3 )
				continue;
			int first = bld.num;
			Dyn_SplitFace( &bld, &cw, side, 0 );
			if( bld.num > first )
				Dyn_AddLightGroup( &bld, first, side, &w );
		}
	}

	if( cell->verts ) Mem_Free( cell->verts );
	if( cell->batches ) Mem_Free( cell->batches );
	if( cell->lightmap ) Mem_Free( cell->lightmap );
	cell->verts = NULL; cell->batches = NULL; cell->lightmap = NULL; cell->numverts = cell->numbatches = 0;
	cell->lmsize[0] = cell->lmsize[1] = 0;
	if( bld.numgroups )
		Dyn_PackLightmaps( &bld, cell );

	// sort into per-texture batches
	if( bld.num )
	{
		cell->verts = Mem_Malloc( dynpool, sizeof( dynvert_t ) * bld.num );
		cell->batches = Mem_Malloc( dynpool, sizeof( dynbatch_t ) * dw.numtextures );
		for( int t = 0; t < dw.numtextures; t++ )
		{
			int first = cell->numverts;
			for( int i = 0; i < bld.num; i++ )
				if( bld.tex[i] == t )
					cell->verts[cell->numverts++] = bld.v[i];
			if( cell->numverts > first )
			{
				dynbatch_t *bt = &cell->batches[cell->numbatches++];
				bt->texture = t;
				bt->first = first;
				bt->count = cell->numverts - first;
			}
		}
		Mem_Free( bld.v );
		Mem_Free( bld.tex );
	}
	cell->dirty = false;
	cell->rev++;
}

static double dyn_buildtime;
static int dyn_builds;

static void Dyn_ReportBuilds( void )
{
	if( dyn_builds && dyn_buildtime > 0.05 )
		Con_Reportf( "Dyn: built %d cells in %.0f ms\n", dyn_builds, dyn_buildtime * 1000.0 );
	dyn_buildtime = 0;
	dyn_builds = 0;
}

// called by the renderer for visible cells; rebuilds at most `budget` stale cells per frame
const dyncell_t *Dyn_GetCell( int ci, int *budget )
{
	if( !dw.active || ci < 0 || ci >= dw.gridsize[0] * dw.gridsize[1] * dw.gridsize[2] )
		return NULL;
	static uint lastframe;
	if( host.framecount != lastframe )
	{
		Dyn_ReportBuilds();
		lastframe = host.framecount;
	}
	dyncell_t *cell = &dw.cells[ci];
	if( cell->dirty && ( *budget > 0 || !cell->rev ))
	{
		double t0 = Sys_DoubleTime();
		Dyn_BuildCell( ci );
		( *budget )--;
		dyn_buildtime += Sys_DoubleTime() - t0;
		dyn_builds++;
	}
	return cell;
}

// the texture of the face a line hits first (NULL: nothing diggable on the way) and how far along
const char *Dyn_TraceTexture( const vec3_t start, const vec3_t end, float *fraction )
{
	dyntw_t tw;
	VectorCopy( start, tw.start ); VectorCopy( end, tw.end );
	VectorClear( tw.mins ); VectorClear( tw.maxs );
	Dyn_Trace( &tw );
	if( tw.brush < 0 || tw.startsolid )
		return NULL;
	if( fraction )
		*fraction = tw.fraction;
	const dynbrush_t *b = &brushes[tw.brush];
	for( int s = 0; s < b->numsides; s++ )
		if( b->sides[s].tex >= 0 && b->sides[s].tex < dw.numtextures && DotProduct( b->sides[s].normal, tw.normal ) > 0.999f )
			return dw.texnames[b->sides[s].tex];
	return b->cuttex >= 0 && b->cuttex < dw.numtextures ? dw.texnames[b->cuttex] : NULL;
}

//
// game API
//
static int GAME_EXPORT Dyn_TraceLineAPI( const float *start, const float *end, float *fraction, float *normal )
{
	dyntw_t tw;
	VectorCopy( start, tw.start ); VectorCopy( end, tw.end );
	VectorClear( tw.mins ); VectorClear( tw.maxs );
	Dyn_Trace( &tw );
	if( fraction ) *fraction = tw.fraction;
	if( tw.brush < 0 )
		return -1;
	if( normal ) VectorCopy( tw.normal, normal );
	return brushes[tw.brush].material;
}

static float GAME_EXPORT Dyn_CarveBoxAPI( const float *mins, const float *maxs, float *volume, int maxmaterials )
{
	return Dyn_CarveBox( mins, maxs, volume, maxmaterials );
}

static int GAME_EXPORT Dyn_PointSolidAPI( const float *p ) { return Dyn_PointSolid( p ); }
static int GAME_EXPORT Dyn_BoxSolidAPI( const float *a, const float *b ) { return Dyn_BoxSolid( a, b ); }
static const dynworld_t * GAME_EXPORT Dyn_WorldAPI( void ) { return &dw; }
static float GAME_EXPORT Dyn_BoxVolumeAPI( const float *mins, const float *maxs, float *volume, int maxmaterials )
{
	return Dyn_BoxVolume( mins, maxs, volume, maxmaterials );
}

static dyn_api_t dyn_api = { DYN_API_VERSION, Dyn_TraceLineAPI, Dyn_CarveBoxAPI, Dyn_PointSolidAPI, Dyn_BoxSolidAPI, Dyn_WorldAPI, Dyn_BoxVolumeAPI };

EXPORT dyn_api_t *Dyn_GetAPI( int version );
EXPORT dyn_api_t *Dyn_GetAPI( int version )
{
	return version == DYN_API_VERSION ? &dyn_api : NULL;
}
