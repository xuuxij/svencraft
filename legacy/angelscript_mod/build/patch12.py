import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (name, a[:80])
        s = s.replace(a, b)
    open(p, 'w', newline='').write(s)

patch('sc_inventory.as', [
    ("\tint hudNameShown = -1;  // block whose name was last popped up on the HUD",
     "\tint hudNameShown = -1;  // block whose name was last popped up on the HUD\n\n"
     "\t// Mining feedback (sc_feedback.as)\n\tEHandle crack;\n\tEHandle glow;\n\tint glowFx = 0;\n\tfloat glowAmt = 0;\n\tVector glowColor;\n\tfloat lastHitTime = 0;"),
])

patch('sc_blocks.as', [
    ("void SC_HitBlock( CBasePlayer@ pPlayer, CBaseEntity@ pBlock, int iTool )",
     "void SC_HitBlock( CBasePlayer@ pPlayer, CBaseEntity@ pBlock, int iTool, const Vector& in vecNormal )"),
    ("""	if( pBlock.pev.fuser1 < 0.999 )
	{
		SC_Debris( pBlock.pev.origin, flSize, m.family, 2 );
		return;
	}

	Vector vecPos = pBlock.pev.origin;""",
     """	if( pBlock.pev.fuser1 < 0.999 )
	{
		SC_Debris( pBlock.pev.origin, flSize, m.family, 2 );
		Vector vecFace;
		float flFace;
		SC_BoxFace( pBlock.pev.origin + pBlock.pev.mins, pBlock.pev.origin + pBlock.pev.maxs, vecNormal, vecFace, flFace );
		SC_ShowCrack( pPlayer, vecFace, SC_AxisNormal( vecNormal ), flFace, pBlock.pev.fuser1 );
		return;
	}
	SC_HideCrack( pPlayer );

	Vector vecPos = pBlock.pev.origin;"""),
    ("void SC_HarvestCell( CBasePlayer@ pPlayer, int iTool, int iMat, const string& in szKey, const Vector& in vecPos )",
     "void SC_HarvestCell( CBasePlayer@ pPlayer, int iTool, int iMat, const string& in szKey, const Vector& in vecPos, const Vector& in vecNormal )"),
    ("""	inv.mineProgress += SC_SwingPower( iTool, iMat );
	if( inv.mineProgress < 0.999 )
	{
		SC_Debris( vecPos, 8, m.family, 2 );
		return;
	}
""", """	inv.mineProgress += SC_SwingPower( iTool, iMat );
	if( inv.mineProgress < 0.999 )
	{
		SC_Debris( vecPos, 8, m.family, 2 );
		SC_ShowCrack( pPlayer, vecPos, vecNormal, SC_BLOCK_SIZE, inv.mineProgress );
		return;
	}
	SC_HideCrack( pPlayer );
"""),
    ("SC_CellKey( vecInside ), tr.vecEndPos );", "SC_CellKey( vecInside ), tr.vecEndPos, tr.vecPlaneNormal );"),
])

patch('sc_sandbox.as', [
    ("""		g_SCCellProgress.set( szKey, flProgress );
		SC_Debris( tr.vecEndPos, 8, m.family, 2 );
		return;
	}
	g_SCCellProgress.delete( szKey );""", """		g_SCCellProgress.set( szKey, flProgress );
		SC_Debris( tr.vecEndPos, 8, m.family, 2 );
		Vector n = SC_AxisNormal( tr.vecPlaneNormal );
		SC_ShowCrack( pPlayer, vecCell + n * ( SC_BLOCK_SIZE * 0.5 ), n, SC_BLOCK_SIZE, flProgress );
		return;
	}
	g_SCCellProgress.delete( szKey );
	SC_HideCrack( pPlayer );"""),
])

patch('sc_props.as', [
    ("void SC_HitProp( CBasePlayer@ pPlayer, CBaseEntity@ pEnt, int iTool, const Vector& in vecSrc, const Vector& in vecEnd, const Vector& in vecHit )",
     "void SC_HitProp( CBasePlayer@ pPlayer, CBaseEntity@ pEnt, int iTool, const Vector& in vecSrc, const Vector& in vecEnd, const Vector& in vecHit, const Vector& in vecNormal )"),
    ('SC_CellKey( vecHit ), vecHit );', 'SC_CellKey( vecHit ), vecHit, vecNormal );'),
    ("""	if( inv.mineProgress < 0.999 )
	{
		SC_Debris( vecHit, 8, m.family, 2 );
		return;
	}
	inv.mineKey = "";
	inv.mineProgress = 0;
""", """	if( inv.mineProgress < 0.999 )
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
"""),
    ("			SC_HitBlock( pPlayer, pieces[p], iTool );", "			SC_HitBlock( pPlayer, pieces[p], iTool, tr.vecPlaneNormal );"),
])

patch('sc_tools.as', [
    ("""		bool bHit = tr.flFraction < 1.0;
""", """		bool bHit = tr.flFraction < 1.0;
		Vector vecNormal = tr.vecPlaneNormal;
"""),
    ("""			vecHit = vecSrc + vecDir * flDecorDist;
			bHit = true;""", """			vecHit = vecSrc + vecDir * flDecorDist;
			vecNormal = -vecDir;
			bHit = true;"""),
    ("		case KIND_BLOCK:   SC_HitBlock( m_pPlayer, pEntity, m_iTool ); return;",
     "		case KIND_BLOCK:   SC_HitBlock( m_pPlayer, pEntity, m_iTool, vecNormal ); return;"),
    ("		case KIND_PROP:    SC_HitProp( m_pPlayer, pEntity, m_iTool, vecSrc, vecEnd, vecHit ); return;",
     "		case KIND_PROP:    SC_HitProp( m_pPlayer, pEntity, m_iTool, vecSrc, vecEnd, vecHit, vecNormal ); return;"),
    ("""	void WeaponIdle()
	{""", """	void WeaponIdle()
	{
		SC_FeedbackIdle( m_pPlayer );
"""),
    ("""	void Holster( int skiplocal = 0 )
	{""", """	void Holster( int skiplocal = 0 )
	{
		SC_ClearFeedback( m_pPlayer );"""),
])

patch('svencraft.as', [
    ('#include "sc_hud"\n', '#include "sc_hud"\n#include "sc_feedback"\n'),
    ("	SC_RegisterProp();\n", "	SC_RegisterProp();\n	SC_RegisterFeedback();\n"),
])
print('ok')
