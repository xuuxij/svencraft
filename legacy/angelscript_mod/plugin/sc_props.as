// Mining things that aren't blocks: props/decorations (mined whole, drop a mix of materials) and
// brush structures like walls and doors (broken into blocks sized to their thickness on the first hit).

enum SCKind
{
	KIND_NONE = 0,     // nothing minable (pickups, sprites, invisible clips...)
	KIND_BLOCK,        // sc_block
	KIND_NODE,         // sc_node (sandbox terrain)
	KIND_CREATURE,     // living players/monsters: normal melee damage
	KIND_PROP,         // mined whole
	KIND_BREAKABLE,    // func_breakable etc.: normal damage, drops materials when it breaks
	KIND_WALL,         // brush structure: broken into blocks
	KIND_SURFACE       // world or protected brush (buttons, trains, ladders...): harvest only
}

const int SC_SHAPE_CUBE = 0, SC_SHAPE_SLAB = 1, SC_SHAPE_PLATE = 2, SC_SHAPE_SHEET = 3;
const array<float> SC_SHAPE_THICKNESS = { 40, 16, 8, 4 };
const int SC_SUBDIV_MAX = 96;     // most pieces one structure may break into; bigger ones are harvest-only
const float SC_PROP_MAX_DROP = 12;
const float SC_PROP_EARTH_MAX = 8;       // earth/stone props bigger than this (in blocks) are terrain
const float SC_PROP_REMOVE_MAX = 1500;   // nothing bigger than this is ever removed

// Studio-model decorations and brush entities that are mined as one object.
const array<string> SC_PROP_CLASSES = { "item_generic", "monster_furniture", "cycler", "cycler_sprite", "cycler_wreckage", "sc_prop" };
const array<string> SC_BRUSH_PROP_CLASSES = { "func_illusionary", "func_pushable", "func_healthcharger", "func_recharge", "func_rotating" };
// Brush entities the map needs to stay playable: never removed, only harvested.
const array<string> SC_PROTECTED_CLASSES = { "func_button", "func_rot_button", "func_train", "func_tracktrain", "func_plat", "func_platrot",
	"func_vehicle", "func_vehicle_custom", "func_ladder", "func_water", "func_monsterclip", "func_tank", "func_tanklaser", "func_tankrocket",
	"func_tankmortar", "func_tankcontrols", "func_guntarget", "momentary_door", "momentary_rot_button", "func_trackchange",
	"func_trackautochange", "func_mortar_field", "func_clip", "func_conveyor", "func_friction" };

// Map-placed studio prop that stays exactly where it's put (item_generic drops to the floor at spawn,
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
		// The engine may consume the "solid" key itself (it's an entvars field), so honour both.
		self.pev.solid = ( m_bSolid || self.pev.solid == SOLID_BBOX ) ? SOLID_BBOX : SOLID_NOT;
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

bool SC_InList( const array<string>@ list, const string& in s )
{
	return list.find( s ) >= 0;
}

int SC_KindOf( CBaseEntity@ pEnt )
{
	if( pEnt is null || pEnt.entindex() == 0 )
		return KIND_SURFACE;
	string szClass = pEnt.GetClassname();
	if( szClass == "sc_block" ) return KIND_BLOCK;
	if( szClass == "sc_node" ) return KIND_NODE;
	if( SC_InList( SC_PROP_CLASSES, szClass ) ) return KIND_PROP;
	if( pEnt.IsPlayer() || pEnt.IsMonster() ) return KIND_CREATURE;
	if( !pEnt.IsBSPModel() ) return KIND_NONE;
	if( ( pEnt.pev.effects & EF_NODRAW ) != 0 || ( pEnt.pev.rendermode != kRenderNormal && pEnt.pev.renderamt <= 0 ) )
		return KIND_NONE;   // invisible clip brushes
	if( szClass.StartsWith( "trigger_" ) || SC_InList( SC_PROTECTED_CLASSES, szClass ) ) return KIND_SURFACE;
	if( pEnt.pev.takedamage != DAMAGE_NO && ( szClass == "func_breakable" || szClass == "func_pushable" ) ) return KIND_BREAKABLE;
	if( SC_InList( SC_BRUSH_PROP_CLASSES, szClass ) ) return KIND_PROP;
	if( szClass.StartsWith( "func_" ) ) return KIND_WALL;
	return KIND_SURFACE;
}

