// Placeable / mineable block entities.

const string SC_BLOCK_MODEL = "models/svencraft/block.mdl";
const float SC_BLOCK_SIZE = 40.0;   // one block in world units: 2 blocks fit the 72-unit player, 1 block the crouched one
const int SC_MAX_BLOCKS = 1000;     // blocks + terrain pieces; clients see at most 1024 entities at once

int g_SCBlockCount = 0;
dictionary g_SCDepleted;            // world surface cells that have already been mined

// pev.iuser1 = material, pev.fuser1 = mining progress (0..1), pev.scale = model scale,
// pev.body = shape (cube/slab/plate/sheet), pev.vuser1 = collision half-extents (zero = cube of the model's size)
class sc_block : ScriptBaseEntity
{
	int ObjectCaps()
	{
		if( self.pev.iuser1 == MAT_WORKBENCH || self.pev.iuser1 == MAT_DOOR )
			return BaseClass.ObjectCaps() | FCAP_IMPULSE_USE;
		return BaseClass.ObjectCaps();
	}

	void Use( CBaseEntity@ pActivator, CBaseEntity@ pCaller, USE_TYPE useType, float flValue )
	{
		if( pActivator is null || !pActivator.IsPlayer() )
			return;
		if( self.pev.iuser1 == MAT_WORKBENCH )
			g_Scheduler.SetTimeout( "SC_OpenCraftingLater", 0.05, EHandle( pActivator ) );
		else if( self.pev.iuser1 == MAT_DOOR )
			SC_ToggleDoor( self );
	}

	void Spawn()
	{
		Precache();
		self.pev.solid = SOLID_BBOX;
		self.pev.movetype = MOVETYPE_NONE;
		self.pev.takedamage = DAMAGE_NO;
		if( self.pev.scale <= 0 )
			self.pev.scale = 1.0;
		if( self.pev.iuser1 == MAT_DOOR )
		{
			g_EntityFuncs.SetModel( self, SC_DOOR_MODEL );
			self.pev.skin = 0;
			self.pev.body = 0;
			self.pev.scale = 1.0;
			SC_DoorApply( self, true );
			g_EntityFuncs.SetOrigin( self, self.pev.origin );
			return;
		}
		g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );
		self.pev.skin = self.pev.iuser1;
		Vector vecHalf = self.pev.vuser1;
		if( vecHalf == g_vecZero )
		{
			float h = SC_BLOCK_SIZE * 0.5 * self.pev.scale;
			vecHalf = Vector( h, h, h );
		}
		g_EntityFuncs.SetSize( self.pev, -vecHalf, vecHalf );
		g_EntityFuncs.SetOrigin( self, self.pev.origin );
	}

	void Precache()
	{
		g_Game.PrecacheModel( SC_BLOCK_MODEL );
		g_Game.PrecacheModel( SC_DOOR_MODEL );
	}

	void StartSwing()
	{
		SetThink( ThinkFunction( this.DoorThink ) );
		self.pev.nextthink = g_Engine.time + 0.01;
	}

	// Door swing: turn toward the yaw of the current state, 18 degrees per tick.
	void DoorThink()
	{
		float flTarget = SC_DoorYaw( self );
		float d = flTarget - self.pev.angles.y;
		while( d > 180 ) d -= 360;
		while( d <= -180 ) d += 360;
		if( SC_Abs( d ) <= 18.0 )
		{
			self.pev.angles.y = flTarget;
			return;
		}
		self.pev.angles.y = self.pev.angles.y + ( d > 0 ? 18.0 : -18.0 );
		self.pev.nextthink = g_Engine.time + 0.02;
	}
}

//
// Doors: 2 blocks tall, hinged on the placer's left at the near edge of the cell, opening away from them.
// The model's origin is the hinge axis at floor level; the panel runs along local X (-3..37), Y -3..3, Z 0..80.
// pev.iuser2 = facing in quarter turns (0 = +x, 1 = +y, ...), pev.iuser3 = 1 when open.
//
const string SC_DOOR_MODEL = "models/svencraft/door.mdl";
const string SC_DOOR_OPEN_SOUND = "doors/doorstop2.wav";
const string SC_DOOR_CLOSE_SOUND = "doors/doorstop1.wav";
const float SC_DOOR_HALF_T = 3.0;
const float SC_DOOR_HEIGHT = 80.0;

