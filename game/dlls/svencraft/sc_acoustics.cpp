/*
sc_acoustics.cpp - Svencraft: rooms sound like rooms (and a head stuck in a block suffocates)

Half-Life colours what a player hears with a "room type" (the DSP's echo and reverb), set by env_sound entities
placed in the map. Svencraft's worlds change as they are dug and its caves are generated, so the room type is
worked out around each player instead, twice a second: under the open sky nothing; under a roof, a room of the
size the walls around say (concrete small, medium, large); under the block world's top, a cavern of that size.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "sc_world.h"
#include "sc_game.h"

// Half-Life's room types (dlls/sound.cpp): 0 normal, 17-19 concrete small/medium/large, 23-25 cavern small/medium/large
#define ROOM_OUTSIDE		0
#define ROOM_CONCRETE_SMALL	17
#define ROOM_CAVERN_SMALL	23

#define SC_MAX_PLAYERS		32
static float g_flNextAcoustics[SC_MAX_PLAYERS + 1];

static int SC_RoomType( CBasePlayer *pPlayer )
{
	Vector eye = pPlayer->pev->origin + pPlayer->pev->view_ofs;
	TraceResult tr;

	// the sky right above: outside
	UTIL_TraceLine( eye, eye + Vector( 0, 0, 2048 ), ignore_monsters, pPlayer->edict(), &tr );
	if( tr.flFraction >= 1.0f )
		return ROOM_OUTSIDE;
	const char *tex = TRACE_TEXTURE( INDEXENT( 0 ), eye, tr.vecEndPos + Vector( 0, 0, 8 ));
	if(( tex && !stricmp( tex, "sky" )) || UTIL_PointContents( tr.vecEndPos - Vector( 0, 0, 2 )) == CONTENTS_SKY
		|| UTIL_PointContents( tr.vecEndPos + Vector( 0, 0, 4 )) == CONTENTS_SKY )
		return ROOM_OUTSIDE;

	// how far the walls are, on average, around and overhead
	float sum = ( tr.vecEndPos - eye ).Length() * 2.0f;
	for( int i = 0; i < 8; i++ )
	{
		float a = i * M_PI / 4.0f;
		UTIL_TraceLine( eye, eye + Vector( cos( a ), sin( a ), 0 ) * 1024.0f, ignore_monsters, pPlayer->edict(), &tr );
		sum += 1024.0f * tr.flFraction;
	}
	float size = sum / 10.0f;
	int step = size < 110.0f ? 0 : size < 300.0f ? 1 : 2;

	// below the realistic ground, in the block world: a cave
	vox_api_t *v = SC_Vox();
	bool cave = v && v->World()->active && eye.z < SC_BlockTop() * VOX_BLOCK_SIZE;
	return ( cave ? ROOM_CAVERN_SMALL : ROOM_CONCRETE_SMALL ) + step;
}

void SC_PlayerAcoustics( CBasePlayer *pPlayer )
{
	int idx = pPlayer->entindex();
	if( idx < 1 || idx > SC_MAX_PLAYERS || gpGlobals->time < g_flNextAcoustics[idx] )
		return;
	g_flNextAcoustics[idx] = gpGlobals->time + 0.5f;
	if( g_flNextAcoustics[idx] > gpGlobals->time + 1.0f )
		g_flNextAcoustics[idx] = gpGlobals->time;	// a new map's clock

	// a map's own env_sound decides where it reaches (it keeps the player until another takes over)
	if( !FNullEnt( pPlayer->m_pentSndLast ))
		return;
	// UpdateClientData sends it when it changes (player.cpp, from hlfixed)
	pPlayer->m_SndRoomtype = SC_RoomType( pPlayer );
}

// Minecraft's suffocation: a head inside a solid block (sand fell on it, a block was put there) takes 1 every half
// second there, 5 here
static float g_flNextSuffocate[SC_MAX_PLAYERS + 1];

void SC_PlayerSuffocate( CBasePlayer *pPlayer )
{
	int idx = pPlayer->entindex();
	if( idx < 1 || idx > SC_MAX_PLAYERS || gpGlobals->time < g_flNextSuffocate[idx] || pPlayer->pev->movetype == MOVETYPE_NOCLIP )
		return;
	g_flNextSuffocate[idx] = gpGlobals->time + 0.5f;
	Vector eye = pPlayer->pev->origin + pPlayer->pev->view_ofs;
	vox_api_t *v = SC_Vox();
	dyn_api_t *d = SC_Dyn();
	bool stuck = false;
	if( v && v->World()->active )
	{
		int id = v->Get( (int)floor( eye.x / VOX_BLOCK_SIZE ), (int)floor( eye.y / VOX_BLOCK_SIZE ), (int)floor( eye.z / VOX_BLOCK_SIZE ));
		stuck = id > BLOCK_AIR && id < BLOCK_COUNT && id != BLOCK_GLASS && id != BLOCK_LEAVES && !SC_IsTorch( id );
	}
	if( !stuck && d && d->World()->active )
		stuck = d->PointSolid( eye ) != 0;
	if( stuck )
		pPlayer->TakeDamage( VARS( INDEXENT( 0 )), VARS( INDEXENT( 0 )), 5.0f, DMG_CRUSH );
}

//
// sc_ambient_random: one of a list of sounds now and then (birds, a dog, a bell, a phone): Half-Life has no
// entity for that. Keys: sounds ("a.wav;b.wav"), mindelay/maxdelay (seconds), volume (0..1), attenuation (as
// EMIT_SOUND's: 0 everywhere .. 2 close), spread (each plays up to this far from the entity, for birds in trees).
//
class CSCAmbientRandom : public CPointEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT RandomThink( void );

private:
	char	m_szSounds[512];
	int	m_iOffsets[16], m_iCount;
	float	m_flMin, m_flMax, m_flVolume, m_flAttn, m_flSpread;
};

LINK_ENTITY_TO_CLASS( sc_ambient_random, CSCAmbientRandom );

void CSCAmbientRandom::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "sounds" ))
		strncpy( m_szSounds, pkvd->szValue, sizeof( m_szSounds ) - 1 );
	else if( FStrEq( pkvd->szKeyName, "mindelay" ))
		m_flMin = atof( pkvd->szValue );
	else if( FStrEq( pkvd->szKeyName, "maxdelay" ))
		m_flMax = atof( pkvd->szValue );
	else if( FStrEq( pkvd->szKeyName, "volume" ))
		m_flVolume = atof( pkvd->szValue );
	else if( FStrEq( pkvd->szKeyName, "attenuation" ))
		m_flAttn = atof( pkvd->szValue );
	else if( FStrEq( pkvd->szKeyName, "spread" ))
		m_flSpread = atof( pkvd->szValue );
	else
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

void CSCAmbientRandom::Precache( void )
{
	// split the list in place: each sound's offset into m_szSounds
	m_iCount = 0;
	char *p = m_szSounds;
	while( *p && m_iCount < 16 )
	{
		m_iOffsets[m_iCount++] = p - m_szSounds;
		char *semi = strchr( p, ';' );
		if( !semi )
			break;
		*semi = 0;
		p = semi + 1;
	}
	for( int i = 0; i < m_iCount; i++ )
		PRECACHE_SOUND( (char *)STRING( ALLOC_STRING( m_szSounds + m_iOffsets[i] )));
}

void CSCAmbientRandom::Spawn( void )
{
	if( m_flMax < m_flMin )
		m_flMax = m_flMin;
	if( m_flVolume <= 0 )
		m_flVolume = 1.0f;
	Precache();
	SetThink( &CSCAmbientRandom::RandomThink );
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( m_flMin, m_flMax );
}

void CSCAmbientRandom::RandomThink( void )
{
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( Q_max( m_flMin, 0.5f ), Q_max( m_flMax, 0.5f ));
	if( !m_iCount )
		return;
	Vector at = pev->origin + Vector( RANDOM_FLOAT( -m_flSpread, m_flSpread ), RANDOM_FLOAT( -m_flSpread, m_flSpread ), 0 );
	EMIT_AMBIENT_SOUND( edict(), at, m_szSounds + m_iOffsets[RANDOM_LONG( 0, m_iCount - 1 )], m_flVolume, m_flAttn, 0, RANDOM_LONG( 94, 106 ));
}