//
// Picking decorations a normal trace passes through (non-solid, or solid with no collision box)
//
bool SC_RayBox( const Vector& in o, const Vector& in d, const Vector& in mn, const Vector& in mx, float& out t )
{
	float tmin = 0, tmax = 1e9;
	for( int i = 0; i < 3; i++ )
	{
		if( SC_Abs( d[i] ) < 1e-6 )
		{
			if( o[i] < mn[i] || o[i] > mx[i] )
				return false;
			continue;
		}
		float t1 = ( mn[i] - o[i] ) / d[i], t2 = ( mx[i] - o[i] ) / d[i];
		if( t1 > t2 ) { float tmp = t1; t1 = t2; t2 = tmp; }
		if( t1 > tmin ) tmin = t1;
		if( t2 < tmax ) tmax = t2;
		if( tmin > tmax )
			return false;
	}
	t = tmin;
	return true;
}

void SC_EntBox( CBaseEntity@ pEnt, Vector& out mn, Vector& out mx )
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
	{
		// Point-sized decorations: assume a small box around the origin.
		mn = pEnt.pev.origin + Vector( -12, -12, 0 );
		mx = pEnt.pev.origin + Vector( 12, 12, 24 );
	}
}

CBaseEntity@ SC_FindDecor( const Vector& in vecSrc, const Vector& in vecDir, float flMaxDist, float& out flDist )
{
	CBaseEntity@ pBest = null;
	float flBest = flMaxDist;
	CBaseEntity@ pEnt = null;
	while( ( @pEnt = g_EntityFuncs.FindEntityInSphere( pEnt, vecSrc, flMaxDist + 256, "*", "classname" ) ) !is null )
	{
		// Prop models often have no collision box (or a zero-size one), so test them all against their
		// model bounds; anything solid in front is still caught by the normal trace distance.
		string szClass = pEnt.GetClassname();
		if( !SC_InList( SC_PROP_CLASSES, szClass ) && szClass != "func_illusionary" )
			continue;
		if( ( pEnt.pev.effects & EF_NODRAW ) != 0 || ( pEnt.pev.rendermode != kRenderNormal && pEnt.pev.renderamt <= 0 ) )
			continue;
		Vector mn, mx;
		SC_EntBox( pEnt, mn, mx );
		float t;
		if( SC_RayBox( vecSrc, vecDir, mn, mx, t ) && t < flBest )
		{
			flBest = t;
			@pBest = pEnt;
		}
	}
	flDist = flBest;
	return pBest;
}

//
// What a prop is made of
//
array<array<string>> g_SCMixKeys;
array<array<int>> g_SCMixMats;
array<array<float>> g_SCMixWeights;

// Model-name keywords -> material mix. First match wins, so specific rules come first.
const array<string> SC_MIX_RULES = {     // keep in sync with RULES in genmodelinfo.py
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
	"bone|skull|skeleton|rib|gib|pelvis|corpse|body|flesh=" };

dictionary g_SCModelIndex;   // studio model path -> index into SC_MODEL_* tables (sc_modelinfo.as)

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

// Fills mats/weights. Studio props use their model name; brush props use the texture under the crosshair.
void SC_PropMix( CBaseEntity@ pEnt, int iTexMat, array<int>& out mats, array<float>& out weights )
{
	mats.resize( 0 ); weights.resize( 0 );
	if( pEnt.IsBSPModel() )
	{
		mats.insertLast( iTexMat == MAT_NONE ? MAT_CONCRETE : iTexMat );
		weights.insertLast( 1 );
		return;
	}
	int iModel = SC_ModelIndex( pEnt );
	if( iModel >= 0 && SC_MODEL_MIX[iModel] != "?" )
	{
		SC_ParseMix( SC_MODEL_MIX[iModel], mats, weights );
		return;
	}
	string szModel = string( pEnt.pev.model );
	szModel = szModel.ToLowercase();
	for( uint r = 0; r < g_SCMixKeys.length(); r++ )
	{
		for( uint k = 0; k < g_SCMixKeys[r].length(); k++ )
		{
			if( SC_Has( szModel, g_SCMixKeys[r][k] ) )
			{
				mats = g_SCMixMats[r];
				weights = g_SCMixWeights[r];
				return;
			}
		}
	}
	mats.insertLast( MAT_METAL );
	weights.insertLast( 1 );
}

