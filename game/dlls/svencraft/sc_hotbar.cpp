/*
sc_hotbar.cpp - Svencraft: the hotbar decides what the player holds, like Minecraft

Tools and guns are inventory items. Every frame the selected hotbar slot picks the active weapon: a tool item its
tool (at the item's tier), a gun item its Half-Life weapon, anything else the bare hand (holding the slot's block or
item). The Half-Life weapons follow the inventory: a gun item in it means the player has that weapon, and a weapon
whose last item leaves (thrown, dropped on death) goes too, its loaded rounds back into the ammo pool. Picking up a
weapon from the world takes an inventory slot (none free: it stays on the ground). Tools wear out on Minecraft's
counts: one use per block broken, two per creature hit.
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "sc_game.h"

static const char *g_SCToolWeapons[TOOL_COUNT] = { "weapon_sc_hand", "weapon_sc_pickaxe", "weapon_sc_shovel", "weapon_sc_axe" };
static bool g_bSCGiving = false;	// a weapon handed over by this file: no inventory item for it

int SC_ToolUses( int tier )
{
	static const int uses[SC_MAX_TIER + 1] = { 0, 59, 131, 250, 1561 };	// Minecraft's wood, stone, iron, diamond
	return uses[Q_max( 0, Q_min( tier, SC_MAX_TIER ))];
}

int SC_HeldTier( CBasePlayer *pPlayer, int tool )
{
	SCInventory &inv = SC_Inv( pPlayer );
	const scslot_t &s = inv.slot[inv.hotbar];
	const scitem_t *it = s.count > 0 ? SC_Item( s.id ) : NULL;
	return ( it && it->kind == SCI_TOOL && it->tool == tool ) ? it->tier : 0;
}

// whether the selected hotbar slot holds this item
bool SC_HoldingItem( CBasePlayer *pPlayer, int id )
{
	SCInventory &inv = SC_Inv( pPlayer );
	return inv.slot[inv.hotbar].count > 0 && inv.slot[inv.hotbar].id == id;
}

// right-click with food: eat one (there is no hunger here: an apple mends 20, Minecraft's 4 hunger x5)
bool SC_EatHeld( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	scslot_t &s = inv.slot[inv.hotbar];
	if( s.count <= 0 || s.id != SCITEM_APPLE || pPlayer->pev->health >= pPlayer->pev->max_health )
		return false;
	pPlayer->TakeHealth( 20.0f, DMG_GENERIC );
	EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_ITEM, "barnacle/bcl_chew1.wav", 0.8f, ATTN_NORM, 0, 120 + RANDOM_LONG( 0, 15 ));
	if( --s.count <= 0 )
		s.id = s.count = s.dmg = 0;
	SC_InvSend( pPlayer );
	return true;
}

// how many of an item the player carries: inventory, the open grid, the mouse
int SC_InvCount( CBasePlayer *pPlayer, int id )
{
	SCInventory &inv = SC_Inv( pPlayer );
	int n = 0;
	for( int i = 0; i < SC_INV_SLOTS; i++ )
		if( inv.slot[i].count > 0 && inv.slot[i].id == id )
			n += inv.slot[i].count;
	for( int i = 0; i < 9; i++ )
		if( inv.grid[i].count > 0 && inv.grid[i].id == id )
			n += inv.grid[i].count;
	if( inv.cursor.count > 0 && inv.cursor.id == id )
		n += inv.cursor.count;
	return n;
}

static int SC_WeaponItem( const char *classname )
{
	for( int id = SCI_FIRST_ITEM; id < SCI_COUNT; id++ )
	{
		const scitem_t *it = SC_Item( id );
		if( it && it->kind == SCI_WEAPON && it->give && !strcmp( it->give, classname ))
			return id;
	}
	return 0;
}

static CBasePlayerItem *SC_FindWeapon( CBasePlayer *pPlayer, const char *classname )
{
	for( int i = 0; i < MAX_ITEM_TYPES; i++ )
		for( CBasePlayerItem *p = pPlayer->m_rgpPlayerItems[i]; p; p = p->m_pNext )
			if( FClassnameIs( p->pev, classname ))
				return p;
	return NULL;
}

// hands a weapon over (with the rounds it comes with, or empty) without making an inventory item for it
static void SC_GiveWeapon( CBasePlayer *pPlayer, const char *classname, bool ammo )
{
	edict_t *pent = CREATE_NAMED_ENTITY( MAKE_STRING( classname ));
	if( FNullEnt( pent ))
		return;
	pent->v.origin = pPlayer->pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;
	DispatchSpawn( pent );
	CBasePlayerItem *pItem = (CBasePlayerItem *)CBaseEntity::Instance( pent );
	CBasePlayerWeapon *pWeapon = pItem ? (CBasePlayerWeapon *)pItem->GetWeaponPtr() : NULL;
	if( pWeapon && !ammo )
		pWeapon->m_iDefaultAmmo = 0;
	g_bSCGiving = true;
	DispatchTouch( pent, ENT( pPlayer->pev ));
	g_bSCGiving = false;
	// not taken (a duplicate with no room for its ammo): don't leave it lying around
	if( pent->v.owner != pPlayer->edict() && !( pent->v.flags & FL_KILLME ))
		UTIL_Remove( CBaseEntity::Instance( pent ));
}

static void SC_TakeWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)pItem->GetWeaponPtr();
	if( pWeapon && pWeapon->m_iClip > 0 && pWeapon->pszAmmo1())
		pPlayer->GiveAmmo( pWeapon->m_iClip, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1());	// unloaded, not lost
	pPlayer->RemovePlayerItem( pItem, true );
	pPlayer->pev->weapons &= ~( 1 << pItem->m_iId );
	pItem->Kill();
}

// crafting a gun: the weapon with its first load (another of one already held: its ammo)
void SC_ArmWeapon( CBasePlayer *pPlayer, const char *classname )
{
	SC_GiveWeapon( pPlayer, classname, true );
}

// a weapon from the world (or the give command) takes an inventory slot; false: no room, it stays put
bool SC_WeaponPickup( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	const char *cls = STRING( pItem->pev->classname );
	if( g_bSCGiving || !strncmp( cls, "weapon_sc_", 10 ))
		return true;
	int id = SC_WeaponItem( cls );
	if( !id || SC_InvCount( pPlayer, id ) > 0 )
		return true;
	return SC_InvAdd( pPlayer, id, 1 ) == 0;
}

// the Half-Life weapons follow the inventory's gun items
static void SC_SyncWeapons( CBasePlayer *pPlayer )
{
	for( int id = SCI_FIRST_ITEM; id < SCI_COUNT; id++ )
	{
		const scitem_t *it = SC_Item( id );
		if( !it || it->kind != SCI_WEAPON || !it->give )
			continue;
		int n = SC_InvCount( pPlayer, id );
		CBasePlayerItem *pItem = SC_FindWeapon( pPlayer, it->give );
		CBasePlayerWeapon *pWeapon = pItem ? (CBasePlayerWeapon *)pItem->GetWeaponPtr() : NULL;

		// thrown grenades, satchels, mines and snarks: when the last one is gone, so is the item
		if( n > 0 && pWeapon && ( pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE ) && pWeapon->PrimaryAmmoIndex() >= 0
			&& pPlayer->m_rgAmmo[pWeapon->PrimaryAmmoIndex()] <= 0 && ( pPlayer->m_pActiveItem != pItem || !pPlayer->pev->viewmodel ))
		{
			SCInventory &inv = SC_Inv( pPlayer );
			for( int i = 0; i < SC_INV_SLOTS; i++ )
				if( inv.slot[i].id == id )
					inv.slot[i].id = inv.slot[i].count = 0;
			n = 0;
			SC_InvSend( pPlayer );
		}

		if( n > 0 && !pItem )
		{
			// thrown kinds come back loaded when there are none left in the pool (picked up again after a death)
			const char *thrown = id == SCITEM_GRENADE ? "Hand Grenade" : id == SCITEM_SATCHEL ? "Satchel Charge"
				: id == SCITEM_TRIPMINE ? "Trip Mine" : id == SCITEM_SNARK ? "Snarks" : NULL;
			int ammo = thrown ? CBasePlayer::GetAmmoIndex( thrown ) : -1;
			SC_GiveWeapon( pPlayer, it->give, thrown && ( ammo < 0 || pPlayer->m_rgAmmo[ammo] <= 0 ));
		}
		else if( n <= 0 && pItem )
			SC_TakeWeapon( pPlayer, pItem );
	}
}

// the weapon for the selected slot, swapped in when it changes
static void SC_SelectHeld( CBasePlayer *pPlayer )
{
	SCInventory &inv = SC_Inv( pPlayer );
	const scslot_t &s = inv.slot[inv.hotbar];
	const scitem_t *it = s.count > 0 ? SC_Item( s.id ) : NULL;
	const char *want = g_SCToolWeapons[TOOL_HAND];
	if( it && it->kind == SCI_TOOL && it->tool > 0 && it->tool < TOOL_COUNT )
		want = g_SCToolWeapons[it->tool];
	else if( it && it->kind == SCI_WEAPON && it->give )
		want = it->give;

	int held = s.count > 0 ? s.id : 0;
	bool changed = inv.heldSlot != inv.hotbar || inv.heldId != held;
	CBasePlayerItem *pCur = pPlayer->m_pActiveItem;
	if( pCur && FClassnameIs( pCur->pev, want ))
	{
		// the same weapon for something else (another block in hand, a tool of another tier): raise it again
		if( changed && pCur->CanHolster())
		{
			inv.heldSlot = inv.hotbar;
			inv.heldId = held;
			pCur->Deploy();
		}
		return;
	}
	CBasePlayerItem *pItem = SC_FindWeapon( pPlayer, want );
	if( !pItem || ( pCur && !pCur->CanHolster()))
		return;	// not handed over yet, or the current one can't be put away this moment (a primed grenade)
	if( pCur )
		pCur->Holster();
	pPlayer->m_pLastItem = pCur;
	pPlayer->m_pActiveItem = pItem;
	inv.heldSlot = inv.hotbar;
	inv.heldId = held;
	pItem->pev->oldbuttons = 1;
	pItem->Deploy();
	pItem->pev->oldbuttons = 0;
	pItem->UpdateItemInfo();
}

void SC_PlayerFrame( CBasePlayer *pPlayer )
{
	SC_SyncGunFeel( pPlayer );
	SC_UpdateTarget( pPlayer );	// the name and health under the crosshair (sc_status.cpp)
	if( !pPlayer->IsAlive())
		return;
	SC_SyncWeapons( pPlayer );
	SC_SelectHeld( pPlayer );
	SC_PlayerAcoustics( pPlayer );
	SC_PlayerSuffocate( pPlayer );
	SC_AntiBlock( pPlayer );
}

// the held tool takes wear; worn through, it breaks
void SC_WearHeld( CBasePlayer *pPlayer, int uses )
{
	SCInventory &inv = SC_Inv( pPlayer );
	scslot_t &s = inv.slot[inv.hotbar];
	const scitem_t *it = s.count > 0 ? SC_Item( s.id ) : NULL;
	if( !it || it->kind != SCI_TOOL )
		return;
	s.dmg += uses;
	if( s.dmg >= SC_ToolUses( it->tier ))
	{
		static const char *snd[SC_MAX_TIER + 1] = { "debris/wood2.wav", "debris/wood2.wav", "debris/concrete2.wav", "debris/metal2.wav", "debris/glass2.wav" };
		EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_STATIC, snd[Q_min( it->tier, SC_MAX_TIER )], 1.0f, ATTN_NORM, 0, 110 );
		s.id = s.count = s.dmg = 0;
	}
	SC_InvSend( pPlayer );
}

// every spawn: the HUD (the HEV suit bit), the hand and tool weapons the hotbar switches between; the first spawn
// of a connection also hands out a set of wooden tools (after a death the player comes back empty-handed, as in
// Minecraft: what they carried lies where they fell)
void SC_PlayerSpawn( CBasePlayer *pPlayer )
{
	pPlayer->pev->weapons |= ( 1 << WEAPON_SUIT );
	for( int t = 0; t < TOOL_COUNT; t++ )
		if( !SC_FindWeapon( pPlayer, g_SCToolWeapons[t] ))
			SC_GiveWeapon( pPlayer, g_SCToolWeapons[t], false );
	bool tools = false;
	for( int id = SCI_FIRST_ITEM; id < SCI_COUNT && !tools; id++ )
		if( SC_Item( id ) && SC_Item( id )->kind == SCI_TOOL && SC_InvCount( pPlayer, id ) > 0 )
			tools = true;
	SCInventory &kit = SC_Inv( pPlayer );
	if( !tools && !kit.startKit )
	{
		kit.startKit = true;
		SC_InvAdd( pPlayer, SCITEM_WOOD_PICKAXE, 1 );
		SC_InvAdd( pPlayer, SCITEM_WOOD_SHOVEL, 1 );
		SC_InvAdd( pPlayer, SCITEM_WOOD_AXE, 1 );
		SC_InvAdd( pPlayer, SCITEM_FIRSTAID, 1 );	// every Sven player carries a medkit, half charged
		pPlayer->GiveAmmo( 50, "health", 100 );
	}
	SCInventory &inv = SC_Inv( pPlayer );
	inv.heldSlot = -1;
	SC_InvSend( pPlayer );
	SC_SelectHeld( pPlayer );
}

// G (drop): throws one of the held slot's items, Minecraft's Q
void SC_DropHeld( CBasePlayer *pPlayer, bool all )
{
	SCInventory &inv = SC_Inv( pPlayer );
	scslot_t &s = inv.slot[inv.hotbar];
	if( s.count <= 0 || inv.screen != SCS_NONE )
		return;
	int n = all ? s.count : 1;
	SC_ThrowItem( pPlayer, s.id, n, s.dmg );
	if(( s.count -= n ) <= 0 )
		s.id = s.count = s.dmg = 0;
	SC_InvSend( pPlayer );
}
