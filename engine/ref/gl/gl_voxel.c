/*
gl_voxel.c - draws the Svencraft block world (the Minecraft-style underground)

The world (engine/common/voxel.c) is split into 16^3 sections. Each section keeps a mesh of the block faces
that border air or see-through blocks, grouped by texture, rebuilt when the engine bumps the section's
revision. Faces get Minecraft-style lighting: a fixed shade per direction, darkness away from the sky,
and per-vertex ambient occlusion.

Block looks come from scripts/blocks.txt in the game directory:
	<id> <name> <top texture> <side texture> <bottom texture> [alpha] [joined]
Textures are gfx/blocks/<texture>.tga (alpha-tested when the block is "alpha").
*/
#include "gl_local.h"
#include "voxel_api.h"
#include "dynworld_api.h"

#define VOXR_MAX_TEXTURES	128
#define VOXR_REBUILDS_PER_FRAME	48

typedef struct
{
	float	xyz[3];
	float	st[2];
	byte	rgba[4];
} vvert_t;

typedef struct
{
	int	count, max;
	vvert_t	*verts;
} vbatch_t;

#define VOXR_SECTION_TORCHES	32	// flames that smoke, per section (more torches than that just don't)

typedef struct
{
	unsigned int	rev;
	qboolean	built;
	qboolean	empty;
	vec3_t	mins, maxs;
	vbatch_t	batch[VOXR_MAX_TEXTURES];
	int	ntorches;
	vec3_t	torches[VOXR_SECTION_TORCHES];	// flame tips, world units; a burning furnace: its mouth's middle
	byte	fxface[VOXR_SECTION_TORCHES];	// 0 = a torch, else the face (face_dir index) the furnace's mouth is on
} vsection_t;

typedef struct
{
	short	tex[3];	// top, side, bottom texture slots (-1 = not drawn)
	byte	alpha;	// alpha-tested (leaves, glass)
	byte	joined;	// no faces between two blocks of this type
	byte	world;	// texture mapped at world scale (realistic blocks line up with the map's walls)
	byte	shape;	// VSHAPE_*: a full cube, or a torch standing on the floor or leaning off a wall
	byte	light;	// light it gives off (Minecraft's levels: a torch is 14)
	short	front;	// a texture slot for one side face only (a furnace's mouth), -1 = none
	byte	frontface;	// which face (index into face_dir) it goes on
} vblockdef_t;

enum { VSHAPE_CUBE = 0, VSHAPE_TORCH, VSHAPE_TORCH_PX, VSHAPE_TORCH_NX, VSHAPE_TORCH_PY, VSHAPE_TORCH_NY };

#define VOXR_CAVE_LIGHT		0.25f	// away from the sky and from any light: Minecraft's caves are dark
static const float vox_torch_color[3] = { 1.0f, 0.84f, 0.6f };

static struct
{
	unsigned int	world_rev;
	int		nsections;
	vsection_t	*sections;
	vblockdef_t	defs[VOX_MAX_IDS];
	int		*roof;		// per column: highest block z covered by map geometry (ground, buildings)
	byte		*light;		// block light per cell (0..15), flooded out from torches and the like
	unsigned int	lightsum;	// sum of the section revisions it was made for
	unsigned int	lightrev;	// bumped whenever the light actually changed (the town's lit meshes follow it)
	vec3_t		litmins, litmaxs;	// the box holding every lit cell, world units (empty: litmins > litmaxs)
	byte		*dynocc;	// the town's walls in the way of light, per cell (see R_VoxDynEdgeBlocked)
	unsigned int	dynrev;		// the diggable world's revision dynocc was found for
	unsigned int	carves_seen;	// the diggable world's carves the roof is up to date with
	unsigned int	carves_world;
	double		lastfx;		// client time torch particles were last spawned for
	int		numtextures;
	char		texnames[VOXR_MAX_TEXTURES][64];
	int		texnums[VOXR_MAX_TEXTURES];
	int		texsize[VOXR_MAX_TEXTURES][2];
	qboolean	texalpha[VOXR_MAX_TEXTURES];
	qboolean	defs_loaded;
} vr;

static int R_VoxTextureSlot( const char *name, qboolean alpha )
{
	int i;

	if( !Q_strcmp( name, "-" ))
		return -1;

	for( i = 0; i < vr.numtextures; i++ )
	{
		if( !Q_stricmp( vr.texnames[i], name ))
		{
			if( alpha ) vr.texalpha[i] = true;
			return i;
		}
	}

	if( vr.numtextures >= VOXR_MAX_TEXTURES )
		return -1;

	Q_strncpy( vr.texnames[i], name, sizeof( vr.texnames[i] ));
	vr.texnums[i] = 0;
	vr.texalpha[i] = alpha;
	vr.numtextures++;
	return i;
}