float SC_BoxBlocks( const Vector& in vecSize )
{
	return vecSize.x * vecSize.y * vecSize.z / ( SC_BLOCK_SIZE * SC_BLOCK_SIZE * SC_BLOCK_SIZE );
}

int SC_TextureMat( CBaseEntity@ pEnt, const Vector& in vecSrc, const Vector& in vecEnd )
{
	if( pEnt is null || !pEnt.IsBSPModel() )
		return MAT_NONE;
	return SC_MatForTexture( g_Utility.TraceTexture( pEnt.edict(), vecSrc, vecEnd ) );
}

void SC_GiveMix( CBasePlayer@ pPlayer, int iTool, const array<int>@ mats, const array<float>@ weights, float flAmount, const Vector& in vecPos )
{
	if( mats.length() == 0 )
		return;
	if( !SC_ToolCanHarvest( pPlayer, iTool, mats[0] ) )
	{
		SC_Print( pPlayer, SC_HarvestHint( mats[0] ) );
		g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/wpn_denyselect.wav", 1, ATTN_NORM );
		return;
	}
	float flTotal = 0;
	for( uint i = 0; i < weights.length(); i++ )
		flTotal += weights[i];
	for( uint i = 0; i < mats.length(); i++ )
		SC_SpawnDrops( vecPos, g_SCMats[mats[i]].drop, flAmount * weights[i] / flTotal );
}

bool SC_IsHugeProp( int iMain, float flVolume )
{
	bool bEarth = iMain == MAT_COBBLE || iMain == MAT_STONE || iMain == MAT_DIRT
		|| iMain == MAT_GRAVEL || iMain == MAT_SNOW || iMain == MAT_GRASS || iMain == MAT_CONCRETE;
	return ( bEarth && flVolume > SC_PROP_EARTH_MAX ) || flVolume > SC_PROP_REMOVE_MAX;
}

// One swing at a prop: it breaks after enough hits and drops its material mix.
void SC_HitProp( CBasePlayer@ pPlayer, CBaseEntity@ pEnt, int iTool, const Vector& in vecSrc, const Vector& in vecEnd, const Vector& in vecHit, const Vector& in vecNormal )
{
	array<int> mats;
	array<float> weights;
	SC_PropMix( pEnt, SC_TextureMat( pEnt, vecSrc, vecEnd ), mats, weights );
	int iMain = mats.length() > 0 ? mats[0] : MAT_PLANKS;
	SCMaterial@ m = g_SCMats[iMain];

	Vector mn, mx;
	SC_EntBox( pEnt, mn, mx );
	float flVolume = SC_BoxBlocks( mx - mn );
	if( mats.length() > 0 && SC_IsHugeProp( iMain, flVolume ) )
	{
		// Rock formations, dunes, buildings: part of the level, so harvest them without removing.
		SC_HarvestCell( pPlayer, iTool, iMain, "prop" + pEnt.entindex() + ":" + SC_CellKey( vecHit ), vecHit, vecNormal );
		return;
	}
	float flAmount = flVolume < 1 ? 1 : ( flVolume > SC_PROP_MAX_DROP ? SC_PROP_MAX_DROP : float( int( flVolume + 0.5 ) ) );

	// Bigger objects take longer: a chair is ~2 axe swings, a car ~20 pickaxe swings.
	float flHits = ( 1 + flAmount * 0.5 ) / SC_SwingPower( pPlayer, iTool, iMain );
	flHits = flHits < 1 ? 1 : ( flHits > 20 ? 20 : flHits );

	SCInventory@ inv = SC_Inv( pPlayer );
	string szKey = "prop:" + pEnt.entindex() + ":" + pEnt.GetClassname();
	if( inv.mineKey != szKey )
	{
		inv.mineKey = szKey;
		inv.mineProgress = 0;
	}
	inv.mineProgress += 1.0 / flHits;
	SC_HitSound( pPlayer, m.family );

	if( inv.mineProgress < 0.999 )
	{
		SC_Debris( vecHit, 8, m.family, 2 );
		if( pEnt.IsBSPModel() )
			SC_ShowCrack( pPlayer, vecHit, vecNormal, SC_BLOCK_SIZE, inv.mineProgress );   // brush props: crack overlay
		else
			SC_ShowGlow( pPlayer, pEnt, inv.mineProgress );                               // studio props: glow shell
		return;
	}
	inv.mineKey = "";
	inv.mineProgress = 0;
	SC_ClearFeedback( pPlayer );

	Vector vecCenter = ( mn + mx ) * 0.5;
	Vector vecSize = mx - mn;
	float flDebris = vecSize.x > vecSize.y ? vecSize.x : vecSize.y;
	SC_Debris( vecCenter, flDebris > 64 ? 64 : flDebris, m.family, 10 );
	SC_BreakSound( vecCenter, m.family );
	SC_MarkGone( pEnt );
	g_EntityFuncs.Remove( pEnt );
	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount, vecCenter );
}