int SC_DoorQuarter( CBaseEntity@ pDoor )
{
	return ( pDoor.pev.iuser2 + ( pDoor.pev.iuser3 != 0 ? 0 : 3 ) ) % 4;   // closed = facing - 90
}

float SC_DoorYaw( CBaseEntity@ pDoor )
{
	return SC_DoorQuarter( pDoor ) * 90.0;
}

// Axis-aligned collision box (relative to the hinge) for the door's current state.
void SC_DoorBox( CBaseEntity@ pDoor, Vector& out mn, Vector& out mx )
{
	int q = SC_DoorQuarter( pDoor );
	float c = q == 0 ? 1 : ( q == 2 ? -1 : 0 ), s = q == 1 ? 1 : ( q == 3 ? -1 : 0 );
	mn = Vector( 9999, 9999, 0 );
	mx = Vector( -9999, -9999, SC_DOOR_HEIGHT );
	for( int i = 0; i < 4; i++ )
	{
		float lx = ( i & 1 ) != 0 ? SC_BLOCK_SIZE - SC_DOOR_HALF_T : -SC_DOOR_HALF_T;
		float ly = ( i & 2 ) != 0 ? SC_DOOR_HALF_T : -SC_DOOR_HALF_T;
		float wx = lx * c - ly * s, wy = lx * s + ly * c;
		if( wx < mn.x ) mn.x = wx;
		if( wx > mx.x ) mx.x = wx;
		if( wy < mn.y ) mn.y = wy;
		if( wy > mx.y ) mx.y = wy;
	}
}

void SC_DoorApply( CBaseEntity@ pDoor, bool bSnap )
{
	Vector mn, mx;
	SC_DoorBox( pDoor, mn, mx );
	g_EntityFuncs.SetSize( pDoor.pev, mn, mx );
	if( bSnap )
		pDoor.pev.angles = Vector( 0, SC_DoorYaw( pDoor ), 0 );
}

bool SC_IsDoor( CBaseEntity@ pEnt )
{
	return SC_IsBlock( pEnt ) && pEnt.pev.iuser1 == MAT_DOOR;
}

// True when a player or monster overlaps the world-space box mn..mx.
bool SC_BoxHasCreature( const Vector& in mn, const Vector& in mx )
{
	Vector c = ( mn + mx ) * 0.5;
	CBaseEntity@ pOther = null;
	while( ( @pOther = g_EntityFuncs.FindEntityInSphere( pOther, c, 160, "*", "classname" ) ) !is null )
	{
		if( !( pOther.IsPlayer() || pOther.IsMonster() ) || !pOther.IsAlive() || pOther.pev.solid == SOLID_NOT )
			continue;
		Vector omn = pOther.pev.origin + pOther.pev.mins, omx = pOther.pev.origin + pOther.pev.maxs;
		if( mn.x < omx.x - 0.5 && mx.x > omn.x + 0.5 && mn.y < omx.y - 0.5 && mx.y > omn.y + 0.5 && mn.z < omx.z - 0.5 && mx.z > omn.z + 0.5 )
			return true;
	}
	return false;
}

void SC_ToggleDoor( CBaseEntity@ pDoor )
{
	pDoor.pev.iuser3 = pDoor.pev.iuser3 != 0 ? 0 : 1;
	Vector mn, mx;
	SC_DoorBox( pDoor, mn, mx );
	if( SC_BoxHasCreature( pDoor.pev.origin + mn, pDoor.pev.origin + mx ) )
	{
		pDoor.pev.iuser3 = pDoor.pev.iuser3 != 0 ? 0 : 1;   // someone is standing in the way
		g_SoundSystem.EmitSound( pDoor.edict(), CHAN_BODY, "common/wpn_denyselect.wav", 0.6, ATTN_NORM );
		return;
	}
	SC_DoorApply( pDoor, false );
	g_EntityFuncs.SetOrigin( pDoor, pDoor.pev.origin );   // relink with the new box
	sc_block@ pScript = cast<sc_block@>( CastToScriptClass( pDoor ) );
	if( pScript !is null )
		pScript.StartSwing();
	g_SoundSystem.EmitSoundDyn( pDoor.edict(), CHAN_BODY, pDoor.pev.iuser3 != 0 ? SC_DOOR_OPEN_SOUND : SC_DOOR_CLOSE_SOUND, 0.8, ATTN_NORM, 0, 95 + Math.RandomLong( 0, 10 ) );
}