static void R_VoxLoadDefs( void )
{
	byte *file;
	char *p, *line;
	fs_offset_t len;
	int i;

	vr.defs_loaded = true;
	vr.numtextures = 0;
	memset( vr.defs, 0, sizeof( vr.defs ));
	for( i = 0; i < VOX_MAX_IDS; i++ )
		vr.defs[i].tex[0] = vr.defs[i].tex[1] = vr.defs[i].tex[2] = vr.defs[i].front = -1;

	file = gEngfuncs.fsapi->LoadFile( "scripts/blocks.txt", &len, false );
	if( !file )
	{
		gEngfuncs.Con_Printf( S_ERROR "R_VoxLoadDefs: scripts/blocks.txt not found\n" );
		return;
	}

	// LoadFile terminates the buffer
	for( line = (char *)file; line && *line; line = p )
	{
		char name[64], top[64], side[64], bottom[64], fl[4][32];
		int id, n, k;

		p = Q_strchr( line, '\n' );
		if( p ) *p++ = '\0';

		fl[0][0] = fl[1][0] = fl[2][0] = fl[3][0] = '\0';
		n = sscanf( line, "%d %63s %63s %63s %63s %31s %31s %31s %31s", &id, name, top, side, bottom, fl[0], fl[1], fl[2], fl[3] );
		if( n < 5 || id <= 0 || id >= VOX_MAX_IDS )
			continue;

		// flags: alpha, joined, world, a shape (torch, torch_px/nx/py/ny = leaning off a wall that way), lightN,
		// front_px/nx/py/ny=<texture>: that one side gets another texture
		for( k = 0; k < 4; k++ )
		{
			const char *f = fl[k];
			if( !Q_strncmp( f, "front_", 6 ) && Q_strlen( f ) > 9 && f[8] == '=' )
			{
				static const char *dirs[4] = { "px", "nx", "py", "ny" };	// face_dir indices 2..5
				int d;
				for( d = 0; d < 4; d++ )
					if( !Q_strncmp( f + 6, dirs[d], 2 ))
					{
						vr.defs[id].front = R_VoxTextureSlot( f + 9, false );
						vr.defs[id].frontface = 2 + d;
					}
				continue;
			}
			if( !Q_strcmp( f, "alpha" )) vr.defs[id].alpha = true;
			else if( !Q_strcmp( f, "joined" )) vr.defs[id].joined = true;
			else if( !Q_strcmp( f, "world" )) vr.defs[id].world = true;
			else if( !Q_strcmp( f, "torch" )) vr.defs[id].shape = VSHAPE_TORCH;
			else if( !Q_strcmp( f, "torch_px" )) vr.defs[id].shape = VSHAPE_TORCH_PX;
			else if( !Q_strcmp( f, "torch_nx" )) vr.defs[id].shape = VSHAPE_TORCH_NX;
			else if( !Q_strcmp( f, "torch_py" )) vr.defs[id].shape = VSHAPE_TORCH_PY;
			else if( !Q_strcmp( f, "torch_ny" )) vr.defs[id].shape = VSHAPE_TORCH_NY;
			else if( !Q_strncmp( f, "light", 5 )) vr.defs[id].light = (byte)bound( 0, Q_atoi( f + 5 ), 15 );
		}
		vr.defs[id].tex[0] = R_VoxTextureSlot( top, vr.defs[id].alpha );
		vr.defs[id].tex[1] = R_VoxTextureSlot( side, vr.defs[id].alpha );
		vr.defs[id].tex[2] = R_VoxTextureSlot( bottom, vr.defs[id].alpha );
	}
	Mem_Free( file );

	for( i = 0; i < vr.numtextures; i++ )
	{
		char path[128];

		Q_snprintf( path, sizeof( path ), "gfx/blocks/%s.tga", vr.texnames[i] );
		vr.texnums[i] = GL_LoadTexture( path, NULL, 0, TF_NEAREST );
		if( !vr.texnums[i] )
		{
			gEngfuncs.Con_Printf( S_WARN "R_VoxLoadDefs: no texture gfx/blocks/%s.tga\n", vr.texnames[i] );
			vr.texnums[i] = tr.defaultTexture;
		}
		{
			gl_texture_t *t = R_GetTexture( vr.texnums[i] );
			vr.texsize[i][0] = t && t->width ? t->width : 64;
			vr.texsize[i][1] = t && t->height ? t->height : 64;
		}
	}
	gEngfuncs.Con_Reportf( "R_VoxLoadDefs: %d block textures\n", vr.numtextures );
}

static void R_VoxFreeSections( void )
{
	int i, j;

	for( i = 0; i < vr.nsections; i++ )
		for( j = 0; j < VOXR_MAX_TEXTURES; j++ )
			free( vr.sections[i].batch[j].verts );
	free( vr.sections );
	vr.sections = NULL;
	vr.nsections = 0;
}

// textures may be gone after a map change; reload the definitions with the next world
static qboolean R_VoxSky( const vox_world_t *w, int x, int y, int z );
static int R_VoxBlockLight( const vox_world_t *w, int x, int y, int z );
float R_VoxLightAt( const vec3_t p );
static qboolean R_VoxDynCellSolid( const vox_world_t *w, int i, int x, int y, int z );

static qboolean R_DynModelLight( const vec3_t p, colorVec *light, vec3_t lightdir )
{
	const dynworld_t *dw = gEngfuncs.Dyn_World ? gEngfuncs.Dyn_World() : NULL;
	byte rgb[3];

	if( !dw || !dw->active || !gEngfuncs.Dyn_ModelLight)
		return false;
	gEngfuncs.Dyn_ModelLight( p, rgb, lightdir );
	light->r = rgb[0];
	light->g = rgb[1];
	light->b = rgb[2];
	return true;
}

// model light in Svencraft's worlds: underground in the block world the same dim light as the cave walls,
// elsewhere the diggable world's sun, sky and lamps
static qboolean R_VoxModelLight( const vec3_t p, colorVec *light, vec3_t lightdir )
{
	const vox_world_t *w = gEngfuncs.Vox_World ? gEngfuncs.Vox_World() : NULL;
	int x, y, z;

	if( !w || !w->active || !vr.roof )
		return R_DynModelLight( p, light, lightdir );
	x = (int)floorf( p[0] / VOX_BLOCK_SIZE );
	y = (int)floorf( p[1] / VOX_BLOCK_SIZE );
	z = (int)floorf( p[2] / VOX_BLOCK_SIZE );
	if( x < w->mins[0] || x >= w->maxs[0] || y < w->mins[1] || y >= w->maxs[1] || z < w->mins[2] || z >= w->maxs[2] )
		return R_DynModelLight( p, light, lightdir );
	// below the map's own ground and away from the sky
	if( z > vr.roof[( y - w->mins[1] ) * w->size[0] + ( x - w->mins[0] )] - 1 || R_VoxSky( w, x, y, z ))
	{
		// the town's light, and a torch's where it is brighter
		float t = R_VoxLightAt( p );
		if( !R_DynModelLight( p, light, lightdir ))
			return false;
		light->r = Q_max( light->r, (int)( 230 * t * vox_torch_color[0] ));
		light->g = Q_max( light->g, (int)( 230 * t * vox_torch_color[1] ));
		light->b = Q_max( light->b, (int)( 230 * t * vox_torch_color[2] ));
		return true;
	}
	{
		// cave darkness, warmed up near torches
		float t = R_VoxBlockLight( w, x, y, z ) / 14.0f, base = VOXR_CAVE_LIGHT * 0.9f;
		t = t / ( 2.0f - t );
		light->r = (int)( 255 * Q_max( base, t * vox_torch_color[0] ));
		light->g = (int)( 255 * Q_max( base, t * vox_torch_color[1] ));
		light->b = (int)( 255 * Q_max( base, t * vox_torch_color[2] ));
	}
	return true;
}

void R_VoxNewMap( void )
{
	r_worldlight_override = R_VoxModelLight;
	R_VoxFreeSections();
	free( vr.roof );
	vr.roof = NULL;
	free( vr.light );
	vr.light = NULL;
	free( vr.dynocc );
	vr.dynocc = NULL;
	vr.dynrev = 0;
	vr.defs_loaded = false;
	vr.world_rev = 0;
}

