/*
sc_recipes.h - Svencraft crafting recipes, shared by the server (crafting) and the client (the recipe book)

Patterns are rows separated by '/', '.' for an empty cell, letters looked up in the key ("P=15 S=64" maps P to
item 15). Like Minecraft, a shaped recipe can sit anywhere in the grid and mirrored left to right; one wider or
taller than two needs a crafting table. A pattern starting with '*' is shapeless: just the ingredients.
Furnace recipes and fuels are at the end.
*/
#ifndef SC_RECIPES_H
#define SC_RECIPES_H

#include "sc_items.h"

typedef struct
{
	const char	*pattern;
	const char	*key;
	int		out;
	int		count;
} screcipedef_t;

// item ids used in keys: 4 cobblestone, 13 log, 14 leaves, 15 planks, 16 glass, 23 rubber; items from 64 (sc_items.h)
static const screcipedef_t g_SCRecipeDefs[] =
{
	// Minecraft
	{ "*L",			"L=13",				15,			4 },	// planks
	{ "P/P",		"P=15",				SCITEM_STICK,		4 },
	{ "PP/PP",		"P=15",				24,			1 },	// crafting table
	{ "CCC/C.C/CCC",	"C=4",				25,			1 },	// furnace
	{ "C/S",		"C=65 S=64",			26,			4 },	// torches from coal
	{ ".I/I.",		"I=67",				SCITEM_SHEARS,		1 },
	{ "C/S",		"C=66 S=64",			26,			4 },	// or charcoal
	{ "PPP/.S./.S.",	"P=15 S=64",			SCITEM_WOOD_PICKAXE,	1 },
	{ "CCC/.S./.S.",	"C=4 S=64",			SCITEM_STONE_PICKAXE,	1 },
	{ "III/.S./.S.",	"I=67 S=64",			SCITEM_IRON_PICKAXE,	1 },
	{ "P/S/S",		"P=15 S=64",			SCITEM_WOOD_SHOVEL,	1 },
	{ "C/S/S",		"C=4 S=64",			SCITEM_STONE_SHOVEL,	1 },
	{ "I/S/S",		"I=67 S=64",			SCITEM_IRON_SHOVEL,	1 },
	{ "PP/PS/.S",		"P=15 S=64",			SCITEM_WOOD_AXE,		1 },
	{ "CC/CS/.S",		"C=4 S=64",			SCITEM_STONE_AXE,		1 },
	{ "II/IS/.S",		"I=67 S=64",			SCITEM_IRON_AXE,		1 },
	{ "MMM/.S./.S.",	"M=69 S=64",			SCITEM_DIAMOND_PICKAXE,	1 },
	{ "M/S/S",		"M=69 S=64",			SCITEM_DIAMOND_SHOVEL,	1 },
	{ "MM/MS/.S",		"M=69 S=64",			SCITEM_DIAMOND_AXE,	1 },

	// the Half-Life side: guns, ammo and supplies from what the two worlds give
	{ "GAG/III",		"G=16 A=68 I=67",		SCITEM_CIRCUIT,		2 },
	{ "..I/.I./I..",	"I=67",				SCITEM_CROWBAR,		1 },
	{ "III/P..",		"I=67 P=15",			SCITEM_HANDGUN,		1 },
	{ "III/PA.",		"I=67 P=15 A=68",		SCITEM_MAGNUM,		1 },
	{ "III/PP.",		"I=67 P=15",			SCITEM_SHOTGUN,		1 },
	{ "III/PIC",		"I=67 P=15 C=72",		SCITEM_MP5,		1 },
	{ "SIS/RSR/.S.",	"S=64 I=67 R=23",		SCITEM_CROSSBOW,		1 },
	{ "SIS/TST/.S.",	"S=64 I=67 T=77",		SCITEM_CROSSBOW,		1 },	// strung with a spider's string
	{ ".I./IGI/.I.",	"I=67 G=71",			SCITEM_GRENADE,		1 },
	{ "III/GCG/P..",	"I=67 G=71 C=72 P=15",		SCITEM_RPG,		1 },
	{ "I/G",		"I=67 G=71",			SCITEM_AMMO_9MM,		1 },
	{ "A/G",		"A=68 G=71",			SCITEM_AMMO_357,		1 },
	{ "R/G",		"R=23 G=71",			SCITEM_AMMO_SHELLS,	1 },
	{ "I/G/I",		"I=67 G=71",			SCITEM_AMMO_MP5,		1 },
	{ "I/S/R",		"I=67 S=64 R=23",		SCITEM_AMMO_BOLTS,	1 },
	{ "B/S",		"B=76 S=64",			SCITEM_AMMO_BOLTS,	1 },	// bone-tipped
	{ "I/G/G",		"I=67 G=71",			SCITEM_AMMO_ROCKET,	1 },
	{ ".L./LRL/.L.",	"L=14 R=23",			SCITEM_MEDKIT,		1 },
	{ "A/C/I",		"A=68 C=72 I=67",		SCITEM_BATTERY,		1 },
};
#define SC_NUM_RECIPES	((int)( sizeof( g_SCRecipeDefs ) / sizeof( g_SCRecipeDefs[0] )))

