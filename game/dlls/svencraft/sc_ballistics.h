/*
sc_ballistics.h - Svencraft: rounds that go through what they can (server combat.cpp + sc_blast.cpp, client ev_hldm.cpp)

With sc_penetration 1 (server cvar; the client reads it from the player's physinfo "scpn", sc_player.cpp) a round
that hits glass, a door, a desk, a sheet of metal goes on through it, weaker:
- each round carries a power in "wood units" (SC_PenPower) and each material costs so much per unit of thickness
  (SC_PenCost); flesh and liquid stop it;
- the far side is found from outside: a point power/cost beyond the hit (at most 48) must be clear, and a trace
  back from it to the hit gives the exit face (it must be the same object, and its material counts too);
- after each surface the damage is scaled by the power left (at least a quarter); at most three surfaces.
The server does the damage; the client mirrors the loop for the exit holes, sounds and particles.
*/
#ifndef SC_BALLISTICS_H
#define SC_BALLISTICS_H

#define SC_PEN_REACH		48.0f	// the thickest a round looks through
#define SC_PEN_SURFACES	3	// surfaces after the first

inline float SC_PenCost( char t )
{
	switch( t )
	{
	case CHAR_TEX_GRATE:	return 0.25f;
	case CHAR_TEX_GLASS:	return 0.5f;
	case CHAR_TEX_WOOD:	return 1.0f;
	case CHAR_TEX_SNOW:
	case CHAR_TEX_SNOW_OPFOR:	return 1.0f;	// 'O': Sven's and Opposing Force's snow
	case CHAR_TEX_VENT:
	case CHAR_TEX_COMPUTER:	return 1.5f;	// sheet metal, plastic
	case CHAR_TEX_DIRT:	return 3.0f;
	case CHAR_TEX_TILE:	return 4.0f;
	case CHAR_TEX_METAL:	return 5.0f;
	case CHAR_TEX_CONCRETE:	return 8.0f;
	default:		return 0.0f;	// flesh, liquid: the round stops
	}
}

inline float SC_PenPower( int bullet )
{
	switch( bullet )
	{
	case BULLET_PLAYER_357:
	case BULLET_PLAYER_EAGLE:	return 40.0f;
	case BULLET_PLAYER_556:		return 30.0f;
	case BULLET_PLAYER_762:		return 60.0f;
	case BULLET_PLAYER_9MM:
	case BULLET_PLAYER_MP5:		return 16.0f;
	case BULLET_PLAYER_BUCKSHOT:	return 6.0f;
	default:			return 0.0f;
	}
}

int SC_Penetration( void );	// sc_penetration: server sc_player.cpp, client hl_weapons.cpp

#endif // SC_BALLISTICS_H
