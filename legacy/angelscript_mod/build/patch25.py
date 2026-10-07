import os
D = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop\svencoop_addon\scripts\plugins\svencraft"

def patch(name, reps):
    p = os.path.join(D, name)
    s = open(p, newline='').read()
    for a, b in reps:
        assert a in s, (name, a[:90])
        s = s.replace(a, b, 1)
    open(p, 'w', newline='').write(s)

# ------------------------------------------------------------------ crafting: no FL_FROZEN, mouse read every frame
patch('sc_inventory.as', [
    ("	float cursorSentTime = 0;", "	float cursorSentTime = 0;\n	float savedMaxspeed = 0;\n	bool snapPending = false;"),
])
patch('sc_crafting.as', [
    ("""	inv.lastButtons = pPlayer.pev.button;      // keys already held don't count as presses
	pPlayer.pev.flags |= FL_FROZEN;""",
     """	inv.lastButtons = pPlayer.pev.button;      // keys already held don't count as presses
	// Hold the player still by speed, not FL_FROZEN: a frozen player's view angles stop reaching the
	// server, and the emulated cursor is driven by them.
	inv.savedMaxspeed = pPlayer.pev.maxspeed;"""),
    ("""	inv.craftClosedTime = g_Engine.time;
	pPlayer.pev.flags &= ~FL_FROZEN;""",
     """	inv.craftClosedTime = g_Engine.time;
	pPlayer.pev.maxspeed = inv.savedMaxspeed;
	inv.snapPending = false;"""),
    ("""	pPlayer.m_flNextAttack = g_WeaponFuncs.WeaponTimeBase() + 0.3;   // no shooting/mining while the window is open
	pPlayer.pev.velocity = g_vecZero;
""", """	pPlayer.m_flNextAttack = g_WeaponFuncs.WeaponTimeBase() + 0.3;   // no shooting/mining while the window is open
	pPlayer.pev.maxspeed = 1;                                          // effectively can't walk
	pPlayer.pev.velocity.x = 0;
	pPlayer.pev.velocity.y = 0;
	SC_CraftMouse( pPlayer );
"""),
])
# replace the PostThink cursor handler with a shared mouse function + a PostThink re-snap
p = os.path.join(D, 'sc_crafting.as')
s = open(p, newline='', encoding='utf-8').read()
a = s.index('// After the player\'s own think (which sets pev.angles from the view)')
s = s[:a] + '''// Mouse movement that would have turned the view moves the cursor instead; the view is snapped back.
void SC_CraftMouse( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	float dYaw = SC_AngleDiff( pPlayer.pev.v_angle.y, inv.craftAngles.y );
	float dPitch = SC_AngleDiff( pPlayer.pev.v_angle.x, inv.craftAngles.x );
	if( SC_Abs( dYaw ) + SC_Abs( dPitch ) < 0.01 )
		return;

	inv.cursorX = inv.cursorX - dYaw * SC_CURSOR_PX_PER_DEGREE;
	inv.cursorY = inv.cursorY + dPitch * SC_CURSOR_PX_PER_DEGREE;
	if( inv.cursorX < -200 ) inv.cursorX = -200;
	if( inv.cursorX > 200 ) inv.cursorX = 200;
	if( inv.cursorY < -116 ) inv.cursorY = -116;
	if( inv.cursorY > 116 ) inv.cursorY = 116;

	pPlayer.pev.angles = inv.craftAngles;
	pPlayer.pev.v_angle = inv.craftAngles;
	pPlayer.pev.fixangle = 1;                    // FAM_FORCEVIEWANGLES: client view snaps back
	inv.snapPending = true;

	int iHit = SC_CraftHit( inv );
	int idx = iHit >= 0 ? SC_GridRecipe( inv, iHit ) : -1;
	if( idx >= 0 && idx != inv.craftSel )
	{
		inv.craftSel = idx;                      // hovering a recipe highlights it
		SC_DrawCraftUI( pPlayer );
	}
	if( g_Engine.time - inv.cursorSentTime >= 0.02 )
		SC_DrawCursor( pPlayer );
}

// The player's own think rewrites pev.angles from the view; restore the held view before it's sent.
HookReturnCode SC_CraftPostThink( CBasePlayer@ pPlayer )
{
	if( pPlayer is null )
		return HOOK_CONTINUE;
	SCInventory@ inv = SC_Inv( pPlayer );
	if( !inv.craftOpen )
		return HOOK_CONTINUE;
	SC_CraftMouse( pPlayer );
	if( inv.snapPending )
	{
		pPlayer.pev.angles = inv.craftAngles;
		pPlayer.pev.v_angle = inv.craftAngles;
		pPlayer.pev.fixangle = 1;
		inv.snapPending = false;
	}
	return HOOK_CONTINUE;
}
'''
open(p, 'w', newline='', encoding='utf-8').write(s)

