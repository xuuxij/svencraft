import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"
p = os.path.join(D, 'sc_props.as')
s = open(p, newline='').read()

# 1. keep runtime rules in sync with genmodelinfo.py
old_rules_start = s.index('const array<string> SC_MIX_RULES = {')
old_rules_end = s.index('};', old_rules_start) + 2
new_rules = '''const array<string> SC_MIX_RULES = {     // keep in sync with RULES in genmodelinfo.py
	"tire|tyre|wheel=rubber:1",
	"tree|palm|trunk|stump=log:2,leaves:1",
	"bush|fern|plant|flower|leaf|leaves|grass|vegit|veget|shrub|hedge|ivy|vine|cactus|weed=leaves:1",
	"rock|stone|boulder|cliff|pebble=cobble:1",
	"ground|dirt|mud|soil|terrain=dirt:1",
	"fungus|mushroom|moss=leaves:1",
	"sandbag|dune|sand=sand:1",
	"brick=brick:1",
	"computer|monitor|television|console|phone|radio|keyboard|server|terminal|screen|holo|machine|panel|electr|camera|speaker|arcade|charger|recharge=circuit:2,glass:1,metal:1",
	"lamp|light|bulb|lantern|chandelier=glass:1,metal:1",
	"bottle|glass|window|jar=glass:1",
	"vase|pot|ceramic|toilet|sink|urinal|bathtub|mug|cup|plate|bowl=tile:1",
	"chair|stool|bench|table|desk|shelf|cabinet|crate|pallet|bed|bookcase|book|wood|plank|door|fence|awning|piano|coffin|sign|easel|sofa|couch|carpet|curtain|tent|flag|cloth|fabric=planks:1",
	"car|truck|jeep|bus|taxi|sedan|vehicle|humvee|apc|forklift|tractor|motorbike|heli|osprey|plane|apache|chopper|blackhawk|boat|wagon=metal:3,rubber:1,glass:1",
	"barrel|drum|locker|oxygen|cylinder|pipe|fan|metal|steel|iron|rail|pole|tank|generator|dumpster|mailbox|hydrant|vent|beam|girder|cage|grate|turret|gun|chrome|rust|toolbox|tool|spray|bomb|grenade|ammo|weapon=metal:1",
	"barrier|jersey|concrete|cement|pillar|column|statue|wall=concrete:1",
	"snow|ice=snow:1",
	"tile=tile:1",
	"bone|skull|skeleton|rib|gib|pelvis|corpse|body|flesh=" };'''
s = s[:old_rules_start] + new_rules + s[old_rules_end:]

# 2. shared mix parser + model table index
old_init = s[s.index('void SC_InitMixRules()'):s.index('// Fills mats/weights.')]
new_init = '''dictionary g_SCModelIndex;   // studio model path -> index into SC_MODEL_* tables (sc_modelinfo.as)

void SC_ParseMix( const string& in szMix, array<int>& out mats, array<float>& out weights )
{
	mats.resize( 0 ); weights.resize( 0 );
	if( szMix == "" )
		return;
	array<string> parts = SC_Split( szMix, "," );
	for( uint p = 0; p < parts.length(); p++ )
	{
		array<string> mw = SC_Split( parts[p], ":" );
		if( mw.length() < 2 )
			continue;
		int iMat = SC_MatByName( mw[0] );
		if( iMat == MAT_NONE )
			continue;
		mats.insertLast( iMat );
		weights.insertLast( atof( mw[1] ) );
	}
}

void SC_InitMixRules()
{
	g_SCMixKeys.resize( 0 ); g_SCMixMats.resize( 0 ); g_SCMixWeights.resize( 0 );
	for( uint i = 0; i < SC_MIX_RULES.length(); i++ )
	{
		array<string> halves = SC_Split( SC_MIX_RULES[i], "=" );
		array<int> mats;
		array<float> weights;
		SC_ParseMix( halves.length() > 1 ? halves[1] : "", mats, weights );
		g_SCMixKeys.insertLast( SC_Split( halves[0], "|" ) );
		g_SCMixMats.insertLast( mats );
		g_SCMixWeights.insertLast( weights );
	}
	g_SCModelIndex.deleteAll();
	for( uint i = 0; i < SC_MODEL_NAMES.length(); i++ )
		g_SCModelIndex.set( SC_MODEL_NAMES[i], int64( i ) );
}

int SC_ModelIndex( CBaseEntity@ pEnt )
{
	string szModel = string( pEnt.pev.model );
	szModel = szModel.ToLowercase();
	int64 i = -1;
	if( !g_SCModelIndex.get( szModel, i ) )
		return -1;
	return int( i );
}

'''
s = s.replace(old_init, new_init)

