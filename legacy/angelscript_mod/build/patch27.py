import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='', encoding='utf-8').read()
    for a, b in reps:
        assert a in s, (name, a[:90])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='', encoding='utf-8').write(s)

# ---------------------------------------------------------------- ores as materials
patch('sc_materials.as', [
    ("	MAT_WORKBENCH,\n	MAT_COUNT", "	MAT_WORKBENCH,\n	MAT_IRON_ORE,\n	MAT_CRYSTAL_ORE,\n	MAT_COUNT"),
    ('''	g_SCMats.insertLast( SCMaterial( "workbench", "Workbench",  2.5, TOOL_AXE,     0,     MAT_WORKBENCH, FAM_WOOD ) );''',
     '''	g_SCMats.insertLast( SCMaterial( "workbench", "Workbench",  2.5, TOOL_AXE,     0,     MAT_WORKBENCH, FAM_WOOD ) );
	g_SCMats.insertLast( SCMaterial( "iron_ore", "Iron Ore",    3.0, TOOL_PICKAXE, 2,     MAT_METAL,    FAM_STONE ) );
	g_SCMats.insertLast( SCMaterial( "crystal_ore", "Xen Crystal Ore", 4.0, TOOL_PICKAXE, 3, MAT_CIRCUIT, FAM_GLASS ) );'''),
])

# ---------------------------------------------------------------- craft icon ids follow the material count
patch('sc_crafting.as', [
    ("const int SC_ICON_EMPTY = 44;\nconst int SC_ICON_SELECTED = 45;",
     "// craftslots.spr icons: blocks 0..MAT_COUNT-1, then 9 tools, 8 weapons, 8 supplies, then the empty slot.\nconst int SC_ICON_TOOLS = MAT_COUNT;\nconst int SC_ICON_WEAPONS = MAT_COUNT + 9;\nconst int SC_ICON_SUPPLIES = MAT_COUNT + 17;\nconst int SC_ICON_EMPTY = MAT_COUNT + 25;\nconst int SC_ICON_SELECTED = MAT_COUNT + 26;   // selected-slot variants start here"),
    ("			r.icon = 19 + ( r.tool - TOOL_PICKAXE ) * 3 + ( r.tier - 1 );", "			r.icon = SC_ICON_TOOLS + ( r.tool - TOOL_PICKAXE ) * 3 + ( r.tier - 1 );"),
    ("			r.icon = 28 + iWeapon++;", "			r.icon = SC_ICON_WEAPONS + iWeapon++;"),
    ("			r.icon = 36 + iSupply++;", "			r.icon = SC_ICON_SUPPLIES + iSupply++;"),
])

# ---------------------------------------------------------------- lazy ore veins + falling sand/gravel (sandbox)
patch('sc_sandbox.as', [
    # SC_SpawnNode: stone pieces made by digging are split until ore cells become their own blocks
    ("""void SC_SpawnNode( int iMat, int x, int y, int z, int sx, int sy, int sz )
{
	string szModel;""", """void SC_SpawnNode( int iMat, int x, int y, int z, int sx, int sy, int sz, bool bRaw = false )
{
	// Ores are revealed lazily: the initial stone chunks are plain stone (bRaw), but stone pieces
	// created by digging are split down until each ore cell becomes its own ore block.
	if( !bRaw && iMat == MAT_STONE && g_SCOreCount > 0 )
	{
		if( sx == 1 && sy == 1 && sz == 1 )
		{
			int iOre = SC_OreAt( x, y, z );
			if( iOre != MAT_NONE )
				iMat = iOre;
		}
		else if( SC_BoxHasOre( x, y, z, sx, sy, sz ) )
		{
			int hx = sx > 1 ? sx / 2 : 1, hy = sy > 1 ? sy / 2 : 1, hz = sz > 1 ? sz / 2 : 1;
			for( int i = 0; i < sx; i += hx )
				for( int j = 0; j < sy; j += hy )
					for( int k = 0; k < sz; k += hz )
						SC_SpawnNode( iMat, x + i, y + j, z + k, hx, hy, hz );
			return;
		}
	}
	string szModel;"""),
    ("""			SC_SpawnNode( MAT_STONE, x, y, -11, SC_CHUNK, SC_CHUNK, 8 );
		}
	}""", """			SC_SpawnNode( MAT_STONE, x, y, -11, SC_CHUNK, SC_CHUNK, 8, true );
		}
	}
	SC_GenerateOres();"""),
    # mining a hidden ore cell inside a plain stone chunk gives the ore
    ("""	int iMat = pNode.pev.iuser1;
	SCMaterial@ m = g_SCMats[iMat];
	SC_HitSound( pPlayer, m.family );

	Vector vecInside = tr.vecEndPos - tr.vecPlaneNormal * 2.0;
	int cx = SC_Cell( vecInside.x ), cy = SC_Cell( vecInside.y ), cz = SC_Cell( vecInside.z );""",
     """	int iMat = pNode.pev.iuser1;
	Vector vecInside = tr.vecEndPos - tr.vecPlaneNormal * 2.0;
	int cx = SC_Cell( vecInside.x ), cy = SC_Cell( vecInside.y ), cz = SC_Cell( vecInside.z );
	if( iMat == MAT_STONE )
	{
		int iOre = SC_OreAt( cx, cy, cz );
		if( iOre != MAT_NONE )
			iMat = iOre;            // a still-hidden ore cell
	}
	SCMaterial@ m = g_SCMats[iMat];
	SC_HitSound( pPlayer, m.family );"""),
    ("""	SC_Debris( vecCell, SC_BLOCK_SIZE, m.family, 8 );
	SC_BreakSound( vecCell, m.family );
	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecCell );""",
     """	SC_Debris( vecCell, SC_BLOCK_SIZE, m.family, 8 );
	SC_BreakSound( vecCell, m.family );
	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecCell );
	SC_ForgetOre( cx, cy, cz );
	SC_CheckFall( cx, cy, cz + 1 );"""),
    ("""	g_SCBlockCount = 0;
	g_SCCellProgress.deleteAll();
	if( g_SCSandbox )""", """	g_SCBlockCount = 0;
	g_SCCellProgress.deleteAll();
	SC_ClearOres();
	if( g_SCSandbox )"""),
])

