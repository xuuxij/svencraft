/*
sc_crafting.cpp - Svencraft: the inventory screens, crafting and furnaces

The client draws the screens (cl_dll/svencraft/hud_inventory.cpp) and only reports what the mouse does
("sc_click <slot> <button> <shift>"); the rules are Minecraft's and they all run here, the result going back
in the SCInv message. The grid is the inventory's 2x2 or a crafting table's 3x3 (sc_recipes.h has the recipes).
Tools and guns are items like any other (a gun comes with its first load); supplies are handed straight to the
player: ammo, a health kit, a battery. Furnaces smelt on Minecraft's clock (20 ticks a second, 10 seconds an item).
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "sc_game.h"

static screcipe_t g_SCRecipes[SC_NUM_RECIPES];
static bool g_bSCRecipesParsed = false;

static void SC_ParseRecipes( void )
{
	if( g_bSCRecipesParsed )
		return;
	for( int i = 0; i < SC_NUM_RECIPES; i++ )
		SC_ParseRecipe( &g_SCRecipeDefs[i], &g_SCRecipes[i] );
	g_bSCRecipesParsed = true;
}

static bool SC_GridCell( const SCInventory &inv, int cell )
{
	if( inv.screen == SCS_TABLE )
		return true;
	return inv.screen == SCS_INVENTORY && cell % 3 < 2 && cell / 3 < 2;
}

static int SC_FindRecipe( const SCInventory &inv )
{
	if( inv.screen != SCS_INVENTORY && inv.screen != SCS_TABLE )
		return -1;
	SC_ParseRecipes();
	int ids[9];
	for( int i = 0; i < 9; i++ )
		ids[i] = ( inv.grid[i].count > 0 && SC_GridCell( inv, i )) ? inv.grid[i].id : 0;
	for( int r = 0; r < SC_NUM_RECIPES; r++ )
	{
		if( inv.screen != SCS_TABLE && SC_RecipeNeedsTable( &g_SCRecipes[r] ))
			continue;
		if( SC_MatchRecipe( &g_SCRecipes[r], ids ))
			return r;
	}
	return -1;
}

int SC_CraftResult( CBasePlayer *pPlayer, int *count )
{
	int r = SC_FindRecipe( SC_Inv( pPlayer ));
	if( r < 0 )
	{
		*count = 0;
		return 0;
	}
	*count = g_SCRecipes[r].count;
	return g_SCRecipes[r].out;
}

//
// moving stacks around
//
static void SC_Clear( scslot_t &s )
{
	s.id = 0;
	s.count = 0;
}

// as much of `from` as fits onto matching stacks in slots[first..last], then into empty ones
static void SC_MoveInto( scslot_t &from, scslot_t *slots, int first, int last )
{
	int max = SC_StackSize( from.id );
	for( int pass = 0; pass < 2 && from.count > 0; pass++ )
		for( int i = first; i <= last && from.count > 0; i++ )
		{
			scslot_t &s = slots[i];
			if( pass == 0 ? ( s.count <= 0 || s.id != from.id ) : s.count > 0 )
				continue;
			if( pass == 1 )
			{
				s.id = from.id;
				s.dmg = from.dmg;
			}
			int n = Q_min( from.count, max - s.count );
			s.count += n;
			from.count -= n;
		}
	if( from.count <= 0 )
		SC_Clear( from );
}

void SC_ThrowItem( CBasePlayer *pPlayer, int id, int count, int dmg )
{
	if( count <= 0 || !SC_Item( id ))
		return;
	UTIL_MakeVectors( pPlayer->pev->v_angle );
	Vector at = pPlayer->GetGunPosition() + gpGlobals->v_forward * 24 - Vector( 0, 0, 12 );
	CBaseEntity *pDrop = SC_SpawnDrop( at, id, count, dmg );
	if( pDrop )
	{
		// thrown forward, and not picked straight back up
		pDrop->pev->velocity = gpGlobals->v_forward * 260 + Vector( 0, 0, 80 );
		pDrop->pev->fuser2 = gpGlobals->time + 1.6f;	// its age counts from then
	}
}

// grid and mouse go back into the inventory when a screen closes; what doesn't fit is thrown
static void SC_ReturnLoose( CBasePlayer *pPlayer, SCInventory &inv )
{
	for( int i = 0; i < 9; i++ )
	{
		SC_MoveInto( inv.grid[i], inv.slot, 0, SC_INV_SLOTS - 1 );
		if( inv.grid[i].count > 0 )
		{
			SC_ThrowItem( pPlayer, inv.grid[i].id, inv.grid[i].count, inv.grid[i].dmg );
			SC_Clear( inv.grid[i] );
		}
	}
	SC_MoveInto( inv.cursor, inv.slot, 0, SC_INV_SLOTS - 1 );
	if( inv.cursor.count > 0 )
	{
		SC_ThrowItem( pPlayer, inv.cursor.id, inv.cursor.count, inv.cursor.dmg );
		SC_Clear( inv.cursor );
	}
}

void SC_OpenScreen( CBasePlayer *pPlayer, int screen, int furnace )
{
	SCInventory &inv = SC_Inv( pPlayer );
	if( inv.screen != SCS_NONE && inv.screen != screen )
		SC_ReturnLoose( pPlayer, inv );
	inv.screen = screen;
	inv.furnace = furnace;
	SC_InvSend( pPlayer );
}

void SC_CloseScreen( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	if( inv.screen == SCS_NONE )
		return;
	SC_ReturnLoose( pPlayer, inv );
	inv.screen = SCS_NONE;
	inv.furnace = -1;
	SC_InvSend( pPlayer );
}

//
// crafting results
//
// supplies go straight to the player
static bool SC_Hand( CBasePlayer *pPlayer, int id )
{
	const scitem_t *it = SC_Item( id );
	if( !it || it->kind != SCI_SUPPLY || !it->give )
		return false;
	pPlayer->GiveNamedItem( it->give );
	return true;
}

static void SC_ConsumeGrid( SCInventory &inv )
{
	for( int i = 0; i < 9; i++ )
		if( inv.grid[i].count > 0 && SC_GridCell( inv, i ) && --inv.grid[i].count <= 0 )
			SC_Clear( inv.grid[i] );
}

static void SC_TakeResult( CBasePlayer *pPlayer, SCInventory &inv, bool shift )
{
	int r = SC_FindRecipe( inv );
	if( r < 0 )
		return;
	const screcipe_t &rc = g_SCRecipes[r];
	const scitem_t *it = SC_Item( rc.out );
	if( !it )
		return;
	int made = 0;
	do
	{
		if( it->kind == SCI_SUPPLY )
		{
			if( !SC_Hand( pPlayer, rc.out ))
				break;
		}
		else if( shift )
		{
			// straight into the inventory, as long as it fits and the grid holds out
			scslot_t add = { rc.out, rc.count };
			scslot_t test[SC_INV_SLOTS];
			memcpy( test, inv.slot, sizeof( test ));
			SC_MoveInto( add, test, 0, SC_INV_SLOTS - 1 );
			if( add.count > 0 )
				break;
			memcpy( inv.slot, test, sizeof( test ));
		}
		else
		{
			// onto the mouse
			if( inv.cursor.count > 0 && ( inv.cursor.id != rc.out || inv.cursor.count + rc.count > it->stack ))
				break;
			inv.cursor.id = rc.out;
			inv.cursor.count += rc.count;
			inv.cursor.dmg = 0;
		}
		if( it->kind == SCI_WEAPON )
			SC_ArmWeapon( pPlayer, it->give );	// a new gun comes loaded
		SC_ConsumeGrid( inv );
		made++;
	} while( shift && made < 64 && SC_FindRecipe( inv ) == r );

	if( made )
		EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_ITEM, it->kind == SCI_WEAPON ? "items/gunpickup2.wav" : "items/9mmclip2.wav", 0.8f, ATTN_NORM, 0, 100 );
}

//
// clicks
//
static scslot_t *SC_ClickSlot( SCInventory &inv, int slot )
{
	if( slot >= 0 && slot < SC_INV_SLOTS )
		return &inv.slot[slot];
	if( slot >= SCSLOT_GRID && slot < SCSLOT_GRID + 9 )
		return SC_GridCell( inv, slot - SCSLOT_GRID ) ? &inv.grid[slot - SCSLOT_GRID] : NULL;
	if( slot >= SCSLOT_FURNACE_IN && slot <= SCSLOT_FURNACE_OUT && inv.screen == SCS_FURNACE )
		return SC_FurnaceSlot( inv.furnace, slot - SCSLOT_FURNACE_IN );
	return NULL;
}

// shift-click: to the other side of the screen
static void SC_QuickMove( SCInventory &inv, int slot, scslot_t &s )
{
	if( s.count <= 0 )
		return;
	if( slot >= SC_INV_SLOTS )
	{
		// grid and furnace slots go back to the inventory
		SC_MoveInto( s, inv.slot, 0, SC_INV_SLOTS - 1 );
		return;
	}
	if( inv.screen == SCS_FURNACE )
	{
		// into the furnace: ore and such on top, fuel below
		scslot_t *in = SC_FurnaceSlot( inv.furnace, 0 ), *fuel = SC_FurnaceSlot( inv.furnace, 1 );
		if( in && SC_SmeltResult( s.id ))
			SC_MoveInto( s, in, 0, 0 );
		else if( fuel && SC_FuelTicks( s.id ))
			SC_MoveInto( s, fuel, 0, 0 );
		if( s.count <= 0 )
			return;
	}
	// hotbar <-> the rest
	if( slot < SC_HOTBAR_SLOTS )
		SC_MoveInto( s, inv.slot, SC_HOTBAR_SLOTS, SC_INV_SLOTS - 1 );
	else
		SC_MoveInto( s, inv.slot, 0, SC_HOTBAR_SLOTS - 1 );
}

void SC_ScreenClick( CBasePlayer *pPlayer, int slot, int button, bool shift )
{
	SCInventory &inv = SC_Inv( pPlayer );
	if( inv.screen == SCS_NONE )
		return;
	scslot_t &c = inv.cursor;

	if( slot == SCSLOT_OUTSIDE )
	{
		// throw what the mouse carries: all of it (left) or one (right)
		if( c.count > 0 )
		{
			int n = button == 0 ? c.count : 1;
			SC_ThrowItem( pPlayer, c.id, n, c.dmg );
			if(( c.count -= n ) <= 0 )
				SC_Clear( c );
		}
	}
	else if( slot == SCSLOT_RESULT )
		SC_TakeResult( pPlayer, inv, shift );
	else
	{
		scslot_t *ps = SC_ClickSlot( inv, slot );
		if( !ps )
			return;
		scslot_t &s = *ps;
		bool takeOnly = slot == SCSLOT_FURNACE_OUT;
		int max = c.count > 0 ? SC_StackSize( c.id ) : 0;

		if( shift )
			SC_QuickMove( inv, slot, s );
		else if( button == 0 )
		{
			if( c.count <= 0 )
			{
				c = s;			// pick it all up
				SC_Clear( s );
			}
			else if( takeOnly )
			{
				if( s.count > 0 && s.id == c.id )
				{
					int n = Q_min( max - c.count, s.count );
					c.count += n;
					if(( s.count -= n ) <= 0 )
						SC_Clear( s );
				}
			}
			else if( s.count <= 0 )
			{
				s = c;			// put it all down
				SC_Clear( c );
			}
			else if( s.id == c.id )
			{
				int n = Q_min( max - s.count, c.count );	// top up the stack
				s.count += n;
				if(( c.count -= n ) <= 0 )
					SC_Clear( c );
			}
			else
			{
				scslot_t t = s;		// swap
				s = c;
				c = t;
			}
		}
		else
		{
			if( c.count <= 0 )
			{
				if( s.count > 0 )
				{
					// pick up half
					int half = ( s.count + 1 ) / 2;
					c.id = s.id;
					c.dmg = s.dmg;
					c.count = half;
					if(( s.count -= half ) <= 0 )
						SC_Clear( s );
				}
			}
			else if( !takeOnly && ( s.count <= 0 || ( s.id == c.id && s.count < max )))
			{
				// put down one
				s.id = c.id;
				s.dmg = c.dmg;
				s.count++;
				if( --c.count <= 0 )
					SC_Clear( c );
			}
			else if( !takeOnly && s.id != c.id )
			{
				scslot_t t = s;
				s = c;
				c = t;
			}
		}
	}
	SC_InvSend( pPlayer );
}

// a number key over a slot (Minecraft's): swaps it with that hotbar slot; over the crafting result, makes one
// into the hotbar slot if it is free; the furnace's output only gives. Nothing while the mouse carries something
void SC_ScreenHotkey( CBasePlayer *pPlayer, int slot, int n )
{
	SCInventory &inv = SC_Inv( pPlayer );
	if( inv.screen == SCS_NONE || n < 0 || n >= SC_HOTBAR_SLOTS || inv.cursor.count > 0 || slot == n )
		return;
	scslot_t &h = inv.slot[n];
	if( slot == SCSLOT_RESULT )
	{
		if( h.count > 0 )
			return;
		SC_TakeResult( pPlayer, inv, false );	// onto the (empty) mouse, then into the slot
		h = inv.cursor;
		SC_Clear( inv.cursor );
	}
	else
	{
		scslot_t *ps = SC_ClickSlot( inv, slot );
		if( !ps )
			return;
		if( slot == SCSLOT_FURNACE_OUT )
		{
			if( ps->count <= 0 || ( h.count > 0 && h.id != ps->id ))
				return;
			int k = h.count > 0 ? Q_min( SC_StackSize( h.id ) - h.count, ps->count ) : ps->count;
			if( h.count <= 0 )
				h = *ps, h.count = 0;
			h.count += k;
			if(( ps->count -= k ) <= 0 )
				SC_Clear( *ps );
		}
		else
		{
			scslot_t t = *ps;
			*ps = h;
			h = t;
		}
	}
	SC_InvSend( pPlayer );
}

// the drop key over a slot (Minecraft's Q): throws one of it, or the whole stack
void SC_ScreenThrow( CBasePlayer *pPlayer, int slot, bool all )
{
	SCInventory &inv = SC_Inv( pPlayer );
	if( inv.screen == SCS_NONE || slot == SCSLOT_RESULT )
		return;
	scslot_t *ps = SC_ClickSlot( inv, slot );
	if( !ps || ps->count <= 0 )
		return;
	int n = all ? ps->count : 1;
	SC_ThrowItem( pPlayer, ps->id, n, ps->dmg );
	if(( ps->count -= n ) <= 0 )
		SC_Clear( *ps );
	SC_InvSend( pPlayer );
}

// a double click (Minecraft's): the second click picks up what is there if the mouse is empty, then gathers the
// same item from the inventory and the grid onto the mouse, part stacks first, up to a full stack
void SC_ScreenGather( CBasePlayer *pPlayer, int slot )
{
	SCInventory &inv = SC_Inv( pPlayer );
	scslot_t &c = inv.cursor;
	if( inv.screen == SCS_NONE )
		return;
	if( c.count <= 0 )
		SC_ScreenClick( pPlayer, slot, 0, false );
	if( c.count <= 0 )
		return;
	int max = SC_StackSize( c.id );
	for( int pass = 0; pass < 2 && c.count < max; pass++ )
	{
		for( int i = 0; i < SC_INV_SLOTS + 9 && c.count < max; i++ )
		{
			scslot_t *s = i < SC_INV_SLOTS ? &inv.slot[i] : SC_ClickSlot( inv, SCSLOT_GRID + i - SC_INV_SLOTS );
			if( !s || s->count <= 0 || s->id != c.id || s->dmg != c.dmg || ( pass == 0 && s->count >= max ))
				continue;
			int k = Q_min( max - c.count, s->count );
			c.count += k;
			if(( s->count -= k ) <= 0 )
				SC_Clear( *s );
		}
	}
	SC_InvSend( pPlayer );
}

// a drag across slots with something on the mouse (Minecraft's): left spreads it evenly over the slots that can
// take it (what doesn't divide stays on the mouse), right puts one in each
void SC_ScreenSpread( CBasePlayer *pPlayer, int button, const int *slots, int n )
{
	SCInventory &inv = SC_Inv( pPlayer );
	scslot_t &c = inv.cursor;
	if( inv.screen == SCS_NONE || c.count <= 0 )
		return;
	int max = SC_StackSize( c.id );
	scslot_t *take[SC_INV_SLOTS + 12];
	int m = 0;
	for( int i = 0; i < n && m < (int)ARRAYSIZE( take ); i++ )
	{
		if( slots[i] == SCSLOT_RESULT || slots[i] == SCSLOT_FURNACE_OUT )
			continue;
		scslot_t *s = SC_ClickSlot( inv, slots[i] );
		bool dup = false;
		for( int k = 0; k < m; k++ )
			dup |= take[k] == s;
		if( s && !dup && ( s->count <= 0 || ( s->id == c.id && s->dmg == c.dmg && s->count < max )))
			take[m++] = s;
	}
	if( !m )
		return;
	int each = button == 0 ? Q_max( c.count / m, 1 ) : 1;
	for( int k = 0; k < m && c.count > 0; k++ )
	{
		scslot_t &s = *take[k];
		int put = Q_min( Q_min( each, c.count ), max - ( s.count > 0 ? s.count : 0 ));
		if( put <= 0 )
			continue;
		if( s.count <= 0 )
		{
			s.id = c.id;
			s.dmg = c.dmg;
			s.count = 0;
		}
		s.count += put;
		if(( c.count -= put ) <= 0 )
			SC_Clear( c );
	}
	SC_InvSend( pPlayer );
}

// the recipe book: lays a recipe out in the grid from the inventory (once, or as many times as possible)
void SC_RecipeFill( CBasePlayer *pPlayer, int recipe, bool all )
{
	SCInventory &inv = SC_Inv( pPlayer );
	if(( inv.screen != SCS_INVENTORY && inv.screen != SCS_TABLE ) || recipe < 0 || recipe >= SC_NUM_RECIPES )
		return;
	SC_ParseRecipes();
	const screcipe_t &rc = g_SCRecipes[recipe];
	if( inv.screen != SCS_TABLE && SC_RecipeNeedsTable( &rc ))
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "That needs a crafting table" );
		return;
	}
	SC_ReturnLoose( pPlayer, inv );

	// how many times it can be made from what's held
	int ids[9], need[9];
	int n = SC_RecipeNeeds( &rc, ids, need );
	int times = 64;
	for( int k = 0; k < n; k++ )
	{
		int have = 0;
		for( int i = 0; i < SC_INV_SLOTS; i++ )
			if( inv.slot[i].count > 0 && inv.slot[i].id == ids[k] )
				have += inv.slot[i].count;
		times = Q_min( times, have / need[k] );
		times = Q_min( times, SC_StackSize( ids[k] ));
	}
	if( times <= 0 )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Missing ingredients" );
		SC_InvSend( pPlayer );
		return;
	}
	if( !all )
		times = 1;

	// the cells it goes in: shaped from the grid's top-left, shapeless one after another
	static const int order2[] = { 0, 1, 3, 4 }, order3[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
	int cells[9], items[9], nc = 0;
	if( rc.h )
	{
		for( int y = 0; y < rc.h; y++ )
			for( int x = 0; x < rc.w; x++ )
				if( rc.cells[y * rc.w + x] )
				{
					cells[nc] = y * 3 + x;
					items[nc++] = rc.cells[y * rc.w + x];
				}
	}
	else
	{
		const int *order = inv.screen == SCS_TABLE ? order3 : order2;
		for( int k = 0; k < rc.w; k++ )
		{
			cells[nc] = order[k];
			items[nc++] = rc.cells[k];
		}
	}
	for( int k = 0; k < nc; k++ )
	{
		int want = times;
		for( int i = SC_INV_SLOTS - 1; i >= 0 && want > 0; i-- )	// from the back, so the hotbar keeps its stacks
		{
			scslot_t &s = inv.slot[i];
			if( s.count <= 0 || s.id != items[k] )
				continue;
			int t = Q_min( want, s.count );
			if(( s.count -= t ) <= 0 )
				SC_Clear( s );
			want -= t;
		}
		inv.grid[cells[k]].id = items[k];
		inv.grid[cells[k]].count = times - want;
	}
	SC_InvSend( pPlayer );
}

//
// E (or right-click with a tool) on a crafting table or furnace
//
bool SC_PlayerUse( CBasePlayer *pPlayer )
{
	vox_api_t *v = SC_Vox();
	if( !v || !v->World()->active )
		return false;
	UTIL_MakeVectors( pPlayer->pev->v_angle );
	Vector src = pPlayer->GetGunPosition();
	Vector end = src + gpGlobals->v_forward * SC_REACH;
	TraceResult tr;
	UTIL_TraceLine( src, end, dont_ignore_monsters, ENT( pPlayer->pev ), &tr );
	float frac;
	int cell[3];
	float n[3];
	int id = v->TraceLine( src, end, &frac, cell, n );
	if( !id || frac > tr.flFraction + 0.001f )
		return false;
	if( id == BLOCK_CRAFTING_TABLE )
	{
		SC_OpenScreen( pPlayer, SCS_TABLE, -1 );
		return true;
	}
	if( SC_IsFurnace( id ))
	{
		int f = SC_FurnaceAt( cell, true );
		if( f >= 0 )
		{
			SC_OpenScreen( pPlayer, SCS_FURNACE, f );
			return true;
		}
	}
	return false;
}

//
// furnaces
//
#define SC_MAX_FURNACES		128
#define SC_TICK			0.05f	// Minecraft's tick

typedef struct
{
	bool		used;
	int		cell[3];
	scslot_t	slot[3];	// in, fuel, out
	int		burn, burnTotal;	// fuel ticks left / of the current fuel
	int		cook;		// ticks into the current item
	bool		changed;
	bool		lit;		// the block shows its fire (and gives light)
} scfurnace_t;

static scfurnace_t g_SCFurnaces[SC_MAX_FURNACES];
static float g_flSCNextTick, g_flSCNextSend;

void SC_CraftingReset( void )
{
	memset( g_SCFurnaces, 0, sizeof( g_SCFurnaces ));
	g_flSCNextTick = g_flSCNextSend = 0;
}

int SC_FurnaceAt( const int *cell, bool create )
{
	int freeSlot = -1;
	for( int i = 0; i < SC_MAX_FURNACES; i++ )
	{
		scfurnace_t &f = g_SCFurnaces[i];
		if( !f.used )
		{
			if( freeSlot < 0 )
				freeSlot = i;
			continue;
		}
		if( f.cell[0] == cell[0] && f.cell[1] == cell[1] && f.cell[2] == cell[2] )
			return i;
	}
	if( !create || freeSlot < 0 )
		return -1;
	scfurnace_t &f = g_SCFurnaces[freeSlot];
	memset( &f, 0, sizeof( f ));
	f.used = true;
	f.cell[0] = cell[0]; f.cell[1] = cell[1]; f.cell[2] = cell[2];
	return freeSlot;
}

void SC_FurnaceRemoved( const int *cell, const Vector &where )
{
	int i = SC_FurnaceAt( cell, false );
	if( i < 0 )
		return;
	scfurnace_t &f = g_SCFurnaces[i];
	for( int k = 0; k < 3; k++ )
		if( f.slot[k].count > 0 )
			SC_SpawnDrop( where, f.slot[k].id, f.slot[k].count );
	f.used = false;
	// anyone looking into it is shown out
	for( int p = 1; p <= gpGlobals->maxClients; p++ )
	{
		CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( p );
		if( pl && SC_Inv( pl ).screen == SCS_FURNACE && SC_Inv( pl ).furnace == i )
			SC_CloseScreen( pl );
	}
}

scslot_t *SC_FurnaceSlot( int furnace, int which )
{
	if( furnace < 0 || furnace >= SC_MAX_FURNACES || !g_SCFurnaces[furnace].used || which < 0 || which > 2 )
		return NULL;
	g_SCFurnaces[furnace].changed = true;
	return &g_SCFurnaces[furnace].slot[which];
}

const scslot_t *SC_FurnaceSlots( int furnace, int *cook, int *burn )
{
	if( furnace < 0 || furnace >= SC_MAX_FURNACES || !g_SCFurnaces[furnace].used )
		return NULL;
	const scfurnace_t &f = g_SCFurnaces[furnace];
	*cook = f.cook * 255 / SC_SMELT_TICKS;
	*burn = f.burnTotal ? f.burn * 255 / f.burnTotal : 0;
	return f.slot;
}

static void SC_FurnaceTick( scfurnace_t &f )
{
	scslot_t &in = f.slot[0], &fuel = f.slot[1], &out = f.slot[2];
	int result = in.count > 0 ? SC_SmeltResult( in.id ) : 0;
	bool canCook = result && ( out.count <= 0 || ( out.id == result && out.count < SC_StackSize( result )));
	int was = f.burn;

	if( f.burn > 0 )
		f.burn--;
	if( f.burn <= 0 && canCook && fuel.count > 0 && SC_FuelTicks( fuel.id ))
	{
		f.burn = f.burnTotal = SC_FuelTicks( fuel.id );
		if( --fuel.count <= 0 )
			SC_Clear( fuel );
		f.changed = true;
	}
	if( f.burn > 0 && canCook )
	{
		if( ++f.cook >= SC_SMELT_TICKS )
		{
			f.cook = 0;
			if( --in.count <= 0 )
				SC_Clear( in );
			out.id = result;
			out.count++;
			f.changed = true;
		}
	}
	else if( f.cook > 0 )
		f.cook = Q_max( 0, f.cook - 2 );	// cools off without fuel
	if( f.burn != was || f.cook )
		f.changed = true;

	// the front shows the fire while it burns, like Minecraft's, and lights the room
	if(( f.burn > 0 ) != f.lit )
	{
		vox_api_t *v = SC_Vox();
		int id = v ? v->Get( f.cell[0], f.cell[1], f.cell[2] ) : 0;
		f.lit = f.burn > 0;
		if( SC_IsFurnace( id ))
			v->Set( f.cell[0], f.cell[1], f.cell[2], SC_FurnaceBlock( SC_FurnaceFacing( id ), f.lit ));
	}
}

void SC_FurnaceFrame( void )
{
	if( g_flSCNextTick > gpGlobals->time + 1.0f )
		g_flSCNextTick = gpGlobals->time;	// a new map
	int ticks = 0;
	while( g_flSCNextTick <= gpGlobals->time && ticks < 20 )
	{
		g_flSCNextTick += SC_TICK;
		ticks++;
	}
	if( g_flSCNextTick < gpGlobals->time )
		g_flSCNextTick = gpGlobals->time;
	for( int t = 0; t < ticks; t++ )
		for( int i = 0; i < SC_MAX_FURNACES; i++ )
			if( g_SCFurnaces[i].used )
				SC_FurnaceTick( g_SCFurnaces[i] );

	// players watching a furnace see it work, a few times a second
	if( gpGlobals->time < g_flSCNextSend )
		return;
	g_flSCNextSend = gpGlobals->time + 0.2f;
	for( int p = 1; p <= gpGlobals->maxClients; p++ )
	{
		CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( p );
		if( !pl )
			continue;
		SCInventory &inv = SC_Inv( pl );
		if( inv.screen == SCS_FURNACE && inv.furnace >= 0 && g_SCFurnaces[inv.furnace].changed )
			SC_InvSend( pl );
	}
	for( int i = 0; i < SC_MAX_FURNACES; i++ )
		g_SCFurnaces[i].changed = false;
}

// death: everything carried spills out around the body, a stack per slot (Minecraft keeps nothing either)
void SC_PlayerDied( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	inv.screen = SCS_NONE;
	scslot_t *all[SC_INV_SLOTS + 10];
	int n = 0;
	for( int i = 0; i < SC_INV_SLOTS; i++ ) all[n++] = &inv.slot[i];
	for( int i = 0; i < 9; i++ ) all[n++] = &inv.grid[i];
	all[n++] = &inv.cursor;
	for( int i = 0; i < n; i++ )
	{
		if( all[i]->count <= 0 )
			continue;
		CBaseEntity *pDrop = SC_SpawnDrop( pPlayer->pev->origin + Vector( 0, 0, 16 ), all[i]->id, all[i]->count, all[i]->dmg );
		if( pDrop )
			pDrop->pev->fuser2 = gpGlobals->time + 2.0f;	// not swallowed again by the corpse's own respawn spot
		SC_Clear( *all[i] );
	}
	SC_InvSend( pPlayer );
}
