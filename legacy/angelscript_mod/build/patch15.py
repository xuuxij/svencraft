import os, re
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps, regex=False):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        if regex:
            s, n = re.subn(a, b, s)
            assert n > 0, (name, a)
        else:
            assert a in s, (name, a[:80])
            s = s.replace(a, b)
    open(p, 'w', newline='').write(s)

# ---------------- materials: harvest levels + tool tiers
patch('sc_materials.as', [
    ("	MAT_VENT,\n	MAT_COUNT", "	MAT_VENT,\n	MAT_WORKBENCH,\n	MAT_COUNT"),
    ("""	bool needsTool;   // without the right tool it breaks but drops nothing
	int drop;         // SCMat added to the inventory when mined
	int family;

	SCMaterial( const string& in _id, const string& in _name, float _hardness, int _tool, bool _needsTool, int _drop, int _family )
	{
		id = _id; name = _name; hardness = _hardness; tool = _tool; needsTool = _needsTool; drop = _drop; family = _family;
	}""", """	int level;        // harvest level: 0 = anything, 1 = wooden+ tool of the right type, 2 = stone+, 3 = iron
	int drop;         // SCMat added to the inventory when mined
	int family;

	SCMaterial( const string& in _id, const string& in _name, float _hardness, int _tool, int _level, int _drop, int _family )
	{
		id = _id; name = _name; hardness = _hardness; tool = _tool; level = _level; drop = _drop; family = _family;
	}"""),
    ("//                            id          name           hard  tool          needs  drop          family",
     "//                            id          name           hard  tool          level  drop          family"),
    ('''	g_SCMats.insertLast( SCMaterial( "vent",     "Vent Grate",  3.0, TOOL_PICKAXE, true,  MAT_METAL,    FAM_METAL ) );''',
     '''	g_SCMats.insertLast( SCMaterial( "vent",     "Vent Grate",  3.0, TOOL_PICKAXE, true,  MAT_METAL,    FAM_METAL ) );
	g_SCMats.insertLast( SCMaterial( "workbench", "Workbench",  2.5, TOOL_AXE,     false, MAT_WORKBENCH, FAM_WOOD ) );'''),
    ("""// How much of a block one swing removes (1.0 = breaks in one swing).
float SC_SwingPower( int iTool, int iMat )
{
	SCMaterial@ m = g_SCMats[iMat];
	float flMult = ( m.tool == iTool || m.tool == TOOL_HAND ) ? 4.0 : 1.0;
	return flMult / ( m.hardness * 3.0 );
}

bool SC_ToolCanHarvest( int iTool, int iMat )
{
	SCMaterial@ m = g_SCMats[iMat];
	return !m.needsTool || m.tool == iTool;
}""", """// Tool tiers: 0 = none, 1 = wooden, 2 = stone, 3 = iron. Higher tiers mine faster and harvest harder blocks.
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
	return m.level == 0 || ( m.tool == iTool && SC_ToolTier( pPlayer, iTool ) >= m.level );
}

string SC_HarvestHint( int iMat )
{
	SCMaterial@ m = g_SCMats[iMat];
	return m.name + " needs a " + SC_TIER_NAMES[m.level] + " " + SC_ToolName( m.tool ) + " or better";
}"""),
])
# levels: stone-type blocks need wooden pickaxe (1), metal/vent/electronics need stone pickaxe (2)
p = os.path.join(D, 'sc_materials.as')
s = open(p, newline='').read()
def lvl(matid, level):
    global s
    pat = r'(SCMaterial\( "%s",[^)]*?TOOL_\w+,\s*)(true|false)' % matid
    s, n = re.subn(pat, lambda m_: m_.group(1) + str(level) + ' ' * (5 - len(str(level))), s)
    assert n == 1, matid
for mid in ('grass', 'dirt', 'planks', 'log', 'leaves', 'sand', 'gravel', 'snow', 'rubber', 'glass', 'workbench'):
    lvl(mid, 0)
for mid in ('stone', 'cobble', 'brick', 'concrete', 'tile'):
    lvl(mid, 1)
for mid in ('metal', 'circuit', 'vent'):
    lvl(mid, 2)
open(p, 'w', newline='').write(s)

# ---------------- call sites
for f in ('sc_blocks.as', 'sc_sandbox.as', 'sc_props.as'):
    p = os.path.join(D, f)
    s = open(p, newline='').read()
    s = s.replace('SC_SwingPower( iTool', 'SC_SwingPower( pPlayer, iTool').replace('SC_ToolCanHarvest( iTool', 'SC_ToolCanHarvest( pPlayer, iTool')
    open(p, 'w', newline='').write(s)
patch('sc_blocks.as', [
    ('''		SC_Print( pPlayer, m.name + " needs a " + SC_ToolName( m.tool ) + " to collect" );''',
     '''		SC_Print( pPlayer, SC_HarvestHint( iMat ) );'''),
])
patch('sc_props.as', [
    ('''		SC_Print( pPlayer, g_SCMats[mats[0]].name + " needs a " + SC_ToolName( g_SCMats[mats[0]].tool ) + " to collect" );''',
     '''		SC_Print( pPlayer, SC_HarvestHint( mats[0] ) );'''),
])

# ---------------- inventory: per-tool tiers (everyone starts with wooden tools)
patch('sc_inventory.as', [
    ("""	SCInventory()
	{
		count.resize( MAT_COUNT );""", """	array<int> toolTier = { 0, 1, 1, 1 };   // by SCTool: hand, pickaxe, shovel, axe (start wooden)
	CTextMenu@ craftMenu = null;

	SCInventory()
	{
		count.resize( MAT_COUNT );"""),
])

