import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (name, a[:70])
        s = s.replace(a, b)
    open(p, 'w', newline='').write(s)

patch('sc_blocks.as', [
    ("const float SC_BLOCK_SIZE = 32.0;   // one full block, in world units (player is 72 tall)",
     "const float SC_BLOCK_SIZE = 40.0;   // one block in world units: 2 blocks fit the 72-unit player, 1 block the crouched one"),
])

patch('sc_props.as', [
    ("const array<float> SC_SHAPE_THICKNESS = { 32, 16, 8, 4 };", "const array<float> SC_SHAPE_THICKNESS = { 40, 16, 8, 4 };"),
    ('const array<string> SC_PROP_CLASSES = { "item_generic", "monster_furniture", "cycler", "cycler_sprite", "cycler_wreckage" };',
     'const array<string> SC_PROP_CLASSES = { "item_generic", "monster_furniture", "cycler", "cycler_sprite", "cycler_wreckage", "sc_prop" };'),
    ("bool SC_InList( const array<string>@ list, const string& in s )",
     '''// Map-placed studio prop that stays exactly where it's put (item_generic drops to the floor at spawn,
// which fails on the sandbox because its terrain is spawned after the map's entities).
// Keys: model, skin, body, scale, angles, solid ("2" = blocking), minhullsize / maxhullsize ("x y z").
class sc_prop : ScriptBaseEntity
{
	private Vector m_vecHullMin = g_vecZero;
	private Vector m_vecHullMax = g_vecZero;
	private bool m_bSolid = false;

	bool KeyValue( const string& in szKey, const string& in szValue )
	{
		if( szKey == "minhullsize" ) { m_vecHullMin = SC_ParseVector( szValue ); return true; }
		if( szKey == "maxhullsize" ) { m_vecHullMax = SC_ParseVector( szValue ); return true; }
		if( szKey == "solid" ) { m_bSolid = atoi( szValue ) == 2; return true; }
		return BaseClass.KeyValue( szKey, szValue );
	}

	void Precache()
	{
		g_Game.PrecacheModel( string( self.pev.model ) );
	}

	void Spawn()
	{
		Precache();
		g_EntityFuncs.SetModel( self, string( self.pev.model ) );
		self.pev.movetype = MOVETYPE_NONE;
		self.pev.solid = m_bSolid ? SOLID_BBOX : SOLID_NOT;
		self.pev.takedamage = DAMAGE_NO;
		g_EntityFuncs.SetSize( self.pev, m_vecHullMin, m_vecHullMax );
		g_EntityFuncs.SetOrigin( self, self.pev.origin );
	}
}

Vector SC_ParseVector( const string& in s )
{
	array<string> p = SC_Split( s, " " );
	return Vector( p.length() > 0 ? atof( p[0] ) : 0, p.length() > 1 ? atof( p[1] ) : 0, p.length() > 2 ? atof( p[2] ) : 0 );
}

void SC_RegisterProp()
{
	g_CustomEntityFuncs.RegisterCustomEntity( "sc_prop", "sc_prop" );
}

bool SC_InList( const array<string>@ list, const string& in s )'''),
])

patch('svencraft.as', [
    ("	SC_RegisterNode();\n", "	SC_RegisterNode();\n	SC_RegisterProp();\n"),
    ("g_Utility.TraceLine( Vector( 80, 80, 200 ), Vector( 80, 80, -500 ), dont_ignore_monsters, null, tr );\n\t\t@pHit",
     "g_Utility.TraceLine( Vector( 100, 100, 200 ), Vector( 100, 100, -600 ), dont_ignore_monsters, null, tr );\n\t\t@pHit"),
    ('SCTEST terrain at (80,80): ', 'SCTEST terrain at (100,100): '),
    ("g_Utility.TraceLine( Vector( 80, 80, 200 ), Vector( 80, 80, -500 ), dont_ignore_monsters, null, tr );\n\tCBaseEntity@ pHit",
     "g_Utility.TraceLine( Vector( 100, 100, 200 ), Vector( 100, 100, -600 ), dont_ignore_monsters, null, tr );\n\tCBaseEntity@ pHit"),
    ('SCTEST after carve, (80,80) hits: ', 'SCTEST after carve, (100,100) hits: '),
    ('(expect dirt at z=-32)', '(expect dirt at z=-40)'),
    ('g_Utility.TraceLine( Vector( 112, 80, 200 ), Vector( 112, 80, -500 )', 'g_Utility.TraceLine( Vector( 140, 100, 200 ), Vector( 140, 100, -600 )'),
    ('SCTEST neighbour (112,80) hits: ', 'SCTEST neighbour (140,100) hits: '),
    ('bool bClear = SC_BoxClear( Vector( 16, 16, 16 ), 16 );\n\tbool bPlaced = SC_PlaceBlockAt( Vector( 16, 16, 16 ), MAT_BRICK );',
     'bool bClear = SC_BoxClear( Vector( 20, 20, 20 ), 20 );\n\tbool bPlaced = SC_PlaceBlockAt( Vector( 20, 20, 20 ), MAT_BRICK );'),
    ("g_Utility.TraceLine( Vector( 16, 16, 200 ), Vector( 16, 16, -500 ), dont_ignore_monsters, null, tr );\n\t@pHit",
     "g_Utility.TraceLine( Vector( 20, 20, 200 ), Vector( 20, 20, -600 ), dont_ignore_monsters, null, tr );\n\t@pHit"),
    ('SCTEST place brick at (16,16,16): ', 'SCTEST place brick at (20,20,20): '),
    ('(expect brick node, z=32)', '(expect brick node, z=40)'),
    ('while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, "item_generic" ) ) !is null )',
     'while( ( @pEnt = g_EntityFuncs.FindEntityByClassname( pEnt, g_SCSandbox ? "sc_prop" : "item_generic" ) ) !is null )'),
])
print('ok')
