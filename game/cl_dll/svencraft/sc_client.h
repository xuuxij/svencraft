/*
sc_client.h - Svencraft client: the inventory as the server last sent it, and the inventory screen's hooks
*/
#ifndef SC_CLIENT_H
#define SC_CLIENT_H

#include "sc_items.h"
#include "sc_recipes.h"

typedef struct
{
	int	id;
	int	count;
	int	dur;	// tools: what is left of them, 0..255 (255 new)
} scclslot_t;

typedef struct
{
	int		hotbar;			// selected hotbar slot
	int		screen;			// SCS_* open screen
	scclslot_t	slot[SC_INV_SLOTS];
	scclslot_t	grid[9];
	scclslot_t	cursor;			// on the mouse
	scclslot_t	result;			// what the grid makes
	scclslot_t	furnace[3];		// in, fuel, out
	int		cook, burn;		// 0..255
} scclinv_t;

extern scclinv_t g_SCInv;

// the inventory screen (hud_inventory.cpp)
bool SC_UIActive( void );
void SC_UIMouse( float relyaw, float relpitch );	// mouse look while a screen is open moves its cursor
int SC_UIKey( int down, int keynum, const char *binding );	// 1: the screen took the key
void SC_UIInit( void );
void SC_UIVidInit( void );
void SC_UIScreenChanged( void );			// the server opened or closed a screen
void SC_DrawDurability( int x, int y, const scclslot_t *s );	// the wear bar under a tool's 48-pixel icon

// the hotbar (hud_hotbar.cpp)
void SC_HotbarSelect( const char *how );		// "1".."9", "next", "prev": number keys and the mouse wheel
struct cl_entity_s;
void SC_ViewModelLook( struct cl_entity_s *view );	// the hand view model holds the selected slot's block or item

// gunfire effects (sc_effects.cpp)
void SC_EffectsInit( void );
void SC_ScriptMuzzleFlash( const struct cl_entity_s *ent, const char *script );
void SC_GunshotEffects( int idx, const float *src, const float *forward, float radius );
void SC_BulletSplash( const float *src, const float *end );	// a round from the air into water splashes
void SC_ApplyBlastShake( float *origin, float *angles, float scale );
void SC_ApplyFog( void );	// the map's fog, every frame (sc_effects.cpp)
#define SC_FX_EMITTER	77	// renderfx of an sc_emitter (dlls/svencraft/sc_emitter.cpp)
void SC_EmitterThink( struct cl_entity_s *ent );	// its puffs, while it is in view (sc_effects.cpp)

#endif // SC_CLIENT_H
