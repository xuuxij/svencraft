/*
scnet.c - Svencraft: the block world and the dug-out town over the network (server side, and snapshots)

On a listen server the host's own client shares the server's memory: it needs nothing from here. A player on
another machine has their own engine, so:
- their client loads maps/<map>.dyn itself when it loads the map (client/cl_scnet.c);
- when they enter the game (the "begin" command) the server sends them a snapshot of the world as it is now: the
  block world (bounds, every block id's flags, the blocks) and every carve made in the diggable geometry since the
  map loaded, bzip2-packed and sent through the netchan's file stream as SNAPSHOT_FILE; they are held still
  (FL_FROZEN) until their client says it has it ("scworld_ack");
- every change after that reaches them as a numbered svc_scworld message (a block set, a fill, a flag change, a
  carve); changes that overtake the snapshot wait on the client until it arrives, and the ones the snapshot
  already holds are skipped.
The same snapshot is what a saved world keeps (SCNet_BuildSnapshot / SCNet_ApplySnapshot).
*/
#include "common.h"
#include "server.h"
#include "protocol.h"
#include "netchan.h"
#include "voxel.h"
#include "dynworld.h"
#include "scnet.h"
#include <bzlib.h>

#define SNAP_MAGIC	0x31574353	// "SCW1"
#define FREEZE_LIMIT	30.0	// a client that never says it has the world is let go after this

static unsigned int sn_seq;		// the number of the last change
static float (*sn_carves)[6];		// every carve since the map loaded, in order
static int sn_numcarves, sn_maxcarves;
static qboolean sn_applying;
static qboolean sn_resync;		// the block world was made anew while players were in

typedef struct
{
	int	userid;		// the connection the snapshot went to (0: none yet)
	qboolean	froze;		// we set FL_FROZEN on them
	double	sent;		// when (host.realtime)
} sncl_t;
static sncl_t sn_cl[MAX_CLIENTS];

void SCNet_SetApplying( qboolean on )
{
	sn_applying = on;
}

qboolean SCNet_Recording( void )
{
	return !sn_applying && SV_Active();
}

void SCNet_NewMap( void )
{
	if( sn_carves )
		Mem_Free( sn_carves );
	sn_carves = NULL;
	sn_numcarves = sn_maxcarves = 0;
	sn_resync = false;
	memset( sn_cl, 0, sizeof( sn_cl ));
}

// the players a change goes to: on another machine, in the game, and given a snapshot on this connection
static qboolean SCNet_Synced( sv_client_t *cl, int i )
{
	return cl->state == cs_spawned && !FBitSet( cl->flags, FCL_FAKECLIENT ) && !NET_IsLocalAddress( cl->netchan.remote_address )
		&& sn_cl[i].userid == cl->userid && cl->userid != 0;
}

static void SCNet_Begin( int type )
{
	sn_seq++;
	(void)type;
}

#define FOR_SYNCED( cl, i ) \
	for( i = 0, cl = svs.clients; svs.clients && i < svs.maxclients && i < MAX_CLIENTS; i++, cl++ ) \
		if( SCNet_Synced( cl, i ))

void SCNet_VoxSet( int x, int y, int z, int id )
{
	sv_client_t *cl;
	int i;
	if( !SCNet_Recording())
		return;
	SCNet_Begin( SCNET_VOX_SET );
	FOR_SYNCED( cl, i )
	{
		sizebuf_t *m = &cl->netchan.message;
		MSG_BeginServerCmd( m, svc_scworld );
		MSG_WriteByte( m, SCNET_VOX_SET );
		MSG_WriteLong( m, sn_seq );
		MSG_WriteShort( m, x ); MSG_WriteShort( m, y ); MSG_WriteShort( m, z );
		MSG_WriteShort( m, id );
	}
}