typedef struct
{
	int	w, h;		// shaped size (shapeless: w = ingredient count, h = 0)
	int	cells[9];	// shaped: row-major w x h ids, 0 = empty; shapeless: the ingredients
	int	out, count;
} screcipe_t;

static inline int SC_RecipeKey( const char *key, char c )
{
	for( const char *p = key; *p; p++ )
	{
		if( p[0] == c && p[1] == '=' )
		{
			int v = 0;
			for( p += 2; *p >= '0' && *p <= '9'; p++ )
				v = v * 10 + ( *p - '0' );
			return v;
		}
	}
	return 0;
}

static inline void SC_ParseRecipe( const screcipedef_t *d, screcipe_t *r )
{
	const char *p = d->pattern;
	int i;
	r->out = d->out;
	r->count = d->count;
	for( i = 0; i < 9; i++ )
		r->cells[i] = 0;
	if( *p == '*' )
	{
		r->w = r->h = 0;
		for( p++; *p && r->w < 9; p++ )
			r->cells[r->w++] = SC_RecipeKey( d->key, *p );
		return;
	}
	int x = 0, y = 0, w = 0;
	for( ; *p; p++ )
	{
		if( *p == '/' ) { y++; x = 0; continue; }
		if( x < 3 && y < 3 )
			r->cells[y * 3 + x] = *p == '.' ? 0 : SC_RecipeKey( d->key, *p );
		x++;
		if( x > w ) w = x;
	}
	r->w = w;
	r->h = y + 1;
	// repack from stride 3 to stride w
	int tmp[9];
	for( i = 0; i < 9; i++ ) tmp[i] = r->cells[i];
	for( i = 0; i < 9; i++ ) r->cells[i] = 0;
	for( y = 0; y < r->h; y++ )
		for( x = 0; x < r->w; x++ )
			r->cells[y * r->w + x] = tmp[y * 3 + x];
}

static inline bool SC_RecipeNeedsTable( const screcipe_t *r )
{
	return r->h ? ( r->w > 2 || r->h > 2 ) : r->w > 4;
}

