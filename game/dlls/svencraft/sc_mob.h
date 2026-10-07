/*
sc_mob.h - Svencraft: the block world's creatures (the creeper, zombie, skeleton, spider)

They don't use Half-Life's schedules: each one thinks ten times a second like a Minecraft mob, walking straight at
what it wants and hopping up single blocks, going round what it bumps into, ambling about when it has nothing to do.
CSCMob has what they share: senses (players and Half-Life's monsters alike), the walking, the red flash and knockback
of a hit, the tip-over death and the puff of smoke; each kind adds its own Behave (sc_creeper.cpp, sc_mobs.cpp).
*/
#ifndef SC_MOB_H
#define SC_MOB_H

class CSCMob : public CBaseMonster
{
public:
	int Classify( void ) { return CLASS_MINECRAFT; }
	void SetYawSpeed( void ) { pev->yaw_speed = 360; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void ) {}

	void EXPORT MobThink( void );
	void EXPORT DyingThink( void );

protected:
	// set up in the kind's Spawn (after its Precache)
	void MobSpawn( const char *model, const Vector &mins, const Vector &maxs, float health, float eyes );
	void MobPrecache( void );

	// what each kind does every think; true while it walks (for the footsteps)
	virtual bool Behave( float dt ) = 0;
	virtual void Drops( void ) {}
	virtual void HurtSound( void );
	virtual void DieSound( void );
	virtual void OnHurt( float flDamage, int bitsDamageType ) {}
	virtual void OnKilled( void ) {}
	virtual void UpdateSkin( void );
	virtual float StepVolume( void ) { return 0.35f; }

	void SetAnim( const char *name, float rate );
	void FindTarget( float sight );
	bool Walk( const Vector &vecGoal, float flSpeed, float dt, const Vector *pFace = NULL );
	bool Chase( CBaseEntity *pEnemy, float flSpeed, float dt );
	bool Wander( float flSpeed, float dt );
	void TurnTowards( const Vector &vecGoal, float dt );
	bool TryJump( const Vector &vecDir );
	void TurnHead( float dt, float limit = 80.0f );
	bool InReach( CBaseEntity *pTarget, float reach );
	void Strike( CBaseEntity *pTarget, float flDamage, float push, const char *const *sounds, int nsounds );

	bool	m_bDying;
	float	m_flLastThink;
	float	m_flNextLook;
	float	m_flNextWander;
	float	m_flWanderUntil;
	Vector	m_vecWander;
	float	m_flStuckSince;
	Vector	m_vecStuckPos;
	float	m_flDetourUntil;
	float	m_flDetourYaw;
	float	m_flNextStep;
	float	m_flStepGap;		// seconds between footsteps while chasing
	float	m_flHurtUntil;
	int	m_iCurAnim;
	float	m_flHeadYaw;
	bool	m_bLanded;
	float	m_flNextKnock;
	int	m_iHurtSkin;		// the red flash
	bool	m_bBlocked;		// the last Walk got nowhere (a spider climbs then)
};

// the skeleton's arrow (sc_mobs.cpp)
CBaseEntity *SC_ShootArrow( CBaseEntity *pShooter, const Vector &vecSrc, CBaseEntity *pTarget, float flSpeed, float flSpread );

#endif // SC_MOB_H
