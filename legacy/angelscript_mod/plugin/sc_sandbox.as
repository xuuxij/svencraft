// Diggable terrain for maps built with block templates (svencraft_sandbox).
//
// The map contains hidden func_wall templates named "sctpl_<material>_<sx>_<sy>_<sz>" (size in blocks,
// origin brush at the center). Their brush models are copied onto "sc_node" entities, which form the
// terrain as big chunks. Mining a block splits only the chunk it is in (octree-style), so the entity
// count grows with how much is dug, not with the size of the world.

dictionary g_SCTemplates;       // "<mat>_<sx>_<sy>_<sz>" -> brush model "*N"
dictionary g_SCTemplatesDark;   // same, dimly lit copies used for natural underground stone and ores
dictionary g_SCCellProgress;    // "x y z" (block cell) -> mining progress on terrain nodes
bool g_SCSandbox = false;

const int SC_TERRAIN_HALF = 32; // terrain spans -32..31 blocks on x and y (64 x 64)
const int SC_CHUNK = 8;

// pev.iuser1 = material, pev.vuser1 = size in blocks, pev.origin = center, pev.iuser2 = 1 when placed by a player
class sc_node : ScriptBaseEntity
{
	int ObjectCaps()
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
		self.pev.solid = SOLID_BSP;
		self.pev.movetype = MOVETYPE_PUSH;
		self.pev.takedamage = DAMAGE_NO;
		g_EntityFuncs.SetModel( self, string( self.pev.model ) );
		g_EntityFuncs.SetOrigin( self, self.pev.origin );
		int iMat = self.pev.iuser1;
		if( iMat == MAT_GLASS || iMat == MAT_LEAVES )
		{
			self.pev.rendermode = kRenderTransAlpha;
			self.pev.renderamt = 255;
		}
	}
}

void SC_RegisterNode()
{
	g_CustomEntityFuncs.RegisterCustomEntity( "sc_node", "sc_node" );
}

bool SC_IsNode( CBaseEntity@ pEnt )
{
	return pEnt !is null && pEnt.GetClassname() == "sc_node";
}

array<string> SC_Split( const string& in s, const string& in sep )
{
	array<string> parts;
	uint start = 0;
	while( true )
	{
		if( start >= s.Length() )
		{
			parts.insertLast( "" );   // Find/SubString misbehave at the very end of the string
			return parts;
		}
		uint i = s.Find( sep, start );
		if( i == String::INVALID_INDEX )
		{
			parts.insertLast( s.SubString( start ) );
			return parts;
		}
		parts.insertLast( s.SubString( start, i - start ) );
		start = i + sep.Length();
	}
	return parts;
}

string SC_TplKey( int iMat, int sx, int sy, int sz )
{
	return "" + iMat + "_" + sx + "_" + sy + "_" + sz;
}

// Collect the template models and remove the template entities themselves.
void SC_FindTemplates()
{
	g_SCTemplates.deleteAll();
	g_SCTemplatesDark.deleteAll();
	array<CBaseEntity@> found;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "func_wall" ) ) !is null )
	{
		string szName = string( pEnt.pev.targetname );
		bool bDark = szName.StartsWith( "sctpd_" );
		if( !szName.StartsWith( "sctpl_" ) && !bDark )
			continue;
		array<string> parts = SC_Split( szName, "_" );
		uint n = parts.length();
		if( n < 5 )
			continue;
		string szMat = parts[1];
		for( uint i = 2; i + 3 < n; i++ )
			szMat += "_" + parts[i];          // material ids can contain underscores (iron_ore)
		int iMat = SC_MatByName( szMat );
		if( iMat == MAT_NONE )
			continue;
		string szKey = SC_TplKey( iMat, atoi( parts[n - 3] ), atoi( parts[n - 2] ), atoi( parts[n - 1] ) );
		if( bDark )
			g_SCTemplatesDark.set( szKey, string( pEnt.pev.model ) );
		else
			g_SCTemplates.set( szKey, string( pEnt.pev.model ) );
		found.insertLast( pEnt );
	}
	for( uint i = 0; i < found.length(); i++ )
		g_EntityFuncs.Remove( found[i] );
	g_SCSandbox = found.length() > 0;
}

