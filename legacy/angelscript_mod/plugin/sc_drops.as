// Mined blocks and objects pop out as small spinning item blocks that fly to a nearby player and are picked up.

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
		if( self.pev.iuser1 == MAT_DOOR )
		{
			g_EntityFuncs.SetModel( self, SC_DOOR_MODEL );
			self.pev.body = 1;            // centred item version of the door
			self.pev.scale = 0.3;
		}
		else
		{
			g_EntityFuncs.SetModel( self, SC_BLOCK_MODEL );
			self.pev.skin = self.pev.iuser1;
			self.pev.body = self.pev.iuser1 == MAT_GLASS_PANE ? SC_SHAPE_SHEET : 0;
			if( self.pev.iuser1 == MAT_GLASS_PANE )
				self.pev.angles.x = 90;    // stand the pane up
			self.pev.scale = SC_DROP_SCALE;
		}
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
		g_Game.PrecacheModel( SC_DOOR_MODEL );
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
