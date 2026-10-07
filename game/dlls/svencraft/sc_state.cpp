/*
sc_state.cpp - Svencraft: the game's state for whoever watches over it

`sc_state` (a server command, so it works from the server console or rcon, with no player needed) prints one
line of JSON: the map and time, every player (name, position, view, health, armour, what they hold) and every
monster (class, position, health, what it's after). Tests read it from the log instead of guessing from
screenshots; it is also the first piece of what the AI dungeon master will watch (DESIGN.md).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "monsters.h"
#include "sc_world.h"
#include "sc_game.h"

// a string for JSON: quotes and backslashes escaped, control characters dropped
static void SC_JsonString( char *out, size_t size, const char *in )
{
	size_t n = 0;
	if( n + 1 < size ) out[n++] = '"';
	for( ; in && *in && n + 3 < size; in++ )
	{
		if( *in == '"' || *in == '\\' )
			out[n++] = '\\';
		if((unsigned char)*in >= 32 )
			out[n++] = *in;
	}
	if( n + 1 < size ) out[n++] = '"';
	out[n] = 0;
}

static void SC_Append( char *buf, size_t size, size_t *len, const char *fmt, ... )
{
	va_list args;
	if( *len >= size )
		return;
	va_start( args, fmt );
	int n = vsnprintf( buf + *len, size - *len, fmt, args );
	va_end( args );
	if( n > 0 )
		*len = Q_min( *len + n, size - 1 );
}

static void SC_StateCommand( void )
{
	static char buf[32768];
	char name[96];
	size_t len = 0;

	SC_JsonString( name, sizeof( name ), STRING( gpGlobals->mapname ));
	SC_Append( buf, sizeof( buf ), &len, "{\"map\":%s,\"time\":%.1f,\"players\":[", name, gpGlobals->time );
	bool first = true;
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *p = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !p || !p->IsPlayer() || FStringNull( p->pev->netname ))
			continue;
		SCInventory &inv = SC_Inv( p );
		const scslot_t &s = inv.slot[inv.hotbar];
		const scitem_t *it = s.count > 0 ? SC_Item( s.id ) : NULL;
		char held[64];
		SC_JsonString( name, sizeof( name ), STRING( p->pev->netname ));
		SC_JsonString( held, sizeof( held ), it ? it->name : "" );
		// the weapon in hand: its classname, clip and both ammo pools (-1: none)
		CBasePlayerWeapon *w = p->m_pActiveItem ? (CBasePlayerWeapon *)p->m_pActiveItem->GetWeaponPtr() : NULL;
		char wname[64];
		SC_JsonString( wname, sizeof( wname ), w ? STRING( w->pev->classname ) : "" );
		int clip = w ? w->m_iClip : -1;
		int ammo1 = ( w && w->m_iPrimaryAmmoType >= 0 ) ? p->m_rgAmmo[w->m_iPrimaryAmmoType] : -1;
		int ammo2 = ( w && w->m_iSecondaryAmmoType > 0 ) ? p->m_rgAmmo[w->m_iSecondaryAmmoType] : -1;
		SC_Append( buf, sizeof( buf ), &len, "%s{\"id\":%d,\"name\":%s,\"pos\":[%.0f,%.0f,%.0f],\"view\":[%.0f,%.0f],\"health\":%.0f,"
			"\"armor\":%.0f,\"alive\":%d,\"held\":%s,\"count\":%d,\"weapon\":%s,\"clip\":%d,\"ammo\":%d,\"ammo2\":%d,"
			"\"fov\":%.0f,\"speed\":%.0f,\"deadflag\":%d,\"buttons\":%d}", first ? "" : ",", i, name, p->pev->origin.x, p->pev->origin.y,
			p->pev->origin.z, p->pev->v_angle.x, p->pev->v_angle.y, p->pev->health, p->pev->armorvalue, p->IsAlive() ? 1 : 0, held, s.count,
			wname, clip, ammo1, ammo2, p->pev->fov, p->pev->velocity.Length(), p->pev->deadflag, p->pev->button );
		// the inventory: [slot, item id, count] for every filled slot (0-8 the hotbar)
		len--;	// reopen the player object
		SC_Append( buf, sizeof( buf ), &len, ",\"inv\":[" );
		bool f2 = true;
		for( int k = 0; k < SC_INV_SLOTS; k++ )
		{
			if( inv.slot[k].count <= 0 )
				continue;
			SC_Append( buf, sizeof( buf ), &len, "%s[%d,%d,%d]", f2 ? "" : ",", k, inv.slot[k].id, inv.slot[k].count );
			f2 = false;
		}
		SC_Append( buf, sizeof( buf ), &len, "]}" );
		first = false;
	}
	SC_Append( buf, sizeof( buf ), &len, "],\"monsters\":[" );
	first = true;
	CBaseEntity *pEnt = NULL;
	while(( pEnt = UTIL_FindEntityInSphere( pEnt, g_vecZero, 65536.0f )) != NULL )
	{
		if( !( pEnt->pev->flags & FL_MONSTER ) || pEnt->IsPlayer())
			continue;
		CBaseMonster *pMon = pEnt->MyMonsterPointer();
		CBaseEntity *pEnemy = pMon ? (CBaseEntity *)pMon->m_hEnemy : NULL;
		SC_JsonString( name, sizeof( name ), STRING( pEnt->pev->classname ));
		SC_Append( buf, sizeof( buf ), &len, "%s{\"idx\":%d,\"class\":%s,\"pos\":[%.0f,%.0f,%.0f],\"health\":%.0f,\"alive\":%d,\"enemy\":%d}",
			first ? "" : ",", pEnt->entindex(), name, pEnt->pev->origin.x, pEnt->pev->origin.y, pEnt->pev->origin.z, pEnt->pev->health,
			pEnt->IsAlive() ? 1 : 0, pEnemy ? pEnemy->entindex() : 0 );
		first = false;
	}
	SC_Append( buf, sizeof( buf ), &len, "]}\n" );
	g_engfuncs.pfnServerPrint( buf );
}

void SC_RegisterStateCommands( void )
{
	g_engfuncs.pfnAddServerCommand( "sc_state", SC_StateCommand );
}
