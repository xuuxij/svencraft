// Svencraft block materials. Material id == skin index in models/svencraft/block.mdl.

enum SCMat
{
	MAT_NONE = -1,
	MAT_GRASS = 0,
	MAT_DIRT,
	MAT_STONE,
	MAT_COBBLE,
	MAT_BRICK,
	MAT_PLANKS,
	MAT_LOG,
	MAT_LEAVES,
	MAT_SAND,
	MAT_GRAVEL,
	MAT_SNOW,
	MAT_METAL,
	MAT_CONCRETE,
	MAT_TILE,
	MAT_RUBBER,
	MAT_GLASS,
	MAT_CIRCUIT,
	MAT_VENT,
	MAT_WORKBENCH,
	MAT_IRON_ORE,
	MAT_CRYSTAL_ORE,
	MAT_GLASS_PANE,
	MAT_DOOR,
	MAT_COUNT
}

enum SCTool
{
	TOOL_HAND = 0,
	TOOL_PICKAXE,
	TOOL_SHOVEL,
	TOOL_AXE
}

// Breakage "families" share gib models and sounds.
enum SCFamily
{
	FAM_EARTH = 0,
	FAM_STONE,
	FAM_WOOD,
	FAM_METAL,
	FAM_GLASS,
	FAM_TECH,
	FAM_PLANT,
	FAM_COUNT
}

class SCMaterial
{
	string id;
	string name;
	float hardness;   // swings needed with the right tool ~= hardness * 3 / 4
	int tool;         // SCTool that mines it fast
	int level;        // harvest level: 0-1 = any tool collects it, 2 = stone+ pickaxe, 3 = iron pickaxe
	int drop;         // SCMat added to the inventory when mined
	int family;

	SCMaterial( const string& in _id, const string& in _name, float _hardness, int _tool, int _level, int _drop, int _family )
	{
		id = _id; name = _name; hardness = _hardness; tool = _tool; level = _level; drop = _drop; family = _family;
	}
}

array<SCMaterial@> g_SCMats;

const array<string> SC_FAM_GIBS = { "models/rockgibs.mdl", "models/cindergibs.mdl", "models/woodgibs.mdl", "models/metalplategibs.mdl", "models/glassgibs.mdl", "models/computergibs.mdl", "models/woodgibs.mdl" };
const array<string> SC_FAM_HIT = { "player/pl_dirt1.wav", "debris/concrete1.wav", "debris/wood1.wav", "debris/metal1.wav", "debris/glass1.wav", "debris/metal3.wav", "player/pl_dirt3.wav" };
const array<string> SC_FAM_BREAK = { "player/pl_dirt2.wav", "debris/bustconcrete1.wav", "debris/bustcrate1.wav", "debris/bustmetal1.wav", "debris/bustglass1.wav", "debris/bustmetal2.wav", "debris/bustcrate2.wav" };
// TE_BREAKMODEL flags: BREAK_GLASS 1, BREAK_METAL 2, BREAK_FLESH 4, BREAK_WOOD 8, BREAK_CONCRETE 64
const array<int> SC_FAM_BREAKFLAGS = { 64, 64, 8, 2, 1, 2, 8 };
array<int> g_SCFamGibIndex( FAM_COUNT, 0 );

