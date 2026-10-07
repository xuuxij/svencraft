/*
voxel_api.h - Svencraft block world, shared by the engine, the renderer and the game DLLs

The world is a fixed box of blocks (VOX_BLOCK_SIZE units each) owned by the engine. Block 0 is air.
Collision is part of the world model: every engine trace (player movement, monsters, bullets, grenades)
clips against solid blocks. The renderer draws the blocks as chunk meshes. The game DLLs read and
change blocks through vox_api_t, which they get from the engine's exported Vox_GetAPI().
*/
#ifndef VOXEL_API_H
#define VOXEL_API_H

#define VOX_API_VERSION	1
#define VOX_BLOCK_SIZE	40.0f	// world units per block: the 72-unit player is 1.8 blocks tall
#define VOX_SECTION	16	// blocks per render/dirty section edge
#define VOX_MAX_IDS	256

// per block id flags (set by the game)
#define VOXF_SOLID	(1<<0)	// collides
#define VOXF_OPAQUE	(1<<1)	// hides the faces of neighbours (leaves and glass are solid but not opaque)

typedef struct vox_world_s
{
	int		active;
	int		mins[3];		// block bounds, maxs exclusive
	int		maxs[3];
	int		size[3];
	unsigned short	*blocks;		// size[0] * size[1] * size[2], x fastest, then y, then z
	int		*height;		// per column: highest non-air block z, mins[2] - 1 when empty (sky light)
	int		nsections[3];
	unsigned int	*section_rev;	// bumped whenever a block in (or bordering) the section changes
	unsigned int	world_rev;		// bumped on init / clear
	unsigned char	flags[VOX_MAX_IDS];
} vox_world_t;

typedef struct vox_api_s
{
	int	version;
	int	(*Init)( int minx, int miny, int minz, int maxx, int maxy, int maxz );	// clears the world
	void	(*Shutdown)( void );
	int	(*Get)( int x, int y, int z );
	void	(*Set)( int x, int y, int z, int id );
	void	(*Fill)( int x0, int y0, int z0, int x1, int y1, int z1, int id );	// [x0, x1) etc.
	void	(*SetFlags)( int id, int flags );
	const vox_world_t *(*World)( void );
	// first solid block along a line: returns its id (0 = none); fraction, cell and face normal of the hit
	int	(*TraceLine)( const float *start, const float *end, float *fraction, int *cell, float *normal );
	// true when any solid block overlaps the world-space box
	int	(*BoxSolid)( const float *absmin, const float *absmax );
	// saving (the paths are in the game folder): the block world and every carve in the diggable geometry, as the
	// snapshot a joining player gets (engine common/scnet.c); loading one puts it in place of the generated world
	int	(*SaveWorld)( const char *path );
	int	(*LoadWorld)( const char *path );
	int	(*WriteFile)( const char *path, const void *data, int size );	// makes the folders
	int	(*RenameFile)( const char *oldpath, const char *newpath );	// newpath NULL: deletes
} vox_api_t;

typedef vox_api_t *(*pfnVox_GetAPI)( int version );
#define VOX_GETAPI_NAME	"Vox_GetAPI"

static inline int Vox_InBounds( const vox_world_t *w, int x, int y, int z )
{
	return x >= w->mins[0] && x < w->maxs[0] && y >= w->mins[1] && y < w->maxs[1] && z >= w->mins[2] && z < w->maxs[2];
}

static inline int Vox_At( const vox_world_t *w, int x, int y, int z )
{
	if( !w->active || !Vox_InBounds( w, x, y, z ))
		return 0;
	return w->blocks[((z - w->mins[2]) * w->size[1] + ( y - w->mins[1] )) * w->size[0] + ( x - w->mins[0] )];
}

#endif // VOXEL_API_H