static vvert_t *R_VoxAlloc( vbatch_t *b, int n )
{
	if( b->count + n > b->max )
	{
		int newmax = Q_max( b->max * 2, b->count + n + 1024 );
		vvert_t *v = realloc( b->verts, sizeof( vvert_t ) * newmax );
		if( !v ) return NULL;
		b->verts = v;
		b->max = newmax;
	}
	b->count += n;
	return &b->verts[b->count - n];
}

// the six faces: outward normal and the four corners of the unit cube face (counter-clockwise from outside)
static const int face_dir[6][3] = { { 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }};
static const float face_corner[6][4][3] =
{
	{{ 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 }},	// +z
	{{ 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 }, { 0, 0, 0 }},	// -z
	{{ 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 }},	// +x
	{{ 0, 1, 0 }, { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }},	// -x
	{{ 1, 1, 0 }, { 0, 1, 0 }, { 0, 1, 1 }, { 1, 1, 1 }},	// +y
	{{ 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 }},	// -y
};
static const float face_st[4][2] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };	// side faces: t = 0 at the top
static const float face_shade[6] = { 1.0f, 0.5f, 0.8f, 0.8f, 0.65f, 0.65f };

static qboolean R_VoxOpaque( const vox_world_t *w, int x, int y, int z )
{
	if( z < w->mins[2] )
		return true;	// nothing to see below the bottom layer
	return FBitSet( w->flags[Vox_At( w, x, y, z )], VOXF_OPAQUE ) != 0;
}

static qboolean R_VoxSky( const vox_world_t *w, int x, int y, int z )
{
	int col;

	if( x < w->mins[0] || x >= w->maxs[0] || y < w->mins[1] || y >= w->maxs[1] )
		return true;
	col = ( y - w->mins[1] ) * w->size[0] + ( x - w->mins[0] );
	if( vr.roof && z <= vr.roof[col] )
		return false;	// under the map's own ground or a building
	return z > w->height[col];
}

// torch light at a point in the open (not inside a block or a wall), 0..1 after the falloff: blended between the
// eight nearest cell middles, leaving out solid cells, like Minecraft's smooth lighting
float R_VoxLightAt( const vec3_t p )
{
	const vox_world_t *w = gEngfuncs.Vox_World ? gEngfuncs.Vox_World() : NULL;
	float f[3], sum = 0.0f, wsum = 0.0f, t;
	int c0[3], i, k;

	if( !w || !w->active || !vr.light || vr.litmins[0] > vr.litmaxs[0] )
		return 0.0f;
	if( p[0] < vr.litmins[0] - VOX_BLOCK_SIZE || p[1] < vr.litmins[1] - VOX_BLOCK_SIZE || p[2] < vr.litmins[2] - VOX_BLOCK_SIZE
		|| p[0] > vr.litmaxs[0] + VOX_BLOCK_SIZE || p[1] > vr.litmaxs[1] + VOX_BLOCK_SIZE || p[2] > vr.litmaxs[2] + VOX_BLOCK_SIZE )
		return 0.0f;
	for( k = 0; k < 3; k++ )
	{
		float g = p[k] / VOX_BLOCK_SIZE - 0.5f - w->mins[k];
		c0[k] = (int)floorf( g );
		f[k] = g - c0[k];
	}
	for( i = 0; i < 8; i++ )
	{
		int c[3] = { c0[0] + ( i & 1 ), c0[1] + (( i >> 1 ) & 1 ), c0[2] + (( i >> 2 ) & 1 ) }, idx;
		float wt = ( i & 1 ? f[0] : 1.0f - f[0] ) * (( i >> 1 ) & 1 ? f[1] : 1.0f - f[1] ) * (( i >> 2 ) & 1 ? f[2] : 1.0f - f[2] );

		if( c[0] < 0 || c[1] < 0 || c[2] < 0 || c[0] >= w->size[0] || c[1] >= w->size[1] || c[2] >= w->size[2] )
			continue;
		idx = ( c[2] * w->size[1] + c[1] ) * w->size[0] + c[0];
		if( FBitSet( w->flags[w->blocks[idx]], VOXF_OPAQUE ) || ( !vr.light[idx] && R_VoxDynCellSolid( w, idx, c[0], c[1], c[2] )))
			continue;
		sum += vr.light[idx] * wt;
		wsum += wt;
	}
	if( wsum < 0.001f )
		return 0.0f;
	t = sum / wsum / 14.0f;
	return t / ( 2.0f - t );
}

// whether any torch light can reach into a box (world units)
qboolean R_VoxLightTouches( const vec3_t mins, const vec3_t maxs )
{
	if( !vr.light || vr.litmins[0] > vr.litmaxs[0] )
		return false;
	return mins[0] <= vr.litmaxs[0] + VOX_BLOCK_SIZE && maxs[0] >= vr.litmins[0] - VOX_BLOCK_SIZE
		&& mins[1] <= vr.litmaxs[1] + VOX_BLOCK_SIZE && maxs[1] >= vr.litmins[1] - VOX_BLOCK_SIZE
		&& mins[2] <= vr.litmaxs[2] + VOX_BLOCK_SIZE && maxs[2] >= vr.litmins[2] - VOX_BLOCK_SIZE;
}

unsigned int R_VoxLightRev( void )
{
	return vr.lightrev;
}

// block light at a cell, 0..15
static int R_VoxBlockLight( const vox_world_t *w, int x, int y, int z )
{
	if( !vr.light || x < w->mins[0] || x >= w->maxs[0] || y < w->mins[1] || y >= w->maxs[1] || z < w->mins[2] || z >= w->maxs[2] )
		return 0;
	return vr.light[(( z - w->mins[2] ) * w->size[1] + ( y - w->mins[1] )) * w->size[0] + ( x - w->mins[0] )];
}

// Minecraft's block light: each light source floods outwards through anything see-through, one level less a
// block. Recomputed whenever a block changes; the sections whose light changed are rebuilt.
//
// The town's diggable walls and floors stop it too. Asking the diggable world is slow-ish, so the answers are
// kept per cell (cleared when anything is dug): bits 0-5 hold the steps to the +x, +y and +z neighbours (2 bits
// each), bits 6-7 the cell's own middle; 0 = not asked yet, 1 = open, 2 = solid.
static qboolean R_VoxDynPoint( const vec3_t p )
{
	return gEngfuncs.Dyn_PointSolid && gEngfuncs.Dyn_PointSolid( p );
}

static void R_VoxCellCenter( const vox_world_t *w, int x, int y, int z, vec3_t p )
{
	p[0] = ( w->mins[0] + x + 0.5f ) * VOX_BLOCK_SIZE;
	p[1] = ( w->mins[1] + y + 0.5f ) * VOX_BLOCK_SIZE;
	p[2] = ( w->mins[2] + z + 0.5f ) * VOX_BLOCK_SIZE;
}

