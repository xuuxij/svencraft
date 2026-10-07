/*
sc_gunfeel.h - Svencraft: guns that kick and spread like real ones (the weapon .cpp files, both DLLs)

The Half-Life guns are laser-accurate and only kick the camera. With sc_gunfeel 1 (server cvar; the client weapons
read it from the player's physinfo "scgf", sc_player.cpp):
- the cone is base + move*(speed/maxspeed) + bloom (+ air when off the ground), times duck when crouched;
- each round adds bloom (up to a cap), which dies away at SC_BLOOM_RECOVER a second;
- each round kicks the aim up (and a little sideways): pev->punchangle, which the bullets follow
  (GetAutoaimVector) and Half-Life's PM_DropPunchAngle brings back down; the camera-only kick of the events is halved.
Everything is predicted: the bloom rides in weapon_data_t.fuser4 (client.cpp GetWeaponData, hl_weapons.cpp), speed
and flags are the player's after this command's move on both sides (m_flSpreadSpeed, m_iSpreadFlags), and the
punch goes through the predicted clientdata.
Cones are VECTOR_CONE units (0.0087 = 1 degree).
*/
#ifndef SC_GUNFEEL_H
#define SC_GUNFEEL_H

typedef struct
{
	float base, move, air, duck;	// the cone standing still, + at full run, + in the air, x crouched
	float shot, maxbloom;		// bloom per round, its cap
	float kick, kickyaw;		// aim kick per round: degrees up, +- degrees sideways
} scgunfeel_t;

#define SC_BLOOM_RECOVER	0.10f

static const scgunfeel_t g_SCFeelGlock	= { 0.010f, 0.025f, 0.08f, 0.80f, 0.012f, 0.050f, 0.80f, 0.25f };
static const scgunfeel_t g_SCFeelGlockRapid	= { 0.100f, 0.030f, 0.10f, 0.90f, 0.010f, 0.050f, 0.60f, 0.40f };
static const scgunfeel_t g_SCFeelMP5	= { 0.026f, 0.035f, 0.09f, 0.75f, 0.006f, 0.045f, 1.15f, 0.45f };
static const scgunfeel_t g_SCFeelPython	= { 0.0087f, 0.040f, 0.12f, 0.80f, 0.040f, 0.060f, 3.00f, 0.60f };
static const scgunfeel_t g_SCFeelShotgun	= { 0.0f, 0.020f, 0.05f, 0.90f, 0.010f, 0.030f, 2.50f, 0.80f };
static const scgunfeel_t g_SCFeelShotgun2	= { 0.0f, 0.020f, 0.05f, 0.90f, 0.015f, 0.030f, 4.00f, 0.80f };
// Opposing Force's (dlls/gearbox), tuned toward Sven Co-op's: the M249's aim climbs slowly and a full burst opens its cone to about 7 degrees (Sven's: 6),
// the Desert Eagle is 6 degrees from the hip and 0.5 with its laser, the sniper rifle is exact only through the scope
static const scgunfeel_t g_SCFeelSAW	= { 0.035f, 0.050f, 0.10f, 0.60f, 0.003f, 0.025f, 0.80f, 0.60f };
static const scgunfeel_t g_SCFeelEagle	= { 0.052f, 0.040f, 0.10f, 0.85f, 0.030f, 0.060f, 2.50f, 0.50f };
static const scgunfeel_t g_SCFeelEagleLaser	= { 0.0044f, 0.030f, 0.10f, 0.85f, 0.020f, 0.040f, 2.50f, 0.50f };
static const scgunfeel_t g_SCFeelSniper	= { 0.001f, 0.080f, 0.15f, 0.70f, 0.0f, 0.0f, 5.00f, 0.80f };
#define SC_SNIPER_UNSCOPED	0.05f	// added to the sniper's cone without the scope

int SC_GunFeel( void );		// sc_gunfeel: server sc_player.cpp, client hl_weapons.cpp

// how wide this gun's cone is right now (on top of f.base)
inline float SC_GunSpread( CBasePlayerWeapon *w, const scgunfeel_t &f )
{
	CBasePlayer *p = w->m_pPlayer;
	float run = p->m_flSpreadSpeed / Q_max( p->pev->maxspeed, 1.0f );
	run = floor( Q_min( run, 1.0f ) * 16.0f + 0.5f ) / 16.0f;	// the client's copy of the speed is coarser
	float s = f.base + f.move * run + w->m_flBloom;
	if( !( p->m_iSpreadFlags & FL_ONGROUND ) && p->pev->waterlevel < 2 )
		s += f.air;
	if( p->m_iSpreadFlags & FL_DUCKING )
		s *= f.duck;
	return s;
}

// a round went off: the cone opens and the aim kicks
inline void SC_GunKick( CBasePlayerWeapon *w, const scgunfeel_t &f )
{
	CBasePlayer *p = w->m_pPlayer;
	w->m_flBloom = Q_min( w->m_flBloom + f.shot, f.maxbloom );
	p->pev->punchangle.x = Q_max( p->pev->punchangle.x - f.kick, -10.0f );
	p->pev->punchangle.y += UTIL_SharedRandomFloat( p->random_seed + 31, -f.kickyaw, f.kickyaw );
}

#endif // SC_GUNFEEL_H
