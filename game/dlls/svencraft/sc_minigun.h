/*
sc_minigun.h - Svencraft: Sven Co-op's minigun (both DLLs: dlls/svencraft/sc_minigun.cpp, client cl_dll/hl/hl_weapons.cpp)
*/
#ifndef SC_MINIGUN_H
#define SC_MINIGUN_H

#define WEAPON_MINIGUN		30	// HL 1-15, Opposing Force 16-25, our tools 26-29, the suit 31
#define MINIGUN_WEIGHT		20
#define MINIGUN_DEFAULT_GIVE	100

// the barrels: the state rides in m_fInSpecialReload (predicted, weapon_data_t), the time to the next state in fuser1
enum
{
	SC_SPIN_STILL = 0,
	SC_SPIN_UP,		// winding up: no rounds yet
	SC_SPIN_ON,		// up to speed: fires while +attack is held, keeps spinning with +attack2
	SC_SPIN_DOWN		// winding down
};

class CMinigun : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 6; }
	int GetItemInfo( ItemInfo *p );
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	void ItemPostFrame( void );

	virtual BOOL UseDecrement( void )
	{
#if CLIENT_WEAPONS
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	void Spin( bool fire );
	void SpinDown( void );
	void SetSpeed( void );
	unsigned short m_usMinigun;
};

#endif // SC_MINIGUN_H