# ---------------- hotbar frame layout follows the material count
patch('sc_hud.as', [
    ("const int SC_HOTBAR_EMPTY = 18;\nconst int SC_HOTBAR_SELECTED = 19;", "const int SC_HOTBAR_EMPTY = MAT_COUNT;           // frames: 0..MAT_COUNT-1 blocks, then empty\nconst int SC_HOTBAR_SELECTED = MAT_COUNT + 1;    // selected variants start here"),
    ('''	inv.menu.SetTitle( "Inventory" + ( inv.creative ? " (creative)" : "" ) + " - pick a block" );
	int iAdded = 0;''', '''	inv.menu.SetTitle( "Inventory" + ( inv.creative ? " (creative)" : "" ) + " - pick a block" );
	inv.menu.AddItem( "Crafting...", any( -2 ) );
	int iAdded = 0;'''),
    ('''	int iMat = -1;
	pItem.m_pUserData.retrieve( iMat );
	if( iMat < 0 || iMat >= MAT_COUNT )
		return;''', '''	int iMat = -1;
	pItem.m_pUserData.retrieve( iMat );
	if( iMat == -2 )
	{
		SC_OpenCrafting( pPlayer );
		return;
	}
	if( iMat < 0 || iMat >= MAT_COUNT )
		return;'''),
])

# ---------------- tools: tiered models, all three on the crowbar rig
patch('sc_tools.as', [
    ("""	bool Deploy()
	{
		bool bResult = self.DefaultDeploy( self.GetV_Model( m_szVModel ), self.GetP_Model( m_szPModel ), AnimDraw(), "crowbar" );""",
     """	string TierModel( const string& in szPrefix )
	{
		return "models/svencraft/" + szPrefix + "_sc" + SC_ToolName( m_iTool ) + SC_ToolTier( m_pPlayer, m_iTool ) + ".mdl";
	}

	// Called after crafting a better tool while holding this one.
	void RefreshModels()
	{
		m_pPlayer.pev.viewmodel = TierModel( "v" );
		m_pPlayer.pev.weaponmodel = TierModel( "p" );
	}

	bool Deploy()
	{
		SC_HudText( m_pPlayer, SC_TIER_NAMES[SC_ToolTier( m_pPlayer, m_iTool )] + " " + SC_ToolName( m_iTool ) );
		bool bResult = self.DefaultDeploy( TierModel( "v" ), TierModel( "p" ), AnimDraw(), "crowbar" );"""),
    ("""		self.PrecacheCustomModels();
		g_Game.PrecacheModel( m_szVModel );
		g_Game.PrecacheModel( m_szPModel );
		g_Game.PrecacheModel( m_szWModel );""", """		self.PrecacheCustomModels();
		for( int t = 1; t <= SC_MAX_TIER; t++ )
		{
			g_Game.PrecacheModel( "models/svencraft/v_sc" + SC_ToolName( m_iTool ) + t + ".mdl" );
			g_Game.PrecacheModel( "models/svencraft/p_sc" + SC_ToolName( m_iTool ) + t + ".mdl" );
			g_Game.PrecacheModel( "models/svencraft/w_sc" + SC_ToolName( m_iTool ) + t + ".mdl" );
		}"""),
    ("""		g_EntityFuncs.SetModel( self, self.GetW_Model( m_szWModel ) );""",
     """		g_EntityFuncs.SetModel( self, "models/svencraft/w_sc" + SC_ToolName( m_iTool ) + "1.mdl" );"""),
])
p = os.path.join(D, 'sc_tools.as')
s = open(p, newline='').read()
s = s.replace('''		m_szVModel = "models/hunger/v_shovel.mdl";
		m_szPModel = "models/hunger/p_shovel.mdl";
		m_szWModel = "models/hunger/w_shovel.mdl";''', '''		m_szVModel = "models/svencraft/v_scshovel1.mdl";
		m_szPModel = "models/svencraft/p_scshovel1.mdl";
		m_szWModel = "models/svencraft/w_scshovel1.mdl";''')
s = s.replace('''		m_iPosition = 8;
		m_flDamage = 14;
		m_iAnimSet = ANIMSET_SHOVEL;''', '''		m_iPosition = 8;
		m_flDamage = 14;
		m_iAnimSet = ANIMSET_CROWBAR;''')
s += '''
// Swap the held tool's models after a tier upgrade.
void SC_RefreshHeldTool( CBasePlayer@ pPlayer )
{
	CBasePlayerItem@ pItem = pPlayer.m_hActiveItem.GetEntity() is null ? null : cast<CBasePlayerItem@>( pPlayer.m_hActiveItem.GetEntity() );
	if( pItem is null )
		return;
	SCToolWeapon@ pTool = cast<SCToolWeapon@>( CastToScriptClass( pItem ) );
	if( pTool !is null )
		pTool.RefreshModels();
}
'''
open(p, 'w', newline='').write(s)

# ---------------- workbench blocks open crafting on +use
for f, cls in (('sc_sandbox.as', 'class sc_node : ScriptBaseEntity\n{'), ('sc_blocks.as', 'class sc_block : ScriptBaseEntity\n{')):
    patch(f, [(cls, cls + '''
	int ObjectCaps()
	{
		if( self.pev.iuser1 == MAT_WORKBENCH )
			return BaseClass.ObjectCaps() | FCAP_IMPULSE_USE;
		return BaseClass.ObjectCaps();
	}

	void Use( CBaseEntity@ pActivator, CBaseEntity@ pCaller, USE_TYPE useType, float flValue )
	{
		if( self.pev.iuser1 == MAT_WORKBENCH && pActivator !is null && pActivator.IsPlayer() )
			SC_OpenCrafting( cast<CBasePlayer@>( pActivator ) );
	}
''')])
print('ok')