void SC_InitMaterials()
{
	g_SCMats.resize( 0 );
	//                            id          name           hard  tool          level  drop          family
	g_SCMats.insertLast( SCMaterial( "grass",    "Grass",       0.6, TOOL_SHOVEL,  0,     MAT_DIRT,     FAM_EARTH ) );
	g_SCMats.insertLast( SCMaterial( "dirt",     "Dirt",        0.5, TOOL_SHOVEL,  0,     MAT_DIRT,     FAM_EARTH ) );
	g_SCMats.insertLast( SCMaterial( "stone",    "Stone",       1.5, TOOL_PICKAXE, 1,     MAT_COBBLE,   FAM_STONE ) );
	g_SCMats.insertLast( SCMaterial( "cobble",   "Cobblestone", 2.0, TOOL_PICKAXE, 1,     MAT_COBBLE,   FAM_STONE ) );
	g_SCMats.insertLast( SCMaterial( "brick",    "Bricks",      2.0, TOOL_PICKAXE, 1,     MAT_BRICK,    FAM_STONE ) );
	g_SCMats.insertLast( SCMaterial( "planks",   "Planks",      2.0, TOOL_AXE,     0,     MAT_PLANKS,   FAM_WOOD ) );
	g_SCMats.insertLast( SCMaterial( "log",      "Log",         2.0, TOOL_AXE,     0,     MAT_LOG,      FAM_WOOD ) );
	g_SCMats.insertLast( SCMaterial( "leaves",   "Leaves",      0.2, TOOL_AXE,     0,     MAT_LEAVES,   FAM_PLANT ) );
	g_SCMats.insertLast( SCMaterial( "sand",     "Sand",        0.5, TOOL_SHOVEL,  0,     MAT_SAND,     FAM_EARTH ) );
	g_SCMats.insertLast( SCMaterial( "gravel",   "Gravel",      0.6, TOOL_SHOVEL,  0,     MAT_GRAVEL,   FAM_EARTH ) );
	g_SCMats.insertLast( SCMaterial( "snow",     "Snow",        0.2, TOOL_SHOVEL,  0,     MAT_SNOW,     FAM_EARTH ) );
	g_SCMats.insertLast( SCMaterial( "metal",    "Metal",       5.0, TOOL_PICKAXE, 2,     MAT_METAL,    FAM_METAL ) );
	g_SCMats.insertLast( SCMaterial( "concrete", "Concrete",    1.8, TOOL_PICKAXE, 1,     MAT_CONCRETE, FAM_STONE ) );
	g_SCMats.insertLast( SCMaterial( "tile",     "Tile",        1.5, TOOL_PICKAXE, 1,     MAT_TILE,     FAM_STONE ) );
	g_SCMats.insertLast( SCMaterial( "rubber",   "Rubber",      0.8, TOOL_AXE,     0,     MAT_RUBBER,   FAM_PLANT ) );
	g_SCMats.insertLast( SCMaterial( "glass",    "Glass",       0.3, TOOL_HAND,    0,     MAT_GLASS,    FAM_GLASS ) );
	g_SCMats.insertLast( SCMaterial( "circuit",  "Electronics", 3.0, TOOL_PICKAXE, 2,     MAT_CIRCUIT,  FAM_TECH ) );
	g_SCMats.insertLast( SCMaterial( "vent",     "Vent Grate",  3.0, TOOL_PICKAXE, 2,     MAT_METAL,    FAM_METAL ) );
	g_SCMats.insertLast( SCMaterial( "workbench", "Workbench",  2.5, TOOL_AXE,     0,     MAT_WORKBENCH, FAM_WOOD ) );
	g_SCMats.insertLast( SCMaterial( "iron_ore", "Iron Ore",    3.0, TOOL_PICKAXE, 2,     MAT_METAL,    FAM_STONE ) );
	g_SCMats.insertLast( SCMaterial( "crystal_ore", "Xen Crystal Ore", 4.0, TOOL_PICKAXE, 3, MAT_CIRCUIT, FAM_GLASS ) );
	g_SCMats.insertLast( SCMaterial( "glass_pane", "Glass Pane", 0.3, TOOL_HAND,    0,     MAT_GLASS_PANE, FAM_GLASS ) );
	g_SCMats.insertLast( SCMaterial( "door",     "Wooden Door", 3.0, TOOL_AXE,     0,     MAT_DOOR,     FAM_WOOD ) );

	InitTexMap();
}

void SC_PrecacheMaterials()
{
	for( uint i = 0; i < SC_FAM_GIBS.length(); i++ )
	{
		g_SCFamGibIndex[i] = g_Game.PrecacheModel( SC_FAM_GIBS[i] );
		g_SoundSystem.PrecacheSound( SC_FAM_HIT[i] );
		g_SoundSystem.PrecacheSound( SC_FAM_BREAK[i] );
	}
}