bool SC_HasTemplate( int iMat, int sx, int sy, int sz )
{
	return g_SCTemplates.exists( SC_TplKey( iMat, sx, sy, sz ) );
}

// Spawn a node covering blocks [x, x+sx) x [y, y+sy) x [z, z+sz). Falls back to smaller pieces when
// there is no template of that size.
void SC_SpawnNode( int iMat, int x, int y, int z, int sx, int sy, int sz, bool bRaw = false, bool bPlaced = false )
{
	// Cave cells are left empty when the terrain around them is first split (natural terrain only).
	if( !bRaw && !bPlaced && g_SCCaveCount > 0 )
	{
		if( sx == 1 && sy == 1 && sz == 1 )
		{
			if( SC_CaveAt( x, y, z ) )
			{
				SC_ForgetCave( x, y, z );
				return;
			}
		}
		else if( SC_BoxHasCave( x, y, z, sx, sy, sz ) )
		{
			int hx = sx > 1 ? sx / 2 : 1, hy = sy > 1 ? sy / 2 : 1, hz = sz > 1 ? sz / 2 : 1;
			for( int i = 0; i < sx; i += hx )
				for( int j = 0; j < sy; j += hy )
					for( int k = 0; k < sz; k += hz )
						SC_SpawnNode( iMat, x + i, y + j, z + k, hx, hy, hz );
			return;
		}
	}
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
	string szModel;
	string szKey = SC_TplKey( iMat, sx, sy, sz );
	if( bPlaced || !g_SCTemplatesDark.get( szKey, szModel ) )
	{
		if( !g_SCTemplates.get( szKey, szModel ) )
		{
			if( sx <= 1 && sy <= 1 && sz <= 1 )
				return;
			int hx = sx > 1 ? sx / 2 : 1, hy = sy > 1 ? sy / 2 : 1, hz = sz > 1 ? sz / 2 : 1;
			for( int i = 0; i < sx; i += hx )
				for( int j = 0; j < sy; j += hy )
					for( int k = 0; k < sz; k += hz )
						SC_SpawnNode( iMat, x + i, y + j, z + k, hx, hy, hz, bRaw, bPlaced );
			return;
		}
	}

	Vector vecCenter( ( x + sx * 0.5 ) * SC_BLOCK_SIZE, ( y + sy * 0.5 ) * SC_BLOCK_SIZE, ( z + sz * 0.5 ) * SC_BLOCK_SIZE );
	CBaseEntity@ pEnt = g_EntityFuncs.Create( "sc_node", vecCenter, g_vecZero, true );
	if( pEnt is null )
		return;
	pEnt.pev.model = szModel;
	pEnt.pev.iuser1 = iMat;
	pEnt.pev.iuser2 = bPlaced ? 1 : 0;
	pEnt.pev.vuser1 = Vector( sx, sy, sz );
	g_EntityFuncs.DispatchSpawn( pEnt.edict() );
	g_SCBlockCount++;
}

void SC_GenerateTerrain()
{
	SC_GenerateCaves();   // before ores (ores skip cave cells) and before the stone chunks (open chunks)
	for( int x = -SC_TERRAIN_HALF; x < SC_TERRAIN_HALF; x += SC_CHUNK )
	{
		for( int y = -SC_TERRAIN_HALF; y < SC_TERRAIN_HALF; y += SC_CHUNK )
		{
			bool bBeach = y == -24 && ( x == 16 || x == 24 );
			bool bGravel = x == -24 && y == 16;
			SC_SpawnNode( bBeach ? MAT_SAND : ( bGravel ? MAT_GRAVEL : MAT_GRASS ), x, y, -1, SC_CHUNK, SC_CHUNK, 1 );
			SC_SpawnNode( bBeach ? MAT_SAND : MAT_DIRT, x, y, -3, SC_CHUNK, SC_CHUNK, 2 );
			// stone stays one raw chunk (hidden caves and ores inside wait until it is dug into),
			// except where the entrance cave runs: those chunks are split now so the cave is open
			SC_SpawnNode( MAT_STONE, x, y, -11, SC_CHUNK, SC_CHUNK, 8, !g_SCOpenChunks.exists( "" + x + " " + y ) );
		}
	}
	SC_GenerateOres();
	SC_BlockTree( -14, -12 );
	SC_BlockTree( 12, -16 );
	g_EngineFuncs.ServerPrint( "Svencraft: generated terrain, " + g_SCBlockCount + " pieces\n" );
}

