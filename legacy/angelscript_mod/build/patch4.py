import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

# sc_blocks.as: split SC_HitSurface into a reusable "harvest one cell" core
p = os.path.join(D, 'sc_blocks.as')
s = open(p, newline='').read()
start = s.index('// One swing against non-removable geometry')
end = s.index('// Place the player\'s selected block')
new = '''// One swing at something that can't be removed (world, big brush entities, terrain-sized props):
// each 32-unit cell of it yields one block of iMat and is then used up.
void SC_HarvestCell( CBasePlayer@ pPlayer, int iTool, int iMat, const string& in szKey, const Vector& in vecPos )
{
	SCMaterial@ m = g_SCMats[iMat];
	SC_HitSound( pPlayer, m.family );
	if( g_SCDepleted.exists( szKey ) )
	{
		SC_Print( pPlayer, "Nothing left to dig here" );
		return;
	}

	SCInventory@ inv = SC_Inv( pPlayer );
	if( inv.mineKey != szKey )
	{
		inv.mineKey = szKey;
		inv.mineProgress = 0;
	}
	inv.mineProgress += SC_SwingPower( iTool, iMat );
	if( inv.mineProgress < 0.999 )
	{
		SC_Debris( vecPos, 8, m.family, 2 );
		return;
	}

	inv.mineKey = "";
	inv.mineProgress = 0;
	g_SCDepleted.set( szKey, true );
	SC_Debris( vecPos, 16, m.family, 8 );
	SC_BreakSound( vecPos, m.family );
	SC_GiveDrop( pPlayer, iTool, iMat, 1.0 );
}

void SC_HitSurface( CBasePlayer@ pPlayer, int iTool, TraceResult& in tr, const Vector& in vecSrc, const Vector& in vecEnd )
{
	int iMat = SC_MatForTexture( g_Utility.TraceTexture( tr.pHit, vecSrc, vecEnd ) );
	if( iMat == MAT_NONE )
		return;
	g_WeaponFuncs.DecalGunshot( tr, BULLET_PLAYER_CROWBAR );
	Vector vecInside = tr.vecEndPos - tr.vecPlaneNormal * 2.0;
	SC_HarvestCell( pPlayer, iTool, iMat, "" + g_EntityFuncs.Instance( tr.pHit ).entindex() + ":" + SC_CellKey( vecInside ), tr.vecEndPos );
}

'''
s = s[:start] + new + s[end:]
open(p, 'w', newline='').write(s)

# sc_props.as: terrain-sized / huge props are harvested in place instead of removed
p = os.path.join(D, 'sc_props.as')
s = open(p, newline='').read()
old = '''	Vector mn, mx;
	SC_EntBox( pEnt, mn, mx );
	float flAmount = SC_BoxBlocks( mx - mn );
	flAmount = flAmount < 1 ? 1 : ( flAmount > SC_PROP_MAX_DROP ? SC_PROP_MAX_DROP : float( int( flAmount + 0.5 ) ) );
'''
new = '''	Vector mn, mx;
	SC_EntBox( pEnt, mn, mx );
	float flVolume = SC_BoxBlocks( mx - mn );
	if( mats.length() > 0 && SC_IsHugeProp( iMain, flVolume ) )
	{
		// Rock formations, dunes, buildings: part of the level, so harvest them without removing.
		SC_HarvestCell( pPlayer, iTool, iMain, "prop" + pEnt.entindex() + ":" + SC_CellKey( vecHit ), vecHit );
		return;
	}
	float flAmount = flVolume < 1 ? 1 : ( flVolume > SC_PROP_MAX_DROP ? SC_PROP_MAX_DROP : float( int( flVolume + 0.5 ) ) );
'''
assert old in s
s = s.replace(old, new)
anchor = '// One swing at a prop: it breaks after enough hits and drops its material mix.'
helper = '''bool SC_IsHugeProp( int iMain, float flVolume )
{
	bool bEarth = iMain == MAT_COBBLE || iMain == MAT_STONE || iMain == MAT_SAND || iMain == MAT_DIRT
		|| iMain == MAT_GRAVEL || iMain == MAT_SNOW || iMain == MAT_GRASS || iMain == MAT_CONCRETE;
	return ( bEarth && flVolume > SC_PROP_EARTH_MAX ) || flVolume > SC_PROP_REMOVE_MAX;
}

'''
assert anchor in s
s = s.replace(anchor, helper + anchor)
s = s.replace('const float SC_PROP_MAX_DROP = 12;', 'const float SC_PROP_MAX_DROP = 12;\nconst float SC_PROP_EARTH_MAX = 8;       // earth/stone props bigger than this (in blocks) are terrain\nconst float SC_PROP_REMOVE_MAX = 1500;   // nothing bigger than this is ever removed')
open(p, 'w', newline='').write(s)
print('ok')