// x, y, z from the world's corner; i the cell index
static qboolean R_VoxDynCellSolid( const vox_world_t *w, int i, int x, int y, int z )
{
	int v;

	if( !vr.dynocc )
		return false;
	v = ( vr.dynocc[i] >> 6 ) & 3;
	if( !v )
	{
		vec3_t p;
		R_VoxCellCenter( w, x, y, z, p );
		v = R_VoxDynPoint( p ) ? 2 : 1;
		vr.dynocc[i] |= v << 6;
	}
	return v == 2;
}

// the step from cell i to the next one along +axis: four points on the way (both middles and two between, a third
// of a block apart, so a 16-unit wall can't slip through)
static qboolean R_VoxDynEdgeBlocked( const vox_world_t *w, int i, int x, int y, int z, int axis )
{
	int shift = axis * 2, v, k;

	if( !vr.dynocc )
		return false;
	v = ( vr.dynocc[i] >> shift ) & 3;
	if( !v )
	{
		vec3_t a, p;
		R_VoxCellCenter( w, x, y, z, a );
		v = 1;
		for( k = 0; k <= 3 && v == 1; k++ )
		{
			VectorCopy( a, p );
			p[axis] += VOX_BLOCK_SIZE * k / 3.0f;
			if( R_VoxDynPoint( p ))
				v = 2;
		}
		vr.dynocc[i] |= v << shift;
	}
	return v == 2;
}

static void R_VoxComputeLight( const vox_world_t *w )
{
	int sx = w->size[0], sy = w->size[1], sz = w->size[2], n = sx * sy * sz, i, head = 0, tail = 0;
	byte *old = vr.light, *light = calloc( n, 1 );
	int *queue = malloc( sizeof( int ) * n );
	static const int dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }};

	if( !light || !queue )
	{
		free( light );
		free( queue );
		return;
	}
	for( i = 0; i < n; i++ )
	{
		int l = vr.defs[w->blocks[i]].light;
		if( l )
		{
			light[i] = (byte)l;
			queue[tail++] = i;
		}
	}
	while( head < tail )
	{
		int c = queue[head++], l = light[c] - 1, x = c % sx, y = ( c / sx ) % sy, z = c / ( sx * sy ), d;
		if( l <= 0 )
			continue;
		for( d = 0; d < 6; d++ )
		{
			int nx = x + dirs[d][0], ny = y + dirs[d][1], nz = z + dirs[d][2], ni;
			if( nx < 0 || nx >= sx || ny < 0 || ny >= sy || nz < 0 || nz >= sz )
				continue;
			ni = ( nz * sy + ny ) * sx + nx;
			if( light[ni] >= l || FBitSet( w->flags[w->blocks[ni]], VOXF_OPAQUE ))
				continue;
			// the town's walls: the step is checked from the cell on its negative side
			if( dirs[d][0] + dirs[d][1] + dirs[d][2] > 0 ? R_VoxDynEdgeBlocked( w, c, x, y, z, d / 2 ) : R_VoxDynEdgeBlocked( w, ni, nx, ny, nz, d / 2 ))
				continue;
			light[ni] = (byte)l;
			if( tail < n )
				queue[tail++] = ni;
		}
	}
	free( queue );

	// the box around everything lit, for the town's meshes to know whether to look
	VectorSet( vr.litmins, 1e9f, 1e9f, 1e9f );
	VectorSet( vr.litmaxs, -1e9f, -1e9f, -1e9f );
	for( i = 0; i < n; i++ )
	{
		vec3_t p;
		if( !light[i] )
			continue;
		R_VoxCellCenter( w, i % sx, ( i / sx ) % sy, i / ( sx * sy ), p );
		AddPointToBounds( p, vr.litmins, vr.litmaxs );
	}
	if( !old || memcmp( old, light, n ))
		vr.lightrev++;

	// sections where the light changed (or next to a change) get rebuilt
	if( old && vr.sections )
	{
		for( i = 0; i < n; i++ )
		{
			int x, y, z, dx, dy, dz;
			if( old[i] == light[i] )
				continue;
			x = i % sx; y = ( i / sx ) % sy; z = i / ( sx * sy );
			for( dz = -1; dz <= 1; dz++ )
			for( dy = -1; dy <= 1; dy++ )
			for( dx = -1; dx <= 1; dx++ )
			{
				int cx = ( x + dx ) / VOX_SECTION, cy = ( y + dy ) / VOX_SECTION, cz = ( z + dz ) / VOX_SECTION;
				if( x + dx < 0 || y + dy < 0 || z + dz < 0 || cx >= w->nsections[0] || cy >= w->nsections[1] || cz >= w->nsections[2] )
					continue;
				vr.sections[( cz * w->nsections[1] + cy ) * w->nsections[0] + cx].built = false;
			}
		}
	}
	free( old );
	vr.light = light;
}

