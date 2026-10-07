import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

# 1. Placement must never overlap a player or monster (traces miss them: players are hit-tested by hitboxes).
p = os.path.join(D, 'sc_blocks.as')
s = open(p, newline='').read()
old = '''bool SC_BoxClear( const Vector& in vecCenter, float flHalf )
{
	if( g_EngineFuncs.PointContents( vecCenter ) == CONTENTS_SOLID )
		return false;
'''
new = '''bool SC_BoxClear( const Vector& in vecCenter, float flHalf )
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
'''
assert old in s
s = s.replace(old, new)
open(p, 'w', newline='').write(s)

# 2. Door textures are wood.
p = os.path.join(D, 'sc_materials.as')
s = open(p, newline='').read()
old = '''	if( SC_Has( tex, "METAL" ) || SC_Has( tex, "STEEL" ) || SC_Has( tex, "IRON" ) || SC_Has( tex, "PIPE" ) || SC_Has( tex, "RUST" ) ) return MAT_METAL;'''
new = old + '''
	if( SC_Has( tex, "DOOR" ) ) return MAT_PLANKS;'''
assert old in s
s = s.replace(old, new)
open(p, 'w', newline='').write(s)

# 3. Minecraft-style block trees in the sandbox forest.
p = os.path.join(D, 'sc_sandbox.as')
s = open(p, newline='').read()
old = '''			SC_SpawnNode( MAT_STONE, x, y, -11, SC_CHUNK, SC_CHUNK, 8 );
		}
	}
}'''
new = '''			SC_SpawnNode( MAT_STONE, x, y, -11, SC_CHUNK, SC_CHUNK, 8 );
		}
	}
	SC_BlockTree( -14, -12 );
	SC_BlockTree( 12, -16 );
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
}'''
assert old in s
s = s.replace(old, new)
open(p, 'w', newline='').write(s)
print('ok')
