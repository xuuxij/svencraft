import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"
S = r"."

def patch(p, reps):
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (p, a[:70])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='').write(s)

# templates for sand / gravel slabs so patches of them stay cheap
patch(os.path.join(S, 'makesandbox.py'), [
    ("""         'stone': [(8, 8, 8), (4, 4, 4), (2, 2, 2)]}""",
     """         'stone': [(8, 8, 8), (4, 4, 4), (2, 2, 2)],
         'sand': [(8, 8, 2), (8, 8, 1), (4, 4, 1), (2, 2, 1)], 'gravel': [(8, 8, 1), (4, 4, 1), (2, 2, 1)]}"""),
])

# terrain: a sand beach (south-east) and a gravel patch (north-west), clear of the test area
patch(os.path.join(D, 'sc_sandbox.as'), [
    ("""			SC_SpawnNode( MAT_GRASS, x, y, -1, SC_CHUNK, SC_CHUNK, 1 );
			SC_SpawnNode( MAT_DIRT, x, y, -3, SC_CHUNK, SC_CHUNK, 2 );""",
     """			bool bBeach = y == -24 && ( x == 16 || x == 24 );
			bool bGravel = x == -24 && y == 16;
			SC_SpawnNode( bBeach ? MAT_SAND : ( bGravel ? MAT_GRAVEL : MAT_GRASS ), x, y, -1, SC_CHUNK, SC_CHUNK, 1 );
			SC_SpawnNode( bBeach ? MAT_SAND : MAT_DIRT, x, y, -3, SC_CHUNK, SC_CHUNK, 2 );"""),
])

# electronics from metal + glass, so every recipe is reachable without special props
patch(os.path.join(D, 'sc_crafting.as'), [
    ("""	SC_BlockRecipe( CRAFT_BLOCKS, true, "metal:1", MAT_VENT, 2 );""",
     """	SC_BlockRecipe( CRAFT_BLOCKS, true, "metal:1", MAT_VENT, 2 );
	SC_BlockRecipe( CRAFT_BLOCKS, true, "metal:1,glass:1", MAT_CIRCUIT, 1 );"""),
])
print('ok')
