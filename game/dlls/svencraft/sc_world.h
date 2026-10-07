/*
sc_world.h - Svencraft: the Minecraft-style block world (generation and block ids)
*/
#ifndef SC_WORLD_H
#define SC_WORLD_H

#include "voxel_api.h"
#include "dynworld_api.h"

// block ids: must match scripts/blocks.txt
enum
{
	BLOCK_AIR = 0,
	BLOCK_GRASS,
	BLOCK_DIRT,
	BLOCK_STONE,
	BLOCK_COBBLE,
	BLOCK_GRAVEL,
	BLOCK_SAND,
	BLOCK_BEDROCK,
	BLOCK_COAL_ORE,
	BLOCK_IRON_ORE,
	BLOCK_GOLD_ORE,
	BLOCK_DIAMOND_ORE,
	BLOCK_CRYSTAL_ORE,
	BLOCK_LOG,
	BLOCK_LEAVES,
	BLOCK_PLANKS,
	BLOCK_GLASS,
	BLOCK_MOSSY_COBBLE,
	// realistic (Sven world) blocks: Half-Life textures at world scale
	BLOCK_BRICK,
	BLOCK_TAN_BRICK,
	BLOCK_CONCRETE,
	BLOCK_ASPHALT,
	BLOCK_METAL,
	BLOCK_RUBBER,
	// workstations
	BLOCK_CRAFTING_TABLE,
	BLOCK_FURNACE,
	// light: a torch on the floor, and on a wall at +x / -x / +y / -y of its cell (leaning away from it)
	BLOCK_TORCH,
	BLOCK_WALL_TORCH_PX,
	BLOCK_WALL_TORCH_NX,
	BLOCK_WALL_TORCH_PY,
	BLOCK_WALL_TORCH_NY,
	// the furnace's other facings (BLOCK_FURNACE's front looks toward -y), then all four burning
	BLOCK_FURNACE_PX,
	BLOCK_FURNACE_NX,
	BLOCK_FURNACE_PY,
	BLOCK_FURNACE_LIT,
	BLOCK_FURNACE_LIT_PX,
	BLOCK_FURNACE_LIT_NX,
	BLOCK_FURNACE_LIT_PY,
	BLOCK_COUNT
};

// furnace facings: 0 = -y, 1 = +x, 2 = -x, 3 = +y
inline bool SC_IsFurnace( int id )
{
	return id == BLOCK_FURNACE || ( id >= BLOCK_FURNACE_PX && id <= BLOCK_FURNACE_LIT_PY );
}
inline int SC_FurnaceFacing( int id )
{
	if( id == BLOCK_FURNACE || id == BLOCK_FURNACE_LIT )
		return 0;
	return id >= BLOCK_FURNACE_LIT_PX ? id - BLOCK_FURNACE_LIT_PX + 1 : id - BLOCK_FURNACE_PX + 1;
}
inline int SC_FurnaceBlock( int facing, bool lit )
{
	if( facing <= 0 || facing > 3 )
		return lit ? BLOCK_FURNACE_LIT : BLOCK_FURNACE;
	return ( lit ? BLOCK_FURNACE_LIT_PX : BLOCK_FURNACE_PX ) + facing - 1;
}

dyn_api_t *SC_Dyn( void );			// the engine's diggable map geometry API
vox_api_t *SC_Vox( void );			// the engine's block world API (NULL when the engine has none)
void SC_RegisterCvars( void );			// GameDLLInit
void SC_WorldSpawn( void );			// worldspawn: makes the (empty) block world for svencraft maps
void SC_WorldActivate( void );		// ServerActivate: generates it once all map entities exist
extern int g_iSCBlockTop;		// worldspawn "sc_blocktop": top of the block world in blocks
int SC_BlockTop( void );		// the current map's (after the world is generated)
float SC_Noise3( float x, float y, float z, int seed );	// smooth value noise in [-1, 1]
bool SC_ClientCommand( edict_t *pEntity, const char *pcmd );	// developer commands; true when handled
int SC_Relationship( int me, int them, int halfLife );	// monster relationships with the block world's creatures

#endif // SC_WORLD_H
