/*
sc_mining.cpp - Svencraft: mining and placing blocks, the crack overlay, dropped block items
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "effects.h"
#include "sc_game.h"

extern const char *g_SCFamHit[FAM_COUNT];
extern const char *g_SCFamBreak[FAM_COUNT];
extern int g_SCFamGibIndex[FAM_COUNT];

static const float g_SCTierSpeed[SC_MAX_TIER + 1] = { 1.0f, 2.0f, 4.0f, 6.0f, 8.0f };	// Minecraft's

float SC_SwingPower( CBasePlayer *pPlayer, int tool, int id )
{
	const scblockdef_t &b = g_SCBlocks[id];
	if( b.hardness < 0 )
		return 0;
	float speed = 1.0f;
	if( tool != TOOL_HAND && tool == b.tool )
		speed = g_SCTierSpeed[SC_HeldTier( pPlayer, tool )];
	// Minecraft's mining time: hardness x 1.5 seconds (x 5 when it won't drop anything) over the tool's speed;
	// here it is spent in swings: stone with a wooden pickaxe 3, dirt with a shovel 1, stone by hand 15
	if( id == BLOCK_LEAVES && SC_HoldingItem( pPlayer, SCITEM_SHEARS ))
		speed = 15.0f;	// Minecraft's shears on leaves
	float seconds = b.hardness * ( SC_CanHarvest( pPlayer, tool, id ) ? 1.5f : 5.0f ) / speed;
	return seconds > 0.0f ? SC_SWING_TIME / seconds : 1.0f;
}

bool SC_CanHarvest( CBasePlayer *pPlayer, int tool, int id )
{
	const scblockdef_t &b = g_SCBlocks[id];
	if( b.level <= 0 )
		return true;
	return tool == TOOL_PICKAXE && SC_HeldTier( pPlayer, TOOL_PICKAXE ) >= b.level;
}

static Vector SC_CellCenter( const int *cell )
{
	return Vector(( cell[0] + 0.5f ) * VOX_BLOCK_SIZE, ( cell[1] + 0.5f ) * VOX_BLOCK_SIZE, ( cell[2] + 0.5f ) * VOX_BLOCK_SIZE );
}

void SC_Debris( const Vector &pos, int family, int count, float speed )
{
	// speed 0: a short puff where a tool hits; blasts throw chunks up and out
	float size = speed > 0 ? 64 : 16;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pos );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( pos.x ); WRITE_COORD( pos.y ); WRITE_COORD( pos.z );
		WRITE_COORD( size ); WRITE_COORD( size ); WRITE_COORD( size );
		WRITE_COORD( 0 ); WRITE_COORD( 0 ); WRITE_COORD( speed * 0.6f );
		WRITE_BYTE( speed > 0 ? (int)( speed / 10 ) : 10 );
		WRITE_SHORT( g_SCFamGibIndex[family] );
		WRITE_BYTE( count );
		WRITE_BYTE( speed > 0 ? 25 : 8 );	// life in 0.1 s: a puff, not a pile
		WRITE_BYTE( family == FAM_GLASS ? BREAK_GLASS : ( family == FAM_WOOD || family == FAM_PLANT ? BREAK_WOOD : ( family == FAM_METAL ? BREAK_METAL : BREAK_CONCRETE )));
	MESSAGE_END();
}

//
// crack overlay: an oriented sprite on the face being mined, 10 frames from scratched to shattered
//
static void SC_ShowCrack( CBasePlayer *pPlayer, const Vector &center, const Vector &normal, float progress )
{
	SCInventory &inv = SC_Inv( pPlayer );
	CSprite *pCrack = (CSprite *)(CBaseEntity *)inv.crack;
	if( !pCrack )
	{
		pCrack = CSprite::SpriteCreate( "sprites/svencraft/cracks.spr", center, FALSE );
		if( !pCrack )
			return;
		pCrack->SetTransparency( kRenderNormal, 255, 255, 255, 255, kRenderFxNone );	// alpha-tested sprite
		pCrack->pev->scale = VOX_BLOCK_SIZE / 64.0f;
		inv.crack = pCrack;
	}
	// an oriented sprite faces along its forward vector: point it into the block
	Vector angles = UTIL_VecToAngles( -normal );
	angles.x = -angles.x;
	pCrack->pev->angles = angles;
	pCrack->pev->frame = Q_min( 9.0f, floor( progress * 10.0f ));
	pCrack->pev->effects &= ~EF_NODRAW;
	UTIL_SetOrigin( pCrack->pev, center + normal * ( VOX_BLOCK_SIZE * 0.5f + 0.5f ));
}

// crack on a face at an arbitrary point (props, cars)
void SC_ShowCrackAt( CBasePlayer *pPlayer, const Vector &hit, const Vector &normal, float progress )
{
	SC_ShowCrack( pPlayer, hit - normal * ( VOX_BLOCK_SIZE * 0.5f ), normal, progress );
}

void SC_HideCrack( CBasePlayer *pPlayer )
{
	CBaseEntity *pCrack = SC_Inv( pPlayer ).crack;
	if( pCrack )
		pCrack->pev->effects |= EF_NODRAW;
}

void SC_HitBlock( CBasePlayer *pPlayer, int tool, const int *cell, const float *normal )
{
	vox_api_t *v = SC_Vox();
	if( !v )
		return;
	int id = v->Get( cell[0], cell[1], cell[2] );
	if( id <= BLOCK_AIR || id >= BLOCK_COUNT )
		return;
	const scblockdef_t &b = g_SCBlocks[id];
	SCInventory &inv = SC_Inv( pPlayer );
	Vector center = SC_CellCenter( cell );
	Vector n( normal[0], normal[1], normal[2] );

	EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_ITEM, g_SCFamHit[b.family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));

	if( b.hardness < 0 )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Unbreakable" );
		return;
	}

	// a new target (or one left alone for a while) starts over
	if( inv.mineCell[0] != cell[0] || inv.mineCell[1] != cell[1] || inv.mineCell[2] != cell[2] || gpGlobals->time - inv.lastHit > 2.0f )
	{
		inv.mineCell[0] = cell[0]; inv.mineCell[1] = cell[1]; inv.mineCell[2] = cell[2];
		inv.mineProgress = 0;
	}
	inv.lastHit = gpGlobals->time;
	inv.mineProgress += SC_SwingPower( pPlayer, tool, id );

	if( inv.mineProgress < 0.999f )
	{
		SC_Debris( center + n * ( VOX_BLOCK_SIZE * 0.5f ), b.family, 1 );
		SC_ShowCrack( pPlayer, center, n, inv.mineProgress );
		return;
	}

	// broken
	SC_HideCrack( pPlayer );
	inv.mineProgress = 0;
	v->Set( cell[0], cell[1], cell[2], BLOCK_AIR );
	SC_BlockRemoved( cell );
	SC_Debris( center, b.family, 4 );
	EMIT_AMBIENT_SOUND( ENT( pPlayer->pev ), center, g_SCFamBreak[b.family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));
	bool harvest = SC_CanHarvest( pPlayer, tool, id );
	if( b.hardness > 0 )
		SC_WearHeld( pPlayer, 1 );	// after the harvest check: the last use of a pickaxe still gets its ore

	if( !harvest )
	{
		char msg[128];
		snprintf( msg, sizeof( msg ), "%s needs a %s pickaxe", b.title, g_SCTierNames[Q_min( b.level, SC_MAX_TIER )] );
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, msg );
		return;
	}
	if( id == BLOCK_LEAVES )
	{
		// Minecraft's leaves: themselves only to shears; otherwise now and then a stick, rarely an apple
		if( SC_HoldingItem( pPlayer, SCITEM_SHEARS ))
			SC_SpawnDrop( center, BLOCK_LEAVES, 1 );
		else if( RANDOM_LONG( 0, 199 ) == 0 )
			SC_SpawnDrop( center, SCITEM_APPLE, 1 );
		else if( RANDOM_LONG( 0, 49 ) == 0 )
			SC_SpawnDrop( center, SCITEM_STICK, RANDOM_LONG( 1, 2 ));
		return;
	}
	if( b.drop != BLOCK_AIR )
		SC_SpawnDrop( center, b.drop, 1 );
}

// place the selected block against the face the player is looking at
bool SC_PlaceBlock( CBasePlayer *pPlayer, const Vector &vecSrc, const Vector &vecDir )
{
	vox_api_t *v = SC_Vox();
	if( !v || !v->World()->active )
		return false;
	int id = SC_InvHeldBlock( pPlayer );
	if( !id )
		return false;

	Vector vecEnd = vecSrc + vecDir * SC_REACH;
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pPlayer->pev ), &tr );
	float vfrac;
	int cell[3];
	float vn[3];
	int hit = v->TraceLine( vecSrc, vecEnd, &vfrac, cell, vn );

	int target[3];
	Vector face;
	if( hit && vfrac <= tr.flFraction + 0.001f )
	{
		for( int i = 0; i < 3; i++ )
			target[i] = cell[i] + (int)vn[i];
		face = Vector( vn[0], vn[1], vn[2] );
	}
	else if( tr.flFraction < 1.0f )
	{
		Vector p = tr.vecEndPos + tr.vecPlaneNormal * ( VOX_BLOCK_SIZE * 0.5f );
		for( int i = 0; i < 3; i++ )
			target[i] = (int)floor( p[i] / VOX_BLOCK_SIZE );
		face = tr.vecPlaneNormal;
	}
	else
		return false;

	// a furnace faces whoever places it
	if( id == BLOCK_FURNACE )
	{
		if( fabs( vecDir.x ) > fabs( vecDir.y ))
			id = SC_FurnaceBlock( vecDir.x > 0 ? 2 : 1, false );	// looking along +x: the front looks back along -x
		else
			id = SC_FurnaceBlock( vecDir.y > 0 ? 0 : 3, false );
	}

	// a torch stands on a floor or leans off a wall, whichever was clicked (not hung from a ceiling)
	bool torch = id == BLOCK_TORCH;
	if( torch )
	{
		if( face.z > 0.7f )
			id = BLOCK_TORCH;
		else if( face.z < -0.7f )
			return false;
		else if( fabs( face.x ) > fabs( face.y ))
			id = face.x > 0 ? BLOCK_WALL_TORCH_NX : BLOCK_WALL_TORCH_PX;	// the wall is on the other side
		else
			id = face.y > 0 ? BLOCK_WALL_TORCH_NY : BLOCK_WALL_TORCH_PY;
	}

	const vox_world_t *w = v->World();
	for( int i = 0; i < 3; i++ )
		if( target[i] < w->mins[i] || target[i] >= w->maxs[i] )
			return false;
	if( v->Get( target[0], target[1], target[2] ))
		return false;

	// the map's own geometry must not be in the way (sample the cell's corners and centre); the realistic
	// ground doesn't follow the block grid, so a block may sink up to a fifth of its height into it
	Vector mins( target[0] * VOX_BLOCK_SIZE, target[1] * VOX_BLOCK_SIZE, target[2] * VOX_BLOCK_SIZE );
	Vector maxs = mins + Vector( VOX_BLOCK_SIZE, VOX_BLOCK_SIZE, VOX_BLOCK_SIZE );
	for( int k = torch ? 8 : 0; k < 9; k++ )	// a torch only needs its middle free
	{
		Vector p = k == 8 ? ( mins + maxs ) * 0.5f : Vector(( k & 1 ) ? maxs.x - 1 : mins.x + 1, ( k & 2 ) ? maxs.y - 1 : mins.y + 1, ( k & 4 ) ? maxs.z - 1 : mins.z + VOX_BLOCK_SIZE * 0.2f );
		if( UTIL_PointContents( p ) == CONTENTS_SOLID )
			return false;
	}

	// nobody standing there (torches don't get in anyone's way)
	CBaseEntity *list[16];
	int n = torch ? 0 : UTIL_EntitiesInBox( list, 16, mins + Vector( 0.5f, 0.5f, 0.5f ), maxs - Vector( 0.5f, 0.5f, 0.5f ), FL_CLIENT | FL_MONSTER );
	for( int i = 0; i < n; i++ )
		if( list[i]->IsAlive() && list[i]->pev->solid != SOLID_NOT )
		{
			ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Something is in the way" );
			return false;
		}

	v->Set( target[0], target[1], target[2], id );
	SC_InvTakeHeld( pPlayer );
	SC_CheckFall( target );	// sand and gravel placed in the air drop
	EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_ITEM, g_SCFamHit[g_SCBlocks[id].family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));
	return true;
}

//
// sc_drop: a small spinning block that pops out of a broken block and flies to a nearby player
//
class CSCDrop : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT DropThink( void );
	void EXPORT DropTouch( CBaseEntity *pOther );
	void Collect( CBasePlayer *pPlayer );
};

LINK_ENTITY_TO_CLASS( sc_drop, CSCDrop )

void CSCDrop::Spawn( void )
{
	SET_MODEL( ENT( pev ), "models/svencraft/blockitem.mdl" );
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	pev->scale = 0.25f;
	pev->gravity = 1.0f;
	pev->friction = 0.6f;
	UTIL_SetSize( pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ));
	UTIL_SetOrigin( pev, pev->origin );
	pev->velocity = Vector( RANDOM_FLOAT( -60, 60 ), RANDOM_FLOAT( -60, 60 ), RANDOM_FLOAT( 120, 200 ));
	pev->fuser2 = gpGlobals->time;
	SetThink( &CSCDrop::DropThink );
	SetTouch( &CSCDrop::DropTouch );
	pev->nextthink = gpGlobals->time + 0.05f;
}

void CSCDrop::Collect( CBasePlayer *pPlayer )
{
	// pev->iuser1 is the item id (pev->skin is only the look), pev->fuser1 how many
	if( gpGlobals->time < pev->fuser3 )
		return;	// that player's inventory was full a moment ago
	int amount = Q_max( 1, (int)pev->fuser1 );
	int left = SC_InvAdd( pPlayer, pev->iuser1, amount, pev->iuser2 );
	if( left >= amount )
	{
		pev->fuser3 = gpGlobals->time + 1.0f;
		return;
	}
	EMIT_SOUND( ENT( pPlayer->pev ), CHAN_ITEM, "items/9mmclip1.wav", 0.7f, ATTN_NORM );
	const scitem_t *it = SC_Item( pev->iuser1 );
	char msg[96];
	snprintf( msg, sizeof( msg ), "+%d %s", amount - left, it ? it->name : "?" );
	ClientPrint( pPlayer->pev, HUD_PRINTCENTER, msg );
	if( left > 0 )
	{
		pev->fuser1 = (float)left;
		pev->fuser3 = gpGlobals->time + 1.0f;
		return;
	}
	UTIL_Remove( this );
}

void CSCDrop::DropTouch( CBaseEntity *pOther )
{
	if( pOther->IsPlayer() && pOther->IsAlive() && gpGlobals->time - pev->fuser2 > 0.4f )
		Collect( (CBasePlayer *)pOther );
}

void CSCDrop::DropThink( void )
{
	pev->nextthink = gpGlobals->time + 0.05f;
	pev->angles.y += 9.0f;
	float age = gpGlobals->time - pev->fuser2;
	if( age > 300.0f )
	{
		UTIL_Remove( this );
		return;
	}
	if( age < 0.4f )
		return;

	// magnet: fly to the nearest living player in range
	CBasePlayer *pBest = NULL;
	float best = 96.0f;
	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *p = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if( !p || !p->IsAlive() )
			continue;
		float d = ( p->pev->origin - pev->origin ).Length();
		if( d < best )
		{
			best = d;
			pBest = p;
		}
	}
	if( !pBest )
		return;
	if( best < 36.0f )
	{
		Collect( pBest );
		return;
	}
	Vector dir = ( pBest->pev->origin - pev->origin ).Normalize();
	pev->movetype = MOVETYPE_FLY;
	pev->velocity = dir * 300.0f;
}

CBaseEntity *SC_SpawnDrop( const Vector &vecPos, int id, int amount, int dmg )
{
	if( !SC_Item( id ) || amount <= 0 )
		return NULL;
	CBaseEntity *pDrop = CBaseEntity::Create( "sc_drop", vecPos, g_vecZero, NULL );
	if( !pDrop )
		return NULL;
	pDrop->pev->iuser1 = id;
	pDrop->pev->iuser2 = dmg;	// a tool's wear goes with it
	pDrop->pev->fuser1 = (float)amount;
	if( g_SCFlatSkin[id] < 0 )
		pDrop->pev->skin = id;	// a little block
	else
	{
		// a flat item, like Minecraft's
		SET_MODEL( pDrop->edict(), "models/svencraft/itemflat.mdl" );
		pDrop->pev->skin = g_SCFlatSkin[id];
		pDrop->pev->scale = 0.8f;
		UTIL_SetSize( pDrop->pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ));
	}
	return pDrop;
}

//
// digging the realistic map geometry: each block-sized cell of it is mined like a block (same grid as the
// block world below, so a hole dug through the ground lines up with the blocks underneath)
//
void SC_HitGround( CBasePlayer *pPlayer, int tool, int material, const Vector &vecHit, const Vector &vecNormal )
{
	dyn_api_t *d = SC_Dyn();
	if( !d || material <= BLOCK_AIR || material >= BLOCK_COUNT )
		return;
	const scblockdef_t &b = g_SCBlocks[material];
	SCInventory &inv = SC_Inv( pPlayer );

	// the cell behind the face that was hit; a hit exactly on a cell boundary belongs to whichever side has ground
	Vector inside = vecHit - vecNormal * 2.0f;
	for( int i = 0; i < 3; i++ )
	{
		float f = inside[i] / VOX_BLOCK_SIZE - floor( inside[i] / VOX_BLOCK_SIZE );
		if( f > 0.01f && f < 0.99f )
			continue;
		Vector lo = inside, hi = inside;
		lo[i] -= 0.5f;
		hi[i] += 0.5f;
		if( d->PointSolid( lo ) && !d->PointSolid( hi ))
			inside = lo;
		else if( d->PointSolid( hi ))
			inside = hi;
	}
	int cell[3];
	for( int i = 0; i < 3; i++ )
		cell[i] = (int)floor( inside[i] / VOX_BLOCK_SIZE );

	EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_ITEM, g_SCFamHit[b.family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));

	if( inv.mineCell[0] != cell[0] || inv.mineCell[1] != cell[1] || inv.mineCell[2] != cell[2] || gpGlobals->time - inv.lastHit > 2.0f )
	{
		inv.mineCell[0] = cell[0]; inv.mineCell[1] = cell[1]; inv.mineCell[2] = cell[2];
		inv.mineProgress = 0;
	}
	inv.lastHit = gpGlobals->time;
	// a cell only partly filled (a 16-unit wall, a sidewalk slab) breaks faster, in proportion
	{
		Vector cmins( cell[0] * VOX_BLOCK_SIZE, cell[1] * VOX_BLOCK_SIZE, cell[2] * VOX_BLOCK_SIZE );
		Vector cmaxs = cmins + Vector( VOX_BLOCK_SIZE, VOX_BLOCK_SIZE, VOX_BLOCK_SIZE );
		float fill = d->BoxVolume( cmins, cmaxs, NULL, 0 ) / ( VOX_BLOCK_SIZE * VOX_BLOCK_SIZE * VOX_BLOCK_SIZE );
		inv.mineProgress += SC_SwingPower( pPlayer, tool, material ) / Q_max( fill, 0.2f );
	}

	if( inv.mineProgress < 0.999f )
	{
		SC_Debris( vecHit, b.family, 1 );
		SC_ShowCrack( pPlayer, vecHit - vecNormal * ( VOX_BLOCK_SIZE * 0.5f ), vecNormal, inv.mineProgress );
		return;
	}

	SC_HideCrack( pPlayer );
	inv.mineProgress = 0;
	Vector mins( cell[0] * VOX_BLOCK_SIZE, cell[1] * VOX_BLOCK_SIZE, cell[2] * VOX_BLOCK_SIZE );
	Vector maxs = mins + Vector( VOX_BLOCK_SIZE, VOX_BLOCK_SIZE, VOX_BLOCK_SIZE );
	float volume[BLOCK_COUNT];
	float removed = d->CarveBox( mins, maxs, volume, BLOCK_COUNT );
	Vector center = ( mins + maxs ) * 0.5f;
	SC_Debris( center, b.family, 4 );
	EMIT_AMBIENT_SOUND( ENT( pPlayer->pev ), center, g_SCFamBreak[b.family], 1.0f, ATTN_NORM, 0, 95 + RANDOM_LONG( 0, 10 ));

	// material comes out by volume: a full cell of dirt is one block, a 16-unit brick wall gives 0.4 of a
	// block per cell, so a whole wall yields as many bricks as it holds
	if( removed <= 0 )
		return;
	const float cellvol = VOX_BLOCK_SIZE * VOX_BLOCK_SIZE * VOX_BLOCK_SIZE;
	for( int m = 1; m < BLOCK_COUNT; m++ )
	{
		if( volume[m] <= 0 || !SC_CanHarvest( pPlayer, tool, m ))
			continue;
		inv.partial[m] += volume[m] / cellvol;
		while( inv.partial[m] >= 0.999f )
		{
			inv.partial[m] -= 1.0f;
			SC_SpawnDrop( center, g_SCBlocks[m].drop, 1 );
		}
	}
	SC_WearHeld( pPlayer, 1 );
}

//
// torches: not solid, so the engine's traces pass through them; found and knocked off here
//
bool SC_IsTorch( int id )
{
	return id >= BLOCK_TORCH && id <= BLOCK_WALL_TORCH_NY;
}

// the nearest torch along a line (stepping through the cells it crosses), 0 if none before maxdist
int SC_TorchTarget( const Vector &src, const Vector &dir, float maxdist, int *cell )
{
	vox_api_t *v = SC_Vox();
	if( !v || !v->World()->active )
		return 0;
	int last[3] = { 1 << 30, 0, 0 };
	for( float d = 0; d <= maxdist; d += 4.0f )
	{
		Vector p = src + dir * d;
		int c[3] = { (int)floor( p.x / VOX_BLOCK_SIZE ), (int)floor( p.y / VOX_BLOCK_SIZE ), (int)floor( p.z / VOX_BLOCK_SIZE ) };
		if( c[0] == last[0] && c[1] == last[1] && c[2] == last[2] )
			continue;
		last[0] = c[0]; last[1] = c[1]; last[2] = c[2];
		int id = v->Get( c[0], c[1], c[2] );
		if( SC_IsTorch( id ))
		{
			cell[0] = c[0]; cell[1] = c[1]; cell[2] = c[2];
			return id;
		}
	}
	return 0;
}

static void SC_DropTorch( const int *cell )
{
	vox_api_t *v = SC_Vox();
	v->Set( cell[0], cell[1], cell[2], BLOCK_AIR );
	SC_SpawnDrop( SC_CellCenter( cell ), BLOCK_TORCH, 1 );
}

void SC_BreakTorch( CBasePlayer *pPlayer, const int *cell )
{
	EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_ITEM, "debris/wood1.wav", 0.6f, ATTN_NORM, 0, 120 );
	SC_DropTorch( cell );
}

// sand and gravel with nothing under them fall, like Minecraft's: a block-sized entity that turns back into the
// block where it lands (or into an item if it lands somewhere a block can't go)
class CSCFallingBlock : public CBaseEntity
{
public:
	void Spawn( void );
	void EXPORT FallThink( void );
};

LINK_ENTITY_TO_CLASS( sc_fallingblock, CSCFallingBlock )

void CSCFallingBlock::Spawn( void )
{
	SET_MODEL( ENT( pev ), "models/svencraft/blockitem.mdl" );
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_NOT;
	pev->scale = 0.98f;
	UTIL_SetSize( pev, Vector( -18, -18, -20 ), Vector( 18, 18, 20 ));
	UTIL_SetOrigin( pev, pev->origin );
	pev->fuser2 = gpGlobals->time;
	SetThink( &CSCFallingBlock::FallThink );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CSCFallingBlock::FallThink( void )
{
	pev->nextthink = gpGlobals->time + 0.05f;
	if( !FBitSet( pev->flags, FL_ONGROUND ) && gpGlobals->time - pev->fuser2 < 10.0f )
		return;
	vox_api_t *v = SC_Vox();
	int cell[3] = { (int)floor( pev->origin.x / VOX_BLOCK_SIZE ), (int)floor( pev->origin.y / VOX_BLOCK_SIZE ), (int)floor( pev->origin.z / VOX_BLOCK_SIZE ) };
	int here = v ? v->Get( cell[0], cell[1], cell[2] ) : -1;
	if( here == BLOCK_AIR || SC_IsTorch( here ))
	{
		if( SC_IsTorch( here ))
			SC_SpawnDrop( SC_CellCenter( cell ), BLOCK_TORCH, 1 );	// crushed out of the way
		v->Set( cell[0], cell[1], cell[2], pev->skin );
		SC_CheckFall( cell );	// still nothing under it (it landed on a prop, say): keep going
	}
	else
		SC_SpawnDrop( pev->origin, pev->skin, 1 );
	EMIT_AMBIENT_SOUND( ENT( pev ), pev->origin, g_SCFamHit[FAM_EARTH], 0.8f, ATTN_NORM, 0, 90 );
	UTIL_Remove( this );
}

void SC_CheckFall( const int *cell )
{
	vox_api_t *v = SC_Vox();
	if( !v )
		return;
	int id = v->Get( cell[0], cell[1], cell[2] );
	if( id != BLOCK_SAND && id != BLOCK_GRAVEL )
		return;
	int below = v->Get( cell[0], cell[1], cell[2] - 1 );
	if( below != BLOCK_AIR && !SC_IsTorch( below ))
		return;
	// the map's own ground holds it up too
	Vector under = SC_CellCenter( cell ) - Vector( 0, 0, VOX_BLOCK_SIZE * 0.5f + 2 );
	if( UTIL_PointContents( under ) == CONTENTS_SOLID )
		return;
	v->Set( cell[0], cell[1], cell[2], BLOCK_AIR );
	CBaseEntity *pFall = CBaseEntity::Create( "sc_fallingblock", SC_CellCenter( cell ), g_vecZero, NULL );
	if( pFall )
		pFall->pev->skin = id;
	SC_BlockRemoved( cell );	// and whatever stood on it
}

void SC_BlockRemoved( const int *cell )
{
	vox_api_t *v = SC_Vox();
	if( !v )
		return;
	SC_FurnaceRemoved( cell, SC_CellCenter( cell ));	// if it was a furnace, what was in it spills out
	int above[3] = { cell[0], cell[1], cell[2] + 1 };
	SC_CheckFall( above );
	// the torch standing on it, and the ones leaning off its sides
	static const struct { int d[3]; int id; } held[] =
	{
		{ { 0, 0, 1 }, BLOCK_TORCH },
		{ { 1, 0, 0 }, BLOCK_WALL_TORCH_NX }, { { -1, 0, 0 }, BLOCK_WALL_TORCH_PX },
		{ { 0, 1, 0 }, BLOCK_WALL_TORCH_NY }, { { 0, -1, 0 }, BLOCK_WALL_TORCH_PY },
	};
	for( int i = 0; i < 5; i++ )
	{
		int c[3] = { cell[0] + held[i].d[0], cell[1] + held[i].d[1], cell[2] + held[i].d[2] };
		if( v->Get( c[0], c[1], c[2] ) == held[i].id )
			SC_DropTorch( c );
	}
}
