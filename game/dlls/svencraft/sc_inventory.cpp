/*
sc_inventory.cpp - Svencraft: block rules, player inventory, the HUD message and the spawn kit
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "sc_game.h"

//                                     title             hard  tool          lvl drop               family
const scblockdef_t g_SCBlocks[BLOCK_COUNT] =
{
	{ "Air",            0.0f,  TOOL_HAND,    0, BLOCK_AIR,          FAM_EARTH },
	{ "Grass",          0.6f,  TOOL_SHOVEL,  0, BLOCK_DIRT,         FAM_EARTH },
	{ "Dirt",           0.5f,  TOOL_SHOVEL,  0, BLOCK_DIRT,         FAM_EARTH },
	{ "Stone",          1.5f,  TOOL_PICKAXE, 1, BLOCK_COBBLE,       FAM_STONE },
	{ "Cobblestone",    2.0f,  TOOL_PICKAXE, 1, BLOCK_COBBLE,       FAM_STONE },
	{ "Gravel",         0.6f,  TOOL_SHOVEL,  0, BLOCK_GRAVEL,       FAM_EARTH },
	{ "Sand",           0.5f,  TOOL_SHOVEL,  0, BLOCK_SAND,         FAM_EARTH },
	{ "Bedrock",        -1.0f, TOOL_PICKAXE, 9, BLOCK_AIR,          FAM_STONE },
	{ "Coal Ore",       3.0f,  TOOL_PICKAXE, 1, SCITEM_COAL,          FAM_STONE },
	{ "Iron Ore",       3.0f,  TOOL_PICKAXE, 2, BLOCK_IRON_ORE,     FAM_STONE },
	{ "Gold Ore",       3.0f,  TOOL_PICKAXE, 3, BLOCK_GOLD_ORE,     FAM_STONE },
	{ "Diamond Ore",    3.0f,  TOOL_PICKAXE, 3, SCITEM_DIAMOND,       FAM_STONE },
	{ "Xen Crystal Ore",3.0f,  TOOL_PICKAXE, 3, SCITEM_XEN_CRYSTAL,   FAM_GLASS },
	{ "Log",            2.0f,  TOOL_AXE,     0, BLOCK_LOG,          FAM_WOOD },
	{ "Leaves",         0.2f,  TOOL_HAND,    0, BLOCK_AIR,          FAM_PLANT },	// shears keep them (sc_mining.cpp)
	{ "Planks",         2.0f,  TOOL_AXE,     0, BLOCK_PLANKS,       FAM_WOOD },
	{ "Glass",          0.3f,  TOOL_HAND,    0, BLOCK_AIR,          FAM_GLASS },	// shatters, like Minecraft's
	{ "Mossy Cobblestone", 2.0f, TOOL_PICKAXE, 1, BLOCK_MOSSY_COBBLE, FAM_STONE },
	{ "Bricks",         1.5f,  TOOL_PICKAXE, 1, BLOCK_BRICK,        FAM_STONE },
	{ "Tan Bricks",     1.5f,  TOOL_PICKAXE, 1, BLOCK_TAN_BRICK,    FAM_STONE },
	{ "Concrete",       1.5f,  TOOL_PICKAXE, 1, BLOCK_CONCRETE,     FAM_STONE },
	{ "Asphalt",        1.0f,  TOOL_PICKAXE, 1, BLOCK_ASPHALT,      FAM_STONE },
	{ "Scrap Metal",    4.0f,  TOOL_PICKAXE, 2, BLOCK_METAL,        FAM_METAL },
	{ "Rubber",         0.8f,  TOOL_AXE,     0, BLOCK_RUBBER,       FAM_PLANT },
	{ "Crafting Table", 2.5f,  TOOL_AXE,     0, BLOCK_CRAFTING_TABLE, FAM_WOOD },
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },
	{ "Torch",          0.0f,  TOOL_HAND,    0, BLOCK_TORCH,        FAM_WOOD },
	{ "Torch",          0.0f,  TOOL_HAND,    0, BLOCK_TORCH,        FAM_WOOD },
	{ "Torch",          0.0f,  TOOL_HAND,    0, BLOCK_TORCH,        FAM_WOOD },
	{ "Torch",          0.0f,  TOOL_HAND,    0, BLOCK_TORCH,        FAM_WOOD },
	{ "Torch",          0.0f,  TOOL_HAND,    0, BLOCK_TORCH,        FAM_WOOD },
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },	// facings
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },	// burning
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },
	{ "Furnace",        3.5f,  TOOL_PICKAXE, 1, BLOCK_FURNACE,      FAM_STONE },
};

const char *g_SCToolNames[TOOL_COUNT] = { "Hand", "Pickaxe", "Shovel", "Axe" };
const char *g_SCTierNames[SC_MAX_TIER + 1] = { "", "Wooden", "Stone", "Iron", "Diamond" };

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 32
#endif

int gmsgSCInv = 0;
int gmsgSCBlast = 0;
int gmsgSCTarget = 0;
int gmsgSCFog = 0;
int gmsgSCSplash = 0;
static SCInventory g_SCInvs[MAX_CLIENTS + 1];

void SC_LinkUserMessages( void )
{
	gmsgSCInv = REG_USER_MSG( "SCInv", -1 );	// see SC_InvSend
	gmsgSCBlast = REG_USER_MSG( "SCBlast", -1 );	// see SC_BlastFeel (sc_blast.cpp)
	gmsgSCTarget = REG_USER_MSG( "SCTarget", -1 );	// see SC_UpdateTarget (sc_status.cpp)
	gmsgSCFog = REG_USER_MSG( "SCFog", -1 );	// see SC_FogSend (sc_fog.cpp)
	gmsgSCSplash = REG_USER_MSG( "SCSplash", 7 );	// see SC_SendSplash (sc_blast.cpp)
}

SCInventory &SC_Inv( CBasePlayer *pPlayer )
{
	int i = pPlayer ? pPlayer->entindex() : 0;
	if( i < 0 || i > MAX_CLIENTS )
		i = 0;
	return g_SCInvs[i];
}

void SC_InvReset( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	memset( inv.slot, 0, sizeof( inv.slot ));
	memset( inv.grid, 0, sizeof( inv.grid ));
	memset( &inv.cursor, 0, sizeof( inv.cursor ));
	inv.hotbar = inv.lastHotbar = 0;
	inv.screen = SCS_NONE;
	inv.furnace = -1;
	inv.heldSlot = inv.heldId = -1;
	inv.startKit = false;
	inv.mineProgress = 0;
	inv.lastHit = 0;
	inv.nextCycle = 0;
	inv.crack = NULL;
	memset( inv.partial, 0, sizeof( inv.partial ));
}

int SC_StackSize( int id )
{
	const scitem_t *it = SC_Item( id );
	return it ? it->stack : 0;
}

static void SC_WriteSlot( const scslot_t &s )
{
	WRITE_BYTE( s.count > 0 ? s.id : 0 );
	WRITE_BYTE( s.count > 0 ? Q_min( s.count, 255 ) : 0 );
	const scitem_t *it = s.count > 0 ? SC_Item( s.id ) : NULL;
	if( it && it->kind == SCI_TOOL )	// tools: how much is left of them, 0..255
		WRITE_BYTE( Q_max( 0, 255 - s.dmg * 255 / Q_max( SC_ToolUses( it->tier ), 1 )));
}

// everything the HUD and the screens show: hotbar choice, open screen, the slots (a tool's slot has a third byte, its
// wear), the grid, the mouse's stack,
// the grid's result, and the open furnace (its three slots, cooking progress and fuel left, 0..255)
void SC_InvSend( CBasePlayer *pPlayer )
{
	if( !gmsgSCInv || !pPlayer )
		return;
	SCInventory &inv = SC_Inv( pPlayer );
	scslot_t result = { 0, 0 };
	result.id = SC_CraftResult( pPlayer, &result.count );
	MESSAGE_BEGIN( MSG_ONE, gmsgSCInv, NULL, pPlayer->pev );
		WRITE_BYTE( inv.hotbar );
		WRITE_BYTE( inv.screen );
		for( int i = 0; i < SC_INV_SLOTS; i++ )
			SC_WriteSlot( inv.slot[i] );
		for( int i = 0; i < 9; i++ )
			SC_WriteSlot( inv.grid[i] );
		SC_WriteSlot( inv.cursor );
		SC_WriteSlot( result );
		int cook = 0, burn = 0;
		const scslot_t *f = inv.screen == SCS_FURNACE ? SC_FurnaceSlots( inv.furnace, &cook, &burn ) : NULL;
		for( int i = 0; i < 3; i++ )
		{
			scslot_t none = { 0, 0 };
			SC_WriteSlot( f ? f[i] : none );
		}
		WRITE_BYTE( cook );
		WRITE_BYTE( burn );
	MESSAGE_END();
}

// Minecraft's pickup order: onto matching stacks (hotbar first), then into the first empty slot
int SC_InvAdd( CBasePlayer *pPlayer, int id, int amount, int dmg )
{
	int max = SC_StackSize( id );
	if( max <= 0 || amount <= 0 )
		return amount;
	SCInventory &inv = SC_Inv( pPlayer );
	for( int pass = 0; pass < 2 && amount > 0; pass++ )
		for( int i = 0; i < SC_INV_SLOTS && amount > 0; i++ )
		{
			scslot_t &s = inv.slot[i];
			if( pass == 0 && ( s.count <= 0 || s.id != id ))
				continue;
			if( pass == 1 && s.count > 0 )
				continue;
			if( pass == 1 )
			{
				s.id = id;
				s.dmg = dmg;
			}
			int add = Q_min( amount, max - s.count );
			s.count += add;
			amount -= add;
		}
	SC_InvSend( pPlayer );
	return amount;
}

int SC_InvHeldBlock( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	const scslot_t &s = inv.slot[inv.hotbar];
	const scitem_t *it = SC_Item( s.id );
	return ( s.count > 0 && it && it->kind == SCI_BLOCK ) ? s.id : 0;
}

bool SC_InvTakeHeld( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	scslot_t &s = inv.slot[inv.hotbar];
	if( s.count <= 0 )
		return false;
	if( --s.count <= 0 )
		s.id = 0;
	SC_InvSend( pPlayer );
	return true;
}

void SC_InvCycle( CBasePlayer *pPlayer, int dir )
{
	SCInventory &inv = SC_Inv( pPlayer );
	inv.hotbar = ( inv.hotbar + dir + SC_HOTBAR_SLOTS ) % SC_HOTBAR_SLOTS;
	SC_InvSend( pPlayer );
}


const char *g_SCFamHit[FAM_COUNT] = { "player/pl_dirt1.wav", "debris/concrete1.wav", "debris/wood1.wav", "player/pl_dirt3.wav", "debris/glass1.wav", "debris/metal1.wav" };
const char *g_SCFamBreak[FAM_COUNT] = { "player/pl_dirt2.wav", "debris/bustconcrete1.wav", "debris/bustcrate1.wav", "player/pl_dirt4.wav", "debris/bustglass1.wav", "debris/bustmetal1.wav" };
const char *g_SCFamGibs[FAM_COUNT] = { "models/cindergibs.mdl", "models/cindergibs.mdl", "models/woodgibs.mdl", "models/woodgibs.mdl", "models/glassgibs.mdl", "models/metalplategibs.mdl" };
int g_SCFamGibIndex[FAM_COUNT];

void SC_Precache( void )
{
	for( int f = 0; f < FAM_COUNT; f++ )
	{
		PRECACHE_SOUND( (char *)g_SCFamHit[f] );
		PRECACHE_SOUND( (char *)g_SCFamBreak[f] );
		g_SCFamGibIndex[f] = PRECACHE_MODEL( (char *)g_SCFamGibs[f] );
	}
	PRECACHE_SOUND( "items/9mmclip1.wav" );
	PRECACHE_SOUND( "common/wpn_denyselect.wav" );
	for( int i = 1; i <= 4; i++ )
		PRECACHE_SOUND( (char *)STRING( ALLOC_STRING( UTIL_VarArgs( "common/npc_step%d.wav", i ))));	// Sven models' footstep event (monsters.cpp)
	PRECACHE_SOUND( "debris/wood2.wav" );	// tools wearing through
	PRECACHE_SOUND( "debris/concrete2.wav" );
	PRECACHE_SOUND( "debris/metal2.wav" );
	PRECACHE_SOUND( "debris/glass2.wav" );
	PRECACHE_SOUND( "barnacle/bcl_chew1.wav" );
	PRECACHE_MODEL( "sprites/wep_smoke_02.spr" );	// gun smoke (cl_dll/svencraft/sc_effects.cpp; the one-frame sprites showed nothing)
	PRECACHE_MODEL( "sprites/MP5Flash.spr" );		// Sven's scripted muzzle flashes (events/muzzle_*.txt)
	PRECACHE_MODEL( "sprites/SGflash.spr" );
	PRECACHE_MODEL( "sprites/Puff1.spr" );		// bullet impact dust (cl_dll/ev_hldm.cpp)
	PRECACHE_MODEL( "models/svencraft/blockitem.mdl" );
	PRECACHE_MODEL( "models/svencraft/itemflat.mdl" );
	PRECACHE_MODEL( "sprites/svencraft/cracks.spr" );
	UTIL_PrecacheOther( "monster_creeper" );

	// the monsters that can be brought in at any time (sc_summon now, the dungeon master later) load with the map,
	// so they never come in half-loaded (late precache: no sounds for clients that joined before)
	static const char *bestiary[] =
	{
		"monster_zombie", "monster_headcrab", "monster_houndeye", "monster_bullchicken", "monster_alien_slave",
		"monster_alien_grunt", "monster_human_grunt", "monster_human_assassin", "monster_barney", "monster_scientist",
		"monster_babycrab", "monster_barnacle",
		// Opposing Force's (dlls/gearbox), as in Sven Co-op
		"monster_male_assassin", "monster_human_grunt_ally", "monster_human_medic_ally", "monster_human_torch_ally",
		"monster_shocktrooper", "monster_pitdrone", "monster_gonome", "monster_alien_voltigore",
		"monster_alien_babyvoltigore", "monster_otis", "monster_zombie_barney", "monster_zombie_soldier",
		"monster_shockroach", "monster_cleansuit_scientist",
		// Sven Co-op's own (sc_hwgrunt.cpp, sc_robogrunt.cpp)
		"monster_hwgrunt", "monster_robogrunt",
	};
	for( int i = 0; i < (int)ARRAYSIZE( bestiary ); i++ )
		UTIL_PrecacheOther( bestiary[i] );
}
