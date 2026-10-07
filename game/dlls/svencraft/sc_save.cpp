/*
sc_save.cpp - Svencraft: keeping a world between sessions

What a world keeps, in worlds/<map>/ in the game folder:
- world.scw: the blocks and everything dug out of the town, the engine's snapshot (common/scnet.c: the same one a
  player joining over the network gets);
- game.txt: the rest, in lines of text: each player's inventory, ammunition, health, armour and where they stood
  (players get theirs back by name, as they join), the furnaces and what is in them, the items lying around.
It is saved every sc_autosave seconds (120; 0: never on a timer), when the map ends, and on sc_save; a map that has
one starts from it instead of a newly generated world (sc_worldsave 0: always a new world, nothing saved).
sc_newworld puts the current one aside (world.scw.old, game.txt.old) and starts the map again with a new world.
Not kept yet: monsters (the map brings its own), wrecked cars and broken glass, the map's own pickups.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "sc_world.h"
#include "sc_game.h"

static cvar_t sc_autosave = { "sc_autosave", "120", FCVAR_SERVER };	// seconds between saves (0: none on a timer)
static cvar_t sc_worldsave = { "sc_worldsave", "1", FCVAR_SERVER };	// 0: worlds are neither loaded nor saved

#define SC_SAVE_PLAYERS	64
#define SC_SAVE_AMMO	32

struct scsaveplayer_t
{
	bool	used;
	char	name[32];
	scslot_t slot[SC_INV_SLOTS];
	scslot_t extra[10];		// what was in the crafting grid or on the mouse: back into the inventory
	int	hotbar;
	bool	kit;
	float	health, armor;
	bool	hasPos;
	Vector	pos, ang;
	float	partial[BLOCK_COUNT];
	int	nammo;
	char	ammoName[SC_SAVE_AMMO][32];
	int	ammo[SC_SAVE_AMMO];
};

static scsaveplayer_t g_SCSaved[SC_SAVE_PLAYERS];
static float g_flSCNextSave;
static bool g_bSCNoSave;	// sc_newworld: the world being left isn't saved over the fresh one
static bool g_bSCWorldMap;	// this map has a block world (and so a world to keep)

static const char *SC_SavePath( const char *file )
{
	static char path[2][128];
	static int n;
	n ^= 1;
	snprintf( path[n], sizeof( path[n] ), "worlds/%s/%s", STRING( gpGlobals->mapname ), file );
	return path[n];
}

static scsaveplayer_t *SC_SavedFor( const char *name, bool create )
{
	scsaveplayer_t *pFree = NULL;
	for( int i = 0; i < SC_SAVE_PLAYERS; i++ )
	{
		if( g_SCSaved[i].used && !strcmp( g_SCSaved[i].name, name ))
			return &g_SCSaved[i];
		if( !g_SCSaved[i].used && !pFree )
			pFree = &g_SCSaved[i];
	}
	if( !create || !pFree )
		return NULL;
	memset( pFree, 0, sizeof( *pFree ));
	pFree->used = true;
	strncpy( pFree->name, name, sizeof( pFree->name ) - 1 );
	return pFree;
}

// a player as they are now, into their record
static void SC_SaveCapture( CBasePlayer *pPlayer )
{
	if( FStringNull( pPlayer->pev->netname ) || !*STRING( pPlayer->pev->netname ))
		return;
	scsaveplayer_t *r = SC_SavedFor( STRING( pPlayer->pev->netname ), true );
	if( !r )
		return;
	SCInventory &inv = SC_Inv( pPlayer );
	memcpy( r->slot, inv.slot, sizeof( r->slot ));
	memset( r->extra, 0, sizeof( r->extra ));
	int e = 0;
	for( int i = 0; i < 9 && e < 10; i++ )
		if( inv.grid[i].count > 0 )
			r->extra[e++] = inv.grid[i];
	if( inv.cursor.count > 0 && e < 10 )
		r->extra[e++] = inv.cursor;
	r->hotbar = inv.hotbar;
	r->kit = inv.startKit;
	memcpy( r->partial, inv.partial, sizeof( r->partial ));
	r->health = pPlayer->pev->health;
	r->armor = pPlayer->pev->armorvalue;
	r->hasPos = pPlayer->IsAlive();
	r->pos = pPlayer->pev->origin;
	r->ang = pPlayer->pev->v_angle;
	// ammunition, with what is loaded in the guns (they come back unloaded)
	r->nammo = 0;
	for( int i = 0; i < MAX_AMMO_SLOTS && r->nammo < SC_SAVE_AMMO; i++ )
	{
		const char *name = CBasePlayerItem::AmmoInfoArray[i].pszName;
		if( !name || !*name )
			continue;
		int n = pPlayer->m_rgAmmo[i];
		for( int t = 0; t < MAX_ITEM_TYPES; t++ )
			for( CBasePlayerItem *p = pPlayer->m_rgpPlayerItems[t]; p; p = p->m_pNext )
			{
				CBasePlayerWeapon *w = (CBasePlayerWeapon *)p->GetWeaponPtr();
				if( w && w->m_iPrimaryAmmoType == i && w->m_iClip > 0 )
					n += w->m_iClip;
			}
		if( n <= 0 )
			continue;
		strncpy( r->ammoName[r->nammo], name, 31 );
		r->ammoName[r->nammo][31] = 0;
		r->ammo[r->nammo++] = n;
	}
}

// joining (the first spawn of a connection): a player of this world gets their things back
bool SC_SaveRestorePlayer( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	if( inv.saveChecked )
		return false;
	inv.saveChecked = true;
	if( !g_bSCWorldMap || FStringNull( pPlayer->pev->netname ))
		return false;
	scsaveplayer_t *r = SC_SavedFor( STRING( pPlayer->pev->netname ), false );
	if( !r )
		return false;
	memcpy( inv.slot, r->slot, sizeof( inv.slot ));
	for( int e = 0; e < 10; e++ )
		if( r->extra[e].count > 0 && SC_InvAdd( pPlayer, r->extra[e].id, r->extra[e].count, r->extra[e].dmg ) > 0 )
			SC_SpawnDrop( pPlayer->pev->origin + Vector( 0, 0, 16 ), r->extra[e].id, r->extra[e].count, r->extra[e].dmg );
	inv.hotbar = Q_min( Q_max( r->hotbar, 0 ), SC_HOTBAR_SLOTS - 1 );
	inv.startKit = true;	// they had their starting tools in this world already
	memcpy( inv.partial, r->partial, sizeof( inv.partial ));
	for( int i = 0; i < r->nammo; i++ )
	{
		int a = CBasePlayer::GetAmmoIndex( r->ammoName[i] );
		if( a >= 0 )
			pPlayer->m_rgAmmo[a] = r->ammo[i];
	}
	if( r->health > 0 )
		pPlayer->pev->health = Q_min( r->health, pPlayer->pev->max_health );
	pPlayer->pev->armorvalue = r->armor;
	if( r->hasPos )
	{
		// where they stood (a little above: standing, they touched the floor), if there's room there now
		Vector at = r->pos + Vector( 0, 0, 2 );
		TraceResult tr;
		UTIL_TraceHull( at, at, dont_ignore_monsters, human_hull, pPlayer->edict(), &tr );
		if( !tr.fStartSolid && !tr.fAllSolid )
		{
			UTIL_SetOrigin( pPlayer->pev, at );
			pPlayer->pev->velocity = g_vecZero;
			pPlayer->pev->angles = pPlayer->pev->v_angle = Vector( r->ang.x, r->ang.y, 0 );
			pPlayer->pev->fixangle = TRUE;
		}
	}
	ALERT( at_console, "world: %s gets back what they had\n", r->name );
	return true;
}

// someone leaving: their things are kept for when they come back
void SC_SavePlayerLeft( CBasePlayer *pPlayer )
{
	if( g_bSCWorldMap && pPlayer && pPlayer->IsAlive())
		SC_SaveCapture( pPlayer );
}

//
// writing
//
struct scbuf_t
{
	char *data;
	int len, max;
};

static void SC_Out( scbuf_t &b, const char *fmt, ... )
{
	char line[512];
	va_list args;
	va_start( args, fmt );
	int n = vsnprintf( line, sizeof( line ), fmt, args );
	va_end( args );
	if( n <= 0 )
		return;
	n = Q_min( n, (int)sizeof( line ) - 1 );
	if( b.len + n + 1 > b.max )
	{
		b.max = Q_max( b.max * 2, b.len + n + 4096 );
		char *d = (char *)malloc( b.max );
		if( b.data )
		{
			memcpy( d, b.data, b.len );
			free( b.data );
		}
		b.data = d;
	}
	memcpy( b.data + b.len, line, n );
	b.len += n;
	b.data[b.len] = 0;
}

static void SC_OutSlots( scbuf_t &b, const char *tag, const scslot_t *s, int n )
{
	for( int i = 0; i < n; i++ )
		if( s[i].count > 0 )
			SC_Out( b, " %s %d %d %d %d\n", tag, i, s[i].id, s[i].count, s[i].dmg );
}

bool SC_SaveWorld( const char *why )
{
	vox_api_t *v = SC_Vox();
	if( !v || !g_bSCWorldMap || g_bSCNoSave || sc_worldsave.value == 0.0f || !v->World()->active )
		return false;
	if( !v->SaveWorld( SC_SavePath( "world.scw" )))
	{
		ALERT( at_console, "world: couldn't save %s\n", SC_SavePath( "world.scw" ));
		return false;
	}

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( pl && pl->IsAlive() && !FStringNull( pl->pev->netname ))
			SC_SaveCapture( pl );
	}

	scbuf_t b = { NULL, 0, 0 };
	SC_Out( b, "svencraft-world 1\n" );
	SC_Out( b, "map %s\n", STRING( gpGlobals->mapname ));
	for( int i = 0; i < SC_SAVE_PLAYERS; i++ )
	{
		const scsaveplayer_t &r = g_SCSaved[i];
		if( !r.used )
			continue;
		SC_Out( b, "player \"%s\" hotbar %d kit %d health %.0f armor %.0f pos %d %.1f %.1f %.1f %.1f %.1f\n", r.name, r.hotbar,
			r.kit ? 1 : 0, r.health, r.armor, r.hasPos ? 1 : 0, r.pos.x, r.pos.y, r.pos.z, r.ang.x, r.ang.y );
		SC_OutSlots( b, "slot", r.slot, SC_INV_SLOTS );
		SC_OutSlots( b, "extra", r.extra, 10 );
		for( int k = 0; k < r.nammo; k++ )
			SC_Out( b, " ammo \"%s\" %d\n", r.ammoName[k], r.ammo[k] );
		for( int k = 0; k < BLOCK_COUNT; k++ )
			if( r.partial[k] > 0.0f )
				SC_Out( b, " partial %d %.3f\n", k, r.partial[k] );
		SC_Out( b, "end\n" );
	}
	// the furnaces
	int cell[3], burn, burnTotal, cook, used;
	bool lit;
	scslot_t fs[3];
	for( int f = 0; ( used = SC_FurnaceSave( f, cell, fs, &burn, &burnTotal, &cook, &lit )) >= 0; f++ )
	{
		if( !used )
			continue;
		SC_Out( b, "furnace %d %d %d burn %d %d cook %d lit %d\n", cell[0], cell[1], cell[2], burn, burnTotal, cook, lit ? 1 : 0 );
		SC_OutSlots( b, "fslot", fs, 3 );
		SC_Out( b, "end\n" );
	}
	// what lies around
	CBaseEntity *pDrop = NULL;
	while(( pDrop = UTIL_FindEntityByClassname( pDrop, "sc_drop" )) != NULL )
	{
		if( pDrop->pev->fuser1 > 0 )
			SC_Out( b, "drop %.1f %.1f %.1f %d %d %d\n", pDrop->pev->origin.x, pDrop->pev->origin.y, pDrop->pev->origin.z,
				pDrop->pev->iuser1, (int)pDrop->pev->fuser1, pDrop->pev->iuser2 );
	}
	bool ok = v->WriteFile( SC_SavePath( "game.txt" ), b.data, b.len ) != 0;
	free( b.data );
	ALERT( at_console, "world: saved (%s)\n", why );
	return ok;
}

//
// reading
//
static bool SC_LoadGameText( void )
{
	int len = 0;
	char *data = (char *)LOAD_FILE_FOR_ME( (char *)SC_SavePath( "game.txt" ), &len );
	if( !data )
		return false;
	char *text = (char *)malloc( len + 1 );
	memcpy( text, data, len );
	text[len] = 0;
	FREE_FILE( data );

	scsaveplayer_t *r = NULL;
	int furnace = -1, drops = 0, players = 0, furnaces = 0;
	for( char *line = strtok( text, "\r\n" ); line; line = strtok( NULL, "\r\n" ))
	{
		char name[64];
		int a, b, c, d, e, f, g;
		float x, y, z, p, q, h, ar;
		if( sscanf( line, "player \"%63[^\"]\" hotbar %d kit %d health %f armor %f pos %d %f %f %f %f %f", name, &a, &b, &h, &ar, &c,
			&x, &y, &z, &p, &q ) == 11 )
		{
			r = SC_SavedFor( name, true );
			if( r )
			{
				r->hotbar = a; r->kit = b != 0; r->health = h; r->armor = ar; r->hasPos = c != 0;
				r->pos = Vector( x, y, z ); r->ang = Vector( p, q, 0 );
				players++;
			}
		}
		else if( r && sscanf( line, " slot %d %d %d %d", &a, &b, &c, &d ) == 4 && a >= 0 && a < SC_INV_SLOTS )
			r->slot[a].id = b, r->slot[a].count = c, r->slot[a].dmg = d;
		else if( r && sscanf( line, " extra %d %d %d %d", &a, &b, &c, &d ) == 4 && a >= 0 && a < 10 )
			r->extra[a].id = b, r->extra[a].count = c, r->extra[a].dmg = d;
		else if( r && sscanf( line, " ammo \"%63[^\"]\" %d", name, &a ) == 2 && r->nammo < SC_SAVE_AMMO )
		{
			strncpy( r->ammoName[r->nammo], name, 31 );
			r->ammoName[r->nammo][31] = 0;
			r->ammo[r->nammo++] = a;
		}
		else if( r && sscanf( line, " partial %d %f", &a, &x ) == 2 && a >= 0 && a < BLOCK_COUNT )
			r->partial[a] = x;
		else if( sscanf( line, "furnace %d %d %d burn %d %d cook %d lit %d", &a, &b, &c, &d, &e, &f, &g ) == 7 )
		{
			int cell[3] = { a, b, c };
			furnace = SC_FurnaceRestore( cell, d, e, f, g != 0 );
			furnaces++;
		}
		else if( furnace >= 0 && sscanf( line, " fslot %d %d %d %d", &a, &b, &c, &d ) == 4 )
		{
			scslot_t *s = SC_FurnaceSlot( furnace, a );
			if( s )
				s->id = b, s->count = c, s->dmg = d;
		}
		else if( sscanf( line, "drop %f %f %f %d %d %d", &x, &y, &z, &a, &b, &c ) == 6 )
		{
			CBaseEntity *pDrop = SC_SpawnDrop( Vector( x, y, z ), a, b, c );
			if( pDrop )
				pDrop->pev->velocity = g_vecZero;	// where it lay
			drops++;
		}
		else if( !strcmp( line, "end" ))
			r = NULL, furnace = -1;
	}
	free( text );
	ALERT( at_console, "world: %d players, %d furnaces, %d items lying around\n", players, furnaces, drops );
	return true;
}

// the map starts (SC_WorldActivate): its kept world in place of a new one, if it has one
bool SC_LoadWorld( void )
{
	vox_api_t *v = SC_Vox();
	memset( g_SCSaved, 0, sizeof( g_SCSaved ));
	g_bSCNoSave = false;
	g_bSCWorldMap = v && v->World()->active;
	g_flSCNextSave = gpGlobals->time + Q_max( sc_autosave.value, 10.0f );
	if( !g_bSCWorldMap || sc_worldsave.value == 0.0f )
		return false;
	if( !v->LoadWorld( SC_SavePath( "world.scw" )))
		return false;
	SC_LoadGameText();
	ALERT( at_console, "world: %s continues where it was left\n", STRING( gpGlobals->mapname ));
	return true;
}

// every frame: the autosave
void SC_SaveFrame( void )
{
	if( !g_bSCWorldMap || sc_autosave.value <= 0.0f || gpGlobals->time < g_flSCNextSave )
		return;
	g_flSCNextSave = gpGlobals->time + Q_max( sc_autosave.value, 10.0f );
	SC_SaveWorld( "autosave" );
}

static void SC_SaveCommand( void )
{
	if( SC_SaveWorld( "sc_save" ))
		UTIL_ClientPrintAll( HUD_PRINTTALK, "World saved\n" );
}

// a fresh world: the current one is put aside, and the map starts again
static void SC_NewWorldCommand( void )
{
	vox_api_t *v = SC_Vox();
	if( !v )
		return;
	v->RenameFile( SC_SavePath( "world.scw" ), SC_SavePath( "world.scw.old" ));
	v->RenameFile( SC_SavePath( "game.txt" ), SC_SavePath( "game.txt.old" ));
	g_bSCNoSave = true;
	SERVER_COMMAND( UTIL_VarArgs( "changelevel %s\n", STRING( gpGlobals->mapname )));
}

void SC_RegisterSaveCommands( void )
{
	CVAR_REGISTER( &sc_autosave );
	CVAR_REGISTER( &sc_worldsave );
	g_engfuncs.pfnAddServerCommand( "sc_save", SC_SaveCommand );
	g_engfuncs.pfnAddServerCommand( "sc_newworld", SC_NewWorldCommand );
}