//
// Breaking brush structures into blocks
//
// True when point v is inside the brush model of pEnt (a trace starting inside a brush reports that brush).
bool SC_PointInBrush( CBaseEntity@ pEnt, const Vector& in v )
{
	TraceResult tr;
	g_Utility.TraceLine( v, v + Vector( 0, 0, 0.5 ), ignore_monsters, null, tr );
	return tr.fStartSolid != 0 && g_EntityFuncs.Instance( tr.pHit ) is pEnt;
}

CBaseEntity@ SC_SpawnShaped( const Vector& in vecCenter, const Vector& in vecHalf, int iMat, int iShape, int iThinAxis, float flScale )
{
	// The model's thin axis is local Z; rotate it onto the structure's thin axis.
	Vector vecAngles = g_vecZero;
	if( iShape != SC_SHAPE_CUBE && iThinAxis == 0 ) vecAngles = Vector( 90, 0, 0 );
	if( iShape != SC_SHAPE_CUBE && iThinAxis == 1 ) vecAngles = Vector( 0, 0, 90 );
	CBaseEntity@ pEnt = g_EntityFuncs.Create( "sc_block", vecCenter, vecAngles, true );
	if( pEnt is null )
		return null;
	pEnt.pev.iuser1 = iMat;
	pEnt.pev.body = iShape;
	pEnt.pev.scale = flScale;
	pEnt.pev.vuser1 = vecHalf;
	g_EntityFuncs.DispatchSpawn( pEnt.edict() );
	g_SCBlockCount++;
	return pEnt;
}

int SC_RoundCount( float flLen, float flCell )
{
	int n = int( flLen / flCell + 0.5 );
	return n < 1 ? 1 : n;
}