void SC_PrecacheDoors()
{
	g_Game.PrecacheModel( SC_DOOR_MODEL );
	g_SoundSystem.PrecacheSound( SC_DOOR_OPEN_SOUND );
	g_SoundSystem.PrecacheSound( SC_DOOR_CLOSE_SOUND );
}

int SC_FacingQuarter( CBasePlayer@ pPlayer )
{
	float y = pPlayer.pev.v_angle.y;
	int q = int( SC_Floor( ( y + 45.0 ) / 90.0 ) ) % 4;
	return q < 0 ? q + 4 : q;
}

// Door on the floor under cell vecCell, facing where the player looks. False when there's no room.
bool SC_PlaceDoor( CBasePlayer@ pPlayer, const Vector& in vecCell )
{
	TraceResult tr;
	g_Utility.TraceLine( vecCell, vecCell - Vector( 0, 0, SC_BLOCK_SIZE * 1.5 ), ignore_monsters, null, tr );
	if( tr.flFraction >= 1.0 || tr.fStartSolid != 0 )
		return false;
	float z0 = tr.vecEndPos.z;
	float h = SC_BLOCK_SIZE * 0.5;
	if( !SC_BoxClear( Vector( vecCell.x, vecCell.y, z0 + h + 0.5 ), h - 0.5 ) || !SC_BoxClear( Vector( vecCell.x, vecCell.y, z0 + h * 3 + 0.5 ), h - 0.5 ) )
		return false;
	int q = SC_FacingQuarter( pPlayer );
	float c = q == 0 ? 1 : ( q == 2 ? -1 : 0 ), s = q == 1 ? 1 : ( q == 3 ? -1 : 0 );
	Vector vecFwd( c, s, 0 ), vecRight( s, -c, 0 );
	float flIn = h - SC_DOOR_HALF_T;
	Vector vecHinge = Vector( vecCell.x, vecCell.y, z0 ) - vecFwd * flIn - vecRight * flIn;
	CBaseEntity@ pEnt = g_EntityFuncs.Create( "sc_block", vecHinge, g_vecZero, true );
	if( pEnt is null )
		return false;
	pEnt.pev.iuser1 = MAT_DOOR;
	pEnt.pev.iuser2 = q;
	pEnt.pev.iuser3 = 0;
	g_EntityFuncs.DispatchSpawn( pEnt.edict() );
	g_SCBlockCount++;
	return true;
}

// Glass pane: a thin sheet in the middle of the cell, across the player's view.
bool SC_PlacePane( CBasePlayer@ pPlayer, const Vector& in vecCell )
{
	int q = SC_FacingQuarter( pPlayer );
	int iAxis = ( q % 2 == 0 ) ? 0 : 1;
	float h = SC_BLOCK_SIZE * 0.5, t = SC_SHAPE_THICKNESS[SC_SHAPE_SHEET] * 0.5;
	Vector vecHalf = iAxis == 0 ? Vector( t, h, h ) : Vector( h, t, h );
	return SC_SpawnShaped( vecCell, vecHalf, MAT_GLASS_PANE, SC_SHAPE_SHEET, iAxis, 1.0 ) !is null;
}

void SC_RegisterBlock()
{
	g_CustomEntityFuncs.RegisterCustomEntity( "sc_block", "sc_block" );
}

bool SC_IsBlock( CBaseEntity@ pEnt )
{
	return pEnt !is null && pEnt.GetClassname() == "sc_block";
}

