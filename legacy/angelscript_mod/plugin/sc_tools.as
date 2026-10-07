// Mining tools. Primary: mine / melee. Secondary: place selected block. Reload: next block type. Attack3: inventory.

const float SC_REACH = 128.0;        // ~4 blocks, like Minecraft
const float SC_MELEE_RANGE = 48.0;   // monsters/players still need crowbar range

enum SCAnimSet
{
	ANIMSET_CROWBAR = 0,   // v_crowbar / v_warhammer layout
	ANIMSET_SHOVEL         // models/hunger/v_shovel.mdl layout
}

class SCToolWeapon : ScriptBasePlayerWeaponEntity
{
	protected CBasePlayer@ m_pPlayer
	{
		get const { return cast<CBasePlayer@>( self.m_hPlayer.GetEntity() ); }
		set { self.m_hPlayer = EHandle( @value ); }
	}

	protected int m_iTool = TOOL_PICKAXE;
	protected string m_szVModel = "models/v_crowbar.mdl";
	protected string m_szPModel = "models/p_crowbar.mdl";
	protected string m_szWModel = "models/w_crowbar.mdl";
	protected int m_iPosition = 7;
	protected float m_flDamage = 15;
	protected int m_iAnimSet = ANIMSET_CROWBAR;

	private int m_iSwing = 0;
	private bool m_bReloadHeld = false;

	int AnimIdle() { return 0; }
	int AnimDraw() { return m_iAnimSet == ANIMSET_SHOVEL ? 3 : 1; }

	int AnimHit()
	{
		array<int> crowbar = { 3, 6, 8 };
		array<int> shovel = { 5, 7, 9 };
		return m_iAnimSet == ANIMSET_SHOVEL ? shovel[m_iSwing % 3] : crowbar[m_iSwing % 3];
	}

	int AnimMiss()
	{
		array<int> crowbar = { 4, 5, 7 };
		array<int> shovel = { 6, 8, 10 };
		return m_iAnimSet == ANIMSET_SHOVEL ? shovel[m_iSwing % 3] : crowbar[m_iSwing % 3];
	}

	void Spawn()
	{
		Precache();
		g_EntityFuncs.SetModel( self, "models/svencraft/w_sc" + SC_ToolName( m_iTool ) + "1.mdl" );
		self.m_iClip = -1;
		self.FallInit();
	}

	void Precache()
	{
		self.PrecacheCustomModels();
		for( int t = 1; t <= SC_MAX_TIER; t++ )
		{
			g_Game.PrecacheModel( "models/svencraft/v_sc" + SC_ToolName( m_iTool ) + t + ".mdl" );
			g_Game.PrecacheModel( "models/svencraft/p_sc" + SC_ToolName( m_iTool ) + t + ".mdl" );
			g_Game.PrecacheModel( "models/svencraft/w_sc" + SC_ToolName( m_iTool ) + t + ".mdl" );
		}
		g_SoundSystem.PrecacheSound( "weapons/cbar_miss1.wav" );
		g_SoundSystem.PrecacheSound( "weapons/cbar_hitbod1.wav" );
		g_SoundSystem.PrecacheSound( "weapons/cbar_hitbod2.wav" );
		g_SoundSystem.PrecacheSound( "weapons/cbar_hit1.wav" );
	}

	bool GetItemInfo( ItemInfo& out info )
	{
		info.iMaxAmmo1 = -1;
		info.iMaxAmmo2 = -1;
		info.iMaxClip = WEAPON_NOCLIP;
		info.iSlot = 0;
		info.iPosition = m_iPosition;
		info.iWeight = 0;
		return true;
	}

	bool AddToPlayer( CBasePlayer@ pPlayer )
	{
		if( !BaseClass.AddToPlayer( pPlayer ) )
			return false;
		@m_pPlayer = pPlayer;
		return true;
	}

