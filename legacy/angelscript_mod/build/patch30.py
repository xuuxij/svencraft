# Doors and glass panes: materials, placement, use/toggle, drops, recipes.
import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"


def patch(name, pairs):
    p = os.path.join(D, name)
    s = open(p, newline='', encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (name, old[:80], s.count(old))
        s = s.replace(old, new)
    open(p, 'w', newline='', encoding='utf-8').write(s)
    print('patched', name)


patch('sc_materials.as', [
    ("\tMAT_CRYSTAL_ORE,\n\tMAT_COUNT", "\tMAT_CRYSTAL_ORE,\n\tMAT_GLASS_PANE,\n\tMAT_DOOR,\n\tMAT_COUNT"),
    ('''	g_SCMats.insertLast( SCMaterial( "crystal_ore", "Xen Crystal Ore", 4.0, TOOL_PICKAXE, 3, MAT_CIRCUIT, FAM_GLASS ) );''',
     '''	g_SCMats.insertLast( SCMaterial( "crystal_ore", "Xen Crystal Ore", 4.0, TOOL_PICKAXE, 3, MAT_CIRCUIT, FAM_GLASS ) );
	g_SCMats.insertLast( SCMaterial( "glass_pane", "Glass Pane", 0.3, TOOL_HAND,    0,     MAT_GLASS_PANE, FAM_GLASS ) );
	g_SCMats.insertLast( SCMaterial( "door",     "Wooden Door", 3.0, TOOL_AXE,     0,     MAT_DOOR,     FAM_WOOD ) );'''),
])

patch('sc_blocks.as', [
    # --- class: doors use their own model and open/close with +use
    ('''	int ObjectCaps()
	{
		if( self.pev.iuser1 == MAT_WORKBENCH )
			return BaseClass.ObjectCaps() | FCAP_IMPULSE_USE;
		return BaseClass.ObjectCaps();
	}

	void Use( CBaseEntity@ pActivator, CBaseEntity@ pCaller, USE_TYPE useType, float flValue )
	{
		if( self.pev.iuser1 == MAT_WORKBENCH && pActivator !is null && pActivator.IsPlayer() )
			g_Scheduler.SetTimeout( "SC_OpenCraftingLater", 0.05, EHandle( pActivator ) );
	}

	void Spawn()
	{
		Precache();
		self.pev.solid = SOLID_BBOX;
		self.pev.movetype = MOVETYPE_NONE;
		self.pev.takedamage = DAMAGE_NO;
		if( self.pev.scale <= 0 )
			self.pev.scale = 1.0;
		g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );''',
     '''	int ObjectCaps()
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
		g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );'''),
    ('''	void Precache()
	{
		g_Game.PrecacheModel( SC_BLOCK_MODEL );
	}
}''', '''	void Precache()
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
}'''),
    # --- right-click: doors toggle, workbenches open (crouch to place against them), then placement
    ('''	SCInventory@ inv = SC_Inv( pPlayer );
	int iMat = inv.selected;
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

	Math.MakeVectors( pPlayer.pev.v_angle );
	Vector vecSrc = pPlayer.GetGunPosition();
	Vector vecEnd = vecSrc + g_Engine.v_forward * SC_REACH;
	TraceResult tr;
	g_Utility.TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pPlayer.edict(), tr );
	if( tr.flFraction >= 1.0 )
		return;

	CBaseEntity@ pHit = g_EntityFuncs.Instance( tr.pHit );''',
     '''	SCInventory@ inv = SC_Inv( pPlayer );
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
'''),
    ('''		if( !SC_BoxClear( candidates[i], h ) )
			continue;
		if( SC_Falls( iMat ) && SC_BoxClear( candidates[i] - Vector( 0, 0, SC_BLOCK_SIZE ), h - 0.5 ) )''',
     '''		if( !SC_BoxClear( candidates[i], h ) )
			continue;
		if( iMat == MAT_DOOR || iMat == MAT_GLASS_PANE )
		{
			if( !( iMat == MAT_DOOR ? SC_PlaceDoor( pPlayer, candidates[i] ) : SC_PlacePane( pPlayer, candidates[i] ) ) )
				continue;
		}
		else if( SC_Falls( iMat ) && SC_BoxClear( candidates[i] - Vector( 0, 0, SC_BLOCK_SIZE ), h - 0.5 ) )'''),
    ('''	SC_Print( pPlayer, "No room for a block there" );''',
     '''	SC_Print( pPlayer, iMat == MAT_DOOR ? "No room for a door there (needs 2 free blocks on a floor)" : "No room for a block there" );'''),
])

patch('sc_drops.as', [
    ('''		g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );
		self.pev.skin = self.pev.iuser1;
		self.pev.body = 0;
		self.pev.scale = SC_DROP_SCALE;''',
     '''		if( self.pev.iuser1 == MAT_DOOR )
		{
			g_EntityFuncs.SetModel( self, SC_DOOR_MODEL );
			self.pev.body = 1;            // centred item version of the door
			self.pev.scale = 0.3;
		}
		else
		{
			g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );
			self.pev.skin = self.pev.iuser1;
			self.pev.body = self.pev.iuser1 == MAT_GLASS_PANE ? SC_SHAPE_SHEET : 0;
			if( self.pev.iuser1 == MAT_GLASS_PANE )
				self.pev.angles.x = 90;    // stand the pane up
			self.pev.scale = SC_DROP_SCALE;
		}'''),
    ('''		g_Game.PrecacheModel( SC_BLOCK_MODEL );
	}

	void DropThink()''', '''		g_Game.PrecacheModel( SC_BLOCK_MODEL );
		g_Game.PrecacheModel( SC_DOOR_MODEL );
	}

	void DropThink()'''),
])

patch('sc_crafting.as', [
    ('''	SC_BlockRecipe( CRAFT_BLOCKS, true, "metal:1,glass:1", MAT_CIRCUIT, 1 );''',
     '''	SC_BlockRecipe( CRAFT_BLOCKS, true, "metal:1,glass:1", MAT_CIRCUIT, 1 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "glass:6", MAT_GLASS_PANE, 16 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "planks:6", MAT_DOOR, 3 );'''),
])

patch('svencraft.as', [
    ('''	g_Game.PrecacheModel( SC_BLOCK_MODEL );
	SC_PrecacheMaterials();''', '''	g_Game.PrecacheModel( SC_BLOCK_MODEL );
	SC_PrecacheDoors();
	SC_PrecacheMaterials();'''),
])