p = os.path.join(D, 'sc_sandbox.as')
s = open(p, newline='', encoding='utf-8').read()
s += '''
//
// Ore veins (sandbox): generated with the terrain, stored as cells, revealed when digging reaches them.
//
dictionary g_SCOres;      // "x y z" -> material
array<int> g_SCOreCells;  // flat x,y,z list for box tests
int g_SCOreCount = 0;

void SC_ClearOres()
{
	g_SCOres.deleteAll();
	g_SCOreCells.resize( 0 );
	g_SCOreCount = 0;
}

void SC_AddOre( int x, int y, int z, int iMat )
{
	string szKey = "" + x + " " + y + " " + z;
	if( g_SCOres.exists( szKey ) )
		return;
	g_SCOres.set( szKey, int64( iMat ) );
	g_SCOreCells.insertLast( x ); g_SCOreCells.insertLast( y ); g_SCOreCells.insertLast( z );
	g_SCOreCount++;
}

int SC_OreAt( int x, int y, int z )
{
	int64 iMat = MAT_NONE;
	if( !g_SCOres.get( "" + x + " " + y + " " + z, iMat ) )
		return MAT_NONE;
	return int( iMat );
}

void SC_ForgetOre( int x, int y, int z )
{
	g_SCOres.delete( "" + x + " " + y + " " + z );    // mined: the cell is gone (the box list may keep it; harmless)
}

bool SC_BoxHasOre( int x, int y, int z, int sx, int sy, int sz )
{
	for( uint i = 0; i + 2 < g_SCOreCells.length(); i += 3 )
	{
		int ox = g_SCOreCells[i], oy = g_SCOreCells[i + 1], oz = g_SCOreCells[i + 2];
		if( ox >= x && ox < x + sx && oy >= y && oy < y + sy && oz >= z && oz < z + sz && SC_OreAt( ox, oy, oz ) != MAT_NONE )
			return true;
	}
	return false;
}

// Random-walk veins: iron through the stone layer, rarer crystal near the bedrock.
void SC_OreVein( int iMat, int zMin, int zMax, int iLength )
{
	int x = Math.RandomLong( -SC_TERRAIN_HALF + 1, SC_TERRAIN_HALF - 2 );
	int y = Math.RandomLong( -SC_TERRAIN_HALF + 1, SC_TERRAIN_HALF - 2 );
	int z = Math.RandomLong( zMin, zMax );
	for( int i = 0; i < iLength; i++ )
	{
		SC_AddOre( x, y, z, iMat );
		int a = Math.RandomLong( 0, 2 ), d = Math.RandomLong( 0, 1 ) * 2 - 1;
		if( a == 0 ) x += d; else if( a == 1 ) y += d; else z += d;
		if( x < -SC_TERRAIN_HALF ) x = -SC_TERRAIN_HALF; if( x > SC_TERRAIN_HALF - 1 ) x = SC_TERRAIN_HALF - 1;
		if( y < -SC_TERRAIN_HALF ) y = -SC_TERRAIN_HALF; if( y > SC_TERRAIN_HALF - 1 ) y = SC_TERRAIN_HALF - 1;
		if( z < zMin ) z = zMin; if( z > zMax ) z = zMax;
	}
}

void SC_GenerateOres()
{
	SC_ClearOres();
	for( int v = 0; v < 14; v++ )
		SC_OreVein( MAT_IRON_ORE, -10, -4, Math.RandomLong( 3, 6 ) );
	for( int v = 0; v < 5; v++ )
		SC_OreVein( MAT_CRYSTAL_ORE, -11, -9, Math.RandomLong( 2, 3 ) );
}

//
// Falling sand and gravel
//
// The block entity (node or studio block) that contains point v, if any.
CBaseEntity@ SC_BlockAt( const Vector& in v )
{
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityInSphere( pEnt, v, 400, "*", "classname" ) ) !is null )
	{
		string szClass = pEnt.GetClassname();
		if( szClass != "sc_node" && szClass != "sc_block" )
			continue;
		Vector mn = pEnt.pev.origin + pEnt.pev.mins, mx = pEnt.pev.origin + pEnt.pev.maxs;
		if( v.x > mn.x && v.x < mx.x && v.y > mn.y && v.y < mx.y && v.z > mn.z && v.z < mx.z )
			return pEnt;
	}
	return null;
}

bool SC_Falls( int iMat )
{
	return iMat == MAT_SAND || iMat == MAT_GRAVEL;
}

// Cell (cx, cy, cz) just lost its support: if it's sand/gravel it starts falling (and so does anything above it).
void SC_CheckFall( int cx, int cy, int cz )
{
	for( int guard = 0; guard < 32; guard++, cz++ )
	{
		Vector v( ( cx + 0.5 ) * SC_BLOCK_SIZE, ( cy + 0.5 ) * SC_BLOCK_SIZE, ( cz + 0.5 ) * SC_BLOCK_SIZE );
		CBaseEntity@ pEnt = SC_BlockAt( v );
		if( pEnt is null || !SC_Falls( pEnt.pev.iuser1 ) )
			return;
		int iMat = pEnt.pev.iuser1;
		if( pEnt.GetClassname() == "sc_node" )
		{
			if( !SC_CarveNode( pEnt, cx, cy, cz ) )
				return;
		}
		else
		{
			g_EntityFuncs.Remove( pEnt );
			g_SCBlockCount--;
		}
		SC_StartFalling( v, iMat );
	}
}

class sc_falling : ScriptBaseEntity
{
	void Spawn()
	{
		g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );
		self.pev.skin = self.pev.iuser1;
		self.pev.movetype = MOVETYPE_TOSS;
		self.pev.solid = SOLID_NOT;
		g_EntityFuncs.SetSize( self.pev, Vector( -19, -19, -20 ), Vector( 19, 19, 20 ) );
		g_EntityFuncs.SetOrigin( self, self.pev.origin );
		self.pev.fuser2 = g_Engine.time;
		SetThink( ThinkFunction( this.FallThink ) );
		self.pev.nextthink = g_Engine.time + 0.1;
	}

	void FallThink()
	{
		self.pev.nextthink = g_Engine.time + 0.05;
		float flAge = g_Engine.time - self.pev.fuser2;
		bool bLanded = ( self.pev.flags & FL_ONGROUND ) != 0 || ( flAge > 0.3 && self.pev.velocity.Length() < 1 );
		if( !bLanded && flAge < 12 )
			return;
		int iMat = self.pev.iuser1;
		Vector v( ( SC_Cell( self.pev.origin.x ) + 0.5 ) * SC_BLOCK_SIZE, ( SC_Cell( self.pev.origin.y ) + 0.5 ) * SC_BLOCK_SIZE, ( SC_Cell( self.pev.origin.z ) + 0.5 ) * SC_BLOCK_SIZE );
		g_EntityFuncs.Remove( self );
		if( flAge < 12 && SC_BoxClear( v, SC_BLOCK_SIZE * 0.5 - 0.5 ) && SC_PlaceBlockAt( v, iMat ) )
			return;
		SC_SpawnDrops( self.pev.origin, iMat, 1 );   // no room (or fell out of the world): drop as an item
	}
}

void SC_StartFalling( const Vector& in v, int iMat )
{
	CBaseEntity@ pEnt = g_EntityFuncs.Create( "sc_falling", v, g_vecZero, true );
	if( pEnt is null )
		return;
	pEnt.pev.iuser1 = iMat;
	g_EntityFuncs.DispatchSpawn( pEnt.edict() );
}

void SC_RegisterFalling()
{
	g_CustomEntityFuncs.RegisterCustomEntity( "sc_falling", "sc_falling" );
}
'''
open(p, 'w', newline='', encoding='utf-8').write(s)

# placing sand/gravel over air makes it fall; breaking a studio block may drop the sand above it
patch('sc_blocks.as', [
    ("""		if( !SC_PlaceBlockAt( candidates[i], iMat ) )
			return;
		inv.Take( iMat );""", """		if( SC_Falls( iMat ) && SC_BoxClear( candidates[i] - Vector( 0, 0, SC_BLOCK_SIZE ), h - 0.5 ) )
			SC_StartFalling( candidates[i], iMat );       // nothing underneath: it drops
		else if( !SC_PlaceBlockAt( candidates[i], iMat ) )
			return;
		inv.Take( iMat );"""),
    ("""	Vector vecPos = pBlock.pev.origin;
	g_EntityFuncs.Remove( pBlock );
	g_SCBlockCount--;
""", """	Vector vecPos = pBlock.pev.origin;
	g_EntityFuncs.Remove( pBlock );
	g_SCBlockCount--;
	SC_CheckFall( SC_Cell( vecPos.x ), SC_Cell( vecPos.y ), SC_Cell( vecPos.z ) + 1 );
"""),
])
patch('svencraft.as', [
    ("	SC_RegisterDrops();\n", "	SC_RegisterDrops();\n	SC_RegisterFalling();\n"),
])
print('ok')
