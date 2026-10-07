# Caves: stored air cells revealed lazily (like ores), one entrance cave realized at generation; dim templates
# for natural underground stone; save support.
import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"


def patch(name, pairs):
    p = os.path.join(D, name)
    s = open(p, newline='', encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (name, old[:90], s.count(old))
        s = s.replace(old, new)
    open(p, 'w', newline='', encoding='utf-8').write(s)
    print('patched', name)


patch('sc_sandbox.as', [
    # --- dim templates
    ('''dictionary g_SCTemplates;       // "<mat>_<sx>_<sy>_<sz>" -> brush model "*N"''',
     '''dictionary g_SCTemplates;       // "<mat>_<sx>_<sy>_<sz>" -> brush model "*N"
dictionary g_SCTemplatesDark;   // same, dimly lit copies used for natural underground stone and ores'''),
    ('''// pev.iuser1 = material, pev.vuser1 = size in blocks, pev.origin = center''',
     '''// pev.iuser1 = material, pev.vuser1 = size in blocks, pev.origin = center, pev.iuser2 = 1 when placed by a player'''),
    ('''	g_SCTemplates.deleteAll();
	array<CBaseEntity@> found;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "func_wall" ) ) !is null )
	{
		string szName = string( pEnt.pev.targetname );
		if( !szName.StartsWith( "sctpl_" ) )
			continue;''',
     '''	g_SCTemplates.deleteAll();
	g_SCTemplatesDark.deleteAll();
	array<CBaseEntity@> found;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "func_wall" ) ) !is null )
	{
		string szName = string( pEnt.pev.targetname );
		bool bDark = szName.StartsWith( "sctpd_" );
		if( !szName.StartsWith( "sctpl_" ) && !bDark )
			continue;'''),
    ('''		g_SCTemplates.set( SC_TplKey( iMat, atoi( parts[n - 3] ), atoi( parts[n - 2] ), atoi( parts[n - 1] ) ), string( pEnt.pev.model ) );''',
     '''		string szKey = SC_TplKey( iMat, atoi( parts[n - 3] ), atoi( parts[n - 2] ), atoi( parts[n - 1] ) );
		if( bDark )
			g_SCTemplatesDark.set( szKey, string( pEnt.pev.model ) );
		else
			g_SCTemplates.set( szKey, string( pEnt.pev.model ) );'''),
    # --- node spawn: caves, placed flag, dim templates
    ('''void SC_SpawnNode( int iMat, int x, int y, int z, int sx, int sy, int sz, bool bRaw = false )
{''',
     '''void SC_SpawnNode( int iMat, int x, int y, int z, int sx, int sy, int sz, bool bRaw = false, bool bPlaced = false )
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
	}'''),
    ('''	string szModel;
	if( !g_SCTemplates.get( SC_TplKey( iMat, sx, sy, sz ), szModel ) )
	{
		if( sx <= 1 && sy <= 1 && sz <= 1 )
			return;
		int hx = sx > 1 ? sx / 2 : 1, hy = sy > 1 ? sy / 2 : 1, hz = sz > 1 ? sz / 2 : 1;
		for( int i = 0; i < sx; i += hx )
			for( int j = 0; j < sy; j += hy )
				for( int k = 0; k < sz; k += hz )
					SC_SpawnNode( iMat, x + i, y + j, z + k, hx, hy, hz );
		return;
	}''',
     '''	string szModel;
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
	}'''),
    ('''	pEnt.pev.model = szModel;
	pEnt.pev.iuser1 = iMat;
	pEnt.pev.vuser1 = Vector( sx, sy, sz );''',
     '''	pEnt.pev.model = szModel;
	pEnt.pev.iuser1 = iMat;
	pEnt.pev.iuser2 = bPlaced ? 1 : 0;
	pEnt.pev.vuser1 = Vector( sx, sy, sz );'''),
    # --- generation
    ('''			SC_SpawnNode( MAT_STONE, x, y, -11, SC_CHUNK, SC_CHUNK, 8, true );
		}
	}
	SC_GenerateOres();''',
     '''			// stone stays one raw chunk (hidden caves and ores inside wait until it is dug into),
			// except where the entrance cave runs: those chunks are split now so the cave is open
			SC_SpawnNode( MAT_STONE, x, y, -11, SC_CHUNK, SC_CHUNK, 8, !g_SCOpenChunks.exists( "" + x + " " + y ) );
		}
	}
	SC_GenerateOres();'''),
    ('''void SC_GenerateTerrain()
{''', '''void SC_GenerateTerrain()
{
	SC_GenerateCaves();   // before ores (ores skip cave cells) and before the stone chunks (open chunks)'''),
    ('''	g_SCBlockCount = 0;
	g_SCCellProgress.deleteAll();
	SC_ClearOres();''', '''	g_SCBlockCount = 0;
	g_SCCellProgress.deleteAll();
	SC_ClearOres();
	SC_ClearCaves();'''),
    ('''void SC_AddOre( int x, int y, int z, int iMat )
{
	string szKey = "" + x + " " + y + " " + z;''', '''void SC_AddOre( int x, int y, int z, int iMat )
{
	if( SC_CaveAt( x, y, z ) )
		return;
	string szKey = "" + x + " " + y + " " + z;'''),
    # --- breaking into a hidden cave
    ('''	int iMat = pNode.pev.iuser1;
	Vector vecInside = tr.vecEndPos - tr.vecPlaneNormal * 2.0;
	int cx = SC_Cell( vecInside.x ), cy = SC_Cell( vecInside.y ), cz = SC_Cell( vecInside.z );''',
     '''	int iMat = pNode.pev.iuser1;
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
	}'''),
])

# caves module appended to sc_sandbox.as
p = os.path.join(D, 'sc_sandbox.as')
s = open(p, newline='', encoding='utf-8').read()
assert 'SC_GenerateCaves()\n{' not in s
s += '''
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
bool g_SCMarkOpen = false;      // cells added now belong to the entrance cave

void SC_ClearCaves()
{
	g_SCCaves.deleteAll();
	g_SCCaveCells.resize( 0 );
	g_SCCaveCount = 0;
	g_SCOpenChunks.deleteAll();
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
	g_EngineFuncs.ServerPrint( "Svencraft: caves " + g_SCCaveCount + " cells, " + g_SCOpenChunks.getKeys().length() + " open chunks\\n" );
}
'''
open(p, 'w', newline='', encoding='utf-8').write(s)
print('appended caves')

patch('sc_blocks.as', [
    ('''		int iBefore = g_SCBlockCount;
		SC_SpawnNode( iMat, SC_Cell( vecCenter.x ), SC_Cell( vecCenter.y ), SC_Cell( vecCenter.z ), 1, 1, 1 );''',
     '''		int iBefore = g_SCBlockCount;
		SC_ForgetCave( SC_Cell( vecCenter.x ), SC_Cell( vecCenter.y ), SC_Cell( vecCenter.z ) );
		SC_SpawnNode( iMat, SC_Cell( vecCenter.x ), SC_Cell( vecCenter.y ), SC_Cell( vecCenter.z ), 1, 1, 1, false, true );'''),
])

patch('sc_save.as', [
    ('''		s += "node " + pEnt.pev.iuser1 + " " + x + " " + y + " " + z + " " + sx + " " + sy + " " + sw + "\\n";''',
     '''		s += "node " + pEnt.pev.iuser1 + " " + x + " " + y + " " + z + " " + sx + " " + sy + " " + sw + " " + pEnt.pev.iuser2 + "\\n";'''),
    ('''	array<string>@ gone = g_SCGone.getKeys();''',
     '''	for( uint i = 0; i + 2 < g_SCCaveCells.length(); i += 3 )
	{
		if( SC_CaveAt( g_SCCaveCells[i], g_SCCaveCells[i + 1], g_SCCaveCells[i + 2] ) )
			s += "cave " + g_SCCaveCells[i] + " " + g_SCCaveCells[i + 1] + " " + g_SCCaveCells[i + 2] + "\\n";
	}
	array<string>@ gone = g_SCGone.getKeys();'''),
    ('''	SC_ClearOres();
	g_SCGone.deleteAll();
	int iBlocks = 0;''', '''	SC_ClearOres();
	SC_ClearCaves();
	g_SCGone.deleteAll();
	int iBlocks = 0;'''),
    ('''				SC_SpawnNode( iMat, atoi( t[2] ), atoi( t[3] ), atoi( t[4] ), atoi( t[5] ), atoi( t[6] ), atoi( t[7] ), true );''',
     '''				SC_SpawnNode( iMat, atoi( t[2] ), atoi( t[3] ), atoi( t[4] ), atoi( t[5] ), atoi( t[6] ), atoi( t[7] ), true, t.length() >= 9 && atoi( t[8] ) != 0 );'''),
    ('''		else if( t[0] == "ore" && t.length() >= 5 )''', '''		else if( t[0] == "cave" && t.length() >= 4 )
			SC_AddCave( atoi( t[1] ), atoi( t[2] ), atoi( t[3] ) );
		else if( t[0] == "ore" && t.length() >= 5 )'''),
])