// grid: 3x3 item ids (the 2x2 inventory grid uses cells 0, 1, 3, 4)
static inline bool SC_MatchRecipe( const screcipe_t *r, const int *grid )
{
	int x0 = 3, y0 = 3, x1 = -1, y1 = -1, n = 0, i;
	for( i = 0; i < 9; i++ )
	{
		if( !grid[i] ) continue;
		n++;
		int x = i % 3, y = i / 3;
		if( x < x0 ) x0 = x;
		if( x > x1 ) x1 = x;
		if( y < y0 ) y0 = y;
		if( y > y1 ) y1 = y;
	}
	if( !n )
		return false;
	if( !r->h )
	{
		// shapeless: the same multiset
		if( n != r->w ) return false;
		bool used[9] = { false };
		for( i = 0; i < 9; i++ )
		{
			if( !grid[i] ) continue;
			int k;
			for( k = 0; k < r->w; k++ )
				if( !used[k] && r->cells[k] == grid[i] ) { used[k] = true; break; }
			if( k == r->w ) return false;
		}
		return true;
	}
	if( x1 - x0 + 1 != r->w || y1 - y0 + 1 != r->h )
		return false;
	for( int mirror = 0; mirror < 2; mirror++ )
	{
		bool ok = true;
		for( int y = 0; y < r->h && ok; y++ )
			for( int x = 0; x < r->w && ok; x++ )
			{
				int px = mirror ? r->w - 1 - x : x;
				if( grid[( y0 + y ) * 3 + x0 + x] != r->cells[y * r->w + px] )
					ok = false;
			}
		if( ok )
			return true;
	}
	return false;
}

// what one craft takes, per ingredient id (for the recipe book)
static inline int SC_RecipeNeeds( const screcipe_t *r, int *ids, int *counts )
{
	int n = 0, total = r->h ? r->w * r->h : r->w;
	for( int i = 0; i < total; i++ )
	{
		int id = r->cells[i], k;
		if( !id ) continue;
		for( k = 0; k < n; k++ )
			if( ids[k] == id ) { counts[k]++; break; }
		if( k == n ) { ids[n] = id; counts[n] = 1; n++; }
	}
	return n;
}

// furnace: what smelts into what, and how long fuels burn (Minecraft's ticks: 20 a second, an item takes 200)
typedef struct { int in, out; } scsmelt_t;
static const scsmelt_t g_SCSmelting[] =
{
	{ 9, SCITEM_IRON_INGOT },		// iron ore
	{ 10, SCITEM_GOLD_INGOT },	// gold ore
	{ 22, SCITEM_IRON_INGOT },	// scrap metal from cars
	{ 6, 16 },			// sand -> glass
	{ 4, 3 },			// cobblestone -> stone
	{ 13, SCITEM_CHARCOAL },		// log
};
#define SC_NUM_SMELTING	((int)( sizeof( g_SCSmelting ) / sizeof( g_SCSmelting[0] )))
#define SC_SMELT_TICKS	200

typedef struct { int id, ticks; } scfuel_t;
static const scfuel_t g_SCFuels[] =
{
	{ SCITEM_COAL, 1600 }, { SCITEM_CHARCOAL, 1600 }, { 13, 300 }, { 15, 300 }, { 24, 300 }, { SCITEM_STICK, 100 },
};
#define SC_NUM_FUELS	((int)( sizeof( g_SCFuels ) / sizeof( g_SCFuels[0] )))

static inline int SC_SmeltResult( int id )
{
	for( int i = 0; i < SC_NUM_SMELTING; i++ )
		if( g_SCSmelting[i].in == id ) return g_SCSmelting[i].out;
	return 0;
}

static inline int SC_FuelTicks( int id )
{
	for( int i = 0; i < SC_NUM_FUELS; i++ )
		if( g_SCFuels[i].id == id ) return g_SCFuels[i].ticks;
	return 0;
}

// the inventory screens: slot numbers used by the client's clicks and the server's inventory message
#define SC_INV_SLOTS		36	// 0..8 the hotbar, 9..35 the rest
#define SC_HOTBAR_SLOTS		9
#define SCSLOT_GRID		36	// 36..44: crafting grid, row-major 3x3 (the inventory's 2x2 uses 0, 1, 3, 4)
#define SCSLOT_RESULT		45
#define SCSLOT_FURNACE_IN	46
#define SCSLOT_FURNACE_FUEL	47
#define SCSLOT_FURNACE_OUT	48
#define SCSLOT_OUTSIDE		-1	// clicking outside the window drops what the mouse carries

enum { SCS_NONE = 0, SCS_INVENTORY, SCS_TABLE, SCS_FURNACE };

#endif // SC_RECIPES_H