# ------------------------------------------------------------------ hotbar: 8 slots, every slot shows its count
patch('sc_hud.as', [
    ("const int SC_HUD_SLOTS = 9;           // HUD sprite channels 0..8\nconst int SC_HUD_CH_COUNT = 9;",
     "const int SC_HUD_SLOTS = 8;           // icon channels 0..7, count channels 8..15 (16 HUD sprite channels in total)\nconst int SC_HUD_CH_COUNT = 8;"),
    ("""	for( int i = 0; i < SC_HUD_SLOTS; i++ )
	{
		int k = iStart + i;
		int iFrame = k < int( owned.length() ) ? owned[k] : SC_HOTBAR_EMPTY;
		if( i == iSelSlot )
			iFrame += SC_HOTBAR_SELECTED;
		SC_HudSprite( pPlayer, i, SC_SPR_HOTBAR, iFrame, i );
	}

	if( iSel >= 0 && !inv.creative )
	{
		int n = inv.Whole( inv.selected );
		SC_HudSprite( pPlayer, SC_HUD_CH_COUNT, SC_SPR_COUNTS, n > 99 ? 100 : n, iSelSlot );
	}
	else
		g_PlayerFuncs.HudToggleElement( pPlayer, SC_HUD_CH_COUNT, false );""",
     """	for( int i = 0; i < SC_HUD_SLOTS; i++ )
	{
		int k = iStart + i;
		bool bFilled = k < int( owned.length() );
		int iFrame = bFilled ? owned[k] : SC_HOTBAR_EMPTY;
		if( i == iSelSlot )
			iFrame += SC_HOTBAR_SELECTED;
		SC_HudSprite( pPlayer, i, SC_SPR_HOTBAR, iFrame, i );
		// Stack count in the corner of every slot (Minecraft shows it when there's more than one).
		int n = bFilled ? inv.Whole( owned[k] ) : 0;
		if( bFilled && !inv.creative && n > 1 )
			SC_HudSprite( pPlayer, SC_HUD_CH_COUNT + i, SC_SPR_COUNTS, n > 99 ? 100 : n, i );
		else
			g_PlayerFuncs.HudToggleElement( pPlayer, SC_HUD_CH_COUNT + i, false );
	}"""),
])