// Replaces a brush structure with blocks of material iMat sized to its thickness.
// Returns KIND_WALL when done, KIND_PROP when it's too small/rotated to split (mine it whole),
// KIND_SURFACE when it's too big, KIND_NONE when the block limit is reached.
int SC_BreakIntoBlocks( CBaseEntity@ pEnt, int iMat, array<CBaseEntity@>@ pieces )
{
	if( pEnt.pev.angles != g_vecZero )
		return KIND_PROP;
	Vector mn = pEnt.pev.origin + pEnt.pev.mins, mx = pEnt.pev.origin + pEnt.pev.maxs;
	Vector d = mx - mn;
	if( d.x < 24 && d.y < 24 && d.z < 24 )
		return KIND_PROP;

	int a = 0;
	if( d.y < d[a] ) a = 1;
	if( d.z < d[a] ) a = 2;
	float t = d[a];
	int iShape = t >= 24 ? SC_SHAPE_CUBE : ( t >= 12 ? SC_SHAPE_SLAB : ( t >= 6 ? SC_SHAPE_PLATE : SC_SHAPE_SHEET ) );

	array<int> n( 3 );
	for( int i = 0; i < 3; i++ )
		n[i] = ( i == a && iShape != SC_SHAPE_CUBE ) ? 1 : SC_RoundCount( d[i], SC_BLOCK_SIZE );
	int iCount = n[0] * n[1] * n[2];
	if( iCount > SC_SUBDIV_MAX )
		return KIND_SURFACE;
	if( g_SCBlockCount + iCount > SC_MAX_BLOCKS )
		return KIND_NONE;

	Vector c( d.x / n[0], d.y / n[1], d.z / n[2] );
	// Fit the 32-unit model face to the cell size.
	float flFace = 0;
	int iFaces = 0;
	for( int i = 0; i < 3; i++ )
	{
		if( i == a && iShape != SC_SHAPE_CUBE )
			continue;
		flFace += c[i];
		iFaces++;
	}
	float flScale = ( flFace / iFaces ) / SC_BLOCK_SIZE;

	for( int i = 0; i < n[0]; i++ )
		for( int j = 0; j < n[1]; j++ )
			for( int k = 0; k < n[2]; k++ )
			{
				Vector vecCenter = mn + Vector( ( i + 0.5 ) * c.x, ( j + 0.5 ) * c.y, ( k + 0.5 ) * c.z );
				if( !SC_PointInBrush( pEnt, vecCenter ) )
					continue;
				CBaseEntity@ pPiece = SC_SpawnShaped( vecCenter, c * 0.5, iMat, iShape, a, flScale );
				if( pPiece !is null )
					pieces.insertLast( pPiece );
			}
	if( pieces.length() == 0 )
		return KIND_PROP;
	SC_MarkGone( pEnt );
	g_EntityFuncs.Remove( pEnt );
	return KIND_WALL;
}

// First tool hit on a brush structure: break it into blocks and land this swing on the piece under the crosshair.
int SC_HitWall( CBasePlayer@ pPlayer, CBaseEntity@ pEnt, int iTool, TraceResult& in tr, const Vector& in vecSrc, const Vector& in vecEnd )
{
	int iMat = SC_TextureMat( pEnt, vecSrc, vecEnd );
	if( iMat == MAT_NONE )
		iMat = MAT_CONCRETE;
	array<CBaseEntity@> pieces;
	int iResult = SC_BreakIntoBlocks( pEnt, iMat, pieces );
	if( iResult == KIND_NONE )
	{
		SC_Print( pPlayer, "Block limit reached (" + SC_MAX_BLOCKS + ")" );
		return KIND_WALL;
	}
	if( iResult != KIND_WALL )
		return iResult;
	SC_BreakSound( tr.vecEndPos, g_SCMats[iMat].family );

	// This swing lands on the piece under the crosshair.
	Vector vecInside = tr.vecEndPos - tr.vecPlaneNormal * 1.0;
	for( uint p = 0; p < pieces.length(); p++ )
	{
		Vector pmn = pieces[p].pev.origin + pieces[p].pev.mins, pmx = pieces[p].pev.origin + pieces[p].pev.maxs;
		if( vecInside.x >= pmn.x && vecInside.x <= pmx.x && vecInside.y >= pmn.y && vecInside.y <= pmx.y && vecInside.z >= pmn.z && vecInside.z <= pmx.z )
		{
			SC_HitBlock( pPlayer, pieces[p], iTool, tr.vecPlaneNormal );
			break;
		}
	}
	return KIND_WALL;
}

// Called after normal damage was applied to a breakable: if it broke, the player collects its materials.
void SC_CheckBreakable( CBasePlayer@ pPlayer, EHandle hEnt, int iTool, int iTexMat, Vector vecSize, Vector vecCenter )
{
	CBaseEntity@ pEnt = hEnt.GetEntity();
	bool bBroken = pEnt is null || pEnt.pev.health <= 0 || pEnt.pev.solid == SOLID_NOT || ( pEnt.pev.effects & EF_NODRAW ) != 0;
	if( !bBroken )
		return;
	SC_MarkGone( pEnt );
	array<int> mats = { iTexMat == MAT_NONE ? MAT_PLANKS : iTexMat };
	array<float> weights = { 1 };
	float flAmount = SC_BoxBlocks( vecSize );
	flAmount = flAmount < 0.25 ? 0.25 : ( flAmount > SC_PROP_MAX_DROP ? SC_PROP_MAX_DROP : flAmount );
	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount, vecCenter );
}