// a torch: a thin stick with its flame on top, standing on the floor or leaning off a wall; always bright
static void R_VoxEmitTorch( vsection_t *s, int x, int y, int z, const vblockdef_t *def )
{
	int slot = def->tex[1], f, c;
	float hw = 1.0f / 16.0f, h = 10.0f / 16.0f;
	vec3_t base, lean = { 0, 0, 0 };
	vvert_t *v, quad[4];

	if( slot < 0 )
		return;
	VectorSet( base, x + 0.5f, y + 0.5f, z );
	switch( def->shape )
	{
	case VSHAPE_TORCH_PX: base[0] += 0.32f; base[2] += 0.2f; lean[0] = -0.45f; break;
	case VSHAPE_TORCH_NX: base[0] -= 0.32f; base[2] += 0.2f; lean[0] = 0.45f; break;
	case VSHAPE_TORCH_PY: base[1] += 0.32f; base[2] += 0.2f; lean[1] = -0.45f; break;
	case VSHAPE_TORCH_NY: base[1] -= 0.32f; base[2] += 0.2f; lean[1] = 0.45f; break;
	}
	for( f = 0; f < 6; f++ )
	{
		for( c = 0; c < 4; c++ )
		{
			// the unit cube corner squeezed to the stick, sheared to lean, moved into place
			float px = ( face_corner[f][c][0] - 0.5f ) * 2.0f * hw, py = ( face_corner[f][c][1] - 0.5f ) * 2.0f * hw;
			float pz = face_corner[f][c][2] * h;
			quad[c].xyz[0] = ( base[0] + px + lean[0] * pz ) * VOX_BLOCK_SIZE;
			quad[c].xyz[1] = ( base[1] + py + lean[1] * pz ) * VOX_BLOCK_SIZE;
			quad[c].xyz[2] = ( base[2] + pz ) * VOX_BLOCK_SIZE;
			// the texture's middle two columns: flame at rows 6-7, stick below; the top shows the flame
			if( f == 0 )
			{
				quad[c].st[0] = ( 7.0f + face_corner[f][c][0] * 2.0f ) / 16.0f;
				quad[c].st[1] = ( 6.0f + face_corner[f][c][1] * 2.0f ) / 16.0f;
			}
			else if( f == 1 )
			{
				quad[c].st[0] = ( 7.0f + face_corner[f][c][0] * 2.0f ) / 16.0f;
				quad[c].st[1] = ( 14.0f + face_corner[f][c][1] * 2.0f ) / 16.0f;
			}
			else
			{
				quad[c].st[0] = ( 7.0f + face_st[c][0] * 2.0f ) / 16.0f;
				quad[c].st[1] = ( 6.0f + face_st[c][1] * 10.0f ) / 16.0f;
			}
			quad[c].rgba[0] = quad[c].rgba[1] = quad[c].rgba[2] = (byte)( 255 * ( f == 1 ? 0.6f : 1.0f ));
			quad[c].rgba[3] = 255;
		}
		v = R_VoxAlloc( &s->batch[slot], 6 );
		if( !v ) continue;
		v[0] = quad[0]; v[1] = quad[1]; v[2] = quad[2];
		v[3] = quad[0]; v[4] = quad[2]; v[5] = quad[3];
		s->empty = false;
	}
	if( s->ntorches < VOXR_SECTION_TORCHES )
	{
		float *tip = s->torches[s->ntorches];
		s->fxface[s->ntorches++] = 0;
		tip[0] = ( base[0] + lean[0] * h ) * VOX_BLOCK_SIZE;
		tip[1] = ( base[1] + lean[1] * h ) * VOX_BLOCK_SIZE;
		tip[2] = ( base[2] + h ) * VOX_BLOCK_SIZE + 1.0f;
	}
}

//
// Minecraft's torch particles: now and then a little flame pops off the tip and shrinks away, and a grey smoke
// square drifts up a block; solid squares facing the viewer, for torches near the viewer
//
#define VOXR_TORCH_FX_DIST	1024.0f
#define VOXR_FLAME_RATE		1.2f	// a second, per torch
#define VOXR_SMOKE_RATE		1.6f
#define VOXR_MAX_VPARTS		512

typedef struct
{
	vec3_t	org, vel;
	float	born, life, size;
	byte	rgb[3];
	qboolean	shrink;
} vpart_t;

static vpart_t vparts[VOXR_MAX_VPARTS];
static int vparts_next;

static void R_VoxSpawnPart( const vec3_t org, const vec3_t vel, float life, float size, byte r, byte g, byte b, qboolean shrink )
{
	vpart_t *p = &vparts[vparts_next];

	vparts_next = ( vparts_next + 1 ) % VOXR_MAX_VPARTS;	// the oldest goes when they run out
	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );
	p->born = gp_cl->time;
	p->life = life;
	p->size = size;
	p->rgb[0] = r; p->rgb[1] = g; p->rgb[2] = b;
	p->shrink = shrink;
}

static void R_VoxTorchParticles( vsection_t *s, float dt )
{
	int i;

	for( i = 0; i < s->ntorches; i++ )
	{
		vec3_t d, org, vel;

		VectorSubtract( s->torches[i], RI.rvp.vieworigin, d );
		if( DotProduct( d, d ) > VOXR_TORCH_FX_DIST * VOXR_TORCH_FX_DIST )
			continue;
		VectorCopy( s->torches[i], org );
		if( s->fxface[i] )
		{
			// a furnace's mouth: somewhere across its width, flames and smoke drifting out and up
			const int *n = face_dir[s->fxface[i]];
			float along = gEngfuncs.COM_RandomFloat( -0.3f, 0.3f ) * VOX_BLOCK_SIZE;
			org[0] += n[1] * along;
			org[1] += n[0] * along;
			org[2] += gEngfuncs.COM_RandomFloat( -0.15f, 0.1f ) * VOX_BLOCK_SIZE;
			if( gEngfuncs.COM_RandomFloat( 0.0f, 1.0f ) < dt * VOXR_FLAME_RATE * 1.5f )
			{
				byte g = (byte)gEngfuncs.COM_RandomLong( 150, 210 );
				VectorSet( vel, n[0] * 3.0f, n[1] * 3.0f, gEngfuncs.COM_RandomFloat( 1.0f, 4.0f ));
				R_VoxSpawnPart( org, vel, gEngfuncs.COM_RandomFloat( 0.4f, 0.7f ), 2.0f, 255, g, 60, true );
			}
			if( gEngfuncs.COM_RandomFloat( 0.0f, 1.0f ) < dt * VOXR_SMOKE_RATE )
			{
				byte c = (byte)gEngfuncs.COM_RandomLong( 40, 80 );
				VectorSet( vel, n[0] * 6.0f, n[1] * 6.0f, gEngfuncs.COM_RandomFloat( 16.0f, 26.0f ));
				R_VoxSpawnPart( org, vel, gEngfuncs.COM_RandomFloat( 1.2f, 2.0f ), gEngfuncs.COM_RandomFloat( 2.0f, 3.2f ), c, c, c, false );
			}
			continue;
		}
		if( gEngfuncs.COM_RandomFloat( 0.0f, 1.0f ) < dt * VOXR_FLAME_RATE )
		{
			// the flame: yellow-orange, a hair above the tip, rising slowly while it shrinks
			byte g = (byte)gEngfuncs.COM_RandomLong( 170, 220 );
			VectorSet( vel, 0, 0, gEngfuncs.COM_RandomFloat( 1.0f, 3.0f ));
			R_VoxSpawnPart( org, vel, gEngfuncs.COM_RandomFloat( 0.4f, 0.8f ), 2.2f, 255, g, 70, true );
		}
		if( gEngfuncs.COM_RandomFloat( 0.0f, 1.0f ) < dt * VOXR_SMOKE_RATE )
		{
			// smoke: dark grey, drifting up about a block
			byte c = (byte)gEngfuncs.COM_RandomLong( 40, 90 );
			org[2] += 2.0f;
			VectorSet( vel, gEngfuncs.COM_RandomFloat( -2.0f, 2.0f ), gEngfuncs.COM_RandomFloat( -2.0f, 2.0f ), gEngfuncs.COM_RandomFloat( 18.0f, 30.0f ));
			R_VoxSpawnPart( org, vel, gEngfuncs.COM_RandomFloat( 1.2f, 2.0f ), gEngfuncs.COM_RandomFloat( 2.0f, 3.2f ), c, c, c, false );
		}
	}
}

