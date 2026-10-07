/*
sc_game.h - Svencraft gameplay: block rules, tools, player inventory
*/
#ifndef SC_GAME_H
#define SC_GAME_H

#include "sc_world.h"
#include "sc_items.h"
#include "sc_recipes.h"

class CBasePlayer;
class CBasePlayerItem;

// tools; tiers: 0 = none (the hand), 1 = wood, 2 = stone, 3 = iron, 4 = diamond
enum
{
	TOOL_HAND = 0,
	TOOL_PICKAXE,
	TOOL_SHOVEL,
	TOOL_AXE,
	TOOL_COUNT
};
#define SC_MAX_TIER	4

// weapon ids of the tools and the bare hand (HL's go up to 15, Opposing Force's 16-25, the suit is 31)
#define WEAPON_SC_PICKAXE	26
#define WEAPON_SC_SHOVEL	27
#define WEAPON_SC_AXE		28
#define WEAPON_SC_HAND		29
#define WEAPON_MEDKIT		5	// Sven's medkit (sc_medkit.cpp), in Half-Life's never-made chaingun's place

// sound/debris families
enum
{
	FAM_EARTH = 0,
	FAM_STONE,
	FAM_WOOD,
	FAM_PLANT,
	FAM_GLASS,
	FAM_METAL,
	FAM_COUNT
};

typedef struct
{
	const char	*title;		// shown on the HUD
	float		hardness;	// < 0: unbreakable
	int		tool;		// best tool
	int		level;		// minimum pickaxe tier to get a drop (0: anything)
	int		drop;		// block id dropped
	int		family;
} scblockdef_t;

extern const scblockdef_t g_SCBlocks[BLOCK_COUNT];
extern const char *g_SCToolNames[TOOL_COUNT];
extern const char *g_SCTierNames[SC_MAX_TIER + 1];

#define SC_REACH		160.0f	// how far players can mine and place (4 blocks)
#define SC_SWING_TIME		0.5f	// one tool swing

typedef struct
{
	int	id;
	int	count;
	int	dmg;		// tools: uses spent (see SC_ToolUses)
} scslot_t;

struct SCInventory
{
	scslot_t slot[SC_INV_SLOTS];	// 0..8 hotbar, 9..35 the rest
	scslot_t grid[9];		// crafting grid (3x3; the inventory's own 2x2 uses 0, 1, 3, 4)
	scslot_t cursor;		// carried by the mouse while a screen is open
	int	hotbar;			// selected hotbar slot
	int	lastHotbar;		// the one before (Q)
	int	screen;			// SCS_* (sc_recipes.h)
	int	furnace;		// the furnace being looked at (SCS_FURNACE)
	int	heldSlot, heldId;	// what the active weapon was deployed for (sc_hotbar.cpp)
	bool	startKit;		// the wooden tools were handed out (once per connection, not again after a death)
	bool	saveChecked;		// a kept world's record for this player was looked for (sc_save.cpp)
	// block being mined
	int	mineCell[3];
	float	mineProgress;
	float	lastHit;
	EHANDLE	crack;
	float	nextCycle;
	float	partial[BLOCK_COUNT];	// material dug out of the realistic world, in blocks (walls give a fraction per cell)
};

