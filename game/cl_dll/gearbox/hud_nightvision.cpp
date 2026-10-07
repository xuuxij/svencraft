/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// flashlight.cpp
//
// implementation of CHudFlashlight class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

DECLARE_MESSAGE( m_Nightvision, Nightvision )
// DECLARE_MESSAGE( m_Nightvision, Flashlight )	// Svencraft: the flashlight message is the HUD flashlight's (flashlight.cpp)

#define NIGHTVISION_SPRITE1_NAME "sprites/of_nv.spr"
#define NIGHTVISION_SPRITE2_NAME "sprites/of_nv_a.spr"
#define NIGHTVISION_SPRITE3_NAME "sprites/of_nv_b.spr"
#define NIGHTVISION_SPRITE4_NAME "sprites/of_nv_int.spr"

int CHudNightvision::Init(void)
{
	m_fOn = 0;

	HOOK_MESSAGE(Nightvision);	// Svencraft: only night vision itself (sc_nightvision 1); the flashlight is Half-Life's

	m_iFlags |= HUD_ACTIVE;

	gHUD.AddHudElem(this);

	return 1;
};

void CHudNightvision::Reset(void)
{
	m_fOn = 0;
}

int CHudNightvision::VidInit(void)
{
	// Svencraft: loaded when night vision is first switched on (sc_nightvision 1): Sven Co-op ships no of_nv sprites
	m_hSprite1 = m_hSprite2 = m_hSprite3 = m_hSprite4 = 0;
	m_nFrameCount = 0;
	m_iFrame = 0;
	return 1;
};

static void NV_LoadSprites( HSPRITE &s1, HSPRITE &s2, HSPRITE &s3, HSPRITE &s4, int &frames )
{
	s1 = LoadSprite(NIGHTVISION_SPRITE1_NAME);
	s2 = LoadSprite(NIGHTVISION_SPRITE2_NAME);
	s3 = LoadSprite(NIGHTVISION_SPRITE3_NAME);
	s4 = LoadSprite(NIGHTVISION_SPRITE4_NAME);

	// Get the number of frames available in this sprite.
	frames = s2 ? SPR_Frames(s2) : 0;
}


int CHudNightvision::MsgFunc_Nightvision(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	m_fOn = READ_BYTE();
	if( m_fOn && !m_hSprite2 )
		NV_LoadSprites( m_hSprite1, m_hSprite2, m_hSprite3, m_hSprite4, m_nFrameCount );

	return 1;
}

int CHudNightvision::MsgFunc_Flashlight( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_fOn = READ_BYTE();

	return 1;
}

int CHudNightvision::Draw(float flTime)
{
	if (gHUD.m_iHideHUDDisplay & (HIDEHUD_FLASHLIGHT | HIDEHUD_ALL))
		return 1;

	int r, g, b, x, y, a;
	
	// Only display this if the player is equipped with the suit.
	if (!(gHUD.m_iWeaponBits & (1 << (WEAPON_SUIT))))
		return 1;

	if (m_fOn)
		a = 225;
	else
		a = MIN_ALPHA;

	// Get each color component from the main
	// hud color.
	UnpackRGB(r, g, b, RGB_YELLOWISH);

	ScaleColors(r, g, b, a);

	// Top left of the screen.
	x = y = 0;

	// Reset the number of frame if we are at last frame.
	if (m_iFrame >= m_nFrameCount)
		m_iFrame = 0;

	if( !m_hSprite2 )
		return 1;	// Svencraft: no night vision sprites (Sven Co-op has none)
	const int nvgSpriteWidth = SPR_Width(m_hSprite2, 0);
	const int nvgSpriteHeight = SPR_Height(m_hSprite2, 0);
	if( nvgSpriteWidth <= 0 || nvgSpriteHeight <= 0 )
		return 1;

	const int colCount = (int)ceil(ScreenWidth / (float)nvgSpriteWidth);
	const int rowCount = (int)ceil(ScreenHeight / (float)nvgSpriteHeight);

	if (m_fOn)
	{  
		//
		// draw nightvision scanlines sprite.
		//
		SPR_Set(m_hSprite2, r, g, b);

		int i, j;
		for (i = 0; i < rowCount; ++i) // height
		{
			for (j = 0; j < colCount; ++j) // width
			{
				SPR_DrawAdditive(m_iFrame, x + (j * nvgSpriteWidth), y + (i * nvgSpriteHeight), NULL);
			}
		}
	}

	// Increase sprite frame.
	m_iFrame++;

	return 1;
}