static void R_VoxDrawParticles( void )
{
	vec3_t right, up;
	int i;

	pglDisable( GL_TEXTURE_2D );
	pglBegin( GL_QUADS );
	for( i = 0; i < VOXR_MAX_VPARTS; i++ )
	{
		vpart_t *p = &vparts[i];
		float age = gp_cl->time - p->born, f, sz;
		vec3_t o;

		if( p->life <= 0.0f )
			continue;
		if( age < 0.0f || age > p->life )
		{
			p->life = 0.0f;
			continue;
		}
		f = age / p->life;
		// flames shrink away; smoke slows down as it rises and shrinks a little at the end
		sz = p->shrink ? p->size * ( 1.0f - f * f ) : p->size * ( f < 0.7f ? 1.0f : 1.0f - ( f - 0.7f ) * 2.0f );
		VectorMA( p->org, p->shrink ? age : age * ( 1.0f - 0.4f * f ), p->vel, o );
		VectorScale( RI.cull_vright, sz, right );
		VectorScale( RI.cull_vup, sz, up );
		pglColor4ub( p->rgb[0], p->rgb[1], p->rgb[2], 255 );
		pglVertex3f( o[0] - right[0] + up[0], o[1] - right[1] + up[1], o[2] - right[2] + up[2] );
		pglVertex3f( o[0] + right[0] + up[0], o[1] + right[1] + up[1], o[2] + right[2] + up[2] );
		pglVertex3f( o[0] + right[0] - up[0], o[1] + right[1] - up[1], o[2] + right[2] - up[2] );
		pglVertex3f( o[0] - right[0] - up[0], o[1] - right[1] - up[1], o[2] - right[2] - up[2] );
	}
	pglEnd();
	pglEnable( GL_TEXTURE_2D );
}

// for each column, the highest block that the map's solid geometry covers (blocks below it get no sky light)
static void R_VoxRoofColumn( const vox_world_t *w, int x, int y );
static void R_VoxComputeRoof( const vox_world_t *w )
{
	int x, y;

	free( vr.roof );
	vr.roof = malloc( sizeof( int ) * w->size[0] * w->size[1] );
	if( !vr.roof || !WORLDMODEL )
		return;

	for( y = w->mins[1]; y < w->maxs[1]; y++ )
	for( x = w->mins[0]; x < w->maxs[0]; x++ )
		R_VoxRoofColumn( w, x, y );
}

static void R_VoxRoofColumn( const vox_world_t *w, int x, int y )
{
	float top = WORLDMODEL ? WORLDMODEL->maxs[2] : 4096.0f, bottom = w->mins[2] * VOX_BLOCK_SIZE;
	{
		int *r = &vr.roof[( y - w->mins[1] ) * w->size[0] + ( x - w->mins[0] )];
		vec3_t p;
		float z;

		qboolean inside = false;

		*r = w->mins[2] - 1000;
		p[0] = ( x + 0.5f ) * VOX_BLOCK_SIZE;
		p[1] = ( y + 0.5f ) * VOX_BLOCK_SIZE;
		for( z = top; z > bottom; z -= 16.0f )
		{
			mleaf_t *leaf;
			int contents;
			qboolean dyn;

			p[2] = z;
			leaf = gEngfuncs.Mod_PointInLeaf( p, WORLDMODEL->nodes, WORLDMODEL );
			contents = leaf ? leaf->contents : CONTENTS_SOLID;
			dyn = gEngfuncs.Dyn_PointSolid && gEngfuncs.Dyn_PointSolid( p );
			// above the sky box the map is solid too: only solid below open air counts as a roof
			if( !inside )
			{
				inside = contents == CONTENTS_EMPTY && !dyn;
				continue;
			}
			if( contents == CONTENTS_SOLID || dyn )
			{
				// the surface is within the 16 units above the sample: covered are the cells whose tops are below it
				*r = (int)floorf(( z + 16.0f ) / VOX_BLOCK_SIZE ) - 1;
				break;
			}
		}
	}
}

// a column's sky changed: its sections (and the next ones over, for the smooth light) are built again
static void R_VoxColumnChanged( const vox_world_t *w, int x, int y )
{
	int sx0 = ( x - 1 - w->mins[0] ) / VOX_SECTION, sx1 = ( x + 1 - w->mins[0] ) / VOX_SECTION;
	int sy0 = ( y - 1 - w->mins[1] ) / VOX_SECTION, sy1 = ( y + 1 - w->mins[1] ) / VOX_SECTION;
	for( int sz = 0; sz < w->nsections[2]; sz++ )
		for( int sy = Q_max( sy0, 0 ); sy <= Q_min( sy1, w->nsections[1] - 1 ); sy++ )
			for( int sx = Q_max( sx0, 0 ); sx <= Q_min( sx1, w->nsections[0] - 1 ); sx++ )
				vr.sections[( sz * w->nsections[1] + sy ) * w->nsections[0] + sx].built = false;
}