float SC_Abs( float v ) { return v < 0 ? -v : v; }

float SC_Floor( float v )
{
	int i = int( v );
	if( float( i ) > v )
		i--;
	return float( i );
}

// Center of the 32-unit grid cell containing v (one axis).
float SC_Snap( float v )
{
	return SC_Floor( v / SC_BLOCK_SIZE ) * SC_BLOCK_SIZE + SC_BLOCK_SIZE * 0.5;
}

string SC_CellKey( const Vector& in v )
{
	return "" + int( SC_Floor( v.x / SC_BLOCK_SIZE ) ) + " " + int( SC_Floor( v.y / SC_BLOCK_SIZE ) ) + " " + int( SC_Floor( v.z / SC_BLOCK_SIZE ) );
}

CBaseEntity@ SC_SpawnBlock( const Vector& in vecCenter, int iMat, float flSize )
{
	CBaseEntity@ pEnt = g_EntityFuncs.Create( "sc_block", vecCenter, g_vecZero, true );
	if( pEnt is null )
		return null;
	pEnt.pev.iuser1 = iMat;
	pEnt.pev.scale = flSize / SC_BLOCK_SIZE;
	g_EntityFuncs.DispatchSpawn( pEnt.edict() );
	g_SCBlockCount++;
	return pEnt;
}

// True when a cube of half-size flHalf at vecCenter overlaps nothing solid
// (world, brush entities, other blocks, players, monsters).
bool SC_BoxClear( const Vector& in vecCenter, float flHalf )
{
	if( g_EngineFuncs.PointContents( vecCenter ) == CONTENTS_SOLID )
		return false;

	// Players/monsters: compare against their movement boxes directly.
	Vector bmn = vecCenter - Vector( flHalf, flHalf, flHalf ), bmx = vecCenter + Vector( flHalf, flHalf, flHalf );
	CBaseEntity@ pOther = null;
	while( ( @pOther = g_EntityFuncs.FindEntityInSphere( pOther, vecCenter, flHalf + 128, "*", "classname" ) ) !is null )
	{
		if( !( pOther.IsPlayer() || pOther.IsMonster() ) || !pOther.IsAlive() || pOther.pev.solid == SOLID_NOT )
			continue;
		Vector omn = pOther.pev.origin + pOther.pev.mins, omx = pOther.pev.origin + pOther.pev.maxs;
		if( bmn.x < omx.x - 0.5 && bmx.x > omn.x + 0.5 && bmn.y < omx.y - 0.5 && bmx.y > omn.y + 0.5 && bmn.z < omx.z - 0.5 && bmx.z > omn.z + 0.5 )
			return false;
	}

	float h = flHalf - 1.0;
	array<Vector> c( 8 );
	for( int i = 0; i < 8; i++ )
		c[i] = vecCenter + Vector( ( i & 1 ) != 0 ? h : -h, ( i & 2 ) != 0 ? h : -h, ( i & 4 ) != 0 ? h : -h );

	// 12 edges + 4 space diagonals
	const array<int> pairs = { 0,1, 2,3, 4,5, 6,7, 0,2, 1,3, 4,6, 5,7, 0,4, 1,5, 2,6, 3,7, 0,7, 1,6, 2,5, 3,4 };
	TraceResult tr;
	for( uint i = 0; i < pairs.length(); i += 2 )
	{
		g_Utility.TraceLine( c[pairs[i]], c[pairs[i + 1]], dont_ignore_monsters, null, tr );
		if( tr.fStartSolid != 0 || tr.fAllSolid != 0 || tr.flFraction < 1.0 )
			return false;
	}
	return true;
}

void SC_Debris( const Vector& in vecPos, float flSize, int iFamily, int iCount )
{
	NetworkMessage m( MSG_PVS, NetworkMessages::SVC_TEMPENTITY, vecPos );
		m.WriteByte( TE_BREAKMODEL );
		m.WriteCoord( vecPos.x );
		m.WriteCoord( vecPos.y );
		m.WriteCoord( vecPos.z );
		m.WriteCoord( flSize );
		m.WriteCoord( flSize );
		m.WriteCoord( flSize );
		m.WriteCoord( 0 );
		m.WriteCoord( 0 );
		m.WriteCoord( 0 );
		m.WriteByte( 10 );    // random velocity
		m.WriteShort( g_SCFamGibIndex[iFamily] );
		m.WriteByte( iCount );
		m.WriteByte( 20 );    // life, 0.1s units
		m.WriteByte( SC_FAM_BREAKFLAGS[iFamily] );
	m.End();
}

