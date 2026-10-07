p = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft\svencraft.as"
s = open(p, newline='').read()
old = '''	int iSplit = 0, iProp = 0, iBig = 0, iPieces = 0;'''
new = '''	// Doors first: they must split like walls.
	array<string> doorClasses = { "func_door", "func_door_rotating" };
	for( uint dc = 0; dc < doorClasses.length(); dc++ )
	{
		CBaseEntity@ pDoor = g_EntityFuncs.FindEntityByClassname( null, doorClasses[dc] );
		if( pDoor is null )
			continue;
		array<CBaseEntity@> dp;
		Vector dd = pDoor.pev.maxs - pDoor.pev.mins;
		int r = SC_BreakIntoBlocks( pDoor, MAT_PLANKS, dp );
		SC_TestOut( "SCTEST door " + doorClasses[dc] + " size " + int( dd.x ) + "x" + int( dd.y ) + "x" + int( dd.z ) + " origin " + pDoor.pev.origin.x + "," + pDoor.pev.origin.y + "," + pDoor.pev.origin.z
			+ " angles " + pDoor.pev.angles.y + " takedamage " + pDoor.pev.takedamage + " kind " + kindNames[SC_KindOf( pDoor )] + " -> result " + kindNames[r] + ", " + dp.length() + " pieces\\n" );
	}

	int iSplit = 0, iProp = 0, iBig = 0, iPieces = 0;'''
if 'Doors first' not in s:
    assert old in s
    s = s.replace(old, new, 1)
open(p, 'w', newline='').write(s)
print('ok')