static void R_VoxBuildSection( const vox_world_t *w, int sx, int sy, int sz, vsection_t *s )
{
	int x0 = w->mins[0] + sx * VOX_SECTION, y0 = w->mins[1] + sy * VOX_SECTION, z0 = w->mins[2] + sz * VOX_SECTION;
	int x1 = Q_min( x0 + VOX_SECTION, w->maxs[0] ), y1 = Q_min( y0 + VOX_SECTION, w->maxs[1] ), z1 = Q_min( z0 + VOX_SECTION, w->maxs[2] );
	int x, y, z, f, c, i;

	for( i = 0; i < VOXR_MAX_TEXTURES; i++ )
		s->batch[i].count = 0;
	s->empty = true;
	s->ntorches = 0;

	VectorSet( s->mins, x0 * VOX_BLOCK_SIZE, y0 * VOX_BLOCK_SIZE, z0 * VOX_BLOCK_SIZE );
	VectorSet( s->maxs, x1 * VOX_BLOCK_SIZE, y1 * VOX_BLOCK_SIZE, z1 * VOX_BLOCK_SIZE );

	for( z = z0; z < z1; z++ )
	for( y = y0; y < y1; y++ )
	for( x = x0; x < x1; x++ )
	{
		int id = Vox_At( w, x, y, z );
		const vblockdef_t *def;

		if( !id )
			continue;
		def = &vr.defs[id];
		if( def->shape != VSHAPE_CUBE )
		{
			R_VoxEmitTorch( s, x, y, z, def );
			continue;
		}
		if( def->light && def->front >= 0 && s->ntorches < VOXR_SECTION_TORCHES )
		{
			// a burning furnace: its mouth (rows 7-12 of the 16-pixel face) smokes and spits flames
			const int *n = face_dir[def->frontface];
			float *m = s->torches[s->ntorches];
			s->fxface[s->ntorches++] = def->frontface;
			m[0] = ( x + 0.5f + n[0] * 0.53f ) * VOX_BLOCK_SIZE;
			m[1] = ( y + 0.5f + n[1] * 0.53f ) * VOX_BLOCK_SIZE;
			m[2] = ( z + 0.4f ) * VOX_BLOCK_SIZE;
		}

		for( f = 0; f < 6; f++ )
		{
			int nx = x + face_dir[f][0], ny = y + face_dir[f][1], nz = z + face_dir[f][2];
			int nid = Vox_At( w, nx, ny, nz );
			int slot = ( def->front >= 0 && f == def->frontface ) ? def->front : def->tex[f == 0 ? 0 : ( f == 1 ? 2 : 1 )];
			float light, shade;
			vvert_t *v, quad[4];
			int a0, a1;

			if( slot < 0 || R_VoxOpaque( w, nx, ny, nz ))
				continue;
			if( nid == id && def->joined )
				continue;

			light = R_VoxSky( w, nx, ny, nz ) ? 1.0f : VOXR_CAVE_LIGHT;
			shade = face_shade[f];

			// the two axes spanning the face, for ambient occlusion
			a0 = face_dir[f][0] ? 1 : 0;
			a1 = face_dir[f][2] ? 1 : 2;

			for( c = 0; c < 4; c++ )
			{
				int d0, d1, s0, s1, corner, ao, bl, nbl, k;
				int p[3];
				float b, t, rgb[3];

				// the neighbours of this corner in the layer in front of the face
				d0 = face_corner[f][c][a0] > 0.5f ? 1 : -1;
				d1 = face_corner[f][c][a1] > 0.5f ? 1 : -1;
				p[0] = nx; p[1] = ny; p[2] = nz; p[a0] += d0;
				s0 = R_VoxOpaque( w, p[0], p[1], p[2] );
				p[0] = nx; p[1] = ny; p[2] = nz; p[a1] += d1;
				s1 = R_VoxOpaque( w, p[0], p[1], p[2] );
				p[0] = nx; p[1] = ny; p[2] = nz; p[a0] += d0; p[a1] += d1;
				corner = R_VoxOpaque( w, p[0], p[1], p[2] );
				ao = ( s0 && s1 ) ? 0 : 3 - ( s0 + s1 + corner );

				// smooth block light: the cells around this corner in front of the face, the solid ones left out
				bl = R_VoxBlockLight( w, nx, ny, nz );
				nbl = 1;
				if( !s0 ) { p[0] = nx; p[1] = ny; p[2] = nz; p[a0] += d0; bl += R_VoxBlockLight( w, p[0], p[1], p[2] ); nbl++; }
				if( !s1 ) { p[0] = nx; p[1] = ny; p[2] = nz; p[a1] += d1; bl += R_VoxBlockLight( w, p[0], p[1], p[2] ); nbl++; }
				if( !corner && !( s0 && s1 )) { p[0] = nx; p[1] = ny; p[2] = nz; p[a0] += d0; p[a1] += d1; bl += R_VoxBlockLight( w, p[0], p[1], p[2] ); nbl++; }
				t = ( bl / (float)nbl ) / 14.0f;
				t = t / ( 2.0f - t );	// Minecraft-ish falloff: bright by the torch, dim a few blocks away

				b = shade * ( 0.55f + 0.15f * ao );
				for( k = 0; k < 3; k++ )
					rgb[k] = bound( 0.0f, b * Q_max( light, t * vox_torch_color[k] ) * 255.0f, 255.0f );

				quad[c].xyz[0] = ( x + face_corner[f][c][0] ) * VOX_BLOCK_SIZE;
				quad[c].xyz[1] = ( y + face_corner[f][c][1] ) * VOX_BLOCK_SIZE;
				quad[c].xyz[2] = ( z + face_corner[f][c][2] ) * VOX_BLOCK_SIZE;
				if( def->world )
				{
					// world-aligned at one texel per unit, like the map's own brushes
					float tw = vr.texsize[slot][0], th = vr.texsize[slot][1];
					const float *wp = quad[c].xyz;
					if( f < 2 ) { quad[c].st[0] = wp[0] / tw; quad[c].st[1] = -wp[1] / th; }
					else if( f < 4 ) { quad[c].st[0] = wp[1] / tw; quad[c].st[1] = -wp[2] / th; }
					else { quad[c].st[0] = wp[0] / tw; quad[c].st[1] = -wp[2] / th; }
				}
				else if( f < 2 )
				{
					quad[c].st[0] = face_corner[f][c][0];
					quad[c].st[1] = face_corner[f][c][1];
				}
				else
				{
					quad[c].st[0] = face_st[c][0];
					quad[c].st[1] = face_st[c][1];
				}
				quad[c].rgba[0] = (byte)rgb[0];
				quad[c].rgba[1] = (byte)rgb[1];
				quad[c].rgba[2] = (byte)rgb[2];
				quad[c].rgba[3] = 255;
			}

			v = R_VoxAlloc( &s->batch[slot], 6 );
			if( !v ) continue;
			v[0] = quad[0]; v[1] = quad[1]; v[2] = quad[2];
			v[3] = quad[0]; v[4] = quad[2]; v[5] = quad[3];
			s->empty = false;
		}
	}
}