	string TierModel( const string& in szPrefix )
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
		bool bResult = self.DefaultDeploy( TierModel( "v" ), TierModel( "p" ), AnimDraw(), "crowbar" );
		self.m_flTimeWeaponIdle = g_Engine.time + 2.0;
		return bResult;
	}

	void Holster( int skiplocal = 0 )
	{
		SC_ClearFeedback( m_pPlayer );
		self.m_fInReload = false;
		m_pPlayer.m_flNextAttack = g_WeaponFuncs.WeaponTimeBase() + 0.5;
		m_pPlayer.pev.viewmodel = "";
		SetThink( null );
	}

	void PrimaryAttack()
	{
		Swing();
	}

	void SecondaryAttack()
	{
		SC_TryPlace( m_pPlayer );
		self.m_flNextSecondaryAttack = g_Engine.time + 0.2;
		self.m_flTimeWeaponIdle = g_Engine.time + 2.0;
	}

	void TertiaryAttack()
	{
		SC_OpenInventory( m_pPlayer );
		self.m_flNextTertiaryAttack = g_Engine.time + 0.5;
	}

	void WeaponIdle()
	{
		SC_FeedbackIdle( m_pPlayer );

		// Reload key cycles block type (tools have no clip, so the engine never calls Reload()).
		bool bReload = ( m_pPlayer.pev.button & IN_RELOAD ) != 0;
		if( bReload && !m_bReloadHeld )
			SC_CycleBlock( m_pPlayer, 1 );
		m_bReloadHeld = bReload;

		if( self.m_flTimeWeaponIdle > g_Engine.time )
			return;
		self.SendWeaponAnim( AnimIdle() );
		self.m_flTimeWeaponIdle = g_Engine.time + 10.0;
	}

	void Swing()
	{
		Math.MakeVectors( m_pPlayer.pev.v_angle );
		Vector vecSrc = m_pPlayer.GetGunPosition();
		Vector vecDir = g_Engine.v_forward;
		Vector vecEnd = vecSrc + vecDir * SC_REACH;

		TraceResult tr;
		g_Utility.TraceLine( vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer.edict(), tr );

		m_pPlayer.SetAnimation( PLAYER_ATTACK1 );
		self.m_flTimeWeaponIdle = g_Engine.time + 2.0;

		CBaseEntity@ pEntity = tr.flFraction < 1.0 ? g_EntityFuncs.Instance( tr.pHit ) : null;
		Vector vecHit = tr.vecEndPos;
		float flDist = ( tr.vecEndPos - vecSrc ).Length();
		bool bHit = tr.flFraction < 1.0;
		Vector vecNormal = tr.vecPlaneNormal;

		// Non-solid decorations in front of whatever the trace hit take priority.
		float flDecorDist;
		CBaseEntity@ pDecor = SC_FindDecor( vecSrc, vecDir, bHit ? flDist : SC_REACH, flDecorDist );
		if( pDecor !is null )
		{
			@pEntity = pDecor;
			flDist = flDecorDist;
			vecHit = vecSrc + vecDir * flDecorDist;
			vecNormal = -vecDir;
			bHit = true;
		}

		int iKind = bHit ? SC_KindOf( pEntity ) : KIND_NONE;
		if( !bHit || iKind == KIND_NONE || ( iKind == KIND_CREATURE && flDist > SC_MELEE_RANGE ) )
		{
			self.SendWeaponAnim( AnimMiss() );
			m_iSwing++;
			g_SoundSystem.EmitSoundDyn( m_pPlayer.edict(), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + Math.RandomLong( 0, 15 ) );
			self.m_flNextPrimaryAttack = g_Engine.time + 0.4;
			return;
		}

		self.SendWeaponAnim( AnimHit() );
		m_iSwing++;
		self.m_flNextPrimaryAttack = g_Engine.time + 0.25;

		if( iKind == KIND_WALL )
			iKind = SC_HitWall( m_pPlayer, pEntity, m_iTool, tr, vecSrc, vecEnd );   // may fall back to PROP/SURFACE

		switch( iKind )
		{
		case KIND_BLOCK:   SC_HitBlock( m_pPlayer, pEntity, m_iTool, vecNormal ); return;
		case KIND_NODE:    SC_HitNode( m_pPlayer, pEntity, m_iTool, tr ); return;
		case KIND_PROP:    SC_HitProp( m_pPlayer, pEntity, m_iTool, vecSrc, vecEnd, vecHit, vecNormal ); return;
		case KIND_SURFACE: SC_HitSurface( m_pPlayer, m_iTool, tr, vecSrc, vecEnd ); return;
		case KIND_WALL:    return;
		}

		// Creatures and breakables take normal melee damage, as with the crowbar.
		int iTexMat = iKind == KIND_BREAKABLE ? SC_TextureMat( pEntity, vecSrc, vecEnd ) : MAT_NONE;
		Vector vecSize = pEntity.pev.maxs - pEntity.pev.mins;
		Vector vecCenter = pEntity.pev.origin + ( pEntity.pev.mins + pEntity.pev.maxs ) * 0.5;
		g_WeaponFuncs.ClearMultiDamage();
		pEntity.TraceAttack( m_pPlayer.pev, m_flDamage, vecDir, tr, DMG_CLUB );
		g_WeaponFuncs.ApplyMultiDamage( m_pPlayer.pev, m_pPlayer.pev );

		if( iKind == KIND_BREAKABLE )
		{
			SC_CheckBreakable( m_pPlayer, EHandle( pEntity ), m_iTool, iTexMat, vecSize, vecCenter );
			g_SoundSystem.EmitSoundDyn( m_pPlayer.edict(), CHAN_WEAPON, "weapons/cbar_hit1.wav", 1, ATTN_NORM, 0, 98 + Math.RandomLong( 0, 3 ) );
		}
		else if( pEntity.BloodColor() != DONT_BLEED )
			g_SoundSystem.EmitSound( m_pPlayer.edict(), CHAN_WEAPON, Math.RandomLong( 0, 1 ) == 0 ? "weapons/cbar_hitbod1.wav" : "weapons/cbar_hitbod2.wav", 1, ATTN_NORM );
		else
			g_SoundSystem.EmitSoundDyn( m_pPlayer.edict(), CHAN_WEAPON, "weapons/cbar_hit1.wav", 1, ATTN_NORM, 0, 98 + Math.RandomLong( 0, 3 ) );
	}
}