int SC_MatByName( const string& in szName )
{
	string s = szName;
	s = s.ToLowercase();
	for( uint i = 0; i < g_SCMats.length(); i++ )
	{
		string szMatName = g_SCMats[i].name;   // ToLowercase() changes the string in place, so use a copy
		szMatName = szMatName.ToLowercase();
		if( g_SCMats[i].id == s || szMatName == s )
			return int( i );
	}
	return MAT_NONE;
}

// Tool tiers: 0 = none, 1 = wooden, 2 = stone, 3 = iron. Higher tiers mine faster and harvest harder blocks.
const array<string> SC_TIER_NAMES = { "Bare", "Wooden", "Stone", "Iron" };
const array<float> SC_TIER_SPEED = { 1.0, 2.0, 4.0, 6.0 };
const int SC_MAX_TIER = 3;

int SC_ToolTier( CBasePlayer@ pPlayer, int iTool )
{
	if( iTool <= TOOL_HAND || pPlayer is null )
		return 0;
	return SC_Inv( pPlayer ).toolTier[iTool];
}

// How much of a block one swing removes (1.0 = breaks in one swing).
float SC_SwingPower( CBasePlayer@ pPlayer, int iTool, int iMat )
{
	SCMaterial@ m = g_SCMats[iMat];
	bool bRight = m.tool == iTool || m.tool == TOOL_HAND;
	float flMult = bRight ? SC_TIER_SPEED[SC_ToolTier( pPlayer, iTool )] : 1.0;
	return flMult / ( m.hardness * 3.0 );
}

bool SC_ToolCanHarvest( CBasePlayer@ pPlayer, int iTool, int iMat )
{
	SCMaterial@ m = g_SCMats[iMat];
	if( m.level <= 1 )
		return true;
	return m.tool == iTool && SC_ToolTier( pPlayer, iTool ) >= m.level;
}

string SC_HarvestHint( int iMat )
{
	SCMaterial@ m = g_SCMats[iMat];
	return m.name + " needs a " + SC_TIER_NAMES[m.level] + " " + SC_ToolName( m.tool ) + " or better";
}

//
// Texture name -> material
//
dictionary g_SCTexChars;

void InitTexMap()
{
	g_SCTexChars.deleteAll();
	for( uint i = 0; i < SC_TEXNAMES.length(); i++ )
		g_SCTexChars.set( SC_TEXNAMES[i], SC_TEXCHARS.SubString( i, 1 ) );
}

bool SC_Has( const string& in s, const string& in sub )
{
	return s.Find( sub ) != String::INVALID_INDEX;
}

