import sys
p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
old = '''		g_Scheduler.SetTimeout( "SC_SelfTestPhase2", 0.5 );
		return;
	}
	SC_TestOut( "SCTEST done\\n" );
}
'''
new = '''		g_Scheduler.SetTimeout( "SC_SelfTestPhase2", 0.5 );
		return;
	}
	SC_SelfTestProps();
	SC_TestOut( "SCTEST done\\n" );
	SC_TestFlush();
}

void SC_TestFlush()
{
	File@ f = g_FileSystem.OpenFile( "scripts/plugins/store/sc_selftest.txt", OpenFile::WRITE );
	if( f !is null && f.IsOpen() )
	{
		f.Write( g_SCTestLog );
		f.Close();
	}
}

// Props/structures on a normal map: classification, splitting walls into blocks, prop material mixes, decor picking.
void SC_SelfTestProps()
{
	dictionary kinds;
	array<string> kindNames = { "none", "block", "node", "creature", "prop", "breakable", "wall", "surface" };
	array<CBaseEntity@> walls;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityInSphere( pEnt, g_vecZero, 65536, "*", "classname" ) ) !is null )
	{
		int k = SC_KindOf( pEnt );
		if( !pEnt.IsBSPModel() && k != KIND_PROP )
			continue;
		string key = pEnt.GetClassname() + "->" + kindNames[k];
		int c = 0;
		kinds.get( key, c );
		kinds.set( key, c + 1 );
		if( k == KIND_WALL && walls.length() < 40 )
			walls.insertLast( pEnt );
	}
	array<string>@ keys = kinds.getKeys();
	keys.sortAsc();
	string szKinds = "";
	for( uint i = 0; i < keys.length(); i++ )
	{
		int c = 0;
		kinds.get( keys[i], c );
		szKinds += keys[i] + "=" + c + " ";
	}
	SC_TestOut( "SCTEST kinds: " + szKinds + "\\n" );

	int iSplit = 0, iProp = 0, iBig = 0, iPieces = 0;
	for( uint i = 0; i < walls.length() && iSplit < 6; i++ )
	{
		Vector d = walls[i].pev.maxs - walls[i].pev.mins;
		array<CBaseEntity@> pieces;
		int r = SC_BreakIntoBlocks( walls[i], MAT_CONCRETE, pieces );
		if( r == KIND_WALL )
		{
			iSplit++;
			iPieces += pieces.length();
			SC_TestOut( "SCTEST split " + walls[i].GetClassname() + " size " + int( d.x ) + "x" + int( d.y ) + "x" + int( d.z ) + " -> " + pieces.length()
				+ " pieces, shape " + pieces[0].pev.body + ", piece box " + int( pieces[0].pev.size.x ) + "x" + int( pieces[0].pev.size.y ) + "x" + int( pieces[0].pev.size.z )
				+ " scale " + pieces[0].pev.scale + " angles " + pieces[0].pev.angles.x + "," + pieces[0].pev.angles.z + "\\n" );
		}
		else if( r == KIND_PROP ) iProp++;
		else if( r == KIND_SURFACE ) iBig++;
	}
	SC_TestOut( "SCTEST walls tried: split=" + iSplit + " (" + iPieces + " pieces) wholeprop=" + iProp + " toobig=" + iBig + "\\n" );

	dictionary seen;
	string szMix = "";
	@pEnt = null;
	CBaseEntity@ pDecorTarget = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "item_generic" ) ) !is null )
	{
		string szModel = string( pEnt.pev.model );
		if( seen.exists( szModel ) )
			continue;
		seen.set( szModel, true );
		array<int> mats;
		array<float> weights;
		SC_PropMix( pEnt, MAT_NONE, mats, weights );
		Vector mn, mx;
		SC_EntBox( pEnt, mn, mx );
		string szM = "";
		for( uint i = 0; i < mats.length(); i++ )
			szM += g_SCMats[mats[i]].id + ( i + 1 < mats.length() ? "+" : "" );
		if( seen.getSize() <= 14 )
			szMix += szModel + "=" + ( szM == "" ? "nothing" : szM ) + "(" + int( SC_BoxBlocks( mx - mn ) * 10 ) / 10.0 + "blk,solid" + pEnt.pev.solid + ") ";
		if( pDecorTarget is null && ( mx - mn ).Length() > 8 && pEnt.pev.solid == SOLID_NOT )
			@pDecorTarget = pEnt;
	}
	SC_TestOut( "SCTEST item_generic models=" + seen.getSize() + ": " + szMix + "\\n" );

	if( pDecorTarget !is null )
	{
		Vector mn, mx;
		SC_EntBox( pDecorTarget, mn, mx );
		Vector vecCenter = ( mn + mx ) * 0.5;
		Vector vecFrom = vecCenter + Vector( 100, 0, 0 );
		float flDist;
		CBaseEntity@ pFound = SC_FindDecor( vecFrom, Vector( -1, 0, 0 ), SC_REACH, flDist );
		SC_TestOut( "SCTEST decor pick of " + string( pDecorTarget.pev.model ) + ": " + ( pFound is pDecorTarget ? "OK" : ( pFound is null ? "missed" : "got " + string( pFound.pev.model ) ) ) + " dist=" + flDist + "\\n" );
	}
	SC_TestOut( "SCTEST entities in use: " + g_EngineFuncs.NumberOfEntities() + " pieces=" + g_SCBlockCount + "\\n" );
}
'''
if 'SC_SelfTestProps' in s:
    print('already patched'); sys.exit()
assert old in s, 'anchor not found'
open(p, 'w', newline='').write(s.replace(old, new))
print('patched')
