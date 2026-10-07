/*
scnet.h - Svencraft: the block world and the dug-out town over the network (common/scnet.c, client/cl_scnet.c)
*/
#ifndef SCNET_H
#define SCNET_H

#define SCNET_SNAPSHOT_FILE	"!scworld.scw"	// the file-stream name of a snapshot ('!': kept in memory, never written)

// what a svc_scworld message carries
enum
{
	SCNET_VOX_SET = 0,	// [long seq][short x y z][short id]
	SCNET_VOX_FILL,		// [long seq][short x0 y0 z0 x1 y1 z1][short id]
	SCNET_VOX_FLAGS,	// [long seq][byte id][byte flags]
	SCNET_DYN_CARVE,	// [long seq][float mins[3] maxs[3]]
};

// server side, and for saves (common/scnet.c)
void SCNet_NewMap( void );			// the server loaded a map: the carve journal starts over
qboolean SCNet_Recording( void );		// a server is running and the change isn't one being applied
void SCNet_VoxSet( int x, int y, int z, int id );
void SCNet_VoxFill( int x0, int y0, int z0, int x1, int y1, int z1, int id );
void SCNet_VoxFlags( int id, int flags );
void SCNet_VoxInit( void );			// a new block world while players are in: they all get it again
void SCNet_DynCarve( const float *mins, const float *maxs );
void SCNet_ClientBegin( void *client );		// sv_client_t: a player has entered the game
void SCNet_ClientAck( void *client );		// ... and has the world now
void SCNet_Frame( void );
byte *SCNet_BuildSnapshot( int *size, poolhandle_t pool );	// bzip2-packed; Mem_Free it
qboolean SCNet_ApplySnapshot( const byte *data, int size, unsigned int *seq, qboolean journal );
int SCNet_SaveWorldFile( const char *path );	// saving (vox_api_t)
int SCNet_LoadWorldFile( const char *path );
int SCNet_WriteFile( const char *path, const void *data, int size );
int SCNet_RenameFile( const char *oldpath, const char *newpath );
void SCNet_SetApplying( qboolean on );		// changes made while applying aren't recorded

// client side (client/cl_scnet.c)
void SCNet_ClientNewMap( void );		// a remote client loaded a map
void SCNet_ParseEdit( sizebuf_t *msg );		// svc_scworld
qboolean SCNet_ClientFile( const char *name, byte *data, int size );	// a received in-memory file: ours?

#endif // SCNET_H