/*
=============
R_DrawVoxelWorld

called right after the BSP world, so entities and translucent surfaces sort against the blocks
=============
*/
void R_DrawVoxelWorld( void )
{
	const vox_world_t *w;
	int sx, sy, sz, i, rebuilt = 0;
	float fxdt;

	if( !gEngfuncs.Vox_World )
		return;
	w = gEngfuncs.Vox_World( );
	if( !w || !w->active )
	{
		if( vr.sections ) R_VoxFreeSections();
		return;
	}

	if( !vr.defs_loaded )
		R_VoxLoadDefs();

	if( vr.world_rev != w->world_rev || !vr.sections )
	{
		R_VoxFreeSections();
		vr.nsections = w->nsections[0] * w->nsections[1] * w->nsections[2];
		vr.sections = calloc( vr.nsections, sizeof( vsection_t ));
		vr.world_rev = w->world_rev;
		if( !vr.sections ) { vr.nsections = 0; return; }
		R_VoxComputeRoof( w );
		free( vr.light );
		vr.light = NULL;
		free( vr.dynocc );
		vr.dynocc = calloc( w->size[0] * w->size[1] * w->size[2], 1 );
		vr.dynrev = 0;
	}

	// digging the town opens new ways for light (and closes none, but placed blocks are handled anyway)
	{
		const dynworld_t *dw = gEngfuncs.Dyn_World ? gEngfuncs.Dyn_World() : NULL;
		unsigned int drev = 1;
		if( dw && dw->active )
		{
			int nc = dw->gridsize[0] * dw->gridsize[1] * dw->gridsize[2];
			drev += dw->world_rev * 7919u;
			for( i = 0; i < nc; i++ )
				drev += dw->cells[i].rev * (unsigned int)( i + 1 );

			// dug since the last frame: the columns under the dug boxes may see the sky now
			if( vr.carves_world != dw->world_rev )
			{
				vr.carves_world = dw->world_rev;
				vr.carves_seen = dw->numcarves;
			}
			if( dw->numcarves - vr.carves_seen > DYN_CARVE_LOG )
				vr.carves_seen = dw->numcarves - DYN_CARVE_LOG;
			for( ; vr.roof && vr.carves_seen != dw->numcarves; vr.carves_seen++ )
			{
				const float *box = dw->carvelog[vr.carves_seen % DYN_CARVE_LOG];
				int x, y;
				int x0 = Q_max( w->mins[0], (int)floorf( box[0] / VOX_BLOCK_SIZE ));
				int x1 = Q_min( w->maxs[0] - 1, (int)floorf(( box[3] - 0.01f ) / VOX_BLOCK_SIZE ));
				int y0 = Q_max( w->mins[1], (int)floorf( box[1] / VOX_BLOCK_SIZE ));
				int y1 = Q_min( w->maxs[1] - 1, (int)floorf(( box[4] - 0.01f ) / VOX_BLOCK_SIZE ));
				for( y = y0; y <= y1; y++ )
					for( x = x0; x <= x1; x++ )
					{
						int col = ( y - w->mins[1] ) * w->size[0] + ( x - w->mins[0] );
						int was = vr.roof[col];
						R_VoxRoofColumn( w, x, y );
						if( vr.roof[col] != was )
							R_VoxColumnChanged( w, x, y );
					}
			}
		}
		if( drev != vr.dynrev && vr.dynocc )
		{
			memset( vr.dynocc, 0, w->size[0] * w->size[1] * w->size[2] );
			vr.dynrev = drev;
			vr.lightsum = ~vr.lightsum;	// recomputed below
		}
	}

	// any block change can move light around
	{
		unsigned int sum = 0;
		for( i = 0; i < vr.nsections; i++ )
			sum += w->section_rev[i] * ( i + 1 );
		if( !vr.light || sum != vr.lightsum )
		{
			R_VoxComputeLight( w );
			vr.lightsum = sum;
		}
	}

	// time since torch particles were last made (the world can be drawn more than once a frame)
	fxdt = Q_min( gp_cl->time - vr.lastfx, 0.1 );
	vr.lastfx = gp_cl->time;

	pglEnable( GL_DEPTH_TEST );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );
	GL_Cull( GL_NONE );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglEnableClientState( GL_COLOR_ARRAY );
	pglAlphaFunc( GL_GREATER, 0.5f );

	for( sz = 0; sz < w->nsections[2]; sz++ )
	for( sy = 0; sy < w->nsections[1]; sy++ )
	for( sx = 0; sx < w->nsections[0]; sx++ )
	{
		int index = ( sz * w->nsections[1] + sy ) * w->nsections[0] + sx;
		vsection_t *s = &vr.sections[index];

		if( !s->built || s->rev != w->section_rev[index] )
		{
			// a budget per frame for stale sections; sections never built always get built
			if( rebuilt < VOXR_REBUILDS_PER_FRAME || !s->built )
			{
				R_VoxBuildSection( w, sx, sy, sz, s );
				s->rev = w->section_rev[index];
				s->built = true;
				rebuilt++;
			}
		}

		if( s->empty || R_CullBox( s->mins, s->maxs ))
			continue;
		if( fxdt > 0.0f )
			R_VoxTorchParticles( s, fxdt );

		for( i = 0; i < vr.numtextures; i++ )
		{
			vbatch_t *b = &s->batch[i];

			if( !b->count )
				continue;

			if( vr.texalpha[i] ) pglEnable( GL_ALPHA_TEST );
			GL_Bind( XASH_TEXTURE0, vr.texnums[i] );
			pglVertexPointer( 3, GL_FLOAT, sizeof( vvert_t ), b->verts[0].xyz );
			pglTexCoordPointer( 2, GL_FLOAT, sizeof( vvert_t ), b->verts[0].st );
			pglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( vvert_t ), b->verts[0].rgba );
			pglDrawArrays( GL_TRIANGLES, 0, b->count );
			if( vr.texalpha[i] ) pglDisable( GL_ALPHA_TEST );
		}
	}

	// dynamic lights (muzzle flashes, explosions, the flashlight): an added pass over the sections they reach
	{
		const dlight_t *dlights[MAX_DLIGHTS];
		int numdlights = R_GatherDlights( dlights, MAX_DLIGHTS );

		if( numdlights )
		{
			pglEnable( GL_BLEND );
			pglBlendFunc( GL_ONE, GL_ONE );
			pglDepthMask( GL_FALSE );
			pglDepthFunc( GL_EQUAL );
			R_FogPass( 2 );		// added light fogs to black (gl_dynworld.c)
		}
		for( int si = 0; numdlights && si < vr.nsections; si++ )
		{
			vsection_t *s = &vr.sections[si];

			if( !s->built || s->empty || R_CullBox( s->mins, s->maxs ) || !R_DlightsTouch( dlights, numdlights, s->mins, s->maxs ))
				continue;
			for( i = 0; i < vr.numtextures; i++ )
			{
				vbatch_t *b = &s->batch[i];
				if( b->count )
					R_DlightsDraw( dlights, numdlights, b->verts[0].xyz, b->verts[0].st, sizeof( vvert_t ), b->count, vr.texnums[i], vr.texalpha[i] );
			}
		}
		if( numdlights )
		{
			R_FogPass( 0 );
			pglDisable( GL_BLEND );
			pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			pglDepthMask( GL_TRUE );
			pglDepthFunc( GL_LEQUAL );
		}
	}

	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
	R_VoxDrawParticles();
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_Cull( GL_FRONT );
}