void SCNet_VoxFill( int x0, int y0, int z0, int x1, int y1, int z1, int id )
{
	sv_client_t *cl;
	int i;
	if( !SCNet_Recording())
		return;
	SCNet_Begin( SCNET_VOX_FILL );
	FOR_SYNCED( cl, i )
	{
		sizebuf_t *m = &cl->netchan.message;
		MSG_BeginServerCmd( m, svc_scworld );
		MSG_WriteByte( m, SCNET_VOX_FILL );
		MSG_WriteLong( m, sn_seq );
		MSG_WriteShort( m, x0 ); MSG_WriteShort( m, y0 ); MSG_WriteShort( m, z0 );
		MSG_WriteShort( m, x1 ); MSG_WriteShort( m, y1 ); MSG_WriteShort( m, z1 );
		MSG_WriteShort( m, id );
	}
}

void SCNet_VoxFlags( int id, int flags )
{
	sv_client_t *cl;
	int i;
	if( !SCNet_Recording())
		return;
	SCNet_Begin( SCNET_VOX_FLAGS );
	FOR_SYNCED( cl, i )
	{
		sizebuf_t *m = &cl->netchan.message;
		MSG_BeginServerCmd( m, svc_scworld );
		MSG_WriteByte( m, SCNET_VOX_FLAGS );
		MSG_WriteLong( m, sn_seq );
		MSG_WriteByte( m, id );
		MSG_WriteByte( m, flags );
	}
}

void SCNet_VoxInit( void )
{
	if( !SCNet_Recording())
		return;
	sn_seq++;
	sn_resync = true;	// whoever is in gets the new world in the next frame (SCNet_Frame)
}

void SCNet_DynCarve( const float *mins, const float *maxs )
{
	sv_client_t *cl;
	int i;
	if( !SCNet_Recording())
		return;
	if( sn_numcarves == sn_maxcarves )
	{
		sn_maxcarves = sn_maxcarves ? sn_maxcarves * 2 : 1024;
		float (*grown)[6] = Mem_Malloc( host.mempool, sizeof( *grown ) * sn_maxcarves );
		if( sn_carves )
		{
			memcpy( grown, sn_carves, sizeof( *grown ) * sn_numcarves );
			Mem_Free( sn_carves );
		}
		sn_carves = grown;
	}
	VectorCopy( mins, sn_carves[sn_numcarves] );
	VectorCopy( maxs, sn_carves[sn_numcarves] + 3 );
	sn_numcarves++;

	SCNet_Begin( SCNET_DYN_CARVE );
	FOR_SYNCED( cl, i )
	{
		sizebuf_t *m = &cl->netchan.message;
		MSG_BeginServerCmd( m, svc_scworld );
		MSG_WriteByte( m, SCNET_DYN_CARVE );
		MSG_WriteLong( m, sn_seq );
		for( int k = 0; k < 3; k++ ) MSG_WriteFloat( m, mins[k] );
		for( int k = 0; k < 3; k++ ) MSG_WriteFloat( m, maxs[k] );
	}
}

//
// snapshots
//
typedef struct
{
	byte	*data;
	int	size, max;
	poolhandle_t pool;
} snbuf_t;

static void SB_Put( snbuf_t *b, const void *p, int n )
{
	if( b->size + n > b->max )
	{
		int max = Q_max( b->max * 2, b->size + n + 65536 );
		byte *d = Mem_Malloc( b->pool, max );
		if( b->data )
		{
			memcpy( d, b->data, b->size );
			Mem_Free( b->data );
		}
		b->data = d;
		b->max = max;
	}
	memcpy( b->data + b->size, p, n );
	b->size += n;
}

static void SB_Int( snbuf_t *b, int v )
{
	SB_Put( b, &v, sizeof( v ));
}