void SC_HitSound( CBasePlayer@ pPlayer, int iFamily )
{
	g_SoundSystem.EmitSoundDyn( pPlayer.edict(), CHAN_WEAPON, SC_FAM_HIT[iFamily], 1.0, ATTN_NORM, 0, 95 + Math.RandomLong( 0, 10 ) );
}

void SC_BreakSound( const Vector& in vecPos, int iFamily )
{
	g_SoundSystem.PlaySound( g_EntityFuncs.IndexEnt( 0 ), CHAN_STATIC, SC_FAM_BREAK[iFamily], 1.0, ATTN_NORM, 0, 95 + Math.RandomLong( 0, 10 ), 0, true, vecPos );
}

void SC_GiveDrop( CBasePlayer@ pPlayer, int iTool, int iMat, float flAmount, const Vector& in vecPos )
{
	SCMaterial@ m = g_SCMats[iMat];
	if( !SC_ToolCanHarvest( pPlayer, iTool, iMat ) )
	{
		SC_Print( pPlayer, SC_HarvestHint( iMat ) );
		g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/wpn_denyselect.wav", 1, ATTN_NORM );
		return;
	}
	SC_SpawnDrops( vecPos, m.drop, flAmount );
}

string SC_ToolName( int iTool )
{
	if( iTool == TOOL_PICKAXE ) return "pickaxe";
	if( iTool == TOOL_SHOVEL ) return "shovel";
	if( iTool == TOOL_AXE ) return "axe";
	return "hand";
}

// One swing of a tool against a block entity.
void SC_HitBlock( CBasePlayer@ pPlayer, CBaseEntity@ pBlock, int iTool, const Vector& in vecNormal )
{
	int iMat = pBlock.pev.iuser1;
	SCMaterial@ m = g_SCMats[iMat];
	Vector vecBox = pBlock.pev.maxs - pBlock.pev.mins;
	float flSize = vecBox.x > vecBox.y ? vecBox.x : vecBox.y;
	if( vecBox.z > flSize ) flSize = vecBox.z;

	// Thin pieces (slabs, plates) break proportionally faster than a full block.
	float flVol = vecBox.x * vecBox.y * vecBox.z / ( SC_BLOCK_SIZE * SC_BLOCK_SIZE * SC_BLOCK_SIZE );
	pBlock.pev.fuser1 += SC_SwingPower( pPlayer, iTool, iMat ) / ( flVol < 0.25 ? 0.25 : ( flVol > 1 ? 1 : flVol ) );
	SC_HitSound( pPlayer, m.family );

	if( pBlock.pev.fuser1 < 0.999 )
	{
		SC_Debris( pBlock.pev.origin, flSize, m.family, 2 );
		Vector vecFace;
		float flFace;
		SC_BoxFace( pBlock.pev.origin + pBlock.pev.mins, pBlock.pev.origin + pBlock.pev.maxs, vecNormal, vecFace, flFace );
		SC_ShowCrack( pPlayer, vecFace, SC_AxisNormal( vecNormal ), flFace, pBlock.pev.fuser1 );
		return;
	}
	SC_HideCrack( pPlayer );

	Vector vecPos = pBlock.pev.origin;
	g_EntityFuncs.Remove( pBlock );
	g_SCBlockCount--;
	SC_CheckFall( SC_Cell( vecPos.x ), SC_Cell( vecPos.y ), SC_Cell( vecPos.z ) + 1 );

	SC_Debris( vecPos, flSize, m.family, 8 );
	SC_BreakSound( vecPos, m.family );
	// Every piece gives one whole block, however thin (Minecraft rule: one block mined = one item).
	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecPos );
}