SCInventory &SC_Inv( CBasePlayer *pPlayer );
void SC_InvReset( CBasePlayer *pPlayer );
int SC_InvAdd( CBasePlayer *pPlayer, int id, int amount, int dmg = 0 );	// returns what didn't fit
int SC_InvHeldBlock( CBasePlayer *pPlayer );			// the selected hotbar slot's block, or 0
bool SC_HoldingItem( CBasePlayer *pPlayer, int id );		// the selected hotbar slot holds this item
bool SC_EatHeld( CBasePlayer *pPlayer );			// food in hand: eat one (sc_hotbar.cpp)
void SC_RegisterStateCommands( void );				// sc_state: the game as JSON (sc_state.cpp)
// falls (sc_player.cpp): sc_falldamage 1 = Minecraft's x5, 2 = realistic, 0 = Half-Life's
void SC_RegisterPlayerCvars( void );
bool SC_OwnFallDamage( void );
float SC_FallHurtSpeed( void );
float SC_FallDamage( CBasePlayer *pPlayer );
void SC_SyncGunFeel( CBasePlayer *pPlayer );
void SC_UpdateTarget( CBasePlayer *pPlayer );	// Sven's name and health under the crosshair (sc_status.cpp)
// fog (sc_fog.cpp): env_fog, sc_fog; sent to a joining player (NULL: everyone)
void SC_FogSend( CBasePlayer *pPlayer );
void SC_FogReset( void );
void SC_RegisterFogCommands( void );
#define SC_FX_EMITTER	77	// renderfx of an sc_emitter (sc_emitter.cpp): the client draws its puffs, not it
bool SC_PlaceCreature( CBaseEntity *pNew, const Vector &want, edict_t *pIgnore );	// free floor for a new monster (sc_world.cpp)
bool SC_BlockFriendlyFire( CBaseEntity *pVictim, entvars_t *pevAttacker );	// mp_npckill (sc_world.cpp)
void SC_SetKeyValue( CBaseEntity *pEnt, const char *key, const char *value );	// as a map would set it (sc_world.cpp)
// Sven Co-op's wall charger keys (func_healthcharger, func_recharge; sc_world.cpp)
struct scCharger_t
{
	int	juice;			// CustomJuice: what a full charger holds (0: the skill's)
	int	recharge;		// CustomRechargeTime, or Half-Life's dmdelay: seconds till an empty one refills (0: the default)
	string_t onEmpty, onRecharged;	// TriggerOnEmpty, TriggerOnRecharged
	string_t denied, start, loop;	// CustomDeniedSound, CustomStartSound, CustomLoopSound
};
bool SC_ChargerKeyValue( scCharger_t *c, KeyValueData *pkvd );
void SC_ChargerPrecache( scCharger_t *c );
const char *SC_ChargerSound( string_t custom, const char *standard );
int SC_ChargerRefillTime( scCharger_t *c, float rules, int standard );
#ifndef SC_MAX_PLAYERS
#define SC_MAX_PLAYERS		32	// per-player tables
#endif
// monster lag compensation (sc_lagcomp.cpp): snapshots every frame, rewinds around a player's shots
void SC_RegisterLagCompCvars( void );
void SC_LagCompFrame( void );
void SC_LagCompCmd( CBasePlayer *pPlayer, int lerpMsec );
void SC_LagCompBegin( CBasePlayer *pPlayer );
void SC_LagCompEnd( void );			// sc_gunfeel to the client weapons (sc_player.cpp)
bool SC_InvTakeHeld( CBasePlayer *pPlayer );			// uses up one of it
void SC_InvCycle( CBasePlayer *pPlayer, int dir );
int SC_StackSize( int id );
void SC_InvSend( CBasePlayer *pPlayer );
void SC_LinkUserMessages( void );
void SC_Precache( void );

// the hotbar decides what is held (sc_hotbar.cpp)
void SC_PlayerSpawn( CBasePlayer *pPlayer );
// Sven's revive (sc_medkit.cpp): a dead player up again where they lie; one being revived doesn't respawn meanwhile
void SC_RevivePlayer( CBasePlayer *pPlayer, int health );
bool SC_BeingRevived( CBasePlayer *pPlayer );
bool SC_StayWithBody( void );	// dead players wait at their bodies, no death camera (sc_respawn, sc_player.cpp)
void SC_AddBot( edict_t *pBy, const char *name );	// sc_bot: a stand-in player (sc_bots.cpp)
void SC_BodyFell( CBasePlayer *pPlayer );	// a dead player's body keeps its facing (sc_player.cpp)
bool SC_PlayerShielded( CBasePlayer *pVictim, CBaseEntity *pAttacker );	// co-op: no harm from other players
void SC_AntiBlock( CBasePlayer *pPlayer );	// hold use on a teammate in the way to change places (sc_player.cpp)
void SC_DisplayName( CBaseEntity *e, char *out, int size );	// a creature's name as the HUD shows it (sc_status.cpp)
void SC_HoldBody( CBasePlayer *pPlayer );
void SC_BotsFrame( void );
void SC_PlayerFrame( CBasePlayer *pPlayer );
int SC_HeldTier( CBasePlayer *pPlayer, int tool );		// the held tool's tier if it is that kind of tool, else 0
int SC_ToolUses( int tier );
void SC_WearHeld( CBasePlayer *pPlayer, int uses );
int SC_InvCount( CBasePlayer *pPlayer, int id );
bool SC_WeaponPickup( CBasePlayer *pPlayer, CBasePlayerItem *pItem );	// false: no inventory room for it
void SC_ArmWeapon( CBasePlayer *pPlayer, const char *classname );
void SC_DropHeld( CBasePlayer *pPlayer, bool all );

// mining and building (sc_mining.cpp)
float SC_SwingPower( CBasePlayer *pPlayer, int tool, int id );
bool SC_CanHarvest( CBasePlayer *pPlayer, int tool, int id );
void SC_HitBlock( CBasePlayer *pPlayer, int tool, const int *cell, const float *normal );
bool SC_PlaceBlock( CBasePlayer *pPlayer, const Vector &vecSrc, const Vector &vecDir );
void SC_HideCrack( CBasePlayer *pPlayer );
void SC_HitGround( CBasePlayer *pPlayer, int tool, int material, const Vector &vecHit, const Vector &vecNormal );
CBaseEntity *SC_SpawnDrop( const Vector &vecPos, int id, int amount, int dmg = 0 );
void SC_BlockRemoved( const int *cell );		// a block is gone: torches it held up fall off, sand above falls
void SC_CheckFall( const int *cell );			// sand or gravel with nothing under it starts falling
bool SC_IsTorch( int id );
int SC_TorchTarget( const Vector &src, const Vector &dir, float maxdist, int *cell );	// torches aren't solid
void SC_BreakTorch( CBasePlayer *pPlayer, const int *cell );
void SC_Debris( const Vector &pos, int family, int count, float speed = 0 );
void SC_ShowCrackAt( CBasePlayer *pPlayer, const Vector &hit, const Vector &normal, float progress );