// Oak-style tree: 5-block log trunk with a leaf canopy (block coordinates of the trunk base, on top of the grass).
void SC_BlockTree( int x, int y )
{
	for( int z = 0; z < 5; z++ )
		SC_SpawnNode( MAT_LOG, x, y, z, 1, 1, 1 );
	for( int z = 3; z <= 6; z++ )
	{
		int r = z <= 4 ? 2 : 1;
		for( int dx = -r; dx <= r; dx++ )
			for( int dy = -r; dy <= r; dy++ )
			{
				if( dx == 0 && dy == 0 && z <= 4 ) continue;                      // trunk
				if( r == 2 && ( dx == -2 || dx == 2 ) && ( dy == -2 || dy == 2 ) ) continue;   // round off corners
				if( z == 6 && dx != 0 && dy != 0 ) continue;                      // plus-shaped top
				SC_SpawnNode( MAT_LEAVES, x + dx, y + dy, z, 1, 1, 1 );
			}
	}
}

void SC_ResetWorld()
{
	array<string> classes = { "sc_node", "sc_block" };
	for( uint c = 0; c < classes.length(); c++ )
	{
		CBaseEntity@ pEnt = null;
		while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, classes[c] ) ) !is null )
			g_EntityFuncs.Remove( pEnt );
	}
	g_SCBlockCount = 0;
	g_SCCellProgress.deleteAll();
	SC_ClearOres();
	SC_ClearCaves();
	SC_ClearWorldSave();
	if( g_SCSandbox )
		g_Scheduler.SetTimeout( "SC_GenerateTerrain", 0.2 ); // after the removals are processed
}

int SC_Cell( float v )
{
	return int( SC_Floor( v / SC_BLOCK_SIZE ) );
}

// Remove one block cell from a node, respawning the rest of the node as smaller pieces.
void SC_SplitAround( int iMat, int x, int y, int z, int sx, int sy, int sz, int cx, int cy, int cz )
{
	if( sx <= 1 && sy <= 1 && sz <= 1 )
		return; // this is the mined cell
	int hx = sx > 1 ? sx / 2 : 1, hy = sy > 1 ? sy / 2 : 1, hz = sz > 1 ? sz / 2 : 1;
	for( int i = 0; i < sx; i += hx )
	{
		for( int j = 0; j < sy; j += hy )
		{
			for( int k = 0; k < sz; k += hz )
			{
				int ax = x + i, ay = y + j, az = z + k;
				bool bContains = cx >= ax && cx < ax + hx && cy >= ay && cy < ay + hy && cz >= az && cz < az + hz;
				if( bContains )
					SC_SplitAround( iMat, ax, ay, az, hx, hy, hz, cx, cy, cz );
				else
					SC_SpawnNode( iMat, ax, ay, az, hx, hy, hz );
			}
		}
	}
}

// Remove block cell (cx, cy, cz) from pNode. Returns false if the block limit prevents splitting.
bool SC_CarveNode( CBaseEntity@ pNode, int cx, int cy, int cz )
{
	Vector vecSize = pNode.pev.vuser1;
	int sx = int( vecSize.x + 0.5 ), sy = int( vecSize.y + 0.5 ), sz = int( vecSize.z + 0.5 );
	if( ( sx > 1 || sy > 1 || sz > 1 ) && g_SCBlockCount >= SC_MAX_BLOCKS )
		return false;

	Vector vecOrigin = pNode.pev.origin;
	int x0 = int( SC_Floor( vecOrigin.x / SC_BLOCK_SIZE - sx * 0.5 + 0.5 ) );
	int y0 = int( SC_Floor( vecOrigin.y / SC_BLOCK_SIZE - sy * 0.5 + 0.5 ) );
	int z0 = int( SC_Floor( vecOrigin.z / SC_BLOCK_SIZE - sz * 0.5 + 0.5 ) );
	int iMat = pNode.pev.iuser1;
	g_EntityFuncs.Remove( pNode );
	g_SCBlockCount--;
	SC_SplitAround( iMat, x0, y0, z0, sx, sy, sz, cx, cy, cz );
	return true;
}