// [magic][uncompressed size] then bzip2 of: [seq][vox active][mins maxs][flags 256][blocks][dyn active][n carves][carves]
byte *SCNet_BuildSnapshot( int *size, poolhandle_t pool )
{
	snbuf_t raw = { NULL, 0, 0, pool };
	const vox_world_t *w = Vox_World();

	SB_Int( &raw, (int)sn_seq );
	SB_Int( &raw, Vox_Active() ? 1 : 0 );
	if( Vox_Active())
	{
		for( int i = 0; i < 3; i++ ) SB_Int( &raw, w->mins[i] );
		for( int i = 0; i < 3; i++ ) SB_Int( &raw, w->maxs[i] );
		SB_Put( &raw, w->flags, VOX_MAX_IDS );
		SB_Put( &raw, w->blocks, sizeof( *w->blocks ) * w->size[0] * w->size[1] * w->size[2] );
	}
	SB_Int( &raw, Dyn_Active() ? 1 : 0 );
	SB_Int( &raw, sn_numcarves );
	if( sn_numcarves )
		SB_Put( &raw, sn_carves, sizeof( *sn_carves ) * sn_numcarves );

	unsigned int packed = raw.size + raw.size / 100 + 1024;
	byte *out = Mem_Malloc( pool, packed + 8 );
	if( BZ2_bzBuffToBuffCompress( (char *)out + 8, &packed, (char *)raw.data, raw.size, 9, 0, 30 ) != BZ_OK )
	{
		Mem_Free( raw.data );
		Mem_Free( out );
		*size = 0;
		return NULL;
	}
	((int *)out)[0] = SNAP_MAGIC;
	((int *)out)[1] = raw.size;
	*size = packed + 8;
	Mem_Free( raw.data );
	return out;
}

// puts a snapshot's world in place; seq gets the change number it was taken at. journal: the server loading a
// saved world (its carves go into the journal, so players who join later get them); a client: no
qboolean SCNet_ApplySnapshot( const byte *data, int size, unsigned int *seq, qboolean journal )
{
	if( size < 8 || ((const int *)data)[0] != SNAP_MAGIC )
	{
		Con_Printf( S_ERROR "world snapshot: not one\n" );
		return false;
	}
	unsigned int rawsize = ((const int *)data)[1];
	if( rawsize < 12 || rawsize > 256 * 1024 * 1024 )
	{
		Con_Printf( S_ERROR "world snapshot: bad size %u\n", rawsize );
		return false;
	}
	byte *raw = Mem_Malloc( host.mempool, rawsize );
	unsigned int got = rawsize;
	if( BZ2_bzBuffToBuffDecompress( (char *)raw, &got, (char *)data + 8, size - 8, 0, 0 ) != BZ_OK || got != rawsize )
	{
		Con_Printf( S_ERROR "world snapshot: can't unpack it\n" );
		Mem_Free( raw );
		return false;
	}

	const byte *p = raw, *end = raw + rawsize;
#define TAKE( dst, n ) do { if( p + ( n ) > end ) goto bad; memcpy(( dst ), p, ( n )); p += ( n ); } while( 0 )
	int s, voxactive, dynactive, ncarves, mins[3], maxs[3];
	byte flags[VOX_MAX_IDS];
	TAKE( &s, 4 );
	TAKE( &voxactive, 4 );
	SCNet_SetApplying( !journal );
	if( voxactive )
	{
		TAKE( mins, 12 );
		TAKE( maxs, 12 );
		TAKE( flags, VOX_MAX_IDS );
		int n = ( maxs[0] - mins[0] ) * ( maxs[1] - mins[1] ) * ( maxs[2] - mins[2] );
		if( n <= 0 || p + n * 2 > end || !Vox_LoadWorld( mins, maxs, flags, (const unsigned short *)p ))
			goto bad;
		p += n * 2;
	}
	else
		Vox_Clear();
	TAKE( &dynactive, 4 );
	TAKE( &ncarves, 4 );
	if( ncarves < 0 || p + ncarves * 24 > end )
		goto bad;
	if( ncarves && !Dyn_Active())
		Con_Printf( S_WARN "world snapshot: %d carves, but this map has no diggable geometry here (maps/<map>.dyn missing?)\n", ncarves );
	for( int i = 0; i < ncarves; i++, p += 24 )
	{
		float box[6];
		memcpy( box, p, 24 );
		Dyn_CarveBox( box, box + 3, NULL, 0 );
	}
	SCNet_SetApplying( false );
	Mem_Free( raw );
	if( seq ) *seq = (unsigned int)s;
	Con_Reportf( "world snapshot: %d bytes packed, %u unpacked, blocks %s, %d carves\n", size, rawsize, voxactive ? "yes" : "no", ncarves );
	return true;
bad:
#undef TAKE
	SCNet_SetApplying( false );
	Mem_Free( raw );
	Con_Printf( S_ERROR "world snapshot: cut short\n" );
	return false;
}

