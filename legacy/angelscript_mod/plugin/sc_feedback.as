// Mining progress feedback: a crack overlay on the face being mined (blocks, terrain, walls, surfaces)
// and a glow shell on studio props that turns from yellow to red as they near breaking.

const string SC_CRACK_SPRITE = "sprites/svencraft/cracks.spr";   // 10 frames, oriented sprite, 64 px
const float SC_FEEDBACK_TIMEOUT = 1.5;                            // hide feedback this long after the last hit

class sc_crack : ScriptBaseEntity
{
	void Spawn()
	{
		g_EntityFuncs.SetModel( self, SC_CRACK_SPRITE );
		self.pev.solid = SOLID_NOT;
		self.pev.movetype = MOVETYPE_NONE;
		self.pev.rendermode = kRenderTransAlpha;
		self.pev.renderamt = 255;
		self.pev.framerate = 0;
	}
}

void SC_RegisterFeedback()
{
	g_CustomEntityFuncs.RegisterCustomEntity( "sc_crack", "sc_crack" );
	g_Game.PrecacheModel( SC_CRACK_SPRITE );
}

// Crack stage for progress 0..1 on a face (center, outward normal, face size in units).
void SC_ShowCrack( CBasePlayer@ pPlayer, const Vector& in vecCenter, const Vector& in vecNormal, float flSize, float flProgress )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	inv.lastHitTime = g_Engine.time;
	CBaseEntity@ pCrack = inv.crack.GetEntity();
	if( pCrack is null )
	{
		@pCrack = g_EntityFuncs.Create( "sc_crack", vecCenter, g_vecZero, false );
		if( pCrack is null )
			return;
		inv.crack = EHandle( pCrack );
	}
	// An oriented sprite shows its face to a viewer looking along its forward vector, so forward must point
	// into the surface. VecToAngles' pitch is the opposite sign of what entity angles expect.
	Vector vecAngles;
	g_EngineFuncs.VecToAngles( -vecNormal, vecAngles );
	vecAngles.x = -vecAngles.x;
	pCrack.pev.angles = vecAngles;
	pCrack.pev.scale = flSize / 64.0;
	int iFrame = int( flProgress * 10 );
	pCrack.pev.frame = iFrame < 0 ? 0 : ( iFrame > 9 ? 9 : iFrame );
	pCrack.pev.effects &= ~EF_NODRAW;
	g_EntityFuncs.SetOrigin( pCrack, vecCenter + vecNormal * 0.5 );
}

void SC_HideCrack( CBasePlayer@ pPlayer )
{
	CBaseEntity@ pCrack = SC_Inv( pPlayer ).crack.GetEntity();
	if( pCrack !is null )
		pCrack.pev.effects |= EF_NODRAW;
}

// Glow shell on a studio prop; restores the previous prop when the player switches targets.
void SC_ShowGlow( CBasePlayer@ pPlayer, CBaseEntity@ pEnt, float flProgress )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	inv.lastHitTime = g_Engine.time;
	if( inv.glow.GetEntity() !is pEnt )
	{
		SC_ClearGlow( pPlayer );
		inv.glow = EHandle( pEnt );
		inv.glowFx = pEnt.pev.renderfx;
		inv.glowAmt = pEnt.pev.renderamt;
		inv.glowColor = pEnt.pev.rendercolor;
	}
	pEnt.pev.renderfx = kRenderFxGlowShell;
	pEnt.pev.rendercolor = Vector( 255, 230 - 200 * flProgress, 40 - 40 * flProgress );
	pEnt.pev.renderamt = 4 + int( flProgress * 14 );    // shell thickness grows as it weakens
}

void SC_ClearGlow( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	CBaseEntity@ pOld = inv.glow.GetEntity();
	if( pOld !is null )
	{
		pOld.pev.renderfx = inv.glowFx;
		pOld.pev.renderamt = inv.glowAmt;
		pOld.pev.rendercolor = inv.glowColor;
	}
	inv.glow = EHandle( null );
}

void SC_ClearFeedback( CBasePlayer@ pPlayer )
{
	SC_HideCrack( pPlayer );
	SC_ClearGlow( pPlayer );
}

// Called every frame from the held tool: hide feedback once the player stops mining.
void SC_FeedbackIdle( CBasePlayer@ pPlayer )
{
	SCInventory@ inv = SC_Inv( pPlayer );
	if( inv.lastHitTime > 0 && g_Engine.time - inv.lastHitTime > SC_FEEDBACK_TIMEOUT )
	{
		SC_ClearFeedback( pPlayer );
		inv.lastHitTime = 0;
	}
}

// Face of an axis-aligned box that a hit with this normal lands on: center and size.
void SC_BoxFace( const Vector& in mn, const Vector& in mx, const Vector& in vecNormal, Vector& out vecCenter, float& out flSize )
{
	int a = 0;
	if( SC_Abs( vecNormal.y ) > SC_Abs( vecNormal[a] ) ) a = 1;
	if( SC_Abs( vecNormal.z ) > SC_Abs( vecNormal[a] ) ) a = 2;
	vecCenter = ( mn + mx ) * 0.5;
	vecCenter[a] = vecNormal[a] > 0 ? mx[a] : mn[a];
	flSize = 0;
	for( int i = 0; i < 3; i++ )
	{
		if( i != a && mx[i] - mn[i] > flSize )
			flSize = mx[i] - mn[i];
	}
	Vector n = g_vecZero;
	n[a] = vecNormal[a] > 0 ? 1 : -1;
}

Vector SC_AxisNormal( const Vector& in v )
{
	int a = 0;
	if( SC_Abs( v.y ) > SC_Abs( v[a] ) ) a = 1;
	if( SC_Abs( v.z ) > SC_Abs( v[a] ) ) a = 2;
	Vector n = g_vecZero;
	n[a] = v[a] >= 0 ? 1 : -1;
	return n;
}