void SC_HitNode( CBasePlayer@ pPlayer, CBaseEntity@ pNode, int iTool, TraceResult& in tr )
{
	int iMat = pNode.pev.iuser1;
	Vector vecInside = tr.vecEndPos - tr.vecPlaneNormal * 2.0;
	int cx = SC_Cell( vecInside.x ), cy = SC_Cell( vecInside.y ), cz = SC_Cell( vecInside.z );
	if( pNode.pev.iuser2 == 0 && SC_CaveAt( cx, cy, cz ) )
	{
		// the face is the wall of a still-hidden cave: it opens up
		SC_HitSound( pPlayer, g_SCMats[iMat].family );
		SC_CarveNode( pNode, cx, cy, cz );
		SC_ForgetCave( cx, cy, cz );
		SC_BreakSound( tr.vecEndPos, g_SCMats[iMat].family );
		return;
	}
	if( iMat == MAT_STONE )
	{
		int iOre = SC_OreAt( cx, cy, cz );
		if( iOre != MAT_NONE )
			iMat = iOre;            // a still-hidden ore cell
	}
	SCMaterial@ m = g_SCMats[iMat];
	SC_HitSound( pPlayer, m.family );
	Vector vecCell( ( cx + 0.5 ) * SC_BLOCK_SIZE, ( cy + 0.5 ) * SC_BLOCK_SIZE, ( cz + 0.5 ) * SC_BLOCK_SIZE );
	string szKey = "" + cx + " " + cy + " " + cz;

	float flProgress = 0;
	g_SCCellProgress.get( szKey, flProgress );
	flProgress += SC_SwingPower( pPlayer, iTool, iMat );
	if( flProgress < 0.999 )
	{
		g_SCCellProgress.set( szKey, flProgress );
		SC_Debris( tr.vecEndPos, 8, m.family, 2 );
		Vector n = SC_AxisNormal( tr.vecPlaneNormal );
		SC_ShowCrack( pPlayer, vecCell + n * ( SC_BLOCK_SIZE * 0.5 ), n, SC_BLOCK_SIZE, flProgress );
		return;
	}
	g_SCCellProgress.delete( szKey );
	SC_HideCrack( pPlayer );

	if( !SC_CarveNode( pNode, cx, cy, cz ) )
	{
		SC_Print( pPlayer, "Block limit reached (" + SC_MAX_BLOCKS + ")\nsay !resetworld to start over" );
		return;
	}

	SC_Debris( vecCell, SC_BLOCK_SIZE, m.family, 8 );
	SC_BreakSound( vecCell, m.family );
	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecCell );
	SC_ForgetOre( cx, cy, cz );
	SC_CheckFall( cx, cy, cz + 1 );
}

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
	if( SC_CaveAt( x, y, z ) )
		return;
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
		if( ( pEnt.pev.flags & FL_KILLME ) != 0 )
			continue;   // removed this frame (e.g. a node that was just split): its replacement pieces are what count
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

//
// Caves (sandbox): worm tunnels of 2x2x2 cells with the odd room, stored as air cells and carved out when the
// stone around them is first split. Cells sit on the octree lattice (x, y even; z odd) so a cave costs few pieces.
// One cave starts with a staircase from the surface and is opened at generation; the others stay hidden
// (and cost no entities) until someone digs into them.
//
dictionary g_SCCaves;           // "x y z" -> true while the cell is still unrealized
array<int> g_SCCaveCells;       // flat x,y,z list for box tests
int g_SCCaveCount = 0;
dictionary g_SCOpenChunks;      // "x y" of stone chunks to split at generation (entrance cave)
array<int> g_SCEntranceCells;   // flat x,y,z list of the entrance cave's cells
bool g_SCMarkOpen = false;      // cells added now belong to the entrance cave

void SC_ClearCaves()
{
	g_SCCaves.deleteAll();
	g_SCCaveCells.resize( 0 );
	g_SCCaveCount = 0;
	g_SCOpenChunks.deleteAll();
	g_SCEntranceCells.resize( 0 );
}