# 3. table-driven mix for studio props
old_mix = '''	string szModel = string( pEnt.pev.model );
	szModel = szModel.ToLowercase();
	for( uint r = 0; r < g_SCMixKeys.length(); r++ )'''
new_mix = '''	int iModel = SC_ModelIndex( pEnt );
	if( iModel >= 0 && SC_MODEL_MIX[iModel] != "?" )
	{
		SC_ParseMix( SC_MODEL_MIX[iModel], mats, weights );
		return;
	}
	string szModel = string( pEnt.pev.model );
	szModel = szModel.ToLowercase();
	for( uint r = 0; r < g_SCMixKeys.length(); r++ )'''
assert old_mix in s
s = s.replace(old_mix, new_mix)

# 4. real bounds for studio props (scaled + rotated)
old_box = '''void SC_EntBox( CBaseEntity@ pEnt, Vector& out mn, Vector& out mx )
{
	mn = pEnt.pev.absmin;
	mx = pEnt.pev.absmax;
	if( ( mx - mn ).Length() < 4 )
	{'''
new_box = '''void SC_EntBox( CBaseEntity@ pEnt, Vector& out mn, Vector& out mx )
{
	mn = pEnt.pev.absmin;
	mx = pEnt.pev.absmax;
	int iModel = ( mx - mn ).Length() < 4 ? SC_ModelIndex( pEnt ) : -1;
	if( iModel >= 0 )
	{
		// World box of the model's bounds, scaled and rotated like the entity.
		float flScale = pEnt.pev.scale > 0 ? pEnt.pev.scale : 1.0;
		Vector f, r, u;
		g_EngineFuncs.AngleVectors( pEnt.pev.angles, f, r, u );
		mn = Vector( 1e9, 1e9, 1e9 );
		mx = Vector( -1e9, -1e9, -1e9 );
		for( int c = 0; c < 8; c++ )
		{
			float x = SC_MODEL_BOUNDS[iModel * 6 + ( ( c & 1 ) != 0 ? 3 : 0 )] * flScale;
			float y = SC_MODEL_BOUNDS[iModel * 6 + ( ( c & 2 ) != 0 ? 4 : 1 )] * flScale;
			float z = SC_MODEL_BOUNDS[iModel * 6 + ( ( c & 4 ) != 0 ? 5 : 2 )] * flScale;
			Vector w = pEnt.pev.origin + f * x - r * y + u * z;
			for( int i = 0; i < 3; i++ )
			{
				if( w[i] < mn[i] ) mn[i] = w[i];
				if( w[i] > mx[i] ) mx[i] = w[i];
			}
		}
		return;
	}
	if( ( mx - mn ).Length() < 4 )
	{'''
assert old_box in s
s = s.replace(old_box, new_box)
open(p, 'w', newline='').write(s)

m = os.path.join(D, 'svencraft.as')
t = open(m, newline='').read()
if '#include "sc_modelinfo"' not in t:
    t = t.replace('#include "sc_props"\n', '#include "sc_modelinfo"\n#include "sc_props"\n')
open(m, 'w', newline='').write(t)
print('ok', '#include "sc_modelinfo"' in t)
