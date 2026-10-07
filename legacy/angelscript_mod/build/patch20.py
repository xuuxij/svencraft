import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (name, a[:70])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='').write(s)

# Any tool collects ordinary blocks (the right tool is just faster). Only metal-class blocks (level 2+)
# still need a pickaxe of a high enough tier: that's the progression gate.
patch('sc_materials.as', [
    ("""	return m.level == 0 || ( m.tool == iTool && SC_ToolTier( pPlayer, iTool ) >= m.level );""",
     """	if( m.level <= 1 )
		return true;
	return m.tool == iTool && SC_ToolTier( pPlayer, iTool ) >= m.level;"""),
    ("	int level;        // harvest level: 0 = anything, 1 = wooden+ tool of the right type, 2 = stone+, 3 = iron",
     "	int level;        // harvest level: 0-1 = any tool collects it, 2 = stone+ pickaxe, 3 = iron pickaxe"),
])
# Make "can't collect" obvious: deny sound + message in the HUD popup slot as well.
patch('sc_blocks.as', [
    ("""		SC_Print( pPlayer, SC_HarvestHint( iMat ) );
		return;""", """		SC_Print( pPlayer, SC_HarvestHint( iMat ) );
		g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/wpn_denyselect.wav", 1, ATTN_NORM );
		return;"""),
])
patch('sc_props.as', [
    ("""		SC_Print( pPlayer, SC_HarvestHint( mats[0] ) );
		return;""", """		SC_Print( pPlayer, SC_HarvestHint( mats[0] ) );
		g_SoundSystem.EmitSound( pPlayer.edict(), CHAN_ITEM, "common/wpn_denyselect.wav", 1, ATTN_NORM );
		return;"""),
])
print('ok')