class weapon_scpickaxe : SCToolWeapon
{
	weapon_scpickaxe()
	{
		m_iTool = TOOL_PICKAXE;
		m_szVModel = "models/svencraft/v_scpickaxe.mdl";
		m_szPModel = "models/svencraft/p_scpickaxe.mdl";
		m_szWModel = "models/svencraft/w_scpickaxe.mdl";
		m_iPosition = 7;
		m_flDamage = 18;
		m_iAnimSet = ANIMSET_CROWBAR;
	}
}

class weapon_scshovel : SCToolWeapon
{
	weapon_scshovel()
	{
		m_iTool = TOOL_SHOVEL;
		m_szVModel = "models/svencraft/v_scshovel1.mdl";
		m_szPModel = "models/svencraft/p_scshovel1.mdl";
		m_szWModel = "models/svencraft/w_scshovel1.mdl";
		m_iPosition = 8;
		m_flDamage = 14;
		m_iAnimSet = ANIMSET_CROWBAR;
	}
}

class weapon_scaxe : SCToolWeapon
{
	weapon_scaxe()
	{
		m_iTool = TOOL_AXE;
		m_szVModel = "models/svencraft/v_scaxe.mdl";
		m_szPModel = "models/svencraft/p_scaxe.mdl";
		m_szWModel = "models/svencraft/w_scaxe.mdl";
		m_iPosition = 9;
		m_flDamage = 20;
		m_iAnimSet = ANIMSET_CROWBAR;
	}
}

const array<string> SC_TOOL_NAMES = { "weapon_scpickaxe", "weapon_scshovel", "weapon_scaxe" };

void SC_RegisterTools()
{
	for( uint i = 0; i < SC_TOOL_NAMES.length(); i++ )
	{
		g_CustomEntityFuncs.RegisterCustomEntity( SC_TOOL_NAMES[i], SC_TOOL_NAMES[i] );
		g_ItemRegistry.RegisterWeapon( SC_TOOL_NAMES[i], "svencraft" );
	}
}

void SC_PrecacheTools()
{
	for( uint i = 0; i < SC_TOOL_NAMES.length(); i++ )
		g_Game.PrecacheOther( SC_TOOL_NAMES[i] );
}

void SC_GiveTools( EHandle hPlayer )
{
	CBasePlayer@ pPlayer = cast<CBasePlayer@>( hPlayer.GetEntity() );
	if( pPlayer is null || !pPlayer.IsConnected() || !pPlayer.IsAlive() )
		return;
	for( uint i = 0; i < SC_TOOL_NAMES.length(); i++ )
	{
		if( pPlayer.HasNamedPlayerItem( SC_TOOL_NAMES[i] ) is null )
			pPlayer.GiveNamedItem( SC_TOOL_NAMES[i] );
	}
}

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
