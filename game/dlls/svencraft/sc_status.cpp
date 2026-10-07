/*
sc_status.cpp - Svencraft: what the crosshair is on, Sven Co-op style

A monster or player under the crosshair (within 2048 units) shows its name and health under the crosshair: green
for friends, red for enemies, yellow for the rest (client: cl_dll/svencraft/hud_target.cpp, message SCTarget).
Checked five times a second per player; sent only when something changed. A map's monster can carry Sven's
"displayname" keyvalue later; until then the names come from the table below (or the classname, tidied).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "sc_game.h"

extern int gmsgSCTarget;

static const struct { const char *cls, *name; } g_SCNames[] =
{
	{ "monster_zombie", "Zombie" }, { "monster_headcrab", "Headcrab" }, { "monster_babycrab", "Baby Headcrab" },
	{ "monster_houndeye", "Houndeye" }, { "monster_bullchicken", "Bullsquid" }, { "monster_alien_slave", "Alien Slave" },
	{ "monster_alien_grunt", "Alien Grunt" }, { "monster_alien_controller", "Alien Controller" },
	{ "monster_human_grunt", "Human Grunt" }, { "monster_hwgrunt", "Heavy Weapons Grunt" },
	{ "monster_robogrunt", "Robot Grunt" }, { "monster_human_assassin", "Female Assassin" },
	{ "monster_barney", "Barney" }, { "monster_scientist", "Scientist" }, { "monster_barnacle", "Barnacle" },
	{ "monster_gargantua", "Gargantua" }, { "monster_babygarg", "Baby Gargantua" }, { "monster_bigmomma", "Big Momma" }, { "monster_ichthyosaur", "Ichthyosaur" },
	{ "monster_snark", "Snark" }, { "monster_tentacle", "Tentacle" }, { "monster_apache", "Apache" },
	{ "monster_osprey", "Osprey" }, { "monster_turret", "Turret" }, { "monster_miniturret", "Mini-Turret" },
	{ "monster_sentry", "Sentry Turret" }, { "monster_gman", "G-Man" }, { "monster_leech", "Leech" },
	{ "monster_male_assassin", "Male Assassin" }, { "monster_human_grunt_ally", "Ally Grunt" },
	{ "monster_human_medic_ally", "Medic Grunt" }, { "monster_human_torch_ally", "Torch Grunt" },
	{ "monster_shocktrooper", "Shock Trooper" }, { "monster_pitdrone", "Pit Drone" }, { "monster_gonome", "Gonome" },
	{ "monster_alien_voltigore", "Voltigore" }, { "monster_alien_babyvoltigore", "Baby Voltigore" },
	{ "monster_otis", "Otis" }, { "monster_zombie_barney", "Zombie Barney" }, { "monster_zombie_soldier", "Zombie Soldier" },
	{ "monster_shockroach", "Shock Roach" }, { "monster_cleansuit_scientist", "Cleansuit Scientist" },
	{ "monster_blkop_apache", "Black Ops Apache" }, { "monster_blkop_osprey", "Black Ops Osprey" },
	{ "monster_drillsergeant", "Drill Sergeant" }, { "monster_recruit", "Recruit" },
	{ "monster_alien_tor", "Tor" }, { "monster_kingpin", "Kingpin" }, { "monster_stukabat", "Stukabat" },
	{ "monster_creeper", "Creeper" }, { "monster_sc_zombie", "Zombie" }, { "monster_sc_skeleton", "Skeleton" },
	{ "monster_sc_spider", "Spider" },
};

void SC_DisplayName( CBaseEntity *e, char *out, int size )
{
	const char *cls = STRING( e->pev->classname );
	for( int i = 0; i < (int)ARRAYSIZE( g_SCNames ); i++ )
	{
		if( !strcmp( cls, g_SCNames[i].cls ))
		{
			strncpy( out, g_SCNames[i].name, size - 1 );
			out[size - 1] = 0;
			return;
		}
	}
	// monster_alien_thing -> Alien Thing
	if( !strncmp( cls, "monster_", 8 ))
		cls += 8;
	int n = 0;
	bool up = true;
	for( ; *cls && n < size - 1; cls++ )
	{
		char c = *cls == '_' ? ' ' : *cls;
		out[n++] = up && c >= 'a' && c <= 'z' ? c - 32 : c;
		up = c == ' ';
	}
	out[n] = 0;
}

static struct
{
	float next;
	int index, health, armor, rel;
} g_SCTarget[SC_MAX_PLAYERS + 1];

void SC_UpdateTarget( CBasePlayer *pPlayer )
{
	int p = pPlayer->entindex();
	if( p < 1 || p > SC_MAX_PLAYERS || !gmsgSCTarget )
		return;
	if( g_SCTarget[p].next - gpGlobals->time > 1.0f )
		g_SCTarget[p].next = 0;		// a new map's clock
	if( gpGlobals->time < g_SCTarget[p].next )
		return;
	g_SCTarget[p].next = gpGlobals->time + 0.2f;

	int index = 0, health = 0, armor = -1, rel = R_NO;
	char name[48] = "";
	if( pPlayer->IsAlive())
	{
		TraceResult tr;
		UTIL_MakeVectors( pPlayer->pev->v_angle + pPlayer->pev->punchangle );
		Vector src = pPlayer->EyePosition();
		UTIL_TraceLine( src, src + gpGlobals->v_forward * 2048.0f, dont_ignore_monsters, pPlayer->edict(), &tr );
		CBaseEntity *e = ( tr.flFraction < 1.0f && !FNullEnt( tr.pHit )) ? CBaseEntity::Instance( tr.pHit ) : NULL;
		if( e && e->IsPlayer() && e->IsAlive())
		{
			index = e->entindex();
			strncpy( name, STRING( e->pev->netname ), sizeof( name ) - 1 );
			health = (int)Q_max( e->pev->health, 0.0f );
			armor = (int)e->pev->armorvalue;
			rel = R_AL;
		}
		else if( e && ( e->pev->flags & FL_MONSTER ) && e->IsAlive() && e->pev->takedamage != DAMAGE_NO
			&& !FClassnameIs( e->pev, "monster_furniture" ) && !FClassnameIs( e->pev, "monster_generic" ))
		{
			index = e->entindex();
			SC_DisplayName( e, name, sizeof( name ));
			health = (int)Q_max( ceilf( e->pev->health ), 1.0f );
			// friend or foe as the monster sees the player: an ally likes you, an enemy would attack you
			CBaseMonster *m = e->MyMonsterPointer();
			rel = m ? m->IRelationship( pPlayer ) : R_NO;
		}
	}
	health = Q_min( health, 32767 );
	if( index == g_SCTarget[p].index && health == g_SCTarget[p].health && armor == g_SCTarget[p].armor && rel == g_SCTarget[p].rel )
		return;
	g_SCTarget[p].index = index;
	g_SCTarget[p].health = health;
	g_SCTarget[p].armor = armor;
	g_SCTarget[p].rel = rel;

	MESSAGE_BEGIN( MSG_ONE, gmsgSCTarget, NULL, pPlayer->edict());
		WRITE_SHORT( index );		// 0: nothing under the crosshair
		WRITE_CHAR( rel );		// R_AL (-2) .. R_NM (3)
		WRITE_SHORT( health );
		WRITE_SHORT( armor );		// -1: not a player
		WRITE_STRING( name );
	MESSAGE_END();
}
