/*
cl_scnet.c - Svencraft: the block world and the dug-out town arriving from a server on another machine
(the server side and the snapshot format: common/scnet.c)

On the host's own client (a listen server) none of this runs: client and server share one world. Here:
- loading a map loads maps/<map>.dyn too (the server's copy can't be seen from here) and empties the block world;
- the snapshot arrives through the file stream as SCNET_SNAPSHOT_FILE: it is put in place, the changes that
  arrived before it and are newer than it are applied, and the server is told ("scworld_ack");
- every svc_scworld after that is applied as it comes.
*/
#include "common.h"
#include "client.h"
#include "protocol.h"
#include "voxel.h"
#include "dynworld.h"
#include "scnet.h"

typedef struct
{
	int		type;
	unsigned int	seq;
	float		v[6];
	int		id;
} snedit_t;

static snedit_t *sc_queue;	// changes that came before the snapshot
static int sc_nqueue, sc_maxqueue;
static qboolean sc_have;	// the snapshot is in place
static unsigned int sc_seq;	// the change number it was taken at

void SCNet_ClientNewMap( void )
{
	char name[MAX_QPATH];

	sc_have = false;
	sc_seq = 0;
	sc_nqueue = 0;
	Vox_Clear();
	if( !cl.worldmodel )
		return;
	COM_FileBase( cl.worldmodel->name, name, sizeof( name ));
	Dyn_LoadForMap( name, cl.worldmodel );
}

static void SCNet_Apply( const snedit_t *e )
{
	int v[6];
	for( int k = 0; k < 6; k++ )
		v[k] = (int)e->v[k];
	switch( e->type )
	{
	case SCNET_VOX_SET:	Vox_NetSet( v[0], v[1], v[2], e->id ); break;
	case SCNET_VOX_FILL:	Vox_NetFill( v[0], v[1], v[2], v[3], v[4], v[5], e->id ); break;
	case SCNET_VOX_FLAGS:	Vox_NetFlags( e->id, v[0] ); break;
	case SCNET_DYN_CARVE:	Dyn_CarveBox( e->v, e->v + 3, NULL, 0 ); break;
	}
}

void SCNet_ParseEdit( sizebuf_t *msg )
{
	snedit_t e;
	int k;

	memset( &e, 0, sizeof( e ));
	e.type = MSG_ReadByte( msg );
	e.seq = (unsigned int)MSG_ReadLong( msg );
	switch( e.type )
	{
	case SCNET_VOX_SET:
		for( k = 0; k < 3; k++ ) e.v[k] = MSG_ReadShort( msg );
		e.id = MSG_ReadShort( msg );
		break;
	case SCNET_VOX_FILL:
		for( k = 0; k < 6; k++ ) e.v[k] = MSG_ReadShort( msg );
		e.id = MSG_ReadShort( msg );
		break;
	case SCNET_VOX_FLAGS:
		e.id = MSG_ReadByte( msg );
		e.v[0] = MSG_ReadByte( msg );
		break;
	case SCNET_DYN_CARVE:
		for( k = 0; k < 6; k++ ) e.v[k] = MSG_ReadFloat( msg );
		break;
	default:
		Host_Error( "svc_scworld: unknown change %d\n", e.type );
		return;
	}
	if( SV_Active())
		return;	// the host shares the server's world (and isn't sent these)
	if( !sc_have )
	{
		if( sc_nqueue == sc_maxqueue )
		{
			sc_maxqueue = sc_maxqueue ? sc_maxqueue * 2 : 256;
			snedit_t *q = Mem_Malloc( host.mempool, sizeof( *q ) * sc_maxqueue );
			if( sc_queue )
			{
				memcpy( q, sc_queue, sizeof( *q ) * sc_nqueue );
				Mem_Free( sc_queue );
			}
			sc_queue = q;
		}
		sc_queue[sc_nqueue++] = e;
		return;
	}
	if( e.seq > sc_seq )
		SCNet_Apply( &e );
}

qboolean SCNet_ClientFile( const char *name, byte *data, int size )
{
	unsigned int seq = 0;

	if( Q_stricmp( name, SCNET_SNAPSHOT_FILE ))
		return false;
	if( SV_Active())
		return true;
	if( data && SCNet_ApplySnapshot( data, size, &seq, false ))
	{
		sc_have = true;
		sc_seq = seq;
		for( int i = 0; i < sc_nqueue; i++ )
			if( sc_queue[i].seq > seq )
				SCNet_Apply( &sc_queue[i] );
	}
	else
	{
		// carry on with what we have rather than wait for ever; the server lets us move either way
		Con_Printf( S_ERROR "the world from the server couldn't be used: the blocks and dug ground may look wrong\n" );
		sc_have = true;
	}
	sc_nqueue = 0;
	CL_ServerCommand( true, "scworld_ack" );
	return true;
}