// One swing at something that can't be removed (world, big brush entities, terrain-sized props):
// each 32-unit cell of it yields one block of iMat and is then used up.
void SC_HarvestCell( CBasePlayer@ pPlayer, int iTool, int iMat, const string& in szKey, const Vector& in vecPos, const Vector& in vecNormal )
{
	SCMaterial@ m = g_SCMats[iMat];
	SC_HitSound( pPlayer, m.family );
	if( g_SCDepleted.exists( szKey ) )
	{
		SC_Print( pPlayer, "Nothing left to dig here" );
		return;
	}

	SCInventory@ inv = SC_Inv( pPlayer );
	if( inv.mineKey != szKey )
	{
		inv.mineKey = szKey;
		inv.mineProgress = 0;
	}
	inv.mineProgress += SC_SwingPower( pPlayer, iTool, iMat );
	if( inv.mineProgress < 0.999 )
	{
		SC_Debris( vecPos, 8, m.family, 2 );
		SC_ShowCrack( pPlayer, vecPos, vecNormal, SC_BLOCK_SIZE, inv.mineProgress );
		return;
	}
	SC_HideCrack( pPlayer );

	inv.mineKey = "";
	inv.mineProgress = 0;
	g_SCDepleted.set( szKey, true );
	SC_Debris( vecPos, 16, m.family, 8 );
	SC_BreakSound( vecPos, m.family );
	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecPos );
}

void SC_HitSurface( CBasePlayer@ pPlayer, int iTool, TraceResult& in tr, const Vector& in vecSrc, const Vector& in vecEnd )
{
	int iMat = SC_MatForTexture( g_Utility.TraceTexture( tr.pHit, vecSrc, vecEnd ) );
	if( iMat == MAT_NONE )
		return;
	g_WeaponFuncs.DecalGunshot( tr, BULLET_PLAYER_CROWBAR );
	Vector vecInside = tr.vecEndPos - tr.vecPlaneNormal * 2.0;
	SC_HarvestCell( pPlayer, iTool, iMat, "" + g_EntityFuncs.Instance( tr.pHit ).entindex() + ":" + SC_CellKey( vecInside ), tr.vecEndPos, tr.vecPlaneNormal );
}

