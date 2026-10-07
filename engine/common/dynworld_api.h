/*
dynworld_api.h - Svencraft destructible map geometry, shared by the engine, the renderer and the game DLLs

Diggable parts of a map (terrain, walls) are not compiled into the BSP. The map tools write them to
maps/<map>.dyn as convex brushes with Half-Life texture info; the engine loads them as part of the world:
every trace collides with them (swept box vs brush, Quake 3 style), the renderer draws them (textures from
the WADs, lighting computed by the engine), and the game carves block-sized boxes out of them.
*/
#ifndef DYNWORLD_API_H
#define DYNWORLD_API_H

#define DYN_API_VERSION		1
#define DYN_CELL_SIZE		256.0f	// render/collision grid cell
#define DYN_MAX_TEXTURES	256
#define DYN_CARVE_LOG		64	// the last carves, for the renderer (sky over the block world)

typedef struct dynvert_s
{
	float		xyz[3];
	float		st[2];
	float		lm[2];		// in the cell's lightmap
	unsigned char	rgba[4];	// the light there (200 = full brightness), for blending torch light over it
} dynvert_t;

typedef struct dynbatch_s
{
	int		texture;	// index into dynworld_t.texnames
	int		first;		// first vertex in the cell's array
	int		count;		// triangles * 3
} dynbatch_t;

typedef struct dyncell_s
{
	float		mins[3], maxs[3];
	unsigned int	rev;		// bumped when the cell's mesh was rebuilt
	int		numverts;
	dynvert_t	*verts;
	int		numbatches;
	dynbatch_t	*batches;
	int		dirty;
	int		lmsize[2];
	unsigned char	*lightmap;	// RGBA, 128 = full brightness (drawn doubled, like Half-Life's overbright lightmaps)
} dyncell_t;

typedef struct dynworld_s
{
	int		active;
	unsigned int	world_rev;	// bumped on load / clear
	int		numtextures;
	char		texnames[DYN_MAX_TEXTURES][32];
	int		gridsize[3];
	float		gridmins[3];
	dyncell_t	*cells;		// gridsize[0] * gridsize[1] * gridsize[2]
	unsigned int	numcarves;	// carves that removed something, so far
	float		carvelog[DYN_CARVE_LOG][6];	// the last of them: mins, maxs (numcarves % DYN_CARVE_LOG)
} dynworld_t;

typedef struct dyn_api_s
{
	int	version;
	// first brush hit along a line: material of the brush (-1 = none); fraction and face normal
	int	(*TraceLine)( const float *start, const float *end, float *fraction, float *normal );
	// cut an axis-aligned box out of the diggable geometry; volume[material] gets the removed volume
	// (world units cubed) per material. Returns the total volume removed.
	float	(*CarveBox)( const float *mins, const float *maxs, float *volume, int maxmaterials );
	int	(*PointSolid)( const float *point );
	int	(*BoxSolid)( const float *absmin, const float *absmax );
	const dynworld_t *(*World)( void );
	// like CarveBox but only measures: how much diggable material is inside the box
	float	(*BoxVolume)( const float *mins, const float *maxs, float *volume, int maxmaterials );
} dyn_api_t;

typedef dyn_api_t *(*pfnDyn_GetAPI)( int version );
#define DYN_GETAPI_NAME	"Dyn_GetAPI"

#endif // DYNWORLD_API_H