# ------------------------------------------------------------------ item drops you pick up (Minecraft style)
open(os.path.join(D, 'sc_drops.as'), 'w', newline='').write('''// Mined blocks and objects pop out as small spinning item blocks that fly to a nearby player and are picked up.

const float SC_DROP_SCALE = 0.25;        // a quarter-size block
const float SC_DROP_PICKUP_DELAY = 0.4;  // seconds before it can be picked up (lets it pop out first)
const float SC_DROP_MAGNET = 96;         // pulled toward players within this range
const float SC_DROP_COLLECT = 40;        // collected within this range
const float SC_DROP_LIFETIME = 300;
const int SC_DROP_MAX_PER_BREAK = 8;

// pev.iuser1 = material, pev.fuser1 = amount (blocks), pev.fuser2 = spawn time
class sc_drop : ScriptBaseEntity
{
	void Spawn()
	{
		Precache();
		g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );
		self.pev.skin = self.pev.iuser1;
		self.pev.body = 0;
		self.pev.scale = SC_DROP_SCALE;
		self.pev.movetype = MOVETYPE_TOSS;
		self.pev.solid = SOLID_TRIGGER;
		self.pev.gravity = 1.0;
		self.pev.friction = 0.6;
		g_EntityFuncs.SetSize( self.pev, Vector( -4, -4, -4 ), Vector( 4, 4, 4 ) );
		g_EntityFuncs.SetOrigin( self, self.pev.origin );
		self.pev.fuser2 = g_Engine.time;
		SetThink( ThinkFunction( this.DropThink ) );
		self.pev.nextthink = g_Engine.time + 0.05;
	}

	void Precache()
	{
		g_Game.PrecacheModel( SC_BLOCK_MODEL );
	}

	void DropThink()
	{
		self.pev.nextthink = g_Engine.time + 0.05;
		self.pev.angles.y = self.pev.angles.y + 9;            // spin
		float flAge = g_Engine.time - self.pev.fuser2;
		if( flAge > SC_DROP_LIFETIME )
		{
			g_EntityFuncs.Remove( self );
			return;
		}
		if( flAge < SC_DROP_PICKUP_DELAY )
			return;

		CBasePlayer@ pBest = null;
		float flBest = SC_DROP_MAGNET;
		for( int i = 1; i <= g_Engine.maxClients; i++ )
		{
			CBasePlayer@ p = g_PlayerFuncs.FindPlayerByIndex( i );
			if( p is null || !p.IsConnected() || !p.IsAlive() )
				continue;
			float d = ( p.pev.origin - self.pev.origin ).Length();
			if( d < flBest )
			{
				flBest = d;
				@pBest = p;
			}
		}
		if( pBest is null )
			return;
		if( flBest <= SC_DROP_COLLECT )
		{
			SC_CollectDrop( pBest, self );
			return;
		}
		// Fly toward the player.
		Vector dir = ( pBest.pev.origin - self.pev.origin ).Normalize();
		self.pev.velocity = dir * 300;
		self.pev.flags &= ~FL_ONGROUND;
	}
}

void SC_RegisterDrops()
{
	g_CustomEntityFuncs.RegisterCustomEntity( "sc_drop", "sc_drop" );
	g_SoundSystem.PrecacheSound( "items/9mmclip1.wav" );
}

void SC_CollectDrop( CBasePlayer@ pPlayer, CBaseEntity@ pDrop )
{
	int iMat = pDrop.pev.iuser1;
	float flAmount = pDrop.pev.fuser1;
	g_EntityFuncs.Remove( pDrop );
	pDrop.pev.fuser1 = 0;
	SCInventory@ inv = SC_Inv( pPlayer );
	bool bHadAny = inv.Has( inv.selected );
	inv.Add( iMat, flAmount );
	if( !bHadAny )
		inv.selected = iMat;
	g_SoundSystem.EmitSoundDyn( pPlayer.edict(), CHAN_ITEM, "items/9mmclip1.wav", 0.6, ATTN_NORM, 0, 110 + Math.RandomLong( 0, 20 ) );
	string szAmount = flAmount >= 0.999 ? "+" + int( flAmount + 0.001 ) : "+" + int( flAmount * 100 ) + "%";
	SC_HudText( pPlayer, szAmount + " " + g_SCMats[iMat].name + "  (" + inv.Whole( iMat ) + ")" );
	inv.hudNameShown = -1;
	SC_UpdateHud( pPlayer );
}

// Spawn item drops for flAmount blocks of iMat at vecPos (one per whole block, up to a cap).
void SC_SpawnDrops( const Vector& in vecPos, int iMat, float flAmount )
{
	if( flAmount <= 0.001 )
		return;
	int n = int( flAmount + 0.001 );
	if( n < 1 ) n = 1;
	if( n > SC_DROP_MAX_PER_BREAK ) n = SC_DROP_MAX_PER_BREAK;
	float flEach = flAmount / n;
	for( int i = 0; i < n; i++ )
	{
		CBaseEntity@ pDrop = g_EntityFuncs.Create( "sc_drop", vecPos, Vector( 0, Math.RandomFloat( 0, 360 ), 0 ), true );
		if( pDrop is null )
			return;
		pDrop.pev.iuser1 = iMat;
		pDrop.pev.fuser1 = flEach;
		g_EntityFuncs.DispatchSpawn( pDrop.edict() );
		pDrop.pev.velocity = Vector( Math.RandomFloat( -60, 60 ), Math.RandomFloat( -60, 60 ), Math.RandomFloat( 120, 200 ) );
	}
}
''')