bool SC_CaveAt( int x, int y, int z )
{
	return g_SCCaveCount > 0 && g_SCCaves.exists( "" + x + " " + y + " " + z );
}

void SC_ForgetCave( int x, int y, int z )
{
	g_SCCaves.delete( "" + x + " " + y + " " + z );
}

void SC_AddCave( int x, int y, int z )
{
	if( x < -SC_TERRAIN_HALF || x >= SC_TERRAIN_HALF || y < -SC_TERRAIN_HALF || y >= SC_TERRAIN_HALF || z < -11 || z > -1 )
		return;
	if( g_SCMarkOpen )
	{
		g_SCEntranceCells.insertLast( x ); g_SCEntranceCells.insertLast( y ); g_SCEntranceCells.insertLast( z );
	}
	if( g_SCMarkOpen )
		g_SCOpenChunks.set( "" + ( int( SC_Floor( float( x + SC_TERRAIN_HALF ) / SC_CHUNK ) ) * SC_CHUNK - SC_TERRAIN_HALF ) + " "
			+ ( int( SC_Floor( float( y + SC_TERRAIN_HALF ) / SC_CHUNK ) ) * SC_CHUNK - SC_TERRAIN_HALF ), true );
	string szKey = "" + x + " " + y + " " + z;
	if( g_SCCaves.exists( szKey ) )
		return;
	g_SCCaves.set( szKey, true );
	g_SCCaveCells.insertLast( x ); g_SCCaveCells.insertLast( y ); g_SCCaveCells.insertLast( z );
	g_SCCaveCount++;
}

bool SC_BoxHasCave( int x, int y, int z, int sx, int sy, int sz )
{
	for( uint i = 0; i + 2 < g_SCCaveCells.length(); i += 3 )
	{
		int cx = g_SCCaveCells[i], cy = g_SCCaveCells[i + 1], cz = g_SCCaveCells[i + 2];
		if( cx >= x && cx < x + sx && cy >= y && cy < y + sy && cz >= z && cz < z + sz && SC_CaveAt( cx, cy, cz ) )
			return true;
	}
	return false;
}

void SC_CaveBox( int x, int y, int z, int s )
{
	for( int i = 0; i < s; i++ )
		for( int j = 0; j < s; j++ )
			for( int k = 0; k < s; k++ )
				SC_AddCave( x + i, y + j, z + k );
}

// Worm of 2x2x2 cells from (x, y, z) (x, y even, z odd); ends in a 4x4x4 room when bRoom.
void SC_CaveWorm( int x, int y, int z, int dx, int dy, int iSteps, bool bRoom )
{
	for( int n = 0; n < iSteps; n++ )
	{
		SC_CaveBox( x, y, z, 2 );
		if( Math.RandomLong( 0, 99 ) < 30 )
		{
			int r = Math.RandomLong( 0, 3 );
			dx = r == 0 ? 2 : ( r == 1 ? -2 : 0 );
			dy = r == 2 ? 2 : ( r == 3 ? -2 : 0 );
		}
		if( Math.RandomLong( 0, 99 ) < 15 )
		{
			int nz = z + ( Math.RandomLong( 0, 1 ) == 0 ? -2 : 2 );
			if( nz >= -11 && nz <= -7 )
			{
				SC_CaveBox( x, y, nz, 2 );   // the step itself, so the tunnel stays walkable
				z = nz;
			}
		}
		x += dx; y += dy;
		if( x < -SC_TERRAIN_HALF + 2 || x > SC_TERRAIN_HALF - 4 ) { x -= dx * 2; dx = -dx; }
		if( y < -SC_TERRAIN_HALF + 2 || y > SC_TERRAIN_HALF - 4 ) { y -= dy * 2; dy = -dy; }
	}
	if( bRoom )
	{
		int rx = int( SC_Floor( float( x ) / 4.0 ) ) * 4, ry = int( SC_Floor( float( y ) / 4.0 ) ) * 4;
		SC_CaveBox( rx, ry, -11, 4 );
	}
}