//
// saving (vox_api_t: the game's sc_save.cpp)
//
int SCNet_SaveWorldFile( const char *path )
{
	int size;
	byte *snap = SCNet_BuildSnapshot( &size, host.mempool );
	if( !snap )
		return false;
	qboolean ok = FS_WriteFile( path, snap, size );
	Mem_Free( snap );
	return ok;
}

int SCNet_LoadWorldFile( const char *path )
{
	fs_offset_t size;
	byte *data = FS_LoadFile( path, &size, true );
	if( !data )
		return false;
	unsigned int seq;
	qboolean ok = SCNet_ApplySnapshot( data, (int)size, &seq, true );
	Mem_Free( data );
	return ok;
}

int SCNet_WriteFile( const char *path, const void *data, int size )
{
	return FS_WriteFile( path, data, size );
}

int SCNet_RenameFile( const char *oldpath, const char *newpath )
{
	if( !newpath )
		return FS_Delete( oldpath );
	FS_Delete( newpath );
	return FS_Rename( oldpath, newpath );
}

//
// players joining
//
static void SCNet_SendSnapshot( sv_client_t *cl, int i, qboolean freeze )
{
	int size;
	byte *snap = SCNet_BuildSnapshot( &size, host.mempool );
	if( !snap )
	{
		Con_Printf( S_ERROR "world snapshot: can't pack it for %s\n", cl->name );
		return;
	}
	Netchan_CreateFileFragmentsFromBuffer( &cl->netchan, SCNET_SNAPSHOT_FILE, snap, size );
	Netchan_FragSend( &cl->netchan );
	Mem_Free( snap );
	sn_cl[i].userid = cl->userid;
	sn_cl[i].sent = host.realtime;
	if( freeze && cl->edict && !FBitSet( cl->edict->v.flags, FL_FROZEN ))
	{
		SetBits( cl->edict->v.flags, FL_FROZEN );
		sn_cl[i].froze = true;
		MSG_BeginServerCmd( &cl->netchan.message, svc_centerprint );
		MSG_WriteString( &cl->netchan.message, "Receiving the world..." );
	}
	Con_Reportf( "world snapshot: %d bytes to %s\n", size, cl->name );
}

void SCNet_ClientBegin( void *client )
{
	sv_client_t *cl = client;
	int i = cl - svs.clients;
	if( i < 0 || i >= MAX_CLIENTS || FBitSet( cl->flags, FCL_FAKECLIENT ) || NET_IsLocalAddress( cl->netchan.remote_address ))
		return;	// the host's own client shares this world
	sn_cl[i].froze = false;
	if( !Vox_Active() && !Dyn_Active())
	{
		sn_cl[i].userid = cl->userid;	// nothing to send: changes from now on still reach them
		return;
	}
	SCNet_SendSnapshot( cl, i, true );
}

void SCNet_ClientAck( void *client )
{
	sv_client_t *cl = client;
	int i = cl - svs.clients;
	if( i < 0 || i >= MAX_CLIENTS )
		return;
	if( sn_cl[i].froze && cl->edict )
		ClearBits( cl->edict->v.flags, FL_FROZEN );
	sn_cl[i].froze = false;
}

void SCNet_Frame( void )
{
	sv_client_t *cl;
	int i;

	if( !svs.clients )
		return;
	if( sn_resync )
	{
		sn_resync = false;
		FOR_SYNCED( cl, i )
			SCNet_SendSnapshot( cl, i, false );
	}
	// nobody stays frozen for ever waiting for a world that won't come
	for( i = 0, cl = svs.clients; i < svs.maxclients && i < MAX_CLIENTS; i++, cl++ )
	{
		if( sn_cl[i].froze && host.realtime - sn_cl[i].sent > FREEZE_LIMIT )
		{
			if( cl->edict )
				ClearBits( cl->edict->v.flags, FL_FROZEN );
			sn_cl[i].froze = false;
			Con_Printf( S_WARN "world snapshot: %s never confirmed it\n", cl->name );
		}
	}
}