# drops instead of direct inventory adds
patch('sc_blocks.as', [
    ("""void SC_GiveDrop( CBasePlayer@ pPlayer, int iTool, int iMat, float flAmount )""",
     """void SC_GiveDrop( CBasePlayer@ pPlayer, int iTool, int iMat, float flAmount, const Vector& in vecPos )"""),
    ("""	SCInventory@ inv = SC_Inv( pPlayer );
	bool bHadAny = inv.Has( inv.selected );
	inv.Add( m.drop, flAmount );
	if( !bHadAny )
		inv.selected = m.drop;
	SC_UpdateHud( pPlayer );
}""", """	SC_SpawnDrops( vecPos, m.drop, flAmount );
}"""),
    ("	SC_GiveDrop( pPlayer, iTool, iMat, 1.0 );\n}\n\n// One swing at something",
     "	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecPos );\n}\n\n// One swing at something"),
    ("	SC_BreakSound( vecPos, m.family );\n	SC_GiveDrop( pPlayer, iTool, iMat, 1.0 );\n}\n\nvoid SC_HitSurface",
     "	SC_BreakSound( vecPos, m.family );\n	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecPos );\n}\n\nvoid SC_HitSurface"),
])
patch('sc_sandbox.as', [
    ("	SC_GiveDrop( pPlayer, iTool, iMat, 1.0 );", "	SC_GiveDrop( pPlayer, iTool, iMat, 1.0, vecCell );"),
])
patch('sc_props.as', [
    ("void SC_GiveMix( CBasePlayer@ pPlayer, int iTool, const array<int>@ mats, const array<float>@ weights, float flAmount )",
     "void SC_GiveMix( CBasePlayer@ pPlayer, int iTool, const array<int>@ mats, const array<float>@ weights, float flAmount, const Vector& in vecPos )"),
    ("""	SCInventory@ inv = SC_Inv( pPlayer );
	bool bHadAny = inv.Has( inv.selected );
	for( uint i = 0; i < mats.length(); i++ )
		inv.Add( g_SCMats[mats[i]].drop, flAmount * weights[i] / flTotal );
	if( !bHadAny )
		inv.selected = g_SCMats[mats[0]].drop;
	SC_UpdateHud( pPlayer );
}""", """	for( uint i = 0; i < mats.length(); i++ )
		SC_SpawnDrops( vecPos, g_SCMats[mats[i]].drop, flAmount * weights[i] / flTotal );
}"""),
    ("	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount );\n}", "	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount, vecCenter );\n}"),
    ("void SC_CheckBreakable( CBasePlayer@ pPlayer, EHandle hEnt, int iTool, int iTexMat, Vector vecSize )",
     "void SC_CheckBreakable( CBasePlayer@ pPlayer, EHandle hEnt, int iTool, int iTexMat, Vector vecSize, Vector vecCenter )"),
    ("	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount );\n}", "	SC_GiveMix( pPlayer, iTool, mats, weights, flAmount, vecCenter );\n}"),
])
patch('sc_tools.as', [
    ("		Vector vecSize = pEntity.pev.maxs - pEntity.pev.mins;",
     "		Vector vecSize = pEntity.pev.maxs - pEntity.pev.mins;\n		Vector vecCenter = pEntity.pev.origin + ( pEntity.pev.mins + pEntity.pev.maxs ) * 0.5;"),
    ("			SC_CheckBreakable( m_pPlayer, EHandle( pEntity ), m_iTool, iTexMat, vecSize );",
     "			SC_CheckBreakable( m_pPlayer, EHandle( pEntity ), m_iTool, iTexMat, vecSize, vecCenter );"),
])
patch('svencraft.as', [
    ('#include "sc_crafting"\n', '#include "sc_crafting"\n#include "sc_drops"\n'),
    ("	SC_RegisterFeedback();\n", "	SC_RegisterFeedback();\n	SC_RegisterDrops();\n"),
])
print('ok')