// Staircase from the surface: 2 wide, one block down per block forward, 3 blocks of headroom.
void SC_CaveStairs( int x, int y, int iDepth )
{
	for( int k = 1; k <= iDepth; k++ )
		for( int w = 0; w < 2; w++ )
			for( int h = 0; h < 3; h++ )
			{
				int z = -k + h;
				if( z <= -1 )
					SC_AddCave( x + k, y + w, z );
			}
}

const int SC_CAVE_ENTRANCE_X = -30;   // stairs start here (block coords) and run toward +x
const int SC_CAVE_ENTRANCE_Y = -8;

void SC_GenerateCaves()
{
	SC_ClearCaves();
	// the entrance cave, open from the start
	g_SCMarkOpen = true;
	SC_CaveStairs( SC_CAVE_ENTRANCE_X, SC_CAVE_ENTRANCE_Y, 9 );          // floor reaches the tunnel floor (z -10 top)
	SC_CaveWorm( SC_CAVE_ENTRANCE_X + 10, SC_CAVE_ENTRANCE_Y, -9, 2, 0, 14, true );
	g_SCMarkOpen = false;
	// hidden caves
	for( int c = 0; c < 5; c++ )
	{
		int x = Math.RandomLong( -14, 12 ) * 2, y = Math.RandomLong( -14, 12 ) * 2;
		int z = -11 + Math.RandomLong( 0, 2 ) * 2;
		int r = Math.RandomLong( 0, 3 );
		SC_CaveWorm( x, y, z, r == 0 ? 2 : ( r == 1 ? -2 : 0 ), r == 2 ? 2 : ( r == 3 ? -2 : 0 ), Math.RandomLong( 8, 14 ), Math.RandomLong( 0, 1 ) == 0 );
	}
	g_EngineFuncs.ServerPrint( "Svencraft: caves " + g_SCCaveCount + " cells, " + g_SCOpenChunks.getKeys().length() + " open chunks\n" );
}

// A world saved before caves existed: generate caves for it, open the entrance cave through whatever natural
// terrain is there, and leave the rest hidden. Player-built blocks and already-dug cells are left alone.
bool SC_IsTerrainMat( int iMat )
{
	return iMat == MAT_GRASS || iMat == MAT_DIRT || iMat == MAT_STONE || iMat == MAT_SAND || iMat == MAT_GRAVEL
		|| iMat == MAT_IRON_ORE || iMat == MAT_CRYSTAL_ORE;
}

void SC_AddCavesToWorld()
{
	SC_GenerateCaves();
	// hidden cells only where natural terrain still is; ore cells inside caves become cave
	for( uint i = 0; i + 2 < g_SCCaveCells.length(); i += 3 )
	{
		int x = g_SCCaveCells[i], y = g_SCCaveCells[i + 1], z = g_SCCaveCells[i + 2];
		CBaseEntity@ pNode = SC_BlockAt( Vector( ( x + 0.5 ) * SC_BLOCK_SIZE, ( y + 0.5 ) * SC_BLOCK_SIZE, ( z + 0.5 ) * SC_BLOCK_SIZE ) );
		if( pNode is null || !SC_IsNode( pNode ) || pNode.pev.iuser2 != 0 || !SC_IsTerrainMat( pNode.pev.iuser1 ) )
			SC_ForgetCave( x, y, z );
		else
			SC_ForgetOre( x, y, z );
	}
	int iOpened = 0;
	for( uint i = 0; i + 2 < g_SCEntranceCells.length(); i += 3 )
	{
		int x = g_SCEntranceCells[i], y = g_SCEntranceCells[i + 1], z = g_SCEntranceCells[i + 2];
		if( !SC_CaveAt( x, y, z ) )
			continue;
		CBaseEntity@ pNode = SC_BlockAt( Vector( ( x + 0.5 ) * SC_BLOCK_SIZE, ( y + 0.5 ) * SC_BLOCK_SIZE, ( z + 0.5 ) * SC_BLOCK_SIZE ) );
		if( SC_IsNode( pNode ) && SC_CarveNode( pNode, x, y, z ) )
			iOpened++;
		SC_ForgetCave( x, y, z );
	}
	g_EngineFuncs.ServerPrint( "Svencraft: added caves to an older save (entrance cells opened " + iOpened + ", pieces now " + g_SCBlockCount + ")\n" );
}
