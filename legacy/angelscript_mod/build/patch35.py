# World save v2 (caves). Loading a v1 save (made before caves existed) adds caves to it in place.
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
    ('''dictionary g_SCOpenChunks;      // "x y" of stone chunks to split at generation (entrance cave)''',
     '''dictionary g_SCOpenChunks;      // "x y" of stone chunks to split at generation (entrance cave)
array<int> g_SCEntranceCells;   // flat x,y,z list of the entrance cave's cells'''),
    ('''	g_SCOpenChunks.deleteAll();
}''', '''	g_SCOpenChunks.deleteAll();
	g_SCEntranceCells.resize( 0 );
}'''),
    ('''	if( g_SCMarkOpen )
		g_SCOpenChunks.set(''', '''	if( g_SCMarkOpen )
	{
		g_SCEntranceCells.insertLast( x ); g_SCEntranceCells.insertLast( y ); g_SCEntranceCells.insertLast( z );
	}
	if( g_SCMarkOpen )
		g_SCOpenChunks.set('''),
])

p = os.path.join(D, 'sc_sandbox.as')
s = open(p, newline='', encoding='utf-8').read()
s += '''
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
	g_EngineFuncs.ServerPrint( "Svencraft: added caves to an older save (entrance cells opened " + iOpened + ", pieces now " + g_SCBlockCount + ")\\n" );
}
'''
open(p, 'w', newline='', encoding='utf-8').write(s)
print('appended upgrade')

patch('sc_save.as', [
    ('''	string s = "svencraft-world 1\\n";''', '''	string s = "svencraft-world 2\\n";'''),
    ('''	if( iNodes == 0 )
		return false;   // reset marker (!resetworld) or empty file
''', '''	if( iNodes == 0 )
		return false;   // reset marker (!resetworld) or empty file
	int iVersion = atoi( SC_Split( lines[0], " " )[1] );
'''),
    ('''	// Map entities that were mined away stay gone.''', '''	if( iVersion < 2 )
		SC_AddCavesToWorld();

	// Map entities that were mined away stay gone.'''),
])