// Returns MAT_NONE for textures that should never yield anything (sky, clip, liquids...).
int SC_MatForTexture( const string& in szTexture )
{
	string tex = szTexture;
	tex = tex.ToUppercase();

	// Strip Half-Life texture prefixes the same way the engine does for material lookup.
	string c0 = tex.SubString( 0, 1 );
	if( tex.Length() > 2 && ( c0 == "-" || c0 == "+" ) )
		tex = tex.SubString( 2 );
	c0 = tex.SubString( 0, 1 );
	if( c0 == "!" )
		return MAT_NONE; // water / liquids
	if( c0 == "{" || c0 == "~" || c0 == " " )
		tex = tex.SubString( 1 );

	if( tex.Length() == 0 || tex.StartsWith( "SKY" ) || tex == "CLIP" || tex == "NULL" || tex == "AAATRIGGER" || tex == "ORIGIN" || tex == "BEVEL" || tex == "HINT" || tex == "SKIP" )
		return MAT_NONE;

	// Svencraft's own block textures (sc_<material>[_top|_side]); bedrock is unbreakable.
	if( tex.StartsWith( "SC_" ) )
	{
		if( tex.StartsWith( "SC_BEDROCK" ) ) return MAT_NONE;
		if( tex.StartsWith( "SC_GRASS" ) ) return MAT_GRASS;
		if( tex.StartsWith( "SC_LOG" ) ) return MAT_LOG;
		string szOwn = tex.SubString( 3 );
		if( szOwn.EndsWith( "_X" ) || szOwn.EndsWith( "_Y" ) || szOwn.EndsWith( "_B" ) )   // shaded side/bottom variants
			szOwn = szOwn.SubString( 0, szOwn.Length() - 2 );
		int iOwn = SC_MatByName( szOwn );
		if( iOwn != MAT_NONE )
			return iOwn;
	}

	// Name keywords first: they describe what the surface *is* better than the sound class.
	if( SC_Has( tex, "GRASS" ) ) return MAT_GRASS;
	if( SC_Has( tex, "LEAF" ) || SC_Has( tex, "LEAVES" ) || SC_Has( tex, "BUSH" ) || SC_Has( tex, "FOLIAGE" ) || SC_Has( tex, "HEDGE" ) ) return MAT_LEAVES;
	if( SC_Has( tex, "SAND" ) ) return MAT_SAND;
	if( SC_Has( tex, "GRAVEL" ) ) return MAT_GRAVEL;
	if( SC_Has( tex, "SNOW" ) ) return MAT_SNOW;
	if( SC_Has( tex, "MUD" ) || SC_Has( tex, "DIRT" ) || SC_Has( tex, "SOIL" ) ) return MAT_DIRT;
	if( SC_Has( tex, "COBBLE" ) ) return MAT_COBBLE;
	if( SC_Has( tex, "BRICK" ) || SC_Has( tex, "BRK" ) ) return MAT_BRICK;
	if( SC_Has( tex, "ROCK" ) || SC_Has( tex, "STONE" ) || SC_Has( tex, "CLIFF" ) || SC_Has( tex, "CAVE" ) ) return MAT_STONE;
	if( SC_Has( tex, "BARK" ) || SC_Has( tex, "TRUNK" ) ) return MAT_LOG;
	if( SC_Has( tex, "WOOD" ) || SC_Has( tex, "PLANK" ) || SC_Has( tex, "CRATE" ) ) return MAT_PLANKS;
	if( SC_Has( tex, "GLASS" ) || SC_Has( tex, "WINDOW" ) ) return MAT_GLASS;
	if( SC_Has( tex, "TIRE" ) || SC_Has( tex, "RUBBER" ) ) return MAT_RUBBER;
	if( SC_Has( tex, "VENT" ) || SC_Has( tex, "DUCT" ) || SC_Has( tex, "GRATE" ) || SC_Has( tex, "GRILL" ) ) return MAT_VENT;
	if( SC_Has( tex, "COMP" ) || SC_Has( tex, "CONSOLE" ) || SC_Has( tex, "MONITOR" ) ) return MAT_CIRCUIT;
	if( SC_Has( tex, "TILE" ) ) return MAT_TILE;
	if( SC_Has( tex, "CONC" ) || SC_Has( tex, "CEMENT" ) ) return MAT_CONCRETE;
	if( SC_Has( tex, "METAL" ) || SC_Has( tex, "STEEL" ) || SC_Has( tex, "IRON" ) || SC_Has( tex, "PIPE" ) || SC_Has( tex, "RUST" ) ) return MAT_METAL;
	if( SC_Has( tex, "DOOR" ) ) return MAT_PLANKS;

	// Fall back to the Half-Life material sound class from materials.txt.
	string key = tex.Length() > 12 ? tex.SubString( 0, 12 ) : tex;
	string c;
	if( !g_SCTexChars.get( key, c ) )
		return MAT_CONCRETE; // engine default material is concrete

	if( c == "M" ) return MAT_METAL;
	if( c == "V" ) return MAT_VENT;
	if( c == "G" ) return MAT_VENT;
	if( c == "D" ) return MAT_DIRT;
	if( c == "T" ) return MAT_TILE;
	if( c == "W" ) return MAT_PLANKS;
	if( c == "P" ) return MAT_CIRCUIT;
	if( c == "Y" ) return MAT_GLASS;
	if( c == "O" ) return MAT_SNOW;
	if( c == "F" ) return MAT_DIRT;
	if( c == "S" ) return MAT_NONE;
	return MAT_CONCRETE;
}