// Place the player's selected block against whatever they're looking at.
void SC_TryPlace( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	int iMat = inv.selected;

	Math.MakeVectors( pPlayer.pev.v_angle );
	Vector vecSrc = pPlayer.GetGunPosition();
	Vector vecEnd = vecSrc + g_Engine.v_forward * SC_REACH;
	TraceResult tr;
	g_Utility.TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pPlayer.edict(), tr );
	CBaseEntity@ pHit = tr.flFraction < 1.0 ? g_EntityFuncs.Instance( tr.pHit ) : null;
	if( SC_IsDoor( pHit ) )
	{
		SC_ToggleDoor( pHit );
		return;
	}
	bool bCrouch = ( pPlayer.pev.button & IN_DUCK ) != 0;
	if( !bCrouch && pHit !is null && pHit.pev.iuser1 == MAT_WORKBENCH && ( SC_IsBlock( pHit ) || SC_IsNode( pHit ) ) )
	{
		g_Scheduler.SetTimeout( "SC_OpenCraftingLater", 0.05, EHandle( pPlayer ) );
		return;
	}

	if( !inv.Has( iMat ) )
	{
		SC_CycleBlock( pPlayer, 1 );
		return;
	}
	if( g_SCBlockCount >= SC_MAX_BLOCKS )
	{
		SC_Print( pPlayer, "Block limit reached (" + SC_MAX_BLOCKS + ")" );
		return;
	}
	if( tr.flFraction >= 1.0 )
		return;

	Vector n = tr.vecPlaneNormal;
	int ax = 0;
	if( SC_Abs( n.y ) > SC_Abs( n[ax] ) ) ax = 1;
	if( SC_Abs( n.z ) > SC_Abs( n[ax] ) ) ax = 2;
	float sgn = n[ax] >= 0 ? 1.0 : -1.0;
	float h = SC_BLOCK_SIZE * 0.5;

	array<Vector> candidates;
	Vector c;
	if( SC_IsNode( pHit ) )
	{
		// Terrain is grid aligned: the target is simply the cell in front of the face.
		Vector t = tr.vecEndPos + n * h;
		candidates.insertLast( Vector( SC_Snap( t.x ), SC_Snap( t.y ), SC_Snap( t.z ) ) );
	}
	else if( SC_IsBlock( pHit ) && pHit.pev.scale == 1.0 )
	{
		// Stack exactly on the face of a full block.
		c = pHit.pev.origin;
		c[ax] = c[ax] + sgn * SC_BLOCK_SIZE;
		candidates.insertLast( c );
	}
	else
	{
		// Flush against the surface along its normal; snapped to the grid along the surface,
		// falling back to centered on the crosshair when the grid cell is obstructed.
		Vector snapped, centered;
		for( int i = 0; i < 3; i++ )
		{
			float flSurface = SC_Floor( tr.vecEndPos[i] + 0.5 );
			snapped[i] = ( i == ax ) ? flSurface + sgn * h : SC_Snap( tr.vecEndPos[i] );
			centered[i] = ( i == ax ) ? flSurface + sgn * h : tr.vecEndPos[i];
		}
		if( SC_IsBlock( pHit ) )
			snapped[ax] = pHit.pev.origin[ax] + sgn * ( SC_BLOCK_SIZE * 0.5 * pHit.pev.scale + h );
		centered[ax] = snapped[ax];
		candidates.insertLast( snapped );
		candidates.insertLast( centered );
	}

	for( uint i = 0; i < candidates.length(); i++ )
	{
		if( !SC_BoxClear( candidates[i], h ) )
			continue;
		if( iMat == MAT_DOOR || iMat == MAT_GLASS_PANE )
		{
			if( !( iMat == MAT_DOOR ? SC_PlaceDoor( pPlayer, candidates[i] ) : SC_PlacePane( pPlayer, candidates[i] ) ) )
				continue;
		}
		else if( SC_Falls( iMat ) && SC_BoxClear( candidates[i] - Vector( 0, 0, SC_BLOCK_SIZE ), h - 0.5 ) )
			SC_StartFalling( candidates[i], iMat );       // nothing underneath: it drops
		else if( !SC_PlaceBlockAt( candidates[i], iMat ) )
			return;
		inv.Take( iMat );
		SC_HitSound( pPlayer, g_SCMats[iMat].family );
		SC_UpdateHud( pPlayer );
		return;
	}
	SC_Print( pPlayer, iMat == MAT_DOOR ? "No room for a door there (needs 2 free blocks on a floor)" : "No room for a block there" );
}

// Grid-aligned blocks on template maps become brush blocks (same look and lighting as the terrain);
// everywhere else they are studio-model blocks.
bool SC_PlaceBlockAt( const Vector& in vecCenter, int iMat )
{
	bool bAligned = SC_Abs( SC_Snap( vecCenter.x ) - vecCenter.x ) < 0.01
		&& SC_Abs( SC_Snap( vecCenter.y ) - vecCenter.y ) < 0.01
		&& SC_Abs( SC_Snap( vecCenter.z ) - vecCenter.z ) < 0.01;
	if( g_SCSandbox && bAligned && SC_HasTemplate( iMat, 1, 1, 1 ) )
	{
		int iBefore = g_SCBlockCount;
		SC_ForgetCave( SC_Cell( vecCenter.x ), SC_Cell( vecCenter.y ), SC_Cell( vecCenter.z ) );
		SC_SpawnNode( iMat, SC_Cell( vecCenter.x ), SC_Cell( vecCenter.y ), SC_Cell( vecCenter.z ), 1, 1, 1, false, true );
		return g_SCBlockCount > iBefore;
	}
	return SC_SpawnBlock( vecCenter, iMat, SC_BLOCK_SIZE ) !is null;
}

void SC_ClearBlocks()
{
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "sc_block" ) ) !is null )
		g_EntityFuncs.Remove( pEnt );
	g_SCBlockCount = 0;
}