// props and cars (sc_props.cpp)
CBaseEntity *SC_PropTarget( const Vector &src, const Vector &dir, float maxdist, float &dist, Vector &normal );
void SC_HitProp( CBasePlayer *pPlayer, int tool, CBaseEntity *pEnt, const Vector &vecHit, const Vector &vecNormal );
void SC_BreakProp( CBaseEntity *pEnt, CBasePlayer *pPlayer, int tool );	// pPlayer NULL: a blast, everything drops
void SC_BlastProps( const Vector &center, float radius );

// crafting and the inventory screens (sc_crafting.cpp)
void SC_OpenScreen( CBasePlayer *pPlayer, int screen, int furnace );
void SC_CloseScreen( CBasePlayer *pPlayer );
void SC_ScreenClick( CBasePlayer *pPlayer, int slot, int button, bool shift );
void SC_ScreenHotkey( CBasePlayer *pPlayer, int slot, int n );	// number key n+1 over a slot
void SC_ScreenThrow( CBasePlayer *pPlayer, int slot, bool all );	// the drop key over a slot
void SC_ScreenGather( CBasePlayer *pPlayer, int slot );		// double click: the same item onto the mouse
void SC_ScreenSpread( CBasePlayer *pPlayer, int button, const int *slots, int n );	// a drag across slots
void SC_RecipeFill( CBasePlayer *pPlayer, int recipe, bool all );
int SC_CraftResult( CBasePlayer *pPlayer, int *count );		// what the grid makes now (0: nothing)
bool SC_PlayerUse( CBasePlayer *pPlayer );			// E on a crafting table or furnace
void SC_ThrowItem( CBasePlayer *pPlayer, int id, int count, int dmg = 0 );
void SC_PlayerDied( CBasePlayer *pPlayer );			// drops the whole inventory where the player fell
// furnaces (sc_crafting.cpp): state per placed furnace block
int SC_FurnaceAt( const int *cell, bool create );
void SC_FurnaceRemoved( const int *cell, const Vector &where );
void SC_FurnaceFrame( void );
void SC_CraftingReset( void );		// a new map: no furnaces yet
const scslot_t *SC_FurnaceSlots( int furnace, int *cook, int *burn );	// in, fuel, out
scslot_t *SC_FurnaceSlot( int furnace, int which );
int SC_FurnaceSave( int i, int *cell, scslot_t *slots, int *burn, int *burnTotal, int *cook, bool *lit );
int SC_FurnaceRestore( const int *cell, int burn, int burnTotal, int cook, bool lit );
// keeping a world between sessions (sc_save.cpp)
bool SC_LoadWorld( void );			// the map starts: its kept world instead of a new one, if any
bool SC_SaveWorld( const char *why );
void SC_SaveFrame( void );			// the autosave
bool SC_SaveRestorePlayer( CBasePlayer *pPlayer );	// a player joining gets back what they had here
void SC_SavePlayerLeft( CBasePlayer *pPlayer );
void SC_RegisterSaveCommands( void );
void SC_RegisterRiftCvars( void );		// the rift's creatures (sc_entities.cpp)

// explosions (sc_blast.cpp): Minecraft's, by power (creeper 3, TNT 4); one in dropOneIn broken blocks drops
void SC_Explosion( const Vector &center, float power, int dropOneIn );
void SC_HLExplosion( const Vector &center, float damage );	// a Half-Life explosion of this much damage
void SC_BlastFeel( const Vector &center, float power );	// shake, flash, kick for those near (in SC_Explosion)
void SC_BulletHitWorld( const Vector &end, const Vector &normal, float damage );	// glass, leaves, wood give way
void SC_BulletSplash( const Vector &src, const Vector &end );	// a monster's round into water splashes (players': the client)
void SC_WaterEntry( CBaseEntity *pEnt, float speed );	// falling into water splashes (sc_blast.cpp)
// a round that hit tr goes on through it if it can: tr becomes the next hit, from the point it left (sc_blast.cpp)
bool SC_BulletPenetrate( CBaseEntity *pShooter, TraceResult &tr, Vector &from, const Vector &dir, float &power, float distance );
void SC_BulletLog( const char *side, int surf, float fraction, const Vector &end );	// sc_bulletlog 1
void SC_RegisterBallisticsCvars( void );
void SC_PlayerAcoustics( CBasePlayer *pPlayer );	// the room type (echo) around a player (sc_acoustics.cpp)
void SC_PlayerSuffocate( CBasePlayer *pPlayer );	// a head inside a block (sc_acoustics.cpp)

#endif // SC_GAME_H
